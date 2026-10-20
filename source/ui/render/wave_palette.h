#ifndef WAVE_PALETTE_H
#define WAVE_PALETTE_H

// The JellyWave / JellyDrop colours, at run time so a theme can repaint them.
//
// Every colour wave_gel.h, wave_light.h and wave_drop.h read used to be a
// compile-time constant from the design.  They are now this one palette, which
// starts as exactly those constants (XMB wave is unchanged, bit for bit) and is
// re-derived from the theme's lockup pair when the theme changes -- see
// jw_palette_for() and ui_wave.cpp.  Pure C so the host tests compile it.
//
// g_jw_pal is a weak definition in this header: every file that includes it
// emits one, and the linker keeps a single copy, so the host tests need no
// extra source and the console build has one palette.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float purple[3], blue[3];   // the body's two ends (the brand pair)
    float rimc[3];              // rim glow
    float deep[3];              // the body's shadowed core
    float fringe[3];            // the outer fringe
    float key[3], fill[3], rim[3];   // the three lights
    float sky[3], gnd[3];       // ambient, upper and lower lobes
    float glow[3];              // specular highlight
} jw_palette;

#define JW_PALETTE_DEFAULT { \
    { 0.666667f, 0.360784f, 0.764706f },   /* purple  #AA5CC3 */ \
    { 0.000000f, 0.643137f, 0.862745f },   /* blue    #00A4DC */ \
    { 0.180000f, 0.840000f, 1.000000f },   /* rimc            */ \
    { 0.082353f, 0.039216f, 0.133333f },   /* deep    #150A22 */ \
    { 0.760000f, 0.360000f, 1.000000f },   /* fringe          */ \
    { 0.862745f, 0.937255f, 1.000000f },   /* key     #DCEFFF */ \
    { 0.541176f, 0.309804f, 0.690196f },   /* fill    #8A4FB0 */ \
    { 0.000000f, 0.643137f, 0.862745f },   /* rim     #00A4DC */ \
    { 0.170196f, 0.236471f, 0.369020f },   /* sky             */ \
    { 0.202353f, 0.137647f, 0.292157f },   /* gnd             */ \
    { 0.220000f, 0.780000f, 1.000000f },   /* glow            */ \
}

__attribute__((weak)) jw_palette g_jw_pal = JW_PALETTE_DEFAULT;

// The design's own pair: a theme using it gets the design's palette untouched.
#define JW_PAL_BRAND_A 0x00AA5CC3u
#define JW_PAL_BRAND_B 0x0000A4DCu

static inline void jw_pal_rgb(uint32_t c, float o[3]) {
    o[0] = (float)((c >> 16) & 0xFF) / 255.0f;
    o[1] = (float)((c >>  8) & 0xFF) / 255.0f;
    o[2] = (float)( c        & 0xFF) / 255.0f;
}

static inline void jw_pal_mix(const float a[3], const float b[3], float t, float o[3]) {
    for (int i = 0; i < 3; i++) o[i] = a[i] + (b[i] - a[i]) * t;
}

static inline void jw_pal_scale(const float a[3], float k, float o[3]) {
    for (int i = 0; i < 3; i++) o[i] = a[i] * k;
}

// The palette for a lockup pair (a = the "purple" end, b = the "blue" end).
// The brand pair returns the design's constants; any other pair derives every
// colour from the two the same way the design's relate to its own pair:
// highlights are the pair mixed toward white, the core and the ambient are
// the pair darkened.
static inline jw_palette jw_palette_for(uint32_t a, uint32_t b) {
    const jw_palette def = JW_PALETTE_DEFAULT;
    if ((a & 0xFFFFFFu) == JW_PAL_BRAND_A && (b & 0xFFFFFFu) == JW_PAL_BRAND_B) return def;
    static const float white[3] = { 1.0f, 1.0f, 1.0f };
    jw_palette p;
    jw_pal_rgb(a, p.purple);
    jw_pal_rgb(b, p.blue);
    jw_pal_mix(p.blue, white, 0.30f, p.rimc);
    jw_pal_scale(p.purple, 0.18f, p.deep);
    jw_pal_mix(p.purple, white, 0.30f, p.fringe);
    jw_pal_mix(white, p.blue, 0.14f, p.key);
    jw_pal_scale(p.purple, 0.80f, p.fill);
    for (int i = 0; i < 3; i++) p.rim[i] = p.blue[i];
    float base[3], tb[3], ta[3];
    jw_pal_scale(p.purple, 0.22f, base);
    jw_pal_scale(p.blue, 0.60f, tb);
    jw_pal_scale(p.purple, 0.60f, ta);
    jw_pal_mix(base, tb, 0.35f, p.sky);
    jw_pal_mix(base, ta, 0.15f, p.gnd);
    jw_pal_mix(p.blue, white, 0.35f, p.glow);
    return p;
}

#ifdef __cplusplus
}
#endif

#endif
