/* SPDX-License-Identifier: GPL-3.0-only */
/* THE TELEPHONE EXCHANGE: the old switchboard has cut the stations off from one another.
 *
 * Six incoming lines (white keys C D E F G A: jacks 1..6, the top row) and six stations' outgoing lines (black
 * keys C# D# F# G# A# C#: jacks A..F, the bottom row). Each incoming line carries a station's call sign: a
 * rhythm and a little tune, crackling down the wire. A white key alone listens to its line; a black key alone
 * plays that station's call sign from the directory, clean. Hold one of each and a patch cable joins them (the
 * same pair again pulls it out; a jack takes one cable). PLAY tests the board: how many lines come through.
 * The second board's lines are worse: only the rhythm survives the wire, so it is matched by its beat alone. */

#define EX_N 6
static const uint8_t EX_IN_KEY[EX_N] = {7, 9, 11, 12, 14, 16};     /* C4 D4 E4 F4 G4 A4 */
static const uint8_t EX_OUT_KEY[EX_N] = {8, 10, 13, 15, 17, 20};   /* C#4 D#4 F#4 G#4 A#4 C#5 */
static const char *const EX_CODE[8] = {"KUR", "MER", "PAC", "CHR", "LIB", "OLY", "AVA", "UNI"};
static const uint8_t EX_RHY[8] = {0x49, 0x33, 0x51, 0x0D, 0x07, 0xC1, 0x55, 0x19};   /* bit i: a pulse on step i of 8 */
static const uint8_t EX_BASE[8] = {60, 62, 64, 67, 69, 72, 74, 76};
static const int8_t EX_SHAPE[4][3] = {{0, 4, 7}, {7, 4, 0}, {0, 7, 12}, {0, -3, 4}};
static const uint16_t EX_CABLE[EX_N] = {C_ROSE, C_CYAN, C_AMBER, RGB(170, 120, 230), C_VERD, C_GOLD};
#define EX_STEP_MS 140u

static struct {
    uint32_t n, rounds, round, won, msg_t, play_t, play_k, through;
    uint8_t st[EX_N];                           /* each output jack: which station (index into EX_CODE) */
    uint8_t perm[EX_N];                         /* each incoming line: which output jack's station it carries */
    uint8_t rhy[EX_N], base[EX_N], shape[EX_N]; /* each output's call sign */
    int8_t conn[EX_N];                          /* each incoming line: the output it is patched to (-1 none) */
    int32_t held_in, held_out;
    const char *msg;
    uint32_t fin_t;
} ex;

static void ex_sound(uint32_t out, int line)    /* a call sign: down the wire (crackle, muffled) or from the directory */
{
    uint32_t s, h = 0, t0 = au_time() + ms_frames(30);
    au_cut(10);
    for (s = 0; s < 8u; s++) {
        if (!(ex.rhy[out] >> s & 1u)) continue;
        {
            int32_t note = (int32_t)ex.base[out] + EX_SHAPE[ex.shape[out]][h % 3u];
            uint32_t at = t0 + ms_frames(s * EX_STEP_MS);
            if (line) {
                au_ev_t e = {0};
                if (ex.round > 0u) note = 69;   /* (the worse board: one flat tone, the beat is all that is left) */
                e.at = at; e.inc1 = note_inc(note); e.inc2 = note_inc(note + 12);
                e.a1 = 5200; e.a2 = 1800; e.an = 1400; e.d1 = 120; e.d2 = 70; e.dn = 50; e.lp = 5000; e.pan = 128; e.tag = 10;
                au_post(&e);
            } else {
                au_ev_t e = {0};
                e.at = at; e.inc1 = note_inc(note); e.inc2 = inc_ratio(e.inc1, 257);
                e.a1 = 2700; e.a2 = 2700; e.d1 = 260; e.d2 = 260; e.pan = 128; e.tag = 10; e.wave = 1; e.lp = 9000;
                au_post(&e);
            }
            h++;
        }
    }
}
static void ex_new_round(void)
{
    uint32_t i, j, t;
    uint8_t pool[8];
    for (i = 0; i < 8u; i++) pool[i] = (uint8_t)i;
    for (i = 8u; i-- > 1u;) { j = rndn(i + 1u); t = pool[i]; pool[i] = pool[j]; pool[j] = (uint8_t)t; }
    for (i = 0; i < ex.n; i++) ex.st[i] = pool[i];
    for (i = 0; i < 8u; i++) pool[i] = (uint8_t)i;           /* the call signs: rhythms and pitches all different */
    for (i = 8u; i-- > 1u;) { j = rndn(i + 1u); t = pool[i]; pool[i] = pool[j]; pool[j] = (uint8_t)t; }
    for (i = 0; i < ex.n; i++) ex.rhy[i] = EX_RHY[pool[i]];
    for (i = 8u; i-- > 1u;) { j = rndn(i + 1u); t = pool[i]; pool[i] = pool[j]; pool[j] = (uint8_t)t; }
    for (i = 0; i < ex.n; i++) { ex.base[i] = EX_BASE[pool[i]]; ex.shape[i] = (uint8_t)rndn(4); }
    for (i = 0; i < ex.n; i++) ex.perm[i] = (uint8_t)i;
    do {
        for (i = ex.n; i-- > 1u;) { j = rndn(i + 1u); t = ex.perm[i]; ex.perm[i] = ex.perm[j]; ex.perm[j] = (uint8_t)t; }
        for (i = 0, t = 0; i < ex.n; i++) t += ex.perm[i] == i;
    } while (t > 1u);                           /* (not left straight through) */
    for (i = 0; i < EX_N; i++) ex.conn[i] = -1;
    ex.through = 0xFF; ex.msg = 0; ex.play_t = 0;
}
static void ex_say(const char *m) { ex.msg = m; ex.msg_t = fm1_ms; }
static void ex_patch(uint32_t a, uint32_t b)    /* a cable from line a to output b (or out again) */
{
    uint32_t i;
    if (ex.conn[a] == (int8_t)b) { ex.conn[a] = -1; sfx_click(0, 50, 128); sfx_thunk(0, 40, 50, 128); ex.through = 0xFF; return; }
    for (i = 0; i < ex.n; i++) if (ex.conn[i] == (int8_t)b) ex.conn[i] = -1;
    ex.conn[a] = (int8_t)b;
    sfx_thunk(0, 47, 80, 128); sfx_click(au_time() + ms_frames(40), 70, 128);
    ex.through = 0xFF;
    ex_sound(ex.perm[a], 1);                    /* the line, heard through the new cable */
    ex.play_t = fm1_ms; ex.play_k = a;
}
static void ex_key(uint32_t k)                  /* (at once, even while the screen is sent) */
{
    uint32_t i;
    if (ex.won) return;
    for (i = 0; i < ex.n; i++) {
        if (EX_IN_KEY[i] == k) {
            ex.held_in = (int32_t)i;
            if (ex.held_out >= 0) ex_patch(i, (uint32_t)ex.held_out);
            else { ex_sound(ex.perm[i], 1); ex.play_t = fm1_ms; ex.play_k = i; }
        }
        if (EX_OUT_KEY[i] == k) {
            ex.held_out = (int32_t)i;
            if (ex.held_in >= 0) ex_patch((uint32_t)ex.held_in, i);
            else { ex_sound(i, 0); ex.play_t = fm1_ms; ex.play_k = 16u + i; }
        }
    }
}
static void ex_enter(void)
{
    ex.n = (uint32_t)PRM(P_EXCHANGE_LINES); if (ex.n > EX_N) ex.n = EX_N; if (ex.n < 3u) ex.n = 3;
    ex.rounds = (uint32_t)PRM(P_EXCHANGE_ROUNDS);
    ex.round = 0; ex.won = 0; ex.fin_t = 0; ex.held_in = -1; ex.held_out = -1;
    ex_new_round();
    kb_hook = ex_key;
}
static int ex_update(void)
{
    uint32_t i, k;
    led_btn_mask = BIT(BTN_EDIT) | BIT(BTN_PLAY);
    for (i = 0; i < ex.n; i++) {                /* keys let go: the hand off the jack */
        if (ex.held_in == (int32_t)i && !(in.kheld >> EX_IN_KEY[i] & 1u)) ex.held_in = -1;
        if (ex.held_out == (int32_t)i && !(in.kheld >> EX_OUT_KEY[i] & 1u)) ex.held_out = -1;
    }
    if (ex.won) return 1;
    if (ex.fin_t) {                             /* a board through: the next, or done */
        if (fm1_ms - ex.fin_t > 1600u) { ex.fin_t = 0; if (++ex.round >= ex.rounds) { ex.won = 1; return 1; } ex_new_round(); }
    } else if (PRESSED(BTN_PLAY)) {             /* test the board */
        uint32_t c = 0, all = 1;
        for (i = 0; i < ex.n; i++) { if (ex.conn[i] < 0) all = 0; else if ((uint32_t)ex.conn[i] == ex.perm[i]) c++; }
        if (!all) { ex_say(T_EX_ALL); sfx_buzz(0); }
        else if (c == ex.n) { ex.through = c; ex.fin_t = fm1_ms; sfx_motif(au_time()); ex_say(T_EX_DONE); }
        else { ex.through = c; sfx_buzz(0); ex_say(0); }
    }
    for (k = 0; k < 27u; k++) {
        uint32_t lit = 0, on = 0;
        for (i = 0; i < ex.n; i++) {
            if (EX_IN_KEY[i] == k) { lit = 1; on = ex.conn[i] >= 0 || ex.held_in == (int32_t)i; }
            if (EX_OUT_KEY[i] == k) { uint32_t j; lit = 1; on = ex.held_out == (int32_t)i; for (j = 0; j < ex.n; j++) if (ex.conn[j] == (int8_t)i) on = 1; }
        }
        fm1_led_key_glow(KEY_LED(k), lit);
        fm1_led_key(KEY_LED(k), on);
    }
    return 0;
}

static int32_t ex_x(uint32_t i) { return 24 + (int32_t)i * (192 / (int32_t)(ex.n > 1u ? ex.n - 1u : 1u)); }
#define EX_TY 84
#define EX_BY 152
static void ex_cable(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c, int pulse)   /* a sagging cord: a cubic, top to bottom */
{
    int32_t j, px = x0, py = y0, dy = (y1 - y0) * 2 / 3;
    for (j = 1; j <= 20; j++) {
        int32_t t = j * 256 / 20, u = 256 - t;
        int32_t b0 = u * u / 256 * u / 256, b1 = 3 * u * u / 256 * t / 256, b2 = 3 * u * t / 256 * t / 256, b3 = t * t / 256 * t / 256;
        int32_t x = (b0 * x0 + b1 * x0 + b2 * x1 + b3 * x1) / 256, y = (b0 * y0 + b1 * (y0 + dy) + b2 * (y1 - dy) + b3 * y1) / 256;
        g_thick(px, py, x, y, 3, c);
        if (pulse && (j == (int32_t)((fm1_ms / 40u) % 20u))) g_disc(x, y, 3, C_CREAM);
        px = x; py = y;
    }
}
static void ex_draw(uint32_t won)
{
    uint32_t i;
    char b[32], *p;
    g_rect(0, STAGE_Y0, SW, STAGE_Y1 - STAGE_Y0, RGB(14, 10, 32));
    g_rect(6, STAGE_Y0 + 1, SW - 12, 1, C_ROSE);
    if (ex.rounds > 1u) { p = str_cat(b, T_EX_BOARD " "); fmt_u(p, ex.round < ex.rounds ? ex.round + 1u : ex.rounds); while (*p) p++; *p++ = '/'; fmt_u(p, ex.rounds); g_text(10, STAGE_Y0 + 6, &FONT_S, b, C_INK2); }
    for (i = 0; i < ex.n; i++) {                /* the jacks: the stations (black keys, A..F) along the top, the lines (white keys, 1..6) along the bottom */
        int32_t x = ex_x(i);
        char t[2] = {(char)('1' + i), 0}, u[2] = {(char)('A' + i), 0};
        int lin = ex.play_k == i && fm1_ms - ex.play_t < 1300u, lout = ex.play_k == 16u + i && fm1_ms - ex.play_t < 1300u;
        g_text_c(x, EX_TY - 44, &FONT_S, EX_CODE[ex.st[i]], lout ? C_CREAM : C_PAPER3);
        g_text_c(x, EX_TY - 30, &FONT_S, u, lout ? C_CREAM : C_INK2);
        g_disc(x, EX_TY, 11, RGB(30, 22, 52));
        g_ring(x, EX_TY, 9, 12, ex.held_out == (int32_t)i ? C_CREAM : lout ? C_GLOW : C_GOLD);
        g_disc(x, EX_BY, 11, RGB(30, 22, 52));
        g_ring(x, EX_BY, 9, 12, ex.held_in == (int32_t)i ? C_CREAM : lin ? C_GLOW : C_CYAN);
        g_text_c(x, EX_BY + 16, &FONT_S, t, lin ? C_CREAM : C_INK2);
    }
    for (i = 0; i < ex.n; i++)                  /* the cords, from a line up to its station */
        if (ex.conn[i] >= 0) ex_cable(ex_x((uint32_t)ex.conn[i]), EX_TY, ex_x(i), EX_BY, EX_CABLE[i], ex.play_k == i && fm1_ms - ex.play_t < 1300u);
    if (ex.held_in >= 0 && ex.held_out < 0) {   /* a plug in the hand: rising from its line */
        int32_t x = ex_x((uint32_t)ex.held_in);
        ex_cable(x + 18, EX_BY - 52, x, EX_BY, EX_CABLE[ex.held_in], 0);
        g_disc(x + 18, EX_BY - 52, 6, EX_CABLE[ex.held_in]);
    }
    if (ex.held_out >= 0 && ex.held_in < 0) {
        int32_t x = ex_x((uint32_t)ex.held_out);
        g_thick(x, EX_TY, x - 10, EX_TY + 46, 3, C_INK2);
        g_disc(x - 10, EX_TY + 46, 6, C_INK2);
    }
    if (ex.msg && fm1_ms - ex.msg_t < 1800u) g_text_c(SW / 2, STAGE_Y1 - 14, &FONT_S, ex.msg, ex.fin_t ? C_VERD : C_ROSE);
    else if (ex.through != 0xFFu) {             /* the test: how many lines came through (in the legend's place) */
        p = b; fmt_u(p, ex.through); while (*p) p++; p = str_cat(p, " OF "); fmt_u(p, ex.n); while (*p) p++; str_cat(p, " " T_EX_THROUGH);
        g_text_c(SW / 2, STAGE_Y1 - 14, &FONT_S, b, ex.through == ex.n ? C_VERD : C_AMBER);
    } else {
        g_text(8, STAGE_Y1 - 14, &FONT_S, T_EX_WHITE, C_CYAN);
        g_text_r(SW - 8, STAGE_Y1 - 14, &FONT_S, T_EX_BLACK, C_ROSE);
    }
    (void)won;
}
static void ex_leave(void) { kb_hook = 0; au_cut(10); ui_keys_clear(); }
static const game_t G_EXCHANGE = {BIT(BTN_EDIT) | BIT(BTN_PLAY), 2000, ex_enter, ex_update, ex_draw, ex_leave};
