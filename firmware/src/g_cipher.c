/* SPDX-License-Identifier: GPL-3.0-only */
/* Chapter VII: THE MARGINS. Read a message enciphered on three wheels.
 *
 * A cipher disk: the fixed outer ring is the plain alphabet; inside it, three paper wheels, each its own
 * alphabet turned by its own amount. The message's letters are enciphered in turn by wheel I, II, III, I, II...
 * (their colour says which). Turn the wheels with K1..K3 and the decoding shows live underneath: when a wheel
 * sits right, every third letter falls into place. The spaces are kept, so short words are footholds ("THE",
 * "AT"): reasoning, not 26 x 26 x 26 guesses. When the whole message reads, it is solved. Message and turns are
 * drawn each time. */

/* the messages are puzzles.txt `message` lines (the chapter's strings) */
static const uint16_t CI_COL[3] = {C_CYAN, C_ROSE, C_AMBER};

static struct {
    const char *plain;
    char ciph[32];
    uint8_t wheel[32];                          /* which wheel enciphered each letter (255: a space) */
    int32_t key[3], turn[3];                    /* the hidden shifts; the wheels' turns now */
    int32_t shown[3];                           /* drawn rotation, Q8 */
    uint32_t solved_t;
} ci;

static char ci_dec(uint32_t i)                 /* the letter as the wheels stand now */
{
    int32_t w = ci.wheel[i], c;
    if (w == 255) return ' ';
    c = (ci.ciph[i] - 'A' - ci.turn[w]) % 26;
    if (c < 0) c += 26;
    return (char)('A' + c);
}
static void ci_enter(void)
{
    uint32_t i, n = 0;
    ci.plain = cur_ch->nstr ? cur_ch->str[rndn(cur_ch->nstr)] : "A";
    for (i = 0; i < 3u; i++) { ci.key[i] = prm_pick(P_CIPHER_SHIFT_LO, P_CIPHER_SHIFT_HI); ci.turn[i] = 0; ci.shown[i] = 0; }
    for (i = 0; ci.plain[i] && i < 31u; i++) {
        if (ci.plain[i] == ' ') { ci.ciph[i] = ' '; ci.wheel[i] = 255; continue; }
        ci.wheel[i] = (uint8_t)(n % 3u);
        ci.ciph[i] = (char)('A' + (ci.plain[i] - 'A' + ci.key[n % 3u]) % 26);
        n++;
    }
    ci.ciph[i] = 0;
    ci.solved_t = 0;
}
static int ci_update(void)
{
    static const uint8_t KN[3] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3};
    uint32_t i;
    int all = 1;
    led_btn_mask = BIT(BTN_EDIT);
    for (i = 0; i < 3u; i++) {
        int32_t e = in.enc[KN[i]] + (i == 0u ? in.enc[ENC_SELECT] : 0);
        if (!e) continue;
        ci.turn[i] = ((ci.turn[i] + e) % 26 + 26) % 26;
        sfx_click(0, 26, (uint8_t)(70u + i * 60u));
        sfx_tick(0, 96 - (int32_t)i * 5, 18, (uint8_t)(70u + i * 60u), 0);
    }
    for (i = 0; ci.plain[i]; i++) if (ci_dec(i) != ci.plain[i]) { all = 0; break; }
    if (all) {
        if (!ci.solved_t) ci.solved_t = fm1_ms;
        if (fm1_ms - ci.solved_t > 400u) return 1;
    } else ci.solved_t = 0;
    return 0;
}

static void ci_ring(int32_t cx, int32_t cy, int32_t r, int32_t rot_q8, uint16_t face, uint16_t ink, uint16_t rule)
{
    int32_t k;
    g_disc(cx, cy, r + 7, face);
    g_ring(cx, cy, r + 6, r + 7, rule);
    for (k = 0; k < 26; k++) {
        uint32_t a = (uint32_t)((k * 256 * 65536 / 26 + rot_q8 * 65536 / 26) / 256) - 16384u;
        char t[2] = {(char)('A' + k), 0};
        g_text_c(px16(cx, r * 16, a) >> 4, (py16(cy, r * 16, a) >> 4) - 8, &FONT_S, t, ink);
        g_line(px16(cx, (r + 6) * 16, a + 1260u) >> 4, py16(cy, (r + 6) * 16, a + 1260u) >> 4,
               px16(cx, (r - 6) * 16, a + 1260u) >> 4, py16(cy, (r - 6) * 16, a + 1260u) >> 4, rule);
    }
}
static void ci_draw(uint32_t won)
{
    {   /* a wheel set right: its letters turn gold, and its key (C D E) lights */
        static const uint8_t WK[3] = {7, 9, 11};
        uint32_t w2;
        for (w2 = 0; w2 < 3u; w2++) { fm1_led_key_glow(KEY_LED(WK[w2]), 1); fm1_led_key(KEY_LED(WK[w2]), ci.turn[w2] == ci.key[w2]); }
    }
    int32_t cx = 120, cy = 102, i;
    uint32_t n;
    /* the bench at night: the dancers' page under the lamp, a sunburst of light */
    g_cab_wall(120, 102, 92);
    for (i = 0; i < 3; i++) {                   /* the wheels ease to their turns */
        int32_t t = ci.turn[i] * 256, d = t - ci.shown[i];
        if (d > 13 * 256) d -= 26 * 256;
        if (d < -13 * 256) d += 26 * 256;
        ci.shown[i] += d / 3 + (d > 0) - (d < 0);
        ci.shown[i] = ((ci.shown[i] % (26 * 256)) + 26 * 256) % (26 * 256);
    }
    /* outer plain ring (cream), then wheels I, II, III, each edged in its colour; the turning is wheel - key */
    ci_ring(cx, cy, 78, 0, RGB(16, 8, 36), C_CREAM, C_ROSE);
    ci_ring(cx, cy, 63, -ci.shown[0], RGB(22, 12, 48), C_CYAN, C_CYAN_D);
    ci_ring(cx, cy, 48, -ci.shown[1], RGB(30, 14, 58), C_ROSE, C_ROSE_D);
    ci_ring(cx, cy, 33, -ci.shown[2], RGB(38, 18, 66), C_AMBER, RGB(130, 70, 20));
    g_disc(cx, cy, 20, C_ROSE);
    g_disc(cx, cy, 18, RGB(16, 8, 36));
    g_deco_c(cx, cy - 6, 12, won ? "OK" : "?", won ? C_VERD : C_CREAM);
    for (i = 0; i < 3; i++) g_led(cx - 10 + i * 10, cy + 12, 2, CI_COL[i]);
    /* the message: enciphered (small, coloured by wheel), deciphered (Deco) */
    g_cab_panel(2, 182, SW - 4, 40, C_ROSE);
    {
        int32_t len = 0, x;
        for (n = 0; ci.plain[n]; n++) len++;
        x = SW / 2 - len * 4;
        for (n = 0; ci.plain[n]; n++) {
            char t[2] = {ci.ciph[n], 0};
            if (ci.wheel[n] != 255) g_text(x + (int32_t)n * 8, 185, &FONT_S, t, mix565(C_LACQ, CI_COL[ci.wheel[n]], 190));
        }
        x = SW / 2 - len * 4;
        for (n = 0; ci.plain[n]; n++) {
            char t[2] = {ci_dec(n), 0};
            if (ci.wheel[n] == 255) continue;
            g_deco(x + (int32_t)n * 8 + 1, 203, 11, t, won ? C_VERD : ci.turn[ci.wheel[n]] == ci.key[ci.wheel[n]] ? C_GOLD : C_CREAM);
            g_rect(x + (int32_t)n * 8, 217, 7, ci.turn[ci.wheel[n]] == ci.key[ci.wheel[n]] ? 2 : 1, CI_COL[ci.wheel[n]]);
        }
    }
}
static void ci_leave(void) { ui_keys_clear(); }
static const game_t G_CIPHER = {BIT(BTN_EDIT), 1800, ci_enter, ci_update, ci_draw, ci_leave};
