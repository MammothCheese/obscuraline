/* SPDX-License-Identifier: GPL-3.0-only */
/* THE RAILWAY RADIO: a broken set in the summit's hut, picking up four transmissions at once.
 *
 * K1 FREQUENCY (SELECT: in fives), K2 BANDWIDTH, K3 GAIN, K4 INTERFERENCE (a notch for the line's whine).
 * Somewhere on the dial a slow voice sings: a tune no timetable has. Close by on either side, other stations
 * (a morse beacon, a station jingle) and further off a carrier's warble. Wide open, neighbours bleed in; too
 * narrow and the voice turns to mud. Gain too low and the hiss swallows it; too high and it clips to buzz. The
 * notch, off true, leaves a whine whose pitch falls as it nears, and vanishes at the right place. The magic
 * eye closes as the voice comes clear; held clear a couple of seconds, the set locks and the message comes in.
 * Everything is made here: carriers, tunes, hiss, the whine, the clipping (no samples). */

#define RX_FMAX 200                             /* the dial: 88.0 .. 108.0 */
static const uint8_t RX_TUNE[8] = {62, 65, 69, 67, 65, 64, 62, 57};   /* the voice: Shinzo's tune */
static const uint8_t RX_JINGLE[6] = {72, 76, 79, 84, 79, 76};
static const uint8_t RX_MORSE[16] = {1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0};   /* (dits and dahs, a beacon's letters) */

static struct {
    int32_t f, bw, gain, nc;                    /* the knobs */
    int32_t ft, fd[3], it, g0;                  /* the voice's frequency, the others', the notch's true place, the gain it wants */
    uint32_t clear_t, won, fin_t, eye;
    uint32_t q;                                 /* clarity, 0..256 */
} rx;

static volatile struct {                        /* what the ISR renders (main writes, one word each) */
    int32_t w[4];                               /* each transmission's level through the filter, Q8 */
    int32_t lpk;                                /* the voice's low-pass, Q15 (narrow: muddy) */
    int32_t stat, wh, whinc, gain, on;          /* static (wide: more), the whine's level and pitch, gain Q3 */
} rxa;
static struct { uint32_t t, ph[5], lfsr; int32_t lp, hiss, env[4]; } rxs;   /* ISR only */

static void rx_audio(int32_t *out, uint32_t n)
{
    uint32_t i, k, t = rxs.t;
    int32_t tgt[4], inc[4], g = rxa.gain, wh = rxa.wh, lpk = rxa.lpk, st = rxa.stat;
    if (!rxa.on) return;
    {   /* this block's notes: the voice (380 ms a note, a rest after eight), the beacon, the jingle, the warble */
        uint32_t vn = (t / 16758u) % 11u, vp = t % 16758u, mn = (t / 3528u) % 24u, jn = (t / 7938u) % 10u;
        tgt[0] = vn < 8u && vp < 14500u ? rx.g0 * 100 : 0; inc[0] = (int32_t)note_inc(RX_TUNE[vn < 8u ? vn : 0]);
        tgt[1] = mn < 16u && RX_MORSE[mn] ? 15000 : 0; inc[1] = (int32_t)note_inc(86);
        tgt[2] = jn < 6u && (t % 7938u) < 6000u ? 16000 : 0; inc[2] = (int32_t)note_inc(RX_JINGLE[jn < 6u ? jn : 0]);
        tgt[3] = 12000; inc[3] = (int32_t)note_inc(78) + mo_sin(t * 2u) * 32;
    }
    for (k = 0; k < 4u; k++) rxs.env[k] += (tgt[k] - rxs.env[k]) / 3;
    for (i = 0; i < n; i++) {
        int32_t v, m, x, nz;
        rxs.lfsr ^= rxs.lfsr << 13; rxs.lfsr ^= rxs.lfsr >> 17; rxs.lfsr ^= rxs.lfsr << 5;
        nz = (int32_t)(rxs.lfsr >> 17) - 16384;
        rxs.ph[0] += (uint32_t)inc[0]; rxs.ph[1] += (uint32_t)inc[1]; rxs.ph[2] += (uint32_t)inc[2]; rxs.ph[3] += (uint32_t)inc[3];
        rxs.ph[4] += (uint32_t)rxa.whinc;
        v = au_sin(rxs.ph[0]) + (au_sin(rxs.ph[0] * 2u) >> 1) + (au_sin(rxs.ph[0] * 3u) >> 2);   /* a voice: a few harmonics */
        rxs.lp += (int32_t)(((int64_t)(v - rxs.lp) * lpk) >> 15);
        m = (int32_t)(((int64_t)rxs.lp * rxs.env[0] >> 15) * rxa.w[0] >> 8);
        m += ((au_sin(rxs.ph[1]) * (rxs.env[1] >> 4)) >> 12) * rxa.w[1] >> 8;
        x = au_sin(rxs.ph[2]); x = x > 0 ? 20000 : -20000;            /* the jingle: square, bright */
        m += ((x * (rxs.env[2] >> 4)) >> 12) * rxa.w[2] >> 8;
        m += ((au_sin(rxs.ph[3]) * (rxs.env[3] >> 4)) >> 12) * rxa.w[3] >> 8;
        m += (nz * st) >> 8;                                          /* static, through the filter */
        m += (au_sin(rxs.ph[4]) * wh) >> 15;                          /* the whine */
        x = m * g / 8;
        if (x > 9000) x = 9000; else if (x < -9000) x = -9000;        /* the set clips */
        rxs.hiss += ((nz >> 2) - rxs.hiss) >> 2;                      /* the set's own hiss, after the gain */
        x += rxa.stat ? rxs.hiss * 3 / 4 : rxs.hiss / 8;   /* (the set's own hiss: faint once clear) */
        out[2u * i] += x; out[2u * i + 1u] += x;
    }
    rxs.t = t + n;
}

static int32_t rx_lvl(int32_t fi) { int32_t d = iabs(rx.f - fi); return d >= rx.bw ? 0 : (rx.bw - d) * 256 / rx.bw; }
static uint32_t rx_steps(void);
static void rx_params(void)                     /* the knobs into the sound, and how clear the voice is */
{
    int32_t w0 = rx_lvl(rx.ft), wo = 0, k, dn = iabs(rx.nc - rx.it), l, clip, mud;
    rxa.w[0] = w0;
    for (k = 0; k < 3; k++) { rxa.w[k + 1] = rx_lvl(rx.fd[k]); wo += rxa.w[k + 1]; }
    rxa.lpk = clampi(rx.bw * 2600, 1500, 30000);
    rxa.stat = 4 + rx.bw * 4;
    rxa.wh = dn ? clampi(dn * 2200, 3000, 9000) : 0;
    rxa.whinc = (int32_t)note_inc(40 + clampi(dn, 0, 30) * 2);    /* the whine falls as the notch nears */
    rxa.gain = rx.gain;
    /* clarity: the voice against the neighbours, the hiss (gain low), the clipping (gain high), the mud, the whine */
    l = w0 * rx.gain * rx.g0 / 256;            /* how loud the voice arrives: 180..400 right (the set clips past ~414) */
    clip = l > 414 ? (l - 414) * 3 : 0;
    mud = rx.bw < 3 ? (3 - rx.bw) * 120 : 0;
    {
        int32_t good = l > 400 ? 400 : l, bad = wo + (l < 180 ? (180 - l) * 3 : 0) + clip + mud + (dn > 1 ? 150 + dn * 15 : 0) + rx.bw * 4;
        rx.q = good <= 0 ? 0u : (uint32_t)(good * 256 / (good + bad));
    }
    if (rx_steps() == 15u) {                    /* all four right: the voice comes in clean (the neighbours, the static, the whine gone) */
        rxa.w[0] = 256; rxa.w[1] = rxa.w[2] = rxa.w[3] = 0; rxa.stat = 0; rxa.wh = 0; rxa.lpk = 26000;
        rxa.gain = clampi(330 * 1 / (rx.g0 ? rx.g0 : 1), 4, 24);   /* (a level that never clips) */
        rx.q = 256;
    }
}
static uint32_t rx_steps(void)                  /* the four steps done (bits): found, narrowed, level right, whine gone */
{
    int32_t l = rx_lvl(rx.ft) * rx.gain * rx.g0 / 256;
    uint32_t m = 0;
    if (iabs(rx.f - rx.ft) <= 2) m |= 1u;
    if (rx.bw >= 2 && rx.bw <= 8 && (m & 1u)) m |= 2u;
    if (l >= 160 && l <= 430 && (m & 1u)) m |= 4u;
    if (iabs(rx.nc - rx.it) <= 2) m |= 8u;
    return m;
}
static void rx_enter(void)
{
    int32_t s;
    rx.ft = 40 + (int32_t)rndn(121);
    s = rndn(2) ? 1 : -1;
    rx.fd[0] = rx.ft - s * (7 + (int32_t)rndn(4));
    rx.fd[1] = rx.ft + s * (8 + (int32_t)rndn(4));
    rx.fd[2] = rx.ft < 100 ? rx.ft + 45 + (int32_t)rndn(40) : rx.ft - 45 - (int32_t)rndn(40);
    rx.it = 4 + (int32_t)rndn(33);
    rx.g0 = 22 + (int32_t)rndn(12);            /* the voice's strength: the gain it wants moves each time */
    rx.f = rx.ft < 100 ? 180 : 20; rx.bw = 18; rx.gain = 3; rx.nc = rx.it > 20 ? 2 : 38;
    rx.clear_t = 0; rx.won = 0; rx.fin_t = 0; rx.eye = 0;
    rxs.lfsr = 0x1234567u; rxs.t = 0;
    rx_params();
    rxa.on = 1;
    au_hook = rx_audio;
}
static int rx_update(void)
{
    int32_t e1 = in.enc[ENC_KNOB1] + in.enc[ENC_SELECT] * 5 + in.enc[ENC_ALGORITHM] * 5, e2 = in.enc[ENC_KNOB2], e3 = in.enc[ENC_KNOB3], e4 = in.enc[ENC_KNOB4];
    led_btn_mask = BIT(BTN_EDIT);
    if (rx.won) return 1;
    rx.f = clampi(rx.f + e1, 0, RX_FMAX);
    rx.bw = clampi(rx.bw + e2, 1, 20);
    rx.gain = clampi(rx.gain + e3, 0, 24);
    rx.nc = clampi(rx.nc + e4, 0, 40);
    if (e1 || e2 || e3 || e4) sfx_tick(0, 60, 25, 128, 0);
    rx_params();
    rx.eye += ((int32_t)rx.q - (int32_t)rx.eye) / 4;
    if (rx_steps() == 15u) {
        if (!rx.clear_t) rx.clear_t = fm1_ms ? fm1_ms : 1u;
        if (fm1_ms - rx.clear_t > (uint32_t)PRM(P_RADIO_LOCK_MS)) { rx.won = 1; rx.fin_t = fm1_ms; sfx_motif(au_time()); return 1; }
    } else rx.clear_t = 0;
    return 0;
}
static void rx_knob(int32_t x, int32_t y, int32_t v, int32_t vmax, const char *lab, uint16_t c)
{
    uint32_t a = (uint32_t)(40960 + v * 49152 / (vmax ? vmax : 1));   /* 7 o'clock to 5 o'clock */
    g_disc(x, y, 13, RGB(40, 26, 30));
    g_ring(x, y, 12, 14, C_BRASS_D);
    g_hand(x, y, 11, 0, a, 2, c);
    g_text_c(x, y + 16, &FONT_S, lab, C_INK2);
}
static void rx_draw(uint32_t won)
{
    int32_t i, x;
    char b[24], *p;
    /* the cabinet: walnut, a cloth speaker, the dial's glass */
    g_rect(0, STAGE_Y0, SW, STAGE_Y1 - STAGE_Y0, RGB(46, 24, 20));
    g_rrect(8, STAGE_Y0 + 4, SW - 16, 62, 6, RGB(22, 14, 18));
    g_rframe(8, STAGE_Y0 + 4, SW - 16, 62, 6, 2, C_BRASS_D);
    for (i = 0; i <= 20; i++) {                 /* the scale: 88 .. 108 */
        x = 20 + i * 200 / 20;
        g_rect(x, STAGE_Y0 + 14, 1, i % 5 ? 4 : 8, C_BRASS);
        if (!(i % 5)) { p = b; fmt_u(p, (uint32_t)(88 + i)); g_text_c(x, STAGE_Y0 + 24, &FONT_S, b, C_BRASS_L); }
    }
    x = 20 + rx.f;
    g_rect(x, STAGE_Y0 + 8, 2, 52, C_RUBY);     /* the needle */
    p = b; fmt_u(p, (uint32_t)(88 + rx.f / 10)); while (*p) p++; *p++ = '.'; fmt_u(p, (uint32_t)(rx.f % 10)); while (*p) p++; str_cat(p, " MHz");
    g_text_c(SW / 2, STAGE_Y0 + 44, &FONT_S, b, C_GOLD);
    /* the speaker cloth, shaking with the sound; the magic eye */
    g_rrect(14, STAGE_Y0 + 74, 128, 74, 8, RGB(70, 46, 36));
    {
        uint32_t pk = au_peak;
        for (i = 0; i < 9; i++) g_rect(20, STAGE_Y0 + 80 + i * 7 + (int32_t)((pk >> 12) & ((uint32_t)i & 1u)), 116, 2, RGB(92, 62, 48));
    }
    {   /* the eye: a green fan that closes as the voice comes clear */
        int32_t cx = 190, cy = STAGE_Y0 + 108, open = 256 - (int32_t)(rx.eye > 256u ? 256u : rx.eye);
        uint32_t a;
        g_disc(cx, cy, 24, RGB(10, 30, 18));
        g_ring(cx, cy, 23, 26, C_BRASS_D);
        for (a = 0; a < 65536u; a += 1024u) {
            int32_t d = (int32_t)(a > 32768u ? 65536u - a : a);   /* 0 at the top */
            if (d < open * 128) continue;        /* the dark wedge: wide when unclear */
            g_line(cx, cy, cx + (mo_sin(a) * 21 >> 15), cy - (mo_cos(a) * 21 >> 15), RGB(80, 255, 140));
        }
        g_disc(cx, cy, 6, RGB(10, 30, 18));
        g_text_c(cx, cy + 28, &FONT_S, T_RX_EYE, C_INK2);
    }
    rx_knob(30, STAGE_Y0 + 166, rx.f, RX_FMAX, "FREQ", C_CYAN);
    rx_knob(90, STAGE_Y0 + 166, rx.bw, 20, "BAND", C_ROSE);
    rx_knob(150, STAGE_Y0 + 166, rx.gain, 24, "GAIN", C_AMBER);
    rx_knob(210, STAGE_Y0 + 166, rx.nc, 40, "NOTCH", C_VERD);
    if (rx.won || won) {                        /* locked: the message, line by line */
        uint32_t el = fm1_ms - rx.fin_t;
        g_rect_a(10, STAGE_Y0 + 70, SW - 20, 84, C_NIGHT, 210);
        g_text_c(SW / 2, STAGE_Y0 + 76, &FONT_S, T_RX_LOCKED, C_VERD);
        if (el > 500u) g_text_c(SW / 2, STAGE_Y0 + 96, &FONT_S, T_RX_MSG1, C_CREAM);
        if (el > 1300u) g_text_c(SW / 2, STAGE_Y0 + 112, &FONT_S, T_RX_MSG2, C_CREAM);
        if (el > 2100u) g_text_c(SW / 2, STAGE_Y0 + 128, &FONT_S, T_RX_MSG3, C_CYAN);
    } else if (rx.clear_t) g_text_c(78, STAGE_Y0 + 104, &FONT_S, T_RX_HOLD, C_VERD);
    if (!rx.won && !won) {                      /* the four steps, ticked off as each comes right */
        static const char *const ST[4] = {T_RX_STEP1, T_RX_STEP2, T_RX_STEP3, T_RX_STEP4};
        uint32_t m = rx_steps(), k;
        for (k = 0; k < 4u; k++) {
            int ok = (m >> k & 1u) != 0u;
            g_rrect(16 + (int32_t)k * 31, STAGE_Y0 + 122, 29, 14, 3, ok ? C_VERD : RGB(60, 40, 34));
            static const char *const DG[4] = {"1", "2", "3", "4"};
            g_text_c(30 + (int32_t)k * 31, STAGE_Y0 + 121, &FONT_S, DG[k], ok ? C_NIGHT : C_INK2);
        }
        if (!rx.clear_t) {                      /* the next step, named */
            uint32_t nx = !(m & 1u) ? 0u : !(m & 2u) ? 1u : !(m & 4u) ? 2u : 3u;
            g_text(20, STAGE_Y0 + 100, &FONT_S, ST[nx], C_CREAM);
        }
    }
}
static void rx_leave(void) { rxa.on = 0; au_hook = 0; ui_keys_clear(); }
static const game_t G_RADIO = {BIT(BTN_EDIT), 3600, rx_enter, rx_update, rx_draw, rx_leave};
