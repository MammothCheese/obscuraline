/* SPDX-License-Identifier: GPL-3.0-only */
/* THE SIGNAL CONTROL: a train controller's screen. Set the route.
 *
 * Tracks run left to right; a train comes in on one of them and must reach the platform asked for. Between the
 * tracks are crossovers (points): thrown, a train moving along either track crosses to the other. Each crossover
 * hangs on a numbered lever, and some levers throw two at once (interlocked). Parked trains (red) block their
 * stretch of track. The keys C D E F G A throw levers 1..6; the screen shows the route as set, to the first
 * block or the platform it ends at. PLAY sends a train along it: the right platform, clear, and the next route is
 * asked for. Layouts are new each time (drawn from a solution, so there always is one). */

#define SW_RM 6                                 /* tracks, at most */
#define SW_C 6                                  /* crossover columns */
#define SW_XM 12
static const uint8_t SW_KEY[6] = {7, 9, 11, 12, 14, 16};   /* C4 D4 E4 F4 G4 A4 */
static const char SW_KN[6] = {'C', 'D', 'E', 'F', 'G', 'A'};

static struct {
    uint32_t rounds, round, rows, levers, entry, target, won, state, state_t;
    uint8_t xn, xc[SW_XM], xr[SW_XM], xl[SW_XM];   /* each crossover: its column, its upper track, its lever */
    uint8_t lev, sol;                           /* levers thrown (bits); the drawn solution */
    uint8_t occ[SW_RM];                         /* bit s: track's stretch s blocked (stretch s: before column s; SW_C: the last) */
    uint8_t pr[SW_C + 1];                       /* the route: its track on each stretch */
    uint32_t block, exitr, run_t, bigrows;
} swb;

static uint32_t sw_sim(uint8_t lev, uint8_t *pr, uint32_t *block)   /* the route the levers set: where it ends (block: 255 none) */
{
    uint32_t s, r = swb.entry, i;
    *block = 255;
    for (s = 0; s <= SW_C; s++) {
        pr[s] = (uint8_t)r;
        if (swb.occ[r] >> s & 1u) { if (*block == 255u) *block = s; }
        if (s == SW_C) break;
        for (i = 0; i < swb.xn; i++)
            if (swb.xc[i] == s && (lev >> swb.xl[i] & 1u) && (swb.xr[i] == r || swb.xr[i] + 1u == r)) { r = swb.xr[i] == r ? r + 1u : r - 1u; break; }
    }
    return r;
}
static void sw_new_round(void)
{
    uint32_t tries, i, j, b0, bs;
    uint8_t p0[SW_C + 1], ps[SW_C + 1];
    swb.rows = swb.bigrows ? 6u : 5u + (swb.round > 0u);
    if (swb.rows > SW_RM) swb.rows = SW_RM;
    swb.levers = swb.bigrows ? 6u : 4u + swb.round;
    if (swb.levers > 6u) swb.levers = 6u;
    for (tries = 0; tries < 3000u; tries++) {
        uint32_t want = swb.levers + 3u + swb.round + swb.bigrows * 2u, e0, es, pc = 0, t2;   /* (more points than levers: some throw two) */
        swb.xn = 0;
        for (i = 0; i < 60u && swb.xn < want && swb.xn < SW_XM; i++) {
            uint32_t c = rndn(SW_C), r = rndn(swb.rows - 1u), ok = 1;
            uint32_t same = 0;
            for (j = 0; j < swb.xn; j++) {
                if (swb.xc[j] == c && (swb.xr[j] + 1u >= r && swb.xr[j] <= r + 1u)) ok = 0;
                if (swb.xr[j] == r) same++;
            }
            if (!ok || same >= 3u) continue;    /* (three points at most between any two tracks: the route spreads over the yard, not a knot of one pair) */
            swb.xc[swb.xn] = (uint8_t)c; swb.xr[swb.xn] = (uint8_t)r;
            swb.xl[swb.xn] = (uint8_t)(swb.xn < swb.levers ? swb.xn : rndn(swb.levers));   /* the extras: a second point on a lever */
            swb.xn++;
        }
        if (swb.xn < swb.levers) continue;
        swb.entry = rndn(swb.rows);
        for (i = 0; i < SW_RM; i++) swb.occ[i] = 0;
        swb.sol = (uint8_t)(1u + rndn((1u << swb.levers) - 1u));
        for (t2 = swb.sol, pc = 0; t2; t2 >>= 1) pc += t2 & 1u;
        if (pc < 3u + (swb.round > 1u)) continue;   /* (an answer of three levers or more) */
        es = sw_sim(swb.sol, ps, &bs);
        e0 = sw_sim(0, p0, &b0);
        if (es == e0) continue;                 /* (the levers as they stand must not already do it) */
        swb.target = es;
        {   /* parked trains: one on the route as it stands, others anywhere off the solution */
            uint32_t placed = 0, k;
            for (k = 0; k < 60u && placed < swb.rows + swb.round; k++) {
                uint32_t r = rndn(swb.rows), s = 1u + rndn(SW_C);
                if (placed == 0u) { s = 1u + rndn(SW_C); r = p0[s]; }
                if (ps[s] == r) continue;
                swb.occ[r] |= (uint8_t)(1u << s); placed++;
            }
        }
        if (sw_sim(swb.sol, ps, &bs) == swb.target && bs == 255u) {
            uint32_t l2, short_cut = 0;
            for (l2 = 1; l2 < (1u << swb.levers) && !short_cut; l2++) {   /* (no shortcut: no answer of fewer than three levers) */
                uint32_t q = 0, u, b2;
                uint8_t p2[SW_C + 1];
                for (u = l2; u; u >>= 1) q += u & 1u;
                if (q < 3u && sw_sim((uint8_t)l2, p2, &b2) == swb.target && b2 == 255u) short_cut = 1;
            }
            if (short_cut) continue;
            b0 = 0; sw_sim(0, p0, &b0); if (b0 != 255u || e0 != swb.target) break;
        }
    }
    swb.lev = 0; swb.state = 0;
}

static void sw_enter(void)
{
    swb.rounds = (uint32_t)PRM(P_SWITCHES_ROUNDS); swb.bigrows = (uint32_t)PRM(P_SWITCHES_BIG);
    swb.round = 0; swb.won = 0;
    sw_new_round();
}

static int sw_update(void)
{
    uint32_t k;
    led_btn_mask = BIT(BTN_EDIT) | BIT(BTN_PLAY);
    led_btn_blink = swb.state == 0u ? BIT(BTN_PLAY) : 0u;
    swb.exitr = sw_sim(swb.lev, swb.pr, &swb.block);
    if (swb.won) return 1;
    if (swb.state == 0u) {
        for (k = 0; k < swb.levers; k++)
            if (in.kdown >> SW_KEY[k] & 1u) { swb.lev ^= (uint8_t)(1u << k); sfx_thunk(0, 50 + (int32_t)k * 2, 60, 128); sfx_click(0, 50, 128); }
        if (PRESSED(BTN_PLAY)) { swb.state = 1; swb.run_t = fm1_ms; sfx_chime(76, 50, 0, 128); }
    } else if (swb.state == 1u) {               /* the train runs the route */
        uint32_t f = (fm1_ms - swb.run_t) * (SW_C + 1u) / 2600u;   /* stretches gone */
        if (swb.block != 255u && f >= swb.block) { swb.state = 3; swb.state_t = fm1_ms; sfx_buzz(0); sfx_thunk(0, 36, 90, 128); }
        else if (f > SW_C) {
            if (swb.exitr == swb.target) { swb.state = 2; swb.state_t = fm1_ms; sfx_motif(au_time()); }
            else { swb.state = 4; swb.state_t = fm1_ms; sfx_buzz(0); }
        }
    } else if (fm1_ms - swb.state_t > 1500u) {
        if (swb.state == 2u) {
            if (++swb.round >= swb.rounds) { swb.won = 1; return 1; }
            sw_new_round();
        } else swb.state = 0;                   /* blocked or the wrong platform: try again */
    }
    for (k = 0; k < 27u; k++) {
        uint32_t j, lv = 6;
        for (j = 0; j < swb.levers; j++) if (SW_KEY[j] == k) lv = j;
        fm1_led_key_glow(KEY_LED(k), lv < 6u);
        fm1_led_key(KEY_LED(k), lv < 6u && (swb.lev >> lv & 1u));
    }
    return 0;
}

#define SW_XL 30
#define SW_XR 200
static int32_t sw_cx(int32_t c) { return SW_XL + (c + 1) * (SW_XR - SW_XL) / (SW_C + 1); }   /* column c's x (-1: entry, SW_C: exit) */
static int32_t sw_ry(int32_t r) { int32_t st = swb.rows > 1u ? 112 / (int32_t)(swb.rows - 1u) : 0; if (st > 26) st = 26; return 120 - st * ((int32_t)swb.rows - 1) / 2 + r * st; }

static void sw_draw(uint32_t won)
{
    uint32_t i, s, r;
    int32_t dx = (SW_XR - SW_XL) / (SW_C + 1), y;
    char b[32], *p;
    /* the screen: dark glass, scanlines, a frame */
    g_rrect(4, STAGE_Y0 + 2, SW - 8, STAGE_Y1 - STAGE_Y0 - 4, 8, RGB(6, 10, 22));
    for (y = STAGE_Y0 + 4; y < STAGE_Y1 - 3; y += 3) g_rect(8, y, SW - 16, 1, RGB(10, 16, 32));
    g_rframe(4, STAGE_Y0 + 2, SW - 8, STAGE_Y1 - STAGE_Y0 - 4, 8, 1, C_CYAN_D);
    p = str_cat(b, T_SW_ROUTE " "); fmt_u(p, swb.round < swb.rounds ? swb.round + 1u : swb.rounds); while (*p) p++; *p++ = '/'; fmt_u(p, swb.rounds);
    g_text(12, STAGE_Y0 + 5, &FONT_S, b, C_CYAN);
    p = str_cat(b, T_SW_TO " "); fmt_u(p, swb.target + 1u);
    g_text_r(SW - 12, STAGE_Y0 + 5, &FONT_S, b, C_GOLD);
    /* the tracks, the parked trains */
    for (r = 0; r < swb.rows; r++) {
        int32_t yy = sw_ry((int32_t)r);
        g_rect(SW_XL - 14, yy, SW_XR - SW_XL + 22, 1, RGB(30, 70, 100));
        for (s = 0; s <= SW_C; s++)
            if (swb.occ[r] >> s & 1u) {
                int32_t xa = sw_cx((int32_t)s - 1) + 4, xb = sw_cx((int32_t)s) - 4;
                g_rrect(xa, yy - 3, xb - xa, 7, 2, C_RUBY);
            }
        {   /* the platforms, numbered; the one asked for lit */
            char t[2] = {(char)('1' + r), 0};
            int tg = r == swb.target;
            g_rrect(SW_XR + 10, yy - 7, 18, 14, 3, tg ? C_GOLD : C_PANEL2);
            g_text_c(SW_XR + 19, yy - 7, &FONT_S, t, tg ? C_NIGHT : C_INK2);
        }
    }
    g_text(SW_XL - 26, sw_ry((int32_t)swb.entry) - 7, &FONT_S, ">", C_CYAN);
    /* the crossovers: an X between two tracks, bright when thrown, its lever's number */
    for (i = 0; i < swb.xn; i++) {
        int32_t x = sw_cx(swb.xc[i]), y0 = sw_ry(swb.xr[i]), y1 = sw_ry(swb.xr[i] + 1), on = swb.lev >> swb.xl[i] & 1u;
        uint16_t c = on ? C_CYAN : RGB(40, 80, 110);
        g_line(x - dx / 3, y0, x + dx / 3, y1, c);
        g_line(x - dx / 3, y1, x + dx / 3, y0, c);
        {
            char t[2] = {(char)('1' + swb.xl[i]), 0};
            g_rrect(x - 5, (y0 + y1) / 2 - 7, 11, 14, 2, on ? C_CYAN : C_PANEL);
            g_text_c(x, (y0 + y1) / 2 - 7, &FONT_S, t, on ? C_NIGHT : C_INK2);
        }
    }
    /* the route as set: bright to the first block or the platform */
    for (s = 0; s <= SW_C; s++) {
        int32_t xa = sw_cx((int32_t)s - 1) + (s ? dx / 3 : -14), xb = sw_cx((int32_t)s) - (s < SW_C ? dx / 3 : -8), yy = sw_ry(swb.pr[s]);
        uint16_t c = swb.block != 255u && s >= swb.block ? (s == swb.block ? C_ROSE : 0) : swb.exitr == swb.target ? C_VERD : C_GOLD;
        if (!c) break;
        g_rect(xa, yy - 1, xb - xa, 3, c);
        if (s < SW_C && swb.block != s) {       /* through the column: across, or straight on */
            if (swb.pr[s + 1] != swb.pr[s]) g_thick(sw_cx((int32_t)s) - dx / 3, yy, sw_cx((int32_t)s) + dx / 3, sw_ry(swb.pr[s + 1]), 2, c);
            else g_rect(sw_cx((int32_t)s) - dx / 3, yy - 1, dx * 2 / 3 + 1, 3, c);
        }
        if (s == swb.block) break;
    }
    /* the train on its way */
    if (swb.state >= 1u && swb.state <= 4u) {
        uint32_t el = swb.state == 1u ? fm1_ms - swb.run_t : 2600u, f = el * (SW_C + 1u) * 256u / 2600u, si = f >> 8, fr = f & 255u;
        int32_t tx, ty;
        if (swb.block != 255u && si >= swb.block) { si = swb.block; fr = 128; }
        if (si > SW_C) { si = SW_C; fr = 255; }
        tx = sw_cx((int32_t)si - 1) + (int32_t)fr * dx / 256;
        ty = sw_ry(swb.pr[si]);
        g_rrect(tx - 7, ty - 4, 14, 9, 3, swb.state == 3u ? C_ROSE : C_CREAM);
        g_rect(tx + 4, ty - 1, 2, 2, C_CYAN);
    }
    if (swb.state >= 2u) g_text_c(SW / 2, STAGE_Y1 - 52, &FONT_S, swb.state == 2u ? T_SW_CLEAR : swb.state == 3u ? T_SW_BLOCKED : T_SW_WRONG, swb.state == 2u ? C_VERD : C_ROSE);
    /* the lever bank: numbered, the keys that throw them */
    for (i = 0; i < swb.levers; i++) {
        int32_t lx = 24 + (int32_t)i * 32, ly = STAGE_Y1 - 34, on = swb.lev >> i & 1u;
        char t[2] = {(char)('1' + i), 0}, kn[2] = {SW_KN[i], 0};
        g_rrect(lx, ly, 24, 26, 4, C_PANEL);
        g_thick(lx + 12, ly + 18, lx + (on ? 20 : 4), ly + 5, 3, on ? C_CYAN : C_PAPER3);
        g_disc(lx + (on ? 20 : 4), ly + 5, 3, on ? C_CREAM : C_INK2);
        g_text(lx + 1, ly + 12, &FONT_S, t, C_INK2);
        g_text_r(lx + 23, ly + 12, &FONT_S, kn, on ? C_CYAN : C_PAPER3);
    }
    (void)won;
}
static void sw_leave(void) { ui_keys_clear(); }
static const game_t G_SWITCHES = {BIT(BTN_EDIT) | BIT(BTN_PLAY), 2000, sw_enter, sw_update, sw_draw, sw_leave};
