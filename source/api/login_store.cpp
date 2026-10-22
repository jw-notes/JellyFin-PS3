// The PS3 user's save data -- see login_store.h.
//
// The save data utility runs the operation itself and calls back into the
// app at each step: the status callback says whether the save exists (and,
// on a save, gives its title and details), then the file callback is called
// once per file operation until it says it is done.  Both run before the call
// returns, so the state below is only ever touched by one operation at a time.

#include <stdio.h>
#include <string.h>

#include <ppu-types.h>
#include <sysutil/save.h>

#include "login_store.h"
#include "plog.h"

#define SAVE_DIR   "JFPS30000-LOGIN"
#define LOGIN_FILE "LOGIN.TXT"
#define PREFS_FILE "PREFS.DAT"
#define ICON_PATH  "/dev_hdd0/game/JFPS30000/ICON0.PNG"
#define NO_CONTAINER ((sys_mem_container_t)0xffffffffu)
#define PREFS_MAX  (16 * 1024)

// What was read, once.
static bool   s_read_done = false, s_read_ok = false, s_found = false, s_has_prefs = false;
static char   s_login[1024];
static char   s_prefs[PREFS_MAX];
static u32    s_login_len, s_prefs_len;

// One operation's state.
static struct {
    bool        saving;
    int         step;
    const char *login;  size_t login_len;
    const char *prefs;  size_t prefs_len;
} s_op;

static char s_title[]      = "JellyFin [PS]";
static char s_subtitle[SYS_SAVE_MAX_SUBTITLE];
static char s_detail[SYS_SAVE_MAX_DETAIL];
static char s_login_name[] = LOGIN_FILE;
static char s_prefs_name[] = PREFS_FILE;
static char s_wbuf_login[1024];
static char s_wbuf_prefs[PREFS_MAX];
static sysSaveSystemFileParam s_param;
static sysSaveFileStatus      s_files[8];
static unsigned char          s_icon[64 * 1024];
static u32                    s_icon_len;

static void status_cb(sysSaveCallbackResult *r, sysSaveStatusIn *in, sysSaveStatusOut *out) {
    r->result = SYS_SAVE_CALLBACK_RESULT_CONTINUE;
    if (!s_op.saving) {
        s_found = !in->isNew;
        s_has_prefs = false;
        for (u32 i = 0; i < in->numFiles && in->fileList; i++)
            if (strcmp(in->fileList[i].filename, PREFS_FILE) == 0) s_has_prefs = true;
        // Nothing to read on a user with no save: stop here, it is not an error.
        if (in->isNew) r->result = SYS_SAVE_CALLBACK_RESULT_DONE;
        out->setParam = NULL;
        return;
    }
    memset(&s_param, 0, sizeof s_param);
    snprintf(s_param.title,    sizeof s_param.title,    "%s", s_title);
    snprintf(s_param.subtitle, sizeof s_param.subtitle, "%s", s_subtitle);
    snprintf(s_param.detail,   sizeof s_param.detail,   "%s", s_detail);
    out->setParam     = &s_param;
    out->recreateMode = SYS_SAVE_RECREATE_MODE_OVERWRITE_CORRUPTED;
}

static void op(sysSaveFileOut *out, sysSaveFileOperation what, sysSaveFileType type,
               char *name, void *buf, u32 size) {
    out->fileOperation = what;
    out->fileType      = type;
    out->filename      = name;
    out->size          = size;
    out->bufferSize    = size;
    out->buffer        = buf;
}

static void file_cb(sysSaveCallbackResult *r, sysSaveFileIn *in, sysSaveFileOut *out) {
    r->result = SYS_SAVE_CALLBACK_RESULT_CONTINUE;
    r->incrementProgress = 25;
    memset(out, 0, sizeof *out);
    const int step = s_op.step++;
    if (!s_op.saving) {
        // Each read's size arrives with the next call.
        if (step == 1) s_login_len = in->previousOperationResultSize;
        if (step == 2) s_prefs_len = in->previousOperationResultSize;
        if (step == 0) {
            op(out, SYS_SAVE_FILE_OPERATION_READ, SYS_SAVE_FILETYPE_STANDARD_FILE,
               s_login_name, s_login, sizeof s_login - 1);
            return;
        }
        if (step == 1 && s_has_prefs) {
            op(out, SYS_SAVE_FILE_OPERATION_READ, SYS_SAVE_FILETYPE_STANDARD_FILE,
               s_prefs_name, s_prefs, sizeof s_prefs - 1);
            return;
        }
        r->result = SYS_SAVE_CALLBACK_RESULT_DONE;
        return;
    }
    switch (step) {
    case 0:
        op(out, SYS_SAVE_FILE_OPERATION_WRITE, SYS_SAVE_FILETYPE_STANDARD_FILE,
           s_login_name, s_wbuf_login, (u32)s_op.login_len);
        return;
    case 1:
        if (s_op.prefs_len) {
            op(out, SYS_SAVE_FILE_OPERATION_WRITE, SYS_SAVE_FILETYPE_STANDARD_FILE,
               s_prefs_name, s_wbuf_prefs, (u32)s_op.prefs_len);
            return;
        }
        s_op.step++;
        // fall through: no settings to write
    case 2:
        // The app's own icon, so the save looks like the app in the XMB.
        if (s_icon_len) {
            op(out, SYS_SAVE_FILE_OPERATION_WRITE, SYS_SAVE_FILETYPE_CONTENT_ICON0,
               NULL, s_icon, s_icon_len);
            return;
        }
        // fall through: no icon
    default:
        r->result = SYS_SAVE_CALLBACK_RESULT_DONE;
        return;
    }
}

static sysSaveBufferSettings buffer_settings(void) {
    sysSaveBufferSettings b;
    memset(&b, 0, sizeof b);
    b.maxDirectories = 0;
    b.maxFiles       = sizeof s_files / sizeof s_files[0];
    b.bufferSize     = sizeof s_files;
    b.buffer         = s_files;
    return b;
}

static void log_rc(const char *what, s32 rc) {
    char line[96];
    snprintf(line, sizeof line, "user save: %s rc=0x%08x", what, (unsigned)rc);
    plog(line);
}

bool login_store_read(void) {
    if (s_read_done) return s_read_ok;
    s_read_done = true;
    memset(&s_op, 0, sizeof s_op);
    memset(s_login, 0, sizeof s_login);
    s_login_len = s_prefs_len = 0;
    sysSaveBufferSettings b = buffer_settings();
    const s32 rc = sysSaveAutoLoad2(SYS_SAVE_CURRENT_VERSION, SAVE_DIR, SYS_SAVE_ERROR_DIALOG_NONE,
                                    &b, status_cb, file_cb, NO_CONTAINER, NULL);
    log_rc(!s_found ? "read (none yet)" : s_has_prefs ? "read, with settings" : "read", rc);
    s_read_ok = rc == SYS_SAVE_RETURN_DONE;
    if (!s_read_ok) s_found = false;
    if (s_login_len >= sizeof s_login) s_login_len = sizeof s_login - 1;
    s_login[s_login_len] = '\0';
    if (s_prefs_len >= sizeof s_prefs) s_prefs_len = sizeof s_prefs - 1;
    if (!s_prefs_len) s_has_prefs = false;
    return s_read_ok;
}

bool login_store_load(char *out, size_t cap, bool *found) {
    *found = false;
    if (!out || cap < 2 || !login_store_read()) return false;
    snprintf(out, cap, "%s", s_found ? s_login : "");
    *found = s_found;
    return true;
}

bool login_store_prefs(const char **data, size_t *len) {
    if (!login_store_read() || !s_found || !s_has_prefs) return false;
    *data = s_prefs;
    *len  = s_prefs_len;
    return true;
}

bool login_store_save(const char *text, const char *prefs, size_t prefs_len,
                      const char *subtitle, const char *detail) {
    if (!s_icon_len) {
        FILE *f = fopen(ICON_PATH, "rb");
        if (f) { s_icon_len = (u32)fread(s_icon, 1, sizeof s_icon, f); fclose(f); }
        if (s_icon_len == sizeof s_icon) s_icon_len = 0;    // larger than expected: leave it out
    }
    snprintf(s_subtitle, sizeof s_subtitle, "%s", subtitle ? subtitle : "");
    snprintf(s_detail,   sizeof s_detail,   "%s", detail ? detail : "");
    memset(&s_op, 0, sizeof s_op);
    snprintf(s_wbuf_login, sizeof s_wbuf_login, "%s", text);
    if (!prefs || prefs_len > sizeof s_wbuf_prefs) prefs_len = 0;
    if (prefs_len) memcpy(s_wbuf_prefs, prefs, prefs_len);
    s_op.saving    = true;
    s_op.login_len = strlen(s_wbuf_login);
    s_op.prefs_len = prefs_len;
    sysSaveBufferSettings b = buffer_settings();
    const s32 rc = sysSaveAutoSave2(SYS_SAVE_CURRENT_VERSION, SAVE_DIR, SYS_SAVE_ERROR_DIALOG_NONE,
                                    &b, status_cb, file_cb, NO_CONTAINER, NULL);
    log_rc("save", rc);
    return rc == SYS_SAVE_RETURN_DONE;
}
