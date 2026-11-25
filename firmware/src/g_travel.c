/* SPDX-License-Identifier: GPL-3.0-only */
/* Obscura Line: the travel mechanism. Drive one leg of the line (content/lines.txt) from the cab and stop at the
 * marker: DENSHA FM1's train engine (physics, the 3D view, the synthesized train, the line's events) inside a
 * chapter. Solved when the train stands within 10 m of the stop marker.
 *
 *   ALGORITHM / K1    master controller: clockwise power P1..P5, anticlockwise brake B1..B8, EB
 *   OCT- / OCT+       one notch toward brake / power      keys F3..G4  one-touch notch (F3 EB .. D4 N .. G4 P5)
 *   REC (hold)        horn (railfans)       FX  headlight dim / bright (oncoming)       SEL  point and call (checks)
 *
 * The view is 240 x 120 drawn straight into the frame (cv_px) under the header; the panel below it. The train's
 * own sound is mixed in from the audio ISR through au_hook (mo_audio.c). Integers only. */

/* ======================================================================== the route (mo_content.h: TR_LEGS) */
enum { TZ_COUNTRY, TZ_TOWN, TZ_RIVER, TZ_TUNNEL, TZ_STREET, TZ_SEA, TZ_HOUSES, TZ_SAKURA, TZ_NEON,
       TZ_INDUSTRIAL, TZ_RIVERSIDE, TZ_ONSEN, TZ_FIELDS, TZ_WOODS, TZ_ROCK, TZ_MVILLAGE, TZ_BEACH, TZ_FESTIVAL };   /* (content.py ZONES) */
enum { TS_DAY, TS_DUSK, TS_NIGHT, TS_RAIN, TS_SUNSET, TS_DAWN };
enum { TE_NONE, TE_ONCOMING, TE_FANS, TE_CHECK, TE_WHISTLE };

static int32_t tr_seg(const tr_seg_t *s, uint32_t n, int32_t m)
{
    int32_t v = s[0].v;
    uint32_t i;
    for (i = 1; i < n && (int32_t)s[i].at <= m; i++)
        v = s[i].v;
    return v;
}

/* ======================================================================== state */
#define TR_EB (-9)
#define TR_PMAX 5
#define TR_HZ 60
#define TR_MAXEV 12u
enum { TC_NONE, TC_BRIGHT, TC_FANS, TC_WHISTLE, TC_CHECK, TC_DIM };   /* prompts, by priority */
enum { TRM_STORY, TRM_ENODEN, TRM_ENDLESS };   /* the game's lines; the Enoden (no synth); the endless line */
static uint8_t tr_mode, tr_song;
static uint8_t tr_rain;                          /* this drive: it rains (wet rails: the brakes hold less) */
static uint8_t tr_rain_pct = 15;                 /* the chance a drive rains (not sunset lines, not tunnels; tests: 0) */
static const tr_leg_t *tr_force;                /* a leg from elsewhere (the Enoden); 0: TR_LEGS[the chapter's] */
static const char *const *tr_names = TR_STATION;
static int32_t tr_last_pts, tr_last_mx;
static uint32_t tr_odo_base;                    /* the endless line: metres before this stretch */
static uint8_t tr_last_tier, tr_best_tier, tr_features = 0xFF, tr_music_off;   /* the run just driven (1 F .. 7 S), its best; the features (F_*); the music muted: the main loop sets them */
enum { TRP_OFF, TRP_TUNE, TRP_REV, TRP_DOORS, TRP_CLOSING, TRP_READY };   /* the start-up, in order */
static const char TR_TIER[8] = {'-', 'F', 'E', 'D', 'C', 'B', 'A', 'S'};
static struct {
    const tr_leg_t *L;
    uint32_t t, acc, last_ms, done, stopped_ms;
    int32_t notch, v, brk;                      /* km/h Q16; brake km/h/s Q16 (the air brake lags) */
    uint32_t pos, rem;                          /* metres Q16 */
    int32_t time_left;                          /* ticks */
    uint32_t over_ticks, pen_speed, pen_eb, horn, dim;
    int32_t sky_x, ev_pts, r_cm, r_stop, r_time;
    uint32_t whistle;
    const char *r_grade;
    uint8_t evs[TR_MAXEV];
    uint32_t cue, cue_arg, cue_ev, ack_ev, on_live, on_judged, on_whoosh, bright_t;
    int32_t on_mm, on_v;
    const char *toast;
    uint32_t toast_until;
    uint16_t toast_c;
    uint32_t kprev;
    int32_t ev_max;                             /* the points the events can give */
    uint8_t tier;
    uint32_t prep, prep_t, appr, mtoast_t;      /* the start-up (TRP_*), since when; the approach chime; the music toast */
    uint8_t ph[8], ph_len, ph_got, ph_state, ph_miss, upd;   /* the phrase to copy: notes, how far, 0 the cab sings 1 yours 2 wrong */
    uint32_t ph_t, lead_k;                      /* when it began singing; the key the lead plays (27: none) */
    uint32_t sys_t, sys_n;                      /* the systems check at power-on: since; the lamps ticked so far */
    uint32_t zone, zone_t, zone_pts, zone_beat, zone_hit_t, zone_bp;   /* (zone_bp: this break's beats, 5 at most) */
      /* the synth break: 0 ahead 1 in it 2 past; since; points (5 at most); the last beat scored */
    char zone_msg[24];
    uint32_t paused, warp, warp_t, inf_at;     /* (the heart's run: past inf_at the limit is none; warp: full power given) */
    int32_t r_pts, r_mx;                        /* the run's points and the most there were */
} tr;

static uint32_t tr_pos_m(void) { return tr.pos >> 16; }
static int32_t tr_pos_mm(void) { return (int32_t)(((tr.pos >> 8) * 1000u) >> 8); }
static int32_t tr_kmh(void) { return tr.v >> 16; }
static int32_t tr_limit(void) { return tr_seg(tr.L->lim, tr.L->nlim, (int32_t)tr_pos_m()); }
static int32_t tr_rel(void) { return (int32_t)tr.L->dist * 1000 - tr_pos_mm(); }
static uint32_t tr_zone(int32_t m) { return (uint32_t)tr_seg(tr.L->zon, tr.L->nzon, m); }
static int32_t tr_next_limit(int32_t *dist)
{
    uint32_t i, m = tr_pos_m();
    for (i = 0; i < tr.L->nlim; i++)
        if (tr.L->lim[i].at > m) {
            if (tr.L->lim[i].at - m > 600u) return 0;
            *dist = (int32_t)(tr.L->lim[i].at - m);
            return tr.L->lim[i].v;
        }
    return 0;
}

/* ======================================================================== physics, 60 Hz */
static const uint16_t TR_PACC[6] = {0, 205, 358, 512, 666, 768};   /* km/h/s x 256 below 35 km/h */
static void tr_phys(void)
{
    int32_t a = 0, tgt, k8 = tr.v >> 8, grade = tr_seg(tr.L->grd, tr.L->ngrd, (int32_t)tr_pos_m()), t;
    if (tr.notch > 0) {
        uint32_t a0 = (uint32_t)TR_PACC[tr.notch] << 8;
        if (k8 > 35 * 256) a0 = a0 * (35u * 256u) / (uint32_t)k8;
        if (tr_rain) a0 = a0 * 9u / 10u;        /* (wet rails: the wheels slip a little) */
        a = (int32_t)a0;
    }
    tgt = tr.notch == TR_EB ? 314573 : tr.notch < 0 ? -tr.notch * 32768 : 0;
    if (tr.brk < tgt) { tr.brk += tr.notch == TR_EB ? 8738 : 3823; if (tr.brk > tgt) tr.brk = tgt; }
    else if (tr.brk > tgt) { tr.brk -= 3823; if (tr.brk < tgt) tr.brk = tgt; }
    if (tr.v > 0) a -= 3277 + (int32_t)((((uint32_t)k8 * (uint32_t)k8) >> 16) * 2u);
    a -= tr_rain ? tr.brk * 4 / 5 : tr.brk;  /* (wet rails: the brakes hold four fifths) */
    a -= grade * 2313;
    tr.v += a / TR_HZ;
    if (tr.v < 0) tr.v = 0;
    if (tr.warp) {                              /* no limit, full power: it runs away, faster and faster */
        tr.v += (int32_t)(820000 / TR_HZ);      /* (~12 km/h a second: from ~90 to ~380 over the twenty seconds) */
        if (tr.v > (420 << 16)) tr.v = 420 << 16;
    } else if (tr.v > (160 << 16)) tr.v = 160 << 16;
    t = tr.v + (int32_t)tr.rem;
    tr.pos += (uint32_t)(t / 216);
    tr.rem = (uint32_t)(t % 216);
    tr.sky_x += tr_seg(tr.L->cur, tr.L->ncur, (int32_t)tr_pos_m()) * (tr.v >> 16) / 4;
}

/* ======================================================================== the autopilot (host tests, the cheat) */
#ifdef DINGHY_HOST
static int tr_autoplay;                         /* DENSHA_AUTOPLAY=1: it drives every leg (tests/walkthrough) */
#else
#define tr_autoplay 0
#endif
static void tr_auto(void)
{
    int32_t m = (int32_t)tr_pos_m(), v = tr_kmh(), v8 = tr.v >> 8, tgt = tr_limit() - 4, d = tr_rel(), n = 0;
    uint32_t i;
    for (i = 0; i < (uint32_t)tr.L->nlim + tr.L->nev; i++) {
        int32_t at, vl, dl, al;
        if (i < tr.L->nlim) { at = tr.L->lim[i].at; vl = tr.L->lim[i].v - 4; }
        else if (tr.L->ev[i - tr.L->nlim].type == TE_CHECK) { at = tr.L->ev[i - tr.L->nlim].at; vl = tr.L->ev[i - tr.L->nlim].arg - 3; }
        else continue;
        dl = at - m - 30;
        if (dl <= -30 || vl >= tgt) continue;
        if (vl < 5) vl = 5;
        al = (int32_t)mo_isqrt((uint32_t)(vl * vl + (tr_rain ? 8 : 10) * (dl > 0 ? dl : 0)));
        if (al < tgt) tgt = al;
    }
    if (d > 300) {
        uint32_t q = (uint32_t)v8 * (uint32_t)v8 / (uint32_t)d, a_q8 = q * 139u >> 8;
        if (tr_rain) a_q8 = a_q8 * 5u / 4u;   /* (the autopilot knows the rails are wet) */
        if (a_q8 >= 400u || (d < 20000 && v < 12)) {
            if (a_q8 < 90u) n = v < 6 ? 1 : 0;
            else n = -(int32_t)((a_q8 + 100u) / 128u);
            tr.notch = n < -8 ? -8 : n;
            return;
        }
    } else { tr.notch = v ? -3 : -8; return; }
    if (v < tgt - 6) n = 5;
    else if (v < tgt - 2) n = 3;
    else if (v > tgt + 3) n = -3;
    else if (v > tgt) n = -1;
    tr.notch = n;
}

/* ======================================================================== the train's sound (mixed in from the ISR) */
static volatile uint32_t ta_kmh8, ta_load, ta_brake, ta_horn, ta_whistle, ta_bell, ta_tunnel, ta_xing;
static volatile uint32_t ta_hiss_n, ta_thunk_n, ta_pass_n, ta_mel_n, ta_chime_n, ta_music;
static const uint8_t *volatile ta_chime;          /* a chime to play (the main loop sets it, then ta_chime_n++) */
static const uint8_t TA_DOORS[] = {88, 3, 85, 5, 88, 3, 85, 9, 0, 0};             /* the door chime: pin-pon, pin-pon */
static const uint8_t TA_NEXT[] = {79, 2, 83, 2, 86, 7, 0, 0};                       /* the next station is near */
static const uint8_t TA_ARRIVE[] = {85, 3, 88, 3, 93, 8, 0, 0};                     /* arrival: the doors open */
static const uint8_t TA_POWER[] = {57, 2, 64, 2, 69, 4, 0, 0};                      /* the cab wakes up */
static uint32_t ta_rng = 0x2545F491u;
static inline int32_t ta_noise(void) { ta_rng = ta_rng * 1664525u + 1013904223u; return (int32_t)ta_rng >> 17; }
static int32_t ta_sine(uint32_t ph)
{
    int32_t x = (int32_t)ph >> 16, ax = x < 0 ? -x : x, y, ay;
    y = (x * (32768 - ax)) >> 13;
    ay = y < 0 ? -y : y;
    y += ((((y * ay) >> 15) - y) * 7373) >> 15;
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}
#define TA_HZ(f) ((uint32_t)((f) * 97391u))
static const uint32_t TA_C9[12] = {815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u,
                                   1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u};
static uint32_t ta_note(uint32_t n) { return n > 131u ? 0u : TA_C9[n % 12u] >> (10u - n / 12u); }
/* the departure melody, this line's own (synthwave: minor, arpeggiated) */
static const uint8_t TA_DEPART[] = {69, 2, 72, 2, 76, 2, 81, 4, 79, 2, 76, 2, 77, 4, 0, 2, 74, 2, 77, 2, 81, 2, 84, 4,
                                    83, 2, 79, 2, 81, 8, 0, 0};
static struct {
    uint32_t hum_ph, car_ph, car_ph2, jp, clk_ph, sq_ph, sq_lfo, thunk_ph, hiss_seen, thunk_seen, pass_seen, mel_seen;
    int32_t hum_amp, wh_amp, roll_amp, roll_lp, roll_k, clk_env, sq_amp, hiss_env, hiss_lp, thunk_env;
    uint32_t horn_ph1, horn_ph2, bell_t, bell_ph1, bell_ph2, xb_t, xb_ph1, xb_ph2, pass_t;
    int32_t horn_amp, horn_lp, bell_env, xb_env, pass_lp, wh_amp2, wh_lp;
    uint32_t wh_ph1, wh_ph2;
    const uint8_t *seq;
    uint32_t chime_seen, seq_step;
    uint32_t seq_left, mv_ph[2], mv_inc[2], mv_next;
    int32_t mv_env[2];
    uint32_t xb_seen;
} ta;
static int32_t ta_slew(int32_t c, int32_t t, int32_t s) { return c < t ? (c + s > t ? t : c + s) : (c - s < t ? t : c - s); }

/* ---- the music while driving: pulsing 80s synthwave. Am F C G at 104 BPM; a bass of two detuned saws and a sub,
 * a pad of six detuned saws through a filter that opens with the speed, a kick that pumps the rest: the backing. The
 * keys are the lead (tr_lead_*). It grows as the train speeds up (bass, then the pad, the drums) and tails off as it
 * slows; muted, it fades */
/* Each line has its own song: a key, four chords, a tempo, a bass line and a drum feel; the heart's has a melody */
typedef struct { uint8_t bpm, root[4], minor, bass, drums; const uint8_t *mel; uint8_t sev, tonic, swing, arp, pad, bdec; } mu_song_t;
/* minor, sev: bit c, chord c; tonic: the key's (minor) tonic; swing: % of a sixteenth the off-sixteenths come late;
 * arp: 0 none, 1 up in sixteenths, 2 up and down in eighths, 3 a few high notes; pad: 0 held, 1 stabs on the
 * off-beats, 2 gated eighths; bdec: the bass note's fall (0 held; 12 short .. 14 long) */
static const uint8_t MU_MEL_HEART[32] = {74, 77, 81, 77, 86, 81, 77, 74,  74, 77, 82, 77, 86, 82, 77, 74,
                                         72, 77, 81, 77, 84, 81, 77, 72,  72, 76, 79, 76, 84, 79, 76, 79};
#define MU_NSONG 13u
static const mu_song_t MU_SONGS[MU_NSONG] = {
    {112, {9, 5, 0, 7}, 1, 1, 0, 0, 0, 9, 0, 0, 0, 13},      /* 0 NEON DEPARTURE: A minor, four on the floor, octave bass */
    {110, {4, 0, 7, 2}, 1, 1, 1, 0, 0, 4, 0, 1, 2, 13},      /* 1 NIGHT DRIVE HOME: E minor, driving, octave bass, gated pads, an arp */
    {136, {2, 10, 5, 0}, 1, 8, 8, 0, 0, 2, 0, 0, 1, 12},     /* 2 RAIN RUN: D minor, motorik, galloping bass, stabs */
    {108, {6, 2, 9, 4}, 1, 6, 2, 0, 0, 6, 20, 0, 1, 13},     /* 3 CITY FUNK: F# minor, swung sixteenths, funk bass, stabs */
    {122, {0, 5, 10, 3}, 3, 2, 4, 0, 0, 0, 0, 2, 0, 14},     /* 4 OUTRUN: C minor, breakbeat, arp up and down */
    {104, {11, 7, 2, 9}, 1, 0, 0, 0, 0, 11, 10, 0, 1, 13},   /* 5 DUB TUNNEL: B minor, four on the floor, skank stabs, a pumping bass */
    {112, {7, 0, 3, 2}, 3, 2, 0, 0, 0, 7, 0, 3, 0, 13},      /* 6 ASCENT: G minor, bold, four on the floor, high notes over it */
    {120, {2, 10, 5, 0}, 1, 1, 2, MU_MEL_HEART, 0, 2, 0, 0, 0, 0},   /* 7 THE HEART: D minor, driving, its own melody */
    {106, {5, 4, 2, 0}, 6, 3, 1, 0, 15, 9, 12, 2, 0, 13},    /* 8 THE BEACH: Fmaj7 Em7 Dm7 Cmaj7, bright, a little swing, arp */
    {124, {3, 10, 0, 8}, 4, 4, 7, 0, 0, 0, 0, 1, 1, 12},     /* 9 CHERRY HOUSE: E flat major, house, off-beat bass and hats */
    {110, {1, 9, 4, 11}, 1, 0, 1, 0, 0, 1, 25, 2, 0, 13},    /* 10 LIGHTHOUSE: C# minor, a light shuffle, arp */
    {114, {0, 9, 5, 7}, 2, 3, 2, 0, 7, 9, 6, 1, 1, 13},      /* 11 SEASIDE TRAM (the ride): C major, Cmaj7 Am7 Fmaj7 G, bright and bouncing: hats, stabs, an arp */
    {144, {9, 5, 7, 4}, 9, 8, 2, 0, 0, 9, 0, 1, 1, 12},      /* 12 FESTIVAL TRAM (the ride): A minor, fast, driving, arp, stabs */
};
#define MU_R (-99)                              /* the bass: a rest */
#define MU_H (-98)                              /* the bass: the note held on */
static const int8_t MU_BASSPAT[10][8] = {
    {0, 0, 12, 0, 0, 12, 7, 12}, {0, 12, 0, 12, 0, 12, 0, 12}, {0, 0, 0, 7, 0, 0, 12, 10}, {0, 0, 7, 0, 12, 0, 7, 0},
    {MU_R, 0, MU_R, 0, MU_R, 0, MU_R, 12},                      /* 4 the off-beats (house) */
    {0, MU_H, MU_H, MU_H, MU_H, MU_H, 7, MU_H},                 /* 5 long notes */
    {0, MU_R, 0, 12, MU_R, 10, 0, 7},                           /* 6 funk */
    {0, MU_H, MU_H, MU_R, MU_R, 7, MU_H, MU_R},                 /* 7 dub: room between */
    {0, 0, 12, 0, 0, 0, 12, 0},                                 /* 8 a gallop */
    {0, MU_H, MU_H, MU_H, MU_H, MU_H, MU_H, MU_H},              /* 9 one note a bar */
};
static uint8_t mu_pad[4][3], mu_root[4], mu_drums, mu_epic, mu_swing, mu_arp, mu_padm, mu_bdec, mu_bpm;
static uint8_t mu_mix[4] = {8, 8, 8, 8};          /* the endless line's faders (K1..K4): drums, bass, pads, arp; 0..8 */
static int32_t mu_ofk = 32767;                   /* the endless line: the music's own low-pass (closes in tunnels) */
static uint16_t mu_scale;                       /* bit pc: in the song's key */
static const int8_t *mu_bass = MU_BASSPAT[0];
static const uint8_t *mu_mel;
static struct {
    uint32_t t, step, abs, stp, ph[9], kick_ph, binc, pinc[3], mph[2], minc, len, xb_ring;
    int32_t benv, brest, penv, lpo, lvl, kick_env, kick_inc, sn_env, hat_env, hat_len, pump, lpb, lpp, lph, lpm, menv, gb, gp, gd;
} mu;
#define MU_STEP (mu.stp)                        /* frames a sixteenth (the song's tempo) */
/* the melody's keeping: D minor (the heart's key). A key off the scale moves down to it; a leap of more than nine
 * semitones folds back an octave; the last note lands on the chord's own (D, F or A) so the phrase comes home */
static uint32_t sig_fit(const uint8_t *sg, uint32_t i, uint32_t n)
{
    static const uint8_t DM[12] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0};   /* C D E F G A Bb (pc 0..11) */
    while (!DM[n % 12u]) n--;
    if (i) {
        uint32_t p = sg[i - 1u];
        if (p && n > p + 9u && n >= 65u) n -= 12u;
        if (p && p > n + 9u && n <= 67u) n += 12u;
    }
    if (i == 7u) {                              /* home: the nearest D, F or A */
        uint32_t d, best = n;
        for (d = 0; d < 4u; d++) {
            if ((n - d) % 12u == 2u || (n - d) % 12u == 5u || (n - d) % 12u == 9u) { best = n - d; break; }
            if ((n + d) % 12u == 2u || (n + d) % 12u == 5u || (n + d) % 12u == 9u) { best = n + d; break; }
        }
        n = best;
    }
    return n;
}
/* the heart's song made of it: each bar two of its notes, a chord of D minor that holds the first, the bass on that
 * chord, the drums driving, the melody on top an octave up */
static uint8_t mu_dyn_mel[32];
static mu_song_t mu_dyn;
static uint8_t mu_dyn_ok;
static void mu_build_sig(const uint8_t *sg)
{
    static const uint8_t CR[6] = {2, 10, 5, 0, 7, 9}, CM[6] = {1, 0, 0, 0, 1, 1};   /* Dm Bb F C Gm Am: the order preferred */
    uint32_t b, k, prev = 99;
    mu_dyn.bpm = 120; mu_dyn.bass = 1; mu_dyn.drums = 2; mu_dyn.sev = 0; mu_dyn.tonic = 2; mu_dyn.minor = 0;
    for (b = 0; b < 4u; b++) {
        uint32_t a = sg[2u * b], c = sg[2u * b + 1u], pick = 0;
        {   /* the chord that holds most of the bar's two notes (the first above all), not the last bar's if another does as well */
            int32_t best = -1;
            for (k = 0; k < 6u; k++) {
                uint32_t r = CR[k], t3 = (r + (CM[k] ? 3u : 4u)) % 12u, f5 = (r + 7u) % 12u, pa = a % 12u, pc2 = c % 12u;
                int32_t sc = (pa == r || pa == t3 || pa == f5) * 4 + (pc2 == r || pc2 == t3 || pc2 == f5) * 2 + (k != prev) - (int32_t)k / 4;
                if (sc > best) { best = sc; pick = k; }
            }
        }
        if (b == 0u) pick = 0;                  /* (it starts at home) */
        prev = pick;
        mu_dyn.root[b] = CR[pick];
        if (CM[pick]) mu_dyn.minor |= (uint8_t)(1u << b);
        {
            const uint8_t P[8] = {(uint8_t)(a + 12u), 0, 0, 0, (uint8_t)(c + 12u), 0, 0, 0};   /* (your eight notes as they are: two a bar, held) */
            for (k = 0; k < 8u; k++) mu_dyn_mel[b * 8u + k] = P[k];
        }
    }
    mu_dyn.mel = mu_dyn_mel;
    mu_dyn_ok = 1;
}
static void mu_set_song(uint32_t i)
{
    const mu_song_t *g = i == 7u && mu_dyn_ok ? &mu_dyn : &MU_SONGS[i < MU_NSONG ? i : 0u];   /* (the heart: the player's own, once made) */
    uint32_t c, tonic = g->tonic;
    for (c = 0; c < 4u; c++) {
        uint32_t r = g->root[c], n = 53u + (r + 12u - 53u % 12u) % 12u;   /* the pad's root: F3..E4 */
        mu_pad[c][0] = (uint8_t)n; mu_pad[c][1] = (uint8_t)(n + ((g->minor >> c) & 1u ? 3u : 4u));
        mu_pad[c][2] = (uint8_t)(n + ((g->sev >> c) & 1u ? ((g->minor >> c) & 1u ? 10u : 11u) : 7u));   /* (a seventh in place of the fifth) */
        mu_root[c] = (uint8_t)(28u + (r + 12u - 4u) % 12u);        /* the bass's: E1..D#2 */
    }
    mu_scale = 0;
    { static const uint8_t NM[7] = {0, 2, 3, 5, 7, 8, 10}; for (c = 0; c < 7u; c++) mu_scale |= (uint16_t)(1u << ((tonic + NM[c]) % 12u)); }
    mu_bass = MU_BASSPAT[g->bass < 10u ? g->bass : 0u]; mu_drums = g->drums; mu_mel = g->mel; mu_epic = g->mel != 0;
    mu_swing = g->swing; mu_arp = g->arp; mu_padm = g->pad; mu_bdec = g->bdec; mu_bpm = g->bpm;
    mu.stp = 44100u * 15u / g->bpm;              /* a sixteenth */
}
static int32_t mu_clip16(int32_t v) { return v < -32767 ? -32767 : v > 32767 ? 32767 : v; }
static volatile uint32_t ta_lead_note, ta_lead_n;   /* the key's note (0: none), a new press */
#define LD_ECHO 12720u                          /* an eighth at 104 BPM */
static int16_t ld_echo[LD_ECHO] __attribute__((section(".pool")));
static struct {
    uint32_t inc, tgt, ph[4], lfo, seen, held, pos;
    int32_t amp, fenv, lp1, lp2, vib, ehp, ep;   /* (ep: the e-piano's own fall, from each press) */
    uint32_t lfo2;
} ld;
static uint32_t tr_snap(uint32_t n, uint32_t bar)   /* in tune: a black key moves to the nearest note of the chord */
{
    uint32_t pc = n % 12u, d, k;
    if (mu_scale >> pc & 1u) return n;          /* in the song's key: as it is */
    for (d = 1; d < 6u; d++)                    /* out of it: the nearest note of the chord */
        for (k = 0; k < 3u; k++) {
            if ((n - d) % 12u == mu_pad[bar][k] % 12u) return n - d;
            if ((n + d) % 12u == mu_pad[bar][k] % 12u) return n + d;
        }
    return n;
}
static void tr_lead_block(void)                 /* once a block: the note (snapped to this bar's chord), a new press */
{
    uint32_t n = ta_lead_note;
    if (n) {
        uint32_t t = ta_note(tr_snap(n, (mu.step >> 4) & 3u));
        if (ta_lead_n != ld.seen) {             /* a press: the filter opens again; from silence, no glide */
            ld.seen = ta_lead_n; ld.fenv = 30000; ld.held = 0; ld.ep = 32767;
            if (ld.amp < 2000) ld.inc = t;
        }
        ld.tgt = t;
    }
}
#define LD_NVOICE 6u
static uint8_t ld_voice;                        /* 0 SAW, 1 GLASS (FM bell), 2 PWM, 3 BRASS, 4 E.PIANO (FM tine), 5 CHOIR: the 80s, one SEQ press each */
static int32_t tr_lead_sample(uint32_t i)
{
    int32_t x, k, e, out;
    if (!(i & 15u)) {
        int32_t d = (int32_t)(ld.tgt - ld.inc);
        ld.inc += (uint32_t)(d >> 3);           /* the glide */
        if (ta_lead_note) {
            ld.amp = ld.amp + 3000 > 32767 ? 32767 : ld.amp + 3000;
            if (ld.held < 100000u) ld.held += 16u;
        } else ld.amp = ld.amp > 70 ? ld.amp - 70 : 0;
        ld.fenv -= (ld.fenv - 9000) >> 6;       /* bright on the press, settling */
        ld.vib = ld.held > 11000u ? (ld.held > 30000u ? 256 : (int32_t)(ld.held - 11000u) * 256 / 19000) : 0;
        ld.lfo += TA_HZ(5u) * 16u + TA_HZ(1u) * 8u;
        ld.lfo2 += TA_HZ(1u) * 9u;               /* (the PWM's slow sweep) */
        ld.ep -= ld.ep >> 7;                    /* (the e-piano dies away while held) */
    }
    if (!ld.amp) out = 0;
    else if (ld_voice == 1u) {                  /* GLASS: a sine bent by another at 3.5 times, the bend fading: a bell */
        uint32_t inc = ld.inc;
        int32_t mod;
        ld.ph[0] += inc; ld.ph[1] += inc * 3u + (inc >> 1);
        mod = (ta_sine(ld.ph[1]) * (ld.fenv - 8000)) >> 15;
        out = ((((ta_sine(ld.ph[0] + ((uint32_t)mod << 15)) >> 2) * ld.amp) >> 15) * 17) >> 6;   /* (level with the others) */
    } else if (ld_voice == 2u) {                /* PWM: two pulses, their width sweeping slowly, a little apart: the Juno lead */
        uint32_t inc = ld.inc, duty = 0x80000000u + (uint32_t)(ta_sine(ld.lfo2) << 15);
        ld.ph[0] += inc; ld.ph[1] += inc + (inc >> 9);
        x = ((ld.ph[0] < duty) ? 2900 : -2900) + ((ld.ph[1] < (duty ^ 0x20000000u)) ? 2900 : -2900);   /* (level with the others) */
        k = ld.fenv;
        ld.lp1 += ((x - ld.lp1) * k) >> 15;
        ld.lp2 += ((ld.lp1 - ld.lp2) * k) >> 15;
        out = ((ld.lp2 >> 1) * ld.amp) >> 15;
    } else if (ld_voice == 3u) {                /* BRASS: three saws, the filter swelling open as the note is held */
        uint32_t inc = ld.inc;
        ld.ph[0] += inc; ld.ph[1] += inc + (inc >> 7); ld.ph[2] += inc - (inc >> 7);
        x = (((int32_t)(ld.ph[0] >> 17) - 16384) + ((int32_t)(ld.ph[1] >> 17) - 16384) + ((int32_t)(ld.ph[2] >> 17) - 16384)) >> 2;
        k = 2400 + (int32_t)(ld.held > 6000u ? 6000u : ld.held) * 4;
        ld.lp1 += ((x - ld.lp1) * k) >> 15;
        ld.lp2 += ((ld.lp1 - ld.lp2) * k) >> 15;
        out = ((ld.lp2 >> 1) * ld.amp) >> 15;
    } else if (ld_voice == 4u) {                /* E.PIANO: a tine, FM at one to one, bright on the press, dying away */
        uint32_t inc = ld.inc;
        int32_t mod;
        ld.ph[0] += inc; ld.ph[1] += inc;
        mod = (ta_sine(ld.ph[1]) * (ld.ep >> 2)) >> 15;
        out = (((ta_sine(ld.ph[0] + ((uint32_t)mod << 14)) >> 2) * ld.amp) >> 15) * (6000 + (ld.ep * 3 >> 2)) >> 15;
    } else if (ld_voice == 5u) {                /* CHOIR: wide saws, a dark filter, a slow rise: voices in the distance */
        uint32_t inc = ld.inc;
        int32_t rise = ld.held * 3 > 32767u ? 32767 : (int32_t)(ld.held * 3u);
        ld.ph[0] += inc; ld.ph[1] += inc + (inc >> 6); ld.ph[2] += inc - (inc >> 6); ld.ph[3] += inc * 2u + (inc >> 8);
        x = (((int32_t)(ld.ph[0] >> 17) - 16384) + ((int32_t)(ld.ph[1] >> 17) - 16384) + ((int32_t)(ld.ph[2] >> 17) - 16384) + (((int32_t)(ld.ph[3] >> 17) - 16384) >> 1)) >> 2;
        ld.lp1 += ((x - ld.lp1) * 3600) >> 15;
        ld.lp2 += ((ld.lp1 - ld.lp2) * 3600) >> 15;
        out = ((((ld.lp2 * 3 >> 2) * ld.amp) >> 15) * rise >> 15) * 21 >> 4;
    } else {                                    /* SAW: three saws and a square an octave down, the filter bright on the press */
        uint32_t inc = ld.inc + (uint32_t)((int32_t)(((int64_t)(int32_t)(ld.inc >> 8) * ta_sine(ld.lfo)) >> 15) * ld.vib >> 8);
        ld.ph[0] += inc; ld.ph[1] += inc + (inc >> 8); ld.ph[2] += inc - (inc >> 8); ld.ph[3] += inc >> 1;
        x = ((int32_t)(ld.ph[0] >> 17) - 16384) + ((int32_t)(ld.ph[1] >> 17) - 16384) + ((int32_t)(ld.ph[2] >> 17) - 16384)
            + ((ld.ph[3] >> 31) ? 7000 : -7000);
        x >>= 2;                                /* (+-14000: the filter's sums stay inside 32 bits) */
        k = ld.fenv;
        ld.lp1 += ((x - ld.lp1) * k) >> 15;
        ld.lp2 += ((ld.lp1 - ld.lp2) * k) >> 15;
        out = ((ld.lp2 >> 1) * ld.amp) >> 15;
    }
    e = ld_echo[ld.pos];                        /* the echo: an eighth behind, fading */
    ld_echo[ld.pos] = (int16_t)mu_clip16(out + (e * 13 >> 5));
    if (++ld.pos >= LD_ECHO) ld.pos = 0;
    return (out * 2 >> 2) + (e * 6 >> 5);       /* (your keys under the backing, not over it) */
}
static int32_t mu_clip(int32_t v) { return v < 0 ? 0 : v > 32767 ? 32767 : v; }
static int32_t mu_hp = -1;                       /* the heart's song: how far it has built, 0..256 (-1: not this song) */
static void tr_music_block(uint32_t kmh_i)        /* once a block: the level, the layers */
{
    int32_t tgt = ta_music ? (mu_epic ? 32767 : mu_clip(((int32_t)kmh_i - 4) * 700)) : 0;   /* (the heart's: all of it, all the way) */
    if (mu_hp >= 0 && ta_music) {               /* the heart's run: it builds as you go */
        mu.lvl += (32767 - mu.lvl) >> 4;
        mu.gp = mu_clip(9000 + mu_hp * 92);
        mu.gb = mu_hp < 50 ? 0 : mu_clip((mu_hp - 50) * 260);
        mu.gd = mu_hp < 110 ? 0 : mu_clip((mu_hp - 110) * 300);
        return;
    }
    mu.lvl += tgt > mu.lvl ? (tgt - mu.lvl > 260 ? 260 : tgt - mu.lvl) : -(mu.lvl - tgt > 90 ? 90 : mu.lvl - tgt);
    mu.gb = mu.lvl;
    mu.gp = mu_clip(mu.lvl * 4 / 3 - 6500);
    mu.gd = mu_clip(mu.lvl * 8 / 5 - 13000);
    if (tr_mode == TRM_ENDLESS) {               /* the endless line: your faders; the tunnels close the music in */
        int32_t kt = ta_tunnel ? 2600 : 32767;
        mu.gd = mu.gd * mu_mix[0] / 8; mu.gb = mu.gb * mu_mix[1] / 8; mu.gp = mu.gp * mu_mix[2] / 8;
        mu_ofk += kt > mu_ofk ? (kt - mu_ofk > 500 ? 500 : kt - mu_ofk) : -(mu_ofk - kt > 700 ? 700 : mu_ofk - kt);
    } else mu_ofk = 32767;
}
static int32_t tr_music_sample(void)
{
    int32_t out = 0, ns, k, x;
    if (++mu.t >= mu.len) {                     /* a sixteenth: the drums, the bass, the arp, the chord */
        uint32_t s16, bar, kick = 0, sn = 0, hv = 0, hl = 7, snv = 12000, kv = 30000, pv = 6000;
        mu.t = 0;
        mu.step = (mu.step + 1u) & 63u;
        mu.abs++;
        s16 = mu.step & 15u; bar = mu.step >> 4;
        mu.len = MU_STEP;                       /* swing: the on-sixteenth long, the off one short (a pair stays two) */
        if (mu_swing) { uint32_t sw = MU_STEP * mu_swing / 100u; mu.len = (s16 & 1u) ? MU_STEP - sw : MU_STEP + sw; }
        switch (mu_drums) {                     /* the song's beat */
        case 0: kick = !(s16 & 3u); sn = s16 == 4u || s16 == 12u; if (s16 & 1u) hv = 5000; break;                 /* four on the floor */
        case 1: kick = !(s16 & 3u) || s16 == 14u; sn = s16 == 4u || s16 == 12u; if ((s16 & 3u) == 2u) hv = 6000; break;
        case 2: kick = !(s16 & 3u); sn = s16 == 4u || s16 == 12u; hv = (s16 & 3u) == 2u ? 6500 : 3500; hl = (s16 & 3u) == 2u ? 10 : 6; break;
        case 3: kick = s16 == 0u || s16 == 10u; sn = s16 == 8u; snv = 9000; pv = 14000; if (!(s16 & 1u)) { hv = 2600; hl = 8; } break;   /* half time */
        case 4: kick = s16 == 0u || s16 == 7u || s16 == 10u; sn = s16 == 4u || s16 == 12u || s16 == 15u; if (s16 == 15u) snv = 3500;  /* breakbeat */
                if (!(s16 & 1u)) hv = 4500;
                break;
        case 5: kick = s16 == 8u; sn = s16 == 8u; snv = 7000; pv = 12000; if ((s16 & 3u) == 2u) { hv = 4200; hl = 9; } else if (s16 & 1u) hv = 1500; break;   /* one drop */
        case 6: kick = s16 == 0u; kv = 20000; if (s16 == 4u || s16 == 12u) { hv = 2600; hl = 9; } break;          /* sparse */
        case 7: kick = !(s16 & 3u); sn = s16 == 4u || s16 == 12u; snv = 10000;                                   /* house: open hats */
                if ((s16 & 3u) == 2u) { hv = 7000; hl = 12; } else if (s16 & 1u) { hv = 1800; hl = 5; }
                break;
        default: kick = s16 == 0u || s16 == 8u || s16 == 10u; sn = s16 == 4u || s16 == 12u; hv = (s16 & 1u) ? 2400 : 5000; break;   /* motorik */
        }
        if (kick) { mu.kick_env = mu.gd ? (int32_t)kv : 0; mu.kick_inc = (int32_t)TA_HZ(150u); mu.pump = (int32_t)pv; }
        if (sn && mu.gd) mu.sn_env = (int32_t)snv;
        if (hv && mu.gd > 12000) { mu.hat_env = (int32_t)hv; mu.hat_len = (int32_t)hl; }
        if (!(s16 & 1u)) {                      /* the bass: a note, held on, or a rest */
            int32_t bv = mu_bass[s16 >> 1];
            if (bv == MU_R) mu.brest = 1;
            else if (bv != MU_H) { mu.binc = ta_note((uint32_t)(mu_root[bar] + bv)); mu.benv = 32767; mu.brest = 0; }
            if (tr_mode == TRM_ENDLESS && ta_xing) mu.xb_ring++;   /* (the endless line: the crossing rings in time) */
        }
        for (k = 0; k < 3; k++) mu.pinc[k] = ta_note(mu_pad[bar][k]);
        if (mu_padm == 1u ? (s16 & 3u) == 2u : mu_padm == 2u ? !(s16 & 1u) : 0) mu.penv = 32767;   /* the pads' stabs, gate */
        if (mu_mel && !(s16 & 1u) && mu_mel[bar * 8u + (s16 >> 1)] && (mu_hp < 0 || mu_hp > 175)) {   /* (the heart's run: the melody comes late) */ mu.minc = ta_note(mu_mel[bar * 8u + (s16 >> 1)]); mu.menv = 26000; }
        else if (mu_arp && !mu_mel) {           /* the arp: chord notes above the pad */
            static const uint8_t UD[4] = {0, 1, 2, 1};
            int32_t lv = 15000 * mu_mix[3] / 8, tone = -1;
            if (mu_arp == 1u) tone = mu_pad[bar][s16 % 3u] + 12;
            else if (mu_arp == 2u && !(s16 & 1u)) tone = mu_pad[bar][UD[(s16 >> 1) & 3u]] + 12;
            else if (mu_arp == 3u && (s16 == 0u || s16 == 3u || s16 == 6u || s16 == 10u || s16 == 13u)) tone = mu_pad[bar][(s16 * 7u) % 3u] + 24;
            if (tone >= 0 && lv) { mu.minc = ta_note((uint32_t)tone); mu.menv = lv; }
        }
    }
    if (!mu.lvl && !mu.kick_env) return 0;     /* (silent: the clock still runs, for the beat) */
    ns = ta_noise();
    mu.pump += (32767 - mu.pump) >> 10;         /* the side-chain: down on the kick, back over a beat */
    if (mu.gb) {                                /* the bass: two saws a little apart, a sub an octave down */
        mu.ph[0] += mu.binc; mu.ph[1] += mu.binc + (mu.binc >> 7); mu.ph[2] += mu.binc >> 1;
        x = ((int32_t)(mu.ph[0] >> 17) - 16384) + ((int32_t)(mu.ph[1] >> 17) - 16384) + (ta_sine(mu.ph[2]) >> 1);
        mu.lpb += ((x - mu.lpb) * (3000 + mu.lvl / 5)) >> 15;
        if (mu.brest) mu.benv -= (mu.benv >> 8) + (mu.benv > 0);                 /* a rest: let go */
        else if (mu_bdec) mu.benv -= (mu.benv >> mu_bdec) + (mu.benv > 0);       /* a plucked note */
        else mu.benv = 32767;
        out += (((((mu.lpb * mu.gb) >> 15) * 1500 >> 14) * mu.pump >> 15) * mu.benv) >> 15;
    }
    if (mu.gp) {                                /* the pad: three notes, two saws each, the filter opening with speed */
        x = 0;
        for (k = 0; k < 3; k++) {
            mu.ph[3 + 2 * k] += mu.pinc[k]; mu.ph[4 + 2 * k] += mu.pinc[k] + (mu.pinc[k] >> 8) + (mu.pinc[k] >> 10);
            x += ((int32_t)(mu.ph[3 + 2 * k] >> 17) - 16384) + ((int32_t)(mu.ph[4 + 2 * k] >> 17) - 16384);
        }
        mu.lpp += ((x / 3 - mu.lpp) * (900 + mu.lvl / 3)) >> 15;
        if (mu_padm) mu.penv -= (mu.penv >> 12) + (mu.penv > 0); else mu.penv = 32767;
        out += (((((mu.lpp * mu.gp) >> 15) * 1700 >> 14) * (12000 + (mu.pump * 20 >> 5)) >> 15) * mu.penv) >> 15;
    }
    if (mu.menv && mu.gp) {                     /* the heart's melody: two saws apart, bright, each note fading */
        mu.mph[0] += mu.minc; mu.mph[1] += mu.minc + (mu.minc >> 8);
        x = (((int32_t)(mu.mph[0] >> 17) - 16384) + ((int32_t)(mu.mph[1] >> 17) - 16384)) >> 1;
        mu.lpm += ((x - mu.lpm) * (6000 + (mu.menv >> 2))) >> 15;
        out += ((((mu.lpm * mu.menv) >> 15) * mu.gp) >> 15) * (mu_mel ? (mu_hp >= 0 ? 3 : 6) : 3) >> 3;   /* (a melody out in front; an arp behind; the heart's run: its melody as loud as your keys, no more) */
        mu.menv -= (mu.menv >> (mu_mel ? 14 : 10)) + 1;   /* (a melody's notes held; the arp's short) */
        if (mu.menv < 0) mu.menv = 0;
    }
    if (mu.kick_env) {                          /* the kick: a sine falling in pitch */
        mu.kick_ph += (uint32_t)mu.kick_inc;
        mu.kick_inc -= mu.kick_inc >> 9;
        out += (((ta_sine(mu.kick_ph) * mu.kick_env) >> 15) * mu.gd >> 15) >> 2;
        mu.kick_env -= (mu.kick_env >> 10) + 1;
        if (mu.kick_env < 0) mu.kick_env = 0;
    }
    if (mu.sn_env) {                            /* a clap: noise, a short tail */
        out += (((ns * mu.sn_env) >> 15) * mu.gd >> 15) * 5 >> 4;
        mu.sn_env -= (mu.sn_env >> 9) + 1;
        if (mu.sn_env < 0) mu.sn_env = 0;
    }
    if (mu.hat_env) {                           /* a hat: the noise's top, very short */
        mu.lph += (ns - mu.lph) >> 2;
        out += ((((ns - mu.lph) * mu.hat_env) >> 15) * mu.gd >> 15) >> 1;
        mu.hat_env -= (mu.hat_env >> mu.hat_len) + 1;
        if (mu.hat_env < 0) mu.hat_env = 0;
    }
    if (mu_ofk < 32767) { mu.lpo += ((out - mu.lpo) * mu_ofk) >> 15; out = mu.lpo; }   /* (the endless line, in a tunnel) */
    return out;
}

static int32_t mu_master = 65536, mu_fade = 0;   /* the music's master level (Q16) and its fade a block (+ up, - down, 0 still) */
static void tr_audio(int32_t *out, uint32_t n)
{
    uint32_t i, k8 = ta_kmh8, kmh_i = k8 >> 8, load = ta_load, hum_inc, car_inc, P, jstep, hn1, hn2, wn1, wn2, xn1, xn2;
    int32_t wh_t, hum_t, roll_t, sq_t;
    hum_inc = k8 * 609u;
    P = kmh_i < 6u ? 0u : kmh_i < 15u ? 27u : kmh_i < 30u ? 15u : kmh_i < 45u ? 9u : kmh_i < 60u ? 5u : kmh_i < 75u ? 3u : 1u;
    car_inc = P ? hum_inc * P : TA_HZ(750u);
    wh_t = P == 1u ? 0 : (int32_t)(load * 2600u >> 8);
    hum_t = k8 ? (int32_t)(load * (1200u + (kmh_i > 80u ? 80u : kmh_i) * 25u) >> 8) : 0;
    roll_t = (int32_t)(kmh_i > 110u ? 110u : kmh_i) * 32;
    if (ta_tunnel) roll_t = roll_t * 5 / 4;
    ta.roll_k = 1500 + (int32_t)(kmh_i > 120u ? 120u : kmh_i) * 60;
    sq_t = ta_brake && k8 && kmh_i < 10u ? (int32_t)((10u * 256u - k8) * 1300u / 2560u) : 0;
    jstep = k8 * 423u >> 10;
    if (mu_fade) {                              /* a fade: in, or out to silence */
        mu_master += mu_fade;
        if (mu_master >= 65536) { mu_master = 65536; mu_fade = 0; }
        if (mu_master <= 0) { mu_master = 0; mu_fade = 0; ta_music = 0; }
    }
    tr_music_block(kmh_i);
    tr_lead_block();
    {   /* the horn, the whistle, the crossing: the line's own pitches, or (the endless line) the chord's */
        uint32_t bar = (mu.step >> 4) & 3u, en = tr_mode == TRM_ENDLESS;
        hn1 = en ? ta_note((uint32_t)mu_root[bar] + 24u) : TA_HZ(311u); hn2 = en ? ta_note((uint32_t)mu_root[bar] + 31u) : TA_HZ(370u);
        wn1 = en ? ta_note((uint32_t)mu_pad[bar][0] + 24u) : TA_HZ(880u); wn2 = en ? ta_note((uint32_t)mu_pad[bar][2] + 24u) : TA_HZ(1047u);
        xn1 = en ? ta_note((uint32_t)mu_pad[bar][1] + 24u) : TA_HZ(700u); xn2 = en ? ta_note((uint32_t)mu_pad[bar][1] + 36u) : TA_HZ(1750u);
        if (en) {                               /* the bells on the eighths, in time */
            if (mu.xb_ring != ta.xb_seen) { ta.xb_seen = mu.xb_ring; ta.xb_env = 9000; }
        }
    }
    if (ta_hiss_n != ta.hiss_seen) { ta.hiss_seen = ta_hiss_n; ta.hiss_env = 5200; }
    if (ta_thunk_n != ta.thunk_seen) { ta.thunk_seen = ta_thunk_n; ta.thunk_env = 9000; ta.thunk_ph = 0; }
    if (ta_pass_n != ta.pass_seen) { ta.pass_seen = ta_pass_n; ta.pass_t = 1; }
    if (ta_mel_n != ta.mel_seen) { ta.mel_seen = ta_mel_n; ta.seq = TA_DEPART; ta.seq_left = 0; ta.seq_step = 5200u; }
    if (ta_chime_n != ta.chime_seen) { ta.chime_seen = ta_chime_n; if (ta_chime) { ta.seq = ta_chime; ta.seq_left = 0; ta.seq_step = 4200u; } }
    if (ta_bell) {
        if (ta.bell_t == 0) { ta.bell_env = 5000; ta.bell_ph1 = ta.bell_ph2 = 0; }
        ta.bell_t += n;
        if (ta.bell_t >= 19000u) ta.bell_t = 0;
    } else ta.bell_t = 0;
    if (ta_xing && tr_mode != TRM_ENDLESS) {
        if (ta.xb_t == 0) ta.xb_env = (int32_t)ta_xing;
        ta.xb_t += n;
        if (ta.xb_t >= 16000u) ta.xb_t = 0;
    } else ta.xb_t = 0;
    for (i = 0; i < n; i++) {
        int32_t s = 0, ns = ta_noise();
        uint32_t jp0, v;
        if (!(i & 15u)) {
            ta.hum_amp = ta_slew(ta.hum_amp, hum_t, 40);
            ta.wh_amp = ta_slew(ta.wh_amp, wh_t, 40);
            ta.roll_amp = ta_slew(ta.roll_amp, roll_t, 30);
            ta.sq_amp = ta_slew(ta.sq_amp, sq_t, 20);
            ta.horn_amp = ta_slew(ta.horn_amp, ta_horn ? 7000 : 0, ta_horn ? 300 : 120);
            ta.wh_amp2 = ta_slew(ta.wh_amp2, ta_whistle ? 5200 : 0, ta_whistle ? 200 : 90);
            if (ta.seq) {
                if (ta.seq_left <= 16u) {
                    uint32_t note = ta.seq[0], len = ta.seq[1];
                    if (!len) ta.seq = 0;
                    else {
                        if (note) { v = ta.mv_next++ & 1u; ta.mv_inc[v] = ta_note(note); ta.mv_ph[v] = 0; ta.mv_env[v] = 6500; }
                        ta.seq += 2;
                        ta.seq_left += len * ta.seq_step;
                    }
                }
                if (ta.seq_left > 16u) ta.seq_left -= 16u;
            }
        }
        if (ta.hum_amp) { ta.hum_ph += hum_inc; s += (ta_sine(ta.hum_ph) * ta.hum_amp + ta_sine(ta.hum_ph * 6u) * (ta.hum_amp >> 2)) >> 15; }
        if (ta.wh_amp) {
            int32_t w;
            ta.car_ph += car_inc;
            ta.car_ph2 += car_inc + (car_inc >> 7);
            w = ta_sine(ta.car_ph) + (ta_sine(ta.car_ph2) >> 1) + (((int32_t)(ta.car_ph >> 17) - 16384) >> 2);
            s += (w * ta.wh_amp) >> 16;
        }
        if (ta.roll_amp) { ta.roll_lp += ((ns - ta.roll_lp) * ta.roll_k) >> 15; s += (ta.roll_lp * ta.roll_amp) >> 14; }   /* (the roll: low, not a hiss over the music) */
        jp0 = ta.jp;
        ta.jp += jstep;
        if (ta.jp >= 419430400u) ta.jp -= 419430400u;
        if (jstep && ((jp0 > ta.jp) || (jp0 < 35232153u && ta.jp >= 35232153u) ||
                      (jp0 < 231525581u && ta.jp >= 231525581u) || (jp0 < 266757734u && ta.jp >= 266757734u))) {
            ta.clk_env = 3000 + (int32_t)(kmh_i > 90u ? 90u : kmh_i) * 80;
            ta.clk_ph = 0;
        }
        if (ta.clk_env) {
            ta.clk_ph += TA_HZ(95u);
            s += (ta_sine(ta.clk_ph) * ta.clk_env >> 15) + ((ns * ta.clk_env) >> 15);
            ta.clk_env -= (ta.clk_env >> 8) + 1;
            if (ta.clk_env < 0) ta.clk_env = 0;
        }
        if (ta.pass_t) {
            int32_t pa = ta.pass_t < 13000u ? (int32_t)ta.pass_t * 9 / 13 : (int32_t)(53000u - ta.pass_t) * 9 / 40;
            ta.pass_lp += ((ns - ta.pass_lp) * 5000) >> 15;
            s += (ta.pass_lp * pa) >> 12;
            if (++ta.pass_t >= 53000u) ta.pass_t = 0;
        }
        if (ta.sq_amp) {
            ta.sq_lfo += TA_HZ(6u);
            ta.sq_ph += TA_HZ(2650u) + (uint32_t)(ta_sine(ta.sq_lfo) * 640);
            s += (ta_sine(ta.sq_ph) * ta.sq_amp) >> 15;
        }
        if (ta.hiss_env) {
            ta.hiss_lp += (ns - ta.hiss_lp) >> 2;
            s += ((ns - ta.hiss_lp) * ta.hiss_env) >> 13;
            ta.hiss_env -= (ta.hiss_env >> 13) + 1;
            if (ta.hiss_env < 0) ta.hiss_env = 0;
        }
        if (ta.thunk_env) {
            ta.thunk_ph += TA_HZ(55u);
            s += ta_sine(ta.thunk_ph) * ta.thunk_env >> 15;
            ta.thunk_env -= (ta.thunk_env >> 10) + 1;
            if (ta.thunk_env < 0) ta.thunk_env = 0;
        }
        if (ta.wh_amp2) {                       /* the whistle: a high two-note pipe, breathy */
            int32_t h;
            ta.wh_ph1 += wn1; ta.wh_ph2 += wn2;
            h = (ta_sine(ta.wh_ph1) + ta_sine(ta.wh_ph2)) / 2 + (ns >> 3);
            ta.wh_lp += ((h - ta.wh_lp) * 14000) >> 15;
            s += (ta.wh_lp * ta.wh_amp2) >> 15;
        }
        if (ta.horn_amp) {
            int32_t h;
            ta.horn_ph1 += hn1;
            ta.horn_ph2 += hn2;
            h = ((int32_t)(ta.horn_ph1 >> 17) - 16384) + ((int32_t)(ta.horn_ph2 >> 17) - 16384);
            ta.horn_lp += ((h - ta.horn_lp) * 9000) >> 15;
            s += (ta.horn_lp * ta.horn_amp) >> 15;
        }
        if (ta.bell_env) {
            ta.bell_ph1 += TA_HZ(1480u);
            ta.bell_ph2 += TA_HZ(3870u);
            s += ((ta_sine(ta.bell_ph1) + (ta_sine(ta.bell_ph2) >> 1)) * ta.bell_env) >> 15;
            ta.bell_env -= (ta.bell_env >> 11) + 1;
            if (ta.bell_env < 0) ta.bell_env = 0;
        }
        if (ta.xb_env) {
            ta.xb_ph1 += xn1;
            ta.xb_ph2 += xn2;
            s += ((ta_sine(ta.xb_ph1) + (ta_sine(ta.xb_ph2) >> 2)) * ta.xb_env) >> 15;
            ta.xb_env -= (ta.xb_env >> 10) + 1;
            if (ta.xb_env < 0) ta.xb_env = 0;
        }
        for (v = 0; v < 2u; v++)
            if (ta.mv_env[v]) {
                ta.mv_ph[v] += ta.mv_inc[v];   /* a soft square-ish lead: the synth of the line */
                s += ((ta_sine(ta.mv_ph[v]) + (ta_sine(ta.mv_ph[v] * 3u) / 3) + (ta_sine(ta.mv_ph[v] * 2u) >> 3)) * ta.mv_env[v]) >> 15;   /* (its overtones in tune: the phrase you copy sounds as your keys do) */
                ta.mv_env[v] -= (ta.mv_env[v] >> 13) + 1;
                if (ta.mv_env[v] < 0) ta.mv_env[v] = 0;
            }
        if (tr_mode != TRM_ENODEN) s = s * 2 / 5 + ((int32_t)(((int64_t)tr_music_sample() * mu_master) >> 16) + tr_lead_sample(i)) * 3 / 2;   /* (the music first, the train under it; the Enoden: the train, no music) */
        out[2u * i] += s;
        out[2u * i + 1u] += s;
    }
}

/* ======================================================================== the view's look: one palette a sky */
typedef struct {
    uint16_t top, mid, hor, fog;                /* sky gradient (top -> mid -> horizon), the haze */
    uint16_t amb;                               /* ambient light 0..256 */
    uint8_t sun, fuji, stars, rain, lit;        /* a striped synthwave sun, the mountain, stars, rain, windows lit */
    uint16_t hill_far, hill_near;
    uint16_t grass_a, grass_b, conc_a, conc_b, ballast, sleeper, rail, wall, asphalt, sand, sea_a, sea_b, yard;
    uint16_t neon1, neon2;                      /* signs, the rails' glint */
} tr_pal_t;
static const tr_pal_t TR_PAL[6] = {
    /* DAY: a clear, slightly lilac day */
    {RGB(60, 110, 210), RGB(130, 170, 235), RGB(220, 200, 235), RGB(200, 195, 230), 256, 0, 1, 0, 0, 0,
     RGB(110, 110, 170), RGB(60, 110, 90), RGB(70, 140, 80), RGB(86, 156, 92), RGB(150, 146, 150), RGB(136, 132, 140),
     RGB(120, 110, 108), RGB(80, 66, 60), RGB(205, 205, 220), RGB(70, 64, 72), RGB(80, 78, 90), RGB(222, 204, 170),
     RGB(40, 110, 190), RGB(56, 128, 204), RGB(96, 124, 84), RGB(255, 60, 200), RGB(80, 230, 255)},
    /* DUSK: synthwave. Indigo above, magenta, a hot horizon, the striped sun behind the mountain */
    {RGB(24, 10, 58), RGB(120, 30, 120), RGB(255, 110, 140), RGB(150, 60, 130), 190, 1, 1, 0, 0, 1,
     RGB(70, 30, 100), RGB(30, 16, 50), RGB(40, 40, 70), RGB(48, 46, 84), RGB(86, 70, 110), RGB(76, 62, 100),
     RGB(80, 60, 86), RGB(50, 34, 56), RGB(255, 160, 220), RGB(46, 30, 60), RGB(44, 36, 66), RGB(160, 110, 140),
     RGB(60, 30, 110), RGB(90, 40, 140), RGB(54, 44, 80), RGB(255, 60, 200), RGB(80, 230, 255)},
    /* NIGHT: neon city, stars */
    {RGB(6, 4, 22), RGB(28, 12, 60), RGB(80, 26, 100), RGB(30, 14, 52), 80, 0, 1, 1, 0, 1,
     RGB(36, 16, 60), RGB(14, 8, 28), RGB(20, 24, 40), RGB(26, 30, 48), RGB(54, 46, 70), RGB(46, 40, 62),
     RGB(60, 50, 70), RGB(34, 26, 40), RGB(140, 230, 255), RGB(30, 22, 40), RGB(30, 26, 44), RGB(90, 70, 90),
     RGB(16, 20, 60), RGB(24, 30, 80), RGB(30, 30, 46), RGB(255, 40, 190), RGB(60, 230, 255)},
    /* RAIN: night, rain falling, everything wet */
    {RGB(10, 8, 30), RGB(40, 20, 70), RGB(110, 40, 120), RGB(50, 26, 76), 90, 0, 0, 0, 1, 1,
     RGB(40, 22, 66), RGB(18, 12, 34), RGB(22, 26, 44), RGB(28, 32, 52), RGB(60, 52, 80), RGB(52, 46, 72),
     RGB(64, 54, 76), RGB(36, 28, 44), RGB(180, 240, 255), RGB(32, 24, 44), RGB(40, 34, 60), RGB(90, 70, 90),
     RGB(18, 22, 64), RGB(28, 34, 86), RGB(34, 34, 52), RGB(255, 50, 200), RGB(80, 240, 255)},
    /* SUNSET: orange into rose */
    {RGB(60, 30, 100), RGB(210, 70, 110), RGB(255, 170, 90), RGB(220, 120, 110), 210, 1, 1, 0, 0, 0,
     RGB(110, 50, 110), RGB(50, 30, 60), RGB(60, 90, 70), RGB(74, 104, 80), RGB(150, 120, 130), RGB(136, 110, 120),
     RGB(120, 96, 100), RGB(70, 50, 54), RGB(255, 200, 200), RGB(70, 54, 64), RGB(70, 60, 80), RGB(230, 180, 150),
     RGB(120, 60, 120), RGB(150, 80, 140), RGB(80, 90, 70), RGB(255, 80, 180), RGB(255, 210, 140)},
    /* DAWN: pale pink and blue, mist */
    {RGB(90, 120, 200), RGB(190, 160, 220), RGB(255, 200, 210), RGB(230, 200, 220), 230, 0, 1, 0, 0, 0,
     RGB(150, 130, 190), RGB(80, 90, 120), RGB(80, 130, 100), RGB(94, 146, 112), RGB(170, 160, 175), RGB(156, 148, 164),
     RGB(130, 120, 125), RGB(84, 70, 70), RGB(230, 220, 240), RGB(80, 74, 84), RGB(100, 96, 112), RGB(232, 214, 190),
     RGB(90, 130, 200), RGB(110, 150, 214), RGB(100, 130, 100), RGB(255, 80, 200), RGB(120, 220, 255)},
};
static const tr_pal_t EN_PAL[6] = {        /* the Enoden: a coast day as it is */
    /* DAY */
    {RGB(70, 130, 220), RGB(140, 185, 240), RGB(225, 235, 245), RGB(215, 225, 235), 256, 0, 1, 0, 0, 0,
     RGB(120, 140, 165), RGB(70, 110, 70), RGB(80, 140, 60), RGB(92, 152, 70), RGB(160, 160, 160), RGB(150, 150, 150),
     RGB(130, 125, 115), RGB(90, 75, 60), RGB(185, 185, 190), RGB(110, 105, 100), RGB(90, 90, 95), RGB(225, 210, 170),
     RGB(40, 110, 170), RGB(60, 130, 190), RGB(110, 120, 90), RGB(215, 215, 220), RGB(200, 200, 210)},
    /* DUSK */
    {RGB(40, 60, 110), RGB(120, 110, 150), RGB(230, 150, 110), RGB(170, 130, 130), 200, 0, 1, 0, 0, 1,
     RGB(90, 90, 120), RGB(50, 70, 60), RGB(60, 100, 60), RGB(70, 110, 66), RGB(130, 125, 130), RGB(120, 115, 120),
     RGB(110, 100, 95), RGB(70, 60, 50), RGB(170, 170, 175), RGB(90, 85, 85), RGB(70, 70, 76), RGB(190, 170, 140),
     RGB(40, 70, 120), RGB(60, 90, 140), RGB(90, 95, 75), RGB(210, 205, 200), RGB(195, 190, 190)},
    /* NIGHT */
    {RGB(10, 14, 30), RGB(25, 35, 60), RGB(50, 60, 90), RGB(30, 36, 54), 90, 0, 0, 1, 0, 1,
     RGB(30, 36, 54), RGB(16, 22, 24), RGB(24, 40, 28), RGB(28, 46, 32), RGB(60, 60, 64), RGB(54, 54, 58),
     RGB(60, 56, 52), RGB(34, 30, 26), RGB(150, 150, 156), RGB(40, 40, 42), RGB(36, 36, 40), RGB(90, 84, 70),
     RGB(14, 24, 44), RGB(20, 32, 56), RGB(36, 40, 34), RGB(230, 225, 210), RGB(220, 220, 230)},
    /* RAIN */
    {RGB(90, 95, 105), RGB(130, 135, 145), RGB(160, 165, 170), RGB(150, 155, 160), 170, 0, 0, 0, 1, 0,
     RGB(110, 115, 120), RGB(60, 80, 66), RGB(64, 100, 60), RGB(72, 108, 66), RGB(120, 120, 124), RGB(112, 112, 116),
     RGB(100, 98, 94), RGB(64, 56, 48), RGB(170, 172, 178), RGB(84, 84, 86), RGB(66, 66, 70), RGB(170, 160, 140),
     RGB(60, 80, 100), RGB(76, 96, 116), RGB(80, 90, 74), RGB(200, 200, 205), RGB(185, 185, 195)},
    /* SUNSET */
    {RGB(70, 90, 160), RGB(220, 140, 110), RGB(255, 190, 120), RGB(230, 170, 140), 210, 0, 1, 0, 0, 0,
     RGB(120, 100, 130), RGB(60, 70, 60), RGB(70, 100, 60), RGB(80, 110, 66), RGB(150, 135, 130), RGB(140, 126, 120),
     RGB(120, 105, 95), RGB(80, 62, 50), RGB(200, 190, 185), RGB(100, 90, 86), RGB(80, 74, 76), RGB(230, 190, 150),
     RGB(80, 90, 140), RGB(110, 110, 150), RGB(100, 100, 80), RGB(230, 220, 205), RGB(215, 205, 195)},
    /* DAWN: the afternoon, warm */
    {RGB(90, 140, 215), RGB(170, 195, 230), RGB(250, 225, 190), RGB(235, 220, 200), 240, 0, 1, 0, 0, 0,
     RGB(140, 150, 170), RGB(80, 115, 75), RGB(90, 145, 65), RGB(100, 155, 75), RGB(170, 165, 160), RGB(160, 156, 150),
     RGB(140, 130, 118), RGB(95, 78, 62), RGB(195, 192, 190), RGB(115, 108, 100), RGB(95, 92, 95), RGB(232, 214, 176),
     RGB(50, 115, 175), RGB(70, 135, 195), RGB(115, 125, 92), RGB(225, 220, 215), RGB(210, 205, 205)},
};
static const tr_pal_t HEART_PAL = {             /* the run to the heart: red dark, a place never meant for passengers */
    RGB(20, 0, 6), RGB(60, 4, 14), RGB(120, 10, 24), RGB(50, 4, 12), 70, 0, 0, 0, 0, 0,
    RGB(40, 4, 10), RGB(20, 2, 6), RGB(30, 6, 8), RGB(36, 8, 10), RGB(60, 20, 24), RGB(54, 16, 20),
    RGB(50, 16, 18), RGB(30, 8, 10), RGB(200, 60, 70), RGB(40, 10, 14), RGB(34, 8, 12), RGB(90, 30, 30),
    RGB(30, 4, 12), RGB(44, 6, 16), RGB(36, 10, 12), RGB(255, 40, 60), RGB(255, 90, 90)};
static uint8_t tr_heart;                         /* this run goes to (or from) the heart */
static uint32_t h_pulse, h_ab;                   /* the heart's run, this frame: the lamps' beat, the walls' abstraction (0..256) */
static uint8_t tr_white;                         /* the heart's run done: it went into the light (no arrival) */
/* the run to the heart changes as it goes (mu_hp, 0..256: how far it has built):
 *   0   a tunnel like any other, the cab as always
 *   30  the tunnel's lamps pulse with the music
 *   80  the walls give way to lines of light: portals, then diamonds
 *   140 the keys beat like a heart
 *   185 the cab goes, piece by piece: the lamps, the notches, the distance, the arrow, the dial, the walls, the speed
 *   256 no limit: full power, nothing but light, and the white */
#define HS_PULSE 30
#define HS_ABSTRACT 80
#define HS_BEAT 140
#define HS_FADE 185
static uint32_t tr_gone;                         /* the cab's pieces gone (0..7) */
static uint8_t tr_trim;                          /* the cab's trim: 0 the train's, 1 the seaside tram's (varnish, brass), 2 the festival tram's (lacquer, gold) */
static int32_t tr_hs(void) { return tr_heart && mu_hp >= 0 ? mu_hp : -1; }
static const tr_pal_t *tp;

/* ======================================================================== the 3D view, straight into the frame */
#define TV_W 240
#define TV_H 180
#define TV_Y 0                                  /* on screen: the windscreen fills the screen above the strip */
#define TV_HY 78                                /* the horizon */
#define TV_GH (TV_H - TV_HY)
#define TV_KZ 390000
#define TV_ZFAR 380000
#define TSW(c) ((uint16_t)((((c) >> 8) & 0xFFu) | (((c) & 0xFFu) << 8)))
static uint16_t *tvb;                           /* the frame's row TV_Y */
static uint8_t tv_cached;                       /* the view's copy is current (0: draw it this frame) */
static int32_t tz_mm[TV_GH], toff[TV_GH], tcx[TV_GH], tR2;
static uint8_t tmtn_far[256], tmtn_near[256], tv_ready;

static inline int32_t tjpx(int32_t x_mm, int32_t jj8) { return (int32_t)(((int64_t)x_mm * jj8 * 56) >> 24); }   /* (a lower eye than DENSHA FM1: 1.17 m) */
static uint32_t th32(uint32_t x) { x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16; return x; }

static void tv_init(void)
{
    uint32_t r, x, h = 0x1234567u;
    if (tv_ready) return;
    for (r = 0; r < TV_GH; r++) tz_mm[r] = TV_KZ / (int32_t)(r + 1u);
    for (x = 0; x < 256u; x++) {
        int32_t t1 = (int32_t)((x * 3u) & 127u) - 64, t2 = (int32_t)((x * 7u + 40u) & 127u) - 64, t3 = (int32_t)((x * 13u + 90u) & 127u) - 64;
        t1 = t1 < 0 ? -t1 : t1; t2 = t2 < 0 ? -t2 : t2; t3 = t3 < 0 ? -t3 : t3;
        tmtn_far[x] = (uint8_t)(2 + (t1 * 9 + t2 * 4 + t3 * 2) / 64);
        h = h * 1103515245u + 12345u;
        tmtn_near[x] = (uint8_t)(1 + (t2 * 3 + t3 * 3) / 64 + ((h >> 28) & 1u));
    }
    tv_ready = 1;
}
static inline void tspan(int32_t y, int32_t x0, int32_t x1, uint16_t c)
{
    uint16_t *p;
    if ((uint32_t)y >= TV_H) return;
    if (x0 < 0) x0 = 0;
    if (x1 > TV_W) x1 = TV_W;
    p = tvb + y * TV_W;
    for (; x0 < x1; x0++) p[x0] = c;
}
static void trect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    if (y0 < 0) y0 = 0;
    if (y1 > TV_H) y1 = TV_H;
    for (; y0 < y1; y0++) tspan(y0, x0, x1, c);
}
static void tline(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int32_t dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1, err = dx + dy, guard = 800;
    while (guard--) {
        int32_t e2 = 2 * err;
        if ((uint32_t)x0 < TV_W && (uint32_t)y0 < TV_H) tvb[y0 * TV_W + x0] = c;
        if (x0 == x1 && y0 == y1) break;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static void tglow(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c, uint32_t a)   /* add light (neon haze) */
{
    int32_t x, y;
    if (y0 < 0) y0 = 0;
    if (y1 > TV_H) y1 = TV_H;
    if (x0 < 0) x0 = 0;
    if (x1 > TV_W) x1 = TV_W;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            uint16_t *p = tvb + y * TV_W + x, o = swap16(*p);
            *p = TSW(mix565(o, c, a));
        }
}

/* sprites */
enum { TP_BOX, TP_TREE, TP_MAST, TP_LIMIT, TP_STOP, TP_NAME, TP_PERSON, TP_PORTAL, TP_POST, TP_TRAIN, TP_CHECK,
       TP_FAN, TP_XING, TP_CAR, TP_LAMP, TP_WHISTLE, TP_SMOKE, TP_ROCK, TP_SHELTER, TP_CRANE, TP_PARASOL, TP_GARLAND, TP_STALL };
typedef struct { int32_t z; int16_t type, x0, x1, h0, h1; uint16_t c, arg; } tspr_t;
#define TV_NSPR 190u
static tspr_t tspr[TV_NSPR];
static uint32_t tnspr;
static void tspr_add(int32_t z, int type, int32_t x0, int32_t x1, int32_t h0, int32_t h1, uint16_t c, uint32_t arg)
{
    tspr_t *s;
    if (tnspr >= TV_NSPR || z < 2500 || z > TV_ZFAR) return;
    s = &tspr[tnspr++];
    s->z = z; s->type = (int16_t)type; s->x0 = (int16_t)(x0 / 10); s->x1 = (int16_t)(x1 / 10);
    s->h0 = (int16_t)(h0 / 10); s->h1 = (int16_t)(h1 / 10); s->c = c; s->arg = (uint16_t)arg;
}
static int tr_in_loop(int32_t wz)
{
    int32_t d = (int32_t)tr.L->dist * 1000;
    return tr.L->loop && wz > d - 160000 && wz < d + 60000;
}
static int tr_at_xing(int32_t wz)
{
    uint32_t i;
    for (i = 0; i < tr.L->nxing; i++) {
        int32_t d = wz - (int32_t)tr.L->xing[i] * 1000;
        if (d > -3500 && d < 3500) return 1;
    }
    return 0;
}
static const uint16_t TR_BLD_SW[6] = {RGB(70, 60, 100), RGB(90, 70, 120), RGB(60, 70, 110), RGB(110, 80, 120),
                                      RGB(80, 56, 90), RGB(50, 60, 96)};
static const uint16_t TR_BLD_EN[6] = {RGB(150, 146, 138), RGB(196, 186, 168), RGB(120, 124, 128), RGB(170, 150, 128),
                                      RGB(206, 204, 196), RGB(110, 100, 92)};   /* (the Enoden: concrete, cream, brown) */
#define TR_BLD (tr_mode == TRM_ENODEN ? TR_BLD_EN : TR_BLD_SW)
static const char *const TR_HORN_WHY[4] = {T_TR_HORN_WORK, T_TR_HORN_PEOPLE, T_TR_HORN_CURVE, T_TR_HORN_DEER};   /* a horn point: why, by its place in the line */
static const char *const TR_HORN_OK[4] = {T_TR_HORN_OK1, T_TR_HORN_OK2, T_TR_HORN_OK3, T_TR_HORN_OK4};
static const uint16_t TR_SHIRT[5] = {RGB(40, 60, 140), RGB(220, 60, 140), RGB(220, 220, 230), RGB(40, 170, 170), RGB(90, 80, 110)};

static void tr_sprites(void)
{
    uint32_t pm = tr_pos_m(), lot, side;
    int32_t pmm = tr_pos_mm(), far = (int32_t)pm + TV_ZFAR / 1000, i, dist = tr.L->dist, plat = tr.L->plat;
    tnspr = 0;
    for (lot = pm / 30u + 1u; (int32_t)lot * 30 < far; lot++) {   /* catenary masts */
        int32_t at = (int32_t)lot * 30;
        if (tr_zone(at) == TZ_TUNNEL) continue;
        if (tr.L->dbl) { if (at % 60 == 0) tspr_add(at * 1000 - pmm, TP_MAST, 2600 + tR2, 2800 + tR2, 0, 6600, RGB(60, 50, 70), 0); }
        else tspr_add(at * 1000 - pmm, TP_MAST, lot & 1u ? 2000 : -2200, lot & 1u ? 2200 : -2000, 0, 6200, RGB(70, 56, 70), lot & 1u ? 0 : 1);
    }
    for (lot = pm / 16u; (int32_t)lot * 16 < far; lot++)   /* the scenery, 16 m lots each side */
        for (side = 0; side < 2u; side++) {
            uint32_t h = th32(lot * 2u + side + (uint32_t)(tr.L - TR_LEGS) * 7919u), z0 = tr_zone((int32_t)lot * 16);
            int32_t sgn = side ? 1 : -1, z = (int32_t)lot * 16000 + (int32_t)(h & 7u) * 1000 - pmm, wz = (int32_t)lot * 16, rx = side ? tR2 : 0;
            if (!side && wz > dist - plat - 15 && wz < dist + 30) continue;
            if (side && z0 == TZ_SEA) continue;
            if (z0 == TZ_BEACH) {               /* the bay: pastel beach houses and palms on the left; sand, parasols, palms, the sea on the right */
                static const uint16_t PASTEL[4] = {RGB(255, 170, 200), RGB(140, 230, 230), RGB(255, 220, 150), RGB(190, 160, 255)};
                if (!side) {
                    int32_t x0 = 3600 + (int32_t)((h >> 4) & 3u) * 700, w = 5000 + (int32_t)((h >> 8) & 3u) * 900;
                    if ((h >> 14) & 1u) tspr_add(z, TP_BOX, -x0 - w, -x0, 0, 3600 + (int32_t)((h >> 12) & 3u) * 500, PASTEL[(h >> 24) & 3u], 2);
                    tspr_add(z - 7000, TP_TREE, -x0 + 400 - 2600, -x0 + 400 + 2600, 0, 8500 + (int32_t)((h >> 10) & 7u) * 500, RGB(40, 30, 60), 2);
                } else {
                    int32_t x0 = 4600 + (int32_t)((h >> 6) & 7u) * 400 + rx;
                    if ((h >> 18) % 3u != 2u) tspr_add(z, TP_PARASOL, x0 - 1300, x0 + 1300, 0, 2400, (h >> 20) & 1u ? RGB(255, 80, 180) : RGB(60, 220, 255), (h >> 22) & 1u);
                    if ((h >> 23) % 3u == 0u) tspr_add(z - 6000, TP_TREE, x0 + 1600 - 2600, x0 + 1600 + 2600, 0, 9500 + (int32_t)((h >> 10) & 7u) * 600, RGB(34, 26, 46), 2);
                }
                continue;
            }
            if (z0 == TZ_FESTIVAL) {            /* the festival: stalls with striped awnings, cherry trees hung with lanterns */
                int32_t x0 = 3300 + (int32_t)((h >> 4) & 3u) * 500 + rx, w = 4200 + (int32_t)((h >> 8) & 3u) * 700;
                if ((h >> 14) % 3u != 0u) tspr_add(z, TP_STALL, sgn < 0 ? -x0 - w : x0, sgn < 0 ? -x0 : x0 + w, 0, 3000, (h >> 16) & 1u ? RGB(255, 80, 120) : RGB(255, 200, 60), (h >> 18) & 3u);
                else tspr_add(z, TP_TREE, sgn * (x0 + 1500) - 2000, sgn * (x0 + 1500) + 2000, 0, 5200, (h >> 21) & 1u ? RGB(255, 160, 210) : RGB(255, 120, 200), 1);
                continue;
            }
            if (z0 == TZ_SEA) {                 /* the beach: palms along the shore, their fronds against the sky */
                if ((h >> 18) % 2u == 0u) { int32_t x0 = 3600 + (int32_t)((h >> 6) & 7u) * 700 + rx; tspr_add(z, TP_TREE, -x0 - 2600, -x0 + 2600, 0, 9500 + (int32_t)((h >> 10) & 7u) * 600, RGB(34, 26, 40), 2); }
                continue;
            }
            if (z0 == TZ_TOWN || z0 == TZ_STREET || z0 == TZ_NEON) {
                int32_t x0 = (z0 == TZ_STREET ? 4600 : 5500 + (int32_t)((h >> 4) & 3u) * 1500) + rx, w = 7000 + (int32_t)((h >> 8) & 7u) * 1000;
                int32_t ht = (z0 == TZ_STREET ? 4500 : z0 == TZ_NEON ? 9000 : 5000) + (int32_t)((h >> 12) & 15u) * (z0 == TZ_STREET ? 400 : 1500);
                if (z0 == TZ_TOWN && (h >> 20) % 5u == 0u) continue;
                tspr_add(z, TP_BOX, sgn < 0 ? -x0 - w : x0, sgn < 0 ? -x0 : x0 + w, 0, ht, TR_BLD[(h >> 24) % 6u],
                         z0 == TZ_STREET ? 3u : z0 == TZ_NEON ? 4u + ((h >> 18) & 1u) : 1u);
                if (z0 == TZ_STREET && ((h >> 18) & 3u) == 0u)
                    tspr_add(z - 4000, TP_CAR, sgn * 3600 - 900, sgn * 3600 + 900, 0, 1450, TR_SHIRT[(h >> 26) % 5u], 0);
                if ((z0 == TZ_STREET || z0 == TZ_NEON) && (lot % 3u) == side)   /* street lamps */
                    tspr_add(z - 8000, TP_LAMP, sgn * 3000 - 80, sgn * 3000 + 80, 0, 4800, RGB(40, 34, 50), 0);
            } else if (z0 == TZ_HOUSES) {
                int32_t x0 = 2600 + (int32_t)((h >> 4) & 3u) * 500 + rx, w = 6000 + (int32_t)((h >> 8) & 3u) * 1000;
                tspr_add(z, TP_BOX, sgn < 0 ? -x0 - w : x0, sgn < 0 ? -x0 : x0 + w, 0, 4200 + (int32_t)((h >> 12) & 7u) * 300, TR_BLD[(h >> 24) % 6u], 2);
                if ((h >> 16) & 1u)
                    tspr_add(z - 6000, TP_TREE, sgn * (x0 + 800) - 900, sgn * (x0 + 800) + 900, 0, 4000, RGB(240, 120, 190), 1);
            } else if (z0 == TZ_INDUSTRIAL) {          /* warehouses, chimneys smoking, a crane */
                int32_t x0 = 5000 + (int32_t)((h >> 4) & 3u) * 1200 + rx, w = 9000 + (int32_t)((h >> 8) & 7u) * 1500;
                tspr_add(z, TP_BOX, sgn < 0 ? -x0 - w : x0, sgn < 0 ? -x0 : x0 + w, 0, 5500 + (int32_t)((h >> 12) & 3u) * 900, RGB(70, 66, 84), 6);
                if (lot % 3u == side) {
                    int32_t cx2 = sgn * (x0 + w / 2), ch = 20000 + (int32_t)((h >> 14) & 7u) * 1500;
                    tspr_add(z + 3000, TP_POST, cx2 - 700, cx2 + 700, 0, ch, RGB(90, 60, 70), 0);
                    tspr_add(z + 2999, TP_SMOKE, cx2 - 2500, cx2 + 2500, ch, ch + 14000, RGB(150, 140, 160), 0);
                }
                if (lot % 7u == 3u && side) tspr_add(z + 6000, TP_CRANE, x0 + 2000, x0 + 16000, 0, 22000, RGB(220, 120, 40), 0);
            } else if (z0 == TZ_RIVERSIDE) {           /* left: wooden houses close; right: the river, the far bank */
                if (!side) {
                    int32_t x0 = 2800 + (int32_t)((h >> 4) & 3u) * 400, w = 5000 + (int32_t)((h >> 8) & 3u) * 900;
                    tspr_add(z, TP_BOX, -x0 - w, -x0, 0, 3800 + (int32_t)((h >> 12) & 3u) * 300, RGB(90, 60, 50), 2);
                    if ((h >> 16) & 1u) tspr_add(z - 7000, TP_TREE, -x0 - 900, -x0 + 900, 0, 4500, RGB(70, 150, 90), 0);
                } else if ((h >> 17) & 1u) {
                    int32_t x0 = 20000 + (int32_t)((h >> 4) & 7u) * 1500 + rx;
                    tspr_add(z, TP_BOX, x0, x0 + 6000, 0, 4500, RGB(100, 70, 60), 2);
                }
            } else if (z0 == TZ_ONSEN) {               /* inns with lanterns, steam from the ground */
                int32_t x0 = 3800 + (int32_t)((h >> 4) & 3u) * 900 + rx, w = 7000 + (int32_t)((h >> 8) & 3u) * 1000;
                tspr_add(z, TP_BOX, sgn < 0 ? -x0 - w : x0, sgn < 0 ? -x0 : x0 + w, 0, 5200 + (int32_t)((h >> 12) & 3u) * 600, RGB(110, 70, 60), 7);
                if ((h >> 18) & 1u) { int32_t sx2 = sgn * (x0 - 900); tspr_add(z - 5000, TP_SMOKE, sx2 - 2000, sx2 + 2000, 0, 9000, RGB(240, 230, 250), 1); }
            } else if (z0 == TZ_FIELDS) {              /* rice paddies (the ground), a farmhouse, poles */
                if (lot % 4u == side) tspr_add(z, TP_POST, sgn * 3400 - 90, sgn * 3400 + 90, 0, 7500, RGB(80, 60, 50), 0);
                if (((h >> 20) & 7u) == 0u) {
                    int32_t x0 = 12000 + (int32_t)((h >> 4) & 15u) * 1200 + rx;
                    tspr_add(z, TP_BOX, sgn < 0 ? -x0 - 8000 : x0, sgn < 0 ? -x0 : x0 + 8000, 0, 3500, RGB(130, 110, 80), 2);
                } else if (((h >> 20) & 7u) == 1u) {
                    int32_t x0 = 9000 + (int32_t)((h >> 4) & 15u) * 1500 + rx;
                    tspr_add(z, TP_TREE, sgn * x0 - 1500, sgn * x0 + 1500, 0, 6000, RGB(40, 100, 60), 0);
                }
            } else if (z0 == TZ_WOODS) {               /* the forest crowding the line */
                int32_t k2;
                for (k2 = 0; k2 < 2; k2++) {
                    int32_t x0 = 3200 + (int32_t)((h >> (4 + k2 * 5)) & 15u) * 600 + k2 * 4000 + rx, ht = 7000 + (int32_t)((h >> (9 + k2 * 3)) & 7u) * 1300;
                    tspr_add(z - k2 * 8000, TP_TREE, sgn * x0 - 1400, sgn * x0 + 1400, 0, ht, RGB(24, 70 + ((h >> (13 + k2)) & 31u), 54), 0);
                }
            } else if (z0 == TZ_ROCK) {                /* rock and snow, a pine or two */
                if ((h >> 19) & 1u) {
                    int32_t x0 = 3500 + (int32_t)((h >> 4) & 15u) * 900 + rx, w = 1500 + (int32_t)((h >> 8) & 7u) * 600;
                    tspr_add(z, TP_ROCK, sgn < 0 ? -x0 - w : x0, sgn < 0 ? -x0 : x0 + w, 0, 900 + (int32_t)((h >> 12) & 7u) * 350, RGB(110, 104, 120), 0);
                } else if (((h >> 21) & 3u) == 0u) {
                    int32_t x0 = 6000 + (int32_t)((h >> 4) & 15u) * 1200 + rx;
                    tspr_add(z, TP_TREE, sgn * x0 - 1000, sgn * x0 + 1000, 0, 5500, RGB(30, 70, 60), 0);
                }
            } else if (z0 == TZ_MVILLAGE) {            /* houses stepping up the slope, pines above */
                int32_t k2;
                for (k2 = 0; k2 < 2; k2++) {
                    int32_t x0 = 4500 + k2 * 7000 + (int32_t)((h >> (4 + k2 * 2)) & 3u) * 800 + rx, w = 5500, base = 0;   /* (on the ground: no houses in the air) */
                    if ((h >> (16 + k2)) & 1u)
                        tspr_add(z - k2 * 2000, TP_BOX, sgn < 0 ? -x0 - w : x0, sgn < 0 ? -x0 : x0 + w, base, base + 4000, TR_BLD[(h >> (24 + k2)) % 6u], 2);
                }
                if ((h >> 22) & 1u) { int32_t x0 = 15000 + (int32_t)((h >> 5) & 7u) * 1500 + rx; tspr_add(z + 4000, TP_TREE, sgn * x0 - 1200, sgn * x0 + 1200, 7000, 13000, RGB(30, 80, 60), 0); }
            } else if (z0 == TZ_COUNTRY || z0 == TZ_SAKURA) {
                int32_t x0 = 3800 + (int32_t)((h >> 4) & 15u) * 1100 + rx, ht = 3500 + (int32_t)((h >> 9) & 7u) * 900;
                if (z0 == TZ_COUNTRY && ((h >> 20) & 1u)) continue;
                if (z0 == TZ_SAKURA) tspr_add(z, TP_TREE, sgn * x0 - 1800, sgn * x0 + 1800, 0, ht, (h >> 21) & 1u ? RGB(255, 140, 200) : RGB(240, 110, 180), 1);
                else tspr_add(z, TP_TREE, sgn * x0 - 1200, sgn * x0 + 1200, 0, ht, RGB(40, 90 + ((h >> 13) & 31u), 80), 0);
                if (((h >> 25) & 3u) == 0u)
                    tspr_add(z + 5000, TP_BOX, sgn < 0 ? -x0 - 13000 : x0 + 6000, sgn < 0 ? -x0 - 6000 : x0 + 13000, 0, 5500, TR_BLD[(h >> 27) % 6u], 2);
            }
        }
    for (lot = pm / 20u + 1u; (int32_t)lot * 20 < (int32_t)pm + 260; lot++)   /* the festival: lanterns strung across the line */
        if (tr_zone((int32_t)lot * 20) == TZ_FESTIVAL) tspr_add((int32_t)lot * 20000 - pmm, TP_GARLAND, -3600, 3600 + tR2, 5000, 6200, RGB(90, 60, 50), lot);
    for (lot = pm / 5u + 1u; (int32_t)lot * 5 < (int32_t)pm + 200; lot++)   /* bridge girders */
        if (tr_zone((int32_t)lot * 5) == TZ_RIVER) {
            int32_t z = (int32_t)lot * 5000 - pmm;
            tspr_add(z, TP_POST, -2270, -2030, 0, 2700, RGB(150, 70, 60), 0);
            tspr_add(z, TP_POST, 2030 + tR2, 2270 + tR2, 0, 2700, RGB(150, 70, 60), 0);
        }
    for (i = 0; i < (int32_t)tr.L->nxing; i++) {
        int32_t z = (int32_t)tr.L->xing[i] * 1000 - 3800 - pmm;
        tspr_add(z, TP_XING, -2900, -2700, 0, 2600, RGB(30, 30, 30), 0);
        tspr_add(z, TP_XING, 2700 + tR2, 2900 + tR2, 0, 2600, RGB(30, 30, 30), 1);
    }
    for (i = 1; i < (int32_t)tr.L->nzon; i++)
        if (tr.L->zon[i].v == TZ_TUNNEL)
            tspr_add((int32_t)tr.L->zon[i].at * 1000 - pmm, TP_PORTAL, -40000, 40000, 0, 30000, RGB(60, 50, 70), 0);
    for (i = 1; i < (int32_t)tr.L->nlim; i++)
        tspr_add((int32_t)tr.L->lim[i].at * 1000 - pmm, TP_LIMIT, -2700, -1900, 1800, 2600, 0, (uint32_t)tr.L->lim[i].v);
    for (i = 0; i < (int32_t)tr.L->nev; i++) {
        const tr_ev_t *e = &tr.L->ev[i];
        int32_t z = (int32_t)e->at * 1000 - pmm, k;
        if (e->type == TE_CHECK) tspr_add(z, TP_CHECK, -2600, -1800, 1600, 2400, 0, e->arg);
        if (e->type == TE_WHISTLE) tspr_add(z, TP_WHISTLE, -2500, -1800, 1500, 2200, 0, 0);
        if (e->type == TE_FANS)
            for (k = 0; k < 4; k++) {
                uint32_t h = th32((uint32_t)(i * 7 + k));
                int32_t x = (k & 1 ? 3600 + tR2 : -3600) + (int32_t)(h & 511u) * (k & 1 ? 1 : -1);
                if ((i % 4) == 2 || ((i % 4) != 1 && k)) continue;   /* (a blind curve: no one; a fox, a deer: one) */
                tspr_add(z + k * 1800 - 2000, TP_FAN, x - 260, x + 260, 0, (i % 4) == 0 ? 600 : (i % 4) == 3 ? 1100 : 1600 + (int32_t)((h >> 9) & 127u) * 2, (i % 4) == 0 ? RGB(230, 110, 40) : (i % 4) == 3 ? RGB(150, 100, 60) : TR_SHIRT[(h >> 12) % 5u], 0);   /* (a fox, low and russet; a deer) */
            }
    }
    if (tr.on_live) {
        int32_t x = tr.L->dbl ? tR2 : 3400;
        tspr_add(tr.on_mm - pmm, TP_TRAIN, x - 1400, x + 1400, 900, 3900, RGB(70, 60, 90), RGB(255, 60, 200));
    }
    tspr_add(dist * 1000 - plat * 340 - pmm, TP_NAME, -4600, -2200, 2600, 3500, 0, 0);
    if (!tr.L->canopy) tspr_add(dist * 1000 - plat * 600 - pmm, TP_SHELTER, -4400, -2400, 1100, 3400, RGB(80, 60, 70), 0);
    else for (i = 0; i * 15 < plat * (tr.L->canopy == 2 ? 9 : 5) / 10; i++)   /* the canopy's pillars */
        tspr_add(dist * 1000 - (plat * (tr.L->canopy == 2 ? 95 : 70) / 100 - i * 15) * 1000 - pmm, TP_POST, -3200, -3000, 1100, 4100, RGB(60, 46, 80), 0);
    for (i = 0; i < 5; i++) {
        uint32_t h = th32((uint32_t)i * 31u + (uint32_t)(tr.L - TR_LEGS));
        int32_t at = dist * 1000 - (plat - 20) * 1000 + i * (plat * 1000 - 20000) / 5 + (int32_t)(h & 4095u) * 2;
        int32_t x = -3000 - (int32_t)((h >> 12) & 1023u);
        tspr_add(at - pmm, TP_PERSON, x - 250, x + 250, 1100, 1100 + 1550 + (int32_t)((h >> 22) & 255u), TR_SHIRT[(h >> 8) % 5u], 0);
    }
    tspr_add(dist * 1000 - pmm, TP_STOP, -1500, -1050, 1250, 1700, 0, 0);
    for (lot = 1; lot < tnspr; lot++) {
        tspr_t x = tspr[lot];
        uint32_t j = lot;
        while (j && tspr[j - 1u].z < x.z) { tspr[j] = tspr[j - 1u]; j--; }
        tspr[j] = x;
    }
}

static uint32_t tfog(int32_t z) { int32_t f = z / 1300; return (uint32_t)(f > 230 ? 230 : f); }
#define TL_TRACK 1
#define TL_TUNNEL 2
static uint16_t tlit(uint16_t c, int32_t z, int how)
{
    uint32_t a = how & TL_TUNNEL ? 230u : tp->amb;
    if (tp->amb < 160u && (how & TL_TRACK) && z < (tr.dim ? 35000 : 80000)) a += (uint32_t)(200 - z / (tr.dim ? 175 : 400));
    if (a > 256u) a = 256u;
    if (how & TL_TUNNEL) return mix565(mix565(0, c, a), RGB(8, 6, 14), tfog(z * 3));
    return mix565(mix565(0, c, a), tp->fog, tfog(z));
}
static int32_t trow(int32_t z) { int32_t r = TV_KZ / (z > 1 ? z : 1) - 1; return r < 0 ? 0 : r >= TV_GH ? TV_GH - 1 : r; }

static const uint16_t TR_STYLE_COL[11] = {   /* each look's colour: name boards, the canopy's strip */
    RGB(255, 60, 200), RGB(40, 120, 220), RGB(255, 160, 40), RGB(60, 200, 210), RGB(230, 60, 60), RGB(120, 180, 90),
    RGB(200, 210, 240), RGB(60, 150, 90), RGB(190, 170, 90), RGB(255, 140, 200), RGB(150, 60, 255)};
static void tspr_draw(const tspr_t *s)
{
    int32_t z = s->z, jj8 = (int32_t)(((uint32_t)TV_KZ * 256u) / (uint32_t)z), off = toff[trow(z)];
    int32_t xa = TV_W / 2 + tjpx(off + s->x0 * 10, jj8), xb = TV_W / 2 + tjpx(off + s->x1 * 10, jj8);
    int32_t yg = TV_HY + (jj8 >> 8), ya = yg - tjpx(s->h1 * 10, jj8), yb = yg - tjpx(s->h0 * 10, jj8), w, h;
    uint16_t c = TSW(tlit(s->c, z, 0));
    if (xb < 0 || xa >= TV_W || yb < 0) return;
    if (xb <= xa) xb = xa + 1;
    w = xb - xa;
    h = yb - ya;
    switch (s->type) {
    case TP_BOX:
        trect(xa, ya, xb, yb, c);
        if (w >= 8 && h >= 8) {                 /* windows: warm when lit */
            uint16_t wc = TSW(tlit(tp->lit ? RGB(255, 200, 120) : RGB(40, 40, 70), z / (tp->lit ? 3 : 1), 0));
            uint16_t wc2 = TSW(tlit(tp->lit ? RGB(120, 230, 255) : RGB(40, 40, 70), z / (tp->lit ? 3 : 1), 0));
            int32_t sx = w / 5 > 3 ? w / 5 : 3, sy = tjpx(1500, jj8), x, y;
            if (sy < 3) sy = 3;
            for (y = ya + sy / 2; y + sy / 2 < yb; y += sy)
                for (x = xa + sx / 2; x + sx / 2 < xb; x += sx) {
                    uint32_t hh = th32((uint32_t)(x * 7 + y));
                    trect(x, y, x + (sx + 1) / 2, y + (sy + 1) / 2, !(hh & 3u) && tp->lit ? c : (hh & 8u) ? wc2 : wc);
                }
        }
        if (s->arg == 2) {                      /* a roof */
            int32_t k, rh = h / 2 + 1;
            uint16_t rc = TSW(tlit(RGB(50, 34, 60), z, 0));
            for (k = 0; k < rh; k++) tspan(ya - rh + k, xa + (rh - k) / 2 - 1, xb - (rh - k) / 2 + 1, rc);
        }
        if (s->arg == 3) {                      /* a shop: a glowing awning */
            int32_t ah = tjpx(2600, jj8), at = tjpx(400, jj8) + 1;
            trect(xa, yb - ah, xb, yb - ah + at, TSW(tlit((s->c & 2u) ? tp->neon1 : tp->neon2, z / 2, 0)));
            trect(xa, yb - ah + at, xb, yb, TSW(tlit(RGB(30, 24, 36), z, 0)));
        }
        if (s->arg == 6u) {                     /* a warehouse: corrugated, a loading door, a sodium lamp */
            int32_t k;
            for (k = xa; k < xb; k += 3) trect(k, ya + 1, k + 1, yb, TSW(tlit(RGB(56, 52, 70), z, 0)));
            if (w > 8) trect(xa + w / 3, yb - h / 2, xa + w / 2, yb, TSW(tlit(RGB(30, 26, 40), z, 0)));
            if (tp->amb < 200u && z < 160000) tglow(xa + w / 2 - 3, ya - 2, xa + w / 2 + 4, ya + 5, RGB(255, 170, 70), 120);
        }
        if (s->arg == 7u) {                     /* an inn: a dark tiled roof, red lanterns along its eaves */
            int32_t k, rh = h / 4 + 1;
            trect(xa - w / 12, ya - rh, xb + w / 12, ya + 1, TSW(tlit(RGB(40, 30, 50), z, 0)));
            for (k = xa + w / 8; k < xb; k += w / 4 + 1)
                trect(k, ya + 2, k + w / 16 + 2, ya + h / 6 + 3, TSW(RGB(255, 70, 60)));
        }
        if (s->arg >= 4u && s->arg <= 5u && w >= 3) {   /* neon tower: a vertical sign and a rooftop line */
            uint16_t nc = s->arg == 4u ? tp->neon1 : tp->neon2;
            int32_t sw = w / 6 + 1;
            trect(xa + w / 6, ya + h / 8, xa + w / 6 + sw, ya + h / 2, TSW(mix565(nc, RGB(255, 255, 255), 60)));
            trect(xa, ya, xb, ya + 1, TSW(nc));
            if (z < 140000) tglow(xa + w / 6 - sw, ya + h / 8 - 2, xa + w / 6 + sw * 2, ya + h / 2 + 2, nc, 70);
        }
        break;
    case TP_TREE: {
        int32_t k, tr2 = tjpx(250, jj8), cx = (xa + xb) / 2, th = h / 4;
        if (s->arg == 2u) {                     /* a palm: a leaning trunk, fronds drooping from its top */
            uint16_t pc = TSW(tlit(s->c, z / 2, 0));
            int32_t ty = yb - h, lean = w / 5, f, tt;
            for (tt = 0; tt < h; tt += 2) trect(cx + lean * tt / (h + 1) - tr2, yb - tt - 2, cx + lean * tt / (h + 1) + tr2 + 1, yb - tt, pc);
            for (f = 0; f < 6; f++) {
                int32_t dx = (f < 3 ? -1 : 1) * (2 + (f % 3)) * w / 10, len = w / 2 + 2;
                for (tt = 0; tt < len; tt += 1) {
                    int32_t px2 = cx + lean + dx * tt / (len + 1), py2 = ty + (tt * tt) / (len + 1) / 2 - (f % 3) * 2;
                    trect(px2, py2, px2 + 2, py2 + 1 + (w > 30), pc);
                }
            }
            break;
        }
        trect(cx - tr2, yb - th, cx + tr2 + 1, yb, TSW(tlit(RGB(60, 36, 40), z, 0)));
        if (s->arg) {                           /* a cherry: a cloud of blossom, five round puffs */
            uint16_t c2 = TSW(tlit(mix565(s->c, RGB(255, 255, 255), 70), z, 0)), c3 = TSW(tlit(mix565(s->c, RGB(60, 10, 50), 60), z, 0));
            static const int8_t PX[5] = {-5, 5, -2, 3, 0}, PY[5] = {5, 5, 2, 1, -2}, PR[5] = {5, 5, 6, 5, 4};
            int32_t ch2 = h - th, r0 = w / 2;
            for (k = 0; k < 5; k++) {
                int32_t pcx = cx + PX[k] * r0 / 10, pcy = ya + ch2 / 2 + PY[k] * ch2 / 20, pr = PR[k] * r0 / 9 + 1, dy;
                for (dy = -pr; dy <= pr; dy++) {
                    int32_t hw = (int32_t)mo_isqrt((uint32_t)(pr * pr - dy * dy));
                    tspan(pcy + dy, pcx - hw, pcx + hw + 1, k < 2 ? c3 : dy < -pr / 3 ? c2 : c);
                }
            }
            if (w > 6) for (k = 0; k < 5; k++) {   /* petals, falling */
                uint32_t hh = th32((uint32_t)k * 131u + (uint32_t)s->x0);
                int32_t px = xa + (int32_t)(hh % (uint32_t)w), py = yb - th + (int32_t)((fm1_ms / 40u + hh) % (uint32_t)(th * 3 + 1)) - th;
                if (py < yg) trect(px, py, px + 1, py + 1, TSW(RGB(255, 190, 230)));
            }
        } else
            for (k = 0; k < 3; k++) {
                int32_t y0 = ya + (h - th) * k / 3, y1 = ya + (h - th) * (k + 1) / 3, hw = (w / 2) * (k + 2) / 4;
                trect(cx - hw, y0, cx + hw + 1, y1 + 1, k & 1 ? c : TSW(tlit(mix565(s->c, 0, 40), z, 0)));
            }
        break;
    }
    case TP_MAST: {
        int32_t ax = TV_W / 2 + tjpx(off + (s->arg ? 300 : -300), jj8), ay = yg - tjpx(5700, jj8), t = tjpx(150, jj8);
        trect(xa, ya, xb, yb, c);
        if (s->arg) trect(xa, ay, ax, ay + (t > 1 ? t : 1), c);
        else trect(ax, ay, xb, ay + (t > 1 ? t : 1), c);
        break;
    }
    case TP_POST:
        trect(xa, ya, xb, yb, c);
        break;
    case TP_PARASOL: {                          /* a beach parasol: a pole, a striped canopy */
        int32_t cx = (xa + xb) / 2, ch = h / 3 + 1, k;
        uint16_t c2 = TSW(tlit(RGB(250, 245, 255), z, 0));
        trect(cx, ya + ch, cx + 1 + (w > 20), yb, TSW(tlit(RGB(230, 220, 230), z, 0)));
        for (k = 0; k < ch; k++) {              /* the canopy: a low cone, stripes */
            int32_t hw2 = w / 2 * (k + 1) / ch, x;
            for (x = cx - hw2; x <= cx + hw2; x++) trect(x, ya + k, x + 1, ya + k + 1, ((x - cx + 64) / (w / 6 + 1)) & 1 ? c : c2);
        }
        if (s->arg && w > 6) trect(xa, yb - 2, xb, yb, TSW(tlit(RGB(255, 210, 120), z, 0)));   /* a towel */
        break;
    }
    case TP_STALL: {                            /* a festival stall: a counter, a striped awning, lanterns lit */
        int32_t aw = h / 3 + 1, k;
        uint16_t c2 = TSW(tlit(RGB(255, 250, 240), z, 0));
        trect(xa, ya + aw, xb, yb, TSW(tlit(RGB(80, 40, 50), z, 0)));
        trect(xa + 1, ya + aw + 1, xb - 1, yb - h / 4, TSW(tlit(RGB(255, 210, 140), z / 3, 0)));   /* the lit inside */
        for (k = xa; k < xb; k++) trect(k, ya, k + 1, ya + aw, ((k - xa) / (w / 5 + 1)) & 1 ? c : c2);
        for (k = 0; k < 3; k++) {               /* lanterns under the awning */
            int32_t lx = xa + w * (k * 2 + 1) / 6, lr = w / 14 + 1;
            trect(lx - lr, ya + aw, lx + lr + 1, ya + aw + lr * 3, TSW(k == (int32_t)s->arg % 3 ? RGB(255, 70, 80) : RGB(255, 170, 60)));
        }
        if (z < 170000) tglow(xa, ya, xb, yb, RGB(255, 170, 90), 50);
        break;
    }
    case TP_GARLAND: {                          /* lanterns on a line across the track, swaying */
        int32_t k, n = w / 7 + 2, sag = h + 2, lr = tjpx(260, jj8) + 1;
        static const uint16_t LC[4] = {RGB(255, 70, 90), RGB(255, 180, 60), RGB(255, 120, 200), RGB(120, 230, 255)};
        for (k = 0; k <= n; k++) {
            int32_t x = xa + w * k / n, f = k * (n - k) * 4 * sag / (n * n + 1), y = ya + f;
            int32_t x2 = xa + w * (k + 1) / n, f2 = (k + 1) * (n - k - 1) * 4 * sag / (n * n + 1);
            if (k < n) tline(x, y, x2, ya + f2, c);
            if (k && k < n && (k & 1)) {
                uint16_t lc = LC[(k / 2 + s->arg) & 3u];
                trect(x - lr, y + 1, x + lr + 1, y + 2 + lr * 3, TSW(lc));
                if (z < 70000) tglow(x - lr * 2, y, x + lr * 2 + 1, y + lr * 4, lc, 40);
            }
        }
        break;
    }
    case TP_LAMP: {                             /* a street lamp: a pole and a glowing head */
        int32_t lh = tjpx(300, jj8) + 1;
        trect(xa, ya, xb, yb, c);
        trect(xa - lh, ya, xb + lh, ya + lh, TSW(RGB(255, 220, 160)));
        if (tp->amb < 200u && z < 160000) tglow(xa - lh * 3, ya - lh, xb + lh * 3, ya + lh * 3, RGB(255, 200, 140), 60);
        break;
    }
    case TP_LIMIT:
    case TP_CHECK: {
        int32_t px = (xa + xb) / 2, pw = tjpx(60, jj8) + 1;
        char t[4], *q;
        trect(px - pw / 2, yb, px - pw / 2 + pw, yg, TSW(tlit(RGB(120, 110, 130), z, 0)));
        trect(xa, ya, xb, yb, TSW(tlit(s->type == TP_CHECK ? RGB(20, 20, 20) : RGB(230, 40, 120), z / 2, 0)));
        if (w > 4) trect(xa + w / 8 + 1, ya + h / 8 + 1, xb - w / 8 - 1, yb - h / 8 - 1,
                         TSW(tlit(s->type == TP_CHECK ? RGB(255, 210, 40) : RGB(250, 245, 255), z / 2, 0)));
        if (w >= 22) {
            t[0] = (char)('0' + s->arg / 100u % 10u); t[1] = (char)('0' + s->arg / 10u % 10u); t[2] = (char)('0' + s->arg % 10u); t[3] = 0;
            q = s->arg >= 100u ? t : t + 1;
            g_text(px - text_w(&FONT_S, q) / 2, TV_Y + (ya + yb) / 2 - 8, &FONT_S, q, RGB(20, 20, 30));
        }
        break;
    }
    case TP_STOP: {
        int32_t px = (xa + xb) / 2, pw = tjpx(70, jj8) + 1, k;
        trect(px - pw / 2, yb, px - pw / 2 + pw, yg, TSW(tlit(RGB(200, 200, 210), z, 0)));
        trect(xa, ya, xb, yb, TSW(RGB(255, 210, 40)));
        for (k = 0; k < 4; k++) trect(xa, ya + h * (2 * k + 1) / 8, xb, ya + h * (2 * k + 2) / 8, TSW(RGB(20, 20, 20)));
        break;
    }
    case TP_NAME: {                             /* the station's board: lit from inside */
        int32_t lw = tjpx(80, jj8) + 1, yp = yg - tjpx(1100, jj8), bh = h / 5 > 1 ? h / 5 : 1;
        const char *nm = tr_names[tr.L->st_to];
        trect(xa + w / 6, yb, xa + w / 6 + lw, yp, TSW(tlit(RGB(60, 50, 70), z, 0)));
        trect(xb - w / 6 - lw, yb, xb - w / 6, yp, TSW(tlit(RGB(60, 50, 70), z, 0)));
        trect(xa, ya, xb, yb, TSW(tlit(RGB(245, 240, 250), z / 3, 0)));
        trect(xa, yb - bh, xb, yb, TSW(TR_STYLE_COL[tr.L->style_to % 11u]));
        if (w > text_w(&FONT_S, nm) + 4 && h >= 20)
            g_text((xa + xb - text_w(&FONT_S, nm)) / 2, TV_Y + ya + (h - bh) / 2 - 8, &FONT_S, nm, RGB(30, 20, 40));
        break;
    }
    case TP_PERSON:
    case TP_FAN: {
        int32_t hd = h / 6 + 1;
        trect(xa, ya + hd, xb, yb - h / 3, c);
        trect(xa + w / 6, yb - h / 3, xb - w / 6 + (w < 6), yb, TSW(tlit(RGB(30, 26, 40), z, 0)));
        trect(xa + w / 4, ya, xb - w / 4 + (w < 4), ya + hd, TSW(tlit(RGB(60, 40, 40), z, 0)));
        if (s->type == TP_FAN) {               /* a camera up, a tripod; a flash now and then */
            trect(xa - w / 4, ya + hd / 2, xb + w / 4, ya + hd + hd / 2 + 1, TSW(tlit(RGB(20, 20, 20), z, 0)));
            tline((xa + xb) / 2 + w, ya + hd * 2, (xa + xb) / 2 + w * 2, yb, TSW(tlit(RGB(70, 70, 80), z, 0)));
            tline((xa + xb) / 2 + w, ya + hd * 2, (xa + xb) / 2, yb, TSW(tlit(RGB(70, 70, 80), z, 0)));
            if (((fm1_ms / 90u + (uint32_t)s->x0) % 23u) == 0u) trect(xa - w / 4, ya, xb + w / 4, ya + hd * 2, TSW(RGB(255, 255, 255)));
        }
        break;
    }
    case TP_PORTAL: {
        int32_t hx0 = TV_W / 2 + tjpx(off - 2300, jj8), hx1 = TV_W / 2 + tjpx(off + 2300 + tR2, jj8), hy = yg - tjpx(5400, jj8);
        trect(xa, ya, hx0, yb, c);
        trect(hx1, ya, xb, yb, c);
        trect(hx0, ya, hx1, hy, c);
        trect(hx0 - 2, hy - 2, hx1 + 2, hy, TSW(tp->neon2));   /* a neon strip over the mouth */
        break;
    }
    case TP_TRAIN: {
        int32_t wy0 = ya + h / 6, wy1 = ya + h / 2, lh = h / 10 + 1, lw = w / 7 + 1;
        uint16_t lc = TSW(RGB(255, 250, 230));
        trect(xa, ya, xb, yb, c);
        trect(xa + w / 10, wy0, xb - w / 10, wy1, TSW(tlit(RGB(30, 30, 50), z, 0)));
        trect(xa, wy1 + h / 8, xb, wy1 + h / 4, TSW(s->arg));
        trect(xa + w / 8, yb - h / 5, xa + w / 8 + lw, yb - h / 5 + lh, lc);
        trect(xb - w / 8 - lw, yb - h / 5, xb - w / 8, yb - h / 5 + lh, lc);
        if (tp->amb < 220u) {                   /* the glare */
            tglow(xa + w / 8 - lw * 2, yb - h / 5 - lh * 2, xa + w / 8 + lw * 3, yb - h / 5 + lh * 3, RGB(255, 250, 220), 110);
            tglow(xb - w / 8 - lw * 3, yb - h / 5 - lh * 2, xb - w / 8 + lw * 2, yb - h / 5 + lh * 3, RGB(255, 250, 220), 110);
        }
        break;
    }
    case TP_XING: {
        int32_t k, lamp = tjpx(260, jj8) + 1;
        uint32_t ph = (fm1_ms / 400u) & 1u;
        for (k = 0; k < 6; k++) trect(xa, ya + h * k / 6, xb, ya + h * (k + 1) / 6, TSW(tlit(k & 1 ? RGB(240, 200, 20) : RGB(20, 20, 20), z, 0)));
        trect(xa - lamp, ya, xa, ya + lamp, TSW(z < 160000 && ph ? RGB(255, 40, 60) : tlit(RGB(80, 20, 30), z, 0)));
        trect(xb, ya, xb + lamp, ya + lamp, TSW(z < 160000 && !ph ? RGB(255, 40, 60) : tlit(RGB(80, 20, 30), z, 0)));
        break;
    }
    case TP_WHISTLE: {                          /* a whistle board: sound the horn; white, a black rim, a W */
        int32_t px = (xa + xb) / 2, pw = tjpx(60, jj8) + 1;
        trect(px - pw / 2, yb, px - pw / 2 + pw, yg, TSW(tlit(RGB(120, 110, 130), z, 0)));
        trect(xa, ya, xb, yb, TSW(tlit(RGB(20, 20, 20), z / 2, 0)));
        if (w > 4) trect(xa + w / 8 + 1, ya + h / 8 + 1, xb - w / 8 - 1, yb - h / 8 - 1, TSW(tlit(RGB(250, 250, 250), z / 2, 0)));
        if (w >= 14) g_text(px - 4, TV_Y + (ya + yb) / 2 - 8, &FONT_S, "W", RGB(20, 20, 20));
        break;
    }
    case TP_SMOKE: {                            /* smoke (or steam) rising and spreading: puffs that cycle upward */
        int32_t k, cx2 = (xa + xb) / 2;
        for (k = 0; k < 4; k++) {
            uint32_t ph = ((fm1_ms / (s->arg ? 18u : 30u)) + (uint32_t)k * 64u + (uint32_t)(s->x0 & 255)) & 255u;
            int32_t py = yb - (int32_t)ph * h / 256, pr = 1 + (w / 4) * (int32_t)(ph + 40u) / 296, drift = (int32_t)ph * w / 900;
            uint32_t a = (256u - ph) * (s->arg ? 110u : 90u) / 256u;
            if (z < 200000) tglow(cx2 + drift - pr, py - pr, cx2 + drift + pr, py + pr, s->c, a);
        }
        break;
    }
    case TP_ROCK: {                             /* a boulder, snow on its top */
        trect(xa + w / 6, ya, xb - w / 6, yb, c);
        trect(xa, ya + h / 3, xb, yb, c);
        trect(xa + w / 6, ya, xb - w / 6, ya + h / 4 + 1, TSW(tlit(RGB(240, 236, 250), z, 0)));
        break;
    }
    case TP_SHELTER: {                          /* a small waiting shelter: a roof, a back wall, a bench */
        int32_t rt = h / 6 + 1;
        trect(xa, ya + rt, xa + w / 8 + 1, yb, c);
        trect(xb - w / 8 - 1, ya + rt, xb, yb, c);
        trect(xa + w / 8, ya + rt, xb - w / 8, yb - h / 4, TSW(tlit(RGB(50, 36, 56), z, 0)));
        trect(xa - w / 10, ya, xb + w / 10, ya + rt, TSW(tlit(RGB(110, 60, 70), z, 0)));
        if (tp->amb < 200u) tglow(xa, ya + rt, xb, ya + rt + h / 3, RGB(255, 210, 150), 60);
        break;
    }
    case TP_CRANE: {                            /* a gantry crane: two legs, a beam */
        int32_t t2 = w / 14 + 1;
        trect(xa, ya, xa + t2, yb, c);
        trect(xb - t2, ya, xb, yb, c);
        trect(xa - t2, ya, xb + t2 * 3, ya + t2 + 1, c);
        trect(xa + w / 2, ya + t2, xa + w / 2 + 1, ya + h / 2, TSW(tlit(RGB(60, 60, 70), z, 0)));
        break;
    }
    case TP_CAR:
        trect(xa, ya + h / 3, xb, yb - h / 6, c);
        trect(xa + w / 6, ya, xb - w / 6, ya + h / 3, TSW(tlit(RGB(30, 40, 60), z, 0)));
        trect(xa + w / 8, yb - h / 6, xa + w / 3, yb, TSW(RGB(16, 14, 20)));
        trect(xb - w / 3, yb - h / 6, xb - w / 8, yb, TSW(RGB(16, 14, 20)));
        if (tp->amb < 200u) { trect(xa, ya + h / 2, xa + w / 8 + 1, ya + h / 2 + 2, TSW(RGB(255, 40, 60))); trect(xb - w / 8 - 1, ya + h / 2, xb, ya + h / 2 + 2, TSW(RGB(255, 40, 60))); }
        break;
    }
}

static void ttrack(int32_t y, int32_t cx, int32_t jj8, int32_t x0, int32_t z, int32_t wz, int32_t dz, int tn, int bed)
{
    int32_t rw = tjpx(70, jj8) + 1, xl = cx + tjpx(x0 - 570, jj8), xr = cx + tjpx(x0 + 500, jj8);
    uint16_t rc = TSW(tlit(tp->rail, z, TL_TRACK | tn));
    if (bed) {
        tspan(y, cx + tjpx(x0 - 1400, jj8), cx + tjpx(x0 + 1400, jj8), TSW(tlit(tn ? RGB(50, 44, 54) : tp->ballast, z, TL_TRACK | tn)));
        if (dz < 380 && (uint32_t)(wz % 625) < 230u) tspan(y, cx + tjpx(x0 - 1050, jj8), cx + tjpx(x0 + 1050, jj8), TSW(tlit(tp->sleeper, z, TL_TRACK | tn)));
        else if (dz < 900) tspan(y, cx + tjpx(x0 - 1050, jj8), cx + tjpx(x0 + 1050, jj8), TSW(tlit(mix565(tp->ballast, tp->sleeper, 110), z, TL_TRACK | tn)));
    }
    tspan(y, xl, xl + rw, rc);
    tspan(y, xr, xr + rw, rc);
    if (tp->lit && !tn && dz < 1500 && ((wz / 4000) & 3) == 0) {   /* the rails catch the neon */
        tspan(y, xl, xl + rw, TSW(tp->neon2));
        tspan(y, xr, xr + rw, TSW(tp->neon1));
    }
}

static void tr_view(void)
{
    int32_t pmm = tr_pos_mm(), r, x, prev_z = 0, slope = 0, off = 0, dist_mm = (int32_t)tr.L->dist * 1000, plat = tr.L->plat * 1000;
    int32_t pxl = 0, pxr = 0, pxp = 0, pyc = 0, pyw = 0, ptop = 0, pch_l = 0, pch_r = 0, pcy = 0, prev_on = 0, pfl = 0, pfr = 0, fon = 0, pcy2 = 0;
    uint32_t si, sea_now = tr_zone(pmm / 1000) == TZ_SEA || tr_zone(pmm / 1000 + 200) == TZ_SEA || tr_zone(pmm / 1000) == TZ_BEACH || tr_zone(pmm / 1000 + 200) == TZ_BEACH;
    tvb = cv_px + TV_Y * TV_W;
    tR2 = tr.L->dbl ? 3800 : 0;
    for (r = TV_GH - 1; r >= 0; r--) {          /* the curve, near to far */
        int32_t z = tz_mm[r], dz = z - prev_z, c = tr_seg(tr.L->cur, tr.L->ncur, (pmm + z) / 1000);
        slope += (int32_t)(((int64_t)c * dz * 5727) >> 16);
        off += (int32_t)(((int64_t)slope * dz) >> 20);
        off = clampi(off, -600000, 600000);
        toff[r] = off;
        tcx[r] = TV_W / 2 + tjpx(off, (r + 1) << 8);
        prev_z = z;
    }
    /* the sky: three-stop gradient, stars, the sun (striped, synthwave), the mountain, the hills */
    {
        int32_t sx = tr.sky_x >> 8, y, xmax = sea_now ? tcx[0] - 6 : TV_W;
        uint16_t hf = TSW(tp->hill_far), hn = TSW(tp->hill_near);
        for (y = 0; y < TV_HY; y++) {
            uint16_t c = TSW(y < TV_HY / 2 ? mix565(tp->top, tp->mid, (uint32_t)y * 512u / TV_HY) : mix565(tp->mid, tp->hor, (uint32_t)(y - TV_HY / 2) * 512u / TV_HY)), *p = tvb + y * TV_W;
            for (x = 0; x < TV_W; x++) p[x] = c;
        }
        if (tp->stars)
            for (x = 0; x < 70; x++) {
                uint32_t h = th32((uint32_t)x * 977u);
                int32_t px = ((int32_t)(h & 1023u) + sx / 2) & 1023;
                if (px < TV_W) tvb[(int32_t)((h >> 10) % (TV_HY - 8)) * TV_W + px] = TSW(((h >> 20) & 3u) ? RGB(255, 240, 255) : RGB(140, 230, 255));
            }
        if (tp->sun) {                          /* the sun: a disc, gold to rose, cut by bars that thicken downward */
            int32_t cx0 = 150 - (((sx / 3) % 400) + 400) % 400, cy0 = TV_HY - 4, dy, R = 22;
            for (dy = -R; dy <= 2; dy++) {
                int32_t hw = (int32_t)mo_isqrt((uint32_t)(R * R - dy * dy)), band = dy + R;
                uint16_t c = mix565(RGB(255, 220, 110), RGB(255, 60, 170), (uint32_t)band * 256u / (uint32_t)(R + 3));
                if (dy > -6 && ((dy + 20) % 4) < (dy + 8) / 4) continue;   /* the bars */
                tspan(cy0 + dy, cx0 - hw, cx0 + hw + 1, TSW(c));
            }
        }
        if (tr_zone(pmm / 1000) == TZ_FESTIVAL) {   /* the festival: fireworks over it, bursting and falling */
            uint32_t k;
            for (k = 0; k < 3u; k++) {
                uint32_t per = 1700u + k * 450u, t = (fm1_ms + k * 977u) % per, hh = th32((fm1_ms + k * 977u) / per * 3u + k);
                int32_t fx = 30 + (int32_t)(hh % 180u), fy = 14 + (int32_t)((hh >> 8) % 30u), rr = (int32_t)(t * 22u / per), a;
                static const uint16_t FC[4] = {RGB(255, 120, 200), RGB(120, 240, 255), RGB(255, 220, 120), RGB(200, 150, 255)};
                uint16_t fc = mix565(FC[(hh >> 16) & 3u], swap16(tvb[0]), t * 200u / per);
                if (t < 120u) continue;
                for (a = 0; a < 12; a++) {
                    int32_t px = fx + (mo_cos((uint32_t)a * 5461u) * rr >> 15), py = fy + (mo_sin((uint32_t)a * 5461u) * rr >> 15) + rr * rr / 60;
                    if (px >= 0 && px < TV_W && py >= 0 && py < TV_HY) { tvb[py * TV_W + px] = TSW(fc); if (px + 1 < TV_W) tvb[py * TV_W + px + 1] = TSW(fc); }
                }
            }
        }
        if (tp->fuji) {                         /* the mountain: a snow cap, far behind */
            int32_t mx = 70 - (((sx / 12) % 360) + 360) % 360, k;   /* (slow, and round again within a screen and a half: nearly always in view) */
            if (mx < -100) mx += 360;
            for (k = -60; k <= 60; k++) {
                int32_t ht = 52 - (k < 0 ? -k : k) * 52 / 60 - ((k < 0 ? -k : k) < 8 ? 0 : 0), y;
                uint16_t mc = TSW(mix565(mix565(tp->hill_near, tp->hill_far, 150), tp->top, 50)), sc = TSW(mix565(RGB(255, 230, 250), tp->hor, 60));
                if (mx + k < 0 || mx + k >= xmax) continue;
                for (y = TV_HY - ht; y < TV_HY; y++) tvb[y * TV_W + mx + k] = (y < TV_HY - ht + 4 + ht / 5 + ((k * 7) & 3)) ? sc : TSW(mix565(swap16(mc), tp->hor, (uint32_t)(y - (TV_HY - ht)) * 2u));   /* (the haze thickens toward its foot) */
            }
        }
        for (x = 0; x < xmax; x++) {
            int32_t hf_h = tmtn_far[(uint32_t)(x + sx / 3) & 255u], hn_h = tmtn_near[(uint32_t)(x + sx) & 255u] * 2;
            if (sea_now && x > xmax - 40) hf_h = hf_h * (xmax - x) / 40, hn_h = hn_h * (xmax - x) / 40;
            for (y = TV_HY - hf_h; y < TV_HY; y++) tvb[y * TV_W + x] = hf;
            for (y = TV_HY - hn_h; y < TV_HY; y++) tvb[y * TV_W + x] = hn;
        }
        if (tp->sun || tp->lit) tspan(TV_HY - 1, 0, xmax, TSW(mix565(tp->hor, RGB(255, 255, 255), 60)));   /* the horizon's glow */
    }
    {   /* the heart's run: how far it has gone from a tunnel */
        int32_t hs = tr_hs();
        h_pulse = 0; h_ab = 0;
        if (hs >= HS_PULSE) {
            uint32_t per = 4u * MU_STEP, ph = ((mu.abs & 3u) * MU_STEP + mu.t) * 256u / (per ? per : 1u);
            h_pulse = ph < 256u ? (256u - ph) * (256u - ph) / 256u : 0u;
        }
        if (hs >= HS_ABSTRACT) h_ab = (uint32_t)clampi((hs - HS_ABSTRACT) * 256 / 60, 0, 256);
        if (h_ab > 255u) h_ab = 255u;
    }
    /* the ground */
    for (r = 0; r < TV_GH; r++) {
        int32_t z = tz_mm[r], wz = pmm + z, jj8 = (r + 1) << 8, cx = tcx[r], y = TV_HY + r, dz = r ? tz_mm[r - 1] - z : 200000;
        uint32_t zn = tr_zone(wz / 1000), band = (uint32_t)(wz / 6000) & 1u;
        uint16_t side, *p = tvb + y * TV_W;
        int tn = zn == TZ_TUNNEL ? TL_TUNNEL : 0, xg = tr_at_xing(wz);
        if (zn == TZ_TOWN || zn == TZ_NEON) side = band ? tp->conc_a : tp->conc_b;
        else if (zn == TZ_RIVER) side = band ? tp->sea_a : tp->sea_b;
        else if (zn == TZ_TUNNEL) side = mix565(tp->wall, RGB(4, 0, 10), h_ab);
        else if (zn == TZ_STREET) side = tp->asphalt;
        else if (zn == TZ_HOUSES) side = band ? tp->yard : mix565(tp->yard, tp->conc_b, 90);
        else if (zn == TZ_SAKURA) side = band ? mix565(tp->grass_a, RGB(255, 150, 210), 60) : tp->grass_b;
        else if (zn == TZ_INDUSTRIAL) side = band ? mix565(tp->conc_b, RGB(20, 16, 24), 90) : mix565(tp->conc_b, RGB(20, 16, 24), 60);
        else if (zn == TZ_ONSEN) side = band ? mix565(tp->conc_a, RGB(150, 120, 110), 80) : tp->conc_b;
        else if (zn == TZ_WOODS) side = band ? mix565(tp->grass_b, RGB(10, 30, 20), 120) : mix565(tp->grass_a, RGB(10, 30, 20), 140);
        else if (zn == TZ_BEACH) side = tp->sand;
        else if (zn == TZ_FESTIVAL) side = band ? RGB(130, 70, 90) : RGB(116, 60, 84);
        else if (zn == TZ_ROCK) side = ((wz / 4000) * 7 % 5 < 2) ? mix565(RGB(235, 232, 248), tp->fog, 40) : mix565(tp->conc_b, RGB(80, 76, 90), 120);
        else side = band ? tp->grass_a : tp->grass_b;
        if (dz > 4000) side = mix565(side, tp->grass_b, 0);
        if (xg) side = tp->asphalt;
        side = TSW(tlit(side, z, tn));
        for (x = 0; x < TV_W; x++) p[x] = side;
        if (zn == TZ_BEACH) {                   /* the bay: sand both sides, the sea from a few metres off on the right, surf */
            tspan(y, cx + tjpx(8500, jj8), TV_W, TSW(tlit(dz > 4000 ? tp->sea_b : ((wz / 9000) & 1) ? tp->sea_a : tp->sea_b, z, 0)));
            if (((wz / 3000 + (int32_t)(fm1_ms / 400u)) % 5) == 0) tspan(y, cx + tjpx(8000, jj8), cx + tjpx(9200, jj8), TSW(tlit(RGB(240, 235, 255), z, 0)));
            if (tp->sun && r < 30 && (r + (int32_t)(fm1_ms / 120u)) % 3) { int32_t sxp = 150 - ((((tr.sky_x >> 8) / 3) % 400) + 400) % 400; if (sxp > cx + tjpx(8500, jj8)) tspan(y, sxp - 2 - r / 4, sxp + 3 + r / 4, TSW(mix565(RGB(255, 140, 180), tp->sea_a, (uint32_t)r * 5u))); }
        }
        if (zn == TZ_SEA) {
            tspan(y, 0, cx + tjpx(-1400, jj8), TSW(tlit(band ? tp->yard : tp->grass_b, z, 0)));
            tspan(y, cx + tjpx(1500, jj8), cx + tjpx(3300, jj8), TSW(tlit(tp->asphalt, z, 0)));
            tspan(y, cx + tjpx(3300, jj8), cx + tjpx(3800, jj8), TSW(tlit(tp->conc_a, z, 0)));
            tspan(y, cx + tjpx(3800, jj8), cx + tjpx(7500, jj8), TSW(tlit(tp->sand, z, 0)));
            tspan(y, cx + tjpx(7500, jj8), TV_W, TSW(tlit(dz > 4000 ? tp->sea_b : ((wz / 9000) & 1) ? tp->sea_a : tp->sea_b, z, 0)));
            if (tp->sun && r < 30) {            /* the sun's road on the water */
                int32_t sxp = 150 - ((((tr.sky_x >> 8) / 3) % 400) + 400) % 400;
                if ((r + (int32_t)(fm1_ms / 120u)) % 3) tspan(y, sxp - 2 - r / 4, sxp + 3 + r / 4, TSW(mix565(RGB(255, 140, 180), tp->sea_a, (uint32_t)r * 5u)));
            }
            if (dz < 2500 && ((wz / 2000) % 7) == 0) tspan(y, cx + tjpx(7500, jj8), cx + tjpx(9000, jj8), TSW(tlit(RGB(230, 220, 255), z, 0)));
        }
        if (zn == TZ_RIVERSIDE) {               /* the river along the right, the far bank beyond */
            tspan(y, cx + tjpx(1600, jj8), cx + tjpx(3200, jj8), TSW(tlit(tp->grass_b, z, 0)));
            tspan(y, cx + tjpx(3200, jj8), cx + tjpx(17000, jj8), TSW(tlit(dz > 4000 ? tp->sea_b : ((wz / 7000) & 1) ? tp->sea_a : tp->sea_b, z, 0)));
            tspan(y, cx + tjpx(17000, jj8), TV_W, TSW(tlit(tp->grass_a, z, 0)));
        }
        if (zn == TZ_FIELDS) {                  /* rice paddies: water with the sky in it, rows of green between */
            uint16_t pc = ((wz / 2500) & 1) ? mix565(tp->hor, tp->sea_a, 140) : mix565(tp->grass_a, RGB(60, 120, 60), 100);
            tspan(y, 0, cx + tjpx(-2200, jj8), TSW(tlit(pc, z, 0)));
            tspan(y, cx + tjpx(2200 + tR2, jj8), TV_W, TSW(tlit(pc, z, 0)));
        }
        if (zn == TZ_STREET) {
            tspan(y, cx + tjpx(-4600, jj8), cx + tjpx(-4300, jj8), TSW(tlit(tp->conc_a, z, 0)));
            tspan(y, cx + tjpx(4300, jj8), cx + tjpx(4600, jj8), TSW(tlit(tp->conc_a, z, 0)));
            if ((wz / 3000) & 1) {
                tspan(y, cx + tjpx(-1700, jj8), cx + tjpx(-1600, jj8), TSW(tlit(RGB(220, 210, 230), z, 0)));
                tspan(y, cx + tjpx(1600, jj8), cx + tjpx(1700, jj8), TSW(tlit(RGB(220, 210, 230), z, 0)));
            }
        }
        if (tp->rain && (zn == TZ_STREET || zn == TZ_TOWN || zn == TZ_NEON || xg) && r > 6 && ((r * 7 + (int32_t)(wz / 2000)) % 5) == 0) {
            int32_t mx = (int32_t)(th32((uint32_t)(wz / 1500)) % 240u);   /* puddles: a streak of reflected neon */
            tspan(y, mx, mx + 3 + r / 10, TSW(mix565(side == 0 ? 0 : swap16(side), (wz / 7000) & 1 ? tp->neon1 : tp->neon2, 120)));
        }
        if (zn == TZ_RIVER) tspan(y, cx + tjpx(-2000, jj8), cx + tjpx(2000 + tR2, jj8), TSW(tlit(RGB(80, 70, 90), z, 0)));
        ttrack(y, cx, jj8, 0, z, wz, dz, tn, zn != TZ_STREET && !xg);
        if (tr.L->dbl) ttrack(y, cx, jj8, tR2, z, wz, dz, tn, !xg);
        else if (tr_in_loop(wz)) ttrack(y, cx, jj8, 3400, z, wz, dz, tn, !xg && zn != TZ_STREET);
    }
    /* the upright things, far to near */
    tr_sprites();
    si = 0;
    for (r = 0; r < TV_GH; r++) {
        int32_t z = tz_mm[r], wz = pmm + z, jj8 = (r + 1) << 8, cx = tcx[r], yg = TV_HY + r, wy = yg - tjpx(5200, jj8);
        uint32_t zn = tr_zone(wz / 1000);
        if (zn == TZ_TUNNEL) {
            int32_t xl = cx + tjpx(-2300, jj8), xr = cx + tjpx(2300 + tR2, jj8), yc = yg - tjpx(5400, jj8);
            uint16_t wc = TSW(mix565(mix565(tlit(tp->wall, z, TL_TUNNEL), tp->neon1, h_pulse / 4u), RGB(4, 0, 10), h_ab));   /* (the heart: the walls flush on the beat) */
            if (!prev_on) { pxl = xl; pxr = xr; pyc = yc; }
            trect(xl, yc, pxl + 1, yg + 1, wc);
            trect(pxr, yc, xr + 1, yg + 1, wc);
            trect(xl, yc, xr + 1, pyc + 1, TSW(mix565(tlit(RGB(36, 30, 42), z, TL_TUNNEL), RGB(4, 0, 10), h_ab)));
            if (r && wz / (h_ab ? 10000 : 20000) != (pmm + tz_mm[r - 1]) / (h_ab ? 10000 : 20000)) {   /* lamps: cyan and magenta strips (the heart: on the beat) */
                int32_t lh = tjpx(160, jj8) + 1, ly = yg - tjpx(3100, jj8), ll = lh * (3 + (int32_t)h_pulse / 64);
                uint32_t ring = (uint32_t)(wz / 10000);
                if (h_ab < 200u) {
                    trect(xl - lh, ly, xl + 1, ly + ll, TSW(mix565(tlit(tp->neon2, z / 4, TL_TUNNEL), RGB(255, 255, 255), h_pulse * 3u / 4u)));
                    trect(xr, ly, xr + lh + 1, ly + ll, TSW(mix565(tlit(tp->neon1, z / 4, TL_TUNNEL), RGB(255, 255, 255), h_pulse * 3u / 4u)));
                }
                if (h_ab) {                     /* the walls become light: a portal each ring, a diamond inside it */
                    uint16_t pc = TSW(mix565(RGB(4, 0, 10), (ring & 1u) ? tp->neon1 : tp->neon2, h_ab > 128u ? 255u : h_ab * 2u));
                    int32_t mx = (xl + xr) / 2, my = (yc + yg) / 2;
                    tline(xl, yc, xr, yc, pc); tline(xr, yc, xr, yg, pc); tline(xr, yg, xl, yg, pc); tline(xl, yg, xl, yc, pc);
                    if (h_ab > 128u) {
                        uint16_t dc = TSW(mix565(RGB(4, 0, 10), (ring & 1u) ? tp->neon2 : tp->neon1, (h_ab - 128u) * 2u > 255u ? 255u : (h_ab - 128u) * 2u));
                        if (ring % 3u == 0u) { tline(mx, yc, xr, my, dc); tline(xr, my, mx, yg, dc); tline(mx, yg, xl, my, dc); tline(xl, my, mx, yc, dc); }
                        else if (ring % 3u == 1u) { tline(xl, yg, mx, yc, dc); tline(mx, yc, xr, yg, dc); }   /* a triangle */
                        else { int32_t q = (xr - xl) / 4; tline(xl + q, yc + (yg - yc) / 4, xr - q, yc + (yg - yc) / 4, dc); tline(xl + q, yg - (yg - yc) / 4, xr - q, yg - (yg - yc) / 4, dc); }
                    }
                }
            }
            pxl = xl; pxr = xr; pyc = yc;
            prev_on = 1;
        } else prev_on = 0;
        if (zn == TZ_HOUSES && !tr_at_xing(wz)) {
            int32_t xl = cx + tjpx(-2400, jj8), xr = cx + tjpx(2400 + (tr_in_loop(wz) ? 3400 : tR2), jj8), top = yg - tjpx(800, jj8);
            uint16_t wc = TSW(tlit(RGB(110, 96, 120), z, 0)), tc = TSW(tlit(RGB(80, 70, 90), z, 0));
            if (!fon) { pfl = xl; pfr = xr; }
            if (!(wz > dist_mm - plat - 15000 && wz < dist_mm + 30000)) trect(xl, top, pfl + 1, yg + 1, wc);
            trect(pfr, top, xr + 1, yg + 1, wc);
            tspan(top, xl, pfl + 1, tc);
            tspan(top, pfr, xr + 1, tc);
            pfl = xl; pfr = xr;
            fon = 1;
        } else fon = 0;
        if (wz > dist_mm - plat && wz < dist_mm + 20000) {   /* the platform, its edge lit */
            int32_t top = yg - tjpx(1100, jj8), xe = cx + tjpx(-1650, jj8), xy = cx + tjpx(-1950, jj8), xf = cx + tjpx(-1800, jj8);
            int32_t xo = cx + tjpx(-6000, jj8);
            if (!ptop) { ptop = top; pxp = xe; }
            trect(xo, ptop, xy, top + 1, TSW(tlit(RGB(120, 110, 130), z, 0)));
            trect(xy, ptop, xf, top + 1, TSW(tlit(RGB(255, 200, 40), z / 2, 0)));
            trect(xf, ptop, xe, top + 1, TSW(tlit(tp->lit ? tp->neon2 : RGB(220, 215, 225), z / 2, 0)));
            trect(xe, top, pxp + 1, yg + 1, TSW(tlit(RGB(70, 60, 80), z, 0)));
            if (tr.L->canopy && wz > dist_mm - plat * (tr.L->canopy == 2 ? 95 : 70) / 100 && wz < dist_mm - plat * (tr.L->canopy == 2 ? 5 : 20) / 100) {
                int32_t ry = yg - tjpx(4100, jj8), xc = cx + tjpx(-1800, jj8);
                if (!pcy2) pcy2 = ry;
                trect(xo, ry, xc + 1, pcy2 + 1, TSW(tlit(RGB(36, 26, 50), z, 0)));
                trect(xc - 1, ry, xc + 2, pcy2 + 2, TSW(TR_STYLE_COL[tr.L->style_to % 11u]));
                pcy2 = ry;
            } else pcy2 = 0;
            ptop = top; pxp = xe;
        } else ptop = 0;
        if (zn == TZ_RIVER) {
            int32_t cy = yg - tjpx(2700, jj8), xl = cx + tjpx(-2150, jj8), xr = cx + tjpx(2150 + tR2, jj8);
            uint16_t gc = TSW(tlit(RGB(150, 70, 60), z, 0));
            if (pch_l) { tline(pch_l, pcy, xl, cy, gc); tline(pch_r, pcy, xr, cy, gc); }
            pch_l = xl; pch_r = xr; pcy = cy;
        } else pch_l = 0;
        if (r && wy >= 0) tline(tcx[r - 1], pyw, cx, wy, TSW(tlit(RGB(30, 26, 40), z, zn == TZ_TUNNEL ? TL_TUNNEL : 0)));
        pyw = wy;
        while (si < tnspr && (r == TV_GH - 1 || tspr[si].z > tz_mm[r + 1])) tspr_draw(&tspr[si++]);
    }
    if (tp->rain && tr_zone((int32_t)tr_pos_m()) != TZ_TUNNEL) {   /* rain: slanted streaks over everything (not in a tunnel) */
        uint32_t k;
        for (k = 0; k < 90u; k++) {
            uint32_t h = th32(k * 2654435761u);
            int32_t px = (int32_t)((h + fm1_ms / 3u * (1u + (h >> 30))) % 260u) - 10, py = (int32_t)((h >> 8) + fm1_ms / 2u) % TV_H;
            tline(px, py, px - 2, py + 6, TSW(RGB(150, 170, 230)));
        }
    }
}

/* ======================================================================== events */
static void tr_score(int32_t pts, const char *txt)
{
    tr.ev_pts += pts;
    tr.toast = txt;
    tr.toast_until = fm1_ms + 1600u;
    tr.toast_c = pts > 0 ? C_CYAN : C_ROSE;
    if (pts > 0) sfx_chime(84, 70, 0, 128); else sfx_buzz(0);
}
static void tr_cue(uint32_t c, uint32_t ev, uint32_t arg) { if (c > tr.cue) { tr.cue = c; tr.cue_ev = ev; tr.cue_arg = arg; } }
static void tr_events(void)
{
    uint32_t i;
    int32_t pmm = tr_pos_mm(), m = pmm / 1000, v = tr_kmh();
    tr.cue = TC_NONE;
    for (i = 0; i < tr.L->nev && i < TR_MAXEV; i++) {
        const tr_ev_t *e = &tr.L->ev[i];
        int32_t a = e->at;
        if (tr.evs[i] == 2u || e->type == TE_NONE) continue;
        if ((e->type == TE_FANS && !(tr_features & F_HORN)) || (e->type == TE_WHISTLE && !(tr_features & F_WHISTLE)) || (e->type == TE_CHECK && !(tr_features & F_CALL))) {
            tr.evs[i] = 2; continue;            /* (not on the train yet: not asked) */
        }
        if (e->type == TE_FANS) {
            if (m >= a - 220 && m <= a + 5) { tr_cue(TC_FANS, i, 0); if (tr.horn) { tr.evs[i] = 2; tr_score(1, TR_HORN_OK[i % 4u]); } }
            else if (m > a + 5) { tr.evs[i] = 2; tr_score(-1, T_TR_NO_HORN); }
        } else if (e->type == TE_WHISTLE) {     /* a whistle board: the whistle before the tunnel or the bridge */
            if (m >= a - 200 && m <= a + 5) { tr_cue(TC_WHISTLE, i, 0); if (tr.whistle) { tr.evs[i] = 2; tr_score(1, T_TR_WHISTLE_OK); } }
            else if (m > a + 5) { tr.evs[i] = 2; tr_score(-1, T_TR_NO_WHISTLE); }
        } else if (e->type == TE_CHECK) {
            if (m >= a - 150 && m < a) { if (tr.ack_ev != i + 1u) tr_cue(TC_CHECK, i, e->arg); }
            else if (m >= a) {
                tr.evs[i] = 2;
                if (tr.ack_ev != i + 1u) tr_score(-1, T_TR_NO_CALL);
                else if (v > e->arg + 1) tr_score(-1, T_TR_TOO_FAST);
                else tr_score(1, T_TR_CHECK_OK);
            }
        } else if (e->type == TE_ONCOMING) {
            int32_t gap;
            if (!tr.evs[i]) {
                if (tr.on_live || (e->arg && m < a) || (!e->arg && m < a - 400)) continue;
                tr.evs[i] = 1;
                tr.on_live = 1; tr.on_judged = 0; tr.on_whoosh = 0; tr.bright_t = 0;
                tr.on_v = e->arg;
                tr.on_mm = e->arg ? pmm + 400000 : a * 1000;
            }
            if (tr.on_v) tr.on_mm -= tr.on_v * 1000 / 216;
            gap = tr.on_mm - pmm;
            if (!(tr_features & F_LIGHTS)) tr.on_judged = 1;   /* (no dimmer yet: the meeting is not judged) */
            if (!tr.on_judged) {
                if (!tr.dim) tr_cue(TC_DIM, i, 0);
                if (gap <= 120000) { tr.on_judged = 1; tr_score(tr.dim ? 1 : -1, tr.dim ? T_TR_DIMMED : T_TR_GLARE); }
            }
            if (tr.on_v && !tr.on_whoosh && gap < 45000) { tr.on_whoosh = 1; ta_pass_n++; }
            if (gap <= -2500) { tr.evs[i] = 2; tr.on_live = 0; if (tr.dim) tr.bright_t = 8u * TR_HZ; }
        }
    }
    if (tr.bright_t) {
        if (!tr.dim || tr.on_live) tr.bright_t = 0;
        else { tr_cue(TC_BRIGHT, 0, 0); if (--tr.bright_t == 0) tr_score(-1, T_TR_LIGHTS_LOW); }
    }
    if (tr_autoplay) {
        if (tr.cue == TC_DIM) tr.dim = 1;
        if (tr.cue == TC_BRIGHT && tr.bright_t < 6u * TR_HZ) tr.dim = 0;
        if (tr.cue == TC_CHECK && m >= (int32_t)tr.L->ev[tr.cue_ev].at - 100) tr.ack_ev = tr.cue_ev + 1u;
        tr.horn = tr.cue == TC_FANS;
        tr.whistle = tr.cue == TC_WHISTLE;
    }
}

/* ======================================================================== the mechanism */
/* the start-up's phrase: the cab sings it, you play it back on the keys. Three notes, each leg its own; once the
 * music box has sung, the line's own melody (the departure melody's opening), five */
static const uint8_t TR_PHRASE_S[6][3] = {{64, 67, 69}, {69, 67, 64}, {60, 64, 67}, {72, 71, 69}, {65, 69, 72}, {67, 64, 60}};
static const uint8_t TR_PHRASE_L[5] = {57, 60, 64, 69, 67};
#define TR_PH_MS 300u
static uint8_t tr_phseq[20];
static void tr_phrase_sing(uint32_t delay_ms)
{
    uint32_t i, n = 0;
    if (delay_ms) { tr_phseq[n++] = 0; tr_phseq[n++] = (uint8_t)(delay_ms * 441u / 42000u); }
    for (i = 0; i < tr.ph_len; i++) { tr_phseq[n++] = tr.ph[i]; tr_phseq[n++] = 3; }   /* 3 x 4200 frames: 286 ms */
    tr_phseq[n++] = 0; tr_phseq[n] = 0;
    ta_chime = tr_phseq; ta_chime_n++;
    tr.ph_state = 0; tr.ph_got = 0; tr.ph_t = fm1_ms + delay_ms;
}
static uint8_t tr_sigseq[20];
static const uint8_t *tr_sig_seq(void)          /* the player's melody as a chime sequence (an octave up), or 0 */
{
    uint32_t i;
    if (!save.sig[0]) return 0;
    for (i = 0; i < 8u; i++) { tr_sigseq[2u * i] = (uint8_t)(save.sig[i] + 12u); tr_sigseq[2u * i + 1u] = (uint8_t)(i == 7u ? 6u : 2u); }
    tr_sigseq[16] = 0; tr_sigseq[17] = 0;
    return tr_sigseq;
}
static void tr_phrase_new(void)
{
    uint32_t i, leg = (uint32_t)(tr.L - TR_LEGS);
    if ((tr_features & F_SYNTH) && save.sig[0]) { tr.ph_len = 5; for (i = 0; i < 5u; i++) tr.ph[i] = save.sig[i]; }   /* (your own melody, to wake the cab) */
    else if (tr_features & F_SYNTH) { tr.ph_len = 5; for (i = 0; i < 5u; i++) tr.ph[i] = TR_PHRASE_L[i]; }
    else { tr.ph_len = 3; for (i = 0; i < 3u; i++) tr.ph[i] = TR_PHRASE_S[leg % 6u][i]; }
    tr.ph_miss = 0;
}
static int32_t tr_phrase_now(void)              /* the note the cab is singing now (-1: none) */
{
    uint32_t el;
    if (tr.ph_state != 0u || (int32_t)(fm1_ms - tr.ph_t) < 0) return -1;
    el = fm1_ms - tr.ph_t;
    return el / TR_PH_MS < tr.ph_len && el % TR_PH_MS < 240u ? (int32_t)tr.ph[el / TR_PH_MS] : -1;
}
static void tr_phrase_key(uint32_t note)        /* a key in the start-up: right, on; wrong, from the top */
{
    if (tr.ph_state != 1u) return;
    if (note == tr.ph[tr.ph_got]) {
        if (++tr.ph_got == tr.ph_len) {         /* the phrase: the cab answers, and the reverser is free */
            tr.prep = TRP_REV; tr.prep_t = fm1_ms; ta_chime = TA_POWER; ta_chime_n++;
        }
    } else {
        sfx_buzz(au_time() + ms_frames(80));
        if (tr.ph_miss < 9u) tr.ph_miss++;
        tr.ph_state = 2; tr.ph_t = fm1_ms;
    }
}

/* ---- the synth break: a stretch at one limit where the line lets you drift into the music. A key on the beat
 * (a quarter of the backing, +-110 ms) scores, one a beat, five at most; off the beat, nothing; nothing, nothing */
#define TR_ZONE_WIN 4851                        /* 110 ms in frames */
static int32_t tr_beat(uint32_t *idx)           /* frames to the nearest beat (the backing's clock), and which beat */
{
    uint32_t a, t, ph, per = 4u * MU_STEP;
    do { a = mu.abs; t = mu.t; } while (a != mu.abs);
    ph = (a & 3u) * MU_STEP + t;
    *idx = (a >> 2) + (ph > per / 2u ? 1u : 0u);
    return (int32_t)(ph > per / 2u ? per - ph : ph);
}
static int tr_in_zone(void)
{
    int32_t m = (int32_t)tr_pos_m();
    uint32_t i;
    for (i = 0; i < tr.L->nbrk; i++) if (m >= (int32_t)tr.L->brk[i * 2u] && m < (int32_t)tr.L->brk[i * 2u + 1u]) return 1;
    return 0;
}
static void tr_zone_hit(void)                   /* a key in the break */
{
    uint32_t idx;
    if (tr.zone != 1u || tr.zone_bp >= 5u) return;
    if (tr_beat(&idx) <= TR_ZONE_WIN && idx != tr.zone_beat) { tr.zone_beat = idx; tr.zone_pts++; tr.zone_bp++; tr.zone_hit_t = fm1_ms; }
}
static void tr_key(uint32_t k) { (void)k; if (tr.prep >= TRP_READY && !tr.done) tr_zone_hit(); }   /* (at once, even while the screen is sent) */

static void tr_set_notch(int32_t n)
{
    int32_t old = tr.notch;
    n = clampi(n, TR_EB, TR_PMAX);
    if (n == old || tr.done) return;
    if (n == TR_EB && tr.v) tr.pen_eb++;
    if (old < 0 && n > old) ta_hiss_n++;
    tr.notch = n;
    sfx_click(0, 30, 128);
}

/* ---- the endless line: 24 km made up at random (scenery, curves, grades, limits 70..120), a tunnel at each end;
 * 1.5 km from the end, inside its tunnel, a new stretch takes over inside its own: the seam is never seen */
#define EG_D 24000u
static tr_seg_t eg_lim[24], eg_cur[100], eg_grd[60], eg_zon[90];
static const tr_ev_t EG_EV[1] = {{65535, 0, 0}};
static const uint16_t EG_XG[1] = {65535};
static tr_leg_t eg_leg;
static const uint8_t EG_Z[13] = {TZ_COUNTRY, TZ_TOWN, TZ_SEA, TZ_SAKURA, TZ_HOUSES, TZ_NEON, TZ_FIELDS, TZ_WOODS, TZ_RIVERSIDE,
                                 TZ_ROCK, TZ_MVILLAGE, TZ_INDUSTRIAL, TZ_COUNTRY};
static void tr_endless_gen(void)
{
    uint32_t m, n;
    n = 0; eg_zon[n].at = 0; eg_zon[n++].v = TZ_TUNNEL;
    for (m = 400; m < EG_D - 1700u && n < 86u;) {
        uint32_t r = rndn(100), len;
        if (r < 8u) { eg_zon[n].v = TZ_TUNNEL; len = 200u + rndn(200); }
        else if (r < 16u) { eg_zon[n].v = TZ_RIVER; len = 100u + rndn(60); }
        else { eg_zon[n].v = EG_Z[rndn(13)]; len = 400u + rndn(700); }
        eg_zon[n++].at = (uint16_t)m; m += len;
    }
    eg_zon[n].at = (uint16_t)(EG_D - 1700u); eg_zon[n++].v = TZ_TUNNEL;
    eg_leg.nzon = (uint8_t)n;
    for (n = 0, m = 0; m < EG_D && n < 100u; n++) {
        eg_cur[n].at = (uint16_t)m; eg_cur[n].v = (int16_t)(rndn(10) < 4u ? 0 : (int32_t)rndn(41) - 20);
        m += 300u + rndn(400);
    }
    eg_leg.ncur = (uint8_t)n;
    for (n = 0, m = 0; m < EG_D && n < 60u; n++) { eg_grd[n].at = (uint16_t)m; eg_grd[n].v = (int16_t)((int32_t)rndn(21) - 10); m += 500u + rndn(800); }
    eg_leg.ngrd = (uint8_t)n;
    for (n = 0, m = 0; m < EG_D && n < 24u; n++) { eg_lim[n].at = (uint16_t)m; eg_lim[n].v = (int16_t)(70 + 10 * (int32_t)rndn(6)); m += 1500u + rndn(1600); }
    eg_leg.nlim = (uint8_t)n;
    eg_leg.dist = EG_D; eg_leg.time_s = 0;
    eg_leg.lim = eg_lim; eg_leg.cur = eg_cur; eg_leg.grd = eg_grd; eg_leg.zon = eg_zon; eg_leg.ev = EG_EV; eg_leg.xing = EG_XG;
    eg_leg.nev = 1; eg_leg.nxing = 1; eg_leg.sky = (uint8_t)rndn(6); eg_leg.loop = 0; eg_leg.dbl = (uint8_t)rndn(2);
    eg_leg.plat = 60; eg_leg.st_from = 0; eg_leg.st_to = 0; eg_leg.line = "ENDLESS"; eg_leg.canopy = 0;
    eg_leg.nbrk = 0; eg_leg.music = tr_song;
}
static const char *const TR_SONG_NAME[MU_NSONG] = {"NEON DEPARTURE", "NIGHT DRIVE HOME", "RAIN RUN", "CITY FUNK", "OUTRUN", "DUB TUNNEL", "ASCENT", "THE HEART", "THE BEACH", "CHERRY HOUSE", "LIGHTHOUSE", "SEASIDE TRAM", "FESTIVAL TRAM"};

static void tr_enter(void)
{
    uint32_t i;
    int32_t leg = PRM(P_TRAVEL_LEG);
    tv_init();
#ifdef DINGHY_HOST
    tr_autoplay = getenv("DENSHA_AUTOPLAY") != 0;
#endif
    if (tr_mode == TRM_ENDLESS) { tr_endless_gen(); tr.L = &eg_leg; tr_odo_base = 0; }
    else tr.L = tr_force ? tr_force : &TR_LEGS[leg < 0 || leg >= (int32_t)NTRLEGS ? 0 : leg];
    tp = &(tr_mode == TRM_ENODEN ? EN_PAL : TR_PAL)[tr.L->sky < 6u ? tr.L->sky : 1u];
    tr_heart = tr_mode == TRM_STORY && (TR_STPOS[tr.L->st_to].hidden || TR_STPOS[tr.L->st_from].hidden);
    tr.warp = 0; tr.warp_t = 0; tr.inf_at = 0; tr_white = 0; mu_hp = -1; mu_master = 65536; mu_fade = 0;
    for (i = 0; i < tr.L->nlim; i++) if (tr.L->lim[i].v >= 999) tr.inf_at = tr.L->lim[i].at;   /* (where the limit ends) */
    if (tr_heart && TR_STPOS[tr.L->st_to].hidden) mu_hp = 0;
    if (tr_heart) { tp = &HEART_PAL; tr_features &= (uint8_t)~F_STOPWATCH; }   /* (no timetable runs here) */
    tr_rain = tp->rain;                         /* rain: a leg's own rain sky, or the weather on the day */
    if (!tr_rain && !tr_heart && tr_mode != TRM_ENDLESS && tr.L->sky != TS_SUNSET && !tr.L->norain && rndn(100) < tr_rain_pct) {
        uint32_t tun = 0, z;                    /* (not on a line that is mostly tunnel) */
        for (z = 0; z < tr.L->nzon; z++)
            if (tr.L->zon[z].v == TZ_TUNNEL) tun += (z + 1u < tr.L->nzon ? tr.L->zon[z + 1u].at : tr.L->dist) - tr.L->zon[z].at;
        if (tun * 2u < tr.L->dist) { tr_rain = 1; tp = &(tr_mode == TRM_ENODEN ? EN_PAL : TR_PAL)[TS_RAIN]; }
    }
    ui_theme(tr_mode == TRM_ENODEN ? 1 : tr_heart ? 2 : 0);
    tr.t = 0; tr.acc = 0; tr.last_ms = fm1_ms; tr.done = 0; tr.stopped_ms = 0;
    tr.pos = 0; tr.rem = 0; tr.v = 0; tr.notch = -8; tr.brk = 8 * 32768;
    tr.time_left = ((int32_t)tr.L->time_s + (tr_rain ? 6 : 0)) * TR_HZ;   /* (rain: 6 s more on the timetable) */
    tr.over_ticks = tr.pen_speed = tr.pen_eb = 0; tr.horn = 0; tr.dim = 0;
    tr.sky_x = 0; tr.ev_pts = 0;
    for (i = 0; i < TR_MAXEV; i++) tr.evs[i] = 0;
    tr.cue = TC_NONE; tr.ack_ev = 0; tr.on_live = 0; tr.bright_t = 0; tr.toast = 0;
    tr.kprev = fm1_in.notes;
    tr.ev_max = 0;
    for (i = 0; i < tr.L->nev && i < TR_MAXEV; i++) {
        uint32_t t = tr.L->ev[i].type;
        if (t == TE_FANS && !(tr_features & F_HORN)) continue;
        if (t == TE_WHISTLE && !(tr_features & F_WHISTLE)) continue;
        if (t == TE_CHECK && !(tr_features & F_CALL)) continue;
        if (t == TE_ONCOMING && !(tr_features & F_LIGHTS)) continue;
        if (t) tr.ev_max++;
    }
    for (i = 0; i < LD_ECHO; i++) ld_echo[i] = 0;   /* (no echo left over from the last leg) */
    tr.upd = 0;
    tr.prep = TRP_OFF; tr.prep_t = fm1_ms; tr.appr = 0; tr.mtoast_t = 0; tr.lead_k = 27u; tr.ph_state = 0;
    ta_lead_note = 0;
    ta_music = 0;
    tr.zone = 0; tr.zone_bp = 0; tr.zone_pts = 0; tr.zone_beat = 0xFFFFFFFFu; tr.zone_hit_t = 0; tr.paused = 0;
    tv_cached = 0;
    au_hook = 0;
    mu_set_song(tr_mode == TRM_ENDLESS ? tr_song : tr.L->music);   /* the line's own song (the endless line: the one chosen) */
    if (tr_mode == TRM_ENDLESS) { tr.prep = TRP_READY; tr.time_left = 0x7FFFFFFF; }   /* (under way at once, no clock) */
    kb_hook = tr_key;
    au_hook = tr_audio;
    /* (the departure melody plays when the cab is switched on, once it is unlocked) */
}

static void tr_evaluate(void)
{
    int32_t d = tr_rel(), cm = (d < 0 ? -d : d) / 10;
    tr.r_cm = -d / 10;
    /* the stop: within 1 m +10, 2 m +8, 3 m +6, 5 m +3, 10 m 0; past by more, -1 a 5 m (to -9); 30 m past, -10 */
    if (d < 0 && cm > 3000) { tr.r_grade = T_TR_OVERRUN; tr.r_stop = -10; }
    else if (cm <= 100) { tr.r_grade = T_TR_PERFECT; tr.r_stop = 10; }
    else if (cm <= 300) { tr.r_grade = T_TR_GREAT; tr.r_stop = cm <= 200 ? 8 : 6; }
    else if (cm <= 500) { tr.r_grade = T_TR_GOOD; tr.r_stop = 3; }
    else if (cm <= 1000) { tr.r_grade = T_TR_OK; tr.r_stop = 0; }
    else { tr.r_grade = T_TR_POOR; tr.r_stop = -(cm / 500 + 1); if (tr.r_stop < -9) tr.r_stop = -9; }
    /* the time: on time (0..10 s to spare: room to beat the autopilot) +5, earlier +2, late -1 a 2 s (to -20) */
    if (tr.time_left < 0) tr.r_time = -((-tr.time_left + 2 * TR_HZ - 1) / (2 * TR_HZ));
    else tr.r_time = tr.time_left <= 10 * TR_HZ ? 5 : 2;
    if (tr.r_time < -20) tr.r_time = -20;
    {   /* the tier: the points won (stop, time, events, less speeding and EB) against the most there were */
        int32_t pts, mx = 10 + ((tr_features & F_STOPWATCH) ? 5 : 0) + tr.ev_max + ((tr_features & F_SYNTH) ? 5 * (int32_t)tr.L->nbrk : 0);
        if (!(tr_features & F_STOPWATCH)) tr.r_time = 0;   /* (no stopwatch yet: the time is not kept) */
        pts = tr.r_stop + tr.r_time + tr.ev_pts + (int32_t)tr.zone_pts - (int32_t)tr.pen_speed - (int32_t)tr.pen_eb;
        tr.r_pts = pts; tr.r_mx = mx; tr_last_pts = pts; tr_last_mx = mx;
        int32_t pct = pts <= 0 ? 0 : pts * 100 / mx;
        tr.tier = (uint8_t)(pct >= 90 ? 7 : pct >= 75 ? 6 : pct >= 60 ? 5 : pct >= 45 ? 4 : pct >= 30 ? 3 : pct >= 15 ? 2 : 1);
        tr_last_tier = tr.tier;
    }
    tr.done = 1;
    ta_thunk_n++;
    ta_hiss_n++;
    ta_chime = TA_ARRIVE; ta_chime_n++;         /* the doors open */
    ta_music = 0;
#ifdef DINGHY_HOST
    printf("%6u ms  travel %s>%s: stop %d cm %s, %d s to spare, events %+d of %d, zone %u, tier %c\n", fm1_ms, tr_names[tr.L->st_from],
           tr_names[tr.L->st_to], tr.r_cm, tr.r_grade, tr.time_left / TR_HZ, tr.ev_pts, tr.ev_max, tr.zone_pts, TR_TIER[tr.tier]);
#endif
}

static void tr_tick(void)
{
    tr.t++;
    if (tr_autoplay && tr.zone != 1u) tr_auto();   /* (in the zone the cruise drives) */
    tr_phys();
    tr_events();
    tr.time_left--;
    if (tr_kmh() > tr_limit() + 1 && tr_limit() < 999) {
        if (++tr.over_ticks >= 5 * TR_HZ) { tr.over_ticks = 0; tr.pen_speed++; }   /* -1 a 5 s over */
    } else tr.over_ticks = 0;
    if (tr.inf_at && tr_pos_m() >= tr.inf_at) {   /* the limit gone: full power, and it is away */
        if (tr.notch == TR_PMAX && !tr.warp) { tr.warp = 1; tr.warp_t = fm1_ms ? fm1_ms : 1u; }
        if (tr_autoplay) tr.notch = TR_PMAX;
        if (tr.warp && fm1_ms - tr.warp_t >= 24000u && !tr.done) { tr.done = 1; tr_white = 1; tr.stopped_ms = 0; ta_music = 0; }   /* into the light: ~20 s flat out, the white over the last ten */
        return;
    }
    if (!tr.v && tr.t > TR_HZ && tr_rel() <= 10000 && !tr.inf_at) tr_evaluate();
}

static int tr_update(void)
{
    uint32_t keys = in.kheld, down = in.kdown, k, run, xg = 0, i;
    int32_t e = in.enc[ENC_PRESETS] + in.enc[ENC_ALGORITHM], n, ld = 0, v, m;
    if (tr_mode == TRM_ENDLESS) {               /* ALGORITHM: the backing track, through the game's songs */
        int32_t a = in.enc[ENC_ALGORITHM];
        static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
        static const char *const FN[4] = {"DRUMS ", "BASS ", "PADS ", "ARP "};
        uint32_t f;
        e = in.enc[ENC_PRESETS];
        for (f = 0; f < 4u; f++) {              /* K1..K4: the faders, drums bass pads arp */
            int32_t kd = in.enc[KN[f]];
            if (kd) {
                static char fb[24];
                char *q;
                uint32_t j;
                mu_mix[f] = (uint8_t)clampi((int32_t)mu_mix[f] + (kd > 0 ? 1 : -1), 0, 8);
                q = str_cat(fb, FN[f]);
                for (j = 0; j < 8u; j++) *q++ = j < mu_mix[f] ? '#' : '.';
                *q = 0;
                tr.toast = fb; tr.toast_until = fm1_ms + 1200u; tr.toast_c = C_CYAN;
            }
        }
        {   /* the train is the clock: the song's tempo from 70% standing to 117% at full speed */
            uint32_t kmh = (uint32_t)clampi(tr_kmh(), 0, 160), bpm = (uint32_t)mu_bpm * (70u + kmh * 35u / 120u) / 100u;
            if (bpm < 40u) bpm = 40u;
            mu.stp = 44100u * 15u / bpm;
        }
        if (a) {
            static char sb[24];
            tr_song = (uint8_t)((tr_song + (a > 0 ? 1u : MU_NSONG - 1u)) % MU_NSONG); mu_set_song(tr_song);
            str_cat(str_cat(sb, "SONG: "), TR_SONG_NAME[tr_song]);
            tr.toast = sb; tr.toast_until = fm1_ms + 1600u; tr.toast_c = C_ROSE;
        }
        if (tr_pos_m() + 1500u >= EG_D) {      /* inside the end tunnel: a new stretch, inside its own */
            uint32_t over = tr_pos_m() - (EG_D - 1500u);
            tr_odo_base += EG_D - 1500u - 200u;
            tr_endless_gen();
            tr.pos = (200u + over) << 16; tr.on_live = 0;
            tp = &TR_PAL[eg_leg.sky]; tr_rain = tp->rain;
#ifdef DINGHY_HOST
            printf("%6u ms  endless: a new stretch at %u m, %d km/h%c", fm1_ms, tr_odo_base + tr_pos_m(), tr_kmh(), 10);
#endif
        }
    }
    if (!tr.upd) { tr.upd = 1; tr.prep_t = fm1_ms; }   /* (the start-up's clock from the first frame in the cab, not the card) */
    if (PRESSED(BTN_GLO)) {                     /* the music: off / on */
        tr_music_off = !tr_music_off; tr.mtoast_t = fm1_ms ? fm1_ms : 1u;
    }
    if (tr.prep >= TRP_READY && !tr.done && tr.L->nbrk && (tr_features & F_SYNTH)) {
        int in = tr_in_zone();
        if (tr.zone != 1u && in) { tr.zone = 1; tr.zone_t = fm1_ms; tr.toast = 0; tr.zone_bp = 0; }   /* (nothing else on the screen as it begins) */
        else if (tr.zone == 1u && !in) {        /* out of the break: what it gave */
            char *q = tr.zone_msg;
            tr.zone = 2; tr.zone_t = fm1_ms; tr.notch = 0;   /* (the controls back: neutral) */
            if (tr.zone_bp) { *q++ = '+'; *q++ = (char)('0' + tr.zone_bp); *q++ = ' '; }
            str_cat(q, tr.zone_bp ? T_TR_ZONE_OUT : T_TR_ZONE_PASS);
            tr.toast = tr.zone_msg; tr.toast_until = fm1_ms + 2200u; tr.toast_c = tr.zone_bp ? C_CYAN : C_INK2;
            if (tr.zone_bp) sfx_chime(88, 70, 0, 128);
        }
        if (tr_autoplay && tr.zone == 1u && tr.zone_bp < 4u) {   /* (host: the autopilot taps a few beats, and misses one) */
            uint32_t idx;
            if (tr_beat(&idx) < 900 && idx != tr.zone_beat && (idx % 3u)) tr_zone_hit();
        }
    }
    if (mu_hp >= 0 && !tr.done) {               /* the heart's song: how far it has built, and its tempo rising with it */
        uint32_t pm = tr_pos_m(), bpm;
        mu_hp = tr.inf_at ? (int32_t)(pm >= tr.inf_at ? 256u : pm * 256u / tr.inf_at) : 256;
        bpm = 84u + (uint32_t)mu_hp * 36u / 256u + (tr.warp ? (fm1_ms - tr.warp_t) / 250u : 0u);
        if (bpm > 190u) bpm = 190u;
        mu.stp = 44100u * 15u / bpm;
    }
    ta_music = (!tr_music_off || tr.zone == 1u || mu_hp >= 0) && tr.prep >= TRP_READY && (!tr.done || tr_white) && tr_mode != TRM_ENODEN;   /* (in the break the music plays, muted or not; the Enoden has none) */
    if (!tr.done && tr.prep < TRP_READY) {      /* switch on, copy the cab's phrase, reverser forward, close the doors */
        uint32_t ap = tr_autoplay && fm1_ms - tr.prep_t > 800u;
        if (tr.prep == TRP_OFF && (PRESSED(BTN_PLAY) || ap)) {
            tr.prep = TRP_TUNE; tr.prep_t = fm1_ms; ta_thunk_n++; tr.sys_t = fm1_ms; tr.sys_n = 0;
            if (tr_mode != TRM_STORY) { tr.prep = TRP_REV; ta_chime = TA_POWER; ta_chime_n++; }   /* (the Enoden: no phrase) */
            else { tr_phrase_new(); tr_phrase_sing(700); }
#ifdef DINGHY_HOST
            printf("%6u ms  start-up: power on, the cab sings %u notes%c", fm1_ms, tr.ph_len, 10);
#endif
        } else if (tr.prep == TRP_TUNE) {
            if (tr.ph_state == 0u && (int32_t)(fm1_ms - tr.ph_t) > (int32_t)(tr.ph_len * TR_PH_MS)) tr.ph_state = 1;
            if (tr.ph_state == 2u && fm1_ms - tr.ph_t > 700u) tr_phrase_sing(100);
            if (PRESSED(BTN_PLAY) && tr.ph_state != 0u) tr_phrase_sing(100);   /* once more */
            for (k = 0; k < 27u; k++) if (down >> k & 1u) tr_phrase_key(53u + k);
            if (tr_autoplay && tr.ph_state == 1u && fm1_ms - tr.prep_t > 300u) { tr.prep_t = fm1_ms; tr_phrase_key(tr.ph[tr.ph_got]); }
        } else if (tr.prep == TRP_REV && (PRESSED(BTN_SEQ) || ap)) {
            tr.prep = TRP_DOORS; tr.prep_t = fm1_ms; sfx_thunk(0, 40, 90, 128);
        } else if (tr.prep == TRP_DOORS && (PRESSED(BTN_ARP) || ap)) {
            tr.prep = TRP_CLOSING; tr.prep_t = fm1_ms; ta_chime = TA_DOORS; ta_chime_n++;
        } else if (tr.prep == TRP_CLOSING && fm1_ms - tr.prep_t > 2200u) {
            tr.prep = TRP_READY; tr.prep_t = fm1_ms; ta_hiss_n++; ta_thunk_n++;   /* the doors shut: air, a thud */
            if (tr_features & F_SYNTH) { if (tr_sig_seq()) { ta_chime = tr_sig_seq(); ta_chime_n++; } else ta_mel_n++; }   /* the departure melody (yours, once made; not on the Enoden) */
        }
        if (tr.prep == TRP_CLOSING && fm1_ms - tr.prep_t > 1400u && fm1_ms - tr.prep_t < 1450u) ta_hiss_n++;
        tr.last_ms = fm1_ms; tr.acc = 0;
    }
    if (tr.prep > TRP_OFF && tr.prep < TRP_READY && tr.sys_n < 8u && (fm1_ms - tr.sys_t) / 150u > tr.sys_n) {   /* the systems check: a tick a lamp */
        tr.sys_n++;
        sfx_tick(0, 70 + (int32_t)tr.sys_n * 3, 40, (uint8_t)(100u + tr.sys_n * 12u), 0);
    }
    if (tr.prep == TRP_TUNE && down && !tr.done) {   /* the start-up: your answer in the cab's own voice, the very sound it sang */
        static uint8_t echo[4];
        for (k = 26u; k < 27u; k--) if (down >> k & 1u) { echo[0] = (uint8_t)(53u + k); echo[1] = 3; echo[2] = 0; echo[3] = 0; ta_chime = echo; ta_chime_n++; break; }
    }
    /* the keys: the lead, once the cab is under way and the synth is on (the latest key held sounds) */
    if (tr.prep > TRP_TUNE && !tr.done && (tr_features & F_SYNTH)) {
        if (down) { for (k = 26u; k < 27u; k--) if (down >> k & 1u) { tr.lead_k = k; ta_lead_n++; break; } }
        else if (tr.lead_k < 27u && !(keys >> tr.lead_k & 1u)) {
            tr.lead_k = 27u;
            for (k = 26u; k < 27u; k--) if (keys >> k & 1u) { tr.lead_k = k; break; }
        }
        ta_lead_note = tr.lead_k < 27u ? 53u + tr.lead_k : 0u;
    } else ta_lead_note = 0;
    if (!tr.done && tr.prep >= TRP_READY && PRESSED(BTN_PLAY) && !tr_autoplay) {   /* PLAY: pause, and on again */
        tr.paused = !tr.paused; sfx_click(0, tr.paused ? 36 : 48, 128);
    }
    if (tr.paused && !tr.done) {                /* paused: nothing moves, nothing sounds, the clock stands */
        tr.last_ms = fm1_ms; tr.acc = 0;
        ta_music = 0; ta_lead_note = 0; ta_kmh8 = 0; ta_load = 0; ta_horn = 0; ta_whistle = 0; ta_bell = 0; ta_xing = 0;
        led_btn_mask |= BIT(BTN_PLAY); led_btn_blink = BIT(BTN_PLAY);
        return 0;
    }
    if (!tr.done && tr.prep >= TRP_READY && tr.zone == 1u) {   /* in the zone: the line holds the speed for you */
        int32_t vz = tr_kmh(), lz = tr_limit() - 3;
        if (vz < lz - 10) tr.notch = 4;         /* well under: power */
        else if (vz > lz + 3) tr.notch = -2;    /* over: brake a little */
        else if (tr.notch > 0 && vz >= lz) tr.notch = 0;   /* reached it: coast */
        else if (tr.notch < 0 && vz <= lz + 1) tr.notch = 0;
        else if (tr.notch == 0 && vz < lz - 3) tr.notch = 2;   /* fallen back: a little power */
        else if (tr.notch > 2 && vz >= lz - 6) tr.notch = 2;
        e = 0;
    }
    if (!tr.done && tr.prep >= TRP_READY) {
        if (e) tr_set_notch(tr.notch + e);
        if (PRESSED(BTN_OCT_DOWN)) tr_set_notch(tr.notch - 1);
        if (PRESSED(BTN_OCT_UP)) tr_set_notch(tr.notch + 1);
        if (PRESSED(BTN_FX) && (tr_features & F_LIGHTS)) { tr.dim = !tr.dim; sfx_click(0, 40, 128); }
        if (PRESSED(BTN_SEL) && tr.cue == TC_CHECK) { tr.ack_ev = tr.cue_ev + 1u; sfx_tick(0, 84, 60, 128, 0); }
        if (PRESSED(BTN_SEQ) && (tr_features & F_VOICE) && (tr_features & F_SYNTH)) {   /* SEQ: the synth's voice */
            static const char *const VN[LD_NVOICE] = {T_TR_VOICE_SAW, T_TR_VOICE_GLASS, T_TR_VOICE_PWM, T_TR_VOICE_BRASS, T_TR_VOICE_EPIANO, T_TR_VOICE_CHOIR};
            ld_voice = (uint8_t)((ld_voice + 1u) % LD_NVOICE);   /* (round them all) */
            tr.toast = VN[ld_voice]; tr.toast_until = fm1_ms + 1400u; tr.toast_c = C_CYAN;
        }
        tr.horn = (tr_features & F_HORN) ? (in.held >> BTN_REC) & 1u : 0u;
        tr.whistle = (tr_features & F_WHISTLE) ? (in.held >> BTN_ARP) & 1u : 0u;
        if (!tr.appr && tr_rel() < 400000) { tr.appr = 1; ta_chime = TA_NEXT; ta_chime_n++; }   /* the next station */
        tr.acc += (fm1_ms - tr.last_ms) * TR_HZ;
        if (tr.acc > 8000u) tr.acc = 8000u;
        while (tr.acc >= 1000u && !tr.done) { tr.acc -= 1000u; tr_tick(); }
    }
    tr.last_ms = fm1_ms;
    /* the sound's parameters */
    run = !tr.done;
    n = tr.notch; v = tr_kmh(); m = (int32_t)tr_pos_m();
    if (run && n > 0) ld = 80 + n * 35;
    else if (run && n < 0 && n != TR_EB && v > 6) ld = 50 + (-n) * 12;
    if (run && n < 0 && v < 10) ld = ld * v / 10;
    for (i = 0; run && i < tr.L->nxing; i++) {
        int32_t dx = (int32_t)tr.L->xing[i] - m;
        if (dx > -8 && dx < 160) xg = (uint32_t)(5200 - (dx > 0 ? dx : 0) * 28);
    }
    ta_kmh8 = run ? (uint32_t)(tr.v >> 8) : 0u;
    ta_load = (uint32_t)ld;
    ta_brake = run && tr.brk > 40000;
    ta_horn = tr.horn && run;
    ta_whistle = tr.whistle && run;
    ta_bell = run && v > tr_limit() + 1;
    ta_tunnel = tr_zone(m) == TZ_TUNNEL;
    ta_xing = xg;
    /* the lights: the notch on keys 0..14, the speed against the limit on 15..26, the event buttons */
    {
        uint32_t blink = (fm1_ms / 200u) & 1u;
        int32_t lim = tr_limit(), sing = tr.prep == TRP_TUNE ? tr_phrase_now() : -1;
        uint32_t bar = (mu.step >> 4) & 3u;
        for (k = 0; k < 27u; k++) {
            uint32_t nt = 53u + k, j, glow = 0, on = tr.lead_k == k;
            if (tr.prep == TRP_TUNE) {          /* the phrase's keys glow; the one sung lights; after two misses, the next */
                for (j = 0; j < tr.ph_len; j++) if (tr.ph[j] == nt) glow = 1;
                if ((int32_t)nt == sing || (tr.ph_state == 1u && tr.ph_miss >= 2u && tr.ph[tr.ph_got] == nt && blink)) on = 1;
            } else if (tr.prep >= TRP_READY) {  /* the keyboard as the line: speed, the marker, the tunnel, the heart; the chord glowing */
                uint32_t bi, rel_m = tr_rel() > 0 ? (uint32_t)tr_rel() / 1000u : 0u, ph = fm1_ms % 1000u;
                for (j = 0; j < 3u; j++) if (nt % 12u == mu_pad[bar][j] % 12u) glow = 1;
                if (!(tr_features & F_SYNTH)) glow = 0;
                (void)ph;
                if (tr_heart && tr_hs() >= HS_BEAT) {   /* the heart: every key beats, lub-dub, on the music's beat */
                    uint32_t per = 4u * MU_STEP, bp = ((mu.abs & 3u) * MU_STEP + mu.t) * 1000u / (per ? per : 1u);
                    on = on || bp < 120u || (bp > 250u && bp < 350u);
                    glow = 1;
                } else if (tr_heart || tr_zone((int32_t)tr_pos_m()) == TZ_TUNNEL) glow = 0;     /* the tunnel: nearly dark */
                else if (tr.zone == 1u) { if (glow && tr_beat(&bi) < 2600) on = 1; }
                else if (rel_m < 250u && tr_mode != TRM_ENDLESS) {                /* the station: the lights close on the marker */
                    uint32_t c2 = (250u - rel_m) * 13u / 250u;
                    on = on || k < c2 || k > 26u - c2 || (k == 13u && blink);
                } else {                                                          /* under way: the speed, left to right */
                    int32_t full = lim > 0 ? tr_kmh() * 27 / lim : 0;
                    on = on || (int32_t)k < full;
                }
            }
            fm1_led_key(KEY_LED(k), (int)on);
            fm1_led_key_glow(KEY_LED(k), (int)glow);
        }
        (void)lim;
        led_btn_mask |= BIT(BTN_OCT_DOWN) | BIT(BTN_OCT_UP) | BIT(BTN_GLO) | ((tr_features & F_HORN) ? BIT(BTN_REC) : 0u) | ((tr_features & F_WHISTLE) && tr.prep >= TRP_READY ? BIT(BTN_ARP) : 0u) |
                        ((tr_features & F_LIGHTS) ? BIT(BTN_FX) : 0u) | ((tr_features & F_CALL) ? BIT(BTN_SEL) : 0u);
        if (tr.prep < TRP_READY) led_btn_mask |= BIT(BTN_PLAY) | BIT(BTN_SEQ) | BIT(BTN_ARP);
        led_btn_blink = (tr.cue == TC_FANS ? BIT(BTN_REC) : 0u) | (tr.cue == TC_WHISTLE ? BIT(BTN_ARP) : 0u) | (tr.cue == TC_DIM || tr.cue == TC_BRIGHT ? BIT(BTN_FX) : 0u) |
                        (tr.cue == TC_CHECK ? BIT(BTN_SEL) : 0u) |
                        (tr.prep == TRP_OFF || (tr.prep == TRP_TUNE && tr.ph_state == 1u) ? BIT(BTN_PLAY) : tr.prep == TRP_REV ? BIT(BTN_SEQ) : tr.prep == TRP_DOORS ? BIT(BTN_ARP) : 0u);
    }
    if (tr.done && !tr.stopped_ms) tr.stopped_ms = fm1_ms;
    return tr.done && fm1_ms - tr.stopped_ms > 1200u;   /* (a breath at the platform before the finale) */
}

/* ---- the panel under the view: speed, an arc gauge, power and brake bars, limit / stop / time */
static char *tr_num(char *d, uint32_t v, uint32_t w)
{
    char t[12];
    uint32_t n = 0;
    do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (w > n) { *d++ = ' '; w--; }
    while (n) *d++ = t[--n];
    *d = 0;
    return d;
}
/* ---- the cab: a windscreen that fills the screen, and one slim strip of instruments under it */
#define CAB_WX 2                                /* the windscreen, on screen */
#define CAB_WY 2
#define CAB_WW 236
#define CAB_WH 176
#define CAB_MY 180                              /* the strip */
#define C_CABWALL RGB(22, 12, 42)

static void tr_lamp(int32_t x, int32_t y, uint16_t on, int lit)
{
    g_rrect(x, y, 9, 9, 2, RGB(10, 6, 20));
    g_rrect(x + 1, y + 1, 7, 7, 2, lit ? on : mix565(RGB(10, 6, 20), on, 50));
    if (lit) { px_blend(x + 2, y + 2, ut.cream, 160); }
}
static void tr_dead(int32_t x, int32_t y)       /* a socket whose part is not back yet: dark, its glass cracked */
{
    g_rrect(x, y, 9, 9, 2, RGB(10, 6, 20));
    g_rrect(x + 1, y + 1, 7, 7, 2, RGB(28, 26, 30));
    g_line(x + 2, y + 2, x + 6, y + 7, RGB(70, 66, 74)); g_line(x + 6, y + 2, x + 4, y + 5, RGB(70, 66, 74));
}
static uint32_t tr_restored(void)               /* the cab restored: 0..7 (the parts back, the key aside) */
{
    uint32_t f = tr_features & 0xFEu, n = 0;
    while (f) { n += f & 1u; f >>= 1; }
    return n;
}
static void tr_cab_frame(void)
{
    int32_t j;
    uint16_t wall = tr_gone >= 6u ? RGB(0, 0, 0) : tr_trim == 1u ? RGB(64, 36, 24) : tr_trim == 2u ? RGB(72, 12, 26) : C_CABWALL;   /* (the heart: dark; a tram: its own) */
    for (j = 0; j < CAB_MY; j++) {              /* the cab's wall, only in the windscreen's rounded corners */
        int32_t r = j - CAB_WY, k;
        if (r < 0 || r >= CAB_WH) { g_rect(0, j, SW, 1, wall); continue; }
        k = g_rr_inset(r, CAB_WH, 12);
        g_rect(0, j, CAB_WX + k, 1, wall);
        g_rect(CAB_WX + CAB_WW - k, j, SW - (CAB_WX + CAB_WW - k), 1, wall);
    }
    if (tr_gone < 6u) g_rframe(CAB_WX - 1, CAB_WY - 1, CAB_WW + 2, CAB_WH + 2, 13, tr_trim ? 2 : 1, tr_trim == 1u ? C_BRASS : tr_trim == 2u ? C_GOLD : ut.frame);
}

static void tr_strip_lit(void);
static void tr_strip(void)
{
    uint16_t sv[4] = {ut.cyan, ut.rose, ut.cyan_d, ut.rose_d};
    uint32_t rs = tr_mode == TRM_STORY && !tr_heart ? tr_restored() : 7u, keep = 90u + rs * 166u / 7u;
    ut.cyan = mix565(RGB(90, 90, 96), ut.cyan, keep); ut.rose = mix565(RGB(96, 90, 96), ut.rose, keep);   /* (greyed, then the colour back) */
    ut.cyan_d = mix565(RGB(40, 40, 44), ut.cyan_d, keep); ut.rose_d = mix565(RGB(44, 40, 44), ut.rose_d, keep);
    tr_strip_lit();
    ut.cyan = sv[0]; ut.rose = sv[1]; ut.cyan_d = sv[2]; ut.rose_d = sv[3];
}
static void tr_strip_lit(void)
{
    int32_t y0 = CAB_MY, v = tr_kmh(), lim = tr_limit(), top = tr.L->dbl ? 120 : 60, k, nd = 0, nl = tr_next_limit(&nd), d = tr_rel();
    int32_t cx = 84, cy = y0 + 34;
    char b[24], *p;
    g_rect(0, y0, SW, SH - y0, tr_gone >= 6u ? RGB(0, 0, 0) : ut.night);
    if (tr_gone < 6u) { g_rect(0, y0, SW, 1, ut.rose); g_rect(0, y0 + 1, SW, 1, ut.rose_d); }
    /* the speed */
    tr_num(b, (uint32_t)v, 1);
    if (tr_gone < 7u) g_text_r(54, y0 + 4, &FONT_L, b, v > lim + 1 ? ut.rose : ut.cream);
    /* the arc: segments cyan to magenta, lit to the speed; the limit's tail red, the next limit amber */
    for (k = 0; k < (tr_gone >= 5u ? 0 : 12); k++) {
        uint32_t a0 = 32768u + (uint32_t)k * 32768u / 12u + 600u, a1 = 32768u + (uint32_t)(k + 1) * 32768u / 12u - 600u, a;
        int32_t kv = (k + 1) * top / 12;
        uint16_t c = mix565(ut.cyan, ut.rose, (uint32_t)k * 256u / 11u);
        int lit = k * top / 12 < v;
        if (kv > lim + top / 24) c = RGB(255, 60, 60);
        else if (nl && kv > nl) c = C_AMBER;
        for (a = a0; a < a1; a += 400u)
            g_thick16(px16(cx, 26 * 16, a), py16(cy, 26 * 16, a), px16(cx, 18 * 16, a), py16(cy, 18 * 16, a), 2 * 16,
                      lit ? c : mix565(ut.night, c, 60));
    }
    if (tr_gone < 5u) {
        uint32_t a = 32768u + (uint32_t)clampi(lim, 0, top) * 32768u / (uint32_t)top;
        g_thick16(px16(cx, 29 * 16, a), py16(cy, 29 * 16, a), px16(cx, 24 * 16, a), py16(cy, 24 * 16, a), 2 * 16, ut.cream);
    }
    /* the limit: a sign, as at the lineside */
    g_disc(130, y0 + 20, 15, RGB(230, 40, 110));
    g_disc(130, y0 + 20, 12, RGB(250, 245, 255));
    if (lim >= 999) { g_ring(124, y0 + 20, 4, 5, RGB(20, 12, 30)); g_ring(136, y0 + 20, 4, 5, RGB(20, 12, 30)); }   /* no limit: infinity */
    else { tr_num(b, (uint32_t)lim, 1); g_text_c(130, y0 + 12, &FONT_S, b, RGB(20, 12, 30)); }
    /* the next change: an arrow, its limit, how far */
    if (nl && tr_gone < 4u) {
        uint16_t c = nl < lim ? C_AMBER : C_VERD;
        for (k = 0; k < 5; k++) {
            if (nl < lim) g_rect(152 + k, y0 + 9 + k, 9 - 2 * k, 1, c);
            else g_rect(156 - k, y0 + 9 + k, 1 + 2 * k, 1, c);
        }
        tr_num(b, (uint32_t)nl, 1);
        g_text(164, y0 + 4, &FONT_S, b, c);
        p = tr_num(b, (uint32_t)nd, 1);
        str_cat(p, "m");
        g_text(152, y0 + 20, &FONT_S, b, mix565(c, ut.night, 60));
    }
    /* the lamps: dimmed, horn, ATS; the event that wants you (check, fans, oncoming) */
    if (tr_gone < 1u) {   /* the lamps: each lit as the systems check reaches it at power-on; a dead socket where a part is not back */
        uint32_t chk = tr.prep == TRP_OFF ? 0u : tr.prep >= TRP_READY ? 99u : (fm1_ms - tr.sys_t) / 150u;
        if (chk >= 1u) tr_lamp(195, y0 + 6, C_VERD, tr.prep >= TRP_READY || chk < 8u);   /* the cab powered */
        else tr_dead(195, y0 + 6);
        if (!(tr_features & F_LIGHTS) || chk < 2u) tr_dead(206, y0 + 6); else tr_lamp(206, y0 + 6, ut.cyan, (int)tr.dim || chk < 8u);
        if (!(tr_features & F_HORN) || chk < 3u) tr_dead(217, y0 + 6); else tr_lamp(217, y0 + 6, ut.rose, (int)tr.horn || chk < 8u);
        if (chk < 4u) tr_dead(228, y0 + 6); else tr_lamp(228, y0 + 6, RGB(255, 60, 60), (v > lim + 1 && ((fm1_ms / 250u) & 1u)) || chk < 8u);
        if (!(tr_features & F_SYNTH) || chk < 5u) tr_dead(195, y0 + 18); else tr_lamp(195, y0 + 18, ut.rose, !tr_music_off || chk < 8u);
        if (!(tr_features & F_CALL) || chk < 6u) tr_dead(206, y0 + 18); else tr_lamp(206, y0 + 18, C_AMBER, tr.cue == TC_CHECK || chk < 8u);
        if (!(tr_features & (F_HORN | F_WHISTLE)) || chk < 7u) tr_dead(217, y0 + 18); else tr_lamp(217, y0 + 18, C_GOLD, tr.cue == TC_FANS || tr.cue == TC_WHISTLE || chk < 8u);
        if (!(tr_features & F_LIGHTS) || chk < 8u) tr_dead(228, y0 + 18); else tr_lamp(228, y0 + 18, ut.cyan, tr.cue == TC_DIM || tr.cue == TC_BRIGHT);
    }
    /* the notch ladder: EB, B8..B1, N, P1..P5 */
    for (k = TR_EB; k <= (tr_gone >= 2u ? TR_EB - 1 : TR_PMAX); k++) {
        int32_t x = 6 + (k - TR_EB) * 6, on = tr.notch < 0 ? (k >= tr.notch && k <= 0) : (k >= 0 && k <= tr.notch), cur = k == tr.notch;
        uint16_t c = k > 0 ? ut.rose : k == TR_EB ? RGB(255, 60, 60) : k < 0 ? (ut_rail ? RGB(90, 150, 220) : RGB(150, 60, 220)) : ut.cream;
        g_rect(x, y0 + (cur ? 42 : 44), 5, cur ? 14 : 10, on ? c : mix565(ut.night, c, 45));
    }
    /* how far to the stop marker, the run so far, the time left */
    p = b;
    if (tr_mode == TRM_ENDLESS) d = (int32_t)((tr_odo_base + tr_pos_m()) * 1000u);   /* (the endless line: the distance run) */
    if (d < 0) { p = str_cat(p, "+"); d = -d; }
    if (d >= 1000000) { p = tr_num(p, (uint32_t)d / 1000000u, 1); *p++ = '.'; *p++ = (char)('0' + (uint32_t)d / 100000u % 10u); *p++ = (char)('0' + (uint32_t)d / 10000u % 10u); str_cat(p, "k"); }
    else if (d < 100000) { p = tr_num(p, (uint32_t)d / 1000u, 1); *p++ = '.'; *p++ = (char)('0' + (uint32_t)d / 100u % 10u); str_cat(p, "m"); }
    else { p = tr_num(p, (uint32_t)d / 1000u, 1); str_cat(p, "m"); }
    if (tr_gone < 3u) g_text(104, y0 + 42, &FONT_S, b, tr_rel() < 0 ? ut.rose : tr_rel() < 50000 ? ut.cyan : ut.cream);
    {
        int32_t x0 = 156, w = 36, done = clampi((int32_t)(((uint32_t)tr_pos_mm() / 100u) * (uint32_t)w / ((uint32_t)tr.L->dist * 10u + 1u)), 0, w);
        if (tr_mode != TRM_ENDLESS && tr_gone < 3u) { g_rect(x0, y0 + 50, w, 2, ut.panel2); g_rect(x0, y0 + 50, done, 2, ut.cyan); }
    }
    if (tr_features & F_STOPWATCH) {            /* the time left: only with the stopwatch */
        uint32_t s = (uint32_t)(tr.time_left < 0 ? -tr.time_left : tr.time_left + TR_HZ - 1) / TR_HZ;
        p = str_cat(b, tr.time_left < 0 ? "+" : "");
        p = tr_num(p, s / 60u, 1); *p++ = ':'; *p++ = (char)('0' + s % 60u / 10u); *p++ = (char)('0' + s % 10u); *p = 0;
        g_text_r(236, y0 + 42, &FONT_S, b, tr.time_left < 0 ? ut.rose : ut.ink2);
    }
}
static void tr_panel(void)
{
    int32_t hs = tr_hs();
    tr_gone = hs >= HS_FADE && !tr.done ? (uint32_t)clampi((hs - HS_FADE) * 8 / (256 - HS_FADE), 0, 7) : 0u;
    tr_cab_frame();
    tr_strip();
}

static uint16_t tv_cache[TV_W * TV_H] __attribute__((section(".pool")));
static uint32_t tv_frame;

/* ---- a ride (the trams of the rhythm game): the same cab view, along the ride's own line, carried by the song */
static void tr_ride_begin(uint32_t leg, uint32_t trim)
{
    tv_init();
    tr_mode = TRM_STORY; tr_force = 0; tr_names = TR_STATION;
    tr.L = &TR_LEGS[leg < NTRLEGS ? leg : 0];
    tp = &TR_PAL[tr.L->sky < 6u ? tr.L->sky : 1u];
    tr_heart = 0; tr_white = 0; mu_hp = -1; tr_rain = 0; tr_gone = 0; tr_trim = (uint8_t)trim;
    tr.pos = 0; tr.rem = 0; tr.v = 0; tr.sky_x = 0; tr.done = 0; tr.prep = TRP_READY; tr.zone = 0; tr.t = 0;
    tr.on_live = 0; tr.cue = TC_NONE; tr.dim = 0; tr.warp = 0; tr.inf_at = 0; tr.paused = 0;
    tv_cached = 0;
    ui_theme(0);
}
static void tr_ride_move(uint32_t num, uint32_t den, int32_t kmh)   /* the song this far: the tram this far along, to its platform */
{
    uint32_t span = (uint32_t)tr.L->dist * 1000u - 12000u, f, mm;
    if (num > den) num = den;
    f = den ? (num >> 8) * 65536u / ((den >> 8) + 1u) : 0u;          /* (no 64-bit division: Q16 of the way) */
    mm = (span >> 8) * f >> 8;
    tr.pos = mm * 65u + mm * 536u / 1000u;      /* (mm to m << 16) */
    tr.v = num < den ? kmh << 16 : 0;
    tr.sky_x += tr_seg(tr.L->cur, tr.L->ncur, (int32_t)tr_pos_m()) * kmh / 4;
}
static void tr_ride_view(void)                  /* the view (every other frame, as the drive's), the cab around it */
{
    if (!tv_cached || !(tv_frame++ & 1u)) {
        tr_view();
        memcpy(tv_cache, cv_px + TV_Y * TV_W, sizeof tv_cache);
        tv_cached = 1;
    } else memcpy(cv_px + TV_Y * TV_W, tv_cache, sizeof tv_cache);
    tr_cab_frame();
    if (tr_trim == 2u) {                        /* the festival tram: a garland of lanterns across the top of the glass */
        int32_t x;
        for (x = 14; x < SW - 10; x += 22) {
            int32_t sag = 3 + ((x * 7) % 5);
            g_line(x, CAB_WY + 1, x + 11, CAB_WY + 1 + sag, RGB(90, 60, 40)); g_line(x + 11, CAB_WY + 1 + sag, x + 22, CAB_WY + 1, RGB(90, 60, 40));
            g_rrect(x + 8, CAB_WY + sag, 7, 9, 3, (x / 22) & 1 ? RGB(255, 150, 50) : RGB(240, 70, 80));
        }
    }
}
static void tr_ride_end(void) { tr_trim = 0; tv_cached = 0; ui_theme(0); }
static void tr_tag(int32_t y, const felucca_font_t *f, const char *t, uint16_t c)   /* words over the view, on a dark band */
{
    int32_t w = text_w(f, t);
    g_rect_a(SW / 2 - w / 2 - 6, y - 1, w + 12, (int32_t)f->h + 2, ut.night, 180);
    g_text_c(SW / 2, y, f, t, c);
}
static void tr_draw(uint32_t won)
{
    int32_t d = tr_rel();
    char b[40], *p;
    if (!tr.L) return;
    if (!tv_cached || !(tv_frame++ & 1u)) {     /* the view every other frame (half the drawing); in between, its copy */
        tr_view();
        memcpy(tv_cache, cv_px + TV_Y * TV_W, sizeof tv_cache);
        tv_cached = 1;
    } else memcpy(cv_px + TV_Y * TV_W, tv_cache, sizeof tv_cache);
    /* overlays on the view: only what asks for an action */
    if (!won && !tr.done) {
        if (tr.prep < TRP_READY) {                     /* the start-up: what to press next */
            const char *t = tr.prep == TRP_OFF ? T_TR_PREP_POWER : tr.prep == TRP_TUNE ? (tr.ph_state == 0u ? T_TR_PREP_LISTEN : tr.ph_state == 2u ? T_TR_PREP_AGAIN : T_TR_PREP_COPY)
                          : tr.prep == TRP_REV ? T_TR_PREP_REVERSER : tr.prep == TRP_DOORS ? T_TR_PREP_DOORS : T_TR_PREP_CLOSING;
            int32_t tw = text_w(&FONT_S, t);
            g_rrect(SW / 2 - tw / 2 - 8, CAB_WY + 30, tw + 16, 18, 5, ut.panel);
            g_rframe(SW / 2 - tw / 2 - 8, CAB_WY + 30, tw + 16, 18, 5, 1, ut.cyan);
            if ((fm1_ms / 350u) & 1u || tr.prep == TRP_CLOSING || tr.prep == TRP_TUNE) g_text_c(SW / 2, CAB_WY + 31, &FONT_S, t, ut.cyan);
            if (tr.prep == TRP_TUNE) {          /* the phrase: a dot a note, lit as you copy it */
                uint32_t j;
                int32_t x0 = SW / 2 - (int32_t)tr.ph_len * 7, sn = tr_phrase_now();
                for (j = 0; j < tr.ph_len; j++) {
                    uint16_t c = j < tr.ph_got && tr.ph_state == 1u ? ut.cyan : sn >= 0 && (fm1_ms - tr.ph_t) / TR_PH_MS == j ? ut.rose : ut.panel2;
                    g_disc(x0 + (int32_t)j * 14 + 7, CAB_WY + 58, 4, c);
                }
            }
        }
        if (tr_rain && (tr.prep < TRP_READY || tr.t < 6u * TR_HZ)) {   /* rain: the warning */
            int32_t tw = text_w(&FONT_S, T_TR_RAIN);
            g_rrect(SW / 2 - tw / 2 - 8, CAB_WY + 62, tw + 16, 18, 5, ut.panel);
            g_rframe(SW / 2 - tw / 2 - 8, CAB_WY + 62, tw + 16, 18, 5, 1, C_AMBER);
            g_text_c(SW / 2, CAB_WY + 63, &FONT_S, T_TR_RAIN, C_AMBER);
        }
        if (tr.prep >= TRP_READY && tr.t < 5u * TR_HZ && tr_mode != TRM_ENDLESS && tr.notch <= 0 && !tr.v) {   /* ready: how to go */
            int32_t tw = text_w(&FONT_S, T_TR_GO);
            g_rrect(SW / 2 - tw / 2 - 8, CAB_WY + 30, tw + 16, 18, 5, ut.panel);
            g_rframe(SW / 2 - tw / 2 - 8, CAB_WY + 30, tw + 16, 18, 5, 1, ut.cyan);
            if ((fm1_ms / 350u) & 1u) g_text_c(SW / 2, CAB_WY + 31, &FONT_S, T_TR_GO, ut.cyan);
        }
        if (tr.mtoast_t && fm1_ms - tr.mtoast_t < 1200u) {
            const char *t = tr_music_off ? T_TR_MUSIC_OFF : T_TR_MUSIC_ON;
            g_rrect(SW - 16 - text_w(&FONT_S, t), CAB_WY + 8, text_w(&FONT_S, t) + 8, 18, 5, ut.panel);
            g_text_r(SW - 12, CAB_WY + 9, &FONT_S, t, ut.rose);
        }
        if (tr.t < 4u * TR_HZ && tr.prep >= TRP_READY && tr_mode != TRM_ENDLESS) {   /* setting off: the line and where to (the endless line: nowhere) */
            int32_t tw;
            p = str_cat(b, "> ");
            str_cat(p, tr_names[tr.L->st_to]);
            tw = text_w(&FONT_S, b);
            g_rrect(10, CAB_WY + 8, tw + 12, 18, 5, ut.panel);
            g_text(16, CAB_WY + 9, &FONT_S, b, ut.cream);
        }
        if (tr.cue && tr.prep >= TRP_READY && !(tr.notch == TR_EB && tr.v) && tr.zone != 1u) {
            const char *t = tr.cue == TC_DIM ? T_TR_CUE_DIM : tr.cue == TC_BRIGHT ? T_TR_CUE_BRIGHT : tr.cue == TC_FANS ? TR_HORN_WHY[tr.cue_ev % 4u] : tr.cue == TC_WHISTLE ? T_TR_CUE_WHISTLE : 0;
            int32_t tw;
            if (!t) { p = str_cat(b, T_TR_CUE_CHECK); p = str_cat(p, " "); p = tr_num(p, tr.cue_arg, 1); str_cat(p, " [SEL]"); t = b; }
            tw = text_w(&FONT_S, t);
            g_rrect(SW / 2 - tw / 2 - 8, CAB_WY + 30, tw + 16, 18, 5, ut.panel);
            g_rframe(SW / 2 - tw / 2 - 8, CAB_WY + 30, tw + 16, 18, 5, 1, tr.cue == TC_BRIGHT ? ut.cyan_d : ut.rose);
            if ((fm1_ms / 300u) & 1u || tr.cue == TC_BRIGHT) g_text_c(SW / 2, CAB_WY + 31, &FONT_S, t, tr.cue == TC_BRIGHT ? ut.cyan : C_AMBER);
        }
        if (d < 120000 && d > -40000) {         /* the stop gauge */
            int32_t y = CAB_WY + CAB_WH - 13, dm = d / 100, xm = 200, xt, u = dm > 100 ? 60 + (dm - 100) * 130 / 900 : dm * 6 / 10;
            g_rrect(14, y - 1, 212, 10, 4, ut.panel);
            g_rect(xm - 60, y + 7, 61, 1, ut.cyan);
            g_rect(xm - 1, y - 4, 3, 16, C_GOLD);
            xt = clampi(xm - u, 18, 222);
            g_rect(xt - 3, y + 1, 6, 6, ut.cream);
        }
    }
    if (tr.zone == 1u && !tr.done) {            /* the synth break */
        uint32_t bi, j;
        int32_t bd = tr_beat(&bi), pulse = bd < 6000 ? (6000 - bd) * 90 / 6000 : 0, el = (int32_t)(fm1_ms - tr.zone_t);
        uint16_t vc = mix565(ut.rose_d, ut.rose, (uint32_t)pulse * 2u);
        g_rect_a(CAB_WX, CAB_WY, CAB_WW, 10, vc, 60u + (uint32_t)pulse);
        g_rect_a(CAB_WX, CAB_WY + CAB_WH - 10, CAB_WW, 10, vc, 60u + (uint32_t)pulse);
        g_rect_a(CAB_WX, CAB_WY + 10, 10, CAB_WH - 20, vc, 60u + (uint32_t)pulse);
        g_rect_a(CAB_WX + CAB_WW - 10, CAB_WY + 10, 10, CAB_WH - 20, vc, 60u + (uint32_t)pulse);
        {   /* the pulse over the horizon: a ring spreading from each beat */
            uint32_t per = 4u * MU_STEP, a2, t2, ph;
            do { a2 = mu.abs; t2 = mu.t; } while (a2 != mu.abs);
            ph = (a2 & 3u) * MU_STEP + t2;
            g_ring(SW / 2, CAB_WY + 52, 4 + (int32_t)(ph * 26u / per), 5 + (int32_t)(ph * 26u / per), mix565(ut.cyan, ut.night, ph * 256u / per));
        }
        for (j = 0; j < 5u; j++)                /* five lights: the beats caught */
            g_disc(SW / 2 - 28 + (int32_t)j * 14, CAB_WY + 22, 4, j < tr.zone_bp ? (fm1_ms - tr.zone_hit_t < 200u && j + 1u == tr.zone_bp ? ut.cream : ut.cyan) : ut.panel2);
        if (el < 3500) {                       /* in: what it is, what to do (each on a dark band: readable on a day sky) */
            tr_tag(CAB_WY + 72, &FONT_L, T_TR_ZONE_IN, ut.rose);
            tr_tag(CAB_WY + 98, &FONT_S, T_TR_ZONE_HINT, ut.cream);
            tr_tag(CAB_WY + 113, &FONT_S, T_TR_ZONE_HINT2, ut.cream);
            tr_tag(CAB_WY + 134, &FONT_S, T_TR_ZONE_CRUISE, ut.cyan);
        } else tr_tag(CAB_WY + 32, &FONT_S, T_TR_ZONE_TAG, ut.cream);
    }
    if (tr.zone == 2u && fm1_ms - tr.zone_t < 2600u && !tr.done) tr_tag(CAB_WY + 108, &FONT_S, T_TR_ZONE_BACK, ut.cyan);
    if (tr.toast && (int32_t)(tr.toast_until - fm1_ms) > 0 && tr.zone != 1u) {   /* (nothing over the zone; the large letters only when they fit) */
        const felucca_font_t *tf = tr_mode != TRM_ENDLESS && text_w(&FONT_L, tr.toast) <= 150 ? &FONT_L : &FONT_S;   /* (the endless line's songs and faders: small, always) */
        int32_t tw = text_w(tf, tr.toast);
        if (tf == &FONT_S) g_rrect(SW / 2 - tw / 2 - 8, CAB_WY + 72, tw + 16, 18, 5, ut.panel);
        g_text_c(SW / 2, CAB_WY + (tf == &FONT_S ? 73 : 70), tf, tr.toast, tr.toast_c);
    }
    if (tr.paused && !tr.done) {                /* paused */
        g_rect_a(CAB_WX, CAB_WY, CAB_WW, CAB_WH, ut.night, 150);
        g_text_c(SW / 2, CAB_WY + 66, &FONT_L, T_TR_PAUSED, ut.cream);
        g_text_c(SW / 2, CAB_WY + 94, &FONT_S, T_TR_RESUME, ut.cyan);
    }
    if (tr.inf_at && tr_pos_m() >= tr.inf_at && !tr_white) {   /* no limit: say so; then the light grows with the speed */
        uint32_t wel = tr.warp ? fm1_ms - tr.warp_t : 0u;   /* (flat out: the ball of light grows from 6 s, the white comes in from 14 s to 24 s) */
        int32_t w = wel > 14000u ? (int32_t)((wel - 14000u) * 64u / 2500u) : 0, r = wel > 6000u ? (int32_t)((wel - 6000u) * 150u / 18000u) : 0;
        if (w > 256) w = 256;
        if ((!tr.warp || wel < 6000u) && (fm1_ms / 300u) & 1u) {   /* (flashes on 6 s into flat out) */
            int32_t tw = text_w(&FONT_S, T_TR_INF_CUE);
            g_rrect(SW / 2 - tw / 2 - 8, CAB_WY + 30, tw + 16, 18, 5, ut.panel);
            g_text_c(SW / 2, CAB_WY + 31, &FONT_S, T_TR_INF_CUE, ut.rose);
        }
        if (r > 0) {
            g_disc(SW / 2, TV_HY, r + 4, RGB(255, 200, 210));
            g_disc(SW / 2, TV_HY, r, RGB(255, 255, 255));
        }
        if (w > 0) g_rect_a(0, 0, SW, SH, RGB(255, 255, 255), (uint32_t)(w * w / 256));
    }
    if (tr.done && tr_white) {                  /* gone into the light */
        g_rect(0, 0, SW, SH, RGB(255, 255, 255));
        return;
    }
    if (tr.done && tr_heart) {                  /* the heart: no points, no tier; only where you are */
        uint32_t el = fm1_ms - tr.stopped_ms;
        g_rect_a(CAB_WX, CAB_WY, CAB_WW, CAB_WH, RGB(20, 0, 6), 120u + (el > 1500u ? 100u : el * 100u / 1500u));
        g_deco_c(SW / 2 + 1, 60, 30, T_HEART_ARRIVE, RGB(90, 0, 20));
        g_deco_c(SW / 2, 58, 30, T_HEART_ARRIVE, RGB(255, 60, 80));
        g_text_c(SW / 2, 110, &FONT_S, T_HEART_SUB, RGB(200, 120, 130));
    } else
    if (tr.done) {                              /* the scoreboard: where the points came from, the total, the tier */
        static const char *lab[6];
        int32_t pv[6], n = 0, i, y;
        const char *det[6];
        char db[6][16], *p2;
        uint32_t a = (uint32_t)(tr.r_cm < 0 ? -tr.r_cm : tr.r_cm);
        lab[n] = T_TR_SB_STOP; pv[n] = tr.r_stop;
        p2 = db[n]; *p2++ = tr.r_cm > 0 ? '+' : '-';
        p2 = tr_num(p2, a / 100u, 1); *p2++ = '.'; *p2++ = (char)('0' + a % 100u / 10u); *p2++ = (char)('0' + a % 10u); str_cat(p2, "m");
        det[n++] = tr.r_grade;
        if (tr_features & F_STOPWATCH) { lab[n] = T_TR_SB_TIME; pv[n] = tr.r_time; det[n++] = tr.time_left < 0 ? T_TR_LATE : tr.r_time == 5 ? T_TR_ON_TIME : T_TR_EARLY; }
        if (tr.ev_max) {
            lab[n] = T_TR_SB_EVENTS; pv[n] = tr.ev_pts;
            p2 = db[n]; p2 = tr_num(p2, (uint32_t)tr.ev_max, 1); str_cat(p2, tr.ev_max == 1 ? " CUE" : " CUES");
            det[n] = db[n]; n++;
        }
        if (tr.L->nbrk && (tr_features & F_SYNTH)) { lab[n] = T_TR_SB_ZONE; pv[n] = (int32_t)tr.zone_pts; p2 = db[n]; p2 = tr_num(p2, tr.zone_pts, 1); str_cat(p2, " BEATS"); det[n] = db[n]; n++; }
        if (tr.pen_speed) { lab[n] = T_TR_SB_SPEED; pv[n] = -(int32_t)tr.pen_speed; det[n++] = ""; }
        if (tr.pen_eb) { lab[n] = T_TR_SB_EB; pv[n] = -(int32_t)tr.pen_eb; det[n++] = ""; }
        g_rect_a(CAB_WX, CAB_WY, CAB_WW, CAB_WH, ut.night, 180);
        g_cab_panel(CAB_WX + 8, 6, CAB_WW - 16, 170, ut.rose);
        {   /* the station, and how far from the marker */
            char hb[40], *q = str_cat(hb, tr_names[tr.L->st_to]);
            q = str_cat(q, "  "); str_cat(q, db[0]);
            g_text_c(SW / 2, 10, &FONT_S, hb, ut.cream);
        }
        y = 28;
        for (i = 0; i < n; i++, y += 15) {
            char pb[8];
            p2 = pb; *p2++ = pv[i] < 0 ? '-' : '+'; tr_num(p2, (uint32_t)(pv[i] < 0 ? -pv[i] : pv[i]), 1);
            g_text(18, y, &FONT_S, lab[i], ut.ink2);
            g_text(96, y, &FONT_S, det[i], i == 0 ? (tr.r_stop > 0 ? ut.cyan : tr.r_stop < 0 ? ut.rose : ut.cream) : ut.cream);
            g_text_r(SW - 18, y, &FONT_S, pb, pv[i] > 0 ? ut.cyan : pv[i] < 0 ? ut.rose : ut.ink2);
        }
        g_rect(18, y + 1, SW - 36, 1, ut.rose_d);
        {
            char tb[16];
            p2 = tb; if (tr.r_pts < 0) *p2++ = '-';
            p2 = tr_num(p2, (uint32_t)(tr.r_pts < 0 ? -tr.r_pts : tr.r_pts), 1); *p2++ = '/'; tr_num(p2, (uint32_t)tr.r_mx, 1);
            g_text(18, y + 4, &FONT_S, T_TR_SB_TOTAL, ut.cream);
            g_text_r(SW - 18, y + 4, &FONT_S, tb, ut.cream);
        }
        y += 24;
        {   /* the tier, big, and the best before it */
            char t2[2] = {TR_TIER[tr.tier], 0};
            uint16_t tc = tr.tier >= 7 ? C_GOLD : tr.tier >= 5 ? ut.cyan : tr.tier >= 3 ? ut.cream : ut.rose;
            if (y > 140) y = 140;
            g_rrect(SW / 2 - 16, y - 2, 32, 32, 6, ut.panel2);
            g_text_c(SW / 2, y, &FONT_L, t2, tc);
            if (tr_best_tier) {
                char bb[8] = "BEST ";
                bb[5] = TR_TIER[tr_best_tier]; bb[6] = 0;
                g_text(SW / 2 + 26, y + 8, &FONT_S, bb, tr.tier > tr_best_tier ? C_GOLD : ut.ink2);
            }
            if (tr.tier > tr_best_tier && tr_best_tier) g_text_r(SW / 2 - 26, y + 8, &FONT_S, "NEW", C_GOLD);
        }
    }
    if (!tr_white) tr_panel();
}

static void tr_leave(void)
{
    uint32_t k;
    kb_hook = 0;
    if (tr_white) { mu_fade = -110; ta_lead_note = 0; ta_kmh8 = 0; ta_load = 0; ta_brake = 0; ta_tunnel = 0; }   /* into the light: the song goes on, fading away under the heart's pages (~3.5 s) */
    else { au_hook = 0; ta_music = 0; mu.lvl = 0; mu.kick_env = 0; ta_lead_note = 0; }
    ta_kmh8 = 0; ta_load = 0; ta_horn = 0; ta_whistle = 0; ta_bell = 0; ta_xing = 0;
    for (k = 0; k < 27u; k++) { fm1_led_key(KEY_LED(k), 0); fm1_led_key_glow(KEY_LED(k), 0); }
}

static void tr_cheat(void)                      /* SAVE held: arrive, stopped at the marker */
{
    tr.pos = ((uint32_t)tr.L->dist << 16);
    tr.v = 0;
    tr.notch = -8;
    tr.t = TR_HZ + 1u;
    tr_evaluate();
    tr.stopped_ms = fm1_ms - 2000u;
}

static const game_t G_TRAVEL = {BIT(BTN_EDIT), 1400, tr_enter, tr_update, tr_draw, tr_leave};
