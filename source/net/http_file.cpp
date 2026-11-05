// Copyright (C) 2026 jw-notes.  GPLv3, see LICENSE.
// A file on the server, read like a file on a drive.  See http_file.h.

#include "http_file.h"
#include "http_range.h"
#include "http.h"
#include "jellyfin_api.h"
#include "lfs.h"
#include "plog.h"
#include "timing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ppu-types.h>
#include <net/net.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/poll.h>

extern u32 running;

// Movian's window on this console.  The larger one the stream socket starts on
// needs the fallback that watches for sender backoff, and a seek here would
// reopen the connection under it.
#define HF_RCVBUF         (128 * 1024)
#define HF_MAX            3
#define HF_SKIP_MAX       (256u * 1024u)       // nearer than this: read on, do not reconnect
#define HF_HDR_DEADLINE   (30ULL * 1000000ULL)
#define HF_READ_DEADLINE  (15ULL * 1000000ULL)
#define HF_TRIES          3

typedef struct {
    bool     used;
    int      sock;       // -1: no connection
    uint64_t pos;        // the file offset of the next byte on it
    uint64_t size;
} HttpFile;

static HttpFile s_f[HF_MAX];
static char     s_host[64];
static int      s_port = 8096;
static char     s_path[512];

static void hf_drop(HttpFile *f) {
    if (f->sock >= 0) netClose(f->sock);
    f->sock = -1;
}

// One connection, positioned at `offset`, its headers read.
static bool hf_connect(HttpFile *f, uint64_t offset) {
    hf_drop(f);
    const int sock = netSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) return false;
    int rb = HF_RCVBUF;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rb, sizeof rb);    // before the connect: the SYN carries the scale

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((u16)s_port);
    unsigned a = 0, b = 0, c = 0, d = 0;
    sscanf(s_host, "%u.%u.%u.%u", &a, &b, &c, &d);
    addr.sin_addr.s_addr = htonl((a << 24) | (b << 16) | (c << 8) | d);
    if (netConnect(sock, (struct sockaddr *)&addr, sizeof addr) < 0) {
        netClose(sock);
        return false;
    }
    { struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 500000;
      netSetSockOpt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv); }

    static char req[1536];
    const int n = snprintf(req, sizeof req,
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Authorization: MediaBrowser Client=\"PS3\", Device=\"PS3\", DeviceId=\"%s\", Version=\"0.1\", Token=\"%s\"\r\n"
        "Range: bytes=%llu-\r\n"
        "User-Agent: " HTTP_USER_AGENT "\r\n"
        "Connection: close\r\n"
        "\r\n",
        s_path, s_host, s_port, jf_device_id(), g_token, (unsigned long long)offset);
    if (n <= 0 || n >= (int)sizeof req || netSend(sock, req, n, 0) != n) {
        netClose(sock);
        return false;
    }

    char hdr[4096];
    int have = 0;
    const u64 t0 = timing_get_us();
    for (;;) {
        if (have >= (int)sizeof hdr - 1 || !running || timing_get_us() - t0 >= HF_HDR_DEADLINE) {
            netClose(sock);
            return false;
        }
        struct pollfd pfd; pfd.fd = sock; pfd.events = POLLIN; pfd.revents = 0;
        const int got = netPoll(&pfd, 1, 50) == 0 ? -1 : netRecv(sock, hdr + have, 1, MSG_DONTWAIT);
        if (got == 0) { netClose(sock); return false; }
        if (got < 0) continue;
        have++;
        if (have >= 4 && memcmp(hdr + have - 4, "\r\n\r\n", 4) == 0) break;
    }
    hdr[have] = '\0';
    uint64_t total = 0;
    if (!http_range_parse(hdr, offset, &total)) {
        char line[64];
        snprintf(line, sizeof line, "http_file: refused at %llu: %.30s", (unsigned long long)offset, hdr);
        plog(line);
        netClose(sock);
        return false;
    }
    f->sock = sock;
    f->pos  = offset;
    f->size = total;
    return true;
}

// Up to `want` bytes from the connection: > 0 read, 0 the server closed it, < 0 nothing in time.
static int hf_recv(HttpFile *f, uint8_t *buf, uint32_t want) {
    const u64 t0 = timing_get_us();
    while (running && timing_get_us() - t0 < HF_READ_DEADLINE) {
        const int got = netRecv(f->sock, buf, want, 0);
        if (got >= 0) return got;
    }
    return -1;
}

static int hf_open(void) {
    if (!s_host[0]) return -1;
    for (int i = 0; i < HF_MAX; i++) {
        if (s_f[i].used) continue;
        memset(&s_f[i], 0, sizeof s_f[i]);
        s_f[i].sock = -1;
        if (!hf_connect(&s_f[i], 0)) return -1;
        s_f[i].used = true;
        return i;
    }
    return -1;
}

static uint64_t hf_size(int h) { return h >= 0 && h < HF_MAX && s_f[h].used ? s_f[h].size : 0; }

static void hf_close(int h) {
    if (h < 0 || h >= HF_MAX || !s_f[h].used) return;
    hf_drop(&s_f[h]);
    s_f[h].used = false;
}

static int hf_read(int h, uint64_t off, void *out, uint32_t n) {
    if (h < 0 || h >= HF_MAX || !s_f[h].used) return LFS_E_IO;
    HttpFile *f = &s_f[h];
    if (off >= f->size) return 0;
    if (n > f->size - off) n = (uint32_t)(f->size - off);
    uint8_t *p = (uint8_t *)out;
    uint32_t done = 0;
    int tries = 0;
    while (done < n) {
        const uint64_t at = off + done;
        if (f->sock >= 0 && at != f->pos) {
            if (at < f->pos || at - f->pos > HF_SKIP_MAX) {
                hf_drop(f);
            } else {
                static uint8_t skip[16 * 1024];
                const uint64_t gap = at - f->pos;
                const int got = hf_recv(f, skip, gap < sizeof skip ? (uint32_t)gap : (uint32_t)sizeof skip);
                if (got > 0) f->pos += (uint64_t)got;
                else         hf_drop(f);
                continue;
            }
        }
        if (f->sock < 0 && (tries++ >= HF_TRIES || !hf_connect(f, at))) {
            if (tries > HF_TRIES) return done ? (int)done : LFS_E_IO;
            continue;
        }
        const int got = hf_recv(f, p + done, n - done);
        if (got <= 0) {             // closed early, or quiet too long: connect again here
            hf_drop(f);
            continue;
        }
        done += (uint32_t)got;
        f->pos += (uint64_t)got;
        tries = 0;
    }
    return (int)done;
}

bool http_file_set_url(const char *url) {
    static const LfsNet net = { HTTP_FILE_PATH, hf_open, hf_size, hf_close, hf_read };
    s_host[0] = '\0';
    if (strncmp(url, "http://", 7) != 0) return false;
    const char *p = url + 7, *h = p;
    while (*p && *p != ':' && *p != '/') p++;
    const size_t hl = (size_t)(p - h);
    if (hl == 0 || hl >= sizeof s_host) return false;
    int port = 8096;
    if (*p == ':') { port = atoi(p + 1); while (*p && *p != '/') p++; }
    if (*p != '/' || strlen(p) >= sizeof s_path) return false;
    memcpy(s_host, h, hl);
    s_host[hl] = '\0';
    s_port = port;
    snprintf(s_path, sizeof s_path, "%s", p);
    lfs_set_net(&net);
    return true;
}
