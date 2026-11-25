/* SPDX-License-Identifier: GPL-3.0-only */
/* Mechanica Obscura: the story. Pages of a vignette and a few typed lines; the machines speak in capitals,
 * in blue, with a lower tick. */

/* The pages, the chapters and every word come from content/script.txt and content/puzzles.txt (tools/content.py
 * -> mo_content.h: CHAP[], page_t, CREDITS[], T_* texts). A page's art is a backdrop id (BG_ART_*). */

/* which game draws each mechanism type (MECH_* from puzzles.txt's `type`); story chapters have none */
static const game_t *const MECH_GAME[MECH_COUNT] = MECH_GAMES;   /* (from the types in tools/content.py) */

/* ---- the vignettes: baked backdrops (BG_ART_* follow the ART_* order), with a few live lights: beacons,
 * lenses, lamps, the patrol */
typedef struct { int16_t art; uint8_t x, y, r; uint16_t c; uint16_t period; } art_light_t;   /* art: BG_ART_* */
static const art_light_t ART_LIGHTS[] = {   /* (x, y in the art's own band: drawn 16 px lower, under the header) */
    {BG_ART_PLATFORM, 160, 40, 5, C_GLOW, 2100}, {BG_ART_PLATFORM, 70, 21, 3, C_CYAN, 900},
    {BG_ART_TRAIN, 135, 91, 3, C_CREAM, 1700}, {BG_ART_TRAIN, 165, 91, 3, C_CREAM, 1700},
    {BG_ART_CAB, 142, 100, 4, C_ROSE, 1300}, {BG_ART_SIGNALBOX, 180, 44, 3, C_RUBY, 700}, {BG_ART_SIGNALBOX, 100, 37, 3, C_ROSE, 1100},
    {BG_ART_WORKSHOP, 150, 62, 6, C_GLOW, 2600}, {BG_ART_MAP, 120, 62, 6, C_RUBY, 1500}, {BG_ART_TIMETABLE, 60, 90, 3, C_ROSE, 400},
    {BG_ART_POCKETWATCH, 120, 106, 3, C_ROSE, 1000}, {BG_ART_HARBOUR, 20, 20, 2, C_RUBY, 900}, {BG_ART_HARBOUR, 60, 8, 2, C_RUBY, 1300},
    {BG_ART_ARCHIVE, 122, 99, 4, C_GOLD, 1800}, {BG_ART_TUNNEL, 120, 30, 4, C_CYAN, 1200}, {BG_ART_OBSERVATORY, 112, 18, 3, C_GLOW, 2400},
    {BG_ART_GHOST, 120, 70, 12, C_CYAN, 2800}, {BG_ART_DOOR, 120, 40, 8, C_GOLD, 2200}, {BG_ART_HEART, 120, 60, 9, C_ROSE, 1000},
    {BG_ART_KEEPER, 135, 86, 4, C_GLOW, 1600},
};
static void art_lights(int32_t art)
{
    uint32_t i;
    for (i = 0; i < sizeof ART_LIGHTS / sizeof ART_LIGHTS[0]; i++) {
        const art_light_t *l = &ART_LIGHTS[i];
        uint32_t ph, a;
        if (l->art != art) continue;
        ph = (fm1_ms % l->period) * 65536u / l->period;
        a = (uint32_t)(mo_sin(ph) + 32768) >> 9;   /* 0..128: a slow pulse */
        {
            int32_t y = l->y + 16;                  /* (the art is baked 16 px down, below the header) */
            g_disc(l->x, y, l->r + 2, mix565(px_get(l->x, y), l->c, a / 2));
            g_disc(l->x, y, l->r / 2 + 1, mix565(px_get(l->x, y), l->c, 64 + a));
        }
    }
}

/* a page: the vignette, the words typed onto a lacquer panel, a mark to go on */
#define STORY_WB 168                            /* the picture's window ends here; the words below */
static uint8_t story_heart;                     /* the heart's pages (the main loop sets it) */
static void story_draw(const page_t *pg, int32_t shown, int done, uint32_t pi, uint32_t pn)
{
    if (story_heart) {                          /* the heart: the picture bare, a pulse at its edges, red words on black */
        uint32_t ph = fm1_ms % 1000u, beat = ph < 140u || (ph > 260u && ph < 400u);
        int32_t k;
        g_bg(pg->art);
        g_rect(0, 0, SW, 22, RGB(6, 0, 2));
        g_rect(0, STORY_WB, SW, SH - STORY_WB, RGB(6, 0, 2));
        for (k = 0; k < 12; k++) {              /* the pulse */
            uint32_t a = (uint32_t)(12 - k) * (beat ? 16u : 8u);
            g_rect_a(0, 22 + k, SW, 1, RGB(200, 0, 30), a); g_rect_a(0, STORY_WB - 1 - k, SW, 1, RGB(200, 0, 30), a);
            g_rect_a(k, 22, 1, STORY_WB - 22, RGB(200, 0, 30), a); g_rect_a(SW - 1 - k, 22, 1, STORY_WB - 22, RGB(200, 0, 30), a);
        }
        g_text_wrap(14, STORY_WB + 10, 212, 14, &FONT_S, pg->text, shown, RGB(255, 80, 100));
        (void)done; (void)pi; (void)pn;
        return;
    }
    uint32_t i;
    int32_t j;
    g_bg(pg->art);
    art_lights(pg->art);
    for (j = 0; j < STORY_WB - 22; j++) {       /* round the picture's corners: a window in the cab's wall */
        int32_t k = g_rr_inset(j, STORY_WB - 22, 9) + 4;
        g_rect(0, 22 + j, k, 1, C_NIGHT);
        g_rect(SW - k, 22 + j, k, 1, C_NIGHT);
    }
    g_rframe(4, 22, SW - 8, STORY_WB - 22, 9, 2, pg->robot ? C_CYAN_D : C_ROSE_D);
    g_rect(0, STORY_WB, SW, STAGE_Y1 - STORY_WB, C_NIGHT);
    g_cab_panel(4, STORY_WB + 3, SW - 8, STAGE_Y1 - STORY_WB - 4, pg->robot ? C_CYAN : C_ROSE);   /* the words: short, so a short box */
    g_text_wrap(14, STORY_WB + 7, 212, 14, &FONT_S, pg->text, shown, pg->robot ? C_CYAN : C_CREAM);
    for (i = 0; i < pn; i++)                     /* the pages, as beads */
        g_disc(SW - 10 - (int32_t)(pn - 1u - i) * 8, STAGE_Y1 + 9, 2, i == pi ? C_ROSE : C_PAPER3);
    if (done && (fm1_ms / 400u) & 1u) {
        int32_t q2[6] = {6 * 16, (STAGE_Y1 + 5) * 16, 12 * 16, (STAGE_Y1 + 9) * 16, 6 * 16, (STAGE_Y1 + 13) * 16};
        g_tri16(q2[0], q2[1], q2[2], q2[3], q2[4], q2[5], C_CYAN);
    }
}

/* ---- the title: Obscura Line. A synthwave sky (the striped sun, the mountain, a neon grid running toward it, a
 * train crossing), then the line's map: the stations round the loop, restored / current / locked, and the
 * chapter chosen on its plate */
/* ---- the title: the loop. The line drawn in neon, as on the map: seven stations, a light running round it, and
 * something at its heart, beating (docs/style/title_options.png, option 2) */
#define TL_N 120u                               /* points round the loop */
static int16_t tl_x[TL_N], tl_y[TL_N];          /* (x16: sixteenths of a pixel) */
static uint32_t tl_text_a = 256u;               /* the name's light, 0..256 (power-on: 0, the loop alone, then up) */
static void tl_glow(int32_t cx, int32_t cy, int32_t r, uint16_t c, uint32_t a)   /* a soft round light, a 0..256 at its centre */
{
    int32_t dx, dy, r2 = r * r;
    for (dy = -r; dy <= r; dy++)
        for (dx = -r; dx <= r; dx++) {
            int32_t d2 = dx * dx + dy * dy;
            if (d2 < r2) px_blend(cx + dx, cy + dy, c, a * (uint32_t)(r2 - d2) / (uint32_t)r2);
        }
}
static void title_scene(uint32_t t)
{
    const int32_t CX = 120, CY = 146, RX = 92, RY = 56;
    uint32_t k, j, head, ph, beat;
    int32_t x, y;
    if (!tl_x[0]) for (k = 0; k < TL_N; k++) {  /* the loop's shape, once: an oval, squeezed a little three ways */
        uint32_t a = k * (0xFFFFFFFFu / TL_N);
        int32_t c = ta_sine(a + 0x40000000u), sn = ta_sine(a), s3 = ta_sine(a * 3u), c2 = ta_sine(a * 2u + 0x40000000u);
        tl_x[k] = (int16_t)(CX * 16 + (int32_t)(((int64_t)RX * 16 * c * (32768 + (2621 * s3 >> 15))) >> 30));
        tl_y[k] = (int16_t)(CY * 16 + (int32_t)(((int64_t)RY * 16 * sn * (32768 + (3277 * c2 >> 15))) >> 30));
    }
    g_rect(0, 0, SW, SH, RGB(8, 4, 22));
    for (y = 70; y < STAGE_Y1; y += 8)          /* a faint survey grid */
        for (x = 4; x < SW; x += 8) px_set(x, y, RGB(40, 20, 60));
    for (k = 0; k < TL_N; k++) tl_glow(tl_x[k] >> 4, tl_y[k] >> 4, 6, RGB(170, 20, 140), 70);   /* the line: a magenta haze */
    for (k = 0; k < TL_N; k++) {                /* and the neon in it */
        uint32_t n = (k + 1u) % TL_N;
        g_line(tl_x[k] >> 4, tl_y[k] >> 4, tl_x[n] >> 4, tl_y[n] >> 4, C_CYAN);
        g_line(tl_x[k] >> 4, (tl_y[k] >> 4) + 1, tl_x[n] >> 4, (tl_y[n] >> 4) + 1, mix565(C_CYAN, RGB(8, 4, 22), 120));
    }
    for (k = 0; k < 7u; k++) {                  /* the stations */
        uint32_t i = k * TL_N / 7u + 2u;
        g_disc(tl_x[i] >> 4, tl_y[i] >> 4, 4, C_CREAM);
        g_disc(tl_x[i] >> 4, tl_y[i] >> 4, 3, RGB(8, 4, 22));
    }
    head = (t / 70u) % TL_N;                    /* the train: a light going round, once in 8.4 s, and its trail */
    for (j = 22u; j-- > 0u;) {
        uint32_t i = (head + TL_N - j) % TL_N;
        int32_t r = 3 - (int32_t)j / 8;
        g_disc(tl_x[i] >> 4, tl_y[i] >> 4, r < 1 ? 1 : r, mix565(RGB(255, 230, 250), RGB(8, 4, 22), j * 256u / 24u));
    }
    tl_glow(tl_x[head] >> 4, tl_y[head] >> 4, 12, RGB(255, 180, 240), 150);
    ph = t % 1000u; beat = ph < 140u ? 256u - ph : ph > 260u && ph < 400u ? 200u - (ph - 260u) : 60u;   /* the heart: lub-dub */
    tl_glow(CX, CY, 12 + (int32_t)beat / 48, RGB(255, 40, 120), 50u + beat / 3u);
    g_disc(CX, CY, 5, C_ROSE);
    g_disc(CX, CY, 2, RGB(255, 230, 250));
    if (tl_text_a) {                            /* the name, big, with a magenta shadow (faded up out of the night) */
        uint32_t a = tl_text_a > 256u ? 256u : tl_text_a;
        g_deco_c(122, 14, 34, "OBSCURA", mix565(RGB(8, 4, 22), C_ROSE, a));
        g_deco_c(120, 12, 34, "OBSCURA", mix565(RGB(8, 4, 22), C_CREAM, a));
        g_deco_c(120, 56, 12, "L I N E", mix565(RGB(8, 4, 22), C_CYAN, a));
    }
}
/* ---- the map: where you choose everything. Stations reached, restored (their puzzles solved), the lines driven and
 * their best tiers; the hidden station appears once every other station and line is done */
static uint8_t dev_mode;                        /* FX + GLO held 3 s: every station reached, every line open, every part on (not saved) */
#define ST_START (CHAP_STATION[0] < NSTATION ? CHAP_STATION[0] : 0u)
static int map_reached(uint32_t s)
{
    if (dev_mode) return s < NSTATION;
    uint32_t l;
    if (s == ST_START) return 1;
    for (l = 0; l < NTRLEGS; l++) if ((save.driven >> l & 1u) && TR_LEGS[l].st_to == s) return 1;
    return 0;
}
static int map_restored(uint32_t s)             /* every puzzle (and story) at the station done */
{
    uint32_t i;
    for (i = 0; i < NCHAP; i++)
        if (CHAP_STATION[i] == s && CHAP[i].type != MECH_TRAVEL && !(save.done & (1u << i))) return 0;
    return map_reached(s);
}
static int map_edge_driven(uint32_t a, uint32_t b)
{
    uint32_t l;
    for (l = 0; l < NTRLEGS; l++)
        if ((save.driven >> l & 1u) && ((TR_LEGS[l].st_from == a && TR_LEGS[l].st_to == b) || (TR_LEGS[l].st_from == b && TR_LEGS[l].st_to == a))) return 1;
    return 0;
}
static int map_secret_open(void)                /* every visible station restored and every line between them driven */
{
    uint32_t s, l;
    for (s = 0; s < NSTATION; s++) if (!TR_STPOS[s].hidden && !map_restored(s)) return 0;
    for (l = 0; l < NTRLEGS; l++) {
        const tr_leg_t *g = &TR_LEGS[l];
        if (TR_STPOS[g->st_from].hidden || TR_STPOS[g->st_to].hidden) continue;
        if (!map_edge_driven(g->st_from, g->st_to)) return 0;
    }
    return 1;
}
static int map_visible(uint32_t s) { return s < NSTATION && (dev_mode || !TR_STPOS[s].hidden || map_secret_open()); }
static int map_leg_open(uint32_t l)             /* reached, its far end on the map, and opened by its chapter (if one opens it) */
{
    if (dev_mode) return l < NTRLEGS;
    return l < NTRLEGS && map_restored(TR_LEGS[l].st_from) && map_visible(TR_LEGS[l].st_to) && (save.features & F_KEY)
        && (TR_LEG_GATE[l] == 255u || (save.done >> TR_LEG_GATE[l] & 1u));
}
static uint32_t map_edge_tier(uint32_t a, uint32_t b)
{
    uint32_t l, t = 0;
    for (l = 0; l < NTRLEGS; l++)
        if (((TR_LEGS[l].st_from == a && TR_LEGS[l].st_to == b) || (TR_LEGS[l].st_from == b && TR_LEGS[l].st_to == a)) && save.tier[l] > t) t = save.tier[l];
    return t;
}
static uint32_t map_adj(uint32_t s, uint8_t *out)   /* the legs out of a station, to stations on the map */
{
    uint32_t l, n = 0;
    for (l = 0; l < NTRLEGS && n < 8u; l++) if (TR_LEGS[l].st_from == s && map_visible(TR_LEGS[l].st_to)) out[n++] = (uint8_t)l;
    return n;
}
static uint32_t map_puzzles_left(uint32_t s)
{
    uint32_t i, n = 0;
    for (i = 0; i < NCHAP; i++) if (CHAP_STATION[i] == s && CHAP[i].type != MECH_TRAVEL && CHAP[i].type != MECH_STORY && !(save.done & (1u << i))) n++;
    return n;
}
static uint32_t map_progress(void)              /* stations restored and lines driven, of all of them, in % */
{
    uint32_t s, l, a = 0, b = 0;
    for (s = 0; s < NSTATION; s++) { b++; if (map_restored(s)) a++; }
    for (l = 0; l < NTRLEGS; l++) {
        uint32_t q, dup = 0;
        for (q = 0; q < l; q++) if (TR_LEGS[q].st_from == TR_LEGS[l].st_to && TR_LEGS[q].st_to == TR_LEGS[l].st_from) dup = 1;
        if (dup) continue;
        b++;
        if (map_edge_driven(TR_LEGS[l].st_from, TR_LEGS[l].st_to)) a++;
    }
    return b ? a * 100u / b : 0u;
}
static void map_xy(uint32_t s, int32_t *x, int32_t *y) { *x = 16 + TR_STPOS[s].x * 208 / 100; *y = 26 + TR_STPOS[s].y * 162 / 100; }

static void map_draw(uint32_t cur, int32_t leg, const char *msg)
{
    uint32_t l, s;
    int32_t x, y;
    char b[40], *p;
    /* the band: the name, a sliver of the synthwave dusk, the progress */
    for (y = 0; y < 18; y++) g_rect(0, y, SW, 1, mix565(RGB(18, 8, 48), RGB(190, 50, 140), (uint32_t)y * 256u / 18u));
    for (y = -8; y <= 0; y++) {
        int32_t hw = (int32_t)mo_isqrt((uint32_t)(64 - y * y));
        if (y > -4 && (y & 1)) continue;
        g_rect(222 - hw, 17 + y, hw * 2, 1, mix565(RGB(255, 220, 110), RGB(255, 60, 170), (uint32_t)(y + 8) * 30u));
    }
    g_deco(6, 3, 11, "OBSCURA", C_CREAM);
    g_text(deco_w("OBSCURA", 11) + 11, 1, &FONT_S, "LINE", C_CYAN);
    p = b; fmt_u(p, map_progress()); while (*p) p++; str_cat(p, "%");
    g_text_r(204, 1, &FONT_S, b, C_CREAM);
    if (dev_mode) g_text_r(160, 1, &FONT_S, "DEV", C_GOLD);
    g_rect(0, 18, SW, 1, C_ROSE);
    g_rect(0, 19, SW, 1, C_NIGHT); g_rect(0, 184, SW, 2, C_NIGHT);   /* (the gaps between the panels: nothing left over) */
    /* the network */
    g_cab_panel(2, 20, SW - 4, 164, C_ROSE_D);
    for (l = 0; l < NTRLEGS; l++) {             /* every line once, driven bright, not yet faint, its best tier on it */
        uint32_t a = TR_LEGS[l].st_from, c = TR_LEGS[l].st_to, q, dup = 0, t;
        int32_t xa, ya, xc, yc, steps, j, lit;
        for (q = 0; q < l; q++) if (TR_LEGS[q].st_from == c && TR_LEGS[q].st_to == a) dup = 1;
        if (dup || !map_visible(a) || !map_visible(c)) continue;
        map_xy(a, &xa, &ya); map_xy(c, &xc, &yc);
        lit = map_edge_driven(a, c);
        steps = (iabs(xc - xa) + iabs(yc - ya)) / 3 + 1;
        for (j = 0; j <= steps; j++) {
            int32_t px = xa + (xc - xa) * j / steps, py = ya + (yc - ya) * j / steps;
            uint16_t col = TR_LEG_LOOP[l] ? mix565(C_CYAN, C_ROSE, (uint32_t)j * 256u / (uint32_t)(steps + 1)) : TR_STPOS[TR_LEGS[l].st_to].hidden || TR_STPOS[TR_LEGS[l].st_from].hidden ? C_ROSE : C_AMBER;
            if (TR_LEGS[l].ride) {              /* a ride: a zigzag in its own colour (gold the bay, pink the festival) */
                int32_t nx = -(yc - ya), ny = xc - xa, ln = (int32_t)mo_isqrt((uint32_t)(nx * nx + ny * ny)) + 1, zz = (j % 6) < 3 ? (j % 6) : 6 - (j % 6);
                col = TR_LEGS[l].ride == 1u ? RGB(255, 214, 120) : RGB(255, 130, 200);
                zz = zz * 2 - 3;
                px += nx * zz / ln; py += ny * zz / ln;
                if (lit) g_disc(px, py, 1, col);
                else if (!(j & 1)) px_set(px, py, mix565(C_PANEL, col, 150));
                continue;
            }
            if (lit) g_disc(px, py, 1, col);
            else if (!(j & 1)) px_set(px, py, mix565(C_PANEL, col, 110));
        }
        if ((t = map_edge_tier(a, c)) != 0) {
            char tt[2] = {TR_TIER[t], 0};
            int32_t mx = (xa + xc) / 2, my = (ya + yc) / 2;
            g_rrect(mx - 6, my - 8, 13, 16, 3, C_PANEL2);
            g_text_c(mx + 1, my - 8, &FONT_S, tt, t >= 7 ? C_GOLD : t >= 5 ? C_CYAN : t >= 3 ? C_CREAM : C_ROSE);
        }
    }
    if (leg >= 0) {                             /* the chosen line: bright, the train running along it */
        int32_t xa, ya, xc, yc, j, k;
        uint32_t ph = (fm1_ms / 8u) % 256u;
        map_xy(TR_LEGS[leg].st_from, &xa, &ya); map_xy(TR_LEGS[leg].st_to, &xc, &yc);
        for (j = 0; j <= 32; j++) g_disc(xa + (xc - xa) * j / 32, ya + (yc - ya) * j / 32, 2, map_leg_open((uint32_t)leg) ? C_ROSE : C_PAPER3);
        for (k = 0; k < 3; k++) {               /* three cars, nose first */
            int32_t f = (int32_t)ph - k * 14;
            if (f < 0) continue;
            g_rrect(xa + (xc - xa) * f / 256 - 3, ya + (yc - ya) * f / 256 - 3, 7, 7, 2, k ? C_CREAM : C_CYAN);
        }
    }
    for (s = 0; s < NSTATION; s++) {            /* the stations */
        int r = map_reached(s), rs = map_restored(s);
        int sel = s == cur && leg < 0;          /* (a line chosen: no station lit, the line is the choice) */
        uint16_t c = sel ? C_ROSE : rs ? C_CYAN : r ? C_GOLD : C_PAPER3;
        if (!map_visible(s)) continue;
        map_xy(s, &x, &y);
        if (sel) g_disc(x, y, 8 + (int32_t)((fm1_ms / 300u) & 1u), mix565(C_PANEL, C_ROSE, 90));
        if (TR_STPOS[s].hidden) {               /* the heart: not a station, a beat */
            uint32_t ph = fm1_ms % 1000u;
            int32_t rr = 5 + (ph < 120u || (ph > 240u && ph < 360u) ? 2 : 0);
            g_disc(x - rr / 2, y - 1, rr / 2 + 2, RGB(220, 20, 50)); g_disc(x + rr / 2, y - 1, rr / 2 + 2, RGB(220, 20, 50));
            { int32_t q3[6] = {(x - rr - 1) * 16, y * 16, (x + rr + 1) * 16, y * 16, x * 16, (y + rr + 3) * 16}; g_tri16(q3[0], q3[1], q3[2], q3[3], q3[4], q3[5], RGB(220, 20, 50)); }
            continue;
        }
        g_rrect(x - 4, y - 4, 9, 9, 2, r ? c : C_PANEL);
        g_rframe(x - 5, y - 5, 11, 11, 3, 1, c);
    }
    if (leg < 0 && cur < NSTATION && map_visible(cur)) {   /* a station chosen: its name over the map (a line: none) */
        uint32_t d = cur;
        int32_t tw = text_w(&FONT_S, TR_STATION[d]), tx, ty;
        map_xy(d, &x, &y);
        tx = clampi(x - tw / 2, 8, SW - 8 - tw); ty = y + 7 > 166 ? y - 24 : y + 7;
        g_rrect(tx - 3, ty, tw + 6, 16, 3, C_PANEL2);
        g_text(tx, ty, &FONT_S, TR_STATION[d], C_CREAM);
    }
    /* the panel: the station, and what PLAY will do */
    g_cab_panel(2, 186, SW - 4, 35, C_ROSE);
    if (cur < NSTATION) {
        uint32_t left = map_puzzles_left(cur);
        g_text(10, 189, &FONT_S, TR_STATION[cur], C_CREAM);
        p = b;
        if (!map_reached(cur)) p = str_cat(p, "NOT REACHED");
        else if (map_restored(cur)) p = str_cat(p, "RESTORED");
        else { fmt_u(p, left); while (*p) p++; p = str_cat(p, left == 1u ? " PUZZLE" : " PUZZLES"); }
        g_text_r(SW - 10, 189, &FONT_S, b, map_restored(cur) ? C_CYAN : C_AMBER);
        if (msg) g_text_c(SW / 2, 204, &FONT_S, msg, C_ROSE);
        else if (leg >= 0) {
            uint32_t t = save.tier[leg];
            p = str_cat(b, TR_LEGS[leg].line);
            if (t) { p = str_cat(p, "  BEST "); *p++ = TR_TIER[t]; *p = 0; }
            g_text_c(SW / 2, 204, &FONT_S, map_leg_open((uint32_t)leg) ? b : T_MAP_LOCKED, map_leg_open((uint32_t)leg) ? C_INK2 : C_ROSE);
        } else g_text_c(SW / 2, 204, &FONT_S, map_reached(cur) ? (map_restored(cur) ? T_MAP_REPLAY : T_MAP_SOLVE) : "", C_INK2);
    }
}

/* ---- the train's blueprint: what it has. A side view in neon on blueprint blue; each part greyed until a puzzle
 * puts it on the train, then lit, named, with the control that works it. hi: a part just added, pulsing */
typedef struct { uint8_t bit, lx, ly, w, ax, ay; const char *ctl; } bp_part_t;   /* (w: the box's width, to its words) */
static const bp_part_t BP_PARTS[8] = {          /* the label box (top row y 30, bottom 146) and where on the train */
    {F_KEY, 6, 30, 50, 204, 118, "PLAY"}, {F_HORN, 65, 30, 50, 206, 76, "REC"}, {F_SYNTH, 124, 30, 50, 184, 104, "KEYS"}, {F_VOICE, 183, 30, 50, 178, 112, "SEQ"},
    {F_WHISTLE, 6, 148, 64, 168, 76, "ARP"}, {F_STOPWATCH, 77, 148, 50, 194, 94, "TIME"}, {F_CALL, 134, 148, 42, 214, 100, "SEL"},
    {F_LIGHTS, 184, 148, 50, 226, 126, "FX"}};   /* (the boxes spread evenly along each row) */
static const char *bp_name(uint32_t bit)
{
    return bit == F_KEY ? T_BP_KEY : bit == F_STOPWATCH ? T_BP_STOPWATCH : bit == F_HORN ? T_BP_HORN : bit == F_WHISTLE ? T_BP_WHISTLE
         : bit == F_LIGHTS ? T_BP_LIGHTS : bit == F_CALL ? T_BP_CALL : bit == F_VOICE ? T_BP_VOICE : T_BP_SYNTH;
}
static const char *bp_text(uint32_t bit)
{
    return bit == F_KEY ? T_UNLOCK_KEY : bit == F_STOPWATCH ? T_UNLOCK_STOPWATCH : bit == F_HORN ? T_UNLOCK_HORN : bit == F_WHISTLE ? T_UNLOCK_WHISTLE
         : bit == F_LIGHTS ? T_UNLOCK_LIGHTS : bit == F_CALL ? T_UNLOCK_CALL : bit == F_VOICE ? T_UNLOCK_VOICE : T_UNLOCK_SYNTH;
}
static void bp_draw(uint32_t feats, uint32_t hi)
{
    int32_t x, y, i, n = 0;
    uint16_t ln = RGB(90, 200, 255), dim = RGB(40, 70, 120);
    char b[16], *p;
    g_rect(0, 0, SW, SH, RGB(8, 14, 40));       /* blueprint blue, its grid */
    for (y = 0; y < SH; y += 12) g_rect(0, y, SW, 1, RGB(14, 26, 62));
    for (x = 0; x < SW; x += 12) g_rect(x, 0, 1, SH, RGB(14, 26, 62));
    for (i = 0; i < 8; i++) if (feats & BP_PARTS[i].bit) n++;
    g_deco(8, 6, 14, T_BP_TITLE, C_CREAM);
    p = b; fmt_u(p, (uint32_t)n); while (*p) p++; str_cat(p, "/8");
    g_text_r(SW - 8, 8, &FONT_S, b, C_CYAN);
    g_rect(0, 28, SW, 1, C_ROSE);
    /* the car, side on: body, roof, pantograph, windows, doors, the cab's face, bogies, rail */
    g_rframe(22, 78, 202, 54, 8, 1, ln);
    g_rect(28, 74, 186, 1, ln);
    g_line(48, 74, 58, 66, ln); g_line(68, 74, 58, 66, ln); g_line(58, 66, 46, 62, ln); g_line(46, 62, 72, 62, ln);
    for (x = 32; x < 176; x += 22) if (x < 64 || x > 84) { if (x < 132 || x > 152) g_rframe(x, 86, 16, 14, 2, 1, ln); }
    g_rframe(66, 84, 18, 44, 2, 1, ln); g_rframe(134, 84, 18, 44, 2, 1, ln);   /* the doors */
    g_rframe(188, 84, 28, 20, 3, 1, ln);         /* the cab window */
    g_rect(22, 114, 202, 1, C_ROSE_D);
    for (x = 46; x <= 194; x += 148) { g_ring(x, 138, 5, 6, ln); g_ring(x + 20, 138, 5, 6, ln); g_rect(x - 6, 132, 32, 1, ln); }
    g_rect(10, 145, 220, 1, dim);
    /* the parts: a light on the train, a leader to its label: lit, named, its control; or a grey box */
    for (i = 0; i < 8; i++) {
        const bp_part_t *q = &BP_PARTS[i];
        int on = (feats & q->bit) != 0u, h = q->bit == hi;
        uint16_t c = h ? ((fm1_ms / 250u) & 1u ? C_CREAM : C_GOLD) : on ? C_CYAN : dim;
        int32_t bx = q->lx, by = q->ly, bw = q->w, cx = bx + bw / 2, ey = by < 100 ? by + 30 : by;
        if (on || h) g_line(cx, ey, q->ax, q->ay, mix565(C_NIGHT, c, 150));
        g_disc(q->ax, q->ay, h ? 4 : 3, c);
        g_rrect(bx, by, bw, 30, 3, on || h ? C_PANEL : RGB(10, 18, 44));
        g_rframe(bx, by, bw, 30, 3, 1, c);
        if (on || h) {
            g_text_c(cx, by + 1, &FONT_S, bp_name(q->bit), h ? C_GOLD : C_CREAM);
            g_text_c(cx, by + 15, &FONT_S, q->ctl, C_ROSE);
        } else g_text_c(cx, by + 8, &FONT_S, T_BP_LOCKED, dim);
    }
    if (hi) {                                   /* a part just added: what it is for, and what it scores */
        g_cab_panel(4, 180, SW - 8, 42, C_GOLD);
        g_text_wrap(10, 182, SW - 20, 13, &FONT_S, bp_text(hi), -1, C_CREAM);
    }
}

static void title_draw(uint32_t sel, uint32_t unlocked)   /* before the map is yours: the picture, PLAY to begin */
{
    (void)sel; (void)unlocked;
    g_rect(0, 0, SW, SH, RGB(10, 6, 26));      /* (the whole screen: nothing of the last one shows) */
    title_scene(fm1_ms);
    ui_footer(T_TITLE_BEGIN);
    if (tl_text_a < 256u) g_rect_a(0, STAGE_Y1, SW, SH - STAGE_Y1, RGB(8, 4, 22), 256u - tl_text_a);   /* (its words fade up with the name) */
}

/* ---- the loop made whole: a light runs round it, each station flaring as it passes, the whole loop lit after */
static void loop_draw(uint32_t el)
{
    static const uint8_t LOOP[8] = {0, 1, 4, 5, 6, 8, 9, 0};   /* Akari Heights, Pacific, Meridian, Chrome, Liberty, Avalon, Union (lines.txt order) */
    uint32_t f = el * 7u * 256u / 5000u, k;      /* 0..7 stretches, x256, over five seconds */
    int32_t x, y, xa, ya, xb, yb;
    map_draw(0, -1, 0);
    g_rect_a(2, 20, SW - 4, 164, C_NIGHT, 110);
    for (k = 0; k < 7u; k++) {                  /* the loop, lit as the light passes */
        uint32_t j, n = 24, upto = f >= (k + 1u) * 256u ? n : f > k * 256u ? (f - k * 256u) * n / 256u : 0u;
        map_xy(LOOP[k], &xa, &ya); map_xy(LOOP[k + 1u], &xb, &yb);
        for (j = 0; j <= upto && upto; j++)
            g_disc(xa + (xb - xa) * (int32_t)j / (int32_t)n, ya + (yb - ya) * (int32_t)j / (int32_t)n, 2, mix565(C_CYAN, C_ROSE, (k * 256u + j * 256u / n) / 7u));
        if (f > k * 256u) {
            uint32_t since = el - k * 5000u / 7u;
            map_xy(LOOP[k], &x, &y);
            if (since < 400u) g_disc(x, y, 12 - (int32_t)since / 40, C_GOLD);
            g_rrect(x - 4, y - 4, 9, 9, 2, C_GOLD);
        }
    }
    if (f < 7u * 256u) {                        /* the light itself */
        uint32_t k2 = f >> 8, fr = f & 255u;
        map_xy(LOOP[k2], &xa, &ya); map_xy(LOOP[k2 + 1u], &xb, &yb);
        g_disc(xa + (xb - xa) * (int32_t)fr / 256, ya + (yb - ya) * (int32_t)fr / 256, 5, C_CREAM);
    } else {                                    /* whole: every station pulsing */
        for (k = 0; k < 7u; k++) { map_xy(LOOP[k], &x, &y); g_disc(x, y, 6 + (int32_t)(((el / 120u) + k) & 1u), C_GOLD); }
    }
    if (el > 2000u) {
        g_cab_panel(14, 186, SW - 28, 36, C_GOLD);
        g_deco_c(SW / 2, 190, 14, T_LOOP_TITLE, C_CREAM);
        if (el > 5200u) g_text_c(SW / 2, 206, &FONT_S, T_LOOP_SUB, C_GOLD);
    }
}

/* ---- your melody: eight notes on a staff (in D minor), the cursor on one; empty slots hollow */
static void sig_draw(const uint8_t *sg, uint32_t cur, int asked, int playing)
{
    uint32_t i;
    int32_t y;
    g_rect(0, 0, SW, SH, C_NIGHT);
    for (y = 0; y < 22; y++) g_rect(0, y, SW, 1, mix565(RGB(18, 8, 48), RGB(190, 50, 140), (uint32_t)y * 11u));
    g_deco_c(SW / 2, 4, 12, T_SIG_TITLE, C_CREAM);
    g_cab_panel(6, 30, SW - 12, 120, playing ? C_GOLD : C_ROSE);
    for (i = 0; i < 5u; i++) g_rect(16, 56 + (int32_t)i * 16, SW - 32, 1, C_PAPER2);   /* the staff */
    for (i = 0; i < 8u; i++) {
        int32_t x = 28 + (int32_t)i * 26, ny = sg[i] ? 132 - ((int32_t)sg[i] - 53) * 3 : 96;
        int on = i == cur;
        uint16_t c = on ? C_GOLD : C_CYAN;
        if (playing && (((mu.step >> 4) & 3u) * 2u + ((mu.step & 15u) >= 8u)) == i) c = C_CREAM;   /* (the note the song is singing: two a bar, from the song's own clock) */
        if (on) g_rrect(x - 11, 36, 22, 108, 4, mix565(C_PANEL, C_ROSE, 70));
        if (sg[i]) { g_disc(x, ny, 5, c); g_rect(x + 4, ny - 20, 2, 20, c); }
        else g_ring(x, ny, 4, 5, mix565(C_PANEL, c, 120));
    }
    g_text_c(SW / 2, 152, &FONT_S, asked ? T_SIG_ASK : playing ? T_SIG_HEARING : T_SIG_KEY, asked ? C_GOLD : C_INK2);
    g_text_wrap(14, 170, SW - 28, 14, &FONT_S, T_SIG_HELP, -1, C_INK2);
}

/* ---- the finale: the loop's stations each send a line into the heart, which grows, pulses, and fills the screen:
 * a disc with seven notches. Turned so every notch meets its station's line, it opens on the line's mark */
static const uint8_t HEART_LOOP[7] = {0, 1, 4, 5, 6, 8, 9};   /* the loop's stations (lines.txt order) */
static int32_t heart_angle(uint32_t k)          /* the station's direction from the heart, in 64ths of a turn */
{
    int32_t sx, sy, hx, hy;
    map_xy(HEART_LOOP[k], &sx, &sy); map_xy(NSTATION - 1u, &hx, &hy);
    return (int32_t)((mo_atan2(sy - hy, sx - hx) + 512u) / 1024u) % 64;
}
static uint32_t heart_aligned(int32_t off)      /* how many notches meet a line at this turn (all seven only at the one turn) */
{
    uint32_t k, j, n = 0;
    for (k = 0; k < 7u; k++)
        for (j = 0; j < 7u; j++) if ((heart_angle(k) + off) % 64 == heart_angle(j)) { n++; break; }
    return n;
}
static void heart_draw(uint32_t el, int32_t off, uint32_t won)
{
    uint32_t k;
    int32_t hx, hy, x, y;
    map_xy(NSTATION - 1u, &hx, &hy);
    if (el < 5500u) {                           /* the map; the lines growing in; the heart swelling */
        uint32_t f = el < 4000u ? el * 256u / 4000u : 256u;
        map_draw(0, -1, 0);
        g_rect_a(2, 20, SW - 4, 164, C_NIGHT, 90);
        g_cab_panel(2, 186, SW - 4, 35, RGB(255, 40, 70));   /* (the panel: only its name now) */
        g_deco_c(SW / 2, 193, 14, TR_STATION[NSTATION - 1u], RGB(255, 80, 100));
        for (k = 0; k < 7u; k++) {
            int32_t j, n = 30;
            map_xy(HEART_LOOP[k], &x, &y);
            for (j = 0; j <= n * (int32_t)f / 256; j++) g_disc(x + (hx - x) * j / n, y + (hy - y) * j / n, 1, mix565(C_ROSE, RGB(255, 40, 70), (uint32_t)j * 8u));
            g_disc(x, y, 4, C_GOLD);
        }
        {
            uint32_t ph = fm1_ms % 1000u;
            int32_t r = 6 + (int32_t)(el > 4000u ? (el - 4000u) * 60u / 1500u : 0u) + (ph < 120u || (ph > 240u && ph < 360u) ? 3 : 0);
            g_disc(hx, hy, r, RGB(200, 10, 40)); g_disc(hx, hy, r * 2 / 3, RGB(255, 60, 90));
        }
        return;
    }
    /* the heart, close: a dark disc, seven notches turning with it, seven lines coming in from the stations */
    g_rect(0, 0, SW, SH, RGB(10, 0, 4));
    {
        int32_t cx = 120, cy = 112, R2 = 78;
        uint32_t ph = fm1_ms % 1000u, beat = !won && (ph < 120u || (ph > 240u && ph < 360u));
        for (k = 0; k < 7u; k++) {             /* the lines, fixed, from the edge toward the disc */
            uint32_t a = (uint32_t)heart_angle(k) * 1024u;
            int32_t al = 0; uint32_t j2;
            for (j2 = 0; j2 < 7u; j2++) if ((heart_angle(j2) + off) % 64 == heart_angle(k)) al = 1;   /* (a notch on it: it glows) */
            g_thick(px16(cx, 118 * 16, a) >> 4, py16(cy, 118 * 16, a) >> 4, px16(cx, (R2 + 4) * 16, a) >> 4, py16(cy, (R2 + 4) * 16, a) >> 4, 2, al || won ? C_CYAN : RGB(120, 20, 40));
        }
        g_disc(cx, cy, R2 + (beat ? 3 : 0), won ? RGB(40, 10, 30) : RGB(90, 0, 24));
        g_ring(cx, cy, R2 - 2 + (beat ? 3 : 0), R2 + (beat ? 3 : 0), won ? C_GOLD : RGB(255, 60, 90));
        for (k = 0; k < 7u; k++) {             /* the notches, turning with the disc */
            uint32_t a = (uint32_t)((heart_angle(k) + off) % 64) * 1024u;
            g_disc(px16(cx, (R2 - 8) * 16, a) >> 4, py16(cy, (R2 - 8) * 16, a) >> 4, 4, won ? C_GOLD : RGB(255, 120, 140));
            g_line(px16(cx, 20 * 16, a) >> 4, py16(cy, 20 * 16, a) >> 4, px16(cx, (R2 - 12) * 16, a) >> 4, py16(cy, (R2 - 12) * 16, a) >> 4, won ? C_GOLD : RGB(150, 30, 60));
        }
        if (won) {                              /* opened: the line's mark, what it was, all along */
            g_sigil(cx, cy, 16 + (int32_t)(won > 1500u ? 12u : won * 12u / 1500u), C_GOLD);
            if (won > 1800u) {                  /* the two lines, alike: the same size, the same panel */
                g_cab_panel(8, 180, SW - 16, 42, C_GOLD);
                g_text_c(SW / 2, 186, &FONT_S, T_HEART_REVEAL, C_CREAM);
                if (won > 3500u) g_text_c(SW / 2, 203, &FONT_S, T_HEART_REVEAL2, C_CREAM);
            }
        } else g_text_c(SW / 2, 6, &FONT_S, T_HEART_TURN, RGB(255, 120, 140));
    }
}

/* ---- the title once the game is finished: four ways in */
static void menu_draw(uint32_t sel, int confirm)
{
    static const char *IT[4];
    char b[24], *p;
    uint32_t i, k, n = 0, sum = 0, mx = 0;
    IT[0] = T_MENU_CONTINUE; IT[1] = T_MENU_NEW; IT[2] = T_MENU_ENODEN; IT[3] = T_MENU_ENDLESS;
    g_rect(0, 0, SW, SH, RGB(10, 6, 26));
    title_scene(fm1_ms);
    g_rect_a(30, 102, 180, 118, C_NIGHT, 170);
    g_cab_panel(30, 102, 180, 118, C_ROSE);
    for (k = 0; k < NENLEGS; k++) if (save.en_mx[k]) { n++; if (save.en_pts[k] > 0) sum += (uint32_t)save.en_pts[k]; mx += save.en_mx[k]; }
    for (i = 0; i < 4u; i++) {
        int32_t y = 110 + (int32_t)i * 26, on = i == sel;
        if (on) g_rrect(38, y - 2, 164, 22, 5, mix565(C_PANEL, C_ROSE, 90));
        g_text(46, y, &FONT_S, on ? ">" : " ", C_GOLD);
        g_text(60, y, &FONT_S, i == 1u && confirm ? T_MENU_SURE : IT[i], i == 1u && confirm ? C_ROSE : on ? C_CREAM : C_INK2);
        if (i >= 2u && ((i == 2u && !n) || (i == 3u && !save.endless_m))) {   /* unlocked, not yet played: an open padlock, gold, NEW (then the score in its place) */
            int32_t lx = 150, ly = y + 3;
            g_rframe(lx + 3, ly - 3, 7, 7, 3, 1, C_GOLD);            /* the shackle, swung open */
            g_rect(lx + 3, ly + 1, 2, 3, mix565(C_NIGHT, C_PANEL, 128));
            g_rrect(lx, ly + 2, 9, 7, 2, C_GOLD);
            g_rect(lx + 4, ly + 4, 1, 3, RGB(90, 60, 10));
            if ((i == 2u && !n) || (i == 3u && !save.endless_m)) {
                g_rrect(166, y, 30, 15, 4, C_ROSE);
                g_text_c(181, y, &FONT_S, T_MENU_NEWTAG, C_CREAM);
            }
        }
        if (i == 2u && n) {                     /* the Enoden's whole-line score */
            uint32_t pct = mx ? sum * 100u / mx : 0u;
            char t[2] = {TR_TIER[pct >= 90u ? 7 : pct >= 75u ? 6 : pct >= 60u ? 5 : pct >= 45u ? 4 : pct >= 30u ? 3 : pct >= 15u ? 2 : 1], 0};
            g_text_r(194, y, &FONT_S, t, C_GOLD);
        }
        if (i == 3u && save.endless_m) {        /* the endless line's farthest */
            p = b; fmt_u(p, save.endless_m / 1000u); while (*p) p++; *p++ = '.'; *p++ = (char)('0' + save.endless_m / 100u % 10u); str_cat(p, "km");
            g_text_r(194, y, &FONT_S, b, C_GOLD);
        }
    }
    ui_footer(T_MENU_FOOT);
}

/* ---- the Enoden's map: the coast line from Fujisawa to Kamakura, each stretch's best, the whole line's score */
/* the stations where the real line runs: south from Fujisawa, east along the coast, up to Kamakura */
static const uint8_t EN_XY[15][2] = {{40, 36}, {36, 54}, {35, 72}, {29, 90}, {28, 110}, {40, 128}, {56, 138}, {74, 146},
                                     {100, 150}, {134, 154}, {152, 139}, {172, 130}, {188, 124}, {205, 118}, {218, 100}};
/* where each stretch's grade badge sits (beside the line, away from the number tags) */
static const int8_t EN_BADGE[14][2] = {{-22, -6}, {-22, -6}, {-22, -6}, {-22, -6}, {-24, -4}, {-10, 12}, {-4, 12}, {0, 12},
                                       {0, 12}, {4, 12}, {6, 12}, {4, 12}, {6, 12}, {12, 6}};
static void en_xy(uint32_t i, int32_t *x, int32_t *y) { *x = EN_XY[i < 15u ? i : 14u][0]; *y = EN_XY[i < 15u ? i : 14u][1]; }
static uint16_t en_tier_col(uint32_t t)          /* a grade's badge: S gold, A green, B teal, C blue, D orange, E/F red */
{
    return t >= 7 ? RGB(232, 170, 30) : t == 6 ? RGB(40, 150, 70) : t == 5 ? RGB(30, 140, 150) : t == 4 ? RGB(50, 100, 190)
         : t == 3 ? RGB(230, 120, 40) : RGB(200, 50, 50);
}
static void en_badge(int32_t cx, int32_t cy, uint32_t t, int big)
{
    char tt[2] = {TR_TIER[t], 0};
    int32_t w = big ? 18 : 13, h = big ? 18 : 15;
    g_rrect(cx - w / 2 + 1, cy - h / 2 + 1, w, h, 3, RGB(60, 70, 60));   /* a shadow */
    g_rrect(cx - w / 2, cy - h / 2, w, h, 3, en_tier_col(t));
    g_text_c(cx, cy - 7, &FONT_S, tt, RGB(255, 255, 255));
}
static void en_map_draw(uint32_t sel)
{
    static const uint8_t COAST[11][2] = {{0, 146}, {20, 146}, {34, 144}, {48, 150}, {70, 160}, {100, 166}, {136, 170},
                                         {158, 156}, {180, 146}, {214, 136}, {240, 132}};
    uint32_t i, n = 0, sum = 0, mx = 0;
    int32_t x, y, xa, ya, xb, yb;
    char b[40], *p;
    const uint16_t LAND = RGB(226, 238, 204), SEA = RGB(172, 214, 234), LINE = RGB(56, 156, 76);
    g_rect(0, 21, SW, 165, LAND);
    for (x = 0; x < SW; x++) {                  /* the sea, below the coast */
        int32_t k = 0, cy;
        while (k < 9 && COAST[k + 1][0] <= x) k++;
        cy = COAST[k][1] + (COAST[k + 1][1] - COAST[k][1]) * (x - COAST[k][0]) / (COAST[k + 1][0] - COAST[k][0] + 1);
        g_rect(x, cy, 1, 186 - cy, SEA);
        if (!(x % 9)) g_rect(x, cy + 8 + (x % 5), 4, 1, RGB(220, 240, 250));   /* a little swell */
    }
    g_rect(0, 21, 14, 40, SEA);                 /* the river mouth at the top left */
    /* the landmarks: Enoshima and its lighthouse, the hills, maple leaves, hydrangeas, the Great Buddha, the temple */
    g_disc(24, 168, 9, RGB(110, 170, 90)); g_disc(30, 166, 7, RGB(96, 160, 80));
    g_rect(24, 152, 3, 12, RGB(200, 200, 205)); g_rect(23, 151, 5, 2, RGB(120, 120, 130));
    g_disc(118, 104, 12, RGB(110, 180, 90)); g_disc(132, 108, 9, RGB(90, 160, 76)); g_rect(104, 108, 40, 6, LAND);
    for (i = 0; i < 3u; i++) { int32_t lx = 82 + (int32_t)i * 8, ly = 62 + (int32_t)(i & 1u) * 6; g_disc(lx, ly, 3, RGB(220, 70, 40)); g_disc(lx + 2, ly - 2, 2, RGB(240, 110, 50)); }
    for (i = 0; i < 9u; i++) g_rrect(136 + (int32_t)(i % 3u) * 7 + (int32_t)(i / 3u) * 2, 36 + (int32_t)(i / 3u) * 6, 6, 5, 1, i & 1u ? RGB(90, 120, 200) : RGB(130, 160, 230));
    g_disc(176, 98, 9, RGB(150, 190, 160)); g_disc(176, 86, 5, RGB(150, 190, 160)); g_rect(166, 100, 21, 6, RGB(130, 170, 140));   /* the Buddha */
    g_rect(202, 54, 26, 3, RGB(200, 40, 40)); g_rect(198, 51, 34, 3, RGB(140, 30, 30)); g_rect(206, 57, 18, 9, RGB(220, 210, 190));   /* the temple */
    g_rect(208, 57, 2, 9, RGB(200, 40, 40)); g_rect(220, 57, 2, 9, RGB(200, 40, 40));
    /* the line: thick green; the chosen stretch in orange, a train on it */
    for (i = 0; i < NENLEGS; i++) {
        en_xy(i, &xa, &ya); en_xy(i + 1u, &xb, &yb);
        g_thick(xa, ya, xb, yb, 4, i == sel ? RGB(240, 140, 30) : LINE);
    }
    {
        uint32_t ph = (fm1_ms / 10u) % 256u;
        en_xy(sel, &xa, &ya); en_xy(sel + 1u, &xb, &yb);
        x = xa + (xb - xa) * (int32_t)ph / 256; y = ya + (yb - ya) * (int32_t)ph / 256;
        g_rrect(x - 5, y - 4, 11, 8, 2, RGB(250, 245, 225)); g_rect(x - 5, y, 11, 2, LINE);
    }
    for (i = 0; i <= NENLEGS; i++) {            /* the stations: white, ringed in red; their numbers on green tags */
        char t[3] = {(char)('0' + (i + 1u) / 10u), (char)('0' + (i + 1u) % 10u), 0};
        int32_t tx, ty, on = i == sel || i == sel + 1u;
        en_xy(i, &x, &y);
        g_disc(x, y, 5, on ? RGB(240, 140, 30) : RGB(220, 40, 60));
        g_disc(x, y, 3, RGB(255, 255, 255));
        tx = i <= 4u ? x + 8 : i == 14u ? x - 26 : x - 9; ty = i <= 4u ? y - 7 : i == 14u ? y - 18 : y - 24;
        if (i >= 5u && i <= 9u) ty = y - 22;
        if (i == 5u) { tx = x - 24; ty = y + 2; }   /* (clear of 05's tag) */
        g_rrect(tx, ty, 19, 14, 3, on ? RGB(240, 140, 30) : LINE);
        g_text(tx + 2, ty, &FONT_S, t, RGB(255, 255, 255));
    }
    for (i = 0; i < NENLEGS; i++) if (save.en_tier[i]) {   /* each stretch's grade, big, beside it */
        en_xy(i, &xa, &ya); en_xy(i + 1u, &xb, &yb);
        en_badge((xa + xb) / 2 + EN_BADGE[i][0], (ya + yb) / 2 + EN_BADGE[i][1], save.en_tier[i], 0);
    }
    /* the band: the line's name, the whole line's grade and points */
    g_rect(0, 0, SW, 21, LINE);
    g_deco(6, 3, 11, "ENODEN", RGB(255, 255, 255));
    for (i = 0; i < NENLEGS; i++) if (save.en_mx[i]) { n++; if (save.en_pts[i] > 0) sum += (uint32_t)save.en_pts[i]; mx += save.en_mx[i]; }
    if (n) {
        uint32_t pct = mx ? sum * 100u / mx : 0u, t = pct >= 90u ? 7 : pct >= 75u ? 6 : pct >= 60u ? 5 : pct >= 45u ? 4 : pct >= 30u ? 3 : pct >= 15u ? 2 : 1;
        p = b; fmt_u(p, n); while (*p) p++; p = str_cat(p, "/14  "); fmt_u(p, sum); while (*p) p++; *p++ = '/'; fmt_u(p, mx);
        g_text_r(SW - 26, 3, &FONT_S, b, RGB(255, 255, 255));
        en_badge(SW - 13, 10, t, 1);
    }
    /* the panel: the chosen stretch, from and to, its length, its best */
    g_rrect(2, 187, SW - 4, 34, 5, RGB(250, 250, 244));
    g_rframe(2, 187, SW - 4, 34, 5, 2, LINE);
    p = b; fmt_u(p, sel + 1u); while (*p) p++; *p++ = ' '; str_cat(p, EN_STATION[sel]);
    g_text(8, 189, &FONT_S, b, RGB(30, 60, 40));
    p = b; fmt_u(p, sel + 2u); while (*p) p++; *p++ = ' '; str_cat(p, EN_STATION[sel + 1u]);
    g_text(8, 204, &FONT_S, b, RGB(30, 60, 40));
    p = b; fmt_u(p, EN_LEGS[sel].dist); while (*p) p++; str_cat(p, "m");
    g_text_r(save.en_tier[sel] ? SW - 30 : SW - 8, 196, &FONT_S, b, RGB(110, 120, 110));
    if (save.en_tier[sel]) en_badge(SW - 16, 204, save.en_tier[sel], 1);
}

/* ---- the end: credits (content/script.txt [credits]) rising over the line under the cherries */
static void end_draw(uint32_t el)
{
    int32_t y = 236 - (int32_t)(el / 40u), i;
    g_bg(BG_ART_SAKURA);                        /* (the credits rise over the line under the cherries) */
    g_dim(0, 0, SW, SH, 90);
    for (i = 0; i < (int32_t)NCREDITS; i++) {
        int32_t yy = y + i * 22;
        if (yy < -20 || yy > 240 || CREDITS[i].style == 3u) continue;
        if (CREDITS[i].style == 0u) g_deco_c(120, yy, 11, CREDITS[i].text, C_GOLD);
        else g_text_c(120, yy, &FONT_S, CREDITS[i].text, CREDITS[i].style == 2u ? C_CYAN : C_CREAM);
    }
}
