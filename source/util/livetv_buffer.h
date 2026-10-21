#pragma once

// Live TV reserve length (Settings > Playback > Live TV Buffer), saved apart
// from the quality.
int livetv_buffer_step_get(void);
unsigned livetv_buffer_seconds(void);
const char *livetv_buffer_label(void);
void livetv_buffer_step(int direction);
