/* SPDX-License-Identifier: GPL-3.0-only */
/* Chapter XII: THE MASTERWORK. Bring the dancing automaton to life: tempo, steps, song.
 *
 *   TEMPO  the governor's flyballs set the beat (K1). The score's tempo ticks on the left, the governor's on the
 *          right: turn until they fall together and stay together (listening; the pendulum helps the eye).
 *   STEPS  the dance is drawn on the score as footprints: left, right, a turn. Transcribe it onto the cam drum:
 *          K1 picks a step, C sets a left peg, E a right, G a turn.
 *   SONG   the comb must know the waltz from the little music box (the title has been playing it). Play it on
 *          the keys C..C, from memory or by ear (REC plays it). A wrong note starts the phrase again.
 * Then the couple dances. Tempo and steps are drawn each time. */

#define MW_SONG mw.song                         /* puzzles.txt song (tines 1..8), tempo, turns */
#define MW_STEPS 8
static struct {
    int32_t stage;                              /* 0 tempo, 1 steps, 2 song, 3 done */
    int32_t bpm, tgt;
    uint32_t r_next, g_next, r_k, g_k;          /* scheduled beats (frame times) */
    uint32_t match_since;
    uint8_t pat[MW_STEPS], cam[MW_STEPS];       /* bit 0 left, 1 right, 2 turn */
    int32_t sel;
    int32_t got;                                /* song notes played right */
    uint32_t press_t[8], stage_t, fin_t;
    uint8_t song[24]; int32_t song_n;
} mw;

static uint32_t mw_beat(int32_t bpm) { return ms_frames(60000u / (uint32_t)bpm); }
static void mw_key(uint32_t k)
{
    uint32_t c;
    if (mw.stage != 2 && mw.stage != 1) return;
    for (c = 0; c < 8u; c++) if (MB_KEY[c] == k) {
        if (mw.stage == 2) sfx_tine(MB_NOTE[c - 0u], 100, 0, (uint8_t)(70u + c * 16u), 7);
        else sfx_click(0, 60, 128);
        mw.press_t[c] = fm1_ms;
    }
}
static void mw_enter(void)
{
    const mo_list_t *tl = LST(L_MASTER_TEMPO), *sl = LST(L_MASTER_SONG);
    int32_t turns = PRM(P_MASTER_TURNS), t2;
    uint32_t i, side = rndn(2);
    for (i = 0; i < sl->n && i < 24u; i++) mw.song[i] = (uint8_t)sl->v[i];
    mw.song_n = (int32_t)i;
    mw.stage = 0; mw.tgt = tl->v[rndn(tl->n)];
    do mw.bpm = 60 + 4 * (int32_t)rndn(21); while (iabs(mw.bpm - mw.tgt) < 16);
    mw.r_next = mw.g_next = au_time() + ms_frames(400); mw.r_k = mw.g_k = 0;
    mw.match_since = 0;
    for (i = 0; i < MW_STEPS; i++) {            /* feet mostly alternate; two turns somewhere */
        if (rndn(4)) side ^= 1u;
        mw.pat[i] = (uint8_t)(1u << side);
        mw.cam[i] = 0;
    }
    for (t2 = 0; t2 < turns; t2++) mw.pat[rndn(MW_STEPS)] |= 4u;   /* turns, somewhere in the dance */
    mw.sel = 0; mw.got = 0; mw.fin_t = 0; mw.stage_t = fm1_ms;
    kb_hook = mw_key;
#ifdef MW_TEST_STAGE
    mw.stage = MW_TEST_STAGE;                   /* (host test builds: start at a later stage) */
#endif
}
static void mw_next_stage(void)
{
    mw.stage++; mw.stage_t = fm1_ms;
    sfx_flourish(67, au_time() + ms_frames(100));
    if (mw.stage == 2) {                        /* the song: first, hear it */
        uint32_t i, t = au_time() + ms_frames(900);
        for (i = 0; i < (uint32_t)mw.song_n; i++) sfx_tine_worn(MB_NOTE[MW_SONG[i] - 1u], t + ms_frames(i * 300u), 7);
    }
}
static void mw_tick_schedule(void)
{
    uint32_t now = au_time(), hz = now + ms_frames(150);
    if ((int32_t)(now - mw.r_next) > (int32_t)ms_frames(1000)) mw.r_next = mw.g_next = now + ms_frames(100);
    while ((int32_t)(mw.r_next - hz) < 0) { sfx_tick(mw.r_next, (mw.r_k % 3u) ? 77 : 82, 60, 50, 7); mw.r_next += mw_beat(mw.tgt); mw.r_k++; }
    while ((int32_t)(mw.g_next - hz) < 0) { sfx_tick(mw.g_next, (mw.g_k % 3u) ? 86 : 91, 70, 206, 7); mw.g_next += mw_beat(mw.bpm); mw.g_k++; }
}
static int mw_update(void)
{
    uint32_t k, c;
    led_btn_mask = BIT(BTN_EDIT) | (mw.stage == 2 ? BIT(BTN_REC) : 0u);
    if (mw.stage == 0) {
        int32_t e = enc_main();
        if (e) {
            mw.bpm = clampi(mw.bpm + e * 4, 56, 144);
            mw.g_next = mw.r_next;              /* the governor restarts with the score's next beat: compare from there */
            mw.g_k = mw.r_k;
            sfx_click(0, 30, 206);
        }
        mw_tick_schedule();
        if (mw.bpm == mw.tgt) { if (!mw.match_since) mw.match_since = fm1_ms; if (fm1_ms - mw.match_since > 2500u) mw_next_stage(); }
        else mw.match_since = 0;
    } else if (mw.stage == 1) {
        int32_t e = enc_main(), all = 1;
        if (e) { mw.sel = ((mw.sel + e) % MW_STEPS + MW_STEPS) % MW_STEPS; sfx_tick(0, 96, 25, 128, 0); }
        for (k = 0; k < 27u; k++) if (in.kdown >> k & 1u) {
            if (k == 7u) mw.cam[mw.sel] ^= 1u;             /* C: left */
            else if (k == 11u) mw.cam[mw.sel] ^= 2u;       /* E: right */
            else if (k == 14u) mw.cam[mw.sel] ^= 4u;       /* G: turn */
        }
        for (k = 0; k < MW_STEPS; k++) if (mw.cam[k] != mw.pat[k]) all = 0;
        if (all && fm1_ms - mw.stage_t > 300u) mw_next_stage();
    } else if (mw.stage == 2) {
        if (PRESSED(BTN_REC)) {
            uint32_t i, t = au_time() + ms_frames(100);
            au_cut(7);
            for (i = 0; i < (uint32_t)mw.song_n; i++) sfx_tine_worn(MB_NOTE[MW_SONG[i] - 1u], t + ms_frames(i * 300u), 7);
        }
        for (k = 0; k < 27u; k++) if (in.kdown >> k & 1u) {
            for (c = 0; c < 8u; c++) if (MB_KEY[c] == k) break;
            if (c == 8u) continue;
            if (MW_SONG[mw.got] == c + 1u) { if (++mw.got == mw.song_n) return 1; }
            else { mw.got = 0; sfx_buzz(au_time() + ms_frames(150)); }
        }
    }
    return 0;
}

static void mw_dancer(int32_t x, int32_t y, uint32_t ph, int lady, int32_t step)
{
    int32_t sw = (mo_sin(ph) * 8) >> 15, lift = step ? 4 : 0;
    uint16_t c = lady ? RGB(200, 70, 80) : C_STEEL_L;
    g_disc(x, y - 30, 6, C_GOLD);
    g_thick(x, y - 24, x, y, 4, c);
    g_thick(x, y - 18, x + (lady ? -14 : 14), y - 24 + sw, 2, c);
    g_thick(x, y - 18, x + (lady ? 10 : -10), y - 8 - sw, 2, c);
    g_thick(x, y, x - 7 + sw, y + 24 - (step == 1 ? lift : 0), 3, c);
    g_thick(x, y, x + 7 - sw, y + 24 - (step == 2 ? lift : 0), 3, c);
    if (lady) {
        int32_t q[8] = {(x - 4) * 16, (y - 4) * 16, (x + 4) * 16, (y - 4) * 16, (x + 15 + sw) * 16, (y + 14) * 16, (x - 15 + sw) * 16, (y + 14) * 16};
        g_quad16(q, c);
    }
}
static void mw_foot(int32_t x, int32_t y, int right, uint16_t c)   /* a footprint */
{
    g_disc(x, y, 4, c);                         /* the ball of the foot, then the heel */
    g_disc(x + (right ? 1 : -1), y + 9, 3, c);
    g_disc(x + (right ? 3 : -3), y - 5, 1, c);  /* a toe */
}
static void mw_draw(uint32_t won)
{
    int32_t i;
    uint32_t now = au_time();
    g_bg(BG_ART_HEART);                         /* the heart of the line, under Shinmachi (the story's strip), dimmed */
    g_dim(0, 22, SW, 118, 110);
    /* the three stages, as lamps */
    for (i = 0; i < 3; i++) {
        static const char *const N[3] = {T_MW_TEMPO, T_MW_STEPS, T_MW_SONG};
        int32_t x = 40 + i * 80;
        if (i < mw.stage || won) g_led(x - 22, 32, 3, C_VERD);
        else g_disc(x - 22, 32, 3, i == mw.stage ? C_AMBER : C_PAPER3);
        g_deco(x - 14, 27, 9, N[i], i == mw.stage && !won ? C_GOLD : C_PAPER3);
    }
    g_panel(6, 44, 228, 176, C_GOLD, 0);
    if (won) {                                  /* the finale: the couple dances to the waltz, at the set tempo */
        uint32_t el, beat;
        if (!mw.fin_t) {
            uint32_t t = au_time() + ms_frames(300), k, b = mw_beat(mw.tgt);
            mw.fin_t = fm1_ms;
            au_cut(7);
            for (k = 0; k < 2u * (uint32_t)mw.song_n; k++) sfx_tine(MB_NOTE[MW_SONG[k % (uint32_t)mw.song_n] - 1u], 100, t + b * k, 128, 7);
        }
        el = fm1_ms - mw.fin_t;
        beat = el * (uint32_t)mw.tgt / 60000u;
        g_clip_y0 = 48; g_clip_y1 = 216;
        g_rays(120, 220, 200, 32, el * 4u, RGB(60, 42, 18));
        g_clip_y0 = 0; g_clip_y1 = SH;
        {
            uint32_t a = el * 40u;
            int32_t st = mw.pat[beat % MW_STEPS];
            mw_dancer(120 + ((mo_cos(a) * 30) >> 15), 140 + ((mo_sin(a) * 8) >> 15), el * 90u, 0, st & 3);
            mw_dancer(120 - ((mo_cos(a) * 30) >> 15), 140 - ((mo_sin(a) * 8) >> 15), el * 90u + 20000u, 1, (st & 3) ^ 3);
        }
        for (i = 0; i < 8; i++) {
            uint32_t e2 = (el + (uint32_t)i * 420u) % 3000u;
            g_led(24 + i * 27, 200 - (int32_t)e2 * 140 / 3000, 2, C_GLOW);
        }
        return;
    }
    if (mw.stage == 0) {
        /* the governor: a spindle, two flyballs flung out by speed, turning; the score's pendulum beside */
        int32_t cx = 80, top = 70, spread = 8 + (mw.bpm - 56) * 30 / 88;
        uint32_t a = now * (uint32_t)mw.bpm / 20u;
        int32_t s = (mo_sin(a) * spread) >> 15;
        g_rect(cx - 1, top, 3, 110, C_STEEL_L);
        g_rect(cx - 16, 180, 33, 6, C_BRASS_D);
        g_thick(cx, top + 8, cx + s, top + 40, 2, C_BRASS);
        g_thick(cx, top + 8, cx - s, top + 40, 2, C_BRASS);
        g_disc(cx + s, top + 40, 7, C_GOLD); g_disc(cx - s, top + 40, 7, C_GOLD);
        g_led(cx + s - 2, top + 38, 1, C_GLOW);
        {
            int32_t px = 170, py = 66;
            uint32_t b2 = 2u * mw_beat(mw.tgt);   /* a swing each way per two beats, in step with the score's ticks */
            uint32_t phase = ((now - mw.r_next + 16u * b2) % b2) * 256u / b2 * 256u;
            int32_t ang = (mo_sin(phase) * 5000) >> 15;
            g_rect(px - 14, 160, 28, 20, RGB(70, 44, 26));
            g_hand(px, py, 74, 0, (uint32_t)(16384 + ang), 2, C_ROSE);
            g_disc(px + ((74 * mo_cos((uint32_t)(16384 + ang))) >> 15), py + ((74 * mo_sin((uint32_t)(16384 + ang))) >> 15), 5, C_ROSE);
        }
        {
            char t[12], *p = t;
            fmt_u(p, (uint32_t)mw.bpm); while (*p) p++;
            str_cat(p, " " T_MW_BPM);
            g_text_c(cx, 190, &FONT_S, t, C_CYAN);
            g_text_c(170, 190, &FONT_S, T_MW_SCORE, C_ROSE);
        }
        if (mw.match_since) g_led(120, 120, 3, C_VERD);
    } else if (mw.stage == 1) {
        /* the score's footprints (above), each column straight above its column on the cam (below): copy them down */
        for (i = 0; i < MW_STEPS; i++) {
            int32_t x = 28 + i * 26, ok = mw.cam[i] == mw.pat[i];
            if (i == mw.sel) { g_rect(x - 12, 50, 24, 136, RGB(40, 34, 20)); g_frame(x - 12, 50, 24, 136, C_GLOW); }
            if (mw.pat[i] & 1u) mw_foot(x - 5, 60, 0, C_CREAM);
            if (mw.pat[i] & 2u) mw_foot(x + 5, 60, 1, C_CREAM);
            if (mw.pat[i] & 4u) { g_ring(x, 86, 5, 6, C_AMBER); g_tri16((x + 5) * 16, 82 * 16, (x + 9) * 16, 86 * 16, (x + 3) * 16, 87 * 16, C_AMBER); }
            if (ok) g_led(x, 104, 2, C_VERD); else g_disc(x, 104, 2, C_RUBY);   /* this column copied right? */
        }
        g_rect(14, 112, 212, 70, RGB(120, 92, 48));
        for (i = 0; i < 3; i++) g_rect(14, 124 + i * 20, 212, 1, RGB(90, 66, 34));
        g_text(16, 116, &FONT_S, "L", C_CYAN); g_text(16, 136, &FONT_S, "R", C_ROSE); g_text(16, 156, &FONT_S, "T", C_AMBER);
        for (i = 0; i < MW_STEPS; i++) {
            int32_t x = 28 + i * 26, l;
            for (l = 0; l < 3; l++)
                if (mw.cam[i] >> l & 1u) { static const uint16_t PC[3] = {C_CYAN, C_ROSE, C_AMBER}; g_disc(x, 124 + l * 20, 4, PC[l]); g_disc(x - 1, 123 + l * 20, 1, C_CREAM); }
                else g_disc(x, 124 + l * 20, 1, RGB(90, 66, 34));
        }
        g_text_c(120, 188, &FONT_S, T_MW_CAM_KEYS, C_CREAM);
    } else {
        /* the comb and the phrase: thirteen notes, lit as they are played right */
        for (i = 0; i < mw.song_n; i++) g_led(120 - (mw.song_n - 1) * clampi(200 / mw.song_n, 6, 15) / 2 + i * clampi(200 / mw.song_n, 6, 15), 70, 3, i < mw.got ? C_VERD : RGB(50, 44, 40));
        g_rect(30, 150, 180, 12, C_STEEL_D);
        for (i = 0; i < 8; i++) {
            int32_t x = 38 + i * 23;
            uint32_t el = fm1_ms - mw.press_t[i];
            int32_t wob = el < 400u ? (mo_sin(el * 9000u) * (int32_t)(400u - el) / 400) >> 14 : 0;
            g_thick(x, 150, x + wob, 100, 6, el < 300u ? C_STEEL_L : C_STEEL);
            g_text_c(x, 166, &FONT_S, MB_NAME[i], C_CREAM);
        }
        g_text_c(120, 190, &FONT_S, T_MW_WALTZ, C_CREAM);
    }
    for (i = 0; i < 8; i++) {                   /* key LEDs */
        int peg = mw.stage == 1 && ((i == 0 && (mw.cam[mw.sel] & 1u)) || (i == 2 && (mw.cam[mw.sel] & 2u)) || (i == 4 && (mw.cam[mw.sel] & 4u)));
        fm1_led_key_glow(KEY_LED(MB_KEY[i]), mw.stage == 2 || (mw.stage == 1 && (i == 0 || i == 2 || i == 4)));
        fm1_led_key(KEY_LED(MB_KEY[i]), fm1_ms - mw.press_t[i] < 160u || peg);   /* (steps: the picked column's pegs) */
    }
}
static void mw_leave(void) { kb_hook = 0; au_cut(7); ui_keys_clear(); }
static const game_t G_MASTER = {BIT(BTN_EDIT), 3600,
                                mw_enter, mw_update, mw_draw, mw_leave};

/* the dance: a sequence's finale (the masterwork). No play of its own: the couple dances to the Lantern Waltz at
 * a waltz tempo, the steps drawn fresh. */
static void dn_enter(void)
{
    static const uint8_t WALTZ[13] = {3, 5, 8, 5, 4, 6, 5, 3, 5, 7, 6, 5, 8};
    uint32_t i, side = rndn(2);
    for (i = 0; i < 13u; i++) mw.song[i] = WALTZ[i];
    mw.song_n = 13; mw.tgt = 96; mw.stage = 3; mw.fin_t = 0;
    for (i = 0; i < MW_STEPS; i++) { if (rndn(4)) side ^= 1u; mw.pat[i] = (uint8_t)(1u << side); }
}
static int dn_update(void) { return 1; }
static void dn_draw(uint32_t won) { mw_draw(won ? won : 1u); }
static void dn_leave(void) { au_cut(7); ui_keys_clear(); }
static const game_t G_DANCE = {0, 3600, dn_enter, dn_update, dn_draw, dn_leave};
