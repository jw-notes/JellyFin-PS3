#pragma once
// Each PS3 user's own settings (the list is in jf_paths.cpp), kept in their
// save data with their sign-in (login_store.h).
//
// At start-up, before any setting is read, user_prefs_begin() unpacks this
// user's settings into a folder of their own and points jf_data_path() at it,
// so every setting module reads and writes them there as it always has.  A
// user with none saved yet starts from the console's copies (the settings as
// they were before each user had their own).  user_prefs_save() packs them
// back into the save: on leaving Settings, and when the app closes.
//
// If the system will not use save data, nothing changes: every setting is
// the console's, as before.

#include <stddef.h>

void user_prefs_begin(void);
bool user_prefs_active(void);

// The settings as one bundle for the save; 0 when there are none.
size_t user_prefs_bundle(char *out, size_t cap);

// Write them to the save now (with the sign-in), if they have changed.
void user_prefs_save(void);

// The same off the calling thread: the system takes seconds to write a save, and
// the render thread must not stand still for it.  Returns at once.
void user_prefs_save_async(void);

// Waits for one in progress.  Anything else that writes the save calls this
// first: they share its buffers.  A no-op on the saving thread itself.
void user_prefs_save_join(void);
