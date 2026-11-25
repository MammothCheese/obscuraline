/* SPDX-License-Identifier: GPL-3.0-only */
/* THE DEPOT YARD: a shunting puzzle (the old sidings at the end of a line).
 *
 * Three sidings (A holds five wagons, B and C three) meet at a set of points; past them the headshunt, where
 * the loco stands, has room for the loco and three wagons. K1 sets the points (A, B, C), K2 how many wagons.
 * SEL: the loco runs into the siding, couples the wagons nearest the points and pulls them out (onto the end
 * of what it already has). FX: it pushes its train into the siding and leaves that many there, the far end
 * first. Make up the train asked for in siding A: exactly those wagons, in that order from the buffer stop.
 * Each start is the solution scrambled by legal moves, so it can always be done. */

#define SH_S 3
#define SH_HEAD 3
#define SH_WM 7
static const uint8_t SH_CAP[SH_S] = {5, 3, 3};
static const uint16_t SH_COL[SH_WM + 1] = {0, C_CYAN, C_ROSE, C_AMBER, C_VERD, C_GOLD, C_STEEL_L, C_COPPER};

static struct {
    uint8_t sid[SH_S][6], n[SH_S];              /* [0] at the buffer .. [n-1] nearest the points */
    uint8_t lo[SH_HEAD + 1], ln;                /* the loco's train: [0] next to the loco .. [ln-1] the far end */
    uint8_t goal[5], gn, wag;
    uint32_t sel, cnt, rounds, round, won, moves, anim_t, anim_s, msg_t;
    const char *msg;
} sh;

static int sh_pull(uint32_t s, uint32_t k)      /* take k wagons out of siding s (0: cannot) */
{
    uint32_t i;
    if (!k || sh.n[s] < k || sh.ln + k > SH_HEAD) return 0;
    for (i = 0; i < k; i++) sh.lo[sh.ln++] = sh.sid[s][--sh.n[s]];
    return 1;
}
static int sh_push(uint32_t s, uint32_t k)      /* leave the far k wagons in siding s */
{
    uint32_t i;
    if (!k || sh.ln < k || sh.n[s] + k > SH_CAP[s]) return 0;
    for (i = 0; i < k; i++) sh.sid[s][sh.n[s]++] = sh.lo[--sh.ln];
    return 1;
}
static int sh_done(void)
{
    uint32_t i;
    if (sh.n[0] != sh.gn) return 0;
    for (i = 0; i < sh.gn; i++) if (sh.sid[0][i] != sh.goal[i]) return 0;
    return 1;
}
static void sh_new_round(void)
{
    uint32_t i, j, w, step;
    sh.wag = (uint8_t)(sh.round ? 6u : 5u); sh.gn = (uint8_t)(sh.round ? 4u : 3u);
    do {
        uint8_t pool[SH_WM];
        for (i = 0; i < sh.wag; i++) pool[i] = (uint8_t)(i + 1u);
        for (i = sh.wag; i-- > 1u;) { j = rndn(i + 1u); w = pool[i]; pool[i] = pool[j]; pool[j] = (uint8_t)w; }
        for (i = 0; i < SH_S; i++) sh.n[i] = 0;
        sh.ln = 0;
        for (i = 0; i < sh.gn; i++) { sh.goal[i] = pool[i]; sh.sid[0][sh.n[0]++] = pool[i]; }   /* the solution ... */
        for (i = sh.gn; i < sh.wag; i++) { uint32_t s = 1u + (i & 1u); sh.sid[s][sh.n[s]++] = pool[i]; }
        for (step = 0; step < 400u && (step < 60u || sh.ln || sh_done()); step++) {   /* ... scrambled by legal moves */
            uint32_t s = rndn(SH_S), k = 1u + rndn(3);
            if (rndn(2)) sh_pull(s, k); else sh_push(s, k);
        }
        while (sh.ln) { for (i = SH_S; i-- > 0u;) if (sh_push(i, 1)) break; }
    } while (sh_done());
    sh.moves = 0; sh.sel = 0; sh.cnt = 1; sh.anim_t = 0;
}

static void sh_enter(void)
{
    sh.rounds = (uint32_t)PRM(P_SHUNTING_ROUNDS);
    sh.round = 0; sh.won = 0; sh.msg = 0;
    sh_new_round();
}
static void sh_say(const char *m) { sh.msg = m; sh.msg_t = fm1_ms; }

static int sh_update(void)
{
    int32_t e1 = enc_main() + in.enc[ENC_ALGORITHM], e2 = in.enc[ENC_KNOB2] + in.enc[ENC_PRESETS];
    led_btn_mask = BIT(BTN_EDIT) | BIT(BTN_SEL) | BIT(BTN_FX);
    if (sh.won) return 1;
    if (sh.anim_t && fm1_ms - sh.anim_t < 700u) return 0;   /* the loco on its way */
    if (sh.anim_t) {
        sh.anim_t = 0;
        if (sh_done()) {
            sfx_motif(au_time());
            if (++sh.round >= sh.rounds) { sh.won = 1; return 1; }
            sh_new_round();
            return 0;
        }
    }
    if (e1) { sh.sel = (uint32_t)(((int32_t)sh.sel + (e1 > 0 ? 1 : 2)) % SH_S); sfx_thunk(0, 52, 50, 128); sfx_click(0, 40, 128); }
    if (e2) { sh.cnt = (uint32_t)clampi((int32_t)sh.cnt + (e2 > 0 ? 1 : -1), 1, 3); sfx_click(0, 50, 128); }
    if (PRESSED(BTN_SEL) || PRESSED(BTN_FX)) {   /* (SEL pulls out, FX pushes in) */
        int pull = PRESSED(BTN_SEL) != 0u, ok = pull ? sh_pull(sh.sel, sh.cnt) : sh_push(sh.sel, sh.cnt);
        if (ok) { sh.moves++; sh.anim_t = fm1_ms; sh.anim_s = sh.sel; sfx_thunk(0, 44, 80, 128); sfx_thunk(au_time() + ms_frames(350), 40, 70, 128); }
        else {
            sfx_buzz(0);
            sh_say(pull ? (sh.n[sh.sel] < sh.cnt ? T_SH_NOT_THERE : T_SH_HEAD_FULL) : (sh.ln < sh.cnt ? T_SH_NOT_THERE : T_SH_FULL));
        }
    }
    return 0;
}

static void sh_mark(int32_t x, int32_t y, uint16_t c) { g_rframe(x - 2, y - 9, 26, 18, 4, 2, c); }   /* a wagon that would move */
static void sh_wagon(int32_t x, int32_t y, uint32_t w)
{
    char t[2] = {(char)('0' + w), 0};
    g_rrect(x, y - 7, 22, 14, 3, SH_COL[w]);
    g_rect(x + 2, y + 6, 4, 2, C_INK); g_rect(x + 16, y + 6, 4, 2, C_INK);
    g_text_c(x + 11, y - 7, &FONT_S, t, C_NIGHT);
}
static const int32_t SH_Y[SH_S] = {92, 130, 168};
#define SH_PX 150                               /* the points */
#define SH_HY 130                               /* the headshunt */

static void sh_draw(uint32_t won)
{
    uint32_t s, i;
    int32_t y;
    char b[32], *p;
    g_rect(0, STAGE_Y0, SW, STAGE_Y1 - STAGE_Y0, RGB(10, 6, 24));
    for (y = STAGE_Y0 + 34; y < STAGE_Y1; y += 12) g_rect(0, y, SW, 1, RGB(16, 10, 34));
    /* what to make up */
    g_rrect(6, STAGE_Y0 + 3, SW - 12, 28, 5, C_PANEL);
    p = str_cat(b, T_SH_MAKE);
    g_text(12, STAGE_Y0 + 9, &FONT_S, b, C_INK2);
    for (i = 0; i < sh.gn; i++) sh_wagon(120 + (int32_t)i * 26, STAGE_Y0 + 17, sh.goal[i]);
    if (sh.rounds > 1u) { p = b; fmt_u(p, sh.round < sh.rounds ? sh.round + 1u : sh.rounds); while (*p) p++; *p++ = '/'; fmt_u(p, sh.rounds); g_text_r(SW - 10, STAGE_Y0 + 9, &FONT_S, b, C_INK2); }
    /* the sidings, the fan, the headshunt */
    for (s = 0; s < SH_S; s++) {
        int on = s == sh.sel;
        uint16_t c = on ? C_CYAN : RGB(60, 50, 110);
        char t[2] = {(char)('A' + s), 0};
        g_rect(14, SH_Y[s] + 8, SH_PX - 40, 2, c);
        g_thick(SH_PX - 26, SH_Y[s] + 9, SH_PX, SH_HY + 9, on ? 2 : 1, c);
        g_rect(8, SH_Y[s] - 4, 4, 14, C_RUBY);           /* the buffer stop */
        for (i = 0; i < SH_CAP[s]; i++) g_rframe(16 + (int32_t)i * 24, SH_Y[s] - 7, 22, 14, 3, 1, RGB(36, 30, 70));
        g_text(SH_PX - 30, SH_Y[s] - 10, &FONT_S, t, on ? C_CYAN : C_INK2);
        for (i = 0; i < sh.n[s]; i++) sh_wagon(16 + (int32_t)i * 24, SH_Y[s], sh.sid[s][i]);
    }
    g_rect(SH_PX, SH_HY + 8, SW - SH_PX - 4, 2, C_CYAN_D);
    g_disc(SH_PX, SH_HY + 9, 3, C_CYAN);
    {   /* the loco and its train: in the headshunt, or out at the points of the siding it went to */
        int32_t lx = SW - 34, ly = SH_HY;
        if (sh.anim_t) {
            uint32_t el = fm1_ms - sh.anim_t, f = el < 350u ? el * 256u / 350u : (700u - el) * 256u / 350u;
            int32_t tx = SH_PX - 30 + (int32_t)sh.ln * 24, ty = SH_Y[sh.anim_s];
            lx += (tx - lx) * (int32_t)f / 256; ly += (ty - ly) * (int32_t)f / 256;
        }
        g_rrect(lx, ly - 8, 30, 16, 4, C_CREAM);
        g_rect(lx + 22, ly - 12, 6, 5, C_CREAM);
        g_rect(lx + 2, ly - 4, 6, 4, C_CYAN);
        for (i = 0; i < sh.ln; i++) sh_wagon(lx - 24 - (int32_t)i * 24, ly, sh.lo[i]);
    }
    if (!sh.anim_t) {                          /* what SEL would take (gold) and FX would leave (cyan), outlined */
        uint32_t s2 = sh.sel, k2;
        int canp = sh.n[s2] >= sh.cnt && sh.ln + sh.cnt <= SH_HEAD, canq = sh.ln >= sh.cnt && sh.n[s2] + sh.cnt <= SH_CAP[s2];
        if (canp) for (k2 = 0; k2 < sh.cnt; k2++) sh_mark(16 + (int32_t)(sh.n[s2] - 1u - k2) * 24, SH_Y[s2], C_GOLD);
        if (canq) for (k2 = 0; k2 < sh.cnt; k2++) sh_mark(SW - 34 - 24 - (int32_t)(sh.ln - 1u - k2) * 24, SH_HY, C_CYAN);
        {
            char t1[24], t2[24], *q;
            q = str_cat(t1, T_SH_PULL_N); *q++ = ' '; *q++ = (char)('0' + sh.cnt); *q++ = ' '; *q++ = (char)('A' + s2); *q = 0;
            q = str_cat(t2, T_SH_PUSH_N); *q++ = ' '; *q++ = (char)('A' + s2); *q = 0;
            g_text(10, STAGE_Y1 - 34, &FONT_S, t1, canp ? C_GOLD : C_PAPER3);
            g_text(10, STAGE_Y1 - 18, &FONT_S, t2, canq ? C_CYAN : C_PAPER3);
        }
    }
    /* the moves, a message */
    (void)p;
    p = b; fmt_u(p, sh.moves); while (*p) p++; str_cat(p, " " T_SH_MOVES);
    g_text_r(SW - 10, STAGE_Y1 - 18, &FONT_S, b, C_INK2);
    if (sh.msg && fm1_ms - sh.msg_t < 1500u) g_text_c(SW / 2, STAGE_Y1 - 52, &FONT_S, sh.msg, C_ROSE);
    (void)won;
}
static void sh_leave(void) { ui_keys_clear(); }
static const game_t G_SHUNT = {BIT(BTN_EDIT) | BIT(BTN_SEL) | BIT(BTN_FX), 2000, sh_enter, sh_update, sh_draw, sh_leave};
