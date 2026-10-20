#pragma once
// Host stand-in for source/ui/ui_visuals.h (720p metrics, colour tokens).
#include "ui.h"
#define UIS_W(x) (x)
#define UIS_H(x) (x)
#define UIS_TF(x) ((float)(x))
#define OSK_UNITS_MAX 11
#define OSK_GAP       UIS_W(8)
#define OSK_KEY_W     80
#define OSK_KEY_H     UIS_H(44)
#define OSK_STEP_X    (OSK_KEY_W + OSK_GAP)
#define OSK_STEP_Y    (OSK_KEY_H + OSK_GAP)
#define XMB_CONTENT_Y 120
#define XMB_DIVIDER_Y 110
#define XMB_ITEM_PAD  40
enum { XMB_BG, XMB_TRACK, XMB_ACCENT, XMB_ACCENT_DEEP, XMB_KEY_SEL, XMB_KEY_NORMAL,
       XMB_TEXT, XMB_TEXT_DIM, XMB_KEY_LABEL_SEL, ICON_BACKSPACE };
typedef struct { char glyph; const char *label; } Hint;
#define TR(s)  (s)
#define TRN(s) (s)
void drawRect(u32 x, u32 y, u32 w, u32 h, u32 c);
void drawTTF(u32 x, u32 y, const char *s, float px, u32 c, bool bold = false);
int  ttf_text_width(const char *s, float px, bool bold = false);
void drawIcon(u32 x, u32 y, int icon, float px, u32 c);
void draw_hints_bar(const Hint *h, int n);
void xmb_draw_divider(void);
