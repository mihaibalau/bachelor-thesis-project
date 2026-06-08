//! One-off helper: `cargo run --example hash_password -- admin`
use argon2::{Algorithm, Argon2, Params, PasswordHasher, Version};
use argon2::password_hash::{SaltString, rand_core::OsRng};

fn main() {
    let plain = std::env::args().nth(1).unwrap_or_else(|| "admin".to_string());
    let salt = SaltString::generate(&mut OsRng);
    // Explicit Argon2id parameters (m=65536 KiB, t=3, p=1) — same as the runtime
    // hashing in the user service and the C backend.
    let params = Params::new(65536, 3, 1, None).expect("valid argon2 params");
    let argon2 = Argon2::new(Algorithm::Argon2id, Version::V0x13, params);
    let hash = argon2
        .hash_password(plain.as_bytes(), &salt)
        .expect("hash failed");
    println!("{hash}");
}
