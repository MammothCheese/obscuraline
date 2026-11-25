/* SPDX-License-Identifier: GPL-3.0-only */
/* Mechanica Obscura: the sound. Everything is a struck or plucked thing (ticks, tines, clicks, bells, whistles),
 * so one voice type covers it: two sine partials and a filtered noise, each with its own exponential decay,
 * a pitch slide, a pan.
 *
 * The main loop never touches a voice. It posts events to a ring (au_post): what to strike and WHEN, as an
 * absolute frame time. The audio ISR takes them into a pending list and starts each in the block it falls in,
 * at its exact sample. So an escapement ticks sample-true and a melody plays in time, however long a frame
 * of drawing takes. */

#ifndef RING_PUBLISH
#define RING_PUBLISH() ((void)0)                /* (host: the ISR runs between passes of the main loop) */
#endif
#define AU_NV 12u                               /* voices */
#define AU_SUB 32u                              /* envelopes step every 32 frames (0.7 ms) */

typedef struct {
    uint32_t ph1, inc1, ph2, inc2;
    int32_t a1, a2, an;                         /* amplitudes, Q15 << 8 (the decay needs the precision) */
    uint32_t d1, d2, dn;                        /* decay per AU_SUB frames, Q16 (65536 = none) */
    uint32_t slide;                             /* pitch multiplier per AU_SUB frames, Q16 */
    int32_t lp, lpy, hp;                        /* noise: one-pole low-pass coeff Q15, its state, high-pass (1 = bright click) */
    uint32_t lfsr;
    uint32_t panl, panr;                        /* Q8 */
    uint32_t start;                             /* frames into the current block before it starts */
    uint32_t age;
    int32_t ty;                                 /* the synth voice's filter state */
    uint8_t on, tag, wave;                      /* wave 1: two detuned saws through a filter (the 80s synth) */
} au_voice_t;
static au_voice_t au_v[AU_NV];

typedef struct {                                /* what the main loop asks for */
    uint32_t at;                                /* absolute frame time */
    uint32_t inc1, inc2;
    uint16_t a1, a2, an;                        /* Q15 */
    uint16_t d1, d2, dn;                        /* decay half-life in ms (0 = 1 ms) */
    int16_t slide;                              /* cents per 100 ms (+ up, - down) */
    uint16_t lp;                                /* noise low-pass Q15 (32767 = open) */
    uint8_t hp, pan, tag;                       /* pan 0 left .. 128 centre .. 255 right; tag: voices to cut by group */
    uint8_t wave;                               /* 0 sines, 1 saws (filtered by lp) */
} au_ev_t;

#define AU_Q 128u
static au_ev_t au_q[AU_Q];
static volatile uint32_t au_qw, au_qr;          /* main writes au_qw, ISR writes au_qr */
static volatile uint32_t au_now;                /* frames since boot (ISR writes) */
static volatile uint32_t au_cut_req, au_cut_done;   /* main asks: drop pending events and voices with tag au_cut_tag */
static volatile uint8_t au_cut_tag;
#define AU_PEND 48u
static au_ev_t au_pend[AU_PEND];                /* ISR only */
static uint8_t au_pend_on[AU_PEND];
static volatile uint32_t au_peak;               /* the loudest sample of the last block (for meters), ISR writes */
static volatile uint32_t au_active;

/* half-life in ms -> per-32-frame multiplier Q16: 2^(-32 / (ms * 44.1)). Precomputed for a log scale of times.
 * (tools: m = 65536 * 2 ** (-32 / (ms * 44.1)) ) */
static uint32_t au_decay_q16(uint32_t ms)
{
    /* piecewise: the multiplier is 65536 * (1 - ln2 * 32 / (44.1 ms)) for long times; exact-ish table for short */
    static const uint16_t SHORT[9] = {39632, 50964, 55420, 57792, 59264, 60266, 60992, 61543, 61974};   /* 1..9 ms */
    if (ms == 0u) ms = 1u;
    if (ms < 10u) return SHORT[ms - 1u];
    return 65536u - (32969u + ms / 2u) / ms;    /* 65536 * ln2 * 32/44.1 = 32969 */
}

static void au_post(const au_ev_t *e)            /* main loop only */
{
    uint32_t w = au_qw;
    if (w - au_qr >= AU_Q) return;              /* full: drop (never block the main loop) */
    au_q[w % AU_Q] = *e;
    RING_PUBLISH();
    au_qw = w + 1u;
}
static uint32_t au_time(void) { return au_now; }
/* a mechanism's own synthesis, mixed in before the soft clip (the train: g_travel.c); 0 = none */
static void (*volatile au_hook)(int32_t *out, uint32_t n);
static uint32_t ms_frames(uint32_t ms) { return ms * 441u / 10u; }
static void au_cut(uint8_t tag)                  /* stop a group (pending and sounding); tag 0 = everything */
{
    au_cut_tag = tag;
    RING_PUBLISH();
    au_cut_req++;
}

static int32_t au_sin(uint32_t ph)
{
    uint32_t i = ph >> 22, f = (ph >> 14) & 255u;
    int32_t s0 = MO_SIN[i], s1 = MO_SIN[(i + 1u) & 1023u];
    return s0 + (((s1 - s0) * (int32_t)f) >> 8);
}

static uint32_t au_slide_q16(int32_t cents_per_100ms)   /* per 32 frames: 2^(c/1200 * 32/4410) ~ 1 + c * 0.0000042 */
{
    return (uint32_t)(65536 + (cents_per_100ms * 275) / 1000);
}

static void au_start(const au_ev_t *e, uint32_t offset)
{
    uint32_t v, pick = 0, best = 0;
    au_voice_t *p;
    for (v = 0; v < AU_NV; v++) {               /* a free voice, else the quietest-oldest */
        uint32_t score;
        if (!au_v[v].on) { pick = v; break; }
        score = au_v[v].age + 0x10000000u - (uint32_t)((au_v[v].a1 + au_v[v].a2 + au_v[v].an) >> 12);
        if (score > best) { best = score; pick = v; }
    }
    p = &au_v[pick];
    p->on = 0;
    p->ph1 = 0; p->ph2 = 0x40000000u;
    p->inc1 = e->inc1; p->inc2 = e->inc2;
    p->a1 = (int32_t)e->a1 << 8; p->a2 = (int32_t)e->a2 << 8; p->an = (int32_t)e->an << 8;
    p->d1 = au_decay_q16(e->d1); p->d2 = au_decay_q16(e->d2); p->dn = au_decay_q16(e->dn);
    p->slide = au_slide_q16(e->slide);
    p->lp = e->lp ? e->lp : 32767; p->lpy = 0; p->hp = e->hp;
    if (!p->lfsr) p->lfsr = 0xACE1u + pick * 77u;
    p->panr = (uint32_t)e->pan + 1u;            /* equal-ish power is overkill: linear, centre = 128/128 */
    p->panl = 257u - p->panr;
    if (p->panl > 256u) p->panl = 256u;
    p->start = offset;
    p->age = 0;
    p->tag = e->tag; p->wave = e->wave; p->ty = 0;
    p->on = 1;
}

static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t i, v, now = au_now, end = now + n, peak = 0, act = 0;
    /* a cut */
    {
        static uint32_t seen;
        uint32_t req = au_cut_req;
        if (req != seen) {
            uint8_t tag = au_cut_tag;
            seen = req;
            for (i = 0; i < AU_PEND; i++) if (au_pend_on[i] && (!tag || au_pend[i].tag == tag)) au_pend_on[i] = 0;
            for (v = 0; v < AU_NV; v++) if (au_v[v].on && (!tag || au_v[v].tag == tag)) {
                au_v[v].d1 = au_v[v].d2 = au_v[v].dn = 52000u;    /* a quick fade, no click */
                au_v[v].tag = 0xFF;
            }
            au_cut_done = req;
        }
    }
    /* take the new events */
    while (au_qr != au_qw) {
        au_ev_t *e = &au_q[au_qr % AU_Q];
        for (i = 0; i < AU_PEND; i++) if (!au_pend_on[i]) break;
        if (i < AU_PEND) { au_pend[i] = *e; au_pend_on[i] = 1; }
        au_qr = au_qr + 1u;
    }
    /* start what falls in this block (late ones at once) */
    for (i = 0; i < AU_PEND; i++)
        if (au_pend_on[i] && (int32_t)(au_pend[i].at - end) < 0) {
            int32_t off = (int32_t)(au_pend[i].at - now);
            au_start(&au_pend[i], off > 0 ? (uint32_t)off : 0u);
            au_pend_on[i] = 0;
        }
    /* render */
    for (v = 0; v < AU_NV; v++) {
        au_voice_t *p = &au_v[v];
        uint32_t f, s0;
        if (!p->on) continue;
        act++;
        s0 = p->start; p->start = 0;
        for (f = s0; f < n; f += AU_SUB) {
            uint32_t fe = f + AU_SUB > n ? n : f + AU_SUB, k;
            int32_t g1 = p->a1 >> 8, g2 = p->a2 >> 8, gn = p->an >> 8;
            for (k = f; k < fe; k++) {
                int32_t s = 0;
                if (p->wave) {                  /* the synth: two saws a hair apart, a filter (lp) */
                    int32_t x2 = 0;
                    if (g1) { p->ph1 += p->inc1; x2 += (((int32_t)(p->ph1 >> 17) - 16384) * g1) >> 14; }
                    if (g2) { p->ph2 += p->inc2; x2 += (((int32_t)(p->ph2 >> 17) - 16384) * g2) >> 14; }
                    p->ty += ((x2 - p->ty) * p->lp) >> 15;
                    s += p->ty;
                    gn = 0;
                } else {
                if (g1) { p->ph1 += p->inc1; s += (au_sin(p->ph1) * g1) >> 15; }
                if (g2) { p->ph2 += p->inc2; s += (au_sin(p->ph2) * g2) >> 15; }
                }
                if (gn) {
                    int32_t x, y;
                    p->lfsr = (p->lfsr >> 1) ^ (-(int32_t)(p->lfsr & 1u) & 0xB400u);
                    x = (int32_t)(p->lfsr & 0xFFFFu) - 32768;
                    y = p->lpy + (((x - p->lpy) * p->lp) >> 15);
                    s += (((p->hp ? x - y : y)) * gn) >> 15;
                    p->lpy = y;
                }
                out[2u * k] += (s * (int32_t)p->panl) >> 8;
                out[2u * k + 1u] += (s * (int32_t)p->panr) >> 8;
            }
            p->a1 = (int32_t)(((int64_t)p->a1 * p->d1) >> 16);
            p->a2 = (int32_t)(((int64_t)p->a2 * p->d2) >> 16);
            p->an = (int32_t)(((int64_t)p->an * p->dn) >> 16);
            if (p->slide != 65536u) {
                p->inc1 = (uint32_t)(((uint64_t)p->inc1 * p->slide) >> 16);
                p->inc2 = (uint32_t)(((uint64_t)p->inc2 * p->slide) >> 16);
            }
        }
        p->age++;
        if (p->a1 < (8 << 8) && p->a2 < (8 << 8) && p->an < (8 << 8)) p->on = 0;
    }
    {
        void (*h)(int32_t *, uint32_t) = au_hook;
        if (h) h(out, n);
    }
    for (i = 0; i < 2u * n; i += 8u) {          /* a soft clip at the top, and the peak for meters */
        int32_t x = out[i] < 0 ? -out[i] : out[i];
        if ((uint32_t)x > peak) peak = (uint32_t)x;
    }
    for (i = 0; i < 2u * n; i++) {
        int32_t x = out[i];
        if (x > 24000) x = 24000 + ((x - 24000) >> 2);
        else if (x < -24000) x = -24000 + ((x + 24000) >> 2);
        out[i] = x;
    }
    au_peak = peak;
    au_active = act;
    au_now = end;
}
static int au_quiet(void) { return au_active == 0u && au_qr == au_qw; }

/* ---- the instruments (main loop): each builds an event. at = 0: now */
static uint32_t au_at(uint32_t at) { return at ? at : au_time(); }
static uint32_t note_inc(int32_t note) { return MO_NOTE[clampi(note, 0, 127)]; }
static uint32_t inc_ratio(uint32_t inc, uint32_t q8) { return (uint32_t)(((uint64_t)inc * q8) >> 8); }

/* a music-box tine: a pure fundamental, a quick metallic partial at ~ x 5.4 (the comb's first overtone) */
static void sfx_tine(int32_t note, uint32_t vel, uint32_t at, uint8_t pan, uint8_t tag)
{
    au_ev_t e = {0};
    e.at = au_at(at); e.inc1 = note_inc(note); e.inc2 = inc_ratio(e.inc1, 1384);
    e.a1 = (uint16_t)(vel * 64u); e.a2 = (uint16_t)(vel * 20u); e.an = (uint16_t)(vel * 10u);
    e.d1 = 520; e.d2 = 35; e.dn = 2; e.lp = 20000; e.hp = 1; e.pan = pan; e.tag = tag;
    au_post(&e);
}
/* the same tine, worn: duller, shorter, a rattle (the box as it was found) */
static void sfx_tine_worn(int32_t note, uint32_t at, uint8_t tag)
{
    au_ev_t e = {0};
    e.at = au_at(at); e.inc1 = note_inc(note); e.inc2 = inc_ratio(e.inc1, 520);
    e.a1 = 4600; e.a2 = 900; e.an = 2400; e.d1 = 160; e.d2 = 60; e.dn = 30; e.lp = 3000; e.pan = 128; e.tag = tag;
    au_post(&e);
}
/* an escapement tick: a bright click with a short pitched ring (tick high, tock lower) */
static void sfx_tick(uint32_t at, int32_t pitch, uint32_t vel, uint8_t pan, uint8_t tag)
{
    au_ev_t e = {0};
    e.at = au_at(at); e.inc1 = note_inc(pitch); e.inc2 = inc_ratio(e.inc1, 700);
    e.a1 = (uint16_t)(vel * 40u); e.a2 = (uint16_t)(vel * 24u); e.an = (uint16_t)(vel * 110u);
    e.d1 = 9; e.d2 = 4; e.dn = 2; e.lp = 26000; e.hp = 1; e.pan = pan; e.tag = tag;
    au_post(&e);
}
/* a ratchet / detent click, dry */
static void sfx_click(uint32_t at, uint32_t vel, uint8_t pan)
{
    au_ev_t e = {0};
    e.at = au_at(at); e.inc1 = note_inc(96); e.a1 = (uint16_t)(vel * 20u); e.an = (uint16_t)(vel * 90u);
    e.d1 = 3; e.dn = 1; e.lp = 30000; e.hp = 1; e.pan = pan;
    au_post(&e);
}
/* a heavy metal thunk: low sine dropping, damped noise (a wheel falling into its gate, a bolt) */
static void sfx_thunk(uint32_t at, int32_t note, uint32_t vel, uint8_t pan)
{
    au_ev_t e = {0};
    e.at = au_at(at); e.inc1 = note_inc(note); e.inc2 = inc_ratio(e.inc1, 590);
    e.a1 = (uint16_t)(vel * 110u); e.a2 = (uint16_t)(vel * 30u); e.an = (uint16_t)(vel * 60u);
    e.d1 = 45; e.d2 = 18; e.dn = 8; e.slide = -900; e.lp = 4000; e.pan = pan;
    au_post(&e);
}
/* a bell / chime: inharmonic partial 2.76 */
static void sfx_chime(int32_t note, uint32_t vel, uint32_t at, uint8_t pan)   /* an 80s synth pluck: two saws, unison */
{
    au_ev_t e = {0};
    e.at = au_at(at); e.inc1 = note_inc(note); e.inc2 = inc_ratio(e.inc1, 257);
    e.a1 = (uint16_t)(vel * 34u); e.a2 = (uint16_t)(vel * 34u); e.d1 = 420; e.d2 = 420; e.pan = pan; e.tag = 9;
    e.wave = 1; e.lp = 9000;
    au_post(&e);
}
static void sfx_pad(int32_t note, uint32_t vel, uint32_t at, uint8_t pan, uint32_t ms)   /* a held synth chord tone */
{
    au_ev_t e = {0};
    e.at = au_at(at); e.inc1 = note_inc(note); e.inc2 = inc_ratio(e.inc1, 258);
    e.a1 = (uint16_t)(vel * 26u); e.a2 = (uint16_t)(vel * 26u); e.d1 = (uint16_t)ms; e.d2 = (uint16_t)ms; e.pan = pan; e.tag = 9;
    e.wave = 1; e.lp = 5000;
    au_post(&e);
}
/* the bird: a whistle that rises into its note (a slide up), breathy */
static void sfx_whistle(int32_t note, uint32_t len_ms, uint32_t at, uint8_t tag)
{
    au_ev_t e = {0};
    e.at = au_at(at); e.inc1 = note_inc(note - 1); e.inc2 = inc_ratio(e.inc1, 512);
    e.a1 = 7200; e.a2 = 700; e.an = 900; e.d1 = (uint16_t)(len_ms * 6u / 10u + 40u); e.d2 = 60; e.dn = 30;
    e.slide = 260; e.lp = 9000; e.pan = 150; e.tag = tag;
    au_post(&e);
}
/* a buzz: something does not fit */
static void sfx_buzz(uint32_t at)              /* the line's word for failure: a tone that falls away */
{
    au_ev_t e = {0};
    e.at = au_at(at); e.inc1 = note_inc(62); e.inc2 = inc_ratio(e.inc1, 257);
    e.a1 = 3600; e.a2 = 3600; e.an = 500; e.d1 = 300; e.d2 = 300; e.dn = 40; e.slide = -1500; e.lp = 7000; e.pan = 128; e.wave = 1;
    au_post(&e);
}
/* the line's word for restoration: three notes, always the same (a safe opens, a route clears, a station wakes) */
static void sfx_chime(int32_t note, uint32_t vel, uint32_t at, uint8_t pan);
static void sfx_motif(uint32_t at)
{
    uint32_t t = au_at(at);
    sfx_chime(72, 80, t, 90); sfx_chime(79, 80, t + ms_frames(130), 128); sfx_chime(84, 90, t + ms_frames(260), 166);
    sfx_chime(96, 30, t + ms_frames(470), 128);
}
/* a flourish: an arpeggio of chimes from a chord */
static void sfx_flourish(int32_t root, uint32_t at)
{
    /* synthwave: a rising arpeggio of plucks, left to right, then a wide chord that hangs, and its echo */
    static const int8_t UP[6] = {0, 7, 12, 16, 19, 24};
    uint32_t i, t = au_at(at);
    for (i = 0; i < 6u; i++) sfx_chime(root + UP[i], 70u + i * 5u, t + ms_frames(i * 70u), (uint8_t)(60u + i * 27u));
    t += ms_frames(6u * 70u + 40u);
    sfx_pad(root, 80, t, 90, 900); sfx_pad(root + 7, 80, t, 166, 900); sfx_pad(root + 16, 80, t, 128, 900);
    sfx_chime(root + 24, 60, t + ms_frames(210), 40); sfx_chime(root + 24, 36, t + ms_frames(420), 215);
}

/* a pitch between semitones: note + cents (linear between table steps: exact on the semitones, < 3 cents off
 * between them, and the same function plays both the reference and the reed, so "in tune" is exact) */
static uint32_t inc_cents(int32_t note, int32_t cents)
{
    int32_t n = note + (cents >= 0 ? cents / 100 : -((99 - cents) / 100));
    int32_t f = cents - (n - note) * 100;       /* 0..99 */
    uint32_t a = note_inc(n), b = note_inc(n + 1);
    return a + (b - a) / 100u * (uint32_t)f + (b - a) % 100u * (uint32_t)f / 100u;   /* (no 64-bit division on the chip) */
}
/* a tuned reed: a long, slightly nasal tone (fundamental + octave + twelfth), for tuning by ear */
static void sfx_reed(uint32_t inc, uint32_t at, uint8_t pan, uint8_t tag, uint32_t vel)
{
    au_ev_t e = {0};
    e.at = au_at(at); e.inc1 = inc; e.inc2 = inc * 2u;
    e.a1 = (uint16_t)(vel * 60u); e.a2 = (uint16_t)(vel * 22u); e.an = (uint16_t)(vel * 4u);
    e.d1 = 1400; e.d2 = 700; e.dn = 4; e.lp = 30000; e.hp = 1; e.pan = pan; e.tag = tag;
    au_post(&e);
}
