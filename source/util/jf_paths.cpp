// App-data directory helper — see jf_paths.h.

#include "jf_paths.h"
#include "../build_config.h"

#include <stdio.h>
#include <string.h>
#if BUILD_FOR_RPCS3
#include <sys/file.h>   // sysLv2FsMkdir (inline LV2 syscall, no PRX module needed)
#endif

// Where config / device id / settings live.  This differs by target:
//
//  - Real PS3 (BUILD_FOR_RPCS3 0): /dev_hdd0/tmp.  The installed game USRDIR is
//    NOT reliably writable while the title runs on hardware, so saving the login
//    there silently failed and user info was never persisted.  /dev_hdd0/tmp is
//    writable at runtime.  It is scratch, so a console that wipes it on boot
//    drops the saved login (re-login next boot) — the accepted trade for it
//    saving at all.
//  - RPCS3 (BUILD_FOR_RPCS3 1): the game USRDIR, unchanged.  RPCS3's USRDIR is
//    writable and persists, so the emulator keeps its saved login across runs.
#if BUILD_FOR_RPCS3
#define JF_DATA_DIR "/dev_hdd0/game/JFPS30000/USRDIR"
#else
#define JF_DATA_DIR "/dev_hdd0/tmp"
#endif

// Each PS3 user's own: how the app looks and behaves for them, and what they
// have done in it.  The rest -- the TV (screen size, 1080p, 24 Hz), the sound
// system (Audio Output), downloads, the server's MAC, logging and the
// developer switches -- is the console's.
static const char *const k_user_files[] = {
    "jellyfin_theme.txt", "jellyfin_layout.txt", "jellyfin_language.txt",
    "jellyfin_daynight.txt", "jellyfin_wavereact.txt", "jellyfin_menusnow.txt",
    "jellyfin_visualiser.txt", "jellyfin_visualizer.txt", "jellyfin_jellydrop.txt",
    "jellyfin_screensaver.txt", "jellyfin_screensaver_delay.txt", "jellyfin_ambient.txt",
    "jellyfin_subfont.txt", "jellyfin_subcolor.txt", "jellyfin_uisfx.txt",
    "jellyfin_centermix.txt", "jellyfin_vquality.txt", "jellyfin_vq_items.txt",
    "jellyfin_autoskip.txt", "jellyfin_directstream.txt", "jellyfin_livetv.txt",
    "jellyfin_livetv_quality.txt", "jellyfin_livetv_buffer.txt", "jellyfin_picsize.txt",
    "jellyfin_aspect.txt", "jellyfin_volume.txt", "jellyfin_search_recent.txt",
    "jellyfin_local_resume.txt",
};
#define N_USER_FILES ((int)(sizeof k_user_files / sizeof k_user_files[0]))

int         jf_user_file_count(void) { return N_USER_FILES; }
const char *jf_user_file(int i)      { return i >= 0 && i < N_USER_FILES ? k_user_files[i] : ""; }

static char s_user_dir[128];
static bool s_user_on = false;

void jf_data_set_user_dir(const char *subdir) {
    s_user_on = subdir && subdir[0];
    if (s_user_on) snprintf(s_user_dir, sizeof s_user_dir, "%s/%s", JF_DATA_DIR, subdir);
}

const char *jf_data_user_dir(void) { return s_user_on ? s_user_dir : NULL; }

static bool is_user_file(const char *name) {
    for (int i = 0; i < N_USER_FILES; i++)
        if (strcmp(name, k_user_files[i]) == 0) return true;
    return false;
}

const char *jf_data_path(const char *name) {
    if (s_user_on && is_user_file(name)) {
        static char ubuf[192];
        snprintf(ubuf, sizeof ubuf, "%s/%s", s_user_dir, name);
        return ubuf;
    }
    return jf_shared_data_path(name);
}

const char *jf_shared_data_path(const char *name) {
#if BUILD_FOR_RPCS3
    static bool made = false;
    if (!made) {
        // Create the tree if missing (bare .self boot under RPCS3).
        sysLv2FsMkdir("/dev_hdd0/game/JFPS30000", 0755);
        sysLv2FsMkdir(JF_DATA_DIR, 0755);
        made = true;
    }
#endif
    static char buf[128];
    snprintf(buf, sizeof(buf), "%s/%s", JF_DATA_DIR, name);
    return buf;
}
