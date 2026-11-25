/* SPDX-License-Identifier: GPL-3.0-only */
/* Chapter IX: THE CELESTIAL CLOCK. Set the old tower clock to the sign on the parchment.
 *
 * The parchment shows a MOON (its phase) and a CONSTELLATION, and the hour is rung on the tower bells: press
 * REC and count the strokes (or, with bells = 0, it is written). Set the clock to match:
 *   K1 the sun hand round the twelve hours, K2 the moon in its window through its eight phases, K3 the star ring
 *   until the constellation stands in the window at the top.
 * Each part glows when it matches, and keys C, E, G light for hour, moon and stars. All three together and the
 * figures come out. Drawn fresh each time. */

#define AS_H 12
#define AS_M 8
#define AS_S 6
static struct {
    int32_t h, m, s, th, tm, ts;                /* the clock now; the sign asked for */
    int32_t sh, sm, ss;                         /* drawn positions, Q8 */
    uint32_t bell_t, fin_t;
    uint8_t bells;
} as;

/* six constellations: up to five stars each, drawn small (x, y in -6..6), joined in order */
static const int8_t AS_CON[AS_S][10] = {
    {-5, 4, -2, -1, 1, 2, 4, -3, 99, 0},        /* the Key */
    {-5, -3, -2, 3, 2, -3, 5, 3, 99, 0},        /* the Crown */
    {-4, 0, 0, -5, 4, 0, 0, 5, -4, 0},          /* the Lens */
    {-5, 5, -5, -3, 0, -3, 5, -3, 5, 5},        /* the Gate */
    {-5, 0, -1, -3, 3, 0, -1, 3, 99, 0},        /* the Bird */
    {0, -5, 0, 5, -4, 1, 4, 1, 99, 0},          /* the Mast */
};
static void as_con(int32_t x, int32_t y, int32_t k, int32_t sc, uint16_t c)
{
    int32_t j, px = 0, py = 0;
    for (j = 0; j < 5; j++) {
        int32_t sx = AS_CON[k][j * 2], sy = AS_CON[k][j * 2 + 1];
        if (sx == 99) break;
        sx = x + sx * sc / 4; sy = y + sy * sc / 4;
        if (j) g_line(px, py, sx, sy, mix565(C_NIGHT, c, 140));
        g_disc(sx, sy, 1 + sc / 6, c);
        px = sx; py = sy;
    }
}
/* the moon in phase k of 8: 0 new, 4 full; waxing lit on the right */
static void as_moon(int32_t x, int32_t y, int32_t r, int32_t k, uint16_t bg)
{
    int32_t off;
    g_disc(x, y, r + 1, C_STEEL_D);
    if (k == 0) { g_disc(x, y, r, RGB(40, 44, 60)); return; }
    g_disc(x, y, r, C_CREAM);
    if (k == 4) return;
    off = k < 4 ? -(2 * r * (4 - k)) / 4 - r / 2 : (2 * r * (k - 4)) / 4 + r / 2;
    g_clip_y0 = (uint32_t)clampi(y - r - 1, 0, SH); g_clip_y1 = (uint32_t)clampi(y + r + 2, 0, SH);
    {
        int32_t yy, xx;                         /* the shadow: a dark disc shifted, kept inside the moon */
        for (yy = -r; yy <= r; yy++)
            for (xx = -r; xx <= r; xx++)
                if (xx * xx + yy * yy <= r * r && (xx - off) * (xx - off) + yy * yy <= r * r) px_set(x + xx, y + yy, RGB(40, 44, 60));
    }
    g_clip_y0 = 0; g_clip_y1 = SH;
    (void)bg;
}
static void as_enter(void)
{
    as.bells = (uint8_t)PRM(P_ASTRO_BELLS);
    as.th = (int32_t)rndn(AS_H); as.tm = (int32_t)rndn(AS_M); as.ts = (int32_t)rndn(AS_S);
    do { as.h = (int32_t)rndn(AS_H); as.m = (int32_t)rndn(AS_M); as.s = (int32_t)rndn(AS_S); } while (as.h == as.th || as.m == as.tm || as.s == as.ts);
    as.sh = as.h * 256; as.sm = as.m * 256; as.ss = as.s * 256;
    as.fin_t = 0; as.bell_t = 0;
}
static void as_ring_bells(void)
{
    uint32_t k, t = au_time() + ms_frames(150);
    au_cut(9);
    for (k = 0; k <= (uint32_t)as.th; k++) {    /* hour 1..12: that many strokes */
        sfx_chime(55, 90, t + ms_frames(k * 650u), 128);
        sfx_thunk(t + ms_frames(k * 650u), 43, 40, 128);
    }
    as.bell_t = fm1_ms;
}
static int as_update(void)
{
    int32_t e;
    led_btn_mask = BIT(BTN_EDIT) | (as.bells ? BIT(BTN_REC) : 0u);
    if ((e = in.enc[ENC_KNOB1] + in.enc[ENC_SELECT]) != 0) { as.h = ((as.h + e) % AS_H + AS_H) % AS_H; sfx_tick(0, 88, 40, 128, 0); }
    if ((e = in.enc[ENC_KNOB2]) != 0) { as.m = ((as.m + e) % AS_M + AS_M) % AS_M; sfx_click(0, 40, 90); }
    if ((e = in.enc[ENC_KNOB3]) != 0) { as.s = ((as.s + e) % AS_S + AS_S) % AS_S; sfx_click(0, 40, 170); sfx_tine(84 + as.s, 30, 0, 170, 0); }
    if (PRESSED(BTN_REC) && as.bells) as_ring_bells();
    return as.h == as.th && as.m == as.tm && as.s == as.ts;
}
static void as_ease(int32_t *shown, int32_t v, int32_t period)
{
    int32_t t = v * 256, d = t - *shown;
    if (d > period * 128) d -= period * 256;
    if (d < -period * 128) d += period * 256;
    *shown += d / 3 + (d > 0) - (d < 0);
    *shown = ((*shown % (period * 256)) + period * 256) % (period * 256);
}
static void as_draw(uint32_t won)
{
    int32_t cx = 104, cy = 118, k;
    int okh = as.h == as.th, okm = as.m == as.tm, oks = as.s == as.ts;
    uint32_t a;
    if (won && !as.fin_t) as.fin_t = fm1_ms;
    as_ease(&as.sh, as.h, AS_H); as_ease(&as.sm, as.m, AS_M); as_ease(&as.ss, as.s, AS_S);
    g_cab_wall(cx, cy, 0);
    g_disc(cx, cy, 94, C_ROSE);
    g_disc(cx, cy, 92, RGB(40, 20, 70));
    g_disc(cx, cy, 90, RGB(16, 10, 50));
    for (k = 0; k < 30; k++) px_set(cx - 80 + (k * 53) % 160, cy - 70 + (k * 37) % 140, C_CREAM);
    g_ring(cx, cy, 88, 90, okh ? C_VERD : C_GOLD);
    for (k = 0; k < AS_H; k++) {                /* the hours */
        char t[3];
        a = (uint32_t)(k * 65536 / AS_H) - 16384u;
        fmt_u(t, (uint32_t)(k ? k : 12));
        g_text_c(px16(cx, 77 * 16, a) >> 4, (py16(cy, 77 * 16, a) >> 4) - 8, &FONT_S, t, C_CREAM);
    }
    /* the star ring: six constellations; the one at the top stands in the window */
    g_ring(cx, cy, 62, 64, oks ? C_VERD : C_GOLD);
    g_ring(cx, cy, 36, 38, C_GOLD);
    for (k = 0; k < AS_S; k++) {
        a = (uint32_t)((k * 256 - as.ss) * (65536 / 256) / AS_S) - 16384u;
        as_con(px16(cx, 50 * 16, a) >> 4, py16(cy, 50 * 16, a) >> 4, k, 6, k == as.s && oks ? C_VERD : C_STEEL_L);
    }
    g_frame(cx - 13, cy - 63, 27, 26, oks ? C_VERD : C_GLOW);   /* the window at the top */
    /* the moon's window, in the middle */
    g_disc(cx, cy + 4, 18, RGB(10, 6, 30));
    as_moon(cx, cy + 4, 13, (as.sm + 128) / 256 % AS_M, RGB(10, 6, 30));
    if (okm) g_ring(cx, cy + 4, 16, 18, C_VERD);
    /* the sun hand */
    a = (uint32_t)(as.sh * (65536 / 256) / AS_H) - 16384u;
    g_hand(cx, cy, 72, 10, a, 4, okh ? C_VERD : C_GOLD);
    g_disc(px16(cx, 72 * 16, a) >> 4, py16(cy, 72 * 16, a) >> 4, 5, C_GOLD);
    g_led(px16(cx, 72 * 16, a) >> 4, py16(cy, 72 * 16, a) >> 4, 3, C_GLOW);
    g_disc(cx, cy - 26, 3, C_BRASS_D);
    /* the parchment: the sign asked for */
    g_cab_panel(196, 26, 42, 164, C_ROSE);
    if (as.bells) {                             /* the hour: on the bells */
        int ring = fm1_ms - as.bell_t < (uint32_t)(as.th + 1) * 650u + 200u;
        g_disc(217, 52, 9, ring ? C_GOLD : C_BRASS_D);
        g_rect(208, 52, 19, 6, ring ? C_GOLD : C_BRASS_D);
        g_disc(217, 60, 3, C_NIGHT);
        g_text_c(217, 66, &FONT_S, "REC", C_CYAN);
    } else {
        char t[3];
        fmt_u(t, (uint32_t)(as.th ? as.th : 12));
        g_text_c(217, 48, &FONT_S, t, C_CREAM);
    }
    as_moon(217, 112, 11, as.tm, C_PANEL);
    g_rrect(201, 136, 32, 38, 3, RGB(16, 10, 50));
    as_con(217, 155, as.ts, 7, C_CREAM);
    g_led(204, 36, 1, okh ? C_VERD : C_RUBY); g_led(204, 98, 1, okm ? C_VERD : C_RUBY); g_led(204, 136, 1, oks ? C_VERD : C_RUBY);
    {   /* key lights: C hour, E moon, G stars, lit when right */
        static const uint8_t HK[3] = {7, 11, 14};
        int okk[3] = {okh, okm, oks};
        for (k = 0; k < 3; k++) { fm1_led_key_glow(KEY_LED(HK[k]), 1); fm1_led_key(KEY_LED(HK[k]), okk[k]); }
    }
    if (won) {                                  /* the procession: figures pass the window, the bells */
        uint32_t el = fm1_ms - as.fin_t;
        for (k = 0; k < 6; k++) {
            int32_t x = (int32_t)((el / 12u + (uint32_t)k * 40u) % 260u) - 10;
            g_disc(x, 32, 4, k & 1 ? C_GOLD : C_STEEL_L);
            g_rect(x - 3, 36, 7, 10, k & 1 ? C_BRASS_D : C_STEEL_D);
        }
    }
}
static void as_leave(void) { ui_keys_clear(); au_cut(9); }
static const game_t G_ASTRO = {BIT(BTN_EDIT), 2600, as_enter, as_update, as_draw, as_leave};
