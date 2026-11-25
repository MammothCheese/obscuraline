/* SPDX-License-Identifier: GPL-3.0-only */
/* THE DRIVER'S WATCH: a timegrapher, kept simple. Three knobs, each doing one thing you can see and hear at once:
 *
 *   K1 SPEED     the trace's slope: up when the watch runs fast, down when slow. Make it flat.
 *   K2 BALANCE   the trace splits in two (tick and tock uneven). Bring the two lines together.
 *   K3 POWER     the balance's swing (and the ticks' strength). Bring it into the marked range.
 *
 * The three are independent. A meter each, with its target marked; the trace scrolls ~10 s and never clears. The
 * ticks run on the audio clock: speed is heard in the tempo, balance as a limp, power as a strong or weak click.
 * All three right, held a few seconds (a bar fills), and the watch is regulated: the trace turns gold, the hands
 * move on past 23:47 and the watch chimes the line's tune. Each start is the true setting moved off by fixed
 * amounts, and every target is a detent or more wide. */

#define WT_NP 64u                               /* dots kept */
#define WT_TY0 64                               /* the trace's glass */
#define WT_TY1 140

static struct {
    int32_t k[3], t[3];                         /* the knobs; their true places (hidden) */
    int32_t rate, beat, amp, bal;               /* s/day, tenths of a ms, degrees; the balance's side (beat, signed) */
    int32_t py[WT_NP]; uint8_t podd[WT_NP];
    uint32_t np, wk, next_at;
    int32_t drift;
    uint32_t good_t, won, fin_t, knob_t[3];
} wt;

static void wt_read(void)
{
    wt.rate = (wt.k[0] - wt.t[0]) * 4;
    wt.bal = (wt.k[1] - wt.t[1]) * 2;
    wt.beat = iabs(wt.bal);
    wt.amp = clampi(280 + (wt.k[2] - wt.t[2]) * 8, 120, 360);
}
static int wt_ok_rate(void) { return iabs(wt.rate) <= 5; }
static int wt_ok_beat(void) { return wt.beat <= 6; }
static int wt_ok_amp(void) { return wt.amp >= 250 && wt.amp <= 310; }
static void wt_enter(void)
{
    uint32_t i;
    for (i = 0; i < 3u; i++) { wt.t[i] = 20 + (int32_t)rndn(20); wt.knob_t[i] = 0; }
    wt.k[0] = wt.t[0] + (rndn(2) ? 21 : -21);    /* ~85 s a day off */
    wt.k[1] = wt.t[1] + (rndn(2) ? 16 : -16);    /* ~3.2 ms of uneven beat */
    wt.k[2] = wt.t[2] - 12;                      /* ~185 degrees: weak */
    wt.np = 0; wt.wk = 0; wt.drift = 0; wt.good_t = 0; wt.won = 0; wt.fin_t = 0;
    wt.next_at = au_time() + ms_frames(300);
    wt_read();
}
static void wt_beat(uint32_t at)                /* one beat: its click, scheduled; its dot */
{
    uint32_t odd = wt.wk & 1u;
    uint32_t vel = (uint32_t)clampi(wt.amp - 120, 10, 200) * 64u / 200u + 6u;
    int32_t h2 = (WT_TY1 - WT_TY0) * 128;
    sfx_tick(at, odd ? 70 : 74, vel, 150, 1);
    wt.drift += wt.rate * 64 / 10;              /* Q8 px a beat */
    if (wt_ok_rate()) { int32_t d = ((wt.drift + h2) % (2 * h2) + 2 * h2) % (2 * h2) - h2; wt.drift = d - d / 24; }   /* (true: it settles on the centre line) */
    wt.py[wt.np % WT_NP] = wt.drift / 256 + (odd ? 1 : -1) * wt.beat * 3 / 2;
    wt.podd[wt.np % WT_NP] = (uint8_t)odd;
    wt.np++; wt.wk++;
}
static int wt_update(void)
{
    static const uint8_t KN[3] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3};
    uint32_t i, ahead = au_time() + ms_frames(120);
    led_btn_mask = BIT(BTN_EDIT);
    for (i = 0; i < 3u; i++) {
        int32_t e = in.enc[KN[i]] + (i == 0u ? in.enc[ENC_SELECT] * 3 : 0);
        if (e) {                                /* (each knob stops where its meter ends: no turns lost past the edge) */
            static const int8_t LO[3] = {-25, -20, -20}, HI[3] = {25, 20, 10};
            wt.k[i] = clampi(wt.k[i] + e, wt.t[i] + LO[i], wt.t[i] + HI[i]); wt.knob_t[i] = fm1_ms; sfx_click(0, 44 + (int32_t)i * 4, 128);
        }
    }
    wt_read();
    while ((int32_t)(wt.next_at - ahead) < 0) { /* the beats, their spacing the faults (exaggerated to hear them) */
        int32_t d = 200 - wt.rate / 8 + ((wt.wk & 1u) ? 1 : -1) * wt.beat * 5 / 2;
        wt_beat(wt.next_at);
        wt.next_at += ms_frames((uint32_t)clampi(d, 150, 250));
    }
    if (wt.won) return 1;
    if (wt_ok_rate() && wt_ok_beat() && wt_ok_amp()) {
        if (!wt.good_t) wt.good_t = fm1_ms ? fm1_ms : 1u;
        if (fm1_ms - wt.good_t >= (uint32_t)PRM(P_WATCH_HOLD_MS)) {
            uint32_t k, t0 = au_time() + ms_frames(300);
            wt.won = 1; wt.fin_t = fm1_ms;
            for (k = 0; k < 8u; k++) sfx_tine(RX_TUNE[k] + 12, 80, t0 + ms_frames(k * 330u), 128, 2);   /* the line's tune */
            return 1;
        }
    } else wt.good_t = 0;
    return 0;
}
static void wt_meter(int32_t y, const char *lab, uint16_t col, int32_t v, int32_t lo, int32_t hi, int32_t zlo, int32_t zhi, int ok, uint32_t kt)
{
    int32_t x0 = 92, w = 136, zx0 = x0 + (zlo - lo) * w / (hi - lo), zx1 = x0 + (zhi - lo) * w / (hi - lo), nx = x0 + (clampi(v, lo, hi) - lo) * w / (hi - lo);
    g_text(8, y, &FONT_S, lab, col);
    g_rrect(x0, y + 4, w, 8, 3, RGB(30, 20, 44));
    g_rect(zx0, y + 4, zx1 - zx0 + 1, 8, ok ? C_VERD : RGB(40, 90, 60));   /* the target */
    g_rect(nx - 1, y + 1, 3, 14, fm1_ms - kt < 200u ? C_CREAM : ok ? C_VERD : C_AMBER);
}
static void wt_draw(uint32_t won)
{
    uint32_t i, n = wt.np < WT_NP ? wt.np : WT_NP, el = won ? fm1_ms - wt.fin_t : 0u;
    int32_t y;
    g_rect(0, STAGE_Y0, SW, STAGE_Y1 - STAGE_Y0, RGB(22, 12, 34));
    {   /* the balance, swinging by the power */
        int32_t sw = wt.amp * 16384 / 360, cx = 26, cy = 42, k;
        uint32_t a = (uint32_t)((mo_sin((fm1_ms * 325u) & 0xFFFFu) * sw) >> 15);
        g_ring(cx, cy, 13, 16, C_BRASS);
        for (k = 0; k < 3; k++) g_hand(cx, cy, 12, 0, a + (uint32_t)k * 21845u, 2, C_BRASS_L);
        g_disc(cx, cy, 3, C_RUBY);
    }
    g_text_c(SW / 2, STAGE_Y0 + 4, &FONT_S, T_WT_TITLE, C_BRASS_L);
    {   /* the dial, stopped at 23:47 until it is right */
        int32_t cx = 214, cy = 42;
        uint32_t mins = 23u * 60u + 47u + (won ? el / 120u : 0u), ha = (mins % 720u) * 65536u / 720u, ma = (mins % 60u) * 65536u / 60u;
        g_disc(cx, cy, 15, C_CREAM); g_ring(cx, cy, 14, 16, C_BRASS);
        g_hand(cx, cy, 8, 0, ha, 2, C_INK); g_hand(cx, cy, 12, 0, ma, 1, C_INK);
        g_disc(cx, cy, 1, C_RUBY);
    }
    /* the trace */
    g_rrect(6, WT_TY0 - 2, SW - 12, WT_TY1 - WT_TY0 + 4, 4, RGB(4, 14, 8));
    g_rframe(6, WT_TY0 - 2, SW - 12, WT_TY1 - WT_TY0 + 4, 4, 1, C_BRASS_D);
    for (y = WT_TY0 + 8; y < WT_TY1; y += 15) g_rect(8, y, SW - 16, 1, RGB(10, 34, 18));
    for (i = 1; i < 10u; i++) g_rect(8 + (int32_t)i * 22, WT_TY0, 1, WT_TY1 - WT_TY0, RGB(10, 34, 18));
    g_rect(8, (WT_TY0 + WT_TY1) / 2, SW - 16, 1, RGB(20, 70, 36));
    for (i = 0; i < n; i++) {
        uint32_t idx = (wt.np - 1u - i) % WT_NP;
        int32_t x = SW - 12 - (int32_t)i * 7 / 2, h = (WT_TY1 - WT_TY0) / 2, yy = ((wt.py[idx] + h) % (2 * h) + 2 * h) % (2 * h) - h;
        if (x < 10) break;
        g_disc(x, (WT_TY0 + WT_TY1) / 2 - yy, i < 2u ? 2 : 1, won ? C_GOLD : wt.podd[idx] ? RGB(120, 255, 150) : RGB(60, 200, 110));
    }
    /* the three meters, each with its target */
    wt_meter(148, T_WT_K1, C_CYAN, wt.rate, -100, 100, -5, 5, wt_ok_rate(), wt.knob_t[0]);
    wt_meter(166, T_WT_K2, C_ROSE, wt.bal, -40, 40, -6, 6, wt_ok_beat(), wt.knob_t[1]);   /* (centred: the knob moves it either way) */
    wt_meter(184, T_WT_K3, C_AMBER, wt.amp, 120, 360, 250, 310, wt_ok_amp(), wt.knob_t[2]);
    if (won) g_text_c(SW / 2, 203, &FONT_S, T_WT_DONE, C_GOLD);
    else if (wt.good_t) {
        uint32_t w = (fm1_ms - wt.good_t) * 180u / (uint32_t)PRM(P_WATCH_HOLD_MS);
        g_rect(30, 214, 180, 4, RGB(30, 20, 44)); g_rect(30, 214, (int32_t)(w > 180u ? 180u : w), 4, C_VERD);
        g_text_c(SW / 2, 201, &FONT_S, T_WT_TESTING, C_VERD);
    } else g_text_c(SW / 2, 203, &FONT_S, !wt_ok_amp() ? T_WT_DO3 : !wt_ok_beat() ? T_WT_DO2 : T_WT_DO1, C_INK2);
}
static void wt_leave(void) { au_cut(1); au_cut(2); ui_keys_clear(); }
static const game_t G_WATCH = {BIT(BTN_EDIT), 3200, wt_enter, wt_update, wt_draw, wt_leave};
