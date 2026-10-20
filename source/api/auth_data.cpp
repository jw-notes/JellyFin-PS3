#include "auth_data.h"
#include <string.h>

static bool read_line(FILE *file, char *out, size_t capacity) {
    size_t used = 0;
    int c, last = 0;
    bool embedded_nul = false;
    while ((c = fgetc(file)) != EOF && c != '\n') {
        if (used + 1 < capacity) out[used] = (char)c;
        ++used;
        last = c;
        if (c == '\0') embedded_nul = true;
    }
    if (used && last == '\r') --used;
    if (used >= capacity) { out[0] = '\0'; return false; }
    out[used] = '\0';
    return !embedded_nul && !ferror(file) && (c == '\n' || used > 0);
}

bool jf_read_session(FILE *file, JFSavedSession *session) {
    if (!file || !session) return false;
    JFSavedSession value = {};
    if (!read_line(file, value.server, sizeof(value.server)) ||
        !read_line(file, value.username, sizeof(value.username)) ||
        !read_line(file, value.token, sizeof(value.token)) ||
        !read_line(file, value.userid, sizeof(value.userid))) return false;
    size_t len = strlen(value.server);
    while (len && value.server[len - 1] == '/') value.server[--len] = '\0';
    if (!value.server[0] || !value.username[0] ||
        !value.token[0] || !value.userid[0]) return false;
    *session = value;
    return true;
}

// The maximum expansion is six bytes per input byte (a JSON \u00XX escape).
static bool append(char *out, size_t capacity, size_t *used,
                   const char *text, size_t length) {
    if (*used >= capacity || length >= capacity - *used) return false;
    memcpy(out + *used, text, length);
    *used += length;
    out[*used] = '\0';
    return true;
}

static bool quoted(char *out, size_t capacity, size_t *used, const char *text) {
    static const char hex[] = "0123456789abcdef";
    if (!append(out, capacity, used, "\"", 1)) return false;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        char escaped[6];
        size_t length = 1;
        escaped[0] = (char)*p;
        if (*p == '"' || *p == '\\') {
            escaped[0] = '\\'; escaped[1] = (char)*p; length = 2;
        } else if (*p < 0x20) {
            memcpy(escaped, "\\u00", 4);
            escaped[4] = hex[*p >> 4]; escaped[5] = hex[*p & 15]; length = 6;
        }
        if (!append(out, capacity, used, escaped, length)) return false;
    }
    return append(out, capacity, used, "\"", 1);
}

bool jf_build_login_body(const char *username, const char *password,
                         char *out, size_t capacity) {
    if (!out || !capacity) return false;
    out[0] = '\0';
    if (!username || !password) return false;
    size_t used = 0;
    if (append(out, capacity, &used, "{\"Username\":", 12) &&
        quoted(out, capacity, &used, username) &&
        append(out, capacity, &used, ",\"Pw\":", 6) &&
        quoted(out, capacity, &used, password) &&
        append(out, capacity, &used, "}", 1)) return true;
    out[0] = '\0';
    return false;
}
