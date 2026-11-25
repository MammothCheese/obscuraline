/* SPDX-License-Identifier: GPL-3.0-only */
/* The gear board: build a train on a board of pegs and watch it turn.
 *
 * Pegs sit in a honeycomb, each six neighbours an equal distance apart. Gears come in three sizes: SMALL, MEDIUM,
 * LARGE. Two gears on neighbouring pegs MESH when their sizes fit the distance (medium + medium, or small +
 * large); then the driven one turns, the other way round. Too big together (medium + large, large + large) they
 * COLLIDE and cannot both be placed; too small (small + small, small + medium) they do not touch.
 * The motor turns on the left. Every gear it reaches turns at once, so you see your train work. Bring the train
 * to the hand(s) on the right, turning the way its arrow shows. Each mesh reverses the direction, so the length
 * of the route matters: an extra gear turns the hand the other way.
 * Rusted gears do not turn: a train that meshes into one JAMS. So does a ring of three gears all meshing
 * (they would have to turn both ways at once).
 *
 * K1 moves along a row, K2 between rows, K3 changes the gear on the peg (empty, small, medium, large: as many as
 * the stock allows). The board is drawn from a solution made first, so there is always a way. Settings
 * (puzzles.txt): hands 1..2, chain (1 = medium gears, 2 = small and large), rusted, spares. */

#define GB_C 6
#define GB_R 5
#define GB_N (GB_C * GB_R)
enum { GB_EMPTY, GB_S, GB_M, GB_L };
static const uint8_t GB_HALF[4] = {0, 2, 3, 4};   /* radius in half units; neighbours are 6 apart */
#define GB_SP 30                                /* peg spacing, px (a half unit = 5 px) */

static struct {
    uint8_t g[GB_N];                            /* the gear on each peg */
    uint8_t fixed[GB_N];                        /* 1 motor, 2 hand, 3 rusted, 0 player's */
    int8_t want[GB_N];                          /* a hand: the way it must turn (+1 clockwise, -1 anticlockwise) */
    int8_t dir[GB_N];                           /* driven this frame: +1 / -1, 0 still */
    uint32_t ang[GB_N];
    int32_t stock[4];
    int32_t cur, motor, nhands, hand[2];
    uint8_t jam;                                /* the train jams (a rusted gear, or a ring of three) */
    uint8_t sol[GB_N];                          /* the solution the board was made from (the skip places it) */
    uint32_t bump_t, ok_since, fin_t, motor_ang;
} gb;

static int32_t gb_x(int32_t i) { int32_t r = i / GB_C, c = i % GB_C; return 34 + c * GB_SP + (r & 1) * (GB_SP / 2); }
static int32_t gb_y(int32_t i) { return 58 + (i / GB_C) * 26; }
static int gb_nb(int32_t i, int32_t *out)       /* the six neighbours ("odd rows shifted right") */
{
    static const int8_t E[6][2] = {{-1, 0}, {1, 0}, {-1, -1}, {0, -1}, {-1, 1}, {0, 1}};
    static const int8_t O[6][2] = {{-1, 0}, {1, 0}, {0, -1}, {1, -1}, {0, 1}, {1, 1}};
    int32_t r = i / GB_C, c = i % GB_C, k, n = 0;
    for (k = 0; k < 6; k++) {
        int32_t dc = (r & 1) ? O[k][0] : E[k][0], dr = (r & 1) ? O[k][1] : E[k][1], cc = c + dc, rr = r + dr;
        if (cc >= 0 && cc < GB_C && rr >= 0 && rr < GB_R) out[n++] = rr * GB_C + cc;
    }
    return n;
}
static int gb_mesh(int a, int b) { return GB_HALF[a] + GB_HALF[b] == 6; }
static int gb_clash(int a, int b) { return GB_HALF[a] + GB_HALF[b] > 6; }

/* drive the train: from the motor, every meshing gear turns the other way; a rusted gear or a contradiction jams */
static void gb_drive(void)
{
    int32_t q[GB_N], qh = 0, qt = 0, i, k, nb[6], n;
    for (i = 0; i < GB_N; i++) gb.dir[i] = 0;
    gb.jam = 0;
    gb.dir[gb.motor] = 1; q[qt++] = gb.motor;
    while (qh < qt) {
        int32_t a = q[qh++];
        n = gb_nb(a, nb);
        for (k = 0; k < n; k++) {
            int32_t b = nb[k];
            if (!gb.g[b] || !gb_mesh(gb.g[a], gb.g[b])) continue;
            if (gb.fixed[b] == 3) { gb.jam = 1; continue; }
            if (!gb.dir[b]) { gb.dir[b] = (int8_t)-gb.dir[a]; q[qt++] = b; }
            else if (gb.dir[b] == gb.dir[a]) gb.jam = 1;   /* both the same way: they bind */
        }
    }
    if (gb.jam) for (i = 0; i < GB_N; i++) gb.dir[i] = 0;
}
static int gb_can_place(int32_t i, int sz)
{
    int32_t nb[6], n = gb_nb(i, nb), k;
    for (k = 0; k < n; k++) if (gb.g[nb[k]] && gb_clash(sz, gb.g[nb[k]])) return 0;
    return 1;
}
/* a route from a to b that touches nothing it should not: each new peg next to no earlier route peg but its
 * predecessor (and the branch peg, for a second hand). Random depth-first, a few hundred tries. */
static int gb_route(int32_t a, int32_t b, const uint8_t *used, int32_t branch, int32_t *path, int32_t maxlen)
{
    int32_t len = 1, tries = 0, k, nb[6], n;
    uint8_t on[GB_N];
    path[0] = a;
    while (tries++ < 400) {
        int32_t cur = path[len - 1], cand[6], nc = 0;
        if (cur == b) return len;
        if (len >= maxlen) { len = 1; continue; }
        n = gb_nb(cur, nb);
        for (k = 0; k < n; k++) {
            int32_t c = nb[k], j, ok = 1, nb2[6], n2;
            for (j = 0; j < len; j++) if (path[j] == c) ok = 0;
            if (!ok || (used[c] && c != b)) continue;
            n2 = gb_nb(c, nb2);                 /* next to no other route peg (or used peg) but cur */
            for (j = 0; j < n2; j++) {
                int32_t d = nb2[j], m;
                if (d == cur || d == branch) continue;
                if (used[d] && d != b) { ok = 0; break; }
                for (m = 0; m < len; m++) if (path[m] == d) { ok = 0; break; }
                if (!ok) break;
            }
            if (ok) cand[nc++] = c;
        }
        if (!nc) { len = 1; continue; }
        /* lean toward the goal two times in three */
        {
            int32_t best = cand[rndn((uint32_t)nc)];
            if (rndn(3)) {
                int32_t j, bd = 1 << 30;
                for (j = 0; j < nc; j++) {
                    int32_t dx = gb_x(cand[j]) - gb_x(b), dy = gb_y(cand[j]) - gb_y(b), dd = dx * dx + dy * dy;
                    if (dd < bd) { bd = dd; best = cand[j]; }
                }
            }
            path[len++] = best;
        }
        (void)on;
    }
    return 0;
}
static void gb_enter(void)
{
    int32_t tries;
    gb.nhands = PRM(P_GEARBOARD_HANDS);
    for (tries = 0; tries < 200; tries++) {
        int32_t p1[GB_N], p2[GB_N], l1, l2 = 0, i, k, chain = PRM(P_GEARBOARD_CHAIN), nb[6], n, placed;
        uint8_t used[GB_N], need[4] = {0, 0, 0, 0}, sz[GB_N];
        for (i = 0; i < GB_N; i++) { gb.g[i] = 0; gb.fixed[i] = 0; gb.want[i] = 0; used[i] = 0; sz[i] = 0; }
        if (chain == 0) chain = 1 + (int32_t)rndn(2);
        gb.motor = (int32_t)(1u + rndn(GB_R - 2)) * GB_C;
        gb.hand[0] = (int32_t)rndn(GB_R) * GB_C + GB_C - 1;
        if (gb.nhands > 1) do gb.hand[1] = (int32_t)rndn(GB_R) * GB_C + GB_C - 1; while (iabs(gb.hand[1] / GB_C - gb.hand[0] / GB_C) < 2);
        used[gb.hand[0]] = 1; if (gb.nhands > 1) used[gb.hand[1]] = 1;
        l1 = gb_route(gb.motor, gb.hand[0], used, -1, p1, 12);
        if (!l1 || l1 < 5) continue;
        for (i = 0; i < l1; i++) used[p1[i]] = 1;
        if (gb.nhands > 1) {
            int32_t br = p1[1 + rndn((uint32_t)(l1 - 3))];
            used[br] = 0;
            l2 = gb_route(br, gb.hand[1], used, br, p2, 10);
            used[br] = 1;
            if (!l2 || l2 < 3) continue;
            for (i = 0; i < l2; i++) used[p2[i]] = 1;
        }
        /* sizes along the routes: all medium, or small and large in turn from the motor */
        {
            uint8_t ms = chain == 1 ? GB_M : (rndn(2) ? GB_S : GB_L);
            for (i = 0; i < l1; i++) sz[p1[i]] = chain == 1 ? GB_M : (uint8_t)((i & 1) ? (ms == GB_S ? GB_L : GB_S) : ms);
            if (gb.nhands > 1)
                for (i = 1; i < l2; i++) {
                    uint8_t prev = sz[p2[i - 1]];
                    sz[p2[i]] = chain == 1 ? GB_M : (uint8_t)(prev == GB_S ? GB_L : GB_S);
                }
            gb.g[gb.motor] = sz[gb.motor]; gb.fixed[gb.motor] = 1;
            for (k = 0; k < gb.nhands; k++) {
                int32_t h = gb.hand[k], d = 0, j;
                gb.g[h] = sz[h]; gb.fixed[h] = 2;
                /* its way: from the route's length (each mesh reverses) */
                if (k == 0) d = l1 - 1;
                else { for (j = 0; j < l1; j++) if (p1[j] == p2[0]) d = j; d += l2 - 1; }
                gb.want[h] = (int8_t)((d & 1) ? -1 : 1);
            }
            for (i = 0; i < GB_N; i++) if (used[i] && !gb.fixed[i]) need[sz[i]]++;
        }
        /* rusted large gears, away from the solution (they block other routes) */
        placed = 0;
        for (k = 0; k < 60 && placed < prm_pick(P_GEARBOARD_RUSTED_LO, P_GEARBOARD_RUSTED_HI); k++) {
            int32_t c = (int32_t)rndn(GB_N), j, ok = !used[c] && !gb.g[c];
            n = gb_nb(c, nb);
            for (j = 0; j < n && ok; j++) if (used[nb[j]] || gb.fixed[nb[j]]) ok = 0;
            if (ok) { gb.g[c] = GB_L; gb.fixed[c] = 3; placed++; }
        }
        /* check it: the solution in place must turn every hand the right way */
        {
            int ok = 1;
            for (i = 0; i < GB_N; i++) { gb.sol[i] = (uint8_t)((used[i] && !gb.fixed[i]) ? sz[i] : 0); if (gb.sol[i]) gb.g[i] = gb.sol[i]; }
            gb_drive();
            for (k = 0; k < gb.nhands; k++) if (gb.dir[gb.hand[k]] != gb.want[gb.hand[k]]) ok = 0;
            if (gb.jam) ok = 0;
            for (i = 0; i < GB_N; i++) if (gb.sol[i]) gb.g[i] = 0;
            if (!ok) continue;
        }
        for (i = 1; i < 4; i++) gb.stock[i] = need[i];
        for (k = prm_pick(P_GEARBOARD_SPARES_LO, P_GEARBOARD_SPARES_HI); k > 0; k--) gb.stock[chain == 1 ? GB_M : (rndn(2) ? GB_S : GB_L)]++;
        if (chain == 1) gb.stock[GB_S] += (int32_t)rndn(2);   /* a gear of the wrong kind, now and then */
        break;
    }
    gb.cur = gb.motor + 1; gb.ok_since = 0; gb.fin_t = 0; gb.motor_ang = 0;
    gb_drive();
}
static int gb_update(void)
{
    int32_t e;
    int ok = 1, k;
    led_btn_mask = BIT(BTN_EDIT);
    if ((e = in.enc[ENC_KNOB1] + in.enc[ENC_SELECT]) != 0) { gb.cur = (gb.cur / GB_C) * GB_C + clampi(gb.cur % GB_C + e, 0, GB_C - 1); sfx_click(0, 25, 128); }
    if ((e = in.enc[ENC_KNOB2]) != 0) { gb.cur = clampi(gb.cur / GB_C + e, 0, GB_R - 1) * GB_C + gb.cur % GB_C; sfx_click(0, 25, 128); }
    if ((e = in.enc[ENC_KNOB3]) != 0 && !gb.fixed[gb.cur]) {   /* the next size that is in stock and fits */
        int32_t s = gb.g[gb.cur], tries;
        if (s) gb.stock[s]++;
        for (tries = 0; tries < 4; tries++) {
            s = (s + (e > 0 ? 1 : 3)) % 4;
            if (s == GB_EMPTY) break;
            if (gb.stock[s] > 0 && gb_can_place(gb.cur, s)) break;
        }
        if (s && gb.stock[s] > 0 && gb_can_place(gb.cur, s)) { gb.stock[s]--; gb.g[gb.cur] = (uint8_t)s; sfx_thunk(0, 70 - s * 6, 40, 128); }
        else { gb.g[gb.cur] = GB_EMPTY; sfx_click(0, 50, 128); if (tries >= 4) gb.bump_t = fm1_ms; }
        gb_drive();
        if (gb.jam) sfx_buzz(0);
    } else if (e && gb.fixed[gb.cur]) { gb.bump_t = fm1_ms; sfx_thunk(0, 44, 40, 128); }
    for (k = 0; k < gb.nhands; k++) if (gb.dir[gb.hand[k]] != gb.want[gb.hand[k]]) ok = 0;
    if (ok) { if (!gb.ok_since) { gb.ok_since = fm1_ms ? fm1_ms : 1u; sfx_chime(79, 60, 0, 200); } if (fm1_ms - gb.ok_since > 1200u) return 1; }
    else gb.ok_since = 0;
    return 0;
}
static void gb_draw(uint32_t won)
{
    int32_t i, k;
    uint32_t spd = won ? 700u : 260u;
    if (won && !gb.fin_t) gb.fin_t = fm1_ms;
    gb.motor_ang += spd;
    g_bg(BG_GEARS);
    g_sigil(SW - 18, STAGE_Y0 + 16, 9, C_AMBER);   /* the line's mark, stamped on the board */
    g_dim(0, 22, SW, 200, 140);
    for (i = 0; i < GB_N; i++) {                /* the honeycomb: a faint line to each neighbour (the meshing distance) */
        int32_t nb[6], n = gb_nb(i, nb), k2;
        for (k2 = 0; k2 < n; k2++) if (nb[k2] > i) g_line(gb_x(i), gb_y(i), gb_x(nb[k2]), gb_y(nb[k2]), RGB(58, 44, 28));
    }
    for (i = 0; i < GB_N; i++) {                /* the pegs; the picked one ringed */
        int32_t x = gb_x(i), y = gb_y(i);
        g_disc(x, y, 4, C_BRASS);
        g_disc(x, y, 2, C_BRASS_DD);
        if (i == gb.cur && !won) g_ring(x, y, 13, 15, fm1_ms - gb.bump_t < 300u ? C_RUBY : C_GLOW);
    }
    for (i = 0; i < GB_N; i++) {
        int32_t x = gb_x(i), y = gb_y(i), r = GB_HALF[gb.g[i]] * 5;
        uint16_t face, dark;
        if (!gb.g[i]) continue;
        if (gb.dir[i]) gb.ang[i] = (uint32_t)((int32_t)gb.motor_ang * GB_HALF[gb.g[gb.motor]] / GB_HALF[gb.g[i]]) * (uint32_t)gb.dir[i] + (uint32_t)i * 5000u;
        face = gb.fixed[i] == 3 ? RGB(150, 90, 60) : gb.fixed[i] == 1 ? C_COPPER : gb.fixed[i] == 2 ? C_STEEL_L : C_GOLD;
        dark = gb.fixed[i] == 3 ? RGB(100, 56, 36) : C_BRASS_D;
        if (gb.jam && !gb.fixed[i]) face = mix565(face, C_RUBY, (fm1_ms / 150u) & 1u ? 160u : 60u);
        g_gear(x, y, r, (uint32_t)(GB_HALF[gb.g[i]] * 4), gb.ang[i], face, dark, C_BRASS_L, r >= 15 ? 4 : 0);
        if (gb.fixed[i] == 3) { g_line(x - 6, y - 6, x + 6, y + 6, RGB(80, 40, 30)); g_line(x - 6, y + 6, x + 6, y - 6, RGB(80, 40, 30)); }
        if (gb.fixed[i] == 1) g_hand(x, y, r - 2, 0, gb.motor_ang, 3, C_CREAM);   /* the motor's crank */
        if (gb.fixed[i] == 2) {                 /* a hand, and the arrow of the way it must turn */
            int ok = gb.dir[i] == gb.want[i];
            uint32_t a0 = 49152u - 9000u, a1 = 49152u + 9000u;
            g_hand(x, y, r + 6, 0, gb.ang[i] - 16384u, 2, C_CREAM);
            g_ring(x, y, r + 9, r + 10, ok ? C_VERD : C_ROSE);
            {
                uint32_t tip = gb.want[i] > 0 ? a1 : a0, a = tip + (gb.want[i] > 0 ? (uint32_t)-2400 : 2400u);
                g_tri16(px16(x, (r + 7) * 16, a), py16(y, (r + 7) * 16, a), px16(x, (r + 13) * 16, a), py16(y, (r + 13) * 16, a),
                        px16(x, (r + 10) * 16, tip), py16(y, (r + 10) * 16, tip), ok ? C_VERD : C_ROSE);
            }
        }
    }
    /* the stock: small, medium, large */
    g_panel(4, 190, 232, 30, C_GOLD, 0);
    for (k = 1; k < 4; k++) {
        char t[8], *p = t;
        int32_t x = 18 + (k - 1) * 74;
        g_gear(x + 10, 205, GB_HALF[k] * 3, (uint32_t)(GB_HALF[k] * 4), 0, gb.stock[k] ? C_GOLD : RGB(70, 60, 40), C_BRASS_D, C_BRASS_L, 0);
        p = str_cat(p, k == 1 ? "S x" : k == 2 ? "M x" : "L x"); fmt_u(p, (uint32_t)gb.stock[k]);
        g_text(x + 26, 197, &FONT_S, t, gb.stock[k] ? C_CREAM : C_PAPER3);
    }
    if (gb.jam && !won) { g_rect(70, 26, 100, 18, C_LACQ); g_text_c(120, 27, &FONT_S, T_GB_JAM, C_ROSE); }
    for (k = 0; k < 2; k++) {                   /* key lights: C (and D) when a hand turns the right way */
        static const uint8_t HK[2] = {7, 9};
        fm1_led_key_glow(KEY_LED(HK[k]), k < gb.nhands);
        fm1_led_key(KEY_LED(HK[k]), k < gb.nhands && gb.dir[gb.hand[k]] == gb.want[gb.hand[k]]);
    }
}
static void gb_leave(void) { ui_keys_clear(); }
static const game_t G_GEARBOARD = {BIT(BTN_EDIT), 1600, gb_enter, gb_update, gb_draw, gb_leave};
