/* SPDX-License-Identifier: GPL-3.0-only */
/* THE GHOST SIGNAL: Akari Heights' signal box hears four decades at once.
 *
 * Four channels, one a decade (the 1920s, the 50s, the 80s, the 2010s), each tuned on its knob (K1..K4). Each dial
 * carries three stations; between them, static. Near one, its words come through the noise, letter by letter,
 * clear when it is tuned; and its sound is its age's (telegraph clicks, a valve set's voice, a modem's warble, a
 * digital pip). One message on each channel belongs to the same conversation, the same signal box answering
 * itself across a hundred years; the others are the line's ordinary chatter. Tune all four, PLAY links them: the
 * conversation read out in order, and where it breaks if it does. Unbroken, the box falls silent: the line knows a
 * driver is coming. Stations' places on the dials are new each time; which message is which is not. */

#define GH_C 4                                  /* channels */
#define GH_S 3                                  /* stations a channel */
#define GH_TUNE 2                               /* within this of a station: tuned */
#define GH_NEAR 10                              /* within this: the words start through */
static const char *const GH_MSG[GH_C][GH_S] = {   /* [0] is the conversation's own */
    {T_GH_A0, T_GH_A1, T_GH_A2}, {T_GH_B0, T_GH_B1, T_GH_B2}, {T_GH_C0, T_GH_C1, T_GH_C2}, {T_GH_D0, T_GH_D1, T_GH_D2}};
static const char *const GH_ERA[GH_C] = {"1920s", "1950s", "1980s", "2010s"};
static const uint16_t GH_COL[GH_C] = {RGB(230, 200, 140), RGB(255, 170, 90), RGB(90, 230, 140), RGB(120, 220, 255)};

static struct {
    int32_t pos[GH_C], st[GH_C][GH_S];          /* the dials; the stations' places */
    uint32_t act, act_t, link_t, link_ok, won, fin_t, msg_t;
    int32_t brk;                                /* the conversation breaks at this channel (-1: whole) */
} gh;
static volatile struct { int32_t era, cl, on; uint32_t pat; } gha;   /* what the ISR plays: the age, clarity, the message's bits */
static struct { uint32_t t, ph[2], lfsr; int32_t env, lp; } ghs;

static void gh_audio(int32_t *out, uint32_t n)  /* static, and the age's voice coming through it */
{
    uint32_t i, era = (uint32_t)gha.era, pat = gha.pat;
    int32_t cl = gha.cl, nz_l = (256 - cl) * 22, v_l = cl * 34;
    if (!gha.on) return;
    for (i = 0; i < n; i++) {
        int32_t x = 0, nz, bit;
        uint32_t t = ghs.t + i, slot = t / (era == 0u ? 2646u : era == 2u ? 1323u : 3528u);   /* 60 ms, 30 ms, 80 ms a symbol */
        ghs.lfsr ^= ghs.lfsr << 13; ghs.lfsr ^= ghs.lfsr >> 17; ghs.lfsr ^= ghs.lfsr << 5;
        nz = (int32_t)(ghs.lfsr >> 17) - 16384;
        bit = (int32_t)((pat >> (slot % 29u)) & 1u);
        if (era == 0u) {                        /* a telegraph: a soft tap at each mark, mostly tone (quieter, less crack than it was) */
            if (bit && t % 2646u == 0u) ghs.lp = 9000;   /* (lp: the tap's target, eased into: no hard edge) */
            ghs.env += (ghs.lp - ghs.env) >> 3;
            ghs.lp -= ghs.lp >> 7;
            ghs.ph[0] += note_inc(79);
            x = ((nz >> 4) + (au_sin(ghs.ph[0]) * 3 >> 2)) * ghs.env >> 15;
        } else if (era == 1u) {                 /* a valve set: a buzzing voice, its syllables, warbling */
            int32_t tg = bit ? 18000 : 0, s;
            ghs.env += (tg - ghs.env) >> 9;
            ghs.ph[0] += note_inc(38) + (uint32_t)(mo_sin(t * 3u) * 40);
            s = ((int32_t)(ghs.ph[0] >> 17) - 16384);
            ghs.lp += (s - ghs.lp) >> 3;
            x = ghs.lp * ghs.env >> 15;
        } else if (era == 2u) {                 /* a modem: two tones, the bits */
            ghs.ph[0] += bit ? note_inc(86) : note_inc(77);
            x = au_sin(ghs.ph[0]) * 9000 >> 15;
        } else {                                /* digital: clean pips */
            int32_t tg = bit && (t % 3528u) < 1800u ? 14000 : 0;
            ghs.env += (tg - ghs.env) >> 4;
            ghs.ph[0] += note_inc(83);
            x = au_sin(ghs.ph[0]) * ghs.env >> 15;
        }
        x = (x * v_l >> 13) + (nz * nz_l >> 15);
        out[2u * i] += x; out[2u * i + 1u] += x;
    }
    ghs.t += n;
}
static int32_t gh_near(uint32_t c, int32_t *which)   /* the nearest station on a channel: its distance */
{
    int32_t s, best = 999;
    *which = -1;
    for (s = 0; s < GH_S; s++) { int32_t d = iabs(gh.pos[c] - gh.st[c][s]); if (d < best) { best = d; *which = s; } }
    return best;
}
static int32_t gh_clarity(uint32_t c)           /* 0..256: how clear the channel is */
{
    int32_t w, d = gh_near(c, &w);
    return d <= GH_TUNE ? 256 : d >= GH_NEAR ? 0 : (GH_NEAR - d) * 256 / (GH_NEAR - GH_TUNE);
}
static void gh_voice(uint32_t c)                /* the ISR plays this channel now */
{
    int32_t w, k;
    uint32_t p = 0;
    const char *m;
    gh_near(c, &w);
    m = GH_MSG[c][w < 0 ? 0 : w];
    for (k = 0; m[k] && k < 29; k++) p |= (uint32_t)((m[k] * 7u + (uint32_t)k) >> 2 & 1u) << k;   /* (the message's own pattern) */
    gha.era = (int32_t)c; gha.pat = p | 1u; gha.cl = gh_clarity(c);
}
static void gh_enter(void)
{
    uint32_t c;
    for (c = 0; c < GH_C; c++) {
        int32_t base = 6 + (int32_t)rndn(10), s, order[GH_S] = {0, 1, 2}, j, t;
        for (j = GH_S - 1; j > 0; j--) { int32_t r = (int32_t)rndn((uint32_t)j + 1u); t = order[j]; order[j] = order[r]; order[r] = t; }
        for (s = 0; s < GH_S; s++) gh.st[c][order[s]] = base + s * (24 + (int32_t)rndn(8));   /* three, well apart, shuffled */
        gh.pos[c] = 50 + (int32_t)rndn(40);
        if (gh_clarity(c) > 0) gh.pos[c] = (gh.pos[c] + 50) % 100;
    }
    gh.act = 0; gh.act_t = 0; gh.link_t = 0; gh.won = 0; gh.fin_t = 0; gh.brk = -2;
    ghs.lfsr = 0x9E3779B9u; ghs.t = 0;
    gh_voice(0); gha.on = 1;
    au_hook = gh_audio;
}
static int gh_update(void)
{
    static const uint8_t KN[GH_C] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
    uint32_t c;
    led_btn_mask = BIT(BTN_EDIT) | BIT(BTN_PLAY);
    if (gh.won) { gha.cl = 256; return 1; }
    if (gh.link_t) {                            /* linking: each channel's voice in turn, then the verdict */
        uint32_t el = fm1_ms - gh.link_t, k = el / 1600u;
        if (k < GH_C) { if (gha.era != (int32_t)k) gh_voice(k); }
        else {
            gh.link_t = 0; gh.msg_t = fm1_ms;
            if (gh.brk < 0) { gh.won = 1; gh.fin_t = fm1_ms; sfx_motif(au_time()); return 1; }
            sfx_buzz(0);
        }
        return 0;
    }
    for (c = 0; c < GH_C; c++) {
        int32_t e = in.enc[KN[c]];
        if (e) { gh.pos[c] = clampi(gh.pos[c] + e, 0, 99); gh.act = c; gh.act_t = fm1_ms; gh.brk = -2; }
    }
    gh_voice(gh.act);
    if (PRESSED(BTN_PLAY)) {                    /* link them: every channel must be on a station */
        int32_t w;
        for (c = 0; c < GH_C; c++) if (gh_near(c, &w) > GH_TUNE) break;
        if (c < GH_C) { gh.brk = -3; gh.msg_t = fm1_ms; sfx_buzz(0); }
        else {
            gh.brk = -1;
            for (c = 0; c < GH_C; c++) { gh_near(c, &w); if (w != 0) { gh.brk = (int32_t)c; break; } }
            gh.link_t = fm1_ms; sfx_click(0, 60, 128);
        }
    }
    return 0;
}
static void gh_draw(uint32_t won)
{
    uint32_t c;
    char b[64];
    g_rect(0, STAGE_Y0, SW, STAGE_Y1 - STAGE_Y0, RGB(16, 10, 22));
    for (c = 0; c < GH_C; c++) {                /* a strip a channel: its decade, its dial, its signal, its words */
        int32_t y = STAGE_Y0 + 3 + (int32_t)c * 45, w, cl = gh_clarity(c), k, x;
        int on = gh.act == c || (gh.link_t && (fm1_ms - gh.link_t) / 1600u == c);
        const char *m;
        gh_near(c, &w);
        m = GH_MSG[c][w < 0 ? 0 : w];
        g_rrect(4, y, SW - 8, 42, 4, on ? RGB(34, 22, 44) : RGB(24, 16, 32));
        if (won || gh.won) g_rframe(4, y, SW - 8, 42, 4, 1, C_GOLD);
        g_text(9, y + 1, &FONT_S, GH_ERA[c], GH_COL[c]);
        g_rect(58, y + 8, 120, 2, RGB(60, 44, 70));   /* the dial: a scale, its needle */
        for (k = 0; k <= 10; k++) g_rect(58 + k * 12, y + 5, 1, k % 5 ? 3 : 6, RGB(110, 90, 120));
        g_rect(58 + gh.pos[c] * 120 / 99 - 1, y + 2, 3, 12, C_RUBY);
        for (k = 0; k < 5; k++) g_rect(190 + k * 8, y + 12 - k * 2, 5, 2 + k * 2, cl > k * 52 ? GH_COL[c] : RGB(50, 40, 60));   /* the signal */
        {   /* the words: through the noise, letter by letter as it clears */
            uint32_t i, len = 0;
            while (m[len]) len++;
            for (i = 0; i < len && i < 2u * 28u; i++) {   /* (27 a line: inside the strip) */
                uint32_t h = th32(i * 2654435761u + fm1_ms / 120u + c);
                char ch = m[i];
                if (ch != ' ' && (int32_t)(h & 255u) >= cl) ch = (char)('!' + (h >> 8) % 30u);   /* (static over it) */
                b[i] = ch;
            }
            b[i < 2u * 28u ? i : 2u * 28u] = 0;
            x = 0;
            if (i > 27u) {                      /* two lines, broken at a space */
                uint32_t cut = 27u;
                char t2;
                while (cut > 10u && m[cut] != ' ') cut--;
                t2 = b[cut]; b[cut] = 0;
                g_text(9, y + 14, &FONT_S, b, cl >= 256 ? C_CREAM : C_INK2);
                b[cut] = t2;
                { char *l2 = b + cut + (m[cut] == ' '); uint32_t n2 = 0; while (l2[n2]) n2++; if (n2 > 27u) l2[27] = 0;   /* (the second line inside too) */
                  g_text(9, y + 27, &FONT_S, l2, cl >= 256 ? C_CREAM : C_INK2); }
            } else g_text(9, y + 14, &FONT_S, b, cl >= 256 ? C_CREAM : C_INK2);
            (void)x;
        }
    }
    if (won || gh.won) g_text_c(SW / 2, STAGE_Y1 - 16, &FONT_S, T_GH_DONE, C_GOLD);
    else if (gh.link_t) g_text_c(SW / 2, STAGE_Y1 - 16, &FONT_S, T_GH_LINKING, C_CYAN);
    else if (gh.brk == -3 && fm1_ms - gh.msg_t < 2500u) g_text_c(SW / 2, STAGE_Y1 - 16, &FONT_S, T_GH_TUNEALL, C_ROSE);
    else if (gh.brk >= 0 && fm1_ms - gh.msg_t < 4000u) {
        char *p = str_cat(b, T_GH_BREAKS " "); str_cat(p, GH_ERA[gh.brk]);
        g_text_c(SW / 2, STAGE_Y1 - 16, &FONT_S, b, C_ROSE);
    } else g_text_c(SW / 2, STAGE_Y1 - 16, &FONT_S, T_GH_HINT, C_PAPER3);
}
static void gh_leave(void) { gha.on = 0; au_hook = 0; ui_keys_clear(); }
static const game_t G_GHOST = {BIT(BTN_EDIT) | BIT(BTN_PLAY), 2400, gh_enter, gh_update, gh_draw, gh_leave};
