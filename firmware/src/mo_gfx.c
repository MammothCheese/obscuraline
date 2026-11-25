/* SPDX-License-Identifier: GPL-3.0-only */
/* Mechanica Obscura: the drawing layer. One full-screen frame (240 x 240, cv_px from gfx.c, CV_MAX set in
 * obscura.c) is drawn each frame and sent in two strips. Pixels are kept byte-swapped as the panel wants them.
 * Integer only: angles are 16-bit (65536 = one turn), sub-pixel coordinates are 1/16 px where it matters
 * (gears turning slowly must not jitter), circles are anti-aliased at their rim. */
#include "mo_tables.h"

/* ---- palette: Obscura Line's "neon clockwork" (synthwave over Mechanica's clockpunk). Indigo night and violet
 * lacquer, brass and gold for the machines, blued steel gone violet, neon cyan and hot magenta for what is alive
 * (the UI, the lights), amber lamps, pale rose dial faces */
#define C_NIGHT   RGB(10, 6, 26)
#define C_LACQ    RGB(14, 8, 30)                /* panels */
#define C_CREAM   RGB(248, 228, 240)            /* narration, dial faces */
#define C_PAPER   C_CREAM
#define C_PAPER2  RGB(46, 32, 72)               /* faint rules on dark */
#define C_PAPER3  RGB(88, 66, 116)              /* separators, dim marks */
#define C_INK     RGB(20, 10, 30)               /* marks on cream faces */
#define C_INK2    RGB(176, 150, 196)            /* secondary text */
#define C_BRASS   RGB(214, 164, 80)
#define C_BRASS_L RGB(255, 226, 150)
#define C_BRASS_D RGB(150, 100, 38)
#define C_BRASS_DD RGB(80, 50, 18)
#define C_GOLD    RGB(236, 186, 92)
#define C_STEEL   RGB(104, 96, 210)             /* violet steel: hairsprings, screws, hands */
#define C_STEEL_L RGB(170, 160, 240)
#define C_STEEL_D RGB(50, 40, 100)
#define C_RUBY    RGB(210, 44, 52)
#define C_GLOW    RGB(255, 226, 150)
#define C_COPPER  RGB(186, 104, 64)
#define C_VERD    RGB(90, 210, 160)             /* success */
#define C_CYAN    RGB(60, 232, 255)             /* neon: the machines, the UI */
#define C_CYAN_D  RGB(20, 92, 140)
#define C_ROSE    RGB(255, 60, 200)             /* neon magenta: the second control, warnings */
#define C_ROSE_D  RGB(112, 20, 96)
#define C_AMBER   RGB(255, 168, 60)             /* lamps, the third control */

#define SW 240
#define SH 240
static uint32_t g_clip_y0 = 0, g_clip_y1 = SH;   /* rows drawn this frame (the whole screen by default) */

static inline uint16_t px_get(int32_t x, int32_t y) { return swap16(cv_px[(uint32_t)y * SW + (uint32_t)x]); }
static inline void px_put(int32_t x, int32_t y, uint16_t c) { cv_px[(uint32_t)y * SW + (uint32_t)x] = swap16(c); }

static uint16_t mix565(uint16_t a, uint16_t b, uint32_t t)   /* t 0..256: a .. b */
{
    uint32_t ra = a >> 11, ga = (a >> 5) & 63u, ba = a & 31u;
    uint32_t rb = b >> 11, gb = (b >> 5) & 63u, bb = b & 31u;
    uint32_t r = (ra * (256u - t) + rb * t) >> 8, g = (ga * (256u - t) + gb * t) >> 8, bl = (ba * (256u - t) + bb * t) >> 8;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static inline void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)   /* a 0..256 */
{
    if ((uint32_t)x >= SW || (uint32_t)y < g_clip_y0 || (uint32_t)y >= g_clip_y1) return;
    if (a >= 256u) px_put(x, y, c);
    else if (a) px_put(x, y, mix565(px_get(x, y), c, a));
}
static inline void px_set(int32_t x, int32_t y, uint16_t c)
{
    if ((uint32_t)x < SW && (uint32_t)y >= g_clip_y0 && (uint32_t)y < g_clip_y1) px_put(x, y, c);
}

/* ---- maths */
static int32_t mo_sin(uint32_t a)               /* a: 16-bit angle -> Q15, linear between table entries */
{
    uint32_t i = (a >> 6) & 1023u, f = a & 63u;
    int32_t s0 = MO_SIN[i], s1 = MO_SIN[(i + 1u) & 1023u];
    return s0 + (((s1 - s0) * (int32_t)f) >> 6);
}
static int32_t mo_cos(uint32_t a) { return mo_sin(a + 16384u); }
static uint32_t mo_atan2(int32_t y, int32_t x) /* -> 16-bit angle, 0 = +x, 16384 = +y (down on screen) */
{
    uint32_t ax = (uint32_t)(x < 0 ? -x : x), ay = (uint32_t)(y < 0 ? -y : y), a;
    if (!ax && !ay) return 0;
    if (ax >= ay) a = MO_ATAN[(ay << 8) / ax];
    else a = 16384u - MO_ATAN[(ax << 8) / ay];
    if (x < 0) a = 32768u - a;
    if (y < 0) a = 65536u - a;
    return a & 0xFFFFu;
}
static uint32_t mo_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return r;
}
static uint32_t rng_s = 0x9E3779B9u;            /* xorshift: puzzles are varied per playthrough */
static uint32_t rnd(void) { rng_s ^= rng_s << 13; rng_s ^= rng_s >> 17; rng_s ^= rng_s << 5; return rng_s; }
static uint32_t rndn(uint32_t n) { return n ? rnd() % n : 0; }
static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }
static int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
/* polar point in 1/16 px: centre (cx, cy) in px, radius r in 1/16 px */
static int32_t px16(int32_t cx, int32_t r16, uint32_t a) { return cx * 16 + ((r16 * mo_cos(a)) >> 15); }
static int32_t py16(int32_t cy, int32_t r16, uint32_t a) { return cy * 16 + ((r16 * mo_sin(a)) >> 15); }

/* ---- primitives */
static void g_clear(uint16_t c)
{
    uint32_t i, n = (g_clip_y1 - g_clip_y0) * SW;
    uint16_t s = swap16(c), *p = cv_px + g_clip_y0 * SW;
    for (i = 0; i < n; i++) p[i] = s;
}
static void g_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t x1 = x + w, y1 = y + h, i;
    uint16_t s = swap16(c);
    if (x < 0) x = 0;
    if (y < (int32_t)g_clip_y0) y = (int32_t)g_clip_y0;
    if (x1 > SW) x1 = SW;
    if (y1 > (int32_t)g_clip_y1) y1 = (int32_t)g_clip_y1;
    for (; y < y1; y++)
        for (i = x; i < x1; i++) cv_px[(uint32_t)y * SW + (uint32_t)i] = s;
}
static void g_rect_a(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, uint32_t a)   /* translucent */
{
    int32_t i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++) px_blend(i, j, c, a);
}
static void g_frame(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* 1 px outline */
{
    g_rect(x, y, w, 1, c); g_rect(x, y + h - 1, w, 1, c);
    g_rect(x, y, 1, h, c); g_rect(x + w - 1, y, 1, h, c);
}
/* an engraved brass plate: bevel light top-left, dark bottom-right */
/* rounded rectangles, the frame's shape: r is the corner radius (px) */
static int32_t g_rr_inset(int32_t j, int32_t h, int32_t r)   /* row j of h: the corner's inset */
{
    int32_t d = j < r ? r - j : j >= h - r ? j - (h - r - 1) : 0;
    return d ? r - (int32_t)mo_isqrt((uint32_t)(r * r - d * d)) : 0;
}
static void g_rrect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    int32_t j;
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    for (j = 0; j < h; j++) { int32_t k = g_rr_inset(j, h, r); g_rect(x + k, y + j, w - 2 * k, 1, c); }
}
static void g_rframe(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, int32_t t, uint16_t c)   /* t px thick */
{
    int32_t j, i;
    if (r * 2 > h) r = h / 2;
    for (j = 0; j < h; j++) {
        int32_t k = g_rr_inset(j, h, r), ki = j >= t && j < h - t ? g_rr_inset(j - t, h - 2 * t, r > t ? r - t : 0) + t : -1;
        if (ki < 0) { g_rect(x + k, y + j, w - 2 * k, 1, c); continue; }
        for (i = 0; i < 2; i++) {
            int32_t a = i ? x + w - ki : x + k, b = i ? x + w - k : x + ki;
            if (b > a) g_rect(a, y + j, b - a, 1, c);
        }
    }
}
/* a panel of the cab: deep violet, a magenta rim with a faint cyan line inside it */
#define C_PANEL   RGB(18, 10, 44)
#define C_PANEL2  RGB(28, 16, 64)
/* the UI's colours: the synthwave night, or (the Enoden) a plain rail cab: grey panels, white, green and orange lamps */
static struct { uint16_t night, rose, rose_d, cyan, cyan_d, panel, panel2, ink2, cream, frame; } ut =
    {C_NIGHT, C_ROSE, C_ROSE_D, C_CYAN, C_CYAN_D, C_PANEL, C_PANEL2, C_INK2, C_CREAM, RGB(70, 44, 100)};
static uint8_t ut_rail;
static void ui_theme(int rail)
{
    if (rail == ut_rail) return;
    ut_rail = (uint8_t)rail;
    if (rail == 2) {                            /* the heart: red dark */
        ut.night = RGB(14, 0, 4); ut.rose = RGB(255, 50, 70); ut.rose_d = RGB(110, 10, 24); ut.cyan = RGB(255, 140, 140);
        ut.cyan_d = RGB(120, 30, 40); ut.panel = RGB(30, 4, 10); ut.panel2 = RGB(50, 8, 16); ut.ink2 = RGB(200, 130, 140);
        ut.cream = RGB(255, 220, 220); ut.frame = RGB(90, 20, 30);
    } else if (rail) {
        ut.night = RGB(24, 26, 30); ut.rose = RGB(255, 140, 30); ut.rose_d = RGB(110, 60, 20); ut.cyan = RGB(110, 220, 110);
        ut.cyan_d = RGB(40, 90, 50); ut.panel = RGB(40, 42, 46); ut.panel2 = RGB(62, 64, 70); ut.ink2 = RGB(175, 178, 182);
        ut.cream = RGB(240, 240, 236); ut.frame = RGB(80, 82, 86);
    } else {
        ut.night = C_NIGHT; ut.rose = C_ROSE; ut.rose_d = C_ROSE_D; ut.cyan = C_CYAN; ut.cyan_d = C_CYAN_D;
        ut.panel = C_PANEL; ut.panel2 = C_PANEL2; ut.ink2 = C_INK2; ut.cream = C_CREAM; ut.frame = RGB(70, 44, 100);
    }
}
static void g_cab_panel(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t rim)
{
    g_rrect(x, y, w, h, 7, ut.panel);
    g_rframe(x, y, w, h, 7, 2, rim);
    g_rframe(x + 3, y + 3, w - 6, h - 6, 5, 1, mix565(ut.panel, ut.cyan, 70));
}
static void g_plate(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t face, uint16_t hi, uint16_t lo)
{
    g_rect(x, y, w, h, face);
    g_rect(x, y, w, 1, hi); g_rect(x, y, 1, h, hi);
    g_rect(x, y + h - 1, w, 1, lo); g_rect(x + w - 1, y, 1, h, lo);
}

/* a horizontal span [xl, xr) in 1/16 px on row y: the end pixels by coverage */
static void g_span16(int32_t y, int32_t xl, int32_t xr, uint16_t c)
{
    int32_t a, b, i;
    uint16_t s;
    if ((uint32_t)y < g_clip_y0 || (uint32_t)y >= g_clip_y1 || xr <= xl) return;
    a = xl >> 4; b = xr >> 4;
    if (a == b) { px_blend(a, y, c, (uint32_t)(xr - xl) << 4); return; }
    px_blend(a, y, c, (uint32_t)(16 - (xl & 15)) << 4);
    px_blend(b, y, c, (uint32_t)(xr & 15) << 4);
    s = swap16(c);
    if (a + 1 < 0) a = -1;
    if (b > SW) b = SW;
    for (i = a + 1; i < b; i++) cv_px[(uint32_t)y * SW + (uint32_t)i] = s;
}
/* disc: centre in 1/16 px, radius in 1/16 px */
static void g_disc16(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t y, y0 = (cy - r) >> 4, y1 = (cy + r) >> 4;
    for (y = y0; y <= y1; y++) {
        int32_t dy = y * 16 + 8 - cy, w;
        uint32_t q;
        if (iabs(dy) >= r) continue;
        q = (uint32_t)(r * r - dy * dy);
        w = (int32_t)mo_isqrt(q);
        g_span16(y, cx - w, cx + w, c);
    }
}
static void g_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c) { g_disc16(cx * 16 + 8, cy * 16 + 8, r * 16, c); }
/* ring between r0 and r1 (1/16 px) */
static void g_ring16(int32_t cx, int32_t cy, int32_t r0, int32_t r1, uint16_t c)
{
    int32_t y, y0 = (cy - r1) >> 4, y1 = (cy + r1) >> 4;
    for (y = y0; y <= y1; y++) {
        int32_t dy = y * 16 + 8 - cy, wo, wi;
        if (iabs(dy) >= r1) continue;
        wo = (int32_t)mo_isqrt((uint32_t)(r1 * r1 - dy * dy));
        if (iabs(dy) >= r0) { g_span16(y, cx - wo, cx + wo, c); continue; }
        wi = (int32_t)mo_isqrt((uint32_t)(r0 * r0 - dy * dy));
        g_span16(y, cx - wo, cx - wi, c);
        g_span16(y, cx + wi, cx + wo, c);
    }
}
static void g_ring(int32_t cx, int32_t cy, int32_t r0, int32_t r1, uint16_t c) { g_ring16(cx * 16 + 8, cy * 16 + 8, r0 * 16, r1 * 16, c); }

/* filled triangle, vertices in 1/16 px (sampled at pixel centres; no AA: edges between teeth are short) */
static void g_tri16(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t c)
{
    int32_t minx, maxx, miny, maxy, x, y, area;
    uint16_t s = swap16(c);
    area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (!area) return;
    if (area < 0) { int32_t t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    minx >>= 4; maxx >>= 4; miny >>= 4; maxy >>= 4;
    if (minx < 0) minx = 0;
    if (maxx > SW - 1) maxx = SW - 1;
    if (miny < (int32_t)g_clip_y0) miny = (int32_t)g_clip_y0;
    if (maxy > (int32_t)g_clip_y1 - 1) maxy = (int32_t)g_clip_y1 - 1;
    for (y = miny; y <= maxy; y++) {
        int32_t py = y * 16 + 8, px0 = minx * 16 + 8;
        /* edge functions at the row start; step +16 in x each pixel */
        int32_t e0 = (x1 - x0) * (py - y0) - (y1 - y0) * (px0 - x0);
        int32_t e1 = (x2 - x1) * (py - y1) - (y2 - y1) * (px0 - x1);
        int32_t e2 = (x0 - x2) * (py - y2) - (y0 - y2) * (px0 - x2);
        int32_t d0 = -(y1 - y0) * 16, d1 = -(y2 - y1) * 16, d2 = -(y0 - y2) * 16;
        for (x = minx; x <= maxx; x++) {
            if ((e0 | e1 | e2) >= 0) cv_px[(uint32_t)y * SW + (uint32_t)x] = s;
            e0 += d0; e1 += d1; e2 += d2;
        }
    }
}
static void g_quad16(const int32_t *p, uint16_t c)   /* p: x0 y0 x1 y1 x2 y2 x3 y3 (convex, in order) */
{
    g_tri16(p[0], p[1], p[2], p[3], p[4], p[5], c);
    g_tri16(p[0], p[1], p[4], p[5], p[6], p[7], c);
}
/* a thick line (a rotated rectangle) with round caps, endpoints in 1/16 px, width in 1/16 px */
static void g_thick16(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t w, uint16_t c)
{
    uint32_t a = mo_atan2(y1 - y0, x1 - x0) + 16384u;
    int32_t hx = (w / 2 * mo_cos(a)) >> 15, hy = (w / 2 * mo_sin(a)) >> 15;
    int32_t q[8] = {x0 + hx, y0 + hy, x1 + hx, y1 + hy, x1 - hx, y1 - hy, x0 - hx, y0 - hy};
    g_quad16(q, c);
    g_disc16(x0, y0, w / 2, c);
    g_disc16(x1, y1, w / 2, c);
}
static void g_thick(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t w, uint16_t c)
{
    g_thick16(x0 * 16 + 8, y0 * 16 + 8, x1 * 16 + 8, y1 * 16 + 8, w * 16, c);
}
static void g_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* 1 px */
{
    int32_t dx = iabs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -iabs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy, n = 1000;
    while (n--) {
        int32_t e2 = 2 * err;
        px_set(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
/* a radial hand: from centre to radius r, angle a, width w (all px except angle); with a counterweight tail */
static void g_hand(int32_t cx, int32_t cy, int32_t r, int32_t tail, uint32_t a, int32_t w, uint16_t c)
{
    int32_t x0 = px16(cx, -tail * 16, a), y0 = py16(cy, -tail * 16, a);
    int32_t x1 = px16(cx, r * 16, a), y1 = py16(cy, r * 16, a);
    g_thick16(x0, y0, x1, y1, w * 16, c);
}

/* ---- a gear: root circle, trapezoid teeth, spoked or solid body, an arbor. angle a = rotation (16-bit) */
static void g_gear(int32_t cx, int32_t cy, int32_t r, uint32_t teeth, uint32_t a, uint16_t face, uint16_t dark,
                   uint16_t hi, int spokes)
{
    int32_t rt = r * 16 + 40, rr = r * 16 - 28, k;   /* tip and root radius, 1/16 px: module ~ fixed tooth height */
    uint32_t pitch = teeth ? 65536u / teeth : 65536u;
    if (!teeth) return;
    g_disc16(cx * 16 + 8, cy * 16 + 8, rr, dark);
    for (k = 0; k < (int32_t)teeth; k++) {           /* teeth: wide at the root, narrow at the tip */
        uint32_t ta = a + (uint32_t)k * pitch, wr = pitch * 30u / 100u, wt = pitch * 17u / 100u;
        int32_t q[8] = {px16(cx, rr - 8, ta - wr), py16(cy, rr - 8, ta - wr), px16(cx, rt, ta - wt), py16(cy, rt, ta - wt),
                        px16(cx, rt, ta + wt), py16(cy, rt, ta + wt), px16(cx, rr - 8, ta + wr), py16(cy, rr - 8, ta + wr)};
        g_quad16(q, dark);
        q[2] += 0; /* inner face slightly smaller: a lit flank */
        {
            int32_t q2[8] = {px16(cx, rr - 20, ta - wr + 260), py16(cy, rr - 20, ta - wr + 260), px16(cx, rt - 14, ta - wt + 200),
                             py16(cy, rt - 14, ta - wt + 200), px16(cx, rt - 14, ta + wt - 200), py16(cy, rt - 14, ta + wt - 200),
                             px16(cx, rr - 20, ta + wr - 260), py16(cy, rr - 20, ta + wr - 260)};
            if (r >= 9) g_quad16(q2, face);
        }
    }
    g_disc16(cx * 16 + 8, cy * 16 + 8, rr - 14, face);
    if (spokes && r >= 16) {                          /* the crossing: a rim, cut-outs between spokes */
        int32_t ri = rr - 14 - r * 4, rh = r * 16 / 4;
        uint32_t s;
        g_ring16(cx * 16 + 8, cy * 16 + 8, ri - 12, ri, hi);
        g_disc16(cx * 16 + 8, cy * 16 + 8, ri - 12, C_PAPER3);
        for (s = 0; s < (uint32_t)spokes; s++) {
            uint32_t sa = a + s * (65536u / (uint32_t)spokes);
            g_thick16(cx * 16 + 8, cy * 16 + 8, px16(cx, ri - 8, sa) + 8, py16(cy, ri - 8, sa) + 8, r * 16 / 5 + 24, face);
        }
        g_disc16(cx * 16 + 8, cy * 16 + 8, rh, face);
    } else {
        g_ring16(cx * 16 + 8, cy * 16 + 8, rr * 2 / 3 - 10, rr * 2 / 3, hi);   /* a turned circle on the face */
    }
    g_disc(cx, cy, r > 14 ? 4 : 2, dark);             /* arbor */
    g_disc(cx, cy, r > 14 ? 2 : 1, C_STEEL_L);
}
static void g_gear_brass(int32_t cx, int32_t cy, int32_t r, uint32_t teeth, uint32_t a, int spokes)
{
    g_gear(cx, cy, r, teeth, a, C_BRASS, C_BRASS_D, C_BRASS_L, spokes);
}
/* a screw head: blued steel, slot at angle a */
static void g_screw(int32_t cx, int32_t cy, int32_t r, uint32_t a)
{
    g_disc(cx, cy, r, C_STEEL_D);
    g_disc(cx, cy, r - 1, C_STEEL);
    g_thick16(px16(cx, -(r - 1) * 16, a) + 8, py16(cy, -(r - 1) * 16, a) + 8, px16(cx, (r - 1) * 16, a) + 8, py16(cy, (r - 1) * 16, a) + 8, 20, C_STEEL_D);
}
static void g_jewel(int32_t cx, int32_t cy)
{
    g_disc(cx, cy, 3, C_BRASS_D);
    g_disc(cx, cy, 2, C_RUBY);
    px_set(cx - 1, cy - 1, RGB(255, 170, 170));
}

/* ---- text, blended onto whatever is under it (gfx.c's cv_text blends onto black only) */
static int32_t g_text(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
#ifdef DINGHY_HOST
    if (getenv("OBL_TEXTCHECK") && *s && y > -8 && y < SH && (x < 0 || x + text_w(f, s) > SW)) {   /* (host: words off the screen's edge) */
        static char seen[64][48]; static uint32_t ns; uint32_t k;
        for (k = 0; k < ns && strncmp(seen[k], s, 47); k++) {}
        if (k == ns && ns < 64u) { strncpy(seen[ns++], s, 47); fprintf(stderr, "TEXT OFF SCREEN x %d..%d y %d: %s\n", (int)x, (int)(x + text_w(f, s)), (int)y, s); }
    }
#endif
    for (; *s; s++) {
        uint32_t gi = glyph(f, (uint8_t)*s), gx, gy, w = f->bw[gi], bpr = (w + 1u) / 2u;
        const uint8_t *gd = f->data + f->off[gi];
        if (*s == '\n') continue;
        for (gy = 0; gy < f->h; gy++)
            for (gx = 0; gx < w; gx++) {
                uint32_t v = gd[gy * bpr + gx / 2u];
                v = (gx & 1u) ? (v & 15u) : (v >> 4);
                if (v) px_blend(x - f->pad + (int32_t)gx, y + (int32_t)gy, c, v * 17u + (v == 15u));
            }
        x += f->adv[gi];
    }
    return x;
}
static void g_text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    g_text(cx - text_w(f, s) / 2, y, f, s, c);
}
static void g_text_r(int32_t rx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    g_text(rx - text_w(f, s), y, f, s, c);
}
/* engraved: a light line under dark text, as letters cut into brass */
static void g_text_engr(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c, uint16_t light)
{
    g_text_c(cx, y + 1, f, s, light);
    g_text_c(cx, y, f, s, c);
}
/* wrapped text in a column of w px, n characters at most shown (the typewriter), line height lh; returns lines */
static int32_t g_text_wrap(int32_t x, int32_t y, int32_t w, int32_t lh, const felucca_font_t *f, const char *s,
                           int32_t nshow, uint16_t c)
{
    char line[64];
    int32_t lines = 0, shown = 0, cols = w / f->adv[0];
    if (cols > 63) cols = 63;
    while (*s) {
        int32_t n = 0, brk = -1, i;
        while (s[n] && s[n] != '\n' && n < cols) { if (s[n] == ' ') brk = n; n++; }
        if (s[n] && s[n] != '\n' && n == cols && brk > 0) n = brk;
        for (i = 0; i < n; i++) line[i] = s[i];
        line[n] = 0;
        if (nshow >= 0) {
            if (shown >= nshow) break;
            if (shown + n > nshow) line[nshow - shown] = 0;
        }
        g_text(x, y + lines * lh, f, line, c);
        shown += n;
        lines++;
        s += n;
        while (*s == ' ') s++, shown++;
        if (*s == '\n') s++, shown++;
    }
    return lines;
}
static void fmt_u(char *p, uint32_t v)         /* unsigned to decimal */
{
    char t[12];
    int32_t n = 0;
    do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < 11);
    while (n) *p++ = t[--n];
    *p = 0;
}
static char *str_cat(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }

/* ---- the paper: a faint engraved guilloché ring pattern and a vignette, drawn once per scene into the back
 * of the frame (cheap: rings only) */
static void g_paper(void)
{
    g_clear(C_NIGHT);
}
static void g_guilloche(int32_t cx, int32_t cy, int32_t r0, int32_t r1, uint16_t c)
{
    int32_t r;
    for (r = r0; r < r1; r += 6) g_ring16(cx * 16 + 8, cy * 16 + 8, r * 16, r * 16 + 10, c);
}

/* ---- the frame goes out: two strips of 120 rows (one DMA of 57.6 KB each) */
static void g_begin(void)
{
    lcd_sync();                                 /* the last frame's DMA may still read the buffer */
    g_clip_y0 = 0;
    g_clip_y1 = SH;
}
static void g_present(void)
{
    lcd_blit(0, 0, SW, 120, cv_px);
    lcd_blit(0, 120, SW, 120, cv_px + 120u * SW);
}
