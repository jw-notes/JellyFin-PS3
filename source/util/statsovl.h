#pragma once
#include <ppu-types.h>

// -------------------------------------------------------------------------
//  Player Stats Overlay toggle
// -------------------------------------------------------------------------
//  Runtime on/off switch for the playback diagnostics overlay (player_stats).
//  It controls DISPLAY ONLY: when off, nothing is drawn and no text layout
//  runs, but the metric hooks keep updating their counters so the numbers are
//  already warm the moment it is switched on mid-movie.
//
//  Stripping the measurement code entirely is a separate, compile-time
//  decision — ENABLE_PLAYER_STATS in build_config.h.  This store deliberately
//  lives OUTSIDE player_stats.cpp so the settings screen still links when that
//  file is compiled out.
//
//  Default is OFF.  Persisted as "0"/"1" in the app data dir next to the
//  other settings files.

void statsovl_load(void);            // read the persisted value (once, at startup)
void statsovl_save(void);            // persist the current value
bool statsovl_enabled(void);         // the setting: shown from the start of every playback
void statsovl_set_enabled(bool on);  // set + persist immediately
// R3 (and the remote's INFO key, and Options) shows or hides it for this playback only, whatever the
// setting; the next playback starts as the setting says again.
bool statsovl_shown(void);           // true => draw the overlay now
void statsovl_toggle_shown(void);
void statsovl_session_end(void);     // the playback is over: back to the setting
