/* SPDX-License-Identifier: GPL-3.0-only */
/* THE RHYTHM: play over a song of the line, in one of three looks (the rides: from the cab, as a drive).
 *
 * Six lanes, six keys: middle C, D, E, F, G, A (they glow). The notes coming down the lanes reach the line on the
 * beat: hit each as it arrives. On the beat: PERFECT (5), near it: GOOD (4); a long note held to its end, +1; a note
 * let past: MISS. Reach the share asked by the song's end to pass; short of it, the song starts again (new notes).
 *
 *   style 0  the synthwave rails: the lead synth plays your notes (a chapter of its own)
 *   style 1  THE SEASIDE TRAM: a vintage electric tram along the beach. Melodic: the notes are the tram's bells,
 *            C D E, one on each beat (struck at once, no held notes)
 *   style 2  THE FESTIVAL TRAM: lanterns and sakura at night. Faster: three drums, C the big taiko, D the small
 *            drum, E the rim, all on the beat; every note hit lights its lantern along the line
 *
 * The two rides are lines on the map (lines.txt: ride = tram / festival), not chapters: a ride is driven once
 * it is passed, and its tier is the score's (S at 95%). A ride looks like a drive: the cab's own view along its
 * line (its sky, its scenery, its platform at the end, reached as the song ends), the notes coming down over the
 * windscreen, the pads and the score in the cab's strip below; the tram's cab its own (varnish and brass; red
 * lacquer and a garland of lanterns). */

#define RH_MAXN 400
#define RH_LANES 6
#define RH_CUE RH_LANES                         /* the tram's trackside cues: a seventh lane, REC */
#define RH_COUNT 16u                            /* a bar counted in: four ticks, the backing silent */
#define RH_LEAD 32u                             /* the count, then a bar of the backing, before the first note */
/* frames either side of the note. The rails: 70 ms PERFECT, 130 ms GOOD. A ride: shares of its own beat, so they
 * stay inside half a beat at any tempo (a press counts for one note only): PERFECT a quarter beat, GOOD 0.4 of a
 * beat, OK just under half (the seaside tram 0.13 / 0.21 / 0.25 s, the festival 0.10 / 0.17 / 0.20 s) */
#define RH_PERFECT (rh.style ? (int32_t)MU_STEP : 3087)
#define RH_GOOD (rh.style ? (int32_t)(MU_STEP * 8u / 5u) : 5733)
#define RH_OKW (rh.style ? (int32_t)(MU_STEP * 19u / 10u) : 5733)
#define RH_HY 62                                /* the lanes' vanishing line */
#define RH_HIT 196                              /* the hit line */
static const uint8_t RH_NOTE[RH_LANES] = {60, 62, 64, 65, 67, 69};   /* C4 D4 E4 F4 G4 A4 */
static const uint8_t RH_TRAM_NOTE[3] = {60, 62, 64};              /* the tram's bells: C D E, side by side */
static const uint8_t RH_FEST_NOTE[3] = {60, 62, 64};               /* the festival's drums: C D E */
static const uint8_t *rh_nt = RH_NOTE;
static char rh_name(uint32_t l) { static const char PCN[12] = {'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B'}; return PCN[rh_nt[l] % 12u]; }
static const uint16_t RH_COL[RH_LANES] = {C_CYAN, C_ROSE, C_AMBER, C_VERD, C_CYAN, C_ROSE};
static const uint16_t RH_TRAM_COL[RH_LANES] = {RGB(255, 214, 120), RGB(250, 190, 150), RGB(255, 236, 170), RGB(170, 230, 220), RGB(255, 214, 120), RGB(250, 190, 150)};
static const uint16_t RH_FEST_COL[RH_LANES] = {RGB(230, 40, 50), RGB(255, 120, 60), RGB(240, 220, 190), C_GOLD, C_GOLD, C_GOLD};

static struct {
    uint16_t step[RH_MAXN], len[RH_MAXN];       /* the note's sixteenth from the song's start; its hold (0: a tap) */
    uint8_t lane[RH_MAXN], res[RH_MAXN], held[RH_MAXN];   /* res 0 coming, 1 good, 2 perfect, 3 missed; held 1 holding 2 kept 3 let go */
    uint32_t n, next, bars, density, pass, end_step, holds, style, song, nl;
    uint32_t pts, combo, best_combo, hits;
    uint32_t judge_t, judge, judge_lane, press_t[RH_LANES + 1], hold_i[RH_LANES];
    uint32_t state, state_t, won, lead_lane, tier;
    uint8_t voice_was, started, paused;
} rh;

static uint32_t rh_pos(void)                    /* frames since the song began (from the backing's own clock) */
{
    uint32_t a, t;
    do { a = mu.abs; t = mu.t; } while (a != mu.abs);
    return a ? (a - 1u) * MU_STEP + t : 0u;     /* (the song's first sixteenth is abs 1: counted from it, the notes land on the backing's own beat) */
}
static uint32_t rh_clk_abs, rh_clk_ph;
static int32_t rh_clk_env, rh_clk_hi;
static void rh_audio(int32_t *out, uint32_t n)
{
    uint32_t i;
    tr_music_block(70u);                        /* the backing, every layer */
    tr_lead_block();                            /* the lead: your keys */
    for (i = 0; i < n; i++) {
        int32_t m = tr_music_sample(), s;
        if (mu.abs <= RH_COUNT) {               /* the count-in: the backing silent, a tick each beat (the first higher); abs 1 is the first sixteenth */
            m = 0;
            if (mu.abs != rh_clk_abs && mu.abs && !((mu.abs - 1u) & 3u)) { rh_clk_env = 14000; rh_clk_hi = mu.abs == 1u; rh_clk_ph = 0; }
        }
        rh_clk_abs = mu.abs;
        if (rh_clk_env) {
            rh_clk_ph += rh_clk_hi ? TA_HZ(1760u) : TA_HZ(1320u);
            m += (ta_sine(rh_clk_ph) * rh_clk_env) >> 15;
            rh_clk_env -= (rh_clk_env >> 9) + 1; if (rh_clk_env < 0) rh_clk_env = 0;
        }
        s = m + tr_lead_sample(i);
        out[2u * i] += s;
        out[2u * i + 1u] += s;
    }
}

static int rh_tone(uint32_t lane, uint32_t bar)     /* a chord tone of this bar? */
{
    uint32_t k;
    for (k = 0; k < 3u; k++) if (rh_nt[lane] % 12u == mu_pad[bar][k] % 12u) return 1;
    return 0;
}
static void rh_add(uint32_t step, uint32_t lane)
{
    if (rh.n >= RH_MAXN) return;
    rh.step[rh.n] = (uint16_t)step; rh.lane[rh.n] = (uint8_t)lane;
    rh.len[rh.n] = 0; rh.res[rh.n] = 0; rh.held[rh.n] = 0; rh.n++;
}
static void rh_chart(void)
{
    uint32_t b, q, i, lane = 2, total = rh.bars * 16u;
    rh.n = 0; rh.holds = 0;
    for (b = 0; b < rh.bars; b++) {
        uint32_t bar = ((RH_LEAD >> 4) + b) & 3u, base = RH_LEAD + b * 16u;
        if (rh.style == 2u) {                   /* the festival: a drum part. Big drums on the beat, small between, bells on top */
            for (q = 0; q < 16u && rh.n < RH_MAXN; q += 4u) {   /* every drum on a beat, never between: the big on one, the small on three, the rim on some twos and fours */
                if (q != 0u && q != 8u && rndn(100) >= 55u) continue;
                rh_add(base + q, q == 0u ? 0u : q == 8u ? 1u : 2u);
            }
            continue;
        }
        for (q = 0; q < 8u && rh.n < RH_MAXN; q++) {   /* eighths: the beats first, offbeats only when dense */
            uint32_t pr, best = lane, bd = 99, l;
            int strong = !(q & 1u);
            pr = rh.style == 1u ? (q == 0u ? 100u : q == 4u ? 90u : strong ? 55u : 0u)   /* (the tram: a bell on the beat, two to four a bar) */
               : rh.density <= 1u ? (q == 0u || q == 4u ? 100u : 0u)
               : rh.density == 2u ? (strong ? (q == 0u ? 100u : q == 4u ? 85u : 55u) : 0u)
               : (strong ? (q == 0u ? 100u : 80u) : 30u);
            if (rndn(100) >= pr) continue;
            for (l = 0; l < rh.nl; l++) {    /* the nearest chord tone (on a strong beat), else the nearest step */
                uint32_t d = l > lane ? l - lane : lane - l;
                if (strong && !rh_tone(l, bar)) continue;
                if (d == 0u && rndn(3)) d = 3;  /* (not too often the same note) */
                if (d < bd || (d == bd && rndn(2))) { bd = d; best = l; }
            }
            if (!strong) best = (uint32_t)clampi((int32_t)lane + (rndn(2) ? 1 : -1), 0, rh.nl - 1);
            lane = best;
            rh_add(base + q * 2u, lane);
        }
    }
    if (rh.style == 0u)                         /* (the rides: taps only) */
        for (i = 0; i < rh.n; i++) {            /* some notes held: to the next note, less a breath */
            uint32_t gap, j;
            if (rh.lane[i] == RH_CUE) continue;
            for (j = i + 1u; j < rh.n && rh.lane[j] == RH_CUE; j++) {}
            gap = (j < rh.n ? rh.step[j] : RH_LEAD + total) - rh.step[i];
            if (gap >= 6u && rndn(100) < (rh.style == 1u ? 20u : rh.density <= 1u ? 55u : 35u)) { rh.len[i] = (uint16_t)(gap - 2u); rh.holds++; }
        }
    if (rh.style == 2u) {                       /* the festival's end: a roll, five strong beats, the big drum and the rim at once */
        for (q = 0; q < 5u; q++) { rh_add(RH_LEAD + total + q * 4u, 0u); rh_add(RH_LEAD + total + q * 4u, 2u); }
        total += 20u;
    }
    rh.end_step = RH_LEAD + total + 8u;
}
static void rh_start(void)                      /* the song from the top: the backing's clock to zero */
{
    uint32_t i;
    au_hook = 0;
    mu.t = 0; mu.step = 63u; mu.abs = 0; mu.lvl = 32767; mu.kick_env = 0; mu.len = 0; rh_clk_abs = 99u; rh_clk_env = 0;
    rh_chart();
    rh.next = 0; rh.pts = 0; rh.combo = 0; rh.hits = 0; rh.judge_t = 0; rh.state = 0; rh.lead_lane = rh.nl;
    for (i = 0; i < rh.nl; i++) rh.hold_i[i] = RH_MAXN;
    for (i = 0; i < LD_ECHO; i++) ld_echo[i] = 0;
    ta_lead_note = 0;
    ta_music = 1;
    au_hook = rh_audio;
}

static uint32_t rh_lane_of(uint32_t k)
{
    uint32_t l;
    for (l = 0; l < rh.nl; l++) if (k + 53u == rh_nt[l]) return l;
    return rh.nl;
}
static void rh_drum(uint32_t lane)              /* the festival's drums: taiko, the small drums, the bells */
{
    au_ev_t e = {0};
    e.at = au_time(); e.tag = 11; e.pan = (uint8_t)(70u + lane * 23u);
    if (lane == 0u) { e.inc1 = note_inc(36); e.inc2 = note_inc(48); e.a1 = 15000; e.a2 = 3000; e.an = 3500; e.d1 = 280; e.d2 = 90; e.dn = 25; e.slide = -900; e.lp = 4000; }   /* the big drum */
    else if (lane == 1u) { e.inc1 = note_inc(45); e.inc2 = note_inc(57); e.a1 = 11000; e.a2 = 2600; e.an = 4000; e.d1 = 160; e.d2 = 70; e.dn = 25; e.slide = -700; e.lp = 6000; }   /* the small drum */
    else if (lane == 2u) { e.inc1 = note_inc(60); e.inc2 = inc_ratio(e.inc1, 400); e.a1 = 6000; e.a2 = 2400; e.an = 6000; e.d1 = 70; e.d2 = 40; e.dn = 30; e.slide = -300; e.lp = 14000; e.hp = 1; }   /* the rim */
    else { e.inc1 = note_inc(88); e.inc2 = inc_ratio(e.inc1, 706); e.a1 = 4400; e.a2 = 2600; e.d1 = 320; e.d2 = 160; }   /* the bell */
    au_post(&e);
}
static void rh_bell(uint32_t lane)              /* the tram's bells: struck at once, ringing (no attack to lag the beat) */
{
    au_ev_t e = {0};
    e.at = au_time(); e.tag = 11; e.pan = (uint8_t)(80u + lane * 24u);
    e.inc1 = note_inc(rh_nt[lane] + 24u); e.inc2 = inc_ratio(e.inc1, 401); e.a1 = 6200; e.a2 = 1800; e.d1 = 420; e.d2 = 140;
    au_post(&e);
}
static void rh_hit(uint32_t lane)               /* a key (or the horn): the judgement */
{
    uint32_t i, best = RH_MAXN, pos;
    int32_t bd = 1 << 30;
    rh.press_t[lane] = fm1_ms;
    if (lane >= rh.nl) return;
    if (rh.style == 2u) rh_drum(lane);
    else if (rh.style == 1u) rh_bell(lane);
    else { rh.lead_lane = lane; ta_lead_note = rh_nt[lane]; ta_lead_n++; }
    if (rh.state != 0u || rh.won) return;
    pos = rh_pos();
    for (i = rh.next; i < rh.n; i++) {
        int32_t dt = (int32_t)(rh.step[i] * MU_STEP) - (int32_t)pos;
        if (dt > RH_OKW) break;
        if (rh.res[i] || rh.lane[i] != lane) continue;
        if (iabs(dt) < iabs(bd)) { bd = dt; best = i; }
    }
    if (best < RH_MAXN && iabs(bd) <= RH_OKW) {
        rh.res[best] = iabs(bd) <= RH_PERFECT ? 2u : iabs(bd) <= RH_GOOD ? 1u : 4u;   /* (res 4: OK) */
        rh.pts += rh.res[best] == 2u ? 5u : rh.res[best] == 1u ? 4u : 2u; rh.hits++;   /* (PERFECT 5, GOOD 4, OK 2: all GOOD is 80%) */
        if (rh.len[best] && lane < rh.nl) { rh.held[best] = 1; rh.hold_i[lane] = best; }
        if (++rh.combo > rh.best_combo) rh.best_combo = rh.combo;
        rh.judge = rh.res[best]; rh.judge_t = fm1_ms; rh.judge_lane = lane;
    } else rh.combo = 0;                        /* nothing there: the note still sounds, no points */
}
static void rh_key(uint32_t k) { uint32_t lane = rh_lane_of(k); if (lane < rh.nl && rh.started && !rh.paused) rh_hit(lane); }   /* (at once, even while the screen is sent) */

static void rh_enter(void)
{
    rh.bars = (uint32_t)PRM(P_RHYTHM_BARS); rh.density = (uint32_t)PRM(P_RHYTHM_DENSITY); rh.pass = (uint32_t)PRM(P_RHYTHM_PASS);
    rh.style = (uint32_t)PRM(P_RHYTHM_STYLE); rh.song = (uint32_t)PRM(P_RHYTHM_SONG);
    rh_nt = rh.style == 1u ? RH_TRAM_NOTE : rh.style == 2u ? RH_FEST_NOTE : RH_NOTE;
    rh.nl = rh.style == 1u ? 3u : rh.style == 2u ? 3u : RH_LANES;
    rh.won = 0; rh.best_combo = 0; rh.tier = 0;
    rh.voice_was = ld_voice;
    ld_voice = 0;                               /* (the rides strike their own bells and drums) */
    mu_set_song(rh.song);
    if (rh.style) {                             /* a ride: its line's cab view */
        uint32_t l;
        for (l = 0; l < NTRLEGS && TR_LEGS[l].ride != rh.style; l++) {}
        tr_ride_begin(l, rh.style);
    }
    rh.started = 0; rh.paused = 0;              /* (not yet: the song starts when the ride is played, not under its card) */
    au_hook = 0; ta_music = 0;
    mu.t = 0; mu.abs = 0; mu.len = 0; rh.n = 0; rh.next = 0; rh.state = 0; rh.pts = 0; rh.combo = 0; rh.hits = 0; rh.judge_t = 0;   /* (under the card: a still, empty line) */
    kb_hook = rh_key;
}

static uint32_t rh_max(void) { uint32_t i, m = 0; for (i = 0; i < rh.n; i++) m += 5u + (rh.len[i] ? 1u : 0u); return m; }
static uint32_t rh_pct(void) { uint32_t m = rh_max(); return m ? rh.pts * 100u / m : 100u; }
static uint32_t rh_acc(void)                    /* so far: the notes judged */
{
    uint32_t i, m = 0;
    for (i = 0; i < rh.next; i++) m += 5u + (rh.len[i] ? 1u : 0u);
    return m ? rh.pts * 100u / m : 100u;
}
static uint32_t rh_tier_of(uint32_t pct)        /* a ride's tier: S is hard (95%), the rest generous */
{
    return pct >= 95u ? 7u : pct >= 85u ? 6u : pct >= 75u ? 5u : pct >= 65u ? 4u : pct >= 55u ? 3u : pct >= 45u ? 2u : 1u;
}

static int rh_update(void)
{
    uint32_t pos, i, k, keys = in.kheld;
    if (!rh.started) { rh.started = 1; rh_start(); }   /* (the count-in from here: the card is gone) */
    led_btn_mask = BIT(BTN_EDIT) | BIT(BTN_PLAY);
    led_btn_blink = rh.paused ? BIT(BTN_PLAY) : 0u;
    if (PRESSED(BTN_PLAY) && !rh.won && rh.state == 0u
#ifdef DINGHY_HOST
        && !tr_autoplay
#endif
       ) {                                      /* PLAY: pause (the song and its clock stop), and on again */
        rh.paused = !rh.paused;
        if (rh.paused) { au_hook = 0; ta_lead_note = 0; } else au_hook = rh_audio;
        sfx_click(0, rh.paused ? 36 : 48, 128);
    }
    if (rh.paused) return 0;
    pos = rh_pos();
#ifdef DINGHY_HOST
    if (rh.state == 0u && tr_autoplay && !rh.won) {   /* (host: the autopilot plays, hands a little loose: +-70 ms; holds kept) */
        static int32_t jit;
        for (i = rh.next; i < rh.n && i < rh.next + 3u; i++) {
            int32_t dt = (int32_t)(rh.step[i] * MU_STEP) - (int32_t)pos;
            if (!rh.res[i] && dt <= jit) { rh_key(rh_nt[rh.lane[i]] - 53u); jit = (int32_t)rndn(6200) - 3100; break; }
        }
        for (i = 0; i < rh.nl; i++) if (rh.hold_i[i] < RH_MAXN || rh.lead_lane == i) keys |= 1u << (rh_nt[i] - 53u);
    }
#endif
    if (rh.lead_lane < rh.nl && !(keys >> (rh_nt[rh.lead_lane] - 53u) & 1u)) {   /* the lead: the last key still held */
        rh.lead_lane = rh.nl; ta_lead_note = 0;
        for (i = 0; i < rh.nl; i++) if (keys >> (rh_nt[i] - 53u) & 1u) { rh.lead_lane = i; ta_lead_note = rh.style ? 0u : rh_nt[i]; }
    }
    if (rh.won) return 1;
    for (i = 0; i < rh.nl; i++) {            /* holds: kept to the end, +1; let go early, nothing more */
        uint32_t h = rh.hold_i[i];
        if (h >= RH_MAXN || rh.held[h] != 1u) continue;
        if (pos + MU_STEP >= (uint32_t)(rh.step[h] + rh.len[h]) * MU_STEP) {
            rh.held[h] = 2; rh.pts++; rh.hold_i[i] = RH_MAXN;
#ifdef DINGHY_HOST
            if (tr_autoplay && rh.lead_lane == i) { rh.lead_lane = rh.nl; ta_lead_note = 0; }
#endif
        }
        else if (!(keys >> (rh_nt[i] - 53u) & 1u)) { rh.held[h] = 3; rh.hold_i[i] = RH_MAXN; }
    }
    if (rh.state == 0u) {
        while (rh.next < rh.n && (rh.res[rh.next] || (int32_t)(rh.step[rh.next] * MU_STEP) - (int32_t)pos < -RH_OKW)) {
            if (!rh.res[rh.next]) { rh.res[rh.next] = 3; rh.combo = 0; rh.judge = 3; rh.judge_t = fm1_ms; rh.judge_lane = rh.lane[rh.next]; }
            rh.next++;
        }
        if (pos >= rh.end_step * MU_STEP) {     /* the song is over */
#ifdef DINGHY_HOST
            printf("%6u ms  rhythm (style %u): %u notes (%u held), %u hit, %u%% (pass %u%%), best combo %u, tier %c%c", fm1_ms, rh.style, rh.n, rh.holds, rh.hits, rh_pct(), rh.pass, rh.best_combo, TR_TIER[rh_tier_of(rh_pct())], 10);
#endif
            if (rh_pct() >= rh.pass) {
                rh.won = 1; rh.tier = rh_tier_of(rh_pct());
                tr_last_tier = (uint8_t)rh.tier; tr_last_pts = (int32_t)rh_pct(); tr_last_mx = 100;   /* (a ride: its tier on the map) */
                return 1;
            }
            rh.state = 1; rh.state_t = fm1_ms; sfx_buzz(au_time());
        }
    } else if (fm1_ms - rh.state_t > 2600u) rh_start();
    for (k = 0; k < 27u; k++) {                 /* the six keys glow; a note arriving lights its key */
        uint32_t lane = rh_lane_of(k), on = 0;
        if (lane < rh.nl) {
            on = (keys >> k & 1u) != 0u;
            for (i = rh.next; i < rh.n && i < rh.next + 4u; i++) {
                int32_t dt = (int32_t)(rh.step[i] * MU_STEP) - (int32_t)pos;
                if (rh.lane[i] == lane && !rh.res[i] && dt < (int32_t)MU_STEP && dt > -RH_OKW) on = 1;
            }
        }
        fm1_led_key_glow(KEY_LED(k), lane < rh.nl);
        fm1_led_key(KEY_LED(k), (int)on);
    }
    return 0;
}

#define RH_RHY 78                               /* a ride: the cab view's horizon */
#define RH_RHIT 162                             /* a ride: the hit line, low in the windscreen */
static int32_t rh_hy(void) { return rh.style ? RH_RHY : RH_HY; }
static int32_t rh_hitline(void) { return rh.style ? RH_RHIT : RH_HIT; }
static int32_t rh_rail_x(int32_t edge, int32_t y)   /* the lane edge 0..6 (the seaside tram: -1, its cue lane) at height y */
{
    int32_t xb = rh.style ? 52 + edge * 45 : 12 + edge * 36;   /* (the rides' three lanes, centred) */
    return 120 + (xb - 120) * (y - rh_hy()) / (rh_hitline() + 20 - rh_hy());
}
static int32_t rh_y(int32_t frames)             /* the height of a time this far ahead (frames) */
{
    int32_t z = frames * 16 / (int32_t)MU_STEP;  /* sixteenths, x16 */
    if (z < 0) return rh_hitline() - z / 2;
    return rh_hitline() - (rh_hitline() - rh_hy()) * z / (z + 120);
}

static void rh_scene(uint32_t pos)              /* the synthwave rails' sky and ground (the rides: the cab's own view) */
{
    int32_t y;
    uint32_t i, bt = MU_STEP * 4u;
    {                                           /* the synthwave: the sky and its sun, the grid */
        for (y = STAGE_Y0; y < RH_HY; y++)
            g_rect(0, y, SW, 1, mix565(RGB(18, 8, 48), RGB(220, 60, 150), (uint32_t)(y - STAGE_Y0) * 256u / (uint32_t)(RH_HY - STAGE_Y0)));
        for (y = -18; y <= 0; y++) {
            int32_t hw = (int32_t)mo_isqrt((uint32_t)(324 - y * y));
            if (y > -9 && (y & 1)) continue;
            g_rect(120 - hw, RH_HY + y, hw * 2, 1, mix565(RGB(255, 230, 120), RGB(255, 70, 160), (uint32_t)(y + 18) * 14u));
        }
        g_rect(0, RH_HY, SW, STAGE_Y1 - RH_HY, C_NIGHT);
        for (i = 0; i < 8u; i++) {
            uint32_t f = ((pos % bt) * 256u / bt + i * 32u) & 255u;
            int32_t gy = RH_HY + (int32_t)(f * f * (uint32_t)(RH_HIT + 20 - RH_HY) >> 16);
            g_rect(0, gy, SW, 1, mix565(C_NIGHT, C_ROSE_D, 60u + f / 2u));
        }
    }
}
static void rh_note(int32_t cx, int32_t cy, int32_t hw, uint32_t l, int missed)   /* a note: a bar, a bell, a drum head */
{
    if (rh.style == 1u) {                       /* a brass bell */
        uint16_t c = missed ? C_PAPER3 : RH_TRAM_COL[l];
        int32_t r = hw / 2 + 4;
        g_disc(cx, cy, r, c);
        g_rect(cx - r - 1, cy + r / 2, r * 2 + 3, 2, c);
        if (!missed) g_disc(cx, cy + r / 2 + 2, 2, RGB(120, 80, 40));
    } else if (rh.style == 2u) {                /* a drum head, a bell */
        uint16_t c = missed ? C_PAPER3 : RH_FEST_COL[l];
        int32_t r = hw / 2 + (l == 0u ? 5 : l == 1u ? 3 : 1);
        g_disc(cx, cy, r, c);
        if (l < 3u) g_ring(cx, cy, r - 2, r, missed ? C_PAPER3 : RGB(250, 240, 230));
    } else g_rrect(cx - hw, cy - (4 + hw / 3) / 2, hw * 2, 4 + hw / 3, 2, missed ? C_PAPER3 : RH_COL[l]);
}
static void rh_notes(uint32_t pos)              /* the notes, far to near (and the festival's lanterns) */
{
    uint32_t i;
    int32_t ybot = rh.style ? CAB_WY + CAB_WH - 2 : STAGE_Y1;
    if (rh.style == 2u)                         /* the lanterns: one beside each note, lit when it was played */
        for (i = 0; i < rh.n; i++) {
            int32_t t0 = (int32_t)(rh.step[i] * MU_STEP) - (int32_t)pos, y0, x, s;
            if (t0 > 16 * (int32_t)MU_STEP || t0 < -8 * (int32_t)MU_STEP || (i & 1u)) continue;
            y0 = rh_y(t0);
            if (y0 > ybot - 4) continue;
            s = 2 + (y0 - rh_hy()) / 18;
            x = rh.lane[i] < 3u ? rh_rail_x(0, y0) - 3 - s : rh_rail_x(rh.nl, y0) + 3 + s;
            g_rect(x, y0 - 14 - s * 3, 1, s + 2, RGB(80, 60, 70));   /* (its cord) */
            if (rh.res[i] == 1u || rh.res[i] == 2u || rh.res[i] == 4u) {   /* lit: warm, glowing */
                g_disc(x, y0 - 12 - s * 2 + s, s + 4, RGB(110, 40, 20));
                g_rrect(x - s, y0 - 12 - s * 2, s * 2 + 1, s * 2 + 2, s, RGB(255, 150, 50));
                g_rect(x - s, y0 - 12 - s, s * 2 + 1, 1, RGB(255, 220, 140));
            } else {                            /* dark: waiting for its note */
                g_rrect(x - s, y0 - 12 - s * 2, s * 2 + 1, s * 2 + 2, s, rh.res[i] == 3u ? RGB(50, 26, 30) : RGB(90, 30, 36));
                g_rect(x - s, y0 - 12 - s, s * 2 + 1, 1, RGB(140, 60, 60));
            }
        }
    for (i = rh.n; i-- > 0;) {
        int32_t t0 = (int32_t)(rh.step[i] * MU_STEP) - (int32_t)pos, y0, xa, xb, hw, l = rh.lane[i];
        if (i < rh.next && rh.held[i] != 1u) continue;
        if (t0 > 16 * (int32_t)MU_STEP) continue;
        if (l == RH_CUE) {                      /* the seaside tram's crossing board, on the left */
            if (rh.res[i] == 1u || rh.res[i] == 2u || rh.res[i] == 4u || t0 < -(int32_t)MU_STEP) continue;
            y0 = rh_y(t0); xa = rh_rail_x(rh.nl, y0); xb = rh_rail_x(-1, y0); hw = (xb - xa) / 2;
            if (y0 > ybot) continue;
            g_rect((xa + xb) / 2 - 1, y0 - hw * 2, 2, hw * 2, RGB(240, 240, 240));
            g_thick((xa + xb) / 2 - hw, y0 - hw * 3, (xa + xb) / 2 + hw, y0 - hw, 2, rh.res[i] == 3u ? C_PAPER3 : C_AMBER);
            g_thick((xa + xb) / 2 - hw, y0 - hw, (xa + xb) / 2 + hw, y0 - hw * 3, 2, rh.res[i] == 3u ? C_PAPER3 : C_AMBER);
            continue;
        }
        if (rh.len[i] && rh.res[i] != 3u && rh.held[i] != 3u && rh.held[i] != 2u) {
            int32_t t1 = t0 + (int32_t)(rh.len[i] * MU_STEP), ya = rh_y(t0 < 0 || rh.held[i] == 1u ? 0 : t0), yb = rh_y(t1), yy;
            uint16_t tc = rh.style == 1u ? RH_TRAM_COL[l] : RH_COL[l];
            if (ya > ybot) ya = ybot;
            if (t1 > 0)
                for (yy = yb; yy <= ya; yy += 2) {
                    int32_t x0 = rh_rail_x(l, yy), x1 = rh_rail_x(l + 1, yy), c = (x0 + x1) / 2, w = (x1 - x0) / 6 + 1;
                    g_rect(c - w, yy, w * 2, 2, rh.held[i] == 1u ? C_CREAM : mix565(C_NIGHT, tc, 170));
                }
        }
        if (rh.res[i] == 1u || rh.res[i] == 2u || rh.res[i] == 4u || t0 < -(int32_t)MU_STEP) continue;
        y0 = rh_y(t0);
        if (y0 > ybot - 2) continue;
        xa = rh_rail_x(l, y0); xb = rh_rail_x(l + 1, y0);
        hw = (xb - xa) / 2 - 2;
        if (hw < 2) hw = 2;
        rh_note((xa + xb) / 2, y0, hw, (uint32_t)l, rh.res[i] == 3u);
    }
}
/* the screen shows a frame a while after it is drawn (its drawing and sending, then it stays up until the next): the
 * notes are drawn that far ahead, so they reach the line as the beat is heard. Measured from the frames themselves
 * (a heavy view, a longer lead); the judging stays on the music's own clock */
static uint32_t rh_draw_last, rh_frame_ms = 40u;
static uint32_t rh_draw_pos(void)
{
    uint32_t now = fm1_ms, lead;
    if (rh_draw_last && now - rh_draw_last < 400u) rh_frame_ms += ((now - rh_draw_last) * 4u + rh_frame_ms * 12u) / 16u - rh_frame_ms;   /* (smoothed) */
    rh_draw_last = now;
    lead = rh_frame_ms * 3u / 2u;               /* (on screen from the end of this frame to the end of the next: on average a frame and a half on) */
    lead = lead > 10u ? lead - 10u : 0u;        /* (less the sound's own short way out) */
    if (lead > 200u) lead = 200u;
    return rh.started && !rh.paused ? rh_pos() + lead * 441u / 10u : rh_pos();
}
static void rh_draw_ride(uint32_t won)          /* a ride: the cab's view, the notes on the glass, the strip */
{
    uint32_t pos = rh_draw_pos(), beat = (pos / MU_STEP) & 3u, span = rh.end_step * MU_STEP;
    int32_t e, hit = rh_hitline(), y0 = CAB_MY;
    char b[32], *p;
    uint16_t edge = rh.style == 1u ? C_BRASS_L : RGB(255, 120, 150);
    tr_ride_move(rh.state ? 0u : pos, span, rh.style == 1u ? 26 : 34);
    tr_ride_view();
    for (e = 0; e <= rh.nl; e++)             /* the lanes, faint on the glass */
        g_line(rh_rail_x(e, rh_hy() + 6), rh_hy() + 6, rh_rail_x(e, CAB_WY + CAB_WH - 4), CAB_WY + CAB_WH - 4, e == 0 || e == rh.nl ? edge : mix565(ut.night, edge, 90));
    rh_notes(pos);
    g_rect(rh_rail_x(0, hit) - 4, hit - 1, rh_rail_x(rh.nl, hit) - rh_rail_x(0, hit) + 8, 3, beat == 0u ? C_CREAM : edge);
    if (pos < ms_frames(4000)) {                /* setting off: where to, as a drive does */
        p = str_cat(b, "> "); str_cat(p, TR_STATION[tr.L->st_to]);
        g_rrect(10, CAB_WY + 8, text_w(&FONT_S, b) + 12, 18, 5, ut.panel);
        g_text(16, CAB_WY + 9, &FONT_S, b, ut.cream);
    }
    if (rh.judge_t && fm1_ms - rh.judge_t < 400u) {
        const char *t = rh.judge == 2u ? T_RH_PERFECT : rh.judge == 1u ? T_RH_GOOD : rh.judge == 4u ? T_RH_OK : T_RH_MISS;
        int32_t jx = rh.judge_lane == RH_CUE ? 214 : (rh_rail_x((int32_t)rh.judge_lane, hit) + rh_rail_x((int32_t)rh.judge_lane + 1, hit)) / 2;
        g_text_c(clampi(jx, 34, SW - 34), hit - 26 - (int32_t)(fm1_ms - rh.judge_t) / 40, &FONT_S, t, rh.judge == 2u ? C_GOLD : rh.judge == 1u ? C_CYAN : rh.judge == 4u ? C_AMBER : C_PAPER3);
    }
    if (pos < RH_COUNT * MU_STEP && rh.state == 0u) {   /* the count-in: 1 2 3 4, big, on the ticks */
        char c[2] = {(char)('1' + (char)((pos / (MU_STEP * 4u)) & 3u)), 0};
        g_rrect(84, 82, 72, 56, 8, ut.panel);
        g_text_c(120, 92, &FONT_L, c, C_CREAM);
    } else if (pos < RH_LEAD * MU_STEP && rh.state == 0u) {   /* the backing in: away */
        g_rrect(40, 92, 160, 26, 6, ut.panel);
        g_text_c(120, 96, &FONT_S, rh.style == 1u ? T_RH_TRAM_GO : T_RH_FEST_GO, C_CYAN);
    }
    if (rh.paused) {                            /* paused */
        g_rrect(50, 88, 140, 52, 8, C_PANEL);
        g_text_c(120, 92, &FONT_L, T_TR_PAUSED, C_CREAM);
        g_text_c(120, 122, &FONT_S, T_TR_RESUME, C_CYAN);
    }
    if (rh.state == 1u) {                       /* short of the mark: again */
        g_rrect(40, 96, 160, 40, 6, ut.panel);
        g_rframe(40, 96, 160, 40, 6, 1, C_ROSE);
        p = b; fmt_u(p, rh_pct()); while (*p) p++; str_cat(p, "%");
        g_text_c(120, 100, &FONT_S, b, C_CREAM);
        g_text_c(120, 116, &FONT_S, T_RH_AGAIN, C_ROSE);
    }
    if (won) {                                  /* the ride's end: its score and tier, as a drive's scoreboard */
        char t2[2] = {TR_TIER[rh.tier & 7u], 0};
        g_rrect(60, 60, 120, 70, 8, ut.panel);
        g_rframe(60, 60, 120, 70, 8, 2, rh.tier >= 7u ? C_GOLD : ut.cyan);
        g_text_c(120, 66, &FONT_L, t2, rh.tier >= 7u ? C_GOLD : ut.cream);
        p = b; fmt_u(p, rh_pct()); while (*p) p++; str_cat(p, "%");
        g_text_c(120, 100, &FONT_S, b, ut.ink2);
        if (rh.style == 2u) { p = str_cat(b, T_RH_LANTERNS " "); fmt_u(p, rh.hits); g_text_c(120, 114, &FONT_S, b, RGB(255, 170, 60)); }
    }
    /* the strip: as the drive's, with the pads in place of the notches */
    g_rect(0, y0, SW, SH - y0, ut.night);
    g_rect(0, y0, SW, 1, ut.rose); g_rect(0, y0 + 1, SW, 1, ut.rose_d);
    for (e = 0; e < rh.nl; e++) {               /* the pads, under their lanes */
        int32_t xa = rh_rail_x(e, hit + 14) + 2, xb = rh_rail_x(e < rh.nl ? e + 1 : -1, hit + 14) - 2;
        int lit = e >= rh.nl ? fm1_ms - rh.press_t[RH_CUE] < 160u : (fm1_ms - rh.press_t[e] < 120u || rh.hold_i[e] < RH_MAXN);
        uint16_t c = e >= rh.nl ? C_AMBER : rh.style == 1u ? RH_TRAM_COL[e] : RH_FEST_COL[e];
        char nm[2] = {e >= (int32_t)rh.nl ? 'R' : rh_name((uint32_t)e), 0};
        g_rrect(xa, y0 + 5, xb - xa, 16, 3, lit ? c : mix565(ut.panel, c, 70));
        g_text_c((xa + xb) / 2, y0 + 5, &FONT_S, nm, lit ? C_NIGHT : ut.cream);
    }
    {
        uint32_t sc = rh.state ? rh_pct() : rh_acc();
        p = b; fmt_u(p, sc); while (*p) p++; str_cat(p, "%");
        g_text(8, y0 + 26, &FONT_L, b, sc >= rh.pass ? ut.cream : ut.rose);   /* (0: always cream) */
        if (rh.pass) { p = b; fmt_u(p, rh.pass); while (*p) p++; str_cat(p, "% " T_RH_TO_PASS); } else str_cat(b, T_RH_ALONG);
        g_text(78, y0 + 26, &FONT_S, b, ut.ink2);
    }
    if (rh.combo >= 4u) {                       /* the combo: top right of the glass */
        int32_t tw;
        p = b; fmt_u(p, rh.combo); while (*p) p++; str_cat(p, " " T_RH_COMBO);
        tw = text_w(&FONT_S, b);
        g_rrect(SW - 20 - tw, CAB_WY + 8, tw + 12, 18, 5, ut.panel);
        g_text_r(SW - 14, CAB_WY + 9, &FONT_S, b, C_GOLD);
    }
    if (rh.style == 2u) { p = str_cat(b, T_RH_LANTERNS " "); fmt_u(p, rh.hits); while (*p) p++; *p++ = '/'; fmt_u(p, rh.n); g_text(78, y0 + 40, &FONT_S, b, RGB(255, 170, 60)); }
    else g_text(78, y0 + 40, &FONT_S, tr.L->line, ut.ink2);
    {
        uint32_t f = span ? (pos > span ? span : pos) / (span / 60u + 1u) : 0u;
        g_rect(170, y0 + 56, 60, 2, ut.panel2);
        g_rect(170, y0 + 56, (int32_t)(f > 60u ? 60u : f), 2, ut.cyan);
    }
}
static void rh_draw(uint32_t won)
{
    uint32_t pos = rh_draw_pos(), beat = (pos / MU_STEP) & 3u;
    int32_t e;
    char b[32], *p;
    if (rh.style) { rh_draw_ride(won); return; }
    rh_scene(pos);
    for (e = 0; e <= rh.nl; e++) {           /* the lanes (the tram: rails; the festival: lacquer) */
        uint16_t c = rh.style == 1u ? (e == 0 || e == rh.nl ? RGB(220, 220, 230) : RGB(120, 96, 90)) : rh.style == 2u ? (e == 0 || e == rh.nl ? RGB(220, 40, 60) : RGB(90, 30, 50)) : (e == 0 || e == rh.nl ? C_ROSE : C_CYAN_D);
        g_line(rh_rail_x(e, RH_HY), RH_HY, rh_rail_x(e, STAGE_Y1), STAGE_Y1, c);
    }
    rh_notes(pos);
    /* the hit line and its pads, named */
    g_rect(0, RH_HIT - 1, SW, 3, beat == 0u ? C_CREAM : rh.style == 1u ? RGB(230, 200, 140) : rh.style == 2u ? RGB(230, 60, 80) : C_ROSE);
    for (e = 0; e < rh.nl; e++) {
        int32_t xa = rh_rail_x(e, RH_HIT) + 3, xb = rh_rail_x(e + 1, RH_HIT) - 3;
        int lit = fm1_ms - rh.press_t[e] < 120u || (rh.hold_i[e] < RH_MAXN);
        uint16_t c = rh.style == 1u ? RH_TRAM_COL[e] : rh.style == 2u ? RH_FEST_COL[e] : RH_COL[e];
        char nm[2] = {rh_name((uint32_t)e), 0};
        g_rrect(xa, RH_HIT + 4, xb - xa, 15, 3, lit ? c : mix565(C_PANEL, c, 70));
        g_text_c((xa + xb) / 2, RH_HIT + 4, &FONT_S, nm, lit ? C_NIGHT : C_CREAM);
    }
    /* the score, the combo, the judgement */
    {
        uint32_t sc = rh.state ? rh_pct() : rh_acc();
        p = b; fmt_u(p, sc); while (*p) p++; str_cat(p, "%");
        g_text(6, STAGE_Y0 + 2, &FONT_S, b, sc >= rh.pass ? C_VERD : C_ROSE);
    }
    p = b; fmt_u(p, rh.pass); while (*p) p++; str_cat(p, "% " T_RH_TO_PASS);
    g_text(6, STAGE_Y0 + 16, &FONT_S, b, rh.style == 1u ? RGB(250, 240, 230) : C_INK2);
    if (rh.style == 2u) {                       /* the lanterns lit */
        p = str_cat(b, T_RH_LANTERNS " "); fmt_u(p, rh.hits); while (*p) p++; *p++ = '/'; fmt_u(p, rh.n);
        g_text_r(SW - 6, STAGE_Y0 + 16, &FONT_S, b, RGB(255, 170, 60));
    }
    if (rh.combo >= 4u) {
        p = b; fmt_u(p, rh.combo); while (*p) p++; str_cat(p, " " T_RH_COMBO);
        g_text_r(SW - 6, STAGE_Y0 + 2, &FONT_S, b, C_GOLD);
    }
    {
        uint32_t span = rh.end_step * MU_STEP, f = span ? (pos > span ? span : pos) / (span / 200u + 1u) : 0u;
        g_rect(20, STAGE_Y1 - 2, 200, 2, C_PANEL2);
        g_rect(20, STAGE_Y1 - 2, (int32_t)(f > 200u ? 200u : f), 2, C_CYAN);
    }
    if (rh.judge_t && fm1_ms - rh.judge_t < 400u) {
        const char *t = rh.judge == 2u ? T_RH_PERFECT : rh.judge == 1u ? T_RH_GOOD : rh.judge == 4u ? T_RH_OK : T_RH_MISS;
        int32_t jx = (rh_rail_x((int32_t)rh.judge_lane, RH_HIT) + rh_rail_x((int32_t)rh.judge_lane + 1, RH_HIT)) / 2;
        if (rh.judge_lane == RH_CUE) jx = 40;
        g_text_c(clampi(jx, 34, SW - 34), RH_HIT - 26 - (int32_t)(fm1_ms - rh.judge_t) / 40, &FONT_S, t, rh.judge == 2u ? C_GOLD : rh.judge == 1u ? C_CYAN : rh.judge == 4u ? C_AMBER : C_PAPER3);
    }
    if (pos < RH_LEAD * MU_STEP && rh.state == 0u) {   /* the count-in */
        char c[2] = {(char)('1' + (char)((pos / (MU_STEP * 4u)) & 3u)), 0};
        if (pos < RH_COUNT * MU_STEP) g_text_c(120, 100, &FONT_S, c, C_CREAM);
        g_text_c(120, 118, &FONT_S, rh.style == 1u ? T_RH_TRAM_GO : rh.style == 2u ? T_RH_FEST_GO : T_RH_READY, C_CYAN);
    }
    if (rh.paused) {                            /* paused */
        g_rrect(50, 88, 140, 52, 8, C_PANEL);
        g_text_c(120, 92, &FONT_L, T_TR_PAUSED, C_CREAM);
        g_text_c(120, 122, &FONT_S, T_TR_RESUME, C_CYAN);
    }
    if (rh.state == 1u) {                       /* short of the mark: again */
        g_rrect(40, 96, 160, 40, 6, C_PANEL);
        g_rframe(40, 96, 160, 40, 6, 1, C_ROSE);
        p = b; fmt_u(p, rh_pct()); while (*p) p++; str_cat(p, "%");
        g_text_c(120, 100, &FONT_S, b, C_CREAM);
        g_text_c(120, 116, &FONT_S, T_RH_AGAIN, C_ROSE);
    }
    if (won && rh.style) {                      /* a ride's end: its score and tier */
        char t2[2] = {TR_TIER[rh.tier & 7u], 0};
        g_rrect(60, 70, 120, 64, 8, C_PANEL);
        g_rframe(60, 70, 120, 64, 8, 2, rh.tier >= 7u ? C_GOLD : C_CYAN);
        g_text_c(120, 76, &FONT_L, t2, rh.tier >= 7u ? C_GOLD : C_CREAM);
        p = b; fmt_u(p, rh_pct()); while (*p) p++; str_cat(p, "%");
        g_text_c(120, 110, &FONT_S, b, C_INK2);
    }
}

static void rh_leave(void) { kb_hook = 0; au_hook = 0; ta_music = 0; ta_lead_note = 0; mu.lvl = 0; mu.kick_env = 0; ld_voice = rh.voice_was; au_cut(11); if (rh.style) tr_ride_end(); ui_keys_clear(); }
static const game_t G_RHYTHM = {BIT(BTN_EDIT), 2400, rh_enter, rh_update, rh_draw, rh_leave};
