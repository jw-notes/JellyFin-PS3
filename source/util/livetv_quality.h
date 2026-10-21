#pragma once
#include "vquality.h"

// Channels have their own saved quality; film/per-title choices never affect it.
vquality_t livetv_quality_get(void);
unsigned livetv_quality_bps(void);
const char *livetv_quality_label(void);
void livetv_quality_step(int direction);
