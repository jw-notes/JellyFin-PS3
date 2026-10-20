#pragma once

// Pixel geometry shared by Classic library rendering and host layout checks.
// All insets and sizes arrive already scaled by the UI's normal scale policy.
struct ClassicGrid {
    int rows, cols, card_w, card_h, stride, grid_w, x0;
};

static inline ClassicGrid classic_grid_make(int width, int height, int margin,
        int top, int bottom, int text_h, int gap_x, int gap_y, bool portrait, bool square = false) {
    ClassicGrid g;
    int usable_w = width - margin * 2;
    int usable_h = height - bottom - top;
    if (usable_w < 1) usable_w = 1;
    if (usable_h < 2) usable_h = 2;
    g.rows = square && usable_h < 2 * (text_h + gap_y + 40) ? 1 : 2;
    g.stride = usable_h / g.rows;
    g.card_h = g.stride - text_h - gap_y;
    if (g.card_h < 1) g.card_h = 1;
    g.card_w = square ? g.card_h : portrait ? g.card_h * 2 / 3 : g.card_h * 16 / 9;
    if (g.card_w < 1) g.card_w = 1;
    if (g.card_w > usable_w) {
        g.card_w = usable_w;
        g.card_h = square ? g.card_w : portrait ? g.card_w * 3 / 2 : g.card_w * 9 / 16;
        if (g.card_h < 1) g.card_h = 1;
    }
    g.cols = (usable_w + gap_x) / (g.card_w + gap_x);
    if (g.cols < 1) g.cols = 1;
    if (g.cols > 12) g.cols = 12;
    g.grid_w = g.cols * g.card_w + (g.cols - 1) * gap_x;
    g.x0 = (width - g.grid_w) / 2;
    return g;
}
