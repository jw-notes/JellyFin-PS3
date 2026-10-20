// Download and install a new release -- see update_install.h.
//
// WHERE FROM.  A release asset's download link redirects to
// release-assets.githubusercontent.com, which only accepts AES-GCM ciphers, and
// the PS3's firmware TLS has none: the handshake fails ("could not reach
// GitHub").  api.github.com still accepts AES-CBC, and its contents API serves a
// file from the repository straight back, no redirect.  So each release also
// keeps its package at release/JellyFin-PS3.pkg in the tagged commit, and that
// is fetched first; the asset link is the fallback.  Either way the bytes are
// checked against the release asset's SHA-256 before anything is installed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ppu-types.h>
#include <sys/thread.h>
#include <sys/file.h>
#include <http/https.h>
#include <http/util.h>
#include <ssl/ssl.h>

#include "update_install.h"
#include "update_check.h"
#include "release_asset.h"
#include "https_client.h"
#include "http.h"
#include "sha256.h"
#include "plog.h"

#define PKG_DIR        "/dev_hdd0/packages"
#define DL_TIMEOUT_US  (15 * 1000 * 1000)
#define DL_CHUNK       (64 * 1024)
#define WEBMAN_INSTALL "http://127.0.0.1/install.ps3"
#define MIRROR_URL     "https://api.github.com/repos/" UPDATE_REPO "/contents/release/JellyFin-PS3.pkg?ref=%s"

static sys_ppu_thread_t s_tid;
static bool             s_thread = false;       // render thread only
static volatile int     s_state  = UDL_IDLE;
static volatile bool    s_done   = false;       // worker finished
static volatile bool    s_cancel = false;
static volatile u32     s_got = 0, s_total = 0;
static release_asset    s_asset;
static char             s_tag[64];
static char             s_path[160];
static char             s_err[96];

static void fail(const char *why) {
    snprintf(s_err, sizeof s_err, "%s", why);
    plog(why);
}

// GitHub's certificate chain is newer than the firmware's CA store: the
// download is checked against the SHA-256 GitHub published, which is what
// actually decides whether it is installed.
static int dl_ssl_cb(s32 verErr, sslCert const certs[], int certNum,
                     const char *host, httpSslId id, void *arg) {
    (void)verErr; (void)certs; (void)certNum; (void)host; (void)id; (void)arg;
    return 0;
}

// One attempt at `url`.  Returns 1 downloaded and verified, 0 failed (s_err
// says why), -1 not there (404) -- the caller then tries the next source.
static int fetch(const char *url, const char *accept) {
    char part[176];
    snprintf(part, sizeof part, "%s.part", s_path);
    int result = 0;
    bool have_rt = false;
    httpClientId client = 0;
    httpTransId trans = 0;
    void *uri_pool = NULL;
    char *buf = NULL;
    FILE *f = NULL;
    sha256_ctx sha;
    s32 rc;
    char m[160];

    s_got = s_total = 0;
    if (!https_rt_acquire()) { fail("update: the secure connection could not start"); goto out; }
    have_rt = true;
    if (httpCreateClient(&client) < 0) { client = 0; fail("update: no HTTP client"); goto out; }
    httpClientSetConnTimeout(client, DL_TIMEOUT_US);
    httpClientSetSendTimeout(client, DL_TIMEOUT_US);
    httpClientSetRecvTimeout(client, DL_TIMEOUT_US);
    httpClientSetUserAgent(client, HTTP_USER_AGENT);
    httpClientSetAutoRedirect(client, 1);
    httpClientSetSslCallback(client, dl_ssl_cb, NULL);
    {
        httpUri uri;
        u32 pool = 0;
        if (httpUtilParseUri(&uri, url, NULL, 0, &pool) < 0 ||
            !(uri_pool = malloc(pool)) ||
            httpUtilParseUri(&uri, url, uri_pool, pool, NULL) < 0) {
            fail("update: bad download address"); goto out;
        }
        if (httpCreateTransaction(&trans, client, HTTP_METHOD_GET, &uri) < 0) {
            trans = 0; fail("update: could not start the download"); goto out;
        }
    }
    if (accept) {
        httpHeader h;
        h.name = "Accept"; h.value = accept;
        httpRequestAddHeader(trans, &h);
    }
    rc = httpSendRequest(trans, NULL, 0, NULL);
    if (rc < 0) {
        snprintf(m, sizeof m, "update: could not reach GitHub (0x%08x)", (u32)rc);
        fail(m); goto out;
    }
    {
        s32 code = 0;
        httpResponseGetStatusCode(trans, &code);
        if (code == 404) { result = -1; snprintf(m, sizeof m, "update: not found at %.100s", url); plog(m); goto out; }
        if (code != 200) {
            snprintf(m, sizeof m, "update: GitHub answered %d", (int)code);
            fail(m); goto out;
        }
        u64 len = 0;
        if (httpResponseGetContentLength(trans, &len) == 0 && len > 0 && len < 0xFFFFFFFFull)
            s_total = (u32)len;
        else
            s_total = s_asset.size;
    }
    sysLv2FsMkdir(PKG_DIR, 0777);
    f = fopen(part, "wb");
    if (!f) { fail("update: cannot write to /dev_hdd0/packages"); goto out; }
    buf = (char *)malloc(DL_CHUNK);
    if (!buf) { fail("update: out of memory"); goto out; }
    sha256_init(&sha);
    for (;;) {
        if (s_cancel) { fail("update: cancelled"); goto out; }
        u32 n = 0;
        rc = httpRecvResponse(trans, buf, DL_CHUNK, &n);
        if (rc != 0) {
            snprintf(m, sizeof m, "update: the download was interrupted (0x%08x)", (u32)rc);
            fail(m); goto out;
        }
        if (n == 0) break;
        if (fwrite(buf, 1, n, f) != n) { fail("update: the hard disk is full"); goto out; }
        sha256_update(&sha, buf, n);
        s_got += n;
    }
    if (fclose(f) != 0) { f = NULL; fail("update: could not finish writing the package"); goto out; }
    f = NULL;
    if (s_asset.size && s_got != s_asset.size) { fail("update: the download is incomplete"); goto out; }
    if (s_asset.sha256[0]) {
        uint8_t d[32]; char hex[65];
        sha256_final(&sha, d);
        sha256_hex(d, hex);
        if (strcmp(hex, s_asset.sha256) != 0) { fail("update: the download is damaged (checksum)"); goto out; }
    }
    remove(s_path);
    if (rename(part, s_path) != 0) { fail("update: could not save the package"); goto out; }
    result = 1;
    snprintf(m, sizeof m, "update: package downloaded and verified from %.90s", url);
    plog(m);

out:
    if (f) fclose(f);
    if (result != 1) remove(part);
    free(buf);
    if (trans) httpDestroyTransaction(trans);
    if (client) httpDestroyClient(client);
    free(uri_pool);
    if (have_rt) https_rt_release();
    return result;
}

static bool download(void) {
    // The repository's copy first (reachable from a PS3), then the asset link.
    char mirror[256];
    snprintf(mirror, sizeof mirror, MIRROR_URL, s_tag);
    int r = fetch(mirror, "application/vnd.github.raw");
    if (r == 1) return true;
    if (s_cancel) return false;
    char first[96];
    snprintf(first, sizeof first, "%s", s_err);
    r = fetch(s_asset.url, NULL);
    if (r == 1) return true;
    if (r == -1 && first[0]) snprintf(s_err, sizeof s_err, "%s", first);
    if (!s_err[0]) fail("update: the package is not on GitHub");
    return false;
}

static void dl_thread(void *arg) {
    (void)arg;
    const bool ok = download();
    s_state = ok ? UDL_READY : UDL_FAILED;
    __sync_synchronize();
    s_done = true;
    sysThreadExit(0);
}

bool update_dl_start(void) {
    if (s_thread || s_state == UDL_RUNNING) return false;
    if (!update_check_asset(&s_asset) || !update_check_result(s_tag, sizeof s_tag)) {
        fail("update: this release has no package to download");
        s_state = UDL_FAILED;
        return false;
    }
    // The tag goes into a file name: keep it to letters, digits, '.', '-'.
    char safe[64]; int n = 0;
    for (const char *p = s_tag; *p && n < (int)sizeof safe - 1; p++)
        if ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
            *p == '.' || *p == '-') safe[n++] = *p;
    safe[n] = '\0';
    snprintf(s_path, sizeof s_path, PKG_DIR "/JellyFin-PS3-%s.pkg", n ? safe : "update");
    s_got = s_total = 0;
    s_err[0] = '\0';
    s_cancel = false;
    s_done = false;
    s_state = UDL_RUNNING;
    static char name[] = "jf_update_dl";
    if (sysThreadCreate(&s_tid, dl_thread, NULL, 1500, 64 * 1024, THREAD_JOINABLE, name) != 0) {
        fail("update: could not start the download");
        s_state = UDL_FAILED;
        return false;
    }
    s_thread = true;
    return true;
}

void update_dl_cancel(void) { if (s_state == UDL_RUNNING) s_cancel = true; }

void update_dl_poll(void) {
    if (!s_thread || !s_done) return;
    u64 ret;
    sysThreadJoin(s_tid, &ret);
    s_thread = false;
}

int update_dl_state(void) { return s_state; }
void update_dl_progress(u32 *got, u32 *total) { if (got) *got = s_got; if (total) *total = s_total; }
const char *update_dl_error(void) { return s_err; }
const char *update_dl_path(void) { return s_path; }

bool update_install(void) {
    if (s_state != UDL_READY) return false;
    char url[256], resp[512];
    snprintf(url, sizeof url, WEBMAN_INSTALL "%s", s_path);
    const int st = http_probe_get(url, NULL, resp, sizeof resp);
    char m[96];
    snprintf(m, sizeof m, "update: webMAN install request -> %d", st);
    plog(m);
    return st == 200;
}
