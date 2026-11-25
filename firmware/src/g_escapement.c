/* SPDX-License-Identifier: GPL-3.0-only */
/* Chapter I: THE COURIER'S HEART. Regulate a chronometer's escapement.
 *
 * The balance beats at 5 a second (18000 bph) against the workshop's master clock. Two faults, hidden and
 * random: the RATE (the beat is too long or too short: the hairspring's active length, the regulator index,
 * K1) and the BEAT ERROR (tick and tock come unevenly: the hairspring stud, K2). As on a real timegrapher,
 * each tick is a dot: its height is how early or late it falls against the master clock, wrapped in a +-25 ms
 * window. A rate error draws a slope (up = fast), a beat error splits it into two lines. One flat line wins.
 * The ear works too: the watch ticks on the right, the master on the left (SEL silences the master) and the
 * limp of a beat error is plain to hear. Ticks are scheduled ahead at their exact sample (mo_audio.c). */

#define ESC_R esc.R                             /* the master's beat in frames (puzzles.txt beat_ms) */
#define ESC_RSTEP 13                            /* a detent of the index: 0.3 ms a beat */
#define ESC_BSTEP 132                           /* a detent of the stud: 3 ms between tick and tock */
#define ESC_WIN 1102                            /* the trace window: +-25 ms */
#define ESC_ND 128u

static struct {
    int32_t e0, b0, idx, stud;                  /* hidden faults; the knobs' positions */
    uint32_t wbase, wk, rnext, rk;              /* the watch's next unshifted beat, its count; the master's next beat */
    uint32_t last_tick, last_half;              /* when the last watch tick sounded (for the wheel), its beat length */
    uint32_t ref0;
    struct { uint32_t t; int8_t y; uint8_t odd; } dot[ESC_ND];
    uint32_t nd;
    uint32_t good;                              /* consecutive true beats */
    uint8_t ref_on;
    uint32_t idx_t, stud_t;                     /* when a knob last moved (a glint on the part) */
    uint32_t ref_flash;
    uint32_t R, hold;                           /* the beat (frames), true beats needed */
} esc;

static int32_t esc_rate(void) { return esc.e0 - esc.idx; }   /* 0 = true; > 0 slow */
static int32_t esc_beat(void) { return esc.b0 + esc.stud; }

static void esc_enter(void)
{
    int32_t s = rndn(2) ? 1 : -1;
    esc.R = ms_frames((uint32_t)PRM(P_ESCAPEMENT_BEAT_MS));
    esc.hold = (uint32_t)PRM(P_ESCAPEMENT_HOLD_TICKS);
    esc.e0 = s * prm_pick(P_ESCAPEMENT_RATE_ERROR_LO, P_ESCAPEMENT_RATE_ERROR_HI);
    esc.b0 = (rndn(2) ? 1 : -1) * prm_pick(P_ESCAPEMENT_BEAT_ERROR_LO, P_ESCAPEMENT_BEAT_ERROR_HI);
    esc.idx = esc.stud = 0;
    esc.ref0 = au_time() + ms_frames(300);
    esc.rnext = esc.ref0; esc.rk = 0;
    esc.wbase = esc.ref0 + ms_frames(40u + rndn(60)); esc.wk = 0;
    esc.last_tick = esc.wbase; esc.last_half = ESC_R;
    esc.nd = 0; esc.good = 0; esc.ref_on = 1;
}

static void esc_schedule(uint32_t lookahead)
{
    uint32_t now = au_time(), horizon = now + lookahead;
    if ((int32_t)(now - esc.rnext) > (int32_t)ESC_R) {   /* paused (the card was open): both clocks skip ahead together */
        uint32_t skip = ((now - esc.rnext) / ESC_R + 1u) * ESC_R;
        esc.rnext += skip; esc.wbase += skip;
    }
    while ((int32_t)(esc.rnext - horizon) < 0) {   /* the master clock: left, soft, lower */
        if (esc.ref_on) sfx_tick(esc.rnext, (esc.rk & 1u) ? 74 : 77, 55, 40, 1);
        esc.rnext += ESC_R; esc.rk++;
    }
    while ((int32_t)(esc.wbase - horizon) < 0) {   /* the watch: right, bright; odd beats shifted by the beat error */
        uint32_t odd = esc.wk & 1u, half = (uint32_t)((int32_t)ESC_R + esc_rate() * ESC_RSTEP);
        uint32_t t = esc.wbase + (odd ? (uint32_t)(esc_beat() * ESC_BSTEP) : 0u);
        int32_t dev = (int32_t)((t - esc.ref0) % ESC_R), y;
        if (dev > (int32_t)ESC_R / 2) dev -= (int32_t)ESC_R;
        y = ((dev + ESC_WIN) % (2 * ESC_WIN + 1) + (2 * ESC_WIN + 1)) % (2 * ESC_WIN + 1) - ESC_WIN;   /* wrap */
        sfx_tick(t, odd ? 86 : 90, 95, 215, 1);
        esc.dot[esc.nd % ESC_ND].t = t;
        esc.dot[esc.nd % ESC_ND].y = (int8_t)(y * 38 / ESC_WIN);   /* +-38 px */
        esc.dot[esc.nd % ESC_ND].odd = (uint8_t)odd;
        esc.nd++;
        esc.wbase += half; esc.wk++;
        if (esc_rate() == 0 && esc_beat() == 0) esc.good++;
        else esc.good = 0;
    }
}

static void esc_track(void)
{
    esc_schedule(ms_frames(120));
    {   /* the wheel follows the tick that is sounding now */
        uint32_t now = au_time(), i;
        for (i = esc.nd > 4u ? esc.nd - 4u : 0u; i < esc.nd; i++)
            if ((int32_t)(esc.dot[i % ESC_ND].t - now) <= 0 && esc.dot[i % ESC_ND].t != esc.last_tick) {
                esc.last_half = esc.dot[i % ESC_ND].t - esc.last_tick;
                esc.last_tick = esc.dot[i % ESC_ND].t;
            }
        if ((int32_t)(esc.rnext - ESC_R - now) <= 0 && now - (esc.rnext - ESC_R) < ms_frames(60)) esc.ref_flash = fm1_ms;
    }
}
static int esc_update(void)
{
    int32_t d1 = enc_main(), d2 = in.enc[ENC_KNOB2];
    if (d1) { esc.idx = clampi(esc.idx + d1, -16, 16); esc.idx_t = fm1_ms; sfx_click(0, 50, 200); }
    if (d2) { esc.stud = clampi(esc.stud + d2, -12, 12); esc.stud_t = fm1_ms; sfx_click(0, 40, 170); }
    if (PRESSED(BTN_SEL)) esc.ref_on ^= 1u;
    led_btn_mask = BIT(BTN_EDIT) | (esc.ref_on ? BIT(BTN_SEL) : 0u);
    if (esc.nd && au_time() - esc.last_tick < ms_frames(60)) led_btn_mask |= BIT(BTN_PLAY);   /* PLAY blinks with each tick */
    esc_track();
    return esc.good >= esc.hold;                /* (14: ~3 s of one flat line) */
}

static void esc_draw_balance(int32_t cx, int32_t cy, uint32_t ang, int glow)
{
    int32_t r = 34, i;
    uint32_t k;
    if (glow) { g_disc(cx, cy, r + 10, mix565(RGB(90, 68, 36), C_GLOW, 90)); g_disc(cx, cy, r + 5, mix565(RGB(90, 68, 36), C_GLOW, 150)); }
    g_ring(cx, cy, r - 5, r, C_BRASS_D);
    g_ring(cx, cy, r - 4, r - 1, C_BRASS);
    g_ring16(cx * 16 + 8, cy * 16 + 8, (r - 3) * 16, (r - 3) * 16 + 8, C_BRASS_L);
    for (k = 0; k < 3u; k++) {                  /* the arms */
        uint32_t a = ang + k * 21845u;
        g_thick16(cx * 16 + 8, cy * 16 + 8, px16(cx, (r - 4) * 16, a) + 8, py16(cy, (r - 4) * 16, a) + 8, 48, C_BRASS_D);
    }
    for (k = 0; k < 12u; k++) {                 /* timing screws on the rim */
        uint32_t a = ang + k * 5461u + 2730u;
        int32_t sx = px16(cx, (r - 2) * 16, a) >> 4, sy = py16(cy, (r - 2) * 16, a) >> 4;
        g_disc(sx, sy, 2, (k & 1u) ? C_STEEL : C_BRASS_D);
    }
    /* the hairspring: an Archimedean spiral, its outer end held at the stud, its inner end turning with the wheel */
    {
        int32_t px = 0, py = 0;
        for (i = 0; i <= 140; i++) {
            int32_t rad = 4 * 16 + i * 17 * 16 / 140;
            uint32_t a = (uint32_t)i * 65536u * 6u / 140u + (uint32_t)(((int32_t)ang * (140 - i)) / 140) + 50000u;
            int32_t x = px16(cx, rad, a) >> 4, y = py16(cy, rad, a) >> 4;
            if (i) g_line(px, py, x, y, C_STEEL);
            px = x; py = y;
        }
    }
    g_disc(cx, cy, 4, C_BRASS_D);
    g_jewel(cx, cy);
}

static void esc_draw(uint32_t won)
{
    uint32_t now = au_time(), i, n;
    int32_t cx = 66, cy = 78, swing;
    uint32_t ang;
    if (won) esc_track();
    g_bg(BG_ESC);
    /* the balance's swing: from one extreme to the other between two ticks */
    {
        uint32_t half = esc.last_half > ESC_R / 4u && esc.last_half < 3u * ESC_R ? esc.last_half : ESC_R;
        uint32_t el = now - esc.last_tick;
        int32_t frac = (int32_t)(el * 256u / half);
        int32_t dir = (esc.wk & 1u) ? 1 : -1;
        frac = clampi(frac, 0, 256);
        swing = (mo_cos((uint32_t)frac * 128u) * 30000) >> 15;   /* +-165 degrees */
        ang = (uint32_t)(dir * swing);
    }
    esc_draw_balance(cx, cy, ang, won > 0u);
    /* the regulator index over its scale, the stud carrier */
    {
        int32_t i2;
        /* the scale: up and to the left, S (slow) .. F (fast) */
        uint32_t ia = 43000u + (uint32_t)(esc.idx * 300);
        for (i2 = -16; i2 <= 16; i2 += 4) {
            uint32_t a = 43000u + (uint32_t)(i2 * 300);
            g_line(px16(cx, 41 * 16, a) >> 4, py16(cy, 41 * 16, a) >> 4, px16(cx, (i2 ? 44 : 46) * 16, a) >> 4, py16(cy, (i2 ? 44 : 46) * 16, a) >> 4, C_CREAM);
        }
        g_text(px16(cx, 49 * 16, 43000u - 4800u) / 16 - 8, py16(cy, 49 * 16, 43000u - 4800u) / 16 - 6, &FONT_S, "S", C_CREAM);
        g_text(px16(cx, 48 * 16, 43000u + 4800u) / 16 + 4, py16(cy, 48 * 16, 43000u + 4800u) / 16 - 8, &FONT_S, "F", C_CREAM);
        g_hand(cx, cy, 45, 8, ia, 3, fm1_ms - esc.idx_t < 200u ? C_GLOW : C_CREAM);
        g_hand(cx, cy, 30, 0, 26000u + (uint32_t)(esc.stud * 700), 4, fm1_ms - esc.stud_t < 200u ? C_STEEL_L : C_STEEL_D);
        g_disc(px16(cx, 30 * 16, 26000u + (uint32_t)(esc.stud * 700)) >> 4, py16(cy, 30 * 16, 26000u + (uint32_t)(esc.stud * 700)) >> 4, 3, C_STEEL_D);
    }
    /* the pallet fork and escape wheel, snapping with each tick */
    {
        uint32_t el = now - esc.last_tick, snap = el < ms_frames(25) ? el * 256u / ms_frames(25) : 256u;
        uint32_t ewa = esc.wk * (65536u / 30u) + (snap * (65536u / 30u) >> 8);
        int32_t fx = 128, fy = 78, ex = 172, ey = 78;
        int32_t rock = (esc.wk & 1u) ? 2600 : -2600;
        g_gear(ex, ey, 22, 15, ewa, RGB(226, 208, 170), RGB(130, 112, 84), RGB(250, 240, 210), 0);
        g_hand(fx, fy, 30, 26, (uint32_t)rock, 4, C_STEEL_D);
        g_hand(fx, fy, 16, 0, (uint32_t)(rock + 16384), 3, C_STEEL);
        g_hand(fx, fy, 16, 0, (uint32_t)(rock - 16384), 3, C_STEEL);
        g_jewel(fx, fy);
        g_jewel(fx + 26, fy - 12); g_jewel(fx + 26, fy + 12);
        g_jewel(ex, ey);
    }
    /* the master clock lamp */
    {
        int on = esc.ref_on && fm1_ms - esc.ref_flash < 70u;
        g_disc(216, 42, 10, C_BRASS_D);
        g_disc(216, 42, 8, RGB(20, 20, 24));
        if (on) g_led(216, 42, 6, C_AMBER);
        else g_disc(216, 42, 6, esc.ref_on ? RGB(110, 70, 30) : RGB(40, 36, 34));
    }
    /* the timegraph: a cyan CRT, the newest beat at the right; tick dots cyan, tock dots rose */
    {
        int32_t x0 = 10, w = 166, mid = 177;
        n = 0;
        for (i = esc.nd; i > 0u && n < 80u; i--) {
            uint32_t j = (i - 1u) % ESC_ND;
            int32_t x, y;
            uint16_t c;
            if ((int32_t)(esc.dot[j].t - now) > 0) continue;   /* not yet heard */
            x = x0 + w - 4 - (int32_t)n * 2;
            y = mid + esc.dot[j].y * 34 / 38;
            c = won ? C_VERD : (esc.dot[j].odd ? C_ROSE : C_CYAN);
            if (n < 3u) g_led(x, y, 1, c);
            else g_rect(x, y - 1, 2, 2, n < 40u ? c : mix565(RGB(4, 18, 22), c, 160));
            n++;
        }
    }
    /* the knobs, as K1 and K2 stand */
    g_text_c(206, 138, &FONT_S, T_ESC_RATE, C_CYAN);
    g_knob(206, 165, 9, 128 - esc.idx * 8, C_CYAN);
    g_text_c(206, 180, &FONT_S, T_ESC_BEAT, C_ROSE);
    g_knob(206, 205, 8, 128 + esc.stud * 10, C_ROSE);
    /* the readout, as a real timegrapher gives it: rate in seconds a day (+ fast), beat error in ms */
    {
        char t[32], *p = t;
        int32_t sd = -esc_rate() * ESC_RSTEP * 86400 / (int32_t)ESC_R, be = iabs(esc_beat()) * 3;
        g_rect(10, 121, 166, 14, C_LACQ);
        p = str_cat(p, sd > 0 ? "+" : sd < 0 ? "-" : " "); fmt_u(p, (uint32_t)iabs(sd)); while (*p) p++;
        p = str_cat(p, " s/d");
        g_text(14, 120, &FONT_S, t, sd ? C_CYAN : C_VERD);
        p = t; fmt_u(p, (uint32_t)be); while (*p) p++;
        p = str_cat(p, ".0 ms");
        g_text_r(172, 120, &FONT_S, t, be ? C_ROSE : C_VERD);
    }
    /* the keys as a light timegraph: the last tick's place, centre key = in time with the master */
    if (esc.nd) {
        uint32_t j = (esc.nd - 1u) % ESC_ND, k;
        int32_t pos = 13 + esc.dot[j].y * 13 / 38;
        for (k = 0; k < 27u; k++) { fm1_led_key(KEY_LED(k), (int32_t)k == pos); fm1_led_key_glow(KEY_LED(k), k == 13u); }
    }
    if (won) {                                  /* the clock, true again, chimes: the tune the music box down the line has lost */
        static uint32_t chimed;
        if (won < 200u && chimed != (uint32_t)(uintptr_t)cur_ch + fm1_ms / 100000u) {
            uint32_t c, i, t0 = au_time() + ms_frames(700);
            chimed = (uint32_t)(uintptr_t)cur_ch + fm1_ms / 100000u;
            for (c = 0; c < NCHAP; c++) if (CHAP[c].type == MECH_MUSICBOX) {
                const mo_list_t *tl = &CHAP[c].lst[L_MUSICBOX_TUNE];
                uint32_t n = tl->n / 16u, pick;
                if (!n) break;
                if (!save.mb_tune) save.mb_tune = (uint8_t)(1u + rndn(n));
                pick = (uint32_t)(save.mb_tune - 1u) % n;
                for (i = 0; i < 16u; i++) { int32_t v = tl->v[pick * 16u + i]; if (v >= 1 && v <= 8) sfx_tine(CHAP[c].lst[L_MUSICBOX_COMB].v[v - 1], 70, t0 + ms_frames(i * 270u), 128, 1); }
                break;
            }
        }
        g_rect(10, 121, 166, 14, C_LACQ);
        g_text_c(93, 120, &FONT_S, T_ESC_DONE, C_VERD);
    }
}

static void esc_leave(void) { au_cut(1); ui_keys_clear(); }
static const game_t G_ESCAPEMENT = {BIT(BTN_EDIT) | BIT(BTN_SEL), 1800,
                                    esc_enter, esc_update, esc_draw, esc_leave};
