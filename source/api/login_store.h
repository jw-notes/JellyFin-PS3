#pragma once
// A PS3 user's own data kept as PS3 save data, the way the console's own apps
// keep theirs: the system files a save under the signed-in PS3 user's folder,
// so every PS3 user has their own without the app knowing who is signed in.
// It shows in Game > Saved Data Utility as "JellyFin [PS]".
//
// Two files: LOGIN.TXT, the sign-in (server, username, token, user id and
// device id, one to a line), and PREFS.DAT, the user's settings
// (user_prefs.h).  No dialogs are shown; a failure returns false and the
// callers fall back to the console's shared files in /dev_hdd0/tmp.

#include <stddef.h>

// Read the save, once (the first call does it; later ones return the result).
// False when the system would not read save data at all.
bool login_store_read(void);

// The sign-in text (NUL-terminated); *found is false when this PS3 user has
// no save yet.  False as login_store_read().
bool login_store_load(char *out, size_t cap, bool *found);

// The settings bundle as it was read; false when the save has none.
bool login_store_prefs(const char **data, size_t *len);

// Write the save, creating it the first time: the sign-in text, the settings
// bundle (prefs may be NULL), and what the Saved Data Utility shows under the
// title (the account, the credits).
bool login_store_save(const char *text, const char *prefs, size_t prefs_len,
                      const char *subtitle, const char *detail);
