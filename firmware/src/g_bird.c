/* SPDX-License-Identifier: GPL-3.0-only */
/* Chapter IV: THE BIRD IN THE CAGE. Answer a singing automaton.
 *
 * A brass bird sings a phrase on five notes (the five lit keys) and waits, head cocked. Sing it back on the
 * keys. A wrong note: it shakes its head and sings the phrase again. Four songs, each a note longer (3..6);
 * in the first the keys light with its notes, after that only the ear tells. PLAY asks it to sing again.
 * Phrases are new each time: a melodic walk that never leaps more than two notes. */

static const uint8_t BD_KEY[5] = {7, 9, 11, 14, 16};   /* C4 D4 E4 G4 A4 */
#define BD_NOTE bd.note                         /* puzzles.txt notes */
#define BD_STEP_MS 400u
#define BD_ROUNDS bd.rounds

static struct {
    uint8_t phrase[12], len, round, got;
    uint8_t note[5], rounds, first_len, show;
    uint8_t state;                              /* 0 sing, 1 listen, 2 shake, 3 happy */
    uint32_t state_t;                           /* ms */
    uint32_t sing_at;                           /* frame time the phrase starts */
    uint32_t press_t[5];
    uint32_t beak_t;                            /* the player's note: the bird echoes with its beak */
    uint32_t fin_t;
    uint8_t started;
    uint8_t misses;                             /* wrong answers to this song: after two, the next key glows */                            /* it first sings when play begins, not under the card */
} bd;

static void bd_key(uint32_t k)
{
    uint32_t i;
    for (i = 0; i < 5u; i++)
        if (BD_KEY[i] == k) { sfx_tine(BD_NOTE[i], 100, 0, 150, 3); bd.press_t[i] = fm1_ms;   /* the same notes the bird sings */ }
}

static void bd_new_phrase(void)
{
    uint32_t i;
    int32_t n = (int32_t)rndn(5);
    bd.len = (uint8_t)(bd.first_len + bd.round);
    for (i = 0; i < bd.len; i++) {
        int32_t step;
        bd.phrase[i] = (uint8_t)n;
        do { step = (int32_t)rndn(5) - 2; } while (n + step < 0 || n + step > 4 || (i && step == 0 && rndn(2)));
        n += step;
    }
}
static void bd_sing(uint32_t delay_ms)
{
    uint32_t i, t0 = au_time() + ms_frames(delay_ms);
    au_cut(3);
    for (i = 0; i < bd.len; i++) sfx_tine(BD_NOTE[bd.phrase[i]], 100, t0 + ms_frames(i * BD_STEP_MS), 150, 3);
    bd.sing_at = t0;
    if (bd.state == 3 || !bd.started) bd.misses = 0;
    bd.state = 0; bd.state_t = fm1_ms; bd.got = 0;
}
static void bd_enter(void)
{
    uint32_t i;
    for (i = 0; i < 5u; i++) bd.note[i] = (uint8_t)LST(L_BIRD_NOTES)->v[i];
    bd.rounds = (uint8_t)PRM(P_BIRD_ROUNDS); bd.first_len = (uint8_t)PRM(P_BIRD_FIRST_LEN); bd.show = (uint8_t)PRM(P_BIRD_SHOW_ROUNDS);
    bd.round = 0; bd.fin_t = 0; bd.started = 0; bd.state = 1; bd.got = 0;
    bd_new_phrase();
    kb_hook = bd_key;
}

static int32_t bd_singing_note(void)            /* the phrase note sounding now, or -1 */
{
    int32_t el = (int32_t)(au_time() - bd.sing_at), i;
    if (bd.state != 0 || el < 0) return -1;
    i = el / (int32_t)ms_frames(BD_STEP_MS);
    if (i >= bd.len || el % (int32_t)ms_frames(BD_STEP_MS) > (int32_t)ms_frames(300)) return -1;
    return bd.phrase[i];
}

static int bd_update(void)
{
    uint32_t k, i;
    led_btn_mask = BIT(BTN_EDIT) | BIT(BTN_PLAY);
    if (!bd.started) { bd.started = 1; bd_sing(500); }
    if (bd.state == 0 && (int32_t)(au_time() - bd.sing_at) > (int32_t)ms_frames(bd.len * BD_STEP_MS)) {
        bd.state = 1; bd.state_t = fm1_ms;
    }
    if (bd.state == 2 && fm1_ms - bd.state_t > 900u) bd_sing(200);
    if (bd.state == 3 && fm1_ms - bd.state_t > 1400u) {
        bd.round++;
        if (bd.round >= BD_ROUNDS) return 1;
        bd_new_phrase();
        bd_sing(300);
    }
    if (PRESSED(BTN_PLAY) && (bd.state == 1 || bd.state == 0)) bd_sing(150);
    if (bd.state == 1) {
        for (k = 0; k < 27u; k++) {
            if (!(in.kdown >> k & 1u)) continue;
            for (i = 0; i < 5u; i++) if (BD_KEY[i] == k) break;
            if (i == 5u) continue;
            bd.beak_t = fm1_ms;
            if (bd.phrase[bd.got] == i) {
                if (++bd.got == bd.len) {          /* the whole phrase: a happy trill */
                    uint32_t t = au_time() + ms_frames(260), j;
                    for (j = 0; j < 4u; j++) sfx_tine(BD_NOTE[4u - (j & 1u)] + 12, 60, t + ms_frames(j * 75u), 150, 3);
                    bd.state = 3; bd.state_t = fm1_ms;
                    break;
                }
            } else {                            /* wrong: it shakes its head */
                sfx_buzz(au_time() + ms_frames(120));
                if (bd.misses < 9u) bd.misses++;
                bd.state = 2; bd.state_t = fm1_ms;
                break;
            }
        }
    }
    return 0;
}

static void bd_draw_bird(int32_t cx, int32_t cy, int32_t tilt, uint32_t beak_open, uint32_t flap)
{
    uint32_t ha = (uint32_t)(-6000 + tilt);     /* head direction */
    int32_t hx = cx + 16 + (tilt / 1200), hy = cy - 20;
    /* tail */
    {
        int32_t q[8] = {(cx - 14) * 16, (cy + 2) * 16, (cx - 44) * 16, (cy + 26) * 16, (cx - 40) * 16, (cy + 34) * 16, (cx - 8) * 16, (cy + 10) * 16};
        g_quad16(q, C_STEEL_D);
        g_thick(cx - 16, cy + 8, cx - 40, cy + 29, 2, C_STEEL_L);
    }
    /* body: brass, a breast lighter */
    g_disc(cx, cy, 20, C_BRASS_D);
    g_disc(cx, cy, 18, C_BRASS);
    g_disc(cx + 6, cy + 4, 11, C_BRASS_L);
    /* wing, raised by flap (0..256) */
    {
        int32_t lift = (int32_t)flap * 22 / 256;
        int32_t q[8] = {(cx - 10) * 16, (cy - 6) * 16, (cx + 10) * 16, (cy - 8) * 16, (cx + 2) * 16, (cy + 10 - lift) * 16, (cx - 30) * 16, (cy + 6 - lift * 2) * 16};
        g_quad16(q, C_STEEL);
        g_thick(cx - 8, cy - 4, cx - 26, cy + 4 - lift * 2, 2, C_STEEL_L);
        g_thick(cx - 4, cy, cx - 20, cy + 8 - lift * 2, 2, C_STEEL_L);
    }
    /* head, eye, beak */
    g_disc(hx, hy, 12, C_BRASS_D);
    g_disc(hx, hy, 11, C_BRASS);
    g_disc(hx + 4, hy - 3, 3, C_INK);
    px_set(hx + 5, hy - 4, C_PAPER);
    {
        int32_t bx = px16(hx, 10 * 16, ha), by = py16(hy, 10 * 16, ha);
        int32_t tx = px16(hx, 26 * 16, ha), ty = py16(hy, 26 * 16, ha);
        uint32_t op = beak_open * 2600u / 256u;
        int32_t ux = px16(hx, 24 * 16, ha - op), uy = py16(hy, 24 * 16, ha - op);
        int32_t lx = px16(hx, 22 * 16, ha + op + 600u), ly = py16(hy, 22 * 16, ha + op + 600u);
        g_tri16(bx, by - 40, ux, uy, bx + 30, by + 10, C_COPPER);
        g_tri16(bx, by + 30, lx, ly, bx + 30, by, RGB(140, 80, 52));
        (void)tx; (void)ty;
    }
    /* the crest: three blued feathers */
    g_thick(hx - 2, hy - 10, hx - 8, hy - 20, 2, C_STEEL);
    g_thick(hx + 1, hy - 11, hx - 1, hy - 23, 2, C_STEEL);
    g_thick(hx + 4, hy - 10, hx + 6, hy - 20, 2, C_STEEL);
    /* legs on the perch */
    g_thick(cx - 2, cy + 18, cx - 4, cy + 30, 2, C_INK2);
    g_thick(cx + 6, cy + 18, cx + 6, cy + 30, 2, C_INK2);
}

static void bd_draw(uint32_t won)                /* the key cabinet: five brass keys on hooks, each ringing a note; the lock listens */
{
    static const uint16_t KC[5] = {C_CYAN, C_ROSE, C_AMBER, C_VERD, C_GOLD};
    int32_t note = bd_singing_note(), i, cx = 120;
    uint32_t door = 0;
    if (won) {
        if (!bd.fin_t) {                        /* opened: the phrase once more, then the doors part */
            uint32_t t0 = au_time() + ms_frames(200), j;
            static const uint8_t SONG[8] = {0, 2, 4, 3, 1, 2, 3, 4};
            bd.fin_t = fm1_ms;
            au_cut(3);
            for (j = 0; j < 8u; j++) sfx_tine(BD_NOTE[SONG[j]], 90, t0 + ms_frames(j * 200u), 150, 3);
        }
        { uint32_t el = fm1_ms - bd.fin_t; door = el > 1700u ? (el - 1700u > 900u ? 256u : (el - 1700u) * 256u / 900u) : 0u; }
    }
    g_cab_wall(cx, 120, 0);
    /* the cabinet: dark wood, a brass rail, five hooks */
    g_rrect(22, 34, SW - 44, 170, 6, RGB(48, 24, 30));
    g_rframe(22, 34, SW - 44, 170, 6, 2, C_BRASS_D);
    g_rect(34, 58, SW - 68, 4, C_BRASS);
    for (i = 0; i < 5; i++) {                   /* each key: it swings and glows while it rings */
        int32_t hx = 52 + i * 34, sw = 0, lit = note == i || fm1_ms - bd.press_t[i] < 180u || (won && door == 0u && ((fm1_ms - bd.fin_t) / 200u) % 5u == (uint32_t)i);
        uint16_t c = lit ? KC[i] : mix565(RGB(48, 24, 30), C_BRASS, 170);
        if (lit) sw = (int32_t)(mo_sin(fm1_ms * 900u) >> 12);
        g_disc(hx, 62, 3, C_BRASS_L);
        g_line(hx, 62, hx + sw, 92, c);
        g_ring(hx + sw, 100, 7, 9, c);          /* the bow */
        g_rect(hx + sw - 2, 109, 4, 52, c);     /* the shank */
        g_rect(hx + sw + 2, 146, 7, 4, c); g_rect(hx + sw + 2, 154, 5, 4, c);   /* the bit */
        if (lit) g_disc(hx + sw, 100, 4, mix565(c, RGB(255, 255, 255), 120));
        {   /* the key's letter: which key on the keyboard */
            static const char KN[5] = {'C', 'D', 'E', 'G', 'A'};
            char t[2] = {KN[i], 0};
            g_text_c(hx, 168, &FONT_S, t, lit ? KC[i] : C_INK2);
        }
    }
    if (door) {                                 /* the doors part on the line's key */
        int32_t dw = (SW - 44) / 2 * (256 - (int32_t)door) / 256;
        g_rect(22, 34, dw, 170, RGB(70, 36, 40)); g_rect(SW - 22 - dw, 34, dw, 170, RGB(70, 36, 40));
        if (door >= 256u) { g_ring(cx, 110, 10, 13, C_GOLD); g_rect(cx - 2, 123, 5, 46, C_GOLD); g_rect(cx + 3, 158, 9, 5, C_GOLD); }
    }
    if (!won) {                                 /* the phrase: a light a note, filled as you ring it back */
        int32_t x0 = SW / 2 - (int32_t)bd.len * 7;
        char t[16], *q = t;
        for (i = 0; i < bd.len; i++) {
            int filled = bd.state == 3 || (bd.state == 1 && i < bd.got);
            g_disc(x0 + i * 14 + 7, 44, 4, filled ? C_VERD : RGB(40, 30, 40));
            if (bd.state == 0 && note >= 0 && i == (int32_t)((au_time() - bd.sing_at) / ms_frames(BD_STEP_MS))) g_disc(x0 + i * 14 + 7, 44, 4, C_GOLD);
        }
        q = str_cat(q, T_BD_SONG " "); fmt_u(q, bd.round < bd.rounds ? bd.round + 1u : bd.rounds); while (*q) q++;
        q = str_cat(q, "/"); fmt_u(q, bd.rounds);
        g_text(30, 182, &FONT_S, t, C_GOLD);
        g_text_r(SW - 30, 182, &FONT_S, bd.state == 1 ? T_BD_YOUR_TURN : bd.state == 2 ? T_BD_AGAIN : "", C_CYAN);
    }
    for (i = 0; i < 5; i++) {                   /* the keyboard: the five keys glow; the one ringing lights */
        int on = fm1_ms - bd.press_t[i] < 150u || (bd.round < bd.show && note == i) || (bd.state == 1 && bd.misses >= 2u && bd.phrase[bd.got] == i && (fm1_ms / 300u) & 1u);
        fm1_led_key_glow(KEY_LED(BD_KEY[i]), 1);
        fm1_led_key(KEY_LED(BD_KEY[i]), on);
    }
}

static void bd_leave(void) { kb_hook = 0; au_cut(3); ui_keys_clear(); }
static const game_t G_BIRD = {BIT(BTN_EDIT) | BIT(BTN_PLAY), 3200,
                              bd_enter, bd_update, bd_draw, bd_leave};
