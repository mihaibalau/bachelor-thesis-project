#ifndef DOTENV_H
#define DOTENV_H

#include <stdbool.h>
#include <stddef.h>

/* Load KEY=VALUE pairs from a .env file into the process environment.
 * Does not overwrite variables that are already set. Returns true if the file was opened. */
bool load_dotenv(const char *path);

/* Try each path in order; load the first file that exists. Returns true if any loaded. */
bool load_dotenv_first(const char *const *paths, size_t count);

/* Load .env from the directory containing the running executable (Windows). */
bool load_dotenv_near_executable(void);

#endif
