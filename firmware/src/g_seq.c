/* SPDX-License-Identifier: GPL-3.0-only */
/* THE RAILWAY SEQUENCER: Union's junction timetable, written as a step sequencer.
 *
 * A star of lines meets at one diamond in the middle. Each train runs out from its own platform, through the
 * diamond to the far platform, and back through it home: four moves a loop of sixteen steps, so it repeats.
 * The sixteen white keys are the steps; K1 (or SELECT) picks the train, a white key sets or clears a move for
 * it on that step, SEL clears its row. The timetable plays round and round: every move sounds the train's
 * note (into the diamond its root, out to the far side its fifth, home its octave). Two trains in the diamond
 * at once is a conflict, and so is a train that leaves the moment it arrives (it must stand a step). A loop with
 * every train home and nothing in the way, and the junction plays its piece: the next timetable, a train more. */

#define SQ_ST 16u
#define SQ_TM 4u
#define SQ_STEP_MS 220u
static const uint8_t SQ_NOTE[SQ_TM] = {62, 65, 69, 72};   /* D F A C: the junction's chord */
static const uint16_t SQ_COL[SQ_TM] = {C_CYAN, C_ROSE, C_AMBER, C_VERD};
static const uint8_t SQ_ARM[SQ_TM] = {0, 2, 1, 3};      /* each train's home arm (of 8: its far one is +4) */

static struct {
    uint32_t n, rounds, round, sel, won, step, step_t, clean, fin_t, msg_t;
    uint16_t pat[SQ_TM];                        /* bit s: the train moves on step s */
    uint16_t bad;                               /* bit s: a conflict on step s */
    uint8_t pos[SQ_TM];                         /* where each train is now: 0 home, 1 diamond, 2 far, 3 diamond */
    uint8_t badt;                               /* bit t: train t in a conflict or not home */
    uint8_t why, why_t, why_s;                  /* the first problem: 1 moves short or over, 2 two moves in a row, 3 two in the diamond; its train, its step */
    const char *msg;
} sq;

static uint32_t sq_moves(uint32_t t) { uint32_t c = 0, p = sq.pat[t]; while (p) { c += p & 1u; p >>= 1; } return c; }
static uint32_t sq_check(void)                  /* the whole loop: conflicts (sq.bad, sq.badt); 1 if it all works */
{
    uint32_t s, t, ok = 1;
    uint8_t p[SQ_TM] = {0, 0, 0, 0};
    sq.bad = 0; sq.badt = 0; sq.why = 0;
    for (s = 0; s < SQ_ST; s++) {               /* two in the diamond at once */
        uint32_t in = 0, who = 0;
        for (t = 0; t < sq.n; t++) {
            if (sq.pat[t] >> s & 1u) p[t] = (uint8_t)((p[t] + 1u) & 3u);
            if (p[t] & 1u) { in++; who |= 1u << t; }
        }
        if (in > 1u) { sq.bad |= (uint16_t)(1u << s); sq.badt |= (uint8_t)who; ok = 0; if (!sq.why) { sq.why = 3; sq.why_s = (uint8_t)s; } }
    }
    for (t = 0; t < sq.n; t++) {
        uint16_t pt = sq.pat[t];
        if (pt & (uint16_t)((pt << 1) | (pt >> 15))) {   /* two moves in a row (round the loop too): it never stood */
            for (s = 0; s < SQ_ST; s++) if ((pt >> s & 1u) && (pt >> ((s + SQ_ST - 1u) % SQ_ST) & 1u)) { sq.bad |= (uint16_t)(1u << s); if (sq.why != 3u && sq.why != 2u) { sq.why = 2; sq.why_t = (uint8_t)t; sq.why_s = (uint8_t)s; } }
            sq.badt |= (uint8_t)(1u << t); ok = 0;
        }
    }
    for (t = 0; t < sq.n; t++)
        if (sq_moves(t) != 4u) { sq.badt |= (uint8_t)(1u << t); ok = 0; if (!sq.why) { sq.why = 1; sq.why_t = (uint8_t)t; } }
    return ok && !sq.bad;
}
static void sq_new_round(void)
{
    uint32_t t;
    sq.n = (uint32_t)PRM(P_SEQUENCER_TRAINS) + sq.round; if (sq.n > 3u) sq.n = 3u;   /* (four would need the diamond 16 steps of 16: no room) */
    for (t = 0; t < SQ_TM; t++) { sq.pat[t] = 0; sq.pos[t] = 0; }
    sq.sel = 0; sq.clean = 0; sq.step = SQ_ST - 1u; sq.step_t = fm1_ms; sq.fin_t = 0;
    sq_check();
}
static void sq_enter(void)
{
    sq.rounds = (uint32_t)PRM(P_SEQUENCER_ROUNDS);
    sq.round = 0; sq.won = 0; sq.msg = 0;
    sq_new_round();
}
static int sq_update(void)
{
    uint32_t k, t, i;
    int32_t e = enc_main() + in.enc[ENC_ALGORITHM];
    led_btn_mask = BIT(BTN_EDIT) | BIT(BTN_SEL);
    if (sq.won) return 1;
    if (e) { sq.sel = (uint32_t)(((int32_t)sq.sel + (e > 0 ? 1 : (int32_t)sq.n - 1)) % (int32_t)sq.n); sfx_click(0, 50, 128); }
    if (PRESSED(BTN_SEL) && !sq.fin_t) { sq.pat[sq.sel] = 0; sq.clean = 0; sq_check(); sfx_thunk(0, 40, 60, 128); }
    for (i = 0; i < 16u && !sq.fin_t; i++)       /* the white keys: the steps */
        if (in.kdown >> WHITE_KEY[i] & 1u) { sq.pat[sq.sel] ^= (uint16_t)(1u << i); sq.clean = 0; sq_check(); sfx_tick(0, 70, 50, 128, 0); }
    while (fm1_ms - sq.step_t >= SQ_STEP_MS) {  /* the timetable runs: a step, its moves, their notes */
        uint32_t at = au_time();
        sq.step_t += SQ_STEP_MS;
        sq.step = (sq.step + 1u) % SQ_ST;
        if (sq.step == 0u) {
            for (t = 0; t < SQ_TM; t++) sq.pos[t] = 0;
            if (!sq.fin_t) {                    /* a loop begins: clean, and once it has played through clean, the piece */
                if (!sq_check()) sq.clean = 0;
                else if (++sq.clean >= 2u) { sq.fin_t = fm1_ms; sfx_flourish(62, at + ms_frames(40)); }
            }
            sfx_thunk(at, 31, 45, 128);         /* the loop's downbeat */
        }
        if (!(sq.step & 3u)) sfx_tick(at, 90, 14, 128, 0);
        for (t = 0; t < sq.n; t++) if (sq.pat[t] >> sq.step & 1u) {
            sq.pos[t] = (uint8_t)((sq.pos[t] + 1u) & 3u);
            sfx_chime(SQ_NOTE[t] + (sq.pos[t] & 1u ? 0 : sq.pos[t] == 2u ? 7 : 12), (sq.bad >> sq.step & 1u) ? 40u : 78u, at, (uint8_t)(60u + t * 45u));
        }
        if ((sq.bad >> sq.step & 1u) && !sq.fin_t) sfx_tick(at, 40, 60, 128, 0);
    }
    if (sq.fin_t && fm1_ms - sq.fin_t > 3600u) {
        if (++sq.round >= sq.rounds) { sq.won = 1; return 1; }
        sq_new_round();
    }
    for (k = 0; k < 27u; k++) {                 /* the keys: the selected train's moves lit, the step running past */
        uint32_t st = 16;
        for (i = 0; i < 16u; i++) if (WHITE_KEY[i] == k) st = i;
        fm1_led_key_glow(KEY_LED(k), st < 16u);
        fm1_led_key(KEY_LED(k), st < 16u && ((sq.pat[sq.sel] >> st & 1u) || st == sq.step));
    }
    return 0;
}
#define SQ_CX 120
#define SQ_CY 72
static void sq_arm_xy(uint32_t arm, int32_t r, int32_t *x, int32_t *y)
{
    uint32_t a = arm * 8192u;
    *x = SQ_CX + (mo_sin(a) * r >> 15) * 3 / 2;
    *y = SQ_CY - (mo_cos(a) * r >> 15);
}
static void sq_draw(uint32_t won)
{
    uint32_t t, s;
    int32_t x, y, x2, y2, gy = 132;
    char b[40], *p;
    static const char SECN[4] = {'H', 'D', 'F', 'D'};
    g_rect(0, STAGE_Y0, SW, STAGE_Y1 - STAGE_Y0, RGB(8, 6, 22));
    p = str_cat(b, T_SQ_TABLE " "); fmt_u(p, sq.round < sq.rounds ? sq.round + 1u : sq.rounds); while (*p) p++; *p++ = '/'; fmt_u(p, sq.rounds);
    g_text(8, STAGE_Y0 + 2, &FONT_S, b, C_INK2);
    for (t = 0; t < sq.n; t++) {                /* the lines: each train's home (H) and far end (F), through the diamond (D) */
        uint16_t c = mix565(RGB(8, 6, 22), SQ_COL[t], t == sq.sel ? 220 : 80);
        sq_arm_xy(SQ_ARM[t], 40, &x, &y); sq_arm_xy(SQ_ARM[t] + 4u, 40, &x2, &y2);
        g_thick(x, y, x2, y2, t == sq.sel ? 2 : 1, c);
        g_rrect(x - 7, y - 6, 14, 12, 3, c); g_rrect(x2 - 7, y2 - 6, 14, 12, 3, c);
        if (t == sq.sel) { g_text_c(x, y - 7, &FONT_S, "H", C_NIGHT); g_text_c(x2, y2 - 7, &FONT_S, "F", C_NIGHT); }
    }
    {
        uint32_t in = 0;
        for (t = 0; t < sq.n; t++) if (sq.pos[t] & 1u) in++;
        g_ring(SQ_CX, SQ_CY, 7, 9, in > 1u ? C_RUBY : C_CREAM);
        g_text(SQ_CX + 12, SQ_CY - 18, &FONT_S, "D", C_INK2);
    }
    for (t = 0; t < sq.n; t++) {                /* the trains, where they are on this step */
        int32_t r = sq.pos[t] & 1u ? 0 : 40;
        sq_arm_xy(SQ_ARM[t] + (sq.pos[t] == 2u ? 4u : 0u), r, &x, &y);
        if (r == 0) { x += (int32_t)t * 4 - 6; }
        g_disc(x, y, 5, SQ_COL[t]);
    }
    /* the timetable: a row a train, a column a step; each move shows where it goes (D F D H), the diamond's stays a bar */
    for (s = 0; s < SQ_ST; s += 4u) { p = b; fmt_u(p, s + 1u); g_text(46 + (int32_t)s * 12, gy - 16, &FONT_S, b, C_PAPER3); }
    for (t = 0; t < sq.n; t++) {
        int32_t ry = gy + (int32_t)t * 15;
        uint32_t mv = sq_moves(t), pos = 0;
        p = b; fmt_u(p, mv); while (*p) p++; str_cat(p, "/4");
        g_disc(8, ry + 6, 4, SQ_COL[t]);
        g_text(15, ry, &FONT_S, b, mv == 4u && !(sq.badt >> t & 1u) ? C_VERD : mv == 4u ? C_ROSE : C_INK2);
        if (t == sq.sel) g_rframe(44, ry - 1, 194, 14, 3, 1, SQ_COL[t]);
        for (s = 0; s < SQ_ST; s++) {
            int32_t cx = 46 + (int32_t)s * 12;
            int mv_here = (sq.pat[t] >> s & 1u) != 0u;
            uint16_t c;
            if (mv_here) pos = (pos + 1u) & 3u;
            c = mv_here ? SQ_COL[t] : (pos & 1u) ? mix565(RGB(8, 6, 22), SQ_COL[t], 90) : (s & 3u) ? RGB(26, 20, 52) : RGB(40, 30, 70);
            g_rect(cx, ry + 1, 10, 10, c);
            if (mv_here) { char nm[2] = {SECN[pos], 0}; g_text_c(cx + 5, ry - 1, &FONT_S, nm, C_NIGHT); }
            if ((sq.bad >> s & 1u) && (sq.badt >> t & 1u) && (mv_here || (pos & 1u))) g_frame(cx - 1, ry, 12, 12, C_RUBY);
        }
    }
    g_rect(46 + (int32_t)sq.step * 12, gy - 3, 10, 2, C_CREAM);
    /* what is wrong (the first thing), or how it stands */
    p = b;
    if (sq.fin_t) str_cat(b, T_SQ_PIECE);
    else if (sq.why == 3u) { p = str_cat(p, "STEP "); fmt_u(p, sq.why_s + 1u); while (*p) p++; str_cat(p, ": " T_SQ_BOTH); }
    else if (sq.why == 2u) { p = str_cat(p, "TRAIN "); fmt_u(p, sq.why_t + 1u); while (*p) p++; str_cat(p, " " T_SQ_TWICE); }
    else if (sq.why == 1u) {
        uint32_t mv = sq_moves(sq.why_t);
        p = str_cat(p, "TRAIN "); fmt_u(p, sq.why_t + 1u); while (*p) p++;
        if (mv < 4u) { p = str_cat(p, ": " T_SQ_MORE " "); fmt_u(p, 4u - mv); } else str_cat(p, ": 4 MOVES, NOT MORE");
    } else str_cat(b, sq.clean ? T_SQ_ONCE : T_SQ_FOUR);
    g_text_c(SW / 2, STAGE_Y1 - 30, &FONT_S, b, sq.fin_t || (!sq.why && sq.clean) ? C_VERD : sq.why >= 2u ? C_ROSE : C_INK2);
    g_text_c(SW / 2, STAGE_Y1 - 15, &FONT_S, T_SQ_ROUTE, C_PAPER3);
    (void)won;
}
static void sq_leave(void) { ui_keys_clear(); }
static const game_t G_SEQ = {BIT(BTN_EDIT) | BIT(BTN_SEL), 2000, sq_enter, sq_update, sq_draw, sq_leave};
