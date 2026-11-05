// Each PS3 user's own settings -- see user_prefs.h.
//
// The bundle is the files one after another, each as its name, its length in
// bytes, then the bytes:
//
//   jellyfin_theme.txt\n10\nJellywave\n\n
//
// Unknown names are skipped when it is unpacked, so a later version can add
// settings without breaking an older save.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/file.h>
#include <sys/thread.h>

#include "user_prefs.h"
#include "login_store.h"
#include "jf_paths.h"
#include "plog.h"

#define USER_SUBDIR "jf_user"

extern void jf_login_resave(void);     // api_auth.cpp: the sign-in and these, saved

static bool s_active = false;

bool user_prefs_active(void) { return s_active; }

// The bundle last known to be in the save, so an unchanged one costs no write.
static uint32_t s_saved_hash = 0;
static bool     s_have_hash  = false;
static char     s_bundle[16 * 1024];    // the save holds the same bundle (api_auth.cpp)

static uint32_t bundle_hash(const char *p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) h = (h ^ (uint8_t)p[i]) * 16777619u;
    return h ^ (uint32_t)n;
}

static bool known(const char *name) {
    for (int i = 0; i < jf_user_file_count(); i++)
        if (strcmp(name, jf_user_file(i)) == 0) return true;
    return false;
}

static void write_file(const char *path, const char *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    if (len) fwrite(data, 1, len, f);
    fclose(f);
}

// The user's folder path for name (dir is jf_data_user_dir()).
static void user_path(const char *dir, const char *name, char *out, size_t cap) {
    snprintf(out, cap, "%s/%s", dir, name);
}

static int unpack(const char *dir, const char *p, size_t len) {
    const char *end = p + len;
    int n = 0;
    while (p < end) {
        const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
        if (!nl) break;
        char name[64];
        snprintf(name, sizeof name, "%.*s", (int)(nl - p), p);
        p = nl + 1;
        nl = (const char *)memchr(p, '\n', (size_t)(end - p));
        if (!nl) break;
        const long size = strtol(p, NULL, 10);
        p = nl + 1;
        if (size < 0 || size > end - p) break;
        if (known(name) && !strchr(name, '/')) {
            char path[192];
            user_path(dir, name, path, sizeof path);
            write_file(path, p, (size_t)size);
            n++;
        }
        p += size;
        if (p < end && *p == '\n') p++;
    }
    return n;
}

void user_prefs_begin(void) {
    if (!login_store_read()) {
        plog("user prefs: no save data, the console's settings are used");
        return;
    }
    jf_data_set_user_dir(USER_SUBDIR);
    const char *dir = jf_data_user_dir();
    sysLv2FsMkdir(dir, 0755);
    // Nothing of the last user's may be left behind.
    char path[192];
    for (int i = 0; i < jf_user_file_count(); i++) {
        user_path(dir, jf_user_file(i), path, sizeof path);
        remove(path);
    }
    const char *data;
    size_t len;
    int n = 0;
    if (login_store_prefs(&data, &len)) {
        n = unpack(dir, data, len);
    } else {
        // None saved yet: start from the console's copies.
        static char buf[4096];
        for (int i = 0; i < jf_user_file_count(); i++) {
            FILE *f = fopen(jf_shared_data_path(jf_user_file(i)), "rb");
            if (!f) continue;
            const size_t got = fread(buf, 1, sizeof buf, f);
            fclose(f);
            user_path(dir, jf_user_file(i), path, sizeof path);
            write_file(path, buf, got);
            n++;
        }
    }
    s_active = true;
    char line[96];
    snprintf(line, sizeof line, "user prefs: %d settings for this PS3 user (%s)", n,
             login_store_prefs(&data, &len) ? "their own" : "from the console's");
    plog(line);
    // A first run's copies are not in the save yet, so they stay "changed".
    if (login_store_prefs(&data, &len)) {
        const size_t have = user_prefs_bundle(s_bundle, sizeof s_bundle);
        s_saved_hash = bundle_hash(s_bundle, have);
        s_have_hash  = true;
    }
}

size_t user_prefs_bundle(char *out, size_t cap) {
    if (!s_active || !out || cap == 0) return 0;
    const char *dir = jf_data_user_dir();
    size_t used = 0;
    static char buf[4096];
    for (int i = 0; i < jf_user_file_count(); i++) {
        char path[192];
        user_path(dir, jf_user_file(i), path, sizeof path);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        const size_t got = fread(buf, 1, sizeof buf, f);
        fclose(f);
        char head[96];
        const int hl = snprintf(head, sizeof head, "%s\n%u\n", jf_user_file(i), (unsigned)got);
        if (hl <= 0 || used + (size_t)hl + got + 1 > cap) break;
        memcpy(out + used, head, (size_t)hl); used += (size_t)hl;
        memcpy(out + used, buf, got);         used += got;
        out[used++] = '\n';
    }
    return used;
}

static void save_if_changed(void) {
    const size_t have = user_prefs_bundle(s_bundle, sizeof s_bundle);
    const uint32_t h = bundle_hash(s_bundle, have);
    if (s_have_hash && h == s_saved_hash) return;
    jf_login_resave();
    s_saved_hash = h;
    s_have_hash  = true;
}

static sys_ppu_thread_t s_save_tid = 0;
static volatile bool    s_saving = false, s_save_again = false;

static void save_thread_fn(void *) {
    do {
        s_save_again = false;
        save_if_changed();
    } while (s_save_again);
    __sync_synchronize();
    s_saving = false;
    sysThreadExit(0);
}

void user_prefs_save_join(void) {
    if (!s_save_tid) return;
    sys_ppu_thread_t self = 0;
    sysThreadGetId(&self);
    if (self == s_save_tid) return;
    u64 ret;
    sysThreadJoin(s_save_tid, &ret);
    s_save_tid = 0;
}

void user_prefs_save_async(void) {
    if (!s_active) return;
    if (s_save_tid && s_saving) {       // caught at its very end, the last save on exit has it
        s_save_again = true;
        return;
    }
    user_prefs_save_join();
    s_saving = true;
    static char name[] = "jf_prefsave";
    if (sysThreadCreate(&s_save_tid, save_thread_fn, NULL, 1500, 128 * 1024,
                        THREAD_JOINABLE, name) != 0) {
        plog("user prefs: save thread FAILED -- saved on exit instead");
        s_save_tid = 0;
        s_saving = false;
    }
}

void user_prefs_save(void) {
    if (!s_active) return;
    user_prefs_save_join();
    save_if_changed();
}
