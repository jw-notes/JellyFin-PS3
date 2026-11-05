#pragma once

// Live TV reserve length (Settings > Playback > Live TV Buffer), saved apart
// from the quality.
int livetv_buffer_step_get(void);
unsigned livetv_buffer_seconds(void);
const char *livetv_buffer_label(void);
void livetv_buffer_step(int direction);

// Settings > Live TV > On Now on Home: Home's row of what is on now.  On by default.
bool livetv_onnow_enabled(void);
void livetv_onnow_toggle(void);
