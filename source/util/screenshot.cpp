// Screenshots for the README and bug reports, taken over FTP: write a request
// to /dev_hdd0/tmp/jf_shot_req.txt and the app saves what is on screen to
// /dev_hdd0/tmp/jfshot/<name>.bmp.
//
//   home                  one shot, named home.bmp
//   burst tour 30 6       30 shots, one every 6 s: tour-01.bmp ... tour-30.bmp
//
// Checked twice a second from waitflip(), so it costs nothing when unused.  A
// shot reads the displayed frame back from video memory, which this hardware
// does slowly: the screen holds still for about a second while it is taken.

#include <stdio.h>
#include <string.h>
#include <sys/file.h>

#include "screenshot.h"
#include "rsxutil.h"
#include "timing.h"
#include "plog.h"

#define REQ_FILE "/dev_hdd0/tmp/jf_shot_req.txt"
#define SHOT_DIR "/dev_hdd0/tmp/jfshot"

static u32  s_frame = 0;
static char s_name[48];          // a one-off shot waiting, "" when none
static char s_prefix[32];
static int  s_left = 0, s_seq = 0;
static u64  s_next_us = 0, s_gap_us = 0;

static void put16(FILE *f, unsigned v) { fputc(v & 0xFF, f); fputc((v >> 8) & 0xFF, f); }
static void put32(FILE *f, unsigned v) { put16(f, v & 0xFFFF); put16(f, v >> 16); }

static void save(const u32 *fb, const char *name) {
    const u32 W = display_width, H = display_height;
    if (!fb || !W || !H || W > 1920) return;
    char path[96];
    snprintf(path, sizeof path, SHOT_DIR "/%s.bmp", name);
    sysLv2FsMkdir(SHOT_DIR, 0777);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    const u32 row = (W * 3 + 3) & ~3u, size = 54 + row * H;
    fputc('B', f); fputc('M', f); put32(f, size); put32(f, 0); put32(f, 54);
    put32(f, 40); put32(f, W); put32(f, H); put16(f, 1); put16(f, 24);
    put32(f, 0); put32(f, row * H); put32(f, 2835); put32(f, 2835); put32(f, 0); put32(f, 0);
    static u32 in[1920];
    static unsigned char out[1920 * 3 + 4];
    for (int y = (int)H - 1; y >= 0; y--) {                 // BMP rows run bottom-up
        memcpy(in, fb + (size_t)y * (color_pitch / 4), W * 4);
        for (u32 x = 0; x < W; x++) {
            out[x * 3]     = (unsigned char)in[x];           // B
            out[x * 3 + 1] = (unsigned char)(in[x] >> 8);    // G
            out[x * 3 + 2] = (unsigned char)(in[x] >> 16);   // R
        }
        memset(out + W * 3, 0, row - W * 3);
        fwrite(out, 1, row, f);
    }
    fclose(f);
    char m[112];
    snprintf(m, sizeof m, "screenshot: %s (%ux%u)", path, W, H);
    plog(m);
}

static void read_request(void) {
    FILE *f = fopen(REQ_FILE, "r");
    if (!f) return;
    char line[96] = "";
    if (!fgets(line, sizeof line, f)) line[0] = '\0';
    fclose(f);
    remove(REQ_FILE);
    char a[32] = "", b[32] = "";
    int count = 0, secs = 0;
    const int n = sscanf(line, "%31s %31s %d %d", a, b, &count, &secs);
    if (n == 4 && strcmp(a, "burst") == 0 && count > 0 && secs > 0) {
        snprintf(s_prefix, sizeof s_prefix, "%s", b);
        s_left = count > 99 ? 99 : count;
        s_seq = 0;
        s_gap_us = (u64)secs * 1000000ull;
        s_next_us = timing_get_us() + 1000000ull;
    } else if (n >= 1) {
        snprintf(s_name, sizeof s_name, "%s", a);
    }
}

void screenshot_frame(const u32 *displayed) {
    if (++s_frame % 30 == 0) read_request();
    if (s_name[0]) { save(displayed, s_name); s_name[0] = '\0'; return; }
    if (s_left > 0 && timing_get_us() >= s_next_us) {
        char name[48];
        snprintf(name, sizeof name, "%s-%02d", s_prefix, ++s_seq);
        save(displayed, name);
        s_left--;
        s_next_us = timing_get_us() + s_gap_us;
    }
}
