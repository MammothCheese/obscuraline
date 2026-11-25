/* SPDX-License-Identifier: GPL-3.0-only */
/* THE NEGATIVE PHOTOGRAPH: a glass plate from the stationmaster's desk, printed again.
 *
 * The picture is the station long ago, drawn here and then spoiled the way an old plate is: a negative, out of
 * focus, wrongly exposed, flat. K1 EXPOSURE (light and dark), K2 FOCUS (the blur), K3 CONTRAST (how far apart the
 * greys sit), K4 DEVELOPMENT (too little: still a faint negative; too much: dark, fogged and grainy). Each has its
 * right place; nothing says where but the print itself. Held right a moment, the print is restored, and it shows
 * what the station no longer has: a sign on the platform, pointing down, to SHINZO. Every start is the right print
 * moved off by bounded amounts; the margins are a few detents. The print comes up in the line's colours: night
 * indigo, hot magenta, a pale gold, the striped sun low over the hills and the grid running to them. */

#define PH_W 160
#define PH_H 100
#define PH_X 40
#define PH_Y (STAGE_Y0 + 8)
static uint8_t ph_src[PH_W * PH_H] __attribute__((section(".pool")));   /* the scene, grey */
static uint8_t ph_blur[PH_W * PH_H] __attribute__((section(".pool")));  /* blurred by the focus */

static struct {
    int32_t k[4], t[4];                         /* exposure, focus, contrast, development; their right places */
    int32_t blur_r;                             /* the blur last made */
    uint16_t lut[256];                          /* grey -> sepia (swapped for the screen), from the knobs */
    uint32_t good_t, won, fin_t, knob_t[4], t0;  /* (t0: since the print came out: the hints come with time) */
} ph;

static void ph_scene(void)                      /* the photograph: drawn on the screen in greys, read back */
{
    /* an art photograph, one-point perspective: down the platform into the striped sun on the horizon; the canopy
     * and its pillars in silhouette running to it; the rails catching the light; a lone figure, his long shadow;
     * in the foreground the hanging sign, SHINZO, its arrow down to a stair in the platform; the corners vignetted */
    int32_t x, y, k;
    uint32_t i;
    const int32_t VX = 104, VY = 50;            /* the vanishing point: the sun's foot */
    #define G(v) RGB(v, v, v)
    #define AT(px, py) (PH_X + (px)), (PH_Y + (py))
    for (y = 0; y < VY; y++) g_rect(PH_X, PH_Y + y, PH_W, 1, G(36 + y * 170 / VY));   /* the sky: dark above, bright at the horizon */
    for (y = -24; y <= 0; y++) {                 /* the sun, striped, its foot on the horizon */
        int32_t hw = (int32_t)mo_isqrt((uint32_t)(576 - y * y));
        if (y > -12 && ((y + 24) % 4) < (y + 14) / 4) continue;
        g_rect(PH_X + VX - hw, PH_Y + VY + y, hw * 2, 1, G(252));
    }
    for (x = 0; x < PH_W; x++) {                 /* the far skyline, low, both sides of the sun */
        int32_t h = (x < VX - 26 || x > VX + 26) ? 3 + ((x * 13) % 7) + ((x / 9) % 3) * 2 : 0;
        if (h) g_rect(PH_X + x, PH_Y + VY - h, 1, h, G(30));
    }
    g_rect(PH_X, PH_Y + VY, PH_W, PH_H - VY, G(22));                       /* the ground, dark */
    for (y = VY; y < PH_H; y++) {               /* the platform on the left, lit toward the sun; its bright edge */
        int32_t xe = VX + (0 - VX) * 0 + (int32_t)((int64_t)(y - VY) * (64 - VX) / (PH_H - VY));
        int32_t lum = 120 - (y - VY) * 50 / (PH_H - VY);
        g_rect(PH_X, PH_Y + y, xe > 0 ? xe : 0, 1, G(lum));
        if (xe > 0) g_rect(PH_X + xe, PH_Y + y, 2, 1, G(235));
    }
    for (k = 0; k < 2; k++)                      /* the rails, shining back at the sun */
        g_line(AT(VX + 1 + k, VY), AT(86 + k * 52, PH_H - 1), G(225));
    for (k = 1; k < 12; k++) {                   /* the sleepers, closer together toward the sun */
        int32_t yy = VY + (PH_H - VY) * k * k / 144;
        int32_t xa = VX + (86 - VX) * (yy - VY) / (PH_H - VY), xb = VX + (138 - VX) * (yy - VY) / (PH_H - VY);
        g_rect(PH_X + xa, PH_Y + yy, xb - xa + 1, 1, G(55));
    }
    for (x = 0; x < VX; x++) {                   /* the canopy: a dark roof running to the sun */
        int32_t yb = 14 + (VY - 8 - 14) * x / VX;
        g_rect(PH_X + x, PH_Y, 1, yb, G(14));
    }
    for (k = 0; k < 6; k++) {                    /* its pillars, smaller and closer as they go */
        int32_t xx = 6 + (VX - 6) * (k * k + k * 4) / 60, top = 14 + (VY - 8 - 14) * xx / VX, bot = VY + (PH_H - VY) * (VX - xx) / VX * 3 / 4, w = 5 - k * 4 / 6;
        if (w < 1) w = 1;
        g_rect(PH_X + xx, PH_Y + top, w, bot - top, G(12));
    }
    for (k = 0; k < 5; k++) {                    /* lamp posts on the right, running to the sun, their heads lit */
        int32_t xx = PH_W - 4 - (PH_W - 4 - VX - 8) * (k * k + k * 4) / 40, ht = 46 - k * 8, base = VY + (PH_H - VY) * (5 - k) / 6;
        if (ht < 6) ht = 6;
        g_rect(PH_X + xx, PH_Y + base - ht, 2 - k / 3, ht, G(10));
        g_rect(PH_X + xx - 3 + k / 2, PH_Y + base - ht, 6 - k, 2, G(10));
        g_disc(PH_X + xx - 3 + k / 2, PH_Y + base - ht + 3, 2 - k / 3, G(250));
    }
    g_line(AT(PH_W - 1, 6), AT(VX + 6, VY - 14), G(16));   /* the wire overhead */
    {   /* a lone figure on the platform, against the light; his shadow long toward us */
        int32_t fx = 58, fy = 74;
        g_disc(PH_X + fx, PH_Y + fy - 17, 3, G(8));
        g_rect(PH_X + fx - 3, PH_Y + fy - 14, 7, 10, G(8));
        g_rect(PH_X + fx - 3, PH_Y + fy - 4, 2, 5, G(8)); g_rect(PH_X + fx + 2, PH_Y + fy - 4, 2, 5, G(8));
        for (k = 0; k < 24; k++) g_rect(PH_X + fx - k / 2 - 2, PH_Y + fy + 1 + k, 4 + k / 4, 1, G(40));
    }
    {   /* the sign in the foreground, hung from the canopy on two chains: SHINZO, its arrow down */
        int32_t sx = 10, sy = 22;
        for (k = 0; k < sy; k += 2) { g_rect(PH_X + sx + 8, PH_Y + k, 1, 1, G(120)); g_rect(PH_X + sx + 50, PH_Y + k, 1, 1, G(120)); }
        g_rect(PH_X + sx, PH_Y + sy, 60, 17, G(245)); g_rect(PH_X + sx, PH_Y + sy + 15, 60, 2, G(150));
        g_text_c(PH_X + sx + 30, PH_Y + sy + 1, &FONT_S, "SHINZO", G(10));
        for (k = 0; k < 6; k++) g_rect(PH_X + sx + 30 - k, PH_Y + sy + 25 - k, 1 + k * 2, 1, G(245));   /* the arrow */
        g_rect(PH_X + sx + 29, PH_Y + sy + 18, 3, 3, G(245));
    }
    for (k = 0; k < 6; k++) g_rect(PH_X + 8 + k * 2, PH_Y + 84 + k * 3, 44 - k * 4, 2, G(6 + k * 10));   /* the stair, going down */
    for (i = 0; i < PH_W * PH_H; i++) {         /* read it back: grey, the corners darkened */
        uint16_t c = swap16(cv_px[(PH_Y + i / PH_W) * SW + PH_X + i % PH_W]);
        int32_t px = (int32_t)(i % PH_W) - PH_W / 2, py = (int32_t)(i / PH_W) - PH_H / 2, v;
        uint32_t d2 = (uint32_t)(px * px * 4 + py * py * 10);
        v = (int32_t)(((c >> 11) * 255u / 31u + (c >> 5 & 63u) * 255u / 63u * 2u + (c & 31u) * 255u / 31u) / 4u);
        v = v * (256 - (int32_t)(d2 > 26000u ? (d2 - 26000u) / 260u > 160u ? 160u : (d2 - 26000u) / 260u : 0u)) / 256;
        ph_src[i] = (uint8_t)clampi(v, 0, 255);
    }
    #undef AT
    #undef G
}
static void ph_make_blur(int32_t r)             /* a box blur, across then down */
{
    int32_t x, y, k;
    static uint8_t row[PH_W > PH_H ? PH_W : PH_H];
    if (r <= 0) { for (x = 0; x < PH_W * PH_H; x++) ph_blur[x] = ph_src[x]; return; }
    for (y = 0; y < PH_H; y++)
        for (x = 0; x < PH_W; x++) {
            int32_t s = 0, n = 0;
            for (k = -r; k <= r; k++) { int32_t xx = x + k; if (xx >= 0 && xx < PH_W) { s += ph_src[y * PH_W + xx]; n++; } }
            ph_blur[y * PH_W + x] = (uint8_t)(s / n);
        }
    for (x = 0; x < PH_W; x++) {
        for (y = 0; y < PH_H; y++) {
            int32_t s = 0, n = 0;
            for (k = -r; k <= r; k++) { int32_t yy = y + k; if (yy >= 0 && yy < PH_H) { s += ph_blur[yy * PH_W + x]; n++; } }
            row[y] = (uint8_t)(s / n);
        }
        for (y = 0; y < PH_H; y++) ph_blur[y * PH_W + x] = row[y];
    }
}
static int32_t ph_dz(int32_t x, int32_t t) { return x > t ? x - t : x < -t ? x + t : 0; }   /* (inside the margin: nothing wrong) */
static void ph_make_lut(void)                   /* exposure, contrast, development: grey in, sepia out */
{
    int32_t v, eo = ph_dz(ph.k[0] - ph.t[0], 2) * 22, cg = clampi(64 + ph_dz(ph.k[2] - ph.t[2], 2) * 16, 4, 220), dd = ph_dz(ph.k[3] - ph.t[3], 1);   /* (a detent outside the margin: a big change) */
    for (v = 0; v < 256; v++) {
        int32_t x = ((v - 128) * cg >> 6) + 128 + eo, r, g, b;
        if (dd < 0) {                           /* underdeveloped: a faint negative */
            int32_t w = clampi(-dd * 60, 0, 256), fl = clampi(-dd * 6, 0, 120);
            x = clampi(x, 0, 255);
            x = (x * (256 - w) + (255 - x) * w) >> 8;
            x = 128 + (x - 128) * (256 - fl) / 256;
        } else if (dd > 0) {                    /* overdeveloped: dark and fogged */
            x = x * (256 - clampi(dd * 34, 0, 200)) / 256 + clampi(dd * 12, 0, 80);
        }
        x = clampi(x, 0, 255);
        if (x < 128) { r = 16 + (230 - 16) * x / 128; g = 6 + (40 - 6) * x / 128; b = 44 + (170 - 44) * x / 128; }   /* indigo -> magenta */
        else { int32_t u = x - 128; r = 230 + (255 - 230) * u / 127; g = 40 + (232 - 40) * u / 127; b = 170 + (190 - 170) * u / 127; }   /* magenta -> gold */
        ph.lut[v] = swap16(RGB(r, g, b));
    }
}
static const uint8_t PH_TOL[4] = {4, 2, 4, 2};   /* how far off each may be and still count (wider than the eye can judge: the print need not be exact) */
static int ph_ok(uint32_t i) { return iabs(ph.k[i] - ph.t[i]) <= PH_TOL[i]; }
static int ph_right(void) { return ph_ok(0) && ph_ok(1) && ph_ok(2) && ph_ok(3); }
static void ph_enter(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) { ph.t[i] = 12 + (int32_t)rndn(16); ph.knob_t[i] = 0; }
    ph.k[0] = ph.t[0] + (rndn(2) ? 8 : -8) + (int32_t)rndn(4);
    ph.k[1] = ph.t[1] + (rndn(2) ? 6 : -6) + (int32_t)rndn(3);
    ph.k[2] = ph.t[2] + (rndn(2) ? 7 : -7);
    ph.k[3] = ph.t[3] - 10 - (int32_t)rndn(4);  /* (it begins a negative: undeveloped) */
    for (i = 0; i < 4u; i++) ph.k[i] = clampi(ph.k[i], 0, 40);
    ph.good_t = 0; ph.won = 0; ph.fin_t = 0; ph.t0 = fm1_ms;
    g_begin();
    ph_scene();
    ph.blur_r = -1;
}
static int ph_update(void)
{
    static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
    uint32_t i;
    led_btn_mask = BIT(BTN_EDIT);
    if (ph.won) return 1;
    for (i = 0; i < 4u; i++) {
        int32_t e = in.enc[KN[i]];
        if (e) { ph.k[i] = clampi(ph.k[i] + e, 0, 40); ph.knob_t[i] = fm1_ms; sfx_click(0, 46 + (int32_t)i * 3, 128); }
    }
    if (ph_right()) {
        if (!ph.good_t) { ph.good_t = fm1_ms ? fm1_ms : 1u; sfx_chime(84, 70, 0, 128); sfx_chime(91, 60, au_time() + ms_frames(120), 128); }   /* (right: it rings)*/
        if (fm1_ms - ph.good_t > (uint32_t)PRM(P_PHOTO_HOLD_MS)) { ph.won = 1; ph.fin_t = fm1_ms; sfx_motif(au_time()); return 1; }
    } else ph.good_t = 0;
    return 0;
}
static void ph_draw(uint32_t won)
{
    static const char *const KL[4] = {T_PH_K1, T_PH_K2, T_PH_K3, T_PH_K4};
    static const uint16_t KC[4] = {C_AMBER, C_CYAN, C_ROSE, C_VERD};
    int32_t r = iabs(ph_dz(ph.k[1] - ph.t[1], 1)) * 2, x, y;
    uint32_t i, over = ph_dz(ph.k[3] - ph.t[3], 1) > 0;
    if (r > 7) r = 7;
    if (ph.won) r = 0;
    if (r != ph.blur_r) { ph_make_blur(r); ph.blur_r = r; }
    ph_make_lut();
    for (y = STAGE_Y0; y < STAGE_Y1; y++) g_rect(0, y, SW, 1, mix565(RGB(14, 6, 34), RGB(60, 14, 70), (uint32_t)(y - STAGE_Y0) * 256u / (uint32_t)(STAGE_Y1 - STAGE_Y0)));   /* the darkroom, violet */
    g_rrect(PH_X - 8, PH_Y - 6, PH_W + 16, PH_H + 12, 4, RGB(10, 4, 24));   /* the print in a neon frame */
    g_rframe(PH_X - 8, PH_Y - 6, PH_W + 16, PH_H + 12, 4, 2, (ph.won || won) ? C_GOLD : C_ROSE);
    g_rframe(PH_X - 4, PH_Y - 2, PH_W + 8, PH_H + 4, 2, 1, C_CYAN_D);
    for (y = 0; y < PH_H; y++) {
        uint16_t *dst = cv_px + (uint32_t)(PH_Y + y) * SW + PH_X;
        const uint8_t *src = ph_blur + y * PH_W;
        for (x = 0; x < PH_W; x++) {
            uint32_t v = src[x];
            if (over) { uint32_t h = th32((uint32_t)(y * PH_W + x) + fm1_ms / 200u); v = v + (h & 31u) > 255u ? 255u : v + (h & 31u); }   /* (grain) */
            dst[x] = ph.lut[v];
        }
    }
    if (won || ph.won) {                        /* restored: the thing that is not there now, ringed */
        uint32_t el = fm1_ms - ph.fin_t;
        if ((el / 300u) & 1u || el > 2400u) g_rframe(PH_X + 6, PH_Y + 18, 68, 32, 6, 2, C_CYAN);
        g_text_c(SW / 2, PH_Y + PH_H + 10, &FONT_S, T_PH_DONE1, C_CREAM);
        g_text_c(SW / 2, PH_Y + PH_H + 26, &FONT_S, T_PH_DONE2, C_ROSE);
    } else {
        for (i = 0; i < 4u; i++) {              /* the knobs: sliders, no mark where right is */
            int32_t yy = PH_Y + PH_H + 10 + (int32_t)i * 15, w = 140;
            g_text(8, yy - 2, &FONT_S, KL[i], KC[i]);
            g_rect(92, yy + 4, w, 2, RGB(70, 50, 56));
            g_rrect(92 + ph.k[i] * w / 40 - 3, yy, 7, 10, 2, fm1_ms - ph.knob_t[i] < 200u ? C_CREAM : KC[i]);
            if (fm1_ms - ph.t0 > 30000u && ph_ok(i)) g_text(78, yy - 2, &FONT_S, "*", C_VERD);   /* hints: after 30 s, the ones already right */
            else if (fm1_ms - ph.t0 > 60000u) g_text(78, yy - 2, &FONT_S, ph.k[i] < ph.t[i] ? ">" : "<", C_CREAM);   /* after 60 s, which way the rest go */
        }
        if (ph.good_t) {                        /* right: the frame lights, a bar fills */
            uint32_t w = (fm1_ms - ph.good_t) * 160u / (uint32_t)PRM(P_PHOTO_HOLD_MS);
            g_rframe(PH_X - 8, PH_Y - 6, PH_W + 16, PH_H + 12, 4, 2, C_VERD);
            g_rect(40, PH_Y + PH_H + 74, 160, 4, RGB(40, 20, 50)); g_rect(40, PH_Y + PH_H + 74, (int32_t)(w > 160u ? 160u : w), 4, C_VERD);
        }
    }
}
static void ph_leave(void) { ui_keys_clear(); }
static const game_t G_PHOTO = {BIT(BTN_EDIT), 3600, ph_enter, ph_update, ph_draw, ph_leave};
