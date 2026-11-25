/* SPDX-License-Identifier: GPL-3.0-only */
/* Mechanica Obscura: the main loop. Title -> a chapter's pages -> its instruction card -> the mechanism ->
 * its finale and plaque -> the closing pages -> the next chapter. Progress is kept in flash between
 * chapters, in silence. HOME twice goes back to the title; EDIT shows the card again. */

enum { S_TITLE, S_STORY_IN, S_HELP, S_PLAY, S_WON, S_STORY_OUT, S_END, S_UNLOCK, S_HEART };
static struct {
    uint32_t s, chap, page, sel;
    uint32_t t_state;                           /* ms the state began */
    uint32_t t_page;                            /* ms the page began typing */
    uint32_t typed_skip;                        /* the typing finished early by a press */
    uint32_t home_t;                            /* HOME pressed once: the second leaves */
    uint32_t title_next;                        /* the title's sound: next event (frame time) */
    uint32_t title_k;
    uint32_t skip_t;                            /* SAVE held since (0: not held) */
    uint32_t skip_run, skip_last;               /* a skip playing out: since, its last step */
    uint32_t workshop;                          /* 1: the daily robots, from the title (no story) */
    uint32_t home_hold;                         /* HOME held since (robots that use every button) */
    uint32_t seq, stage;                        /* a sequence chapter: 1 in its stages, 2 its finale; the stage */
    uint32_t travel, station_mode, replay;      /* driving a leg from the map; a station's puzzles in turn; a replay */
    uint32_t map_st, nadj, msg_t;               /* the map's chosen station, its lines, a message's time */
    int32_t map_dest, leg;                      /* the chosen line (index into adj, -1 none); the leg being driven */
    uint8_t adj[8];
    const char *msg;
    chapter_t travel_ch;                        /* the leg being driven, as a chapter (its words from the leg's own) */
    int32_t ride_prm[8];                        /* a ride: the rhythm game's settings, from the leg */
    int32_t travel_prm[2];
    uint32_t unl_feat, unl_chap, unl_phase, unl_seen;   /* a reward to show: new parts; the chapter (its lines); which screen; shown for */
    uint32_t bp;                                /* the map: the train's blueprint up instead */
    uint32_t cel_t;                             /* a station just restored: the keyboard's sweep, since */
    uint32_t ask_t, ask_st;                     /* a replay asked: when, of which station */
    uint32_t dev_t, dev_latch;                  /* FX + GLO held since; toggled until they are let go */
    uint32_t menu, msel, confirm_t, enoden, en_sel;   /* the title's menu (once finished); its choice; NEW GAME asked; the Enoden's map, its stretch */
    uint32_t boot, boot_t;                      /* power-on: the loop alone (the loading screen), since */

    uint32_t tut;                               /* the first drive's lessons: which card */
    uint32_t wipe_t, wipe_ask;                  /* FX + SEL held on the title since; the full reset asked */
} st;
static char ws_line[64];                        /* the robot's own words at the door */
static page_t ws_page;

#ifdef DINGHY_HOST
static void host_screens(void);
#endif
#ifdef DINGHY_HOST
#define HOST_MENU_AUTO (tr_autoplay && getenv("OBL_MENU"))
#else
#define HOST_MENU_AUTO 0
#endif
static void go(uint32_t s) { st.s = s; st.t_state = fm1_ms; st.skip_run = 0; }
static void page_start(uint32_t p) { st.page = p; st.t_page = fm1_ms; st.typed_skip = 0; }

static const page_t *cur_pages(uint32_t *n)
{
    const chapter_t *c = st.travel ? cur_ch : &CHAP[st.chap];
    if (st.workshop) { *n = st.s == S_STORY_OUT ? 0u : 1u; return &ws_page; }
    if (st.s == S_STORY_OUT) { *n = c->nout; return c->out; }
    *n = c->nin; return c->in;
}
static uint32_t mo_strlen(const char *s) { uint32_t n = 0; while (s[n]) n++; return n; }

static const game_t *ch_game(uint32_t c) { (void)c; return MECH_GAME[cur_ch->type]; }   /* the chapter, stage or robot being played */
#define WS_SEL ((uint32_t)NCHAP)             /* the title's entry after the chapters */
static int ws_open(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < NCHAP; i++) if (save.done >> i & 1u) n++;
    return HAVE_WORKSHOP && n >= 4u;
}
static void story_or_play(void);
static void save_where(uint32_t at, uint32_t s)   /* where the player is, kept: a power-on goes back there */
{
    if (save.at == at && save.at_st == s) return;
    save.at = (uint8_t)at; save.at_st = (uint8_t)s; save_mark();
}
static void chapter_begin(uint32_t c)
{
    if (!st.workshop && c < 32u) save_where(1u + c, CHAP_STATION[c] < NSTATION ? CHAP_STATION[c] : st.map_st);
    st.chap = c;
    st.seq = 0; st.stage = 0;
#ifdef DINGHY_HOST
    printf("%6u ms  chapter %s at %s%s (features %02X)\n", fm1_ms, CHAP[c].name, CHAP_STATION[c] < NSTATION ? TR_STATION[CHAP_STATION[c]] : "-", st.replay ? " (replay)" : "", save.features);
#endif
    rb_force_layout = 0;                        /* (a robot chapter: its own layout setting) */
    cur_ch = &CHAP[c];                          /* the mechanisms read their settings from here (PRM, LST) */
    au_cut(0);
    ui_keys_clear();
    led_btn_blink = 0;
    rng_s ^= fm1_ms * 2654435761u ^ au_time();  /* each playthrough its own puzzles */
    if (!rng_s) rng_s = 1;
    go(S_STORY_IN);
    page_start(0);
    if (!CHAP[c].nin) story_or_play();          /* (a chapter may have no opening pages) */
}
/* the Workshop: a robot comes in (its model and layout chosen now), says its piece, and is mended; then the next */
static void workshop_next(void)
{
#if HAVE_WORKSHOP
    char *p = ws_line;
    st.workshop = 1;
    cur_ch = &WORKSHOP_CH;
    au_cut(0); ui_keys_clear(); led_btn_blink = 0;
    rng_s ^= fm1_ms * 2654435761u ^ au_time();
    if (!rng_s) rng_s = 1;
    rb_force_layout = save.repairs < 3u ? (int32_t)save.repairs + 1 : 0;   /* one part each, then the mix */
    G_ROBOT.enter();
    p = str_cat(p, rb.name); p = str_cat(p, ". "); str_cat(p, T_RB_SAYS);
    ws_page.art = BG_ART_WORKSHOP; ws_page.robot = 1; ws_page.text = ws_line;
    go(S_STORY_IN);
    page_start(0);
#endif
}
static void map_message(const char *m) { st.msg = m; st.msg_t = fm1_ms; }
static uint32_t chap_opens(uint32_t c)          /* the first leg chapter c opens (NTRLEGS: none) */
{
    uint32_t l;
    for (l = 0; l < NTRLEGS; l++) if (TR_LEG_GATE[l] == c) return l;
    return NTRLEGS;
}
static int loop_whole_now(void) { return 0; }  /* (the loop's scene comes after its last line is driven: loop_all_driven) */
static int loop_all_driven(void)                /* every line of the loop driven at least once, either way (TR_LEG_LOOP: from the track) */
{
    uint32_t l;
    uint32_t n = 0;
    for (l = 0; l < NTRLEGS; l++) if (TR_LEG_LOOP[l]) { n++; if (!map_edge_driven(TR_LEGS[l].st_from, TR_LEGS[l].st_to)) return 0; }
    return n > 0u;                              /* (no loop at all: never whole) */
}
/* the loop's music: what the campaign brought back, one after another, into the heart's song */
static void loop_event(uint32_t el)
{
    static uint32_t step;
    if (el < 40u) { step = 0; au_hook = 0; mu_set_song(7); ta_music = 0; ta_kmh8 = 0; ta_load = 0; mu.lvl = 0; au_hook = tr_audio; }
    if (step == 0u && el >= 100u) { ta_chime = TA_POWER; ta_chime_n++; step++; }                       /* the cab, waking */
    if (step == 1u && el >= 900u) { ta_chime = TA_DOORS; ta_chime_n++; step++; }                       /* the doors */
    if (step == 2u && el >= 2300u) {                                                                    /* the music box's tune */
        uint32_t i, t0 = au_time();
        for (i = 0; i < 8u; i++) { int32_t c = TITLE_TUNE[i]; if (c >= 1 && c <= 8) sfx_tine(MB_NOTE[c - 1], 80, t0 + ms_frames(i * 230u), 128, 4); }
        step++;
    }
    if (step == 3u && el >= 4300u) { ta_horn = 1; step++; }                                             /* the horn */
    if (step == 4u && el >= 4800u) { ta_horn = 0; ta_whistle = 1; step++; }                             /* the whistle */
    if (step == 5u && el >= 5200u) { ta_whistle = 0; if (tr_sig_seq()) { ta_chime = tr_sig_seq(); ta_chime_n++; } else ta_mel_n++; ta_music = 1; step++; }   /* your melody, the heart's song */
    if (step == 6u && el >= 9000u) { ta_chime = TA_ARRIVE; ta_chime_n++; sfx_flourish(62, au_time()); step++; }
}
static void loop_event_end(void) { ta_music = 0; ta_horn = 0; ta_whistle = 0; mu.lvl = 0; au_hook = 0; }
static void unlock(uint32_t c)                  /* a chapter solved the first time: its parts join the train, its lines open */
{
    uint32_t f = CHAP_UNLOCK[c] & ~save.features;
    save.features |= (uint8_t)f;
    if (f || chap_opens(c) < NTRLEGS) { st.unl_feat = f; st.unl_chap = c; st.unl_phase = f ? 0u : 1u; st.unl_seen = 0; }
}
static uint32_t map_order(uint32_t o)            /* the stations in the line's order: as the story reaches them, the rest after */
{
    static uint8_t ord[NSTATION], made;
    if (!made) {
        uint32_t i, j, n = 0, k;
        for (i = 0; i < NCHAP && n < NSTATION; i++) {
            uint32_t s2 = CHAP_STATION[i];
            if (s2 >= NSTATION) continue;
            for (k = 0; k < n && ord[k] != s2; k++) {}
            if (k == n) ord[n++] = (uint8_t)s2;
        }
        for (j = 0; j < NSTATION && n < NSTATION; j++) {
            for (k = 0; k < n && ord[k] != j; k++) {}
            if (k == n) ord[n++] = (uint8_t)j;
        }
        made = 1;
    }
    return ord[o < NSTATION ? o : 0];
}
static void map_point_next(uint32_t s)         /* the map at station s, its line toward the story's next station lit */
{
    uint32_t j, n, target = 255, a2[8];
    uint8_t *adj = (uint8_t *)a2;
    for (j = 0; j < NCHAP; j++)
        if (CHAP_STATION[j] < NSTATION && CHAP_STATION[j] != s && CHAP[j].type != MECH_TRAVEL && !(save.done >> j & 1u)) { target = CHAP_STATION[j]; break; }
    st.map_st = s; st.map_dest = -1;
    n = map_adj(s, adj);
    for (j = 0; j < n; j++) if (TR_LEGS[adj[j]].st_to == target) { st.map_dest = (int32_t)j; return; }
    for (j = 0; j < n; j++) if (!map_reached(TR_LEGS[adj[j]].st_to)) { st.map_dest = (int32_t)j; return; }
}
static void map_pick_next(void)                /* back on the map: the first line open and not yet driven (in the story's order), chosen */
{
    uint32_t c, j, n;
    if (map_puzzles_left(st.map_st)) return;    /* (puzzles still here: the station stays chosen) */
    for (c = 0; c < NCHAP; c++) {
        uint32_t l;
        if (CHAP[c].type != MECH_TRAVEL) continue;
        l = (uint32_t)CHAP[c].prm[P_TRAVEL_LEG];
        if (l >= NTRLEGS || (save.driven >> l & 1u) || !map_leg_open(l)) continue;
        st.map_st = TR_LEGS[l].st_from;
        n = map_adj(st.map_st, st.adj);
        for (j = 0; j < n; j++) if (st.adj[j] == l) { st.map_dest = (int32_t)j; return; }
    }
}
static void go_map(void)
{
    if (tr_mode == TRM_ENDLESS) {               /* off the endless line: its distance, if the farthest; back to the menu */
        uint32_t m = tr_odo_base + tr_pos_m();
        if (m > save.endless_m) { save.endless_m = m; save_mark(); }
        st.menu = 1;
    }
    tr_mode = TRM_STORY; tr_force = 0; tr_names = TR_STATION; tr_heart = 0; mu_hp = -1;
    st.travel = 0; st.station_mode = 0; st.replay = 0; st.workshop = 0;
    au_cut(0); ui_keys_clear(); led_btn_blink = 0;
    if (!dev_mode) map_pick_next();             /* the next line to drive, chosen for you */
    st.nadj = map_adj(st.map_st, st.adj);
    if (st.map_dest >= (int32_t)st.nadj) st.map_dest = -1;
    if (!st.boot) save_where(0, st.map_st);     /* (on the map: kept as where they are) */
    go(S_TITLE);
}
static void chapter_next(void)
{
    uint32_t j, all = 1, s;
    if (st.workshop) { workshop_next(); return; }
    if (st.travel) {
        if (tr_white) {                         /* into the light: straight on to the heart's story (no map) */
            uint32_t c;
            for (c = 0; c < NCHAP; c++)
                if (CHAP_STATION[c] < NSTATION && TR_STPOS[CHAP_STATION[c]].hidden && CHAP[c].type != MECH_TRAVEL && !(save.done >> c & 1u)) {
                    st.map_st = CHAP_STATION[c]; go_map(); st.station_mode = 1; st.replay = 0; chapter_begin(c); return;
                }
        }
        if (!save.loop_seen && loop_all_driven()) {   /* the loop driven all the way round: now it is whole */
#ifdef DINGHY_HOST
            printf("%6u ms  the loop is whole%c", fm1_ms, 10);
#endif
            save.loop_seen = 1; save_mark(); st.unl_phase = 3; st.unl_chap = 255; go(S_UNLOCK); return;
        }
        go_map(); return;
    }
    if (CHAP[st.chap].type == MECH_STORY || CHAP[st.chap].nstages) {   /* (no mechanism to solve: done once read) */
        if (!(save.done >> st.chap & 1u)) { save.done |= 1u << st.chap; unlock(st.chap); }
        save_mark();
    }
    if (st.unl_chap == st.chap && !st.unl_seen && (st.unl_feat || chap_opens(st.chap) < NTRLEGS)) {   /* its reward, shown */
        st.unl_seen = 1; au_cut(0); ui_keys_clear(); go(S_UNLOCK); return;
    }
    s = CHAP_STATION[st.chap];
    if (st.station_mode)                        /* the station's next puzzle, if any */
        for (j = st.chap + 1u; j < NCHAP; j++)
            if (CHAP_STATION[j] == s && CHAP[j].type != MECH_TRAVEL && (st.replay ? CHAP[j].type != MECH_STORY : !(save.done >> j & 1u))) { chapter_begin(j); return; }
    for (j = 0; j < NCHAP; j++) if (CHAP[j].type != MECH_TRAVEL && !(save.done >> j & 1u)) all = 0;
    if ((all || dev_mode) && s < NSTATION && TR_STPOS[s].hidden && !st.replay) {   /* (dev mode: the finale too) */
#ifdef DINGHY_HOST
        printf("%6u ms  THE END\n", fm1_ms);
#endif
        au_cut(0); go(S_HEART); return; }   /* the heart's story read: the lines into it, its turning, then the end */
    if (!st.msg || fm1_ms - st.msg_t > 3000u) {
        if (map_secret_open() && s < NSTATION && !TR_STPOS[s].hidden && !map_reached(NSTATION - 1u)) map_message(T_MAP_SECRET);
        else if (s < NSTATION && map_restored(s) && !st.replay) { }   /* (restored: straight to the map, the next line chosen) */
    }
    if (s < NSTATION && map_restored(s) && !st.replay) map_point_next(s);
    go_map();
}
/* a station's puzzles: the first not yet solved, then the rest in turn; all solved, from the first again */
static void station_play(uint32_t s)
{
    uint32_t j, first = 255;
    for (j = 0; j < NCHAP; j++)
        if (CHAP_STATION[j] == s && CHAP[j].type != MECH_TRAVEL) {
            if (first == 255u && CHAP[j].type != MECH_STORY) first = j;   /* (a replay: the puzzles, not the story) */
            if (!(save.done >> j & 1u)) { st.station_mode = 1; st.replay = 0; chapter_begin(j); return; }
        }
    if (first != 255u) { st.station_mode = 1; st.replay = 1; chapter_begin(first); return; }
    map_message(T_MAP_NOTHING);
}
/* a leg from the map: its words from the leg's own travel chapter (its page only the first time), or any */
static void travel_begin(uint32_t leg)
{
    uint32_t c = TR_LEG_CHAP[leg], j, t = 255;
    if (c == 255u) { for (j = 0; j < NCHAP; j++) if (CHAP[j].type == MECH_TRAVEL) { t = j; break; } }
    else t = c;
    if (t == 255u) return;
    st.travel_ch = CHAP[t];
    st.travel_prm[0] = (int32_t)leg;
    st.travel_ch.prm = st.travel_prm;
    if (TR_LEGS[leg].ride) {                    /* a ride: the line is a rhythm game to its song */
        st.travel_ch.type = MECH_RHYTHM;
        st.ride_prm[P_RHYTHM_BARS] = TR_LEGS[leg].rbars; st.ride_prm[P_RHYTHM_DENSITY] = TR_LEGS[leg].rdens;
        st.ride_prm[P_RHYTHM_PASS] = TR_LEGS[leg].rpass; st.ride_prm[P_RHYTHM_STYLE] = TR_LEGS[leg].ride;
        st.ride_prm[P_RHYTHM_SONG] = TR_LEGS[leg].music;
        st.travel_ch.prm = st.ride_prm;
    }
    if (c == 255u || (save.driven >> leg & 1u)) st.travel_ch.nin = 0;
    st.travel_ch.nout = 0;
    st.travel = 1; st.station_mode = 0; st.leg = (int32_t)leg; st.tut = 0;
    if (leg < 64u) save_where(64u + leg, TR_LEGS[leg].st_from);
    tr_best_tier = save.tier[leg];
    tr_features = (uint8_t)(save.features | (dev_mode ? 0xFFu : 0u));
    tr_music_off = save.music_off;
    st.chap = t; st.seq = 0; st.stage = 0;
    cur_ch = &st.travel_ch;
    au_cut(0); ui_keys_clear(); led_btn_blink = 0;
    go(S_STORY_IN);
    page_start(0);
    if (!st.travel_ch.nin) story_or_play();
}

/* before the first drive: a short card a lesson, a picture of the thing it teaches (0 power, 1 limits, 2 the stop,
 * 3 the score), page beads; the words are tut_* in script.txt */
#define NTUT 4u
static void ui_tutor(uint32_t i)
{
    static const char *T[NTUT], *X[NTUT];
    static const int16_t NO[9][2] = {{-223, 90}, {-239, -25}, {-199, -134}, {-113, -212}, {0, -240}, {113, -212}, {199, -134}, {239, -25}, {223, 90}};   /* the controller's notches, x16, round the top */
    int32_t x = 8, y = STAGE_Y0 + 6, w = SW - 16, h = STAGE_Y1 - STAGE_Y0 - 10, cx = SW / 2, k;
    T[0] = T_TUT_POWER_T; T[1] = T_TUT_LIMIT_T; T[2] = T_TUT_STOP_T; T[3] = T_TUT_SCORE_T;
    X[0] = T_TUT_POWER; X[1] = T_TUT_LIMIT; X[2] = T_TUT_STOP; X[3] = T_TUT_SCORE;
    if (i >= NTUT) return;
    ui_card(x, y, w, h);
    g_rrect(x + 10, y + 8, w - 20, 18, 4, ut.panel2);
    g_text_c(cx, y + 9, &FONT_S, T[i], ut.cyan);
    if (i == 0u) {                              /* the controller: brake to the left, power to the right */
        g_disc(cx, 92, 22, C_BRASS_D); g_disc(cx, 92, 19, ut.night);
        for (k = 0; k < 9; k++) g_disc16(cx * 16 + NO[k][0], 92 * 16 + NO[k][1], 24, k < 4 ? C_ROSE : k > 4 ? C_CYAN : ut.cream);
        g_thick16(cx * 16, 92 * 16, (cx + 9) * 16, (92 - 9) * 16, 40, ut.cream);
        g_disc(cx, 92, 3, C_BRASS_D);
        g_text_c(46, 84, &FONT_S, "BRAKE", C_ROSE);
        g_text_c(194, 84, &FONT_S, "POWER", C_CYAN);
    } else if (i == 1u) {                       /* the limit sign, and the next one ahead */
        g_disc(96, 90, 24, RGB(220, 30, 60)); g_disc(96, 90, 18, RGB(250, 246, 240));
        g_text_c(96, 74, &FONT_L, "40", RGB(20, 16, 30));
        g_tri16(140 * 16, 89 * 16, 150 * 16, 89 * 16, 145 * 16, 80 * 16, C_AMBER);   /* (as the cab shows the next limit) */
        g_text(156, 76, &FONT_S, "60", C_AMBER);
        g_text(140, 94, &FONT_S, "200m", C_AMBER);
    } else if (i == 2u) {                       /* the train's front coming up to the marker */
        g_rect(24, 106, 192, 2, RGB(190, 170, 230));
        for (k = 0; k < 16; k++) g_rect(28 + k * 12, 108, 6, 3, RGB(80, 60, 100));
        g_rrect(64, 80, 84, 26, 4, RGB(70, 50, 96)); g_rect(64, 88, 84, 3, C_ROSE);
        for (k = 0; k < 4; k++) g_rect(70 + k * 18, 83, 12, 5, C_GOLD);
        g_rect(170, 66, 3, 42, C_AMBER); g_rrect(162, 60, 19, 10, 2, C_AMBER);
        g_text(180, 84, &FONT_S, "12m", ut.cream);
    } else {                                    /* the scoreboard's grade */
        g_text(40, 70, &FONT_S, "STOP   +10", ut.cream);
        g_text(40, 88, &FONT_S, "TIME    +5", ut.cream);
        g_rrect(164, 66, 42, 42, 6, ut.panel2);
        g_text_c(185, 71, &FONT_L, "S", C_GOLD);
    }
    g_text_wrap(x + 12, 118, w - 24, 15, &FONT_S, X[i], -1, ut.cream);   /* (4 lines fit above the beads) */
    for (k = 0; k < (int32_t)NTUT; k++) g_disc(cx - 18 + k * 12, y + h - 34, 2, (uint32_t)k == i ? C_ROSE : ut.panel2);
    g_rect(x + 12, y + h - 26, w - 24, 1, ut.rose_d);
    g_text_c(SW / 2, y + h - 22, &FONT_S, T_TUT_FOOT, ut.rose);
}
/* power-on, a game under way: back where the player was (the puzzle they were on, from its start; the line they were
 * driving, from its start; else the map at their station) */
static void boot_resume(void)
{
    uint32_t a = save.at, s = save.at_st;
    if (dev_mode) return;
    if (s < NSTATION && map_reached(s)) { st.map_st = s; st.map_dest = -1; st.nadj = map_adj(s, st.adj); }
    if (a >= 1u && a <= 32u && a - 1u < NCHAP && !(save.done >> (a - 1u) & 1u) && CHAP[a - 1u].type != MECH_TRAVEL) {
        st.menu = 0; st.station_mode = 1; st.replay = 0; chapter_begin(a - 1u);
    } else if (a >= 64u && a - 64u < NTRLEGS && map_leg_open(a - 64u)) {
        st.menu = 0; travel_begin(a - 64u);
    } else if (a == 0u && s < NSTATION && !save.finished) st.menu = 0;   /* (the map, at their station; once finished: the menu, as ever) */
}

/* the Enoden (the title's menu): a stretch of the coast line, as a train: every part but the synth */
static void travel_begin_mode(uint32_t mode, uint32_t leg)
{
    uint32_t j, t = 255;
    for (j = 0; j < NCHAP; j++) if (CHAP[j].type == MECH_TRAVEL) { t = j; break; }
    if (t == 255u) return;
    st.travel_ch = CHAP[t];
    st.travel_prm[0] = 0;
    st.travel_ch.prm = st.travel_prm;
    st.travel_ch.nin = 0; st.travel_ch.nout = 0;
    tr_mode = (uint8_t)mode;
    if (mode == TRM_ENODEN) {
        tr_force = &EN_LEGS[leg]; tr_names = EN_STATION;
        tr_features = (uint8_t)(0xFFu & ~(F_SYNTH | F_VOICE));
        tr_best_tier = save.en_tier[leg];
        st.travel_ch.name = T_EN_TITLE; st.travel_ch.help_title = T_EN_TITLE; st.travel_ch.help = T_EN_HELP; st.travel_ch.ctrl = T_EN_CTRL; st.travel_ch.footer = T_EN_FOOTER;
    } else {
        tr_force = 0; tr_names = TR_STATION;
        tr_features = (uint8_t)(F_KEY | F_HORN | F_WHISTLE | F_SYNTH | F_VOICE);
        tr_best_tier = 0;
        st.travel_ch.name = T_EL_TITLE; st.travel_ch.help_title = T_EL_TITLE; st.travel_ch.help = T_EL_HELP; st.travel_ch.ctrl = T_EL_CTRL; st.travel_ch.footer = T_EL_FOOTER;
    }
    tr_music_off = save.music_off;
    st.travel = 1; st.station_mode = 0; st.leg = (int32_t)leg; st.tut = 0;
    st.chap = t; st.seq = 0; st.stage = 0;
    cur_ch = &st.travel_ch;
    au_cut(0); ui_keys_clear(); led_btn_blink = 0;
    go(S_STORY_IN);
    page_start(0);
    story_or_play();
}

/* a sequence chapter (the masterwork): its stages are puzzles of their own (STAGES[]), played in turn */
static void stage_begin(uint32_t k)
{
    st.seq = 1; st.stage = k;
    cur_ch = &STAGES[CHAP[st.chap].stages[k]];
    au_cut(0); ui_keys_clear(); led_btn_blink = 0;
    MECH_GAME[cur_ch->type]->enter();
    go(S_HELP);
}
/* after the opening pages: the mechanism's card, or (a story chapter) the next chapter */
static void story_or_play(void)
{
    const game_t *g = ch_game(st.chap);
    if (st.workshop) { go(save.repairs ? S_PLAY : S_HELP); return; }   /* (the robot was made at the door) */
    if (CHAP[st.chap].nstages) { stage_begin(0); return; }
    if (g) { g->enter(); go(st.travel && cur_ch->type == MECH_TRAVEL && tr_mode == TRM_STORY && save.driven ? S_PLAY : S_HELP); }   /* (a drive: its card only the first time) */
    else chapter_next();
}
static void to_closing_pages(void)
{
    if (st.workshop) {                          /* one more mended: count it, the next is at the door */
        if (save.repairs < 60000u) save.repairs++;
        save_mark();
        workshop_next();
        return;
    }
    if (st.travel) { chapter_next(); return; }  /* (a leg: back to the map) */
    if (st.seq == 1) {                          /* a stage done: the next, or the sequence's own finale */
        if (st.stage + 1u < CHAP[st.chap].nstages) { stage_begin(st.stage + 1u); return; }
        st.seq = 2;
        cur_ch = &CHAP[st.chap];
        au_cut(0); ui_keys_clear();
        MECH_GAME[cur_ch->type]->enter();
        go(S_WON);
        return;
    }
    st.seq = 0;
    cur_ch = &CHAP[st.chap];
    go(S_STORY_OUT);
    page_start(0);
    if (!CHAP[st.chap].nout) chapter_next();
}

/* the title's sound: a slow clock; once the music box has sung, its tune */
static void title_sound(void)
{
    uint32_t now = au_time();
    if ((int32_t)(st.title_next - now) > (int32_t)ms_frames(200)) return;
    if ((int32_t)(st.title_next - now) < 0) st.title_next = now + ms_frames(100);
    if (save.heard_music) {
        uint32_t c = (uint32_t)TITLE_TUNE[st.title_k % TITLE_TUNE_N];   /* content/puzzles.txt [global] */
        if (c >= 1u && c <= 8u) sfx_tine(MB_NOTE[c - 1u], 60, st.title_next, 128, 4);
        st.title_next += ms_frames(st.title_k % TITLE_TUNE_N == TITLE_TUNE_N - 1u ? 1600u : 340u);
    } else {
        sfx_tick(st.title_next, (st.title_k & 1u) ? 74 : 79, 60, (st.title_k & 1u) ? 90 : 166, 4);
        st.title_next += ms_frames(1000);
    }
    st.title_k++;
}

/* skip a puzzle: SAVE held for 2 s puts the mechanism into a solved state, so its own finale plays as if it
 * had been solved (the walkthrough script uses it to reach every finale too) */
#define SKIP_MS 2000u
/* a skip played out: a step toward the answer every so often, so the solving is seen (the rest: mo_cheat at the end) */
static int32_t sk_toward(int32_t v, int32_t t, int32_t most) { int32_t d = t - v; return v + (d > most ? most : d < -most ? -most : d); }
static void mo_skip_step(void)
{
    uint32_t b;
    switch (cur_ch->type) {
    case MECH_REEDS:                            /* each reed a little truer, sounding */
        for (b = 0; b < 4u; b++) rd.off[b] = sk_toward(rd.off[b], 0, rd.step * 3);
        rd_tap(st.skip_last / 220u % 4u, 0);
        break;
    case MECH_MUSICBOX:                         /* the next missing pin, set */
        for (b = 0; b < MB_STEPS; b++) if (!mb.orig[b] && mb.pin[b] != mb.tune[b]) { mb.pin[b] = mb.tune[b]; mb.target = (int32_t)b; if (mb.tune[b]) sfx_tine(mb.comb[mb.tune[b] - 1u], 80, 0, 128, 2); break; }
        break;
    case MECH_CIPHER:                           /* the wheels turned, one notch, first to last */
        for (b = 0; b < 3u; b++) if (ci.turn[b] != ci.key[b]) { ci.turn[b] += ((ci.key[b] - ci.turn[b] + 26) % 26) <= 13 ? 1 : -1; ci.turn[b] = (ci.turn[b] + 26) % 26; sfx_click(0, 50, 128); break; }
        break;
    case MECH_SAFE:                             /* the dial turned to each number, each set */
        if (sf.stage < (uint32_t)sf.wheels) {
            int32_t tgt = sf.combo[sf.stage], d = ((tgt - sf_num()) % sf.n + sf.n) % sf.n;
            if (d == 0) { if (sf.stage < 3u) sf.set_t[sf.stage] = fm1_ms; sf.stage++; sfx_thunk(0, 44, 70, 128); }
            else { sf.pos += d <= sf.n / 2 ? (d > 4 ? 4 : d) : -(sf.n - d > 4 ? 4 : sf.n - d); sfx_tick(0, 70, 30, 128, 0); }
        }
        break;
    case MECH_SWITCHES:                         /* the levers set, the train sent */
        if (swb.state == 0u) { swb.lev = swb.sol; swb.state = 1; swb.run_t = fm1_ms; sfx_thunk(0, 44, 70, 128); }
        break;
    case MECH_RADIO: {                          /* the knobs brought round, one after another */
        int32_t g = clampi(330 / (rx.g0 ? rx.g0 : 1), 4, 24);
        if (rx.f != rx.ft) rx.f = sk_toward(rx.f, rx.ft, 12);
        else if (rx.bw != 5) rx.bw = sk_toward(rx.bw, 5, 3);
        else if (rx.gain != g) rx.gain = sk_toward(rx.gain, g, 3);
        else rx.nc = sk_toward(rx.nc, rx.it, 4);
        break;
    }
    case MECH_EXCHANGE:                         /* a cord a step, then the test */
        for (b = 0; b < ex.n; b++) if (ex.conn[b] != (int8_t)ex.perm[b]) { ex.held_in = -1; ex.held_out = -1; ex_patch(b, ex.perm[b]); return; }
        if (!ex.fin_t) { ex.through = ex.n; ex.fin_t = fm1_ms; sfx_motif(au_time()); ex.msg = T_EX_DONE; ex.msg_t = fm1_ms; }
        break;
    case MECH_SEQUENCER:                        /* a train's timetable written a step */
        for (b = 0; b < sq.n; b++) {
            uint32_t a2 = 2u * b, b2 = 2u * (b + sq.n);
            uint16_t want = (uint16_t)((1u << a2) | (1u << ((a2 + 2u) % 16u)) | (1u << (b2 % 16u)) | (1u << ((b2 + 2u) % 16u)));
            if (sq.pat[b] != want) { sq.pat[b] = want; sq.sel = b; sq_check(); sfx_tick(0, 70, 50, 128, 0); return; }
        }
        break;
    case MECH_WATCH:                            /* the knobs brought true, in the order a watchmaker would; then the test */
        if (wt.k[2] != wt.t[2]) wt.k[2] = sk_toward(wt.k[2], wt.t[2], 3);
        else if (wt.k[1] != wt.t[1]) wt.k[1] = sk_toward(wt.k[1], wt.t[1], 4);
        else wt.k[0] = sk_toward(wt.k[0], wt.t[0], 5);
        break;
    case MECH_GHOST:                            /* each dial brought to its message, in turn; then linked */
        for (b = 0; b < GH_C; b++) if (gh.pos[b] != gh.st[b][0]) { gh.act = b; gh.pos[b] = sk_toward(gh.pos[b], gh.st[b][0], 6); return; }
        if (!gh.link_t) { gh.brk = -1; gh.link_t = fm1_ms; }
        break;
    case MECH_PHOTO:                            /* the plate developed, then lit, focused, contrasted */
        if (ph.k[3] != ph.t[3]) ph.k[3] = sk_toward(ph.k[3], ph.t[3], 2);
        else if (ph.k[0] != ph.t[0]) ph.k[0] = sk_toward(ph.k[0], ph.t[0], 2);
        else if (ph.k[1] != ph.t[1]) ph.k[1] = sk_toward(ph.k[1], ph.t[1], 2);
        else ph.k[2] = sk_toward(ph.k[2], ph.t[2], 2);
        break;
    default: break;
    }
}
static void mo_cheat(void)
{
    uint32_t b;
    switch (cur_ch->type) {
    case MECH_ESCAPEMENT: esc.idx = esc.e0; esc.stud = -esc.b0; break;
    case MECH_GEARBOARD: for (b = 0; b < GB_N; b++) if (!gb.fixed[b]) gb.g[b] = gb.sol[b]; gb_drive(); break;   /* (the player's own gears out) */
    case MECH_MUSICBOX: for (b = 0; b < MB_STEPS; b++) if (!mb.orig[b]) mb.pin[b] = mb.tune[b]; break;
    case MECH_BIRD: bd.round = bd.rounds - 1u; bd.state = 3; bd.state_t = fm1_ms; break;
    case MECH_RHYTHM: rh.won = 1; rh.tier = 2; tr_last_tier = 2; break;   /* (a ride skipped: an E) */
    case MECH_SWITCHES: swb.won = 1; break;
    case MECH_SHUNTING: sh.won = 1; break;
    case MECH_EXCHANGE: ex.won = 1; break;
    case MECH_RADIO: rx.won = 1; rx.fin_t = fm1_ms; break;
    case MECH_PANTOGRAPH: pa.state = 3; break;
    case MECH_SEQUENCER: sq.won = 1; break;
    case MECH_WATCH: wt.won = 1; break;
    case MECH_GHOST: gh.won = 1; break;
    case MECH_PHOTO: ph.won = 1; ph.fin_t = fm1_ms; break;
    case MECH_SAFE: sf.stage = (uint32_t)sf.wheels; break;
    case MECH_REEDS: for (b = 0; b < 4u; b++) rd.off[b] = 0; break;
    case MECH_CIPHER: for (b = 0; b < 3u; b++) ci.turn[b] = ci.key[b]; break;
    case MECH_SLIDER: {     /* the key slides out: clear its row to the right (the skip need not solve it) */
        int32_t i2;
        for (i2 = 0; i2 < sl.n; i2++) if (i2 != sl.key && !sl.hz[i2] && sl.c[i2] >= sl.c[sl.key] && sl.r[i2] <= 2 && sl.r[i2] + sl.len[i2] > 2) sl.r[i2] = sl.r[i2] >= 1 ? 3 : 0;
        sl.c[sl.key] = SL_N - 2; break;
    }
    case MECH_ASTRO: as.h = as.th; as.m = as.tm; as.s = as.ts; break;
    case MECH_RINGS: rg.lvl = rg.n - 2; rg.fall = 255; rg.fdir = 1; break;
    case MECH_ROBOT: rb.done = rb.parts; break;
    case MECH_MASTER: mw.stage = 2; mw.got = mw.song_n; break;
    case MECH_TRAVEL: tr_cheat(); break;
    }
}

static void frame(void)
{
    const game_t *g = ch_game(st.chap);
    const chapter_t *ch = cur_ch;
    uint32_t pn;
    const page_t *pg;
    const char *htitle = ch->name;
    ui_read();
    ui_hdr_station = st.chap < NCHAP && CHAP_STATION[st.chap] < NSTATION ? TR_STATION[CHAP_STATION[st.chap]] : 0;
    if (st.workshop) ui_hdr_station = 0;
    if (ch->type == MECH_TRAVEL && NTRLEGS) {  /* the cab: the line's name, > where it is going */
        const tr_leg_t *lg = &TR_LEGS[ch->prm[P_TRAVEL_LEG] < (int32_t)NTRLEGS ? ch->prm[P_TRAVEL_LEG] : 0];
        htitle = lg->line; ui_hdr_station = TR_STATION[lg->st_to];
        if (tr_mode == TRM_ENODEN && tr_force) { htitle = tr_force->line; ui_hdr_station = EN_STATION[tr_force->st_to]; }
        if (tr_mode == TRM_ENDLESS) { htitle = T_EL_TITLE; ui_hdr_station = 0; }   /* (no station ahead: no arrow) */
    }
    led_btn_mask = 0;

    /* HOME twice: back to the title (from anywhere but the title). A mechanism that uses every button (the
     * robots' relays) wants HOME held instead */
    {
        int grab = g && g->grab && (st.s == S_PLAY || st.s == S_HELP);
        int leave = 0;
        if (grab) {
            if (in.held & BIT(BTN_HOME)) { if (!st.home_hold) st.home_hold = fm1_ms ? fm1_ms : 1u; if (fm1_ms - st.home_hold > 1500u) leave = 1; }
            else st.home_hold = 0;
        } else if (PRESSED(BTN_HOME) && st.s != S_TITLE) {
            if (fm1_ms - st.home_t < 2000u) leave = 1; else st.home_t = fm1_ms;
        }
        if (leave) {
            if (g && g->leave) g->leave();
            au_cut(0); ui_keys_clear(); kb_hook = 0; led_btn_blink = 0;
            st.home_hold = 0; st.seq = 0;
            go_map();
            st.home_t = 0;
        }
    }

    if ((in.held >> BTN_FX & 1u) && (in.held >> BTN_GLO & 1u)) {   /* developer mode: FX + GLO held for 3 s */
        if (!st.dev_t) st.dev_t = fm1_ms ? fm1_ms : 1u;
        if (!st.dev_latch && fm1_ms - st.dev_t >= 3000u) {
            st.dev_latch = 1; dev_mode = !dev_mode; st.bp = 0;
            if (dev_mode) { st.menu = 1; st.msel = 2; }   /* (the title's menu too: ENODEN and ENDLESS) */
            else if (!save.finished) { st.menu = 0; st.enoden = 0; }
            map_message(dev_mode ? T_DEV_ON : T_DEV_OFF);
            sfx_flourish(dev_mode ? 72 : 60, au_time());
        }
    } else { st.dev_t = 0; st.dev_latch = 0; }

    ui_theme(tr_mode == TRM_ENODEN || (st.s == S_TITLE && st.enoden && !st.menu) ? 1 : st.travel && tr_heart && st.s != S_TITLE ? 2 : 0);   /* (the Enoden: a plain rail cab; the heart: red) */
    g_begin();
    switch (st.s) {
    case S_TITLE: {
        int32_t e1 = enc_main() + in.enc[ENC_PRESETS] + in.enc[ENC_ALGORITHM] + in.enc[ENC_KNOB2];   /* (any knob) */
        if (st.boot) {                          /* power-on: the loop alone for 2 s; a new game's title then fades its words in */
            uint32_t el = fm1_ms - st.boot_t;
            if (!(save.features & F_KEY) && !dev_mode && !st.menu && !st.enoden) {
                if (PRESSED(BTN_PLAY) && el < 3000u) { st.boot_t = fm1_ms - 3000u; el = 3000u; }   /* (PLAY: the words at once) */
                tl_text_a = el < 2000u ? 0u : el >= 3000u ? 256u : (el - 2000u) * 256u / 1000u;
                title_draw(0, 0);
                if (el >= 3000u) { st.boot = 0; st.t_state = fm1_ms; }
            } else {                            /* (a game under way: the loop, then its map or the menu) */
                tl_text_a = 0;
                g_rect(0, 0, SW, SH, RGB(8, 4, 22));
                title_scene(fm1_ms);
                if (el >= 2000u || in.press) { st.boot = 0; tl_text_a = 256u; st.t_state = fm1_ms; boot_resume(); }
            }
            break;
        }
        {   /* hidden (in no guide): FX + SEL held 3 s on the title or its menu asks to reset everything; PLAY wipes every save */
            int on_title = ((save.finished || dev_mode) && st.menu) || (!(save.features & F_KEY) && !dev_mode && !st.enoden);
            if (on_title && (in.held >> BTN_FX & 1u) && (in.held >> BTN_SEL & 1u) && !st.wipe_ask) {
                if (!st.wipe_t) st.wipe_t = fm1_ms ? fm1_ms : 1u;
                else if (fm1_ms - st.wipe_t >= 3000u) { st.wipe_ask = 1; st.t_state = fm1_ms; sfx_click(0, 40, 128); }
            } else st.wipe_t = 0;
            if (st.wipe_ask) {
                led_btn_mask = BIT(BTN_PLAY) | BIT(BTN_HOME); led_btn_blink = BIT(BTN_PLAY);
                if (st.menu) menu_draw(st.msel, 0); else title_draw(0, 0);
                g_rect_a(0, 0, SW, SH, C_NIGHT, 170);
                g_rrect(24, 72, 192, 96, 8, C_PANEL);
                g_rframe(24, 72, 192, 96, 8, 2, C_ROSE);
                g_text_c(SW / 2, 82, &FONT_S, T_WIPE_ASK, C_ROSE);
                g_text_c(SW / 2, 108, &FONT_S, T_WIPE_SURE, C_CREAM);
                g_text_c(SW / 2, 140, &FONT_S, T_WIPE_KEYS, C_CYAN);
                if (PRESSED(BTN_HOME)) { st.wipe_ask = 0; sfx_click(0, 40, 128); }
                else if (PRESSED(BTN_PLAY) && fm1_ms - st.t_state > 400u) {   /* every save back to nothing: a fresh FM-1 */
                    memset(&save, 0, sizeof save); save.magic = MO_MAGIC; save_mark();
                    mu_dyn_ok = 0; dev_mode = 0; st.wipe_ask = 0; st.menu = 0; st.enoden = 0; st.msel = 0; st.confirm_t = 0;
                    st.map_st = ST_START; st.map_dest = -1; tl_text_a = 256u;
                    sfx_thunk(0, 40, 90, 128); go(S_TITLE);
                }
                break;
            }
        }
        if ((save.finished || dev_mode) && PRESSED(BTN_HOME)) { st.menu = 1; st.enoden = 0; st.bp = 0; sfx_click(0, 40, 128); }
        if ((save.finished || dev_mode) && st.menu) {   /* finished once (or developer mode): CONTINUE, NEW GAME, ENODEN, ENDLESS */
            led_btn_mask = BIT(BTN_PLAY); led_btn_blink = BIT(BTN_PLAY);
            if (e1) { st.msel = (st.msel + (e1 > 0 ? 1u : 3u)) % 4u; st.confirm_t = 0; sfx_click(0, 50, 128); }
            menu_draw(st.msel, st.confirm_t && fm1_ms - st.confirm_t < 3000u);
            if ((PRESSED(BTN_PLAY) || (HOST_MENU_AUTO && fm1_ms - st.t_state > 1500u && st.msel >= 2u)) && fm1_ms - st.t_state > 300u) {
                sfx_thunk(0, 48, 70, 128);
                if (st.msel == 0u) { st.menu = 0; st.enoden = 0; }
                else if (st.msel == 1u) {       /* NEW GAME: asked twice; the story starts over; kept: the Enoden's grades, the endless line's farthest, the workshop's repairs, the music setting, the menu itself */
                    if (st.confirm_t && fm1_ms - st.confirm_t < 3000u) {
                        save.done = 0; save.driven = 0; save.features = 0; save.heard_music = 0;
                        save.loop_seen = 0; save.mb_tune = 0; save.at = 0; save.at_st = 0;   /* (the loop's scene to come again, a new tune for the music box, nowhere to resume) */
                        { uint32_t k; for (k = 0; k < 32u; k++) save.tier[k] = 0; for (k = 0; k < 8u; k++) save.sig[k] = 0; }   /* (and your melody to write again) */
                        mu_dyn_ok = 0;                  /* (the heart's song: back to its own until the new melody is made) */
                        save_mark(); st.menu = 0; st.enoden = 0; st.confirm_t = 0;
                        st.map_st = ST_START; st.map_dest = -1;
                    } else st.confirm_t = fm1_ms ? fm1_ms : 1u;
                } else if (st.msel == 2u) { st.menu = 0; st.enoden = 1; }
                else { st.menu = 0; travel_begin_mode(TRM_ENDLESS, 0); }
            }
            break;
        }
        if (st.enoden) {                        /* the Enoden: its stretches along the coast, each one's best, the whole line's */
            led_btn_mask = BIT(BTN_PLAY); led_btn_blink = BIT(BTN_PLAY);
            if (e1) { st.en_sel = (st.en_sel + (e1 > 0 ? 1u : NENLEGS - 1u)) % NENLEGS; sfx_click(0, 50, 128); }
            en_map_draw(st.en_sel);
            ui_footer(T_EN_FOOT);
            if (PRESSED(BTN_PLAY) && fm1_ms - st.t_state > 300u) { sfx_thunk(0, 48, 70, 128); travel_begin_mode(TRM_ENODEN, st.en_sel); }
#ifdef DINGHY_HOST
            if (tr_autoplay && fm1_ms - st.t_state > 700u && st.en_sel < NENLEGS && !save.en_tier[st.en_sel]) travel_begin_mode(TRM_ENODEN, st.en_sel);
#endif
            break;
        }
        if (!(save.features & F_KEY) && !dev_mode) {   /* no key yet: the title, and the first station */
            led_btn_mask = BIT(BTN_PLAY); led_btn_blink = BIT(BTN_PLAY);
            title_draw(0, 0);
            if ((PRESSED(BTN_PLAY) || (tr_autoplay && fm1_ms - st.t_state > 700u)) && fm1_ms - st.t_state > 300u) {
                sfx_thunk(0, 48, 70, 128); st.map_st = ST_START; station_play(ST_START);
            }
            break;
        }
        if (e1) {                               /* one knob: each station reached, then each line out of it, in the line's order */
            uint8_t es[64]; int8_t ed[64];
            uint32_t n = 0, o, j, cur = 0, a2[8], na;
            for (o = 0; o < NSTATION; o++) {
                uint32_t s2 = map_order(o);
                if (!map_visible(s2) || !map_reached(s2)) continue;
                es[n] = (uint8_t)s2; ed[n] = -1; n++;
                na = map_adj(s2, (uint8_t *)a2);
                for (j = 0; j < na && n < 64u; j++)     /* (only the lines that can be driven: their station restored first) */
                    if (map_leg_open(((uint8_t *)a2)[j])) { es[n] = (uint8_t)s2; ed[n] = (int8_t)j; n++; }
            }
            for (j = 0; j < n; j++) if (es[j] == st.map_st && ed[j] == st.map_dest) cur = j;
            if (n) { cur = (cur + (e1 > 0 ? 1u : n - 1u)) % n; st.map_st = es[cur]; st.map_dest = ed[cur]; }
            st.msg = 0;
            sfx_click(0, st.map_dest < 0 ? 50 : 40, 128);
        }
        st.nadj = map_adj(st.map_st, st.adj);
        if (st.map_dest >= (int32_t)st.nadj) st.map_dest = -1;
        if (PRESSED(BTN_FX)) { st.bp = !st.bp; sfx_click(0, 50, 128); }
        if (st.bp) {                            /* the train's blueprint, over the map */
            led_btn_mask = BIT(BTN_FX) | BIT(BTN_PLAY); led_btn_blink = 0;
            bp_draw(save.features | (dev_mode ? 0xFFu : 0u), 0);
            ui_footer(T_BP_FOOT);
            if (PRESSED(BTN_PLAY)) { st.bp = 0; sfx_click(0, 40, 128); }
            break;
        }
        led_btn_mask = BIT(BTN_PLAY) | BIT(BTN_FX);
        led_btn_blink = BIT(BTN_PLAY);
        {   /* the keyboard: a station just restored, a sweep out from the middle and back; otherwise dark */
            uint32_t k, el2 = fm1_ms - st.cel_t;
            int cel = st.cel_t && fm1_ms - st.cel_t < 2400u;
            for (k = 0; k < 27u; k++) {
                uint32_t d2 = k > 13u ? k - 13u : 13u - k, w = (el2 / 60u) % 28u;
                fm1_led_key(KEY_LED(k), cel && (d2 == w / 2u || d2 + 1u == w / 2u));
                fm1_led_key_glow(KEY_LED(k), cel && d2 <= w / 2u);
            }
        }
        map_draw(st.map_st, st.map_dest >= 0 ? (int32_t)st.adj[st.map_dest] : -1, st.msg && fm1_ms - st.msg_t < 4000u ? st.msg : 0);
        ui_footer(T_MAP_FOOT);
        if (PRESSED(BTN_PLAY) && fm1_ms - st.t_state > 300u && !tr_autoplay) {
            sfx_thunk(0, 48, 70, 128);
            if (st.map_dest >= 0) {
                uint32_t l = st.adj[st.map_dest];
                if (map_leg_open(l)) travel_begin(l); else map_message(T_MAP_LOCKED);
            } else if (map_reached(st.map_st)) {
                if (map_restored(st.map_st) && !(st.ask_t && st.ask_st == st.map_st && fm1_ms - st.ask_t < 4000u)) {   /* done already: ask first */
                    st.ask_t = fm1_ms ? fm1_ms : 1u; st.ask_st = st.map_st; map_message(T_MAP_ASK_REPLAY);
                } else { st.ask_t = 0; station_play(st.map_st); }
            }
        }
#ifdef DINGHY_HOST
        if (tr_autoplay && st.s == S_TITLE && fm1_ms - st.t_state > 700u) {   /* (host: the autopilot plays the map too) */
            uint32_t s2, l, pick = 255, found = 0;
            for (s2 = 0; s2 < NSTATION && !found; s2++)
                if (map_visible(s2) && map_reached(s2) && !map_restored(s2)) { st.map_st = s2; station_play(s2); found = 1; }
            for (l = 0; l < NTRLEGS && !found; l++)
                if (map_leg_open(l) && !(save.driven >> l & 1u) && (pick == 255u || !map_reached(TR_LEGS[l].st_to))) pick = l;
            if (!found && pick != 255u) { st.map_st = TR_LEGS[pick].st_from; travel_begin(pick); }
        }
#endif
        break;
    }
    case S_STORY_IN:
    case S_STORY_OUT: {
        uint32_t len, shown;
        pg = cur_pages(&pn);
        len = mo_strlen(pg[st.page].text);
        shown = st.typed_skip ? len : (fm1_ms - st.t_page) / 28u;
        if (shown > len) shown = len;
        {   /* the typewriter: a soft tick a letter (the machines' lower) */
            static uint32_t last;
            if (shown != last && shown < len && (shown & 1u) && pg[st.page].text[shown] != ' ')
                sfx_tick(0, pg[st.page].robot ? 62 : 84, 14, 128, 5);
            last = shown;
        }
        led_btn_mask = BIT(BTN_PLAY);
        story_heart = !st.travel && CHAP_STATION[st.chap] < NSTATION && TR_STPOS[CHAP_STATION[st.chap]].hidden;
        if (story_heart && st.page == 1u && fm1_ms - st.t_page < 40u && save.sig[0]) {   /* the machine plays your melody back, slowly */
            uint32_t k, t0 = au_time() + ms_frames(400);
            for (k = 0; k < 8u; k++) sfx_tine(save.sig[k], 70, t0 + ms_frames(k * 520u), 128, 6);
        }
        if (story_heart) {                      /* the heart: a beat under the words */
            static uint32_t hb;
            if (fm1_ms - hb >= 1000u) { hb = fm1_ms; sfx_thunk(0, 30, 70, 128); sfx_thunk(au_time() + ms_frames(260), 28, 50, 128); }
        }
        story_draw(&pg[st.page], (int32_t)shown, shown >= len, st.page, pn);
        if (!story_heart) ui_header(ch->type == MECH_TRAVEL ? "" : ch->num, htitle);
        ui_footer(fm1_ms - st.home_t < 2000u ? T_HOME_AGAIN : T_PLAY_ON);
        {   /* (the beads and the mark live in the footer: draw them again over it) */
            uint32_t i2;
            for (i2 = 0; i2 < pn; i2++) g_disc(SW - 10 - (int32_t)(pn - 1u - i2) * 8, STAGE_Y1 + 9, 2, i2 == st.page ? C_GOLD : C_PAPER3);
        }
        if (PRESSED(BTN_PLAY) || in.kdown || in.enc[ENC_KNOB1] > 0) {
            if (shown < len) st.typed_skip = 1;
            else if (st.page + 1u < pn) { page_start(st.page + 1u); sfx_click(0, 40, 128); }
            else if (st.s == S_STORY_IN) { sfx_click(0, 50, 128); story_or_play(); }
            else chapter_next();
        }
        break;
    }
    case S_HELP:
        led_btn_mask = BIT(BTN_PLAY) | BIT(BTN_EDIT);
        g_rect(0, 0, SW, SH, C_NIGHT);          /* (nothing of the last screen under the card: a drive's view is not drawn yet) */
        g->draw(0);
        ui_header(ch->type == MECH_TRAVEL ? "" : ch->num, htitle);
        if (st.travel && ch->type == MECH_TRAVEL && tr_mode == TRM_STORY && !save.driven && st.tut < NTUT) {   /* the first drive: its lessons first */
            led_btn_mask = BIT(BTN_PLAY);
            ui_tutor(st.tut);
            ui_footer("");
            if (PRESSED(BTN_PLAY) || (tr_autoplay && fm1_ms - st.t_state > 600u * (st.tut + 1u))) { st.tut++; sfx_click(0, 50, 128); }
            break;
        }
        ui_help(ch->help_title, ch->help, ch->ctrl, T_HELP_FOOT);
        ui_footer(ch->footer);
        if (PRESSED(BTN_PLAY) || (PRESSED(BTN_EDIT) && !g->grab)) { go(S_PLAY); sfx_click(0, 50, 128); }
        break;
    case S_PLAY: {
        int solved;
        uint32_t held;
        if (in.held & BIT(BTN_SAVE)) { if (!st.skip_t) st.skip_t = fm1_ms ? fm1_ms : 1u; }
        else st.skip_t = 0;
        held = st.skip_t ? fm1_ms - st.skip_t : 0u;
        solved = g->update();
        if (held >= SKIP_MS && !solved && !st.skip_run && !(tr_autoplay && (cur_ch->type == MECH_TRAVEL || cur_ch->type == MECH_RHYTHM))) {   /* (host autoplay: the train is driven, not skipped) */
            st.skip_t = 0; sfx_thunk(0, 44, 90, 128);
            if (cur_ch->type == MECH_TRAVEL || cur_ch->type == MECH_RHYTHM) { mo_cheat(); solved = 1; }   /* (a drive, a ride: at once) */
            else { st.skip_run = fm1_ms ? fm1_ms : 1u; st.skip_last = 0; }   /* a puzzle: its solving played out */
        }
        if (st.skip_run && !solved) {
            if (fm1_ms - st.skip_last >= 220u) { st.skip_last = fm1_ms; mo_skip_step(); }
            if (fm1_ms - st.skip_run > 4000u) { mo_cheat(); solved = 1; }   /* (what is left, at the end) */
        }
        led_btn_mask |= g->buttons;
        g->draw(0);
        if (ch->type == MECH_TRAVEL || (ch->type == MECH_RHYTHM && rh.style)) {   /* driving (or a ride): no header, no footer, the windscreen takes the screen */
            if (held > 250u && !solved) g_rect(0, SH - 2, (int32_t)(held * (uint32_t)SW / SKIP_MS), 2, C_GOLD);
        } else {
        ui_header(ch->num, htitle);
        if (held > 250u && !solved) {           /* holding: the footer fills with gold until it skips */
            int32_t w = (int32_t)(held * (uint32_t)(SW - 20) / SKIP_MS);
            ui_footer("");
            g_text_c(SW / 2, STAGE_Y1 + 1, &FONT_S, T_KEEP_HOLDING, C_CREAM);
            g_rect(10, STAGE_Y1 + 15, SW - 20, 3, C_PAPER2);
            g_rect(10, STAGE_Y1 + 15, w, 3, C_GOLD);
        } else
            ui_footer(fm1_ms - st.home_t < 2000u ? T_HOME_AGAIN : ch->footer);
        }
        if (PRESSED(BTN_EDIT) && !g->grab) go(S_HELP);
        if (solved) {
            if (!st.travel) sfx_motif(au_time() + ms_frames(g->plaque_ms));   /* (restored: the motif) */
            if (st.travel && tr_mode == TRM_ENODEN) {   /* an Enoden stretch: its best; the next one chosen */
                uint32_t l = (uint32_t)st.leg;
                if (tr_last_tier > save.en_tier[l]) save.en_tier[l] = tr_last_tier;
                if (!save.en_mx[l] || tr_last_pts > save.en_pts[l]) save.en_pts[l] = (int8_t)clampi(tr_last_pts, -100, 100);
                save.en_mx[l] = (uint8_t)tr_last_mx;
                st.en_sel = l + 1u < NENLEGS ? l + 1u : l;
            } else if (st.travel) {             /* a leg: driven, its best tier, the train now at its far end */
                uint32_t l = (uint32_t)st.leg;
                save.driven |= 1u << l;
                if (tr_last_tier > save.tier[l]) save.tier[l] = tr_last_tier;
                st.map_st = TR_LEGS[l].st_to; st.map_dest = -1;
                save.music_off = (uint8_t)tr_music_off;
            } else if (!st.workshop && !st.seq) {
                if (ch->type == MECH_MUSICBOX && !save.heard_music) save.heard_music = 1;
                if (!(save.done >> st.chap & 1u)) { save.done |= 1u << st.chap; unlock(st.chap); }
            }
            save_mark();
            go(S_WON);
        }
        break;
    }
    case S_WON: {
        uint32_t el = fm1_ms - st.t_state;
        led_btn_mask = el > g->plaque_ms + 900u ? BIT(BTN_PLAY) : 0u;
        led_btn_blink = led_btn_mask;
        g->draw(el + 1u);
        if (ch->type != MECH_TRAVEL && !(ch->type == MECH_RHYTHM && rh.style)) {   /* (a drive or a ride: its scoreboard is the finale, no header, no plaque) */
            ui_header(ch->num, htitle);
            if (el > g->plaque_ms) ui_plaque(el - g->plaque_ms, ch->word);
        }
        ui_footer(el > g->plaque_ms + 900u && !tr_white ? T_PLAY_ON : "");
        if ((el > g->plaque_ms + 900u && (PRESSED(BTN_PLAY) || in.kdown || el > g->plaque_ms + 2800u)) || (st.travel && tr_white && el > 1800u)) {   /* (on by itself after a moment) */
            if (g->leave) g->leave();
            led_btn_blink = 0;
            to_closing_pages();
        }
        break;
    }
    case S_UNLOCK: {                            /* a reward: the part on the blueprint, the line on the map, the loop made whole */
        uint32_t el = fm1_ms - st.t_state, l = chap_opens(st.unl_chap), bit = 1;
        led_btn_mask = BIT(BTN_PLAY); led_btn_blink = el > 800u ? BIT(BTN_PLAY) : 0u;
        if (el < 30u && st.unl_phase != 3u && st.unl_phase != 4u) sfx_motif(au_time());
        if (st.unl_phase == 4u) {               /* your melody: eight notes in D minor, the line's (and the heart's song) from now on */
            static uint8_t sg[8], cur, asked;
            static uint32_t init, prev_t;
            uint32_t k, full = 1;
            int32_t e3 = enc_main() + in.enc[ENC_ALGORITHM];
            if (init != st.t_state) { for (k = 0; k < 8u; k++) sg[k] = 0; cur = 0; asked = 0; prev_t = 0; init = st.t_state; }
#ifdef DINGHY_HOST
            if (tr_autoplay && !sg[7]) { static const uint8_t D[8] = {62, 65, 69, 67, 65, 64, 62, 57}; for (k = 0; k < 8u; k++) sg[k] = (uint8_t)sig_fit(sg, k, D[k]); cur = 0; }
#endif
            if (e3) { cur = (uint8_t)((cur + (e3 > 0 ? 1u : 7u)) % 8u); sfx_click(0, 50, 128); asked = 0; }
            for (k = 0; k < 27u; k++) if (in.kdown >> k & 1u) {   /* a key: the note at the cursor, kept in key */
                sg[cur] = (uint8_t)sig_fit(sg, cur, 53u + k);
                sfx_chime((int32_t)sg[cur] + 12, 70, 0, 128);
                if (cur < 7u) cur++;
                asked = 0;
            }
            if (PRESSED(BTN_REC)) { for (k = 0; k < 8u; k++) sg[k] = 0; cur = 0; asked = 0; sfx_click(0, 30, 128); sfx_thunk(0, 44, 50, 128); }   /* again, from nothing */
            for (k = 0; k < 8u; k++) if (!sg[k]) full = 0;
            if (PRESSED(BTN_PLAY) && full) {    /* hear it as it will be: the heart's song, built of it */
                if (prev_t) { loop_event_end(); prev_t = 0; }
                else { mu_build_sig(sg); au_hook = 0; mu_set_song(7); mu.lvl = 0; mu.step = 63u; mu.t = 0; ta_music = 1; ta_kmh8 = 0; au_hook = tr_audio; prev_t = fm1_ms ? fm1_ms : 1u; }
            }
            if (prev_t && fm1_ms - prev_t > 16500u) { loop_event_end(); prev_t = 0; }   /* (two times round, then quiet) */
            sig_draw(sg, cur, asked, prev_t != 0u);
            ui_footer(asked ? T_SIG_SURE : T_SIG_FOOT);
            led_btn_mask = BIT(BTN_REC) | (full ? BIT(BTN_PLAY) | BIT(BTN_SEL) : 0u);
            led_btn_blink = asked ? BIT(BTN_SEL) : 0u;
            for (k = 0; k < 27u; k++) {         /* the keys that are in key glow; the one at the cursor lights */
                static const uint8_t DM[12] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0};
                fm1_led_key_glow(KEY_LED(k), DM[(53u + k) % 12u]);
                fm1_led_key(KEY_LED(k), 53u + k == sg[cur]);
            }
            if (full && (PRESSED(BTN_SEL) || (tr_autoplay && el > 1500u))) {
                if (!asked && !tr_autoplay) { asked = 1; sfx_click(0, 60, 128); }
                else {                          /* kept: the line's melody from now on */
                    if (prev_t) { loop_event_end(); prev_t = 0; }
                    for (k = 0; k < 8u; k++) save.sig[k] = sg[k];
                    mu_build_sig(save.sig);
                    save_mark(); sfx_motif(au_time()); ui_keys_clear(); led_btn_blink = 0;
                    st.unl_feat &= ~(uint32_t)F_SYNTH;
                    st.unl_phase = st.unl_feat ? 0u : l < NTRLEGS ? 1u : 2u;
                    if (st.unl_phase == 2u) chapter_next(); else go(S_UNLOCK);
                }
            }
            break;
        }
        if (st.unl_phase == 3u) {               /* the loop made whole */
            loop_event(el);
            loop_draw(el);
            ui_footer(el > 8000u ? T_PLAY_ON : "");
            if (el > 8000u && (PRESSED(BTN_PLAY) || (tr_autoplay && el > 11000u)) ) {
                loop_event_end(); st.unl_phase = 2;
                if (st.unl_chap == 255u) go_map(); else chapter_next();
            }
            break;
        }
        {   /* the keyboard: pulses running across it */
            uint32_t k;
            for (k = 0; k < 27u; k++) { uint32_t p2 = (fm1_ms / 30u + 27u - k) % 27u; fm1_led_key(KEY_LED(k), p2 < 3u); fm1_led_key_glow(KEY_LED(k), p2 < 8u); }
        }
        if (st.unl_phase == 0u) {
            while (bit < 0x100u && !(st.unl_feat & bit)) bit <<= 1;
            bp_draw(save.features & ~bit, bit);
        } else {
            static char lb[40];
            char *q = str_cat(lb, T_UNLOCK_LINE); q = str_cat(q, " "); str_cat(q, TR_STATION[TR_LEGS[l].st_to]);
            map_draw(TR_LEGS[l].st_from, (int32_t)l, lb);   /* (the line itself, its train running: no name flashing) */
        }
        ui_footer(el > 800u ? T_PLAY_ON : "");
        if (el > 800u && (PRESSED(BTN_PLAY) || el > 3800u || (tr_autoplay && el > 1500u))) {
            if (st.unl_phase == 0u) {           /* the next part; then the lines, if it opened any */
                uint32_t b2 = 1;
                while (b2 < 0x100u && !(st.unl_feat & b2)) b2 <<= 1;
                st.unl_feat &= ~b2;
                if (b2 == F_SYNTH && !save.sig[0]) st.unl_phase = 4u;   /* the cab synth: now your melody */
                else if (!st.unl_feat) st.unl_phase = l < NTRLEGS ? (loop_whole_now() ? 3u : 1u) : 2u;
            } else st.unl_phase = (st.unl_phase == 1u && loop_whole_now()) ? 3u : 2u;
            if (st.unl_phase == 2u) { if (l < NTRLEGS) { st.map_st = TR_LEGS[l].st_from; } chapter_next(); }
            else go(S_UNLOCK);
        }
        break;
    }
    case S_HEART: {                             /* the finale: every loop station's line grows into the heart; then turn it */
        static int32_t off, last_al;
        static uint32_t won_t, init2;
        uint32_t el = fm1_ms - st.t_state, al = 0, k;
        int32_t e4 = enc_main() + in.enc[ENC_ALGORITHM] + in.enc[ENC_PRESETS] + in.enc[ENC_KNOB2];
        if (init2 != st.t_state) { init2 = st.t_state; off = 13 + (int32_t)rndn(40); won_t = 0; last_al = 0; }
        if (el < 30u) { au_hook = 0; mu_set_song(7); mu.lvl = 0; ta_music = 0; ta_kmh8 = 0; mu_master = 65536; mu_fade = 0; au_hook = tr_audio; }
        if (el > 5500u && !won_t) {            /* the turning: 64 steps a turn */
#ifdef DINGHY_HOST
            if (tr_autoplay && el > 7000u && (el / 100u) % 2u) e4 = off < 32 ? 1 : -1;
#endif
            if (e4) { off = ((off - e4) % 64 + 64) % 64; sfx_click(0, 40, 128); sfx_tick(0, 50, 20, 128, 0); }
        }
        al = heart_aligned(off);
        if ((int32_t)al > last_al && el > 5500u) sfx_chime(72 + (int32_t)al * 2, 60, 0, 128);   /* (a notch meets its line) */
        last_al = (int32_t)al;
        if (!won_t && al >= 7u) { won_t = fm1_ms ? fm1_ms : 1u; sfx_motif(au_time()); ta_music = 1; }   /* it opens; its song (yours) */
        if (el > 5500u && !won_t && (el % 1000u) < 40u) { sfx_thunk(0, 30, 60, 128); sfx_thunk(au_time() + ms_frames(260), 28, 45, 128); }
        heart_draw(el, off, won_t ? fm1_ms - won_t : 0u);
        ui_footer(won_t && fm1_ms - won_t > 6000u ? T_PLAY_ON : el > 5500u && !won_t ? T_HEART_FOOT : "");
        for (k = 0; k < 27u; k++) { uint32_t ph = fm1_ms % 1000u; fm1_led_key(KEY_LED(k), won_t ? ((fm1_ms / 80u + k) % 9u) < 2u : (ph < 110u || (ph > 230u && ph < 340u))); fm1_led_key_glow(KEY_LED(k), won_t != 0u); }
        led_btn_mask = won_t ? BIT(BTN_PLAY) : 0u;
        if (won_t && fm1_ms - won_t > 6000u && (PRESSED(BTN_PLAY) || (tr_autoplay && fm1_ms - won_t > 9000u))) {
            ta_music = 0; au_hook = 0; ui_keys_clear(); au_cut(0); go(S_END);
        }
        break;
    }
    case S_END: {
        uint32_t el = fm1_ms - st.t_state;
        if (!save.finished) { save.finished = 1; save_mark(); }
        if (el < 40u) {                         /* the run to the heart's song, at its end, fading back in under the credits */
            au_hook = 0; mu_set_song(7); mu_hp = 256; mu.lvl = 0; ta_kmh8 = 0; ta_load = 0; ta_lead_note = 0;
            mu.stp = 44100u * 15u / 120u; mu_master = 0; mu_fade = 40; ta_music = 1; au_hook = tr_audio;
        }
        end_draw(el);
        if ((el > 3000u && (PRESSED(BTN_PLAY) || in.kdown)) || el > 6000u + (uint32_t)NCREDITS * 900u) {   /* (the song fades away into the title) */
            mu_fade = -160; st.sel = 0; st.menu = 1; st.msel = 2; go(S_TITLE);
        }
        break;
    }
    }
#ifdef DINGHY_HOST
    host_screens();
#endif
    g_present();
    ui_leds_buttons();
}


#ifdef DINGHY_HOST
/* (host: OBL_SHOTS=dir saves the screen once at each moment worth showing: tests/all_screens.py makes the sheets) */
static void host_screens(void)
{
    static const char *const SN[] = {"title", "story", "help", "play", "won", "storyout", "end", "unlock", "heart"};
    static const char *dir;
    static uint32_t last = 0xFFFFFFFFu, t0, shot, n;
    uint32_t key, sub = 0, delay;
    if (!dir) { dir = getenv("OBL_SHOTS"); if (!dir) dir = ""; }
    if (!*dir) return;
    if (st.s == S_PLAY && cur_ch->type == MECH_TRAVEL)
        sub = tr.done ? 7u : tr.prep == TRP_TUNE && tr.ph_state == 0u ? 8u : tr.prep < TRP_READY ? tr.prep : tr.t > 25u * TR_HZ ? 6u : 5u;
    if (st.s == S_WON) sub = fm1_ms - st.t_state > ch_game(st.chap)->plaque_ms + 300u ? 1u : 0u;
    if (st.s == S_STORY_IN || st.s == S_STORY_OUT) sub = st.page;
    if (st.s == S_UNLOCK) sub = st.unl_phase;
    key = st.s | (st.chap << 4) | (sub << 10) | ((uint32_t)st.travel << 14) | ((uint32_t)(cur_ch - CHAP) << 16);
    if (key != last) { last = key; t0 = fm1_ms; shot = 0; }
    delay = st.s == S_TITLE ? 450u : st.s == S_HELP ? 700u : st.s == S_WON ? (sub ? 250u : 1200u) : st.s == S_END ? 2500u
          : st.s == S_PLAY ? (cur_ch->type == MECH_TRAVEL ? (sub == 8u ? 1000u : sub < 5u ? 250u : sub == 7u ? 1100u : 400u) : 1300u) : 150u;
    if (st.s == S_STORY_IN || st.s == S_STORY_OUT) {   /* a page: once its text is all typed */
        uint32_t pn2; const page_t *pg2 = cur_pages(&pn2);
        if (st.page < pn2 && !st.typed_skip && (fm1_ms - st.t_page) / 28u < mo_strlen(pg2[st.page].text) + 2u) t0 = fm1_ms;
    }
    if (st.s == S_PLAY && cur_ch->type == MECH_TRAVEL && sub < 5u && sub != 1u && !((fm1_ms / 350u) & 1u)) return;   /* (a prompt: lit) */
    if (!shot && fm1_ms - t0 >= delay) {
        char name[300], t[64], *q = t;
        const char *a = cur_ch->name;
        char route[48];
        if (st.travel) {
            if (tr_mode == TRM_ENDLESS) snprintf(route, sizeof route, "endless");
            else if (tr_mode == TRM_ENODEN) snprintf(route, sizeof route, "enoden-%s-to-%s", EN_STATION[EN_LEGS[st.leg].st_from], EN_STATION[EN_LEGS[st.leg].st_to]);
            else snprintf(route, sizeof route, "%s-to-%s", TR_STATION[TR_LEGS[st.leg].st_from], TR_STATION[TR_LEGS[st.leg].st_to]);
            a = route;
        }
        uint32_t i;
        shot = 1;
        for (; *a && q < t + 40; a++) *q++ = (char)((*a >= 'A' && *a <= 'Z') ? *a - 'A' + 'a' : (*a >= '0' && *a <= '9') || (*a >= 'a' && *a <= 'z') ? *a : '-');
        *q = 0;
        snprintf(name, sizeof name, "%s/%04u_%s_%u_%s.ppm", dir, n++, SN[st.s], sub, st.s == S_TITLE ? (save.finished && st.menu ? "menu" : st.enoden ? "enodenmap" : save.features & F_KEY ? "map" : "prekey") : t);
        {
            FILE *f = fopen(name, "wb");
            if (!f) return;
            fprintf(f, "P6 240 240 255\n");
            for (i = 0; i < 240u * 240u; i++) {
                uint16_t c = swap16(cv_px[i]);
                fputc((c >> 11) * 255 / 31, f); fputc((c >> 5 & 63) * 255 / 63, f); fputc((c & 31) * 255 / 31, f);
            }
            fclose(f);
        }
    }
}
#endif

static void app_main(void)
{
    uint32_t t_frame = 0;
#ifdef DINGHY_HOST
    tr_autoplay = getenv("DENSHA_AUTOPLAY") != 0;   /* (host: the autopilot drives, and plays the map) */
#endif
    if (settings_load(&save, sizeof save) || save.magic != MO_MAGIC) {
        memset(&save, 0, sizeof save);
        save.magic = MO_MAGIC;
    }
    st.map_st = ST_START; st.map_dest = -1;
    st.menu = save.finished;
    if (save.sig[0]) mu_build_sig(save.sig);   /* (the heart's song: the player's melody) */
#ifdef DINGHY_HOST
    if (getenv("OBL_MENU")) { save.finished = 1; save.features = 0xFF; st.menu = 1; st.msel = (uint32_t)atoi(getenv("OBL_MENU")); }   /* (host: as if finished, a menu entry chosen) */
#endif
    st.boot = 1; st.boot_t = fm1_ms;            /* (the loading screen first; where they were stays kept till it ends) */
    go_map();
    for (;;) {
        platform_poll();
        kb_service();
        if (fm1_ms - t_frame >= 33u) {          /* ~30 frames a second */
            t_frame = fm1_ms;
            frame();
        }
        save_service();
    }
}
