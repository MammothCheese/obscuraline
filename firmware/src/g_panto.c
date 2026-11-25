/* SPDX-License-Identifier: GPL-3.0-only */
/* THE PANTOGRAPH: the depot's old unit, out on the line with the overhead wire neglected for years.
 *
 * The wire wanders: higher in the open, low under bridges, sagging between the masts (deep where it is slack),
 * with a clamp at every mast that jolts the bow. K1 HEIGHT sets the bow's rise (SELECT: in fours), K2 TENSION
 * its spring. The bow touches when it is set at or above the wire; it presses with tension x how far above.
 * Too little: it loses the wire and arcs. Too much (the gauge's red): the wire is pushed up and wears. A stiff
 * spring rides over the clamps (short arcs), but a sagging wire swings the bow through more than a stiff spring
 * allows: slack spans want it soft. PLAY sets off; keep contact for the run (the share asked for, the wire whole).
 * The power's hum is the contact: it cuts out as the bow leaves the wire, and the arc crackles. */

#define PA_SPAN 5000                            /* cm between masts */
#define PA_SEC 20000                            /* cm a section (four spans) */
#define PA_V 20                                 /* cm a 10 ms tick (72 km/h) */
#define PA_FMAX 44                              /* the force the wire takes */
#define PA_SECM 20
#define PA_HX 80                                /* the bow's x on the screen */

static struct {
    uint8_t base[PA_SECM], sag[PA_SECM];
    uint32_t nsec, dist, pass, state, state_t, last, acc;
    int32_t x, h, t, hd, jolt;                  /* cm run; the knobs; the bow's drawn height; ms of jolt left */
    uint32_t ticks, touch, wear, arcs, last_mast;
    int contact;
} pa;
static volatile struct { int32_t hum, arc, squeal, run; } paa;
static struct { uint32_t ph[4], lfsr; int32_t crack; } pas;

static void pa_audio(int32_t *out, uint32_t n)
{
    uint32_t i, inc0 = note_inc(31), inc1 = note_inc(43), inc2 = note_inc(98);
    int32_t hum = paa.hum, arc = paa.arc, sq = paa.squeal, run = paa.run;
    for (i = 0; i < n; i++) {
        int32_t x, nz;
        pas.lfsr ^= pas.lfsr << 13; pas.lfsr ^= pas.lfsr >> 17; pas.lfsr ^= pas.lfsr << 5;
        nz = (int32_t)(pas.lfsr >> 17) - 16384;
        pas.ph[0] += inc0; pas.ph[1] += inc1; pas.ph[2] += inc2;
        x = (((int32_t)(pas.ph[0] >> 17) - 16384) + (au_sin(pas.ph[1]) >> 1)) * hum >> 9;   /* the hum: power on */
        if (arc && (pas.lfsr & 255u) < 6u) pas.crack = 14000;                               /* the arc: cracks */
        x += (nz * pas.crack) >> 15; pas.crack = pas.crack * 250 >> 8;
        x += (au_sin(pas.ph[2]) * sq) >> 15;                                                /* the wire scraping */
        x += (nz * run) >> 12;                                                              /* the rails */
        out[2u * i] += x; out[2u * i + 1u] += x;
    }
}

static int32_t pa_wire(int32_t xcm)             /* the wire's height above the roof, px, at a place */
{
    int32_t s = xcm / PA_SEC, in = xcm % PA_SEC, b, u, sag;
    if (xcm < 0) return pa.base[0];
    if (s >= (int32_t)pa.nsec) s = (int32_t)pa.nsec - 1;
    b = pa.base[s];
    if (s > 0 && in < 3000) b = pa.base[s - 1] + (b - pa.base[s - 1]) * in / 3000;   /* (a ramp into the section) */
    u = (xcm % PA_SPAN) * 256 / PA_SPAN;
    sag = (int32_t)pa.sag[s] * 4 * u * (256 - u) / 65536;
    return b - sag;
}
static void pa_reset(void)
{
    pa.x = 0; pa.ticks = 0; pa.touch = 0; pa.wear = 0; pa.arcs = 0; pa.jolt = 0; pa.last_mast = 0;
    pa.state = 0; pa.state_t = fm1_ms; pa.acc = 0; pa.last = fm1_ms;
}
static void pa_enter(void)
{
    static const uint8_t SAGS[4] = {3, 5, 8, 12};
    uint32_t i;
    pa.dist = (uint32_t)PRM(P_PANTOGRAPH_DIST) * 100u; pa.pass = (uint32_t)PRM(P_PANTOGRAPH_PASS);
    pa.nsec = pa.dist / PA_SEC + 2u; if (pa.nsec > PA_SECM) pa.nsec = PA_SECM;
    pa.base[0] = 64; pa.sag[0] = 4;
    for (i = 1; i < pa.nsec; i++) {             /* each section its own height and slack, never two alike in a row */
        do { pa.base[i] = (uint8_t)(42 + rndn(52)); } while (iabs((int32_t)pa.base[i] - pa.base[i - 1]) < 12);
        do { pa.sag[i] = SAGS[rndn(4)]; } while (pa.sag[i] == pa.sag[i - 1]);
    }
    pa.h = 30; pa.t = 5; pa.hd = 30;
    pa_reset();
    paa.hum = 0; paa.arc = 0; paa.squeal = 0; paa.run = 0;
    pas.lfsr = 0xACE1u;
    au_hook = pa_audio;
}
static void pa_tick(void)                       /* 10 ms of the run */
{
    int32_t w = pa_wire(pa.x), gap = pa.h - w, f = pa.t * gap;
    uint32_t mast = (uint32_t)(pa.x / PA_SPAN);
    if (pa.state == 1u) {
        pa.x += PA_V; pa.ticks++;
        if (mast != pa.last_mast) { pa.last_mast = mast; if (gap >= 0) pa.jolt = 600 / pa.t; }   /* the clamp: the bow jumps */
    }
    if (pa.jolt > 0) pa.jolt -= 10;
    pa.contact = gap >= 0 && pa.jolt <= 0;
    if (pa.state == 1u) {
        if (pa.contact) pa.touch++; else if (gap >= 0 || (pa.ticks & 7u) == 0u) pa.arcs++;
        if (pa.contact && f > PA_FMAX) pa.wear += (uint32_t)(f - PA_FMAX);
    }
}
static int pa_update(void)
{
    int32_t e1 = in.enc[ENC_KNOB1] + in.enc[ENC_SELECT] * 4 + in.enc[ENC_ALGORITHM] * 4, e2 = in.enc[ENC_KNOB2] + in.enc[ENC_PRESETS];
    led_btn_mask = BIT(BTN_EDIT) | BIT(BTN_PLAY);
    if (pa.state == 3u) return 1;
    pa.h = clampi(pa.h + e1, 0, 120);
    pa.t = clampi(pa.t + e2, 1, 9);
    if (e2) sfx_click(0, 40 + pa.t * 3, 128);
    pa.acc += fm1_ms - pa.last; pa.last = fm1_ms;
    if (pa.acc > 200u) pa.acc = 200u;
    while (pa.acc >= 10u) { pa.acc -= 10u; pa_tick(); }
    if (pa.state == 0u && PRESSED(BTN_PLAY)) { pa.state = 1; pa.state_t = fm1_ms; sfx_thunk(0, 38, 90, 128); }
    else if (pa.state == 1u) {
        if (pa.wear >= 30000u) { pa.state = 2; pa.state_t = fm1_ms; sfx_buzz(0); }   /* the wire down */
        else if ((uint32_t)pa.x >= pa.dist) {
            if (pa.touch * 100u >= pa.pass * pa.ticks) { pa.state = 3; pa.state_t = fm1_ms; sfx_motif(au_time()); paa.run = 0; return 1; }
            pa.state = 2; pa.state_t = fm1_ms; sfx_buzz(0);
        }
    } else if (pa.state == 2u && fm1_ms - pa.state_t > 2200u) pa_reset();
    {   /* the sound: hum while it touches, crackle when it arcs, a scrape when it presses too hard */
        int32_t w = pa_wire(pa.x), f = pa.t * (pa.h - w);
        paa.hum = pa.contact ? 120 : 0;
        paa.arc = !pa.contact && pa.h >= w - 25;
        paa.squeal = pa.contact && f > PA_FMAX ? clampi((f - PA_FMAX) * 300, 0, 6000) : 0;
        paa.run = pa.state == 1u ? 900 : 0;
    }
    {   /* the bow, drawn: on the wire (pushed up past the limit), or where it is set, below */
        int32_t w = pa_wire(pa.x), f = pa.t * (pa.h - w), tg = pa.contact ? w + (f > PA_FMAX ? (f - PA_FMAX) / 6 : 0) : pa.jolt > 0 ? w - 4 : pa.h;
        pa.hd += (tg - pa.hd) / 2;
    }
    return 0;
}
static int32_t pa_pct(void) { return pa.ticks ? (int32_t)(pa.touch * 100u / pa.ticks) : 100; }
static void pa_draw(uint32_t won)
{
    int32_t sx, y, roof = 176, prevy = 0, wh = pa_wire(pa.x), f = pa.t * (pa.h - wh);
    char b[32], *p;
    for (y = STAGE_Y0; y < roof; y++) g_rect(0, y, SW, 1, mix565(RGB(16, 8, 44), RGB(150, 40, 120), (uint32_t)(y - STAGE_Y0) * 256u / (uint32_t)(roof - STAGE_Y0)));
    g_rect(0, roof, SW, STAGE_Y1 - roof, RGB(30, 26, 46));
    for (sx = 0; sx < SW; sx++) {               /* the masts, the messenger and the contact wire */
        int32_t xc = pa.x + (sx - PA_HX) * 25, w = pa_wire(xc), wy = roof - w;
        if (sx == PA_HX && pa.contact && f > PA_FMAX) wy -= (f - PA_FMAX) / 6;
        if (((xc % PA_SPAN) + PA_SPAN) % PA_SPAN < 25) {
            int32_t s = xc / PA_SEC, top = roof - (s < (int32_t)pa.nsec && s >= 0 ? pa.base[s] : pa.base[0]) - 24;
            g_rect(sx - 2, top, 4, STAGE_Y1 - top, RGB(70, 60, 96));
            g_rect(sx - 2, top, 24, 2, RGB(70, 60, 96));
        }
        if (((xc % 500) + 500) % 500 < 25) g_rect(sx, roof - pa_wire(xc - (xc % PA_SPAN)) - 14, 1, wy - (roof - pa_wire(xc - (xc % PA_SPAN)) - 14), RGB(90, 80, 120));
        g_rect(sx, roof - pa_wire(xc - (xc % PA_SPAN)) - 14, 1, 1, RGB(90, 80, 120));
        if (sx && iabs(wy - prevy) > 1) g_rect(sx, wy < prevy ? wy : prevy, 1, iabs(wy - prevy), C_BRASS_L);
        g_rect(sx, wy, 1, 2, C_BRASS_L);
        prevy = wy;
    }
    {   /* the train's roof and the bow */
        int32_t hy = roof - pa.hd, ky = (roof + hy) / 2;
        g_rect(0, roof, SW, 6, C_STEEL_D);
        g_rect(PA_HX - 16, roof - 3, 32, 4, C_STEEL);
        g_thick(PA_HX - 10, roof - 2, PA_HX + 8, ky, 2, C_STEEL_L); g_thick(PA_HX + 8, ky, PA_HX, hy, 2, C_STEEL_L);
        g_thick(PA_HX + 10, roof - 2, PA_HX - 8, ky, 2, C_STEEL_L); g_thick(PA_HX - 8, ky, PA_HX, hy, 2, C_STEEL_L);
        g_rrect(PA_HX - 14, hy - 2, 28, 4, 2, pa.contact ? C_CREAM : C_INK2);
        if (!pa.contact && pa.state == 1u && pa.h >= wh - 25) {   /* the arc */
            uint32_t r = fm1_ms * 2654435761u, k;
            for (k = 0; k < 4u; k++) { r = r * 1103515245u + 12345u; g_line(PA_HX - 10 + (int32_t)(r >> 28), hy, PA_HX - 12 + (int32_t)((r >> 20) & 31u), roof - wh, (r & 1u) ? C_CYAN : C_CREAM); }
        }
    }
    /* the gauges: the run, contact, the force (its red), the knobs */
    g_rect(8, STAGE_Y0 + 4, SW - 16, 4, C_PANEL2);
    g_rect(8, STAGE_Y0 + 4, (SW - 16) * (int32_t)((uint32_t)pa.x > pa.dist ? pa.dist : (uint32_t)pa.x) / (int32_t)(pa.dist ? pa.dist : 1u), 4, C_CYAN);
    p = str_cat(b, T_PA_CONTACT " "); fmt_u(p, (uint32_t)pa_pct()); while (*p) p++; str_cat(p, "%");
    g_text(8, STAGE_Y0 + 10, &FONT_S, b, pa_pct() >= (int32_t)pa.pass ? C_VERD : C_ROSE);
    p = str_cat(b, T_PA_WEAR " "); fmt_u(p, pa.wear / 300u); while (*p) p++; str_cat(p, "%");
    g_text_r(SW - 8, STAGE_Y0 + 10, &FONT_S, b, pa.wear > 20000u ? C_ROSE : C_INK2);
    {
        int32_t gx = 150, gy = roof + 10, gw = 80, fv = clampi(f, 0, PA_FMAX * 2), mk = gw / 2;
        g_rect(gx, gy, gw, 6, C_PANEL2);
        g_rect(gx + mk, gy, gw - mk, 6, C_ROSE_D);
        g_rect(gx, gy, 3, 6, C_RUBY);
        g_rect(gx + fv * gw / (PA_FMAX * 2) - 1, gy - 2, 3, 10, pa.contact ? (f > PA_FMAX ? C_ROSE : C_VERD) : C_RUBY);
        g_text(gx, gy + 7, &FONT_S, T_PA_FORCE, C_INK2);
    }
    p = str_cat(b, "H "); fmt_u(p, (uint32_t)pa.h); while (*p) p++; p = str_cat(p, "  T "); fmt_u(p, (uint32_t)pa.t);
    g_text(8, roof + 14, &FONT_S, b, C_CYAN);
    if (pa.state == 0u) g_text_c(SW / 2, STAGE_Y0 + 30, &FONT_S, T_PA_READY, C_CREAM);
    if (pa.state == 2u) g_text_c(SW / 2, STAGE_Y0 + 30, &FONT_S, pa.wear >= 30000u ? T_PA_DOWN : T_PA_AGAIN, C_ROSE);
    (void)won;
}
static void pa_leave(void) { au_hook = 0; ui_keys_clear(); }
static const game_t G_PANTO = {BIT(BTN_EDIT) | BIT(BTN_PLAY), 2000, pa_enter, pa_update, pa_draw, pa_leave};
