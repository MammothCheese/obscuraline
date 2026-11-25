/* SPDX-License-Identifier: GPL-3.0-only */
/* Chapter VIII: THE CABINET. A sliding-panel puzzle box: free the brass key panel.
 *
 * Six by six cells of walnut panels, each two or three long, each sliding only along its own length. The brass
 * key panel lies across the third row; the drawer opens when it slides out through the opening on the right.
 * K1 (or SELECT) picks a panel, K2 slides it a cell at a time; what blocks it is plain to see. The layouts are
 * puzzles.txt `layout` lines, each proven solvable (and its shortest solution counted) by tools/slider.py.
 * The key LEDs light for the panels that can move right now. */

#define SL_N 6
#define SL_MAX 16
static struct {
    int8_t r[SL_MAX], c[SL_MAX], len[SL_MAX], hz[SL_MAX];
    char name[SL_MAX];
    int32_t n, sel, key, moves;
    int32_t shown[SL_MAX];                      /* drawn position along its axis, Q8 */
    uint32_t bump_t, fin_t;
} sl;

static int sl_occ(int32_t r, int32_t c, int32_t skip)
{
    int32_t i, k;
    if (r < 0 || c < 0 || r >= SL_N || c >= SL_N) return 1;
    for (i = 0; i < sl.n; i++) {
        if (i == skip) continue;
        for (k = 0; k < sl.len[i]; k++)
            if ((sl.hz[i] ? sl.r[i] : sl.r[i] + k) == r && (sl.hz[i] ? sl.c[i] + k : sl.c[i]) == c) return 1;
    }
    return 0;
}
static int sl_can(int32_t i, int32_t d)         /* may panel i slide one cell (d = -1 / +1)? */
{
    int32_t r, c;
    if (sl.hz[i]) { r = sl.r[i]; c = d > 0 ? sl.c[i] + sl.len[i] : sl.c[i] - 1; }
    else { c = sl.c[i]; r = d > 0 ? sl.r[i] + sl.len[i] : sl.r[i] - 1; }
    if (i == sl.key && d > 0 && c == SL_N) return 1;   /* the key leaves through the opening */
    return !sl_occ(r, c, i);
}
static void sl_enter(void)
{
    const char *g = cur_ch->nstr ? cur_ch->str[rndn(cur_ch->nstr)] : "............XX......................";
    int32_t r, c, i;
    sl.n = 0; sl.key = 0;
    for (r = 0; r < SL_N; r++)
        for (c = 0; c < SL_N; c++) {
            char ch = g[r * SL_N + c];
            if (ch == '.') continue;
            for (i = 0; i < sl.n; i++) if (sl.name[i] == ch) break;
            if (i == sl.n) {
                if (sl.n >= SL_MAX) continue;
                sl.name[i] = ch; sl.r[i] = (int8_t)r; sl.c[i] = (int8_t)c; sl.len[i] = 1;
                sl.hz[i] = (int8_t)(c + 1 < SL_N && g[r * SL_N + c + 1] == ch);
                sl.n++;
            } else sl.len[i]++;
            if (ch == 'X') sl.key = i;
        }
    for (i = 0; i < sl.n; i++) sl.shown[i] = (sl.hz[i] ? sl.c[i] : sl.r[i]) * 256;
    sl.sel = sl.key; sl.moves = 0; sl.fin_t = 0;
}
static int sl_update(void)
{
    int32_t e = enc_main(), d = in.enc[ENC_KNOB2];
    led_btn_mask = BIT(BTN_EDIT);
    if (e) { sl.sel = ((sl.sel + e) % sl.n + sl.n) % sl.n; sfx_click(0, 30, 128); }
    while (d) {
        int32_t s = d > 0 ? 1 : -1;
        d -= s;
        if (sl_can(sl.sel, s)) {
            if (sl.hz[sl.sel]) sl.c[sl.sel] = (int8_t)(sl.c[sl.sel] + s); else sl.r[sl.sel] = (int8_t)(sl.r[sl.sel] + s);
            sl.moves++;
            sfx_click(0, 40, (uint8_t)(40 + sl.c[sl.sel] * 30));
            sfx_thunk(au_time() + ms_frames(20), 66, 18, 128);
        } else { sl.bump_t = fm1_ms; sfx_thunk(0, 50, 40, 128); break; }
    }
    return sl.c[sl.key] >= SL_N - 2 && sl_can(sl.key, 1) && sl.c[sl.key] + sl.len[sl.key] >= SL_N;
}
#define SL_X0 36
#define SL_Y0 36
#define SL_C 28
static void sl_draw(uint32_t won)
{
    int32_t i, k;
    uint32_t open = 0;
    if (won) { if (!sl.fin_t) sl.fin_t = fm1_ms; open = (uint32_t)ease_out((int32_t)((fm1_ms - sl.fin_t) * 256u / 1200u)); }
    g_cab_wall(SL_X0 + SL_N * SL_C / 2, SL_Y0 + SL_N * SL_C / 2, 0);
    /* the cabinet front: a cab panel, a dark well, the opening on the right of the key's row */
    g_cab_panel(SL_X0 - 10, SL_Y0 - 10, SL_N * SL_C + 20, SL_N * SL_C + 20, C_ROSE);
    g_rrect(SL_X0 - 2, SL_Y0 - 2, SL_N * SL_C + 4, SL_N * SL_C + 4, 3, RGB(10, 6, 24));
    for (k = 1; k < SL_N; k++) {
        g_rect(SL_X0 + k * SL_C, SL_Y0, 1, SL_N * SL_C, RGB(30, 18, 56));
        g_rect(SL_X0, SL_Y0 + k * SL_C, SL_N * SL_C, 1, RGB(30, 18, 56));
    }
    g_rect(SL_X0 + SL_N * SL_C, SL_Y0 + 2 * SL_C + 2, 12, SL_C - 4, RGB(4, 2, 10));   /* the opening */
    g_led(SL_X0 + SL_N * SL_C + 14, SL_Y0 + 2 * SL_C + SL_C / 2, 2, won ? C_VERD : C_AMBER);
    for (i = 0; i < sl.n; i++) {
        int32_t t = (sl.hz[i] ? sl.c[i] : sl.r[i]) * 256, x, y, w, h;
        if (i == sl.key && won) t = (sl.c[i] * 256) + (int32_t)open * 3;   /* the key slides out */
        sl.shown[i] += (t - sl.shown[i]) / 3 + ((t > sl.shown[i]) - (t < sl.shown[i]));
        x = SL_X0 + (sl.hz[i] ? sl.shown[i] * SL_C / 256 : sl.c[i] * SL_C) + 2;
        y = SL_Y0 + (sl.hz[i] ? sl.r[i] * SL_C : sl.shown[i] * SL_C / 256) + 2;
        w = (sl.hz[i] ? sl.len[i] * SL_C : SL_C) - 4;
        h = (sl.hz[i] ? SL_C : sl.len[i] * SL_C) - 4;
        if (i == sl.key) {
            g_plate(x, y, w, h, C_BRASS, C_BRASS_L, C_BRASS_DD);
            g_disc(x + 12, y + h / 2, 5, C_BRASS_DD); g_rect(x + 12, y + h / 2 - 2, 26, 4, C_BRASS_DD);   /* a key, engraved */
            g_rect(x + 32, y + h / 2, 3, 6, C_BRASS_DD);
        } else {
            uint16_t face = (sl.name[i] & 1) ? RGB(66, 40, 112) : RGB(54, 32, 96);
            g_rrect(x, y, w, h, 3, RGB(110, 60, 160));
            g_rrect(x + 1, y + 1, w - 2, h - 2, 3, face);
            if (sl.hz[i]) g_rect(x + 4, y + h / 2, w - 8, 1, C_ROSE_D);     /* a neon seam along it */
            else g_rect(x + w / 2, y + 4, 1, h - 8, C_ROSE_D);
        }
        if (i == sl.sel && !won) {              /* the picked panel: a glowing knob, arrows along its way */
            uint16_t gc = fm1_ms - sl.bump_t < 300u ? C_RUBY : C_GLOW;
            g_frame(x - 1, y - 1, w + 2, h + 2, gc);
            g_led(x + w / 2, y + h / 2, 3, gc);
        } else if (i != sl.key) g_disc(x + w / 2, y + h / 2, 3, C_BRASS_D);
    }
    if (!won) {
        char t[16], *p = t;
        p = str_cat(p, T_CB_MOVES " "); fmt_u(p, (uint32_t)sl.moves);
        g_text(SL_X0 - 8, 206, &FONT_S, t, C_CREAM);
    }
    /* the keys show which panels can move now: key k for panel k (in the order K1 picks them) */
    for (k = 0; k < 27; k++) {
        int on = k < sl.n && (sl_can(k, -1) || sl_can(k, 1));
        fm1_led_key(KEY_LED((uint32_t)k), k == sl.sel && !won);
        fm1_led_key_glow(KEY_LED((uint32_t)k), on && !won);
    }
}
static void sl_leave(void) { ui_keys_clear(); }
static const game_t G_SLIDER = {BIT(BTN_EDIT), 1600, sl_enter, sl_update, sl_draw, sl_leave};
