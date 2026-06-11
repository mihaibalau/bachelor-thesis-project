#include "include/jwt_utils.h"
#include "service/include/service_error.h"

#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <jansson.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


// Base64url encode: RFC 4648 §5 — '+' → '-', '/' → '_', strip '='
static char *b64url_encode(const unsigned char *data, size_t len) {
    BIO *bmem = BIO_new(BIO_s_mem());
    BIO *b64  = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_push(b64, bmem);
    BIO_write(b64, data, (int)len);
    BIO_flush(b64);

    BUF_MEM *bptr;
    BIO_get_mem_ptr(b64, &bptr);

    char *out = (char *)malloc(bptr->length + 1);
    if (!out) { BIO_free_all(b64); return NULL; }
    memcpy(out, bptr->data, bptr->length);
    out[bptr->length] = '\0';
    BIO_free_all(b64);

    // JWT uses base64url, not standard base64 — remap chars and drop padding.
    for (size_t i = 0; out[i]; ++i) {
        if (out[i] == '+') out[i] = '-';
        else if (out[i] == '/') out[i] = '_';
    }
    // Strip trailing '=' padding.
    size_t olen = strlen(out);
    while (olen > 0 && out[olen - 1] == '=') out[--olen] = '\0';

    return out;
}

// Base64url decode → allocates output buffer; caller must free()
static unsigned char *b64url_decode(const char *in, size_t *out_len) {
    size_t in_len = strlen(in);

    // Base64 needs length multiple of 4; JWT strips padding, so add it back.
    size_t pad  = (4 - (in_len % 4)) % 4;
    char *padded = (char *)malloc(in_len + pad + 1);
    if (!padded) return NULL;
    memcpy(padded, in, in_len);
    for (size_t i = 0; i < pad; ++i) padded[in_len + i] = '=';
    padded[in_len + pad] = '\0';

    // Undo base64url char mapping before OpenSSL decode.
    for (size_t i = 0; padded[i]; ++i) {
        if (padded[i] == '-') padded[i] = '+';
        else if (padded[i] == '_') padded[i] = '/';
    }

    BIO *bmem = BIO_new_mem_buf(padded, -1);
    BIO *b64  = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_push(b64, bmem);

    unsigned char *buf = (unsigned char *)malloc(in_len + 4);
    int decoded_len = 0;
    if (buf) decoded_len = BIO_read(b64, buf, (int)(in_len + 4));
    BIO_free_all(b64);
    free(padded);

    if (decoded_len <= 0) { free(buf); return NULL; }
    *out_len = (size_t)decoded_len;
    return buf;
}

// ── HMAC-SHA256 ────────────────────────────────────────────────────

// HMAC-SHA256 over the signing input (header.payload string).
static bool hmac_sha256(
    const char *secret,
    const char *message,
    unsigned char out_digest[32]
) {
    unsigned int digest_len = 32;
    unsigned char *result = HMAC(
        EVP_sha256(),
        secret, (int)strlen(secret),
        (const unsigned char *)message, strlen(message),
        out_digest, &digest_len
    );
    return result != NULL;
}

// ── Public API ─────────────────────────────────────────────────────────

bool jwt_decode_user_id(
    const char *secret,
    const char *token,
    UserId *out_user_id,
    ServiceError *err
) {
    // JWT = header.payload.signature

    // 1. Split the token
    const char *dot1 = strchr(token, '.');
    if (!dot1) {
        if (err) *err = service_error_validation("invalid JWT: missing first dot");
        return false;
    }
    const char *dot2 = strchr(dot1 + 1, '.');
    if (!dot2) {
        if (err) *err = service_error_validation("invalid JWT: missing second dot");
        return false;
    }

    // 2. Check the signature: HMAC-SHA256(header + "." + payload, secret)
    size_t header_payload_len = (size_t)(dot2 - token);
    char *header_payload = (char *)malloc(header_payload_len + 1);
    if (!header_payload) {
        if (err) *err = service_error_validation("OOM");
        return false;
    }
    memcpy(header_payload, token, header_payload_len);
    header_payload[header_payload_len] = '\0';

    unsigned char computed_sig[32];
    if (!hmac_sha256(secret, header_payload, computed_sig)) {
        free(header_payload);
        if (err) *err = service_error_validation("HMAC computation failed");
        return false;
    }
    free(header_payload);

    // 3. Decode the signature from the token and compare
    size_t token_sig_len = 0;
    unsigned char *token_sig = b64url_decode(dot2 + 1, &token_sig_len);
    if (!token_sig || token_sig_len != 32) {
        free(token_sig);
        if (err) *err = service_error_validation("invalid JWT signature format");
        return false;
    }

    if (CRYPTO_memcmp(computed_sig, token_sig, 32) != 0) {
        free(token_sig);
        if (err) *err = service_error_validation("invalid JWT signature");
        return false;
    }
    free(token_sig);

    // 4. Decode payload and extract "sub" + "exp".
    size_t payload_b64_len = (size_t)(dot2 - dot1 - 1);
    char *payload_b64 = (char *)malloc(payload_b64_len + 1);
    if (!payload_b64) {
        if (err) *err = service_error_validation("OOM");
        return false;
    }
    memcpy(payload_b64, dot1 + 1, payload_b64_len);
    payload_b64[payload_b64_len] = '\0';

    size_t payload_json_len = 0;
    unsigned char *payload_json = b64url_decode(payload_b64, &payload_json_len);
    free(payload_b64);

    if (!payload_json) {
        if (err) *err = service_error_validation("failed to decode JWT payload");
        return false;
    }

    // Null-terminate for jansson
    unsigned char *payload_str = (unsigned char *)realloc(payload_json, payload_json_len + 1);
    if (!payload_str) { free(payload_json); if (err) *err = service_error_validation("OOM"); return false; }
    payload_str[payload_json_len] = '\0';

    json_error_t jerr;
    json_t *root = json_loadb((const char *)payload_str, payload_json_len, 0, &jerr);
    free(payload_str);

    if (!root) {
        if (err) *err = service_error_validation("invalid JWT payload JSON");
        return false;
    }

    // 5. Check exp (required + must not be expired, matching Rust default validation)
    json_t *exp_j = json_object_get(root, "exp");
    if (!exp_j || !json_is_integer(exp_j)) {
        json_decref(root);
        if (err) *err = service_error_validation("JWT missing 'exp' claim");
        return false;
    }
    {
        time_t exp = (time_t)json_integer_value(exp_j);
        if (exp < time(NULL)) {
            json_decref(root);
            if (err) *err = service_error_validation("JWT expired");
            return false;
        }
    }

    // 6. Require 'tag' claim (Rust Claims deserialization requires it to be present)
    json_t *tag_j = json_object_get(root, "tag");
    if (!tag_j || !json_is_string(tag_j)) {
        json_decref(root);
        if (err) *err = service_error_validation("JWT missing 'tag' claim");
        return false;
    }

    // 7. Extract sub
    json_t *sub_j = json_object_get(root, "sub");
    if (!sub_j || !json_is_integer(sub_j)) {
        json_decref(root);
        if (err) *err = service_error_validation("JWT missing 'sub' claim");
        return false;
    }

    out_user_id->value = (int64_t)json_integer_value(sub_j);

    json_decref(root);

    if (err) *err = service_error_ok();
    return true;
}

bool jwt_encode_user_id(
    const char *secret,
    int64_t user_id,
    const char *tag,
    char *out_token,
    size_t out_token_size,
    ServiceError *err
) {
    // HS256 JWT: base64url(header) . base64url(payload) . HMAC-SHA256(secret, above).
    const char *header_json = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
    char *header_b64 = b64url_encode(
        (const unsigned char *)header_json, strlen(header_json));
    if (!header_b64) {
        if (err) *err = service_error_validation("failed to encode JWT header");
        return false;
    }

    // Payload: {"sub": user_id, "exp": now+24h} — matches Rust JWT lifetime
    time_t exp = time(NULL) + 86400;
    char payload_json[256];
    snprintf(payload_json, sizeof payload_json,
             "{\"sub\":%lld,\"tag\":\"%s\",\"exp\":%lld}",
             (long long)user_id,
             tag ? tag : "",
             (long long)exp);

    char *payload_b64 = b64url_encode(
        (const unsigned char *)payload_json, strlen(payload_json));
    if (!payload_b64) {
        free(header_b64);
        if (err) *err = service_error_validation("failed to encode JWT payload");
        return false;
    }

    // header.payload
    size_t hp_len = strlen(header_b64) + 1 + strlen(payload_b64) + 1;
    char *hp = (char *)malloc(hp_len);
    if (!hp) { free(header_b64); free(payload_b64); if (err) *err = service_error_validation("OOM"); return false; }
    snprintf(hp, hp_len, "%s.%s", header_b64, payload_b64);
    free(header_b64);
    free(payload_b64);

    // HMAC-SHA256
    unsigned char sig[32];
    if (!hmac_sha256(secret, hp, sig)) {
        free(hp);
        if (err) *err = service_error_validation("HMAC failed");
        return false;
    }

    char *sig_b64 = b64url_encode(sig, 32);
    if (!sig_b64) {
        free(hp);
        if (err) *err = service_error_validation("failed to encode JWT signature");
        return false;
    }

    int written = snprintf(out_token, out_token_size, "%s.%s", hp, sig_b64);
    free(hp);
    free(sig_b64);

    if (written < 0 || (size_t)written >= out_token_size) {
        if (err) *err = service_error_validation("JWT output buffer too small");
        return false;
    }

    if (err) *err = service_error_ok();
    return true;
}
