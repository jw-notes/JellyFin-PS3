#pragma once

// Absolute path for an app-data file (login, device id, settings).
//
// Location is target-specific (see jf_paths.cpp): on the real PS3 it is
// /dev_hdd0/tmp — the one place writable at runtime, since the installed game
// USRDIR is not reliably writable while the title runs, so the login silently
// failed to save there.  Under RPCS3 it stays in the game USRDIR (writable and
// persistent there), unchanged.
//
// A PS3 user's own settings (the theme, layout, language, quality ...: see
// k_user_files in jf_paths.cpp) are each user's own.  Once user_prefs.h has
// unpacked them from the user's save data, jf_data_path() points those names
// at that folder; every other file stays the console's.
const char *jf_data_path(const char *name);

// The console's copy of a file, wherever the user's settings are.
const char *jf_shared_data_path(const char *name);

// The folder this PS3 user's settings live in this session (NULL: none, so
// every file is the console's).  The app-data folder's own path is joined on.
void jf_data_set_user_dir(const char *subdir);
const char *jf_data_user_dir(void);       // absolute, or NULL

// The settings that are each PS3 user's own.
int         jf_user_file_count(void);
const char *jf_user_file(int i);
