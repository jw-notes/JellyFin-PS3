#pragma once
#include <stdbool.h>
#include <stdint.h>

// -------------------------------------------------------------------------
//  Live TV: when the player has to ask for a fresh stream
// -------------------------------------------------------------------------
//  Pure (no PS3 headers) so tests/test_live_watch.c compiles the same code the
//  player runs.  The player calls live_watch_step() once per loop iteration
//  and acts on what it returns; the clock is timing_get_us().
//
//  A live stream is only good for as long as somebody reads it:
//    * paused longer than LIVE_PAUSE_REOPEN_US, the server has stopped
//      producing and what is buffered is stale -> resume on a fresh stream;
//    * the decode thread reached the stream's end -> the server dropped it;
//    * no new picture for LIVE_STALL_US while playing -> it stopped sending;
//    * no new picture for LIVE_SOUND_STALL_US while the sound played on more
//      than LIVE_SOUND_AHEAD_US past the last picture -> a stitched ad break
//      left the picture behind, and only a fresh stream brings it back.
//  A stall reopens at most LIVE_RECONNECTS_PER_MIN times a minute and
//  LIVE_RECONNECT_GAP_US apart, so a dead channel waits instead of looping.
//  Any other reopen that is followed by another within LIVE_QUICK_REOPEN_US,
//  LIVE_QUICK_REOPEN_MAX times running, is a stream that will not stay up:
//  playback ends instead of asking the server again and again.

#define LIVE_PAUSE_REOPEN_US     10000000ULL
#define LIVE_STALL_US             6000000ULL
#define LIVE_SOUND_STALL_US       2500000ULL
#define LIVE_SOUND_AHEAD_US       2000000ULL
#define LIVE_RECONNECT_GAP_US    20000000ULL
#define LIVE_RECONNECT_WINDOW_US 60000000ULL
#define LIVE_RECONNECTS_PER_MIN  2
#define LIVE_QUICK_REOPEN_US     10000000ULL
#define LIVE_QUICK_REOPEN_MAX    3

typedef struct {
    uint64_t paused_at_us;     // 0 = not paused
    int      stall_frames;     // the frame count last seen, -1 = none yet
    uint64_t stall_since_us;   // when it last changed
    uint64_t stall_audio_us;   // the audio clock at that moment, 0 = none yet
    uint64_t reconnect_us;     // the last stall reopen, 0 = none
    uint64_t window_us;        // when the current minute of them began
    int      reconnects;       // stall reopens in that minute
    uint64_t last_open_us;     // when the stream was last (re)opened
    int      quick_opens;      // consecutive automatic reopens inside the window
} LiveWatch;

typedef enum {
    LIVE_EV_NONE = 0,
    LIVE_EV_RESUME_STALE,      // un-paused after too long a pause
    LIVE_EV_STALLED,           // playing, but no new picture
    LIVE_EV_ENDED              // the stream ended
} LiveEvent;

static inline void live_watch_init(LiveWatch *w, uint64_t now_us) {
    w->paused_at_us   = 0;
    w->stall_frames   = -1;
    w->stall_since_us = now_us;
    w->stall_audio_us = 0;
    w->reconnect_us   = 0;
    w->window_us      = 0;
    w->reconnects     = 0;
    w->last_open_us   = now_us;
    w->quick_opens    = 0;
}

// May a stall reopen now?  True counts it as taken.
static inline bool live_watch_may_reconnect(LiveWatch *w, uint64_t now_us) {
    if (w->reconnect_us && now_us - w->reconnect_us < LIVE_RECONNECT_GAP_US) return false;
    if (!w->window_us || now_us - w->window_us >= LIVE_RECONNECT_WINDOW_US) {
        w->window_us  = now_us;
        w->reconnects = 0;
    }
    if (w->reconnects >= LIVE_RECONNECTS_PER_MIN) return false;
    w->reconnects++;
    w->reconnect_us = now_us;
    return true;
}

// One loop iteration.  frame_count is the player's pictures shown so far; it
// is 0 until the first, and a stream with no picture yet is not stalled.
// audio_us is the stream's audio clock, 0 while it has none.
static inline LiveEvent live_watch_step(LiveWatch *w, uint64_t now_us, bool paused,
                                        int frame_count, bool ended, uint64_t audio_us) {
    LiveEvent ev = LIVE_EV_NONE;
    if (paused) {
        if (!w->paused_at_us) w->paused_at_us = now_us;
    } else if (w->paused_at_us) {
        if (now_us - w->paused_at_us > LIVE_PAUSE_REOPEN_US) ev = LIVE_EV_RESUME_STALE;
        w->paused_at_us = 0;
        w->stall_since_us = now_us;        // a pause is not a stall
    }
    if (!paused && frame_count > 0) {
        if (frame_count != w->stall_frames) {
            w->stall_frames   = frame_count;
            w->stall_since_us = now_us;
            w->stall_audio_us = audio_us;
        } else {
            if (!w->stall_audio_us || audio_us < w->stall_audio_us) w->stall_audio_us = audio_us;
            const uint64_t still = now_us - w->stall_since_us;
            const bool sound_on  = w->stall_audio_us &&
                                   audio_us > w->stall_audio_us + LIVE_SOUND_AHEAD_US;
            if (ev == LIVE_EV_NONE &&
                (still > LIVE_STALL_US || (sound_on && still > LIVE_SOUND_STALL_US)) &&
                live_watch_may_reconnect(w, now_us))
                ev = LIVE_EV_STALLED;
        }
    } else {
        w->stall_since_us = now_us;
        w->stall_audio_us = 0;
    }
    if (ev == LIVE_EV_NONE && ended && !paused) ev = LIVE_EV_ENDED;
    return ev;
}

// A reopen the player decided on, other than a stall (which has its own limit),
// is about to run.  True when the stream keeps dropping and playback should end.
static inline bool live_watch_looping(LiveWatch *w, uint64_t now_us) {
    if (now_us - w->last_open_us < LIVE_QUICK_REOPEN_US) w->quick_opens++;
    else                                                 w->quick_opens = 0;
    return w->quick_opens >= LIVE_QUICK_REOPEN_MAX;
}

// Any reopen has finished (opened or not): measure the next from here.
static inline void live_watch_opened(LiveWatch *w, uint64_t now_us) {
    w->last_open_us   = now_us;
    w->stall_frames   = -1;
    w->stall_since_us = now_us;
    w->stall_audio_us = 0;
    w->paused_at_us   = 0;
}

// Another channel: the limit on stall reopens starts again.
static inline void live_watch_channel_changed(LiveWatch *w) {
    w->reconnect_us = 0;
    w->window_us    = 0;
    w->reconnects   = 0;
}
