#pragma once
// Host stand-in for source/ui/ui.h: just what the drawn login keyboard uses.
#include <stdint.h>
typedef uint32_t u32;
typedef struct { bool up, down, left, right, cross, circle, square, start; } Buttons;
extern Buttons btn_cur, btn_prev;
extern u32 running;
extern u32 display_width, display_height;
#define BTN_PRESSED(b) (btn_cur.b && !btn_prev.b)
#define BTN_REPEAT(b)  BTN_PRESSED(b)
void init_btns(void);
void poll_buttons(void);
void waitflip(void);
void flip(void);
void clearScreen(u32 c);
void rsxSync(void);
