/* SPDX-License-Identifier: GPL-3.0-only */
/* Chapter V: THE STRONGBOX. Listen a three-wheel lock open.
 *
 * Every number of the dial clicks as it passes. Turned the right way for the wheel being set (I right,
 * II left, III right), the fence brushes that wheel: near its gate the click gains a second, heavier
 * "clunk" that grows to its loudest at the gate itself, two numbers either side fainter. The stethoscope's
 * needle and the key LEDs (a bar graph) show the same thing for the eye. SEL lets the wheel drop: at its gate
 * it falls in with a clack; anywhere else nothing catches. With three wheels down, PLAY pulls the handle.
 * The combination is new each time. Turned the wrong way, the wheel does not listen: only a hollow click. */

#define SF_N sf.n                                /* puzzles.txt numbers */
static struct {
    int32_t pos, shown;                         /* the dial: number at the index = pos mod 40; drawn, Q8 */
    int32_t combo[5];
    int32_t n, wheels, near;
    uint32_t stage;                             /* wheels set: 0..3 */
    int32_t meter;                              /* the stethoscope, 0..256, decays */
    int32_t last_dir;
    uint32_t msg_t; const char *msg;
    uint32_t set_t[3];
    uint32_t pull_t;                            /* the finale: the handle pulled */
} sf;

static int32_t sf_num(void) { return ((sf.pos % SF_N) + SF_N) % SF_N; }
static int32_t sf_dist(int32_t a, int32_t b) { int32_t d = iabs(a - b) % SF_N; return d > SF_N / 2 ? SF_N - d : d; }

static void sf_enter(void)
{
    uint32_t i;
    sf.n = PRM(P_SAFE_NUMBERS); sf.wheels = PRM(P_SAFE_WHEELS); sf.near = PRM(P_SAFE_NEAR);
    for (i = 0; i < (uint32_t)sf.wheels; i++) {
        int32_t c, ok;
        do {
            uint32_t j;
            c = (int32_t)rndn(SF_N);
            ok = sf_dist(c, 0) > 3;             /* not where the dial rests */
            for (j = 0; j < i; j++) if (sf_dist(c, sf.combo[j]) < clampi(sf.n / (sf.wheels * 2), 2, 6)) ok = 0;
        } while (!ok);
        sf.combo[i] = c;
    }
    sf.pos = sf.shown = 0; sf.stage = 0; sf.meter = 0; sf.last_dir = 0; sf.pull_t = 0; sf.msg = 0;
}
static int32_t sf_dir_wanted(void) { return (sf.stage & 1u) ? -1 : 1; }

static void sf_say(const char *m) { sf.msg = m; sf.msg_t = fm1_ms; }

static int sf_update(void)
{
    int32_t e = in.enc[ENC_SELECT] + in.enc[ENC_ALGORITHM] + in.enc[ENC_KNOB1] + in.enc[ENC_KNOB2] + in.enc[ENC_KNOB3] + in.enc[ENC_KNOB4] + in.enc[ENC_PRESETS], step;   /* (any knob turns the dial) */
    led_btn_mask = BIT(BTN_EDIT) | (sf.stage < (uint32_t)sf.wheels ? BIT(BTN_SEL) : BIT(BTN_PLAY));
    led_btn_blink = sf.stage == (uint32_t)sf.wheels ? BIT(BTN_PLAY) : 0u;
    while (e) {                                 /* one number at a time: each its own click, a little apart */
        uint32_t at = au_time() + ms_frames(10u * (uint32_t)(iabs(e) - 1));
        step = e > 0 ? 1 : -1;
        e -= step;
        sf.pos += step;
        sf.last_dir = step;
        if (sf.stage < (uint32_t)sf.wheels && step == sf_dir_wanted()) {
            int32_t d = sf_dist(sf_num(), sf.combo[sf.stage]);
            sfx_click(at, 45, 128);
            if (d <= sf.near) {                       /* the fence on the wheel's gate: a second, heavier sound */
                static const uint8_t VEL[4] = {120, 62, 30, 16};
                sfx_thunk(at + ms_frames(14), 52, VEL[d], 128);
                sfx_click(at + ms_frames(14), VEL[d] / 2u, 128);
                if (VEL[d] * 2 > sf.meter) sf.meter = VEL[d] * 2;
            }
        } else {
            au_ev_t h = {0};                    /* hollow: nothing listening behind the dial */
            h.at = at; h.inc1 = note_inc(70); h.a1 = 1200; h.an = 2200; h.d1 = 6; h.dn = 2; h.lp = 9000; h.hp = 1; h.pan = 128;
            au_post(&h);
        }
    }
    if (PRESSED(BTN_SEL) && sf.stage < (uint32_t)sf.wheels) {
        if (sf_num() == sf.combo[sf.stage] && sf.last_dir == sf_dir_wanted()) {
            sfx_thunk(0, 40, 127, 128);
            sfx_click(ms_frames(20) + au_time(), 120, 128);
            sf.set_t[sf.stage] = fm1_ms;
            sf.stage++;
            sf_say(sf.stage == (uint32_t)sf.wheels ? T_SF_ALL_DOWN : (sf.stage & 1u) ? T_SF_NOW_LEFT : T_SF_NOW_RIGHT);
        } else {
            sfx_thunk(0, 58, 30, 128);
            sf_say(T_SF_NO_CATCH);
        }
    }
    if (PRESSED(BTN_PLAY)) {
        if (sf.stage == (uint32_t)sf.wheels) return 1;
        sfx_thunk(0, 50, 80, 200);              /* the handle will not move */
        sf_say(T_SF_BOLTS_HOLD);
    }
    sf.meter -= sf.meter / 10 + 1;
    if (sf.meter < 0) sf.meter = 0;
    return 0;
}

static void sf_draw(uint32_t won)
{
    int32_t cx = 120, cy = 108, R = 62, i;
    uint32_t open = 0, bolts = 0;
    {
        int32_t d = sf.pos * 256 - sf.shown;
        sf.shown += d / 2 + (d > 0) - (d < 0);
    }
    if (won) {
        uint32_t el;
        if (!sf.pull_t) {
            sf.pull_t = fm1_ms;
            sfx_thunk(au_time() + ms_frames(100), 38, 127, 128);
            sfx_thunk(au_time() + ms_frames(400), 42, 110, 60);
            sfx_thunk(au_time() + ms_frames(470), 42, 110, 200);
            sfx_motif(au_time() + ms_frames(1500));
        }
        el = fm1_ms - sf.pull_t;
        bolts = el > 300u ? (el > 700u ? 256u : (el - 300u) * 256u / 400u) : 0u;
        open = el > 900u ? (uint32_t)ease_out((int32_t)((el - 900u) * 256u / 1200u)) : 0u;
    }
    /* closed: the baked door (Metropolis rays, bolt strips). Opening: the strongbox's inside, the map of the line
     * coming out of the dark, and the door redrawn live, foreshortened as it swings on its left hinge */
    if (!open) g_bg(BG_SAFE);
    else {
        g_bg(BG_ART_MAP);                       /* (the map the safe holds: the story's map picture) */
        g_rect(0, 168, SW, 54, RGB(30, 24, 20));    /* (the safe's floor under it) */
        g_rect(6, 168, 228, 2, C_BRASS_D);
        g_dim(0, 22, SW, 200, open);            /* (out of the dark as the door swings) */
        g_frame(6, 24, 228, 198, C_BRASS_D);
        {
            int32_t w = 228 - (int32_t)open * 210 / 256;
            g_rect(6, 24, w, 198, RGB(14, 14, 18));
            g_frame(6, 24, w, 198, C_GOLD);
            if (w > 40) g_frame(9, 27, w - 6, 192, C_BRASS_D);
        }
    }
    if (!open) {
        int32_t dcx = cx;
        uint32_t rot = (uint32_t)(-(sf.shown * 65536 / SF_N / 256));
        /* the dial: a gold ring of 40 numbers, a dark sunburst face, a gold boss */
        g_disc(dcx, cy, R + 6, C_BRASS_D);
        g_disc(dcx, cy, R + 4, C_GOLD);
        g_disc(dcx, cy, R - 16, RGB(26, 24, 26));
        g_rays(dcx, cy, R - 17, 24, rot, RGB(58, 54, 58));
        for (i = 0; i < SF_N; i++) {
            uint32_t a = rot + (uint32_t)(i * 65536 / SF_N) - 16384u;
            int32_t r0 = i % 5 ? R - 3 : R - 8;
            g_line(px16(dcx, r0 * 16, a) >> 4, py16(cy, r0 * 16, a) >> 4, px16(dcx, (R + 2) * 16, a) >> 4, py16(cy, (R + 2) * 16, a) >> 4, C_BRASS_DD);
            if (i % 5 == 0) {
                char t[4];
                fmt_u(t, (uint32_t)i);
                g_text_c(px16(dcx, (R - 11) * 16, a) >> 4, (py16(cy, (R - 11) * 16, a) >> 4) - 8, &FONT_S, t, C_BRASS_DD);
            }
        }
        g_disc(dcx, cy, 17, C_BRASS_D);
        g_disc(dcx, cy, 11, C_GOLD);
        for (i = 0; i < 12; i++) {               /* the grip */
            uint32_t a = rot + (uint32_t)i * 5461u;
            g_disc(px16(dcx, 14 * 16, a) >> 4, py16(cy, 14 * 16, a) >> 4, 2, C_BRASS_DD);
        }
        {   /* the index: a neon cyan point */
            int32_t q[6] = {dcx * 16 - 80, (cy - R - 13) * 16, dcx * 16 + 96, (cy - R - 13) * 16, dcx * 16 + 8, (cy - R - 4) * 16};
            g_led(dcx, cy - R - 9, 3, C_CYAN_D);
            g_tri16(q[0], q[1], q[2], q[3], q[4], q[5], C_CYAN);
        }
        {   /* the handle: a gold lever on the right; pulled in the finale */
            uint32_t ha = won ? 16384u - (uint32_t)clampi((int32_t)(fm1_ms - sf.pull_t), 0, 300) * 40u : 16384u;
            int32_t hx = 196, hy = 108;
            g_disc(hx, hy, 8, C_BRASS_D);
            g_hand(hx, hy, 34, 0, ha, 5, C_GOLD);
            g_disc(hx, hy, 4, C_BRASS_L);
        }
        for (i = 0; i < 3; i++) {                /* the bolts slide back in the finale */
            int32_t by = 46 + i * 64, out = 8 - (int32_t)bolts * 12 / 256;
            if (out > 0) g_rect(234 - out, by, out, 9, C_STEEL_L);
        }
    }
    /* the stethoscope: an engraved needle meter and LEDs on the baked rose panel; the wheels' lamps */
    if (!won) {
        int32_t mx = 170, my = 217, i2;
        uint32_t na = 49152u - 7000u + (uint32_t)(sf.meter * 14000 / 256);
        for (i2 = 0; i2 <= 4; i2++) {
            uint32_t a = 49152u - 7000u + (uint32_t)(i2 * 3500);
            g_line(px16(mx, 18 * 16, a) >> 4, py16(my, 18 * 16, a) >> 4, px16(mx, 22 * 16, a) >> 4, py16(my, 22 * 16, a) >> 4, i2 == 4 ? C_RUBY : C_ROSE);
        }
        g_hand(mx, my, 20, 0, na, 1, C_CREAM);
        for (i2 = 0; i2 < 5; i2++) {
            int32_t lx = 202 + (i2 % 3) * 8, ly = 192 + (i2 / 3) * 10;
            if (sf.meter > i2 * 50 + 10) g_led(lx, ly, 2, i2 == 4 ? C_RUBY : C_ROSE);
            else g_disc(lx, ly, 2, C_ROSE_D);
        }
        for (i2 = 0; i2 < sf.wheels; i2++) {
            int32_t lx = 12 + i2 * 10;
            g_disc(lx, 198, 5, C_BRASS_D);
            if ((uint32_t)i2 < sf.stage) g_led(lx, 198, 3, C_AMBER);
            else g_disc(lx, 198, 3, RGB(40, 34, 30));
        }
        if (sf.stage < (uint32_t)sf.wheels) {
            const char *d = sf_dir_wanted() > 0 ? T_SF_TURN_RIGHT : T_SF_TURN_LEFT;
            g_text(56, 190, &FONT_S, d, C_CYAN);
            {
                static const char *const RN[5] = {"I", "II", "III", "IV", "V"};
                char w[16], *q = w;
                q = str_cat(q, T_SF_WHEEL " "); str_cat(q, RN[sf.stage]);
                g_text(56, 204, &FONT_S, w, C_GOLD);
            }
        } else
            g_text(56, 196, &FONT_S, T_SF_HANDLE, C_AMBER);
        if (sf.msg && fm1_ms - sf.msg_t < 1300u) { g_rect(60, 28, 120, 18, C_LACQ); g_text_c(120, 29, &FONT_S, sf.msg, C_GLOW); }
    }
    /* key LEDs: the stethoscope as a bar graph */
    {
        int32_t lit = won ? 0 : sf.meter * 27 / 256;
        for (i = 0; i < 27; i++) fm1_led_key(KEY_LED((uint32_t)i), i < lit);
    }
}

static void sf_leave(void) { ui_keys_clear(); led_btn_blink = 0; }
static const game_t G_SAFE = {BIT(BTN_EDIT) | BIT(BTN_SEL), 3200,
                              sf_enter, sf_update, sf_draw, sf_leave};
