#pragma once
#include <ppu-types.h>

// Download and install a new release (Settings > Software Update, and the
// "New version" popup).
//
// The update check (update_check.cpp) finds the release's JellyFin-PS3.pkg and
// its SHA-256.  update_dl_start() downloads it on a worker into
// /dev_hdd0/packages/, checking size and SHA-256 as it arrives, so a broken or
// altered download is deleted instead of installed.  update_install() then asks
// webMAN MOD to install it (http://127.0.0.1/install.ps3<path>) and closes the
// app, since a package cannot be installed over the app while it runs.  Without
// webMAN the package waits in /dev_hdd0/packages for Package Manager.

enum { UDL_IDLE, UDL_RUNNING, UDL_READY, UDL_FAILED };

bool        update_dl_start(void);          // false: nothing to download, or already running
void        update_dl_cancel(void);
void        update_dl_poll(void);           // render thread, every frame while shown
int         update_dl_state(void);
void        update_dl_progress(u32 *got, u32 *total);
const char *update_dl_error(void);          // why it failed, for the screen
const char *update_dl_path(void);           // the downloaded package

// Ask webMAN to install the downloaded package.  True when webMAN accepted it
// (the caller then closes the app); false when webMAN is not there.
bool        update_install(void);
