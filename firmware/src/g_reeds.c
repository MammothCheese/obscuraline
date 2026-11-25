/* SPDX-License-Identifier: GPL-3.0-only */
/* Chapter VI: THE PATROL'S EYE. Tune a robot's ident chime, by ear.
 *
 * Four brass reeds in the patrol's chest should sound a chord (its ident). Each is off by a hidden amount,
 * 50 to 240 cents sharp or flat. K1..K4 turn their tuning screws (10 cents a detent); each turn sounds the reed
 * together with its true pitch from the spec, and two tones that nearly agree BEAT: the slower the throb, the
 * closer. At unison the beating stops, and the strobe above the reed stands still. KEYS C D E F tap a reed
 * alone; REC plays the spec chord, PLAY the chord as tuned. When all four are true, PLAY confirms the ident.
 * The chord (shape and root) is drawn each time. */

/* chord shapes, root, mistuning and step: puzzles.txt */
static const uint8_t RD_KEY[4] = {7, 9, 11, 12};   /* C4 D4 E4 F4 */
#define RD_STEP rd.step

static struct {
    int32_t note[4];                            /* the spec */
    int32_t off[4];                             /* cents off now */
    uint32_t sound_t[4];                        /* when the reed last sounded (strobe, glow) */
    uint32_t screw[4];                          /* screw angle, for the drawing */
    uint32_t verdict_t; int8_t verdict;         /* after PLAY: 1 confirmed, -1 refused */
    uint32_t fin_t;
    int32_t step;
} rd;

static uint32_t rd_inc(int i) { return inc_cents(rd.note[i], rd.off[i]); }
static void rd_tap(uint32_t i, int with_ref)
{
    sfx_reed(rd_inc((int)i), 0, (uint8_t)(60u + i * 45u), 6, 90);
    if (with_ref) sfx_reed(note_inc(rd.note[i]), 0, (uint8_t)(60u + i * 45u), 6, 90);
    rd.sound_t[i] = fm1_ms;
}
static void rd_key(uint32_t k)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) if (RD_KEY[i] == k) rd_tap(i, 0);
}
static void rd_enter(void)
{
    const mo_list_t *cl = LST(L_REEDS_CHORD);
    uint32_t i, c = rndn(cl->n / 4u);
    int32_t root = prm_pick(P_REEDS_ROOT_LO, P_REEDS_ROOT_HI), lo, hi;
    rd.step = PRM(P_REEDS_STEP);
    lo = (PRM(P_REEDS_ERROR_LO) + rd.step - 1) / rd.step; hi = PRM(P_REEDS_ERROR_HI) / rd.step;
    if (hi < lo) hi = lo;
    for (i = 0; i < 4u; i++) {
        int32_t o = (lo + (int32_t)rndn((uint32_t)(hi - lo + 1))) * RD_STEP;   /* a whole number of steps off */
        rd.note[i] = root + cl->v[c * 4u + i];
        rd.off[i] = rndn(2) ? o : -o;
        rd.sound_t[i] = 0; rd.screw[i] = rnd();
    }
    rd.verdict = 0; rd.fin_t = 0;
    kb_hook = rd_key;
}
static int rd_update(void)
{
    static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
    uint32_t i;
    led_btn_mask = BIT(BTN_EDIT) | BIT(BTN_PLAY) | BIT(BTN_REC);
    for (i = 0; i < 4u; i++) {
        int32_t e = in.enc[KN[i]];
        if (!e) continue;
        rd.off[i] = clampi(rd.off[i] + e * RD_STEP, -400, 400);
        rd.screw[i] += (uint32_t)(e * 3000);
        au_cut(6);
        sfx_click(0, 30, (uint8_t)(60u + i * 45u));
        rd_tap(i, 1);                           /* the reed against its spec: listen for the beating */
    }
    if (PRESSED(BTN_REC)) {                     /* the spec chord */
        au_cut(6);
        for (i = 0; i < 4u; i++) sfx_reed(note_inc(rd.note[i]), au_time() + ms_frames(i * 90u), 128, 6, 70);
    }
    if (PRESSED(BTN_PLAY)) {                    /* the chord as tuned; all true: the ident is confirmed */
        int ok = 1;
        au_cut(6);
        for (i = 0; i < 4u; i++) { sfx_reed(rd_inc((int)i), au_time() + ms_frames(i * 90u), 128, 6, 70); rd.sound_t[i] = fm1_ms + i * 90u; ok &= rd.off[i] == 0; }
        rd.verdict = ok ? 1 : -1;
        rd.verdict_t = fm1_ms;
        if (!ok) sfx_buzz(au_time() + ms_frames(700));
    }
    if (rd.verdict == 1 && fm1_ms - rd.verdict_t > 900u) return 1;
    return 0;
}

static void rd_draw(uint32_t won)
{
    int32_t i, k;
    if (won && !rd.fin_t) rd.fin_t = fm1_ms;
    /* the patrol's chest, opened: dark steel, a sunburst of rivets, the lens above watching */
    g_cab_wall(120, 44, 40);
    g_cab_panel(10, 68, 220, 150, C_ROSE);
    for (k = 0; k < 9; k++) { g_disc(22 + k * 25, 76, 1, RGB(90, 70, 130)); g_disc(22 + k * 25, 210, 1, RGB(90, 70, 130)); }
    {   /* the lens: red, glaring; cyan once the ident is confirmed */
        uint16_t lc = won ? C_CYAN : C_ROSE;   /* the lighthouse lamp: magenta, cyan once the horn is true */
        int32_t r = 14 + ((mo_sin(fm1_ms * 25u) * 2) >> 15);
        g_disc(120, 44, 22, RGB(40, 24, 70));
        g_disc(120, 44, 19, C_ROSE_D);
        g_disc(120, 44, 17, RGB(20, 8, 30));
        g_led(120, 44, r / 2, lc);
        for (k = 0; k < 8; k++) g_hand(120, 44, 21, -16, (uint32_t)k * 8192u + fm1_ms * 3u, 1, C_STEEL_D);
    }
    for (i = 0; i < 4; i++) {
        int32_t x = 42 + i * 52, y0 = 132, len = 60 - i * 8;   /* longer reeds sound lower */
        uint32_t el = fm1_ms - rd.sound_t[i];
        int sounding = rd.sound_t[i] && el < 1600u;
        int wob = sounding ? (mo_sin(el * 7000u) * (int32_t)(1600u - el) / 1600) >> 14 : 0;
        uint16_t rc = rd.off[i] == 0 && rd.verdict == 1 ? C_VERD : C_BRASS;
        /* the strobe: a ring of bars that turns with the error while the reed sounds; still when true */
        {
            uint32_t spin = sounding ? (uint32_t)(rd.off[i] * (int32_t)el * 3) : 0u;
            g_disc(x, 100, 15, RGB(8, 20, 32));
            g_ring(x, 100, 13, 15, C_CYAN_D);
            for (k = 0; k < 10; k++) {
                uint32_t a = spin + (uint32_t)k * 6554u;
                g_thick16(px16(x, 6 * 16, a) + 8, py16(100, 6 * 16, a) + 8, px16(x, 12 * 16, a) + 8, py16(100, 12 * 16, a) + 8, 26,
                          sounding ? (rd.off[i] == 0 ? C_VERD : C_CYAN) : C_CYAN_D);
            }
        }
        /* (no needle: the strobe's spin and the throb you hear are the only guides) */
        /* the reed: a brass tongue in its slot, the tuning screw on its root */
        g_rrect(x - 7, y0 - 4, 14, len + 10, 4, RGB(10, 6, 24));
        g_thick(x, y0, x + wob, y0 + len, 6, rc);
        g_thick(x - 1, y0, x - 1 + wob, y0 + len, 1, C_BRASS_L);
        if (sounding) g_led(x + wob, y0 + len, 2, C_GLOW);
        g_screw(x, y0 - 8, 6, rd.screw[i]);
        {
            char t[2] = {(char)('1' + i), 0};
            g_text_c(x, 194, &FONT_S, t, C_CYAN);
        }
    }
    if (rd.verdict && fm1_ms - rd.verdict_t < 1800u && !won) {
        g_rrect(40, 198, 160, 19, 5, C_PANEL2);
        g_text_c(120, 200, &FONT_S, rd.verdict > 0 ? T_RD_OK : T_RD_REFUSED, rd.verdict > 0 ? C_VERD : C_ROSE);
    }
    if (won) {
        uint32_t el = fm1_ms - rd.fin_t;
        if (el < 3000u && cur_ch->done && cur_ch->done[0]) { g_rrect(40, 198, 160, 19, 5, C_PANEL2); g_text_c(120, 200, &FONT_S, cur_ch->done, C_CYAN); }
    }
    for (i = 0; i < 4; i++) {                   /* key LEDs: the reed keys glow, a sounding reed lights its key */
        fm1_led_key_glow(KEY_LED(RD_KEY[i]), 1);
        {   /* the key pulses with the beating while the reed sounds: slower as it nears, steady when true */
            uint32_t el = fm1_ms - rd.sound_t[i], per = 120u + (uint32_t)iabs(rd.off[i]) * 3u;
            int on = rd.sound_t[i] && el < 1600u && (rd.off[i] == 0 || (el % per) < per / 2u);
            fm1_led_key(KEY_LED(RD_KEY[i]), on);
        }
    }
}

static void rd_leave(void) { kb_hook = 0; au_cut(6); ui_keys_clear(); }
static const game_t G_REEDS = {BIT(BTN_EDIT) | BIT(BTN_PLAY) | BIT(BTN_REC), 2000,
                               rd_enter, rd_update, rd_draw, rd_leave};
