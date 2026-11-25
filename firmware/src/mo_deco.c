/* SPDX-License-Identifier: GPL-3.0-only */
/* Mechanica Obscura: the "Weimar clockpunk" layer on top of mo_gfx.c: baked backdrops, the Deco alphabet,
 * lacquer panels. */

/* ---- backdrops: baked art (tools/style/bake.py -> mo_art.h), one decoded at a time into an index buffer,
 * expanded through its palette into each frame. A scene change costs one LZ4 decode (~58 KB out). */
#include "mo_art.h"
static uint8_t bg_idx[SW * SH] __attribute__((section(".pool")));
static uint16_t bg_pal_sw[64];                  /* the palette, byte-swapped as the panel wants it */
static int32_t bg_cur = -1;

static int lz4_decode(const uint8_t *src, uint32_t n, uint8_t *dst, uint32_t cap)
{
    uint32_t i = 0, o = 0;
    while (i < n) {
        uint32_t tok = src[i++], ll = tok >> 4, ml, off;
        if (ll == 15u) { uint32_t b; do { if (i >= n) return -1; b = src[i++]; ll += b; } while (b == 255u); }
        if (i + ll > n || o + ll > cap) return -1;
        while (ll--) dst[o++] = src[i++];
        if (i >= n) break;                      /* the last sequence has literals only */
        if (i + 2u > n) return -1;
        off = (uint32_t)src[i] | (uint32_t)src[i + 1] << 8;
        i += 2;
        ml = tok & 15u;
        if (ml == 15u) { uint32_t b; do { if (i >= n) return -1; b = src[i++]; ml += b; } while (b == 255u); }
        ml += 4u;
        if (!off || off > o || o + ml > cap) return -1;
        while (ml--) { dst[o] = dst[o - off]; o++; }
    }
    return (int)o;
}
/* Obscura Line's grade over the baked clockpunk art: a gradient map by brightness (indigo, violet, magenta, rose,
 * a pale warm white), mixed with the original so brass stays brass: warm saturated colours keep more of
 * themselves. One pass over a backdrop's palette when it loads (64 colours), nothing a frame. */
static uint16_t synth_grade(uint16_t c)
{
    static const uint16_t RAMP[5] = {RGB(8, 4, 24), RGB(56, 22, 96), RGB(196, 48, 156), RGB(255, 132, 178), RGB(255, 236, 222)};
    int32_t r = c >> 11, g = (c >> 5) & 63, b = c & 31, l = (r * 2 * 77 + g * 151 + b * 2 * 28) >> 6;   /* 0..~255 */
    int32_t mx = r * 2 > g ? (r * 2 > b * 2 ? r * 2 : b * 2) : (g > b * 2 ? g : b * 2), mn = r * 2 < g ? (r * 2 < b * 2 ? r * 2 : b * 2) : (g < b * 2 ? g : b * 2);
    int32_t sat = mx - mn, warm = r * 2 > b * 2 + 10 && g > b * 2 ? sat : 0, k, f;
    uint32_t keep;
    uint16_t m;
    if (l > 255) l = 255;
    k = l * 4 / 256;
    f = (l * 4 - k * 256);
    m = mix565(RAMP[k], RAMP[k < 4 ? k + 1 : 4], (uint32_t)f);
    keep = 60u + (uint32_t)(warm * 3 > 150 ? 150 : warm * 3);   /* 60..210 of 256: brass and lamps stay */
    return mix565(m, c, keep);
}
/* a backdrop covers rows y0 .. y0+h (story art: only the window under the header); 8 bits a pixel, or 4 (two
 * pixels a byte, unpacked in place from the end) */
static void bg_load(int32_t id)
{
    const mo_bg_t *b;
    uint32_t i, n;
    if (id == bg_cur || id < 0 || id >= BG_COUNT) return;
    b = &MO_BG[id];
    n = SW * (uint32_t)b->h;
    if (b->bits == 4u) {
        if (lz4_decode(b->lz, b->lz_n, bg_idx, n / 2u) != (int)(n / 2u)) for (i = 0; i < n / 2u; i++) bg_idx[i] = 0;
        for (i = n / 2u; i-- > 0;) { uint8_t v = bg_idx[i]; bg_idx[2u * i] = v >> 4; bg_idx[2u * i + 1u] = v & 15u; }
    } else if (lz4_decode(b->lz, b->lz_n, bg_idx, n) != (int)n)
        for (i = 0; i < n; i++) bg_idx[i] = 0;  /* (a bad asset: a plain backdrop, never a crash) */
    for (i = 0; i < 64u; i++) bg_pal_sw[i] = swap16(i < b->ncol ? b->pal[i] : C_NIGHT);   /* (all the art is drawn in the new style now: no grade) */
    bg_cur = id;
}
static void g_bg(int32_t id)                    /* the frame starts as this backdrop (night above and below a strip) */
{
    const mo_bg_t *b;
    uint32_t i, n, o;
    uint16_t *d = cv_px;
    if (id < 0 || id >= BG_COUNT) { g_clear(C_NIGHT); return; }
    bg_load(id);
    b = &MO_BG[id];
    o = SW * (uint32_t)b->y0;
    n = SW * (uint32_t)b->h;
    if (b->y0 || b->h != SH) g_clear(C_NIGHT);
    for (i = 0; i < n; i++) d[o + i] = bg_pal_sw[bg_idx[i] & 63u];
}
static void g_dim(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t keep)   /* darken a region, keep 0..256 */
{
    int32_t i, j;
    for (j = y; j < y + h; j++) {
        if ((uint32_t)j < g_clip_y0 || (uint32_t)j >= g_clip_y1) continue;
        for (i = x; i < x + w; i++) if ((uint32_t)i < SW) px_put(i, j, mix565(C_NIGHT, px_get(i, j), keep));
    }
}

/* ---- the Deco alphabet: tall, narrow, Metropolis-like capitals as strokes on a 4 x 10 grid (in half units),
 * drawn with round-capped thick lines. 255 ends a stroke, 254 ends the letter. */
static const uint8_t DECO_A[] = {0,20, 4,0, 8,20, 255, 2,14, 6,14, 254};
static const uint8_t DECO_B[] = {0,0, 0,20, 6,20, 8,16, 8,12, 6,10, 0,10, 255, 0,0, 6,0, 7,3, 7,7, 6,10, 254};
static const uint8_t DECO_C[] = {8,2, 6,0, 2,0, 0,3, 0,17, 2,20, 6,20, 8,18, 254};
static const uint8_t DECO_D[] = {0,0, 0,20, 5,20, 8,16, 8,4, 5,0, 0,0, 254};
static const uint8_t DECO_E[] = {8,0, 0,0, 0,20, 8,20, 255, 0,8, 6,8, 254};
static const uint8_t DECO_F[] = {8,0, 0,0, 0,20, 255, 0,8, 6,8, 254};
static const uint8_t DECO_G[] = {8,2, 6,0, 2,0, 0,3, 0,17, 2,20, 8,20, 8,12, 4,12, 254};
static const uint8_t DECO_H[] = {0,0, 0,20, 255, 8,0, 8,20, 255, 0,8, 8,8, 254};
static const uint8_t DECO_I[] = {0,0, 0,20, 254};
static const uint8_t DECO_J[] = {8,0, 8,17, 6,20, 2,20, 0,17, 254};
static const uint8_t DECO_K[] = {0,0, 0,20, 255, 8,0, 0,12, 255, 3,9, 8,20, 254};
static const uint8_t DECO_L[] = {0,0, 0,20, 8,20, 254};
static const uint8_t DECO_M[] = {0,20, 0,0, 4,14, 8,0, 8,20, 254};
static const uint8_t DECO_N[] = {0,20, 0,0, 8,20, 8,0, 254};
static const uint8_t DECO_O[] = {2,0, 6,0, 8,3, 8,17, 6,20, 2,20, 0,17, 0,3, 2,0, 254};
static const uint8_t DECO_P[] = {0,20, 0,0, 6,0, 8,3, 8,8, 6,11, 0,11, 254};
static const uint8_t DECO_Q[] = {2,0, 6,0, 8,3, 8,17, 6,20, 2,20, 0,17, 0,3, 2,0, 255, 5,15, 9,22, 254};
static const uint8_t DECO_R[] = {0,20, 0,0, 6,0, 8,3, 8,8, 6,11, 0,11, 255, 4,11, 8,20, 254};
static const uint8_t DECO_S[] = {8,2, 6,0, 2,0, 0,3, 0,7, 8,13, 8,17, 6,20, 2,20, 0,18, 254};
static const uint8_t DECO_T[] = {0,0, 8,0, 255, 4,0, 4,20, 254};
static const uint8_t DECO_U[] = {0,0, 0,17, 2,20, 6,20, 8,17, 8,0, 254};
static const uint8_t DECO_V[] = {0,0, 4,20, 8,0, 254};
static const uint8_t DECO_W[] = {0,0, 2,20, 4,6, 6,20, 8,0, 254};
static const uint8_t DECO_X[] = {0,0, 8,20, 255, 8,0, 0,20, 254};
static const uint8_t DECO_Y[] = {0,0, 4,10, 8,0, 255, 4,10, 4,20, 254};
static const uint8_t DECO_Z[] = {0,0, 8,0, 0,20, 8,20, 255, 2,10, 6,10, 254};
static const uint8_t DECO_0[] = {2,0, 6,0, 8,3, 8,17, 6,20, 2,20, 0,17, 0,3, 2,0, 255, 8,3, 0,17, 254};
static const uint8_t DECO_1[] = {0,4, 3,0, 3,20, 254};
static const uint8_t DECO_2[] = {0,3, 2,0, 6,0, 8,3, 8,7, 0,20, 8,20, 254};
static const uint8_t DECO_3[] = {0,1, 8,0, 3,9, 6,9, 8,12, 8,17, 6,20, 0,19, 254};
static const uint8_t DECO_4[] = {6,20, 6,0, 0,13, 8,13, 254};
static const uint8_t DECO_5[] = {8,0, 0,0, 0,9, 6,9, 8,12, 8,17, 6,20, 0,20, 254};
static const uint8_t DECO_6[] = {7,0, 2,0, 0,3, 0,17, 2,20, 6,20, 8,17, 8,12, 6,10, 0,10, 254};
static const uint8_t DECO_7[] = {0,0, 8,0, 3,20, 254};
static const uint8_t DECO_8[] = {2,0, 6,0, 8,3, 8,7, 0,13, 0,17, 2,20, 6,20, 8,17, 8,13, 0,7, 0,3, 2,0, 254};
static const uint8_t DECO_9[] = {8,10, 2,10, 0,8, 0,3, 2,0, 6,0, 8,3, 8,17, 6,20, 1,20, 254};
static const uint8_t DECO_AP[] = {1,0, 0,5, 254};
static const uint8_t DECO_DOT[] = {0,19, 0,20, 254};
static const uint8_t *const DECO_GL[26] = {DECO_A, DECO_B, DECO_C, DECO_D, DECO_E, DECO_F, DECO_G, DECO_H, DECO_I, DECO_J,
    DECO_K, DECO_L, DECO_M, DECO_N, DECO_O, DECO_P, DECO_Q, DECO_R, DECO_S, DECO_T, DECO_U, DECO_V, DECO_W, DECO_X,
    DECO_Y, DECO_Z};
static const uint8_t *const DECO_DIG[10] = {DECO_0, DECO_1, DECO_2, DECO_3, DECO_4, DECO_5, DECO_6, DECO_7, DECO_8, DECO_9};
#define CH_APOS 39
static const uint8_t *deco_glyph(char c)
{
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (c >= 'A' && c <= 'Z') return DECO_GL[c - 'A'];
    if (c >= '0' && c <= '9') return DECO_DIG[c - '0'];
    if (c == CH_APOS) return DECO_AP;
    if (c == '.') return DECO_DOT;
    return 0;
}
static int deco_narrow(char c) { return c == 'I' || c == 'i' || c == CH_APOS || c == '.'; }
static int32_t deco_st16(int32_t h) { return h * 16 / 9 + 6; }   /* stroke width, 1/16 px */
static int32_t deco_adv16(char c, int32_t h)   /* advance of a letter, 1/16 px, cap height h px */
{
    int32_t cw = h * 16 * 4 / 10, gap = h * 16 * 24 / 100;
    if (c == ' ') return cw;
    if (deco_narrow(c)) return deco_st16(h) + gap;
    return cw + gap;
}
static int32_t deco_w(const char *s, int32_t h)  /* px */
{
    int32_t w = 0;
    for (; *s; s++) w += deco_adv16(*s, h);
    return (w - h * 16 * 24 / 100) / 16;
}
static int32_t g_deco(int32_t x, int32_t y, int32_t h, const char *s, uint16_t c)   /* x, y: top-left, px */
{
    int32_t x16 = x * 16, st = deco_st16(h);
    for (; *s; s++) {
        const uint8_t *g = deco_glyph(*s);
        int32_t ox = x16 + (deco_narrow(*s) ? st / 2 : 0), px0 = -1, py0 = 0;
        while (g && *g != 254u) {
            int32_t px, py;
            if (*g == 255u) { px0 = -1; g++; continue; }
            px = ox + (int32_t)g[0] * h * 16 / 20;   /* half units: x 8 = 0.4 h, y 20 = h */
            py = y * 16 + (int32_t)g[1] * h * 16 / 20;
            if (px0 >= 0) g_thick16(px0, py0, px, py, st, c);
            else if (g[2] == 254u || g[2] == 255u) g_disc16(px, py, st / 2, c);   /* a lone point: a dot */
            px0 = px; py0 = py;
            g += 2;
        }
        x16 += deco_adv16(*s, h);
    }
    return x16 / 16;
}
static void g_deco_c(int32_t cx, int32_t y, int32_t h, const char *s, uint16_t c) { g_deco(cx - deco_w(s, h) / 2, y, h, s, c); }

/* ---- a pedal-style panel: black lacquer, an engraved neon double rule, Deco fans in the corners */
static void g_panel(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t line, int fans)
{
    (void)fans;
    g_cab_panel(x, y, w, h, line == C_GOLD ? C_ROSE : line);   /* (Obscura Line: the cab's rounded neon panel) */
}
/* the Metropolis sunburst, live (for plaques and finales): alternating wedges from a centre */
static void g_rays(int32_t cx, int32_t cy, int32_t r, uint32_t n, uint32_t a0, uint16_t c)
{
    uint32_t i, step = 65536u / n;
    for (i = 0; i < n; i += 2) {
        uint32_t a = a0 + i * step;
        g_tri16(cx * 16, cy * 16, px16(cx, r * 16, a), py16(cy, r * 16, a), px16(cx, r * 16, a + step * 45u / 100u), py16(cy, r * 16, a + step * 45u / 100u), c);
    }
}
/* a pedal knob as an indicator: a dotted scale, a black knob, a cream pointer. v 0..256 = 7 to 5 o'clock */
static void g_knob(int32_t cx, int32_t cy, int32_t r, int32_t v, uint16_t line)
{
    int32_t k;
    uint32_t a;
    for (k = 0; k <= 10; k++) {
        a = 24576u + (uint32_t)k * 4915u;       /* 135 .. 405 degrees */
        g_disc16(px16(cx, (r + 3) * 16, a) + 8, py16(cy, (r + 3) * 16, a) + 8, 10, line);
    }
    g_disc(cx, cy, r, RGB(70, 70, 80));
    g_disc(cx, cy, r - 1, RGB(22, 22, 26));
    g_disc(cx, cy, r * 3 / 4, RGB(36, 36, 42));
    a = 24576u + (uint32_t)(clampi(v, 0, 256) * 49152 / 256);
    g_thick16(px16(cx, r * 3, a) + 8, py16(cy, r * 3, a) + 8, px16(cx, r * 14, a) + 8, py16(cy, r * 14, a) + 8, 22, C_CREAM);
}
/* a lit dot with a halo (neon dots, LEDs) */
static void g_cab_wall(int32_t gx, int32_t gy, int32_t gr)   /* glow at (gx, gy), radius gr (0: none) */
{
    int32_t y, x;
    for (y = 22; y < 222; y++)
        g_rect(0, y, SW, 1, mix565(RGB(28, 14, 54), RGB(14, 8, 30), (uint32_t)(y - 22) * 256u / (uint32_t)(222 - 22)));
    for (x = 0; x < SW; x += 30) g_rect(x, 22, 1, 222 - 22, RGB(36, 20, 64));
    for (y = 22 + 8; y < 222; y += 30) {
        g_rect(0, y, SW, 1, RGB(36, 20, 64));
        for (x = 4; x < SW; x += 30) g_rect(x, y + 3, 2, 2, RGB(64, 42, 96));
    }
    if (gr) {
        g_disc(gx, gy, gr, RGB(40, 14, 66));
        g_disc(gx, gy, gr * 3 / 4, RGB(52, 16, 78));
    }
}
static void g_led(int32_t x, int32_t y, int32_t r, uint16_t c)
{
    g_disc16(x * 16 + 8, y * 16 + 8, r * 16 + 40, mix565(px_get(clampi(x, 0, 239), clampi(y, 0, 239)), c, 70));
    g_disc16(x * 16 + 8, y * 16 + 8, r * 16, c);
}

/* the line's mark: a wheel of three spokes in a ring, a dot at its heart. Pacific's gear train carries it; it comes
 * back on Olympia's tunnel door (the observant will know the door belongs to the same machine) */
static void g_sigil(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t k;
    g_ring(cx, cy, r - 1, r, c);
    for (k = 0; k < 3; k++) {
        uint32_t a = (uint32_t)k * 21845u + 16384u;
        g_line(cx, cy, px16(cx, (r - 2) * 16, a) >> 4, py16(cy, (r - 2) * 16, a) >> 4, c);
        g_disc(px16(cx, (r - 2) * 16, a) >> 4, py16(cy, (r - 2) * 16, a) >> 4, 1, c);
    }
    g_disc(cx, cy, 2, c);
}
