#pragma once
#include <ppu-types.h>

// Screenshots on request over FTP (see screenshot.cpp).  Called from
// waitflip() with the buffer now on screen.
void screenshot_frame(const u32 *displayed);
