#include "include/dotenv.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Update CRT env block (_putenv on Windows so getenv() sees changes).
static void set_env_var(const char *key, const char *value) {
    if (!key || !*key || !value) return;
#ifdef _WIN32
    char entry[1024];
    int n = snprintf(entry, sizeof entry, "%s=%s", key, value);
    if (n < 0 || (size_t)n >= sizeof entry) return;
    _putenv(entry);
#else
    setenv(key, value, 1);
#endif
}

static void trim_inplace(char *s) {
    if (!s || !*s) return;
    char *start = s;
    while (*start && isspace((unsigned char)*start)) ++start;
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static void strip_quotes(char *s) {
    size_t len = strlen(s);
    if (len >= 2 && ((s[0] == '"' && s[len - 1] == '"') || (s[0] == '\'' && s[len - 1] == '\''))) {
        s[len - 1] = '\0';
        memmove(s, s + 1, len - 1);
    }
}

bool load_dotenv(const char *path) {
    // 1. Open file and parse KEY=VALUE lines.
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[1024];
    while (fgets(line, sizeof line, f)) {
        char *start = line;
        while (*start && isspace((unsigned char)*start)) ++start;
        if (*start == '\0' || *start == '#' || *start == '\n' || *start == '\r') continue;

        char *eq = strchr(start, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = start;
        char *value = eq + 1;

        trim_inplace(key);
        trim_inplace(value);
        strip_quotes(value);

        if (*key == '\0') continue;
        set_env_var(key, value);
    }

    fclose(f);
    return true;
}

bool load_dotenv_first(const char *const *paths, size_t count) {
    if (!paths) return false;
    for (size_t i = 0; i < count; ++i) {
        if (paths[i] && load_dotenv(paths[i])) {
            return true;
        }
    }
    return false;
}

bool load_dotenv_near_executable(void) {
#ifdef _WIN32
    char exe_path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, exe_path, (DWORD)sizeof exe_path);
    if (n == 0 || n >= sizeof exe_path) return false;

    char *slash = strrchr(exe_path, '\\');
    if (!slash) slash = strrchr(exe_path, '/');
    if (!slash) return false;
    *(slash + 1) = '\0';

    char env_path[MAX_PATH];
    int written = snprintf(env_path, sizeof env_path, "%s.env", exe_path);
    if (written < 0 || (size_t)written >= sizeof env_path) return false;
    return load_dotenv(env_path);
#else
    return false;
#endif
}
