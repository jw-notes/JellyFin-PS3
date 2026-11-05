#pragma once
// -------------------------------------------------------------------------
//  Skip Intros / Skip Credits
// -------------------------------------------------------------------------
//  Settings > Playback, each one Off, Button (the default) or Auto.  Off: the
//  segment is left alone.  Button: a badge offers it and X takes it.  Auto: it
//  goes by itself.  Skip Intros covers recaps too.  Credits with a next episode
//  go to that episode; without one, to the credits' end.  Credits set to Off
//  also keep the next episode's countdown to the last 25 s.

typedef enum { SKIP_OFF = 0, SKIP_BUTTON, SKIP_AUTO, SKIP_MODES } SkipMode;

void        autoskip_load(void);         // read the persisted values (once, at startup)
SkipMode    skip_intro_mode(void);
SkipMode    skip_credits_mode(void);
void        skip_intro_step(int dir);    // set + persist immediately
void        skip_credits_step(int dir);
const char *skip_mode_label(SkipMode m); // English, for tr()
