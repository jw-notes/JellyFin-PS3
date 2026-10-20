#pragma once
#include <stddef.h>
#include <stdio.h>

struct JFSavedSession {
    char server[256];
    char username[64];
    char token[256];
    char userid[64];
};

// Read the existing four-line format without splitting usernames on spaces.
// Invalid or truncated files leave the caller's session untouched.
bool jf_read_session(FILE *file, JFSavedSession *session);

// Build a JSON request without changing the user's credentials.
// Returns false and clears out if the destination cannot hold the full body.
bool jf_build_login_body(const char *username, const char *password,
                         char *out, size_t capacity);
