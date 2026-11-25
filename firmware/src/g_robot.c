/* SPDX-License-Identifier: GPL-3.0-only */
/* The daily robots: the clients Tobias mends every day. Each model's faults sit on one or more parts of the
 * FM-1 itself, and the FM-1's own lights help:
 *
 *   CALIBRATION  (SELECT, PRESETS, ALGORITHM, OCT- OCT+)  three gauges, each turned by its dial; SELECT also drags
 *                PRESETS' gauge and PRESETS drags ALGORITHM's (up + or down -, drawn): set them in that order. OCT- / OCT+ nudge all three at once; their LEDs say
 *                which way the gauges as a whole are off (lit OCT+ = too low).
 *   RELAYS       (K1..K4, the buttons FX..GLO and HOME..REC)  twelve relays in two rows of six, on the twelve
 *                buttons: a press flips the relay and its neighbours (left, right, the one above or below). The
 *                button LEDs ARE the relays: light them all. K1..K4 tune four meters; a lamp warms as each nears.
 *   VOICE BOX    (the keys)  a code of notes on the white keys C..B. Play a guess; then the keys answer:
 *                BRIGHT = right note, right place; GLOWING = in the code, elsewhere; dark = not in it.
 *
 * layout (puzzles.txt): 1 calibration, 2 relays, 3 voice box, 4 = 1+2, 5 = 1+3, 6 = 2+3, 7 all three.
 * A range (1..7) picks one each time. In the Workshop (title screen) the robots come one after another. */

#define RB_CAL 1u
#define RB_REL 2u
#define RB_VOX 4u
static const uint8_t RB_PARTS[8] = {0, RB_CAL, RB_REL, RB_VOX, RB_CAL | RB_REL, RB_CAL | RB_VOX, RB_REL | RB_VOX, 7};
/* the relays: two rows of six buttons, as they sit on the panel */
static const uint8_t RB_BTN[12] = {BTN_FX, BTN_SEL, BTN_ENV, BTN_LFO, BTN_EDIT, BTN_GLO,
                                   BTN_HOME, BTN_SAVE, BTN_ARP, BTN_SEQ, BTN_PLAY, BTN_REC};
static const uint8_t RB_VKEY[7] = {7, 9, 11, 12, 14, 16, 18};   /* C4 .. B4 */
static const uint8_t RB_VNOTE[7] = {72, 74, 76, 77, 79, 81, 83};

static int32_t rb_force_layout;                 /* the Workshop sets the model's layout (0: the chapter's setting) */
static struct {
    uint32_t parts, done;                       /* subsystems this robot has; those already working */
    int32_t g[3], t[3], link[3];                /* calibration: gauges, targets, how each dial drags the next */
    uint32_t relay;                             /* 12 bits, 1 = closed (lit) */
    int32_t m[4], h[4];                         /* meters, their hidden targets */
    uint8_t code[5], guess[5], fb[5];           /* voice box; feedback 2 right place, 1 elsewhere, 0 none */
    int32_t clen, glen, tries;
    uint8_t hist[4][5], histfb[4][5]; int32_t nhist;
    uint32_t fb_t;                              /* when the last guess was answered (the key lights show it) */
    uint32_t seed, look;                        /* the robot's looks */
    uint32_t done_t[3], fin_t;
    char name[24];
} rb;

static void rb_relay_flip(int i)
{
    int r = i / 6, c = i % 6;
    rb.relay ^= 1u << i;
    if (c > 0) rb.relay ^= 1u << (i - 1);
    if (c < 5) rb.relay ^= 1u << (i + 1);
    rb.relay ^= 1u << (r ? i - 6 : i + 6);
}
static void rb_vox_key(uint32_t k)
{
    uint32_t i;
    if (!(rb.parts & RB_VOX) || (rb.done & RB_VOX)) return;
    for (i = 0; i < 7u; i++) if (RB_VKEY[i] == k) sfx_tine(RB_VNOTE[i], 100, 0, (uint8_t)(60u + i * 20u), 8);
}
static void rb_name(void)
{
    static const char *const KIND[8] = {"WASHER", "SWEEPER", "PORTER", "LOOM", "PRESS", "SORTER", "WELDER", "OILER"};
    char *p = rb.name;
    p = str_cat(p, KIND[rb.seed % 8u]);
    *p++ = ' '; *p++ = (char)('A' + (rb.seed >> 3) % 26u); *p++ = '-';
    fmt_u(p, 10u + (rb.seed >> 8) % 90u);
}
static void rb_enter(void)
{
    uint32_t i, j, n;
    int32_t lay = rb_force_layout ? rb_force_layout : prm_pick(P_ROBOT_LAYOUT_LO, P_ROBOT_LAYOUT_HI);
    rb.parts = RB_PARTS[clampi(lay, 1, 7)];
    rb.done = 0; rb.fin_t = 0; rb.seed = rnd(); rb.look = rnd();
    rb_name();
    /* calibration: targets, links, then scrambled from the solution by random turns */
    for (i = 0; i < 3u; i++) { rb.t[i] = 4 + (int32_t)rndn(13); rb.g[i] = rb.t[i]; rb.link[i] = rndn(2) ? 1 : -1; }
    rb.link[2] = 0;                             /* the chain runs SELECT -> PRESETS -> ALGORITHM, and stops */
    n = (uint32_t)PRM(P_ROBOT_CAL_TURNS);
    for (j = 0; j < n; j++) {
        uint32_t d = rndn(3);
        int32_t s = rndn(2) ? 1 : -1;
        rb.g[d] += s; rb.g[(d + 1u) % 3u] += s * rb.link[d];
    }
    for (i = 0; i < 3u; i++) rb.g[i] = clampi(rb.g[i], 0, 20);
    /* relays: all closed, then the given number of random presses */
    rb.relay = 0xFFFu;
    n = (uint32_t)prm_pick(P_ROBOT_RELAY_PRESSES_LO, P_ROBOT_RELAY_PRESSES_HI);
    for (j = 0; j < n; j++) rb_relay_flip((int)rndn(12));
    if (rb.relay == 0xFFFu) rb_relay_flip(3);
    for (i = 0; i < 4u; i++) { rb.h[i] = 3 + (int32_t)rndn(25); do rb.m[i] = (int32_t)rndn(31); while (iabs(rb.m[i] - rb.h[i]) < 6); }
    /* voice box: distinct notes */
    rb.clen = prm_pick(P_ROBOT_CODE_LEN_LO, P_ROBOT_CODE_LEN_HI);
    for (i = 0; i < (uint32_t)rb.clen; i++) {
        uint8_t c;
        do { c = (uint8_t)rndn(7); for (j = 0; j < i; j++) if (rb.code[j] == c) break; } while (j < i);
        rb.code[i] = c;
    }
    rb.glen = 0; rb.tries = 0; rb.nhist = 0; rb.fb_t = 0;
    kb_hook = rb_vox_key;
}
static void rb_part_done(uint32_t part, int idx)
{
    rb.done |= part;
    rb.done_t[idx] = fm1_ms;
    sfx_chime(76 + idx * 4, 80, 0, 128);
    sfx_chime(83 + idx * 4, 70, au_time() + ms_frames(120), 128);
}
static int rb_update(void)
{
    static const uint8_t CK[3] = {ENC_SELECT, ENC_PRESETS, ENC_ALGORITHM};
    static const uint8_t MK[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
    uint32_t i, k;
    led_btn_mask = 0;
    if ((rb.parts & RB_CAL) && !(rb.done & RB_CAL)) {
        int32_t sum = 0, tsum = 0, ok = 1, d;
        for (i = 0; i < 3u; i++) {
            if (!(d = in.enc[CK[i]])) continue;
            rb.g[i] = clampi(rb.g[i] + d, 0, 20);
            rb.g[(i + 1u) % 3u] = clampi(rb.g[(i + 1u) % 3u] + d * rb.link[i], 0, 20);
            sfx_click(0, 40, (uint8_t)(60u + i * 60u));
        }
        if (PRESSED(BTN_OCT_DOWN)) { for (i = 0; i < 3u; i++) rb.g[i] = clampi(rb.g[i] - 1, 0, 20); sfx_thunk(0, 60, 30, 128); }
        if (PRESSED(BTN_OCT_UP)) { for (i = 0; i < 3u; i++) rb.g[i] = clampi(rb.g[i] + 1, 0, 20); sfx_thunk(0, 66, 30, 128); }
        for (i = 0; i < 3u; i++) { sum += rb.g[i]; tsum += rb.t[i]; if (rb.g[i] != rb.t[i]) ok = 0; }
        if (sum > tsum) led_btn_mask |= BIT(BTN_OCT_DOWN);   /* the lights point the way */
        if (sum < tsum) led_btn_mask |= BIT(BTN_OCT_UP);
        if (ok) rb_part_done(RB_CAL, 0);
    }
    if ((rb.parts & RB_REL) && !(rb.done & RB_REL)) {
        int ok = 1;
        for (i = 0; i < 12u; i++) if (PRESSED(RB_BTN[i])) { rb_relay_flip((int)i); sfx_click(0, 70, (uint8_t)(40u + (i % 6u) * 35u)); sfx_tick(0, 70 + (int32_t)(i % 6u) * 2, 30, 128, 0); }
        for (i = 0; i < 4u; i++) {
            int32_t d = in.enc[MK[i]];
            if (d) { rb.m[i] = clampi(rb.m[i] + d, 0, 30); sfx_click(0, 25, (uint8_t)(60u + i * 40u)); }
            if (rb.m[i] != rb.h[i]) ok = 0;
        }
        for (i = 0; i < 12u; i++) if (rb.relay >> i & 1u) led_btn_mask |= BIT(RB_BTN[i]);   /* the LEDs are the relays */
        if (rb.relay != 0xFFFu) ok = 0;
        if (ok) rb_part_done(RB_REL, 1);
    }
    if ((rb.parts & RB_VOX) && !(rb.done & RB_VOX)) {
        for (k = 0; k < 27u; k++) {
            if (!(in.kdown >> k & 1u)) continue;
            for (i = 0; i < 7u; i++) if (RB_VKEY[i] == k) break;
            if (i == 7u) continue;
            if (fm1_ms - rb.fb_t < 2600u) rb.fb_t = 0;   /* a new guess clears the last answer */
            rb.guess[rb.glen++] = (uint8_t)i;
            if (rb.glen == rb.clen) {           /* the guess is answered: right place, elsewhere, none */
                int32_t right = 0, j2;
                for (j2 = 0; j2 < rb.clen; j2++) {
                    int32_t m2;
                    rb.fb[j2] = 0;
                    if (rb.guess[j2] == rb.code[j2]) { rb.fb[j2] = 2; right++; continue; }
                    for (m2 = 0; m2 < rb.clen; m2++) if (rb.code[m2] == rb.guess[j2]) rb.fb[j2] = 1;
                }
                if (rb.nhist == 4) {            /* the oldest guess scrolls away */
                    int32_t r2, c2;
                    for (r2 = 0; r2 < 3; r2++) for (c2 = 0; c2 < 5; c2++) { rb.hist[r2][c2] = rb.hist[r2 + 1][c2]; rb.histfb[r2][c2] = rb.histfb[r2 + 1][c2]; }
                    rb.nhist = 3;
                }
                for (j2 = 0; j2 < rb.clen; j2++) { rb.hist[rb.nhist][j2] = rb.guess[j2]; rb.histfb[rb.nhist][j2] = rb.fb[j2]; }
                rb.nhist++; rb.tries++;
                rb.glen = 0; rb.fb_t = fm1_ms;
                if (right == rb.clen) rb_part_done(RB_VOX, 2);
                else sfx_tick(au_time() + ms_frames(200), 60, 50, 128, 0);
            }
        }
    }
    return rb.done == rb.parts;
}
/* the voice box's answer, on the keys: bright = right place, glow = elsewhere (shown for a while after a guess) */
static void rb_key_leds(void)
{
    uint32_t i, j;
    for (i = 0; i < 7u; i++) {
        int on = 0, glow = 1;
        if ((rb.parts & RB_VOX) && rb.fb_t && fm1_ms - rb.fb_t < 2600u && rb.nhist) {
            glow = 0;
            for (j = 0; j < (uint32_t)rb.clen; j++)
                if (rb.hist[rb.nhist - 1][j] == i) { if (rb.histfb[rb.nhist - 1][j] == 2u) on = 1; else if (rb.histfb[rb.nhist - 1][j] == 1u) glow = 1; }
        }
        if (!(rb.parts & RB_VOX)) glow = 0;
        fm1_led_key(KEY_LED(RB_VKEY[i]), on);
        fm1_led_key_glow(KEY_LED(RB_VKEY[i]), glow);
    }
}
static void rb_draw_robot(int32_t cx, int32_t cy, int happy)
{
    uint32_t L = rb.look;
    uint16_t body = (L & 3u) == 0 ? C_STEEL : (L & 3u) == 1 ? RGB(150, 130, 100) : (L & 3u) == 2 ? RGB(120, 140, 120) : RGB(170, 110, 90);
    uint16_t eye = happy ? C_CYAN : (fm1_ms / 600u) & 1u ? C_RUBY : RGB(90, 30, 30);
    int32_t w = 30 + (int32_t)(L >> 2 & 7u) * 2;
    if ((L >> 5) & 1u) { g_rect(cx - w / 2, cy - 18, w, 30, C_STEEL_D); g_rect(cx - w / 2 + 2, cy - 16, w - 4, 26, body); }
    else { g_disc(cx, cy - 3, w / 2, C_STEEL_D); g_disc(cx, cy - 3, w / 2 - 2, body); }
    if ((L >> 6) & 1u) { g_led(cx - 8, cy - 4, 3, eye); g_led(cx + 8, cy - 4, 3, eye); }
    else { g_rect(cx - 10, cy - 6, 20, 5, C_NIGHT); g_rect(cx - 9, cy - 5, 18, 3, eye); }
    g_line(cx, cy - w / 2 - 4, cx, cy - w / 2 - 10, C_INK2);
    g_led(cx, cy - w / 2 - 11, 2, happy ? C_VERD : C_AMBER);
    g_rect(cx - 6, cy + 4, 12, 3, C_NIGHT);
    if (happy) g_rect(cx - 6, cy + 4, 12, 3, C_CYAN_D);
}
static void rb_draw(uint32_t won)
{
    int32_t y = 72, i, part = 0;
    if (won && !rb.fin_t) rb.fin_t = fm1_ms;
    g_bg(BG_GEARS);                             /* the workshop bench, dim */
    g_dim(0, 22, SW, 200, 150);
    rb_draw_robot(42, 46, won != 0);
    g_text(76, 26, &FONT_S, rb.name, C_CREAM);
    {   /* its parts' lamps (the panels below are in the same order, edged in the same colours) */
        static const uint16_t PC[3] = {C_CYAN, C_ROSE, C_AMBER};
        int32_t x = 80;
        for (i = 0; i < 3; i++) {
            if (!(rb.parts >> i & 1u)) continue;
            g_disc(x, 52, 5, PC[i]);
            if (rb.done >> i & 1u) g_led(x, 52, 3, C_VERD); else g_disc(x, 52, 3, C_RUBY);
            x += 16;
        }
    }
    for (part = 0; part < 3; part++) {
        if (!(rb.parts >> part & 1u)) continue;
        g_panel(4, y, 232, 48, (rb.done >> part & 1u) ? C_VERD : part == 0 ? C_CYAN : part == 1 ? C_ROSE : C_AMBER, 0);
        if (part == 0) {                        /* three gauges, their target wedges, the links between */
            for (i = 0; i < 3; i++) {
                int32_t gx = 44 + i * 76, gy = y + 26;
                uint32_t ta = 32768u + (uint32_t)(rb.t[i] * 32768 / 20), ga = 32768u + (uint32_t)(rb.g[i] * 32768 / 20);
                g_disc(gx, gy, 18, RGB(8, 20, 32));
                g_ring(gx, gy, 16, 18, C_CYAN_D);
                g_thick16(px16(gx, 11 * 16, ta) + 8, py16(gy, 11 * 16, ta) + 8, px16(gx, 17 * 16, ta) + 8, py16(gy, 17 * 16, ta) + 8, 40, C_VERD);
                g_hand(gx, gy, 15, 3, ga, 2, rb.g[i] == rb.t[i] ? C_VERD : C_ROSE);
                g_disc(gx, gy, 2, C_CREAM);
                {                               /* the link: this dial drags the next gauge, up (+) or down (-) */
                    int32_t nx = 44 + ((i + 1) % 3) * 76;
                    if (i < 2) { g_line(gx + 20, gy - 12, nx - 20, gy - 12, C_CYAN_D); g_text(gx + 30, y + 4, &FONT_S, rb.link[i] > 0 ? "+" : "-", C_CYAN); }
                    (void)nx;
                }
            }
            g_text(8, y + 34, &FONT_S, "S", C_INK2); g_text(84, y + 34, &FONT_S, "P", C_INK2); g_text(160, y + 34, &FONT_S, "A", C_INK2);
        } else if (part == 1) {                 /* the relays as the buttons sit, and four warmth meters */
            for (i = 0; i < 12; i++) {
                int32_t rx = 12 + (i % 6) * 18, ry = y + 10 + (i / 6) * 18;
                if (rb.relay >> i & 1u) g_led(rx + 6, ry + 6, 5, C_ROSE); else { g_disc(rx + 6, ry + 6, 5, C_ROSE_D); g_disc(rx + 6, ry + 6, 3, C_NIGHT); }
            }
            for (i = 0; i < 4; i++) {
                int32_t mx = 132 + i * 25, d = iabs(rb.m[i] - rb.h[i]);
                uint32_t warm = d >= 12 ? 0u : (uint32_t)(12 - d) * 21u;
                g_rrect(mx, y + 8, 8, 32, 2, RGB(8, 20, 32));
                g_rect(mx, y + 40 - rb.m[i], 8, 2, C_CREAM);
                if (d == 0) g_led(mx + 4, y + 44, 2, C_VERD);
                else g_disc(mx + 4, y + 44, 2, mix565(RGB(30, 20, 20), C_AMBER, warm));
                {
                    char t[3] = {'K', (char)('1' + i), 0};
                    g_text(mx + 9, y + 6, &FONT_S, t, C_INK2);
                }
            }
        } else {                                /* the voice box: guesses so far, their answers as pegs */
            int32_t r, j;
            for (r = 0; r < rb.nhist; r++)
                for (j = 0; j < rb.clen; j++) {
                    int32_t px = 12 + r * 56 + j * 10, py = y + 14;
                    char t[2] = {"CDEFGAB"[rb.hist[r][j]], 0};
                    g_text(px, py, &FONT_S, t, C_CREAM);
                    if (rb.histfb[r][j] == 2u) g_led(px + 3, py + 22, 3, C_AMBER);          /* solid: right note, right place */
                    else if (rb.histfb[r][j] == 1u) g_ring(px + 3, py + 22, 2, 4, C_AMBER);  /* ring: in the code, elsewhere */
                    else g_disc(px + 3, py + 22, 1, C_PAPER3);                             /* dot: not in it */
                }
            for (j = 0; j < rb.clen; j++) g_rect(170 + j * 12, y + 36, 9, 2, j < rb.glen ? C_AMBER : C_PAPER3);
            if (!rb.nhist) g_text(12, y + 16, &FONT_S, T_RB_VOX_HINT, C_INK2);
        }
        if (rb.done >> part & 1u) g_rect_a(6, y + 2, 228, 44, C_NIGHT, 90);
        y += 50;
    }
    if (won) {
        uint32_t el = fm1_ms - rb.fin_t;
        if (el < 2500u) { g_rrect(56, 198, 128, 19, 5, C_PANEL2); g_text_c(120, 200, &FONT_S, T_RB_OK, C_CYAN); }
    }
    rb_key_leds();
}
static void rb_leave(void) { kb_hook = 0; au_cut(8); ui_keys_clear(); }
static const game_t G_ROBOT = {0, 1600, rb_enter, rb_update, rb_draw, rb_leave, 1};
