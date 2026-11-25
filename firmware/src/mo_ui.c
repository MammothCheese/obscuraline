/* SPDX-License-Identifier: GPL-3.0-only */
/* Mechanica Obscura: the panel, and what every scene shares (header, footer, help card, the plaque). */

/* ---- the panel, read once a frame */
static struct {
    int32_t enc[8];                             /* detents this frame, + clockwise (FM-1: 7, SMK-37: 8) */
    uint32_t press, rel, held;                  /* buttons: pressed / released this frame, held now */
    uint32_t kdown, kheld;                      /* keys: pressed this frame (none lost), held now */
} in;
#define PRESSED(b) (in.press & (1u << (b)))
#define KEY0_NOTE 53                            /* key 0 = F3 */
/* white keys of the FM-1's 27 (F3 .. G5): key index of the n-th white key */
static const uint8_t WHITE_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};

/* keys sound at once (from the main loop and while the screen's DMA runs): a game sets kb_hook */
static void (*kb_hook)(uint32_t key);
static uint32_t kb_prev, kb_pending;
static uint8_t kb_busy;
static void kb_service(void)
{
    uint32_t keys, down, k;
    if (kb_busy) return;
    kb_busy = 1;
    keys = fm1_in.notes;
    down = (keys & ~kb_prev) | fm1_input_note_edges();
    kb_prev = keys;
    if (down) {
        for (k = 0; k < 27u; k++)
            if (down >> k & 1u && kb_hook) kb_hook(k);
        kb_pending |= down;
    }
    kb_busy = 0;
}
static void app_idle(void) { kb_service(); }

static void ui_read(void)
{
    uint32_t e;
    kb_service();
    in.press = fm1_input_edges(&in.rel);
    in.held = fm1_in.buttons;
    for (e = 0; e < (BOARD_SMK37 ? 8u : 7u); e++) in.enc[e] = fm1_enc_take(e);
    in.kdown = kb_pending;
    kb_pending = 0;
    in.kheld = fm1_in.notes;
}
/* the dial most games turn: KNOB1, or the big SELECT encoder, whichever the hand is on */
static int32_t enc_main(void) { return in.enc[ENC_KNOB1] + in.enc[ENC_SELECT]; }

/* LEDs: the buttons a scene listens to are lit, so the panel shows what does something */
static uint32_t led_btn_mask, led_btn_blink;
static void ui_leds_buttons(void)
{
    uint32_t b, blink = (fm1_ms / 300u) & 1u;
    for (b = 0; b < 14u; b++) {
        int on = (led_btn_mask >> b & 1u) != 0;
        if (led_btn_blink >> b & 1u) on = blink;
        fm1_led_key(b, on);
    }
}
static void ui_keys_clear(void)
{
    uint32_t k;
    for (k = 0; k < 27u; k++) { fm1_led_key(KEY_LED(k), 0); fm1_led_key_glow(KEY_LED(k), 0); }
}

/* ---- easing (t 0..256) */
static int32_t ease_out(int32_t t) { t = clampi(t, 0, 256); return 256 - (((256 - t) * (256 - t)) >> 8); }
static int32_t ease_bounce(int32_t t)           /* a drop that settles with two small bounces */
{
    t = clampi(t, 0, 256);
    if (t < 150) return (t * t * 256) / (150 * 150);
    if (t < 210) { int32_t u = t - 180; return 256 - (900 - u * u) * 40 / 900; }
    { int32_t u = t - 233; return 256 - (529 - u * u) * 12 / 529; }
}

/* ---- the frame of a scene: a lacquer header with a gold Deco title over a half sunburst; a footer of control
 * legends in neon, as printed under a pedal's knobs */
#define STAGE_Y0 22
#define STAGE_Y1 222
static const char *ui_hdr_station;              /* the header's right-hand side: > STATION (0: none) */
static void ui_header(const char *num, const char *title)
{                                               /* the cab's header: a square lamp, the title, > the station */
    int32_t tw, sw2 = 0, x = 26;
    char t[40], *p = t;
    g_rect(0, 0, SW, 22, ut.night);
    g_rrect(5, 3, 15, 15, 3, ut.cream);
    g_rrect(7, 5, 11, 11, 2, ut.night);
    if (num && *num) { p = str_cat(p, num); p = str_cat(p, " "); }
    str_cat(p, title);
    tw = text_w(&FONT_S, t);
    if (ui_hdr_station && *ui_hdr_station) sw2 = text_w(&FONT_S, ui_hdr_station) + 14;
    if (x + tw + sw2 + 14 > SW) sw2 = 0;        /* (a long title wins: the station goes, rather than crowd it) */
    if (x + tw > SW - 4) { t[(SW - 4 - x) / 8] = 0; }
    g_text(x, 2, &FONT_S, t, ut.cream);
    if (sw2) {
        int32_t sx = SW - 6 - sw2;
        int32_t q[6] = {sx * 16, 6 * 16, sx * 16, 15 * 16, (sx + 7) * 16, 168};
        g_tri16(q[0], q[1], q[2], q[3], q[4], q[5], ut.cream);
        g_text(sx + 12, 2, &FONT_S, ui_hdr_station, ut.cream);
    }
    g_rect(0, 20, SW, 2, ut.rose);
    g_rect(0, 22, SW, 1, ut.rose_d);
}
static void ui_footer(const char *txt)          /* legends split by two spaces, each its own colour */
{
    const uint16_t COL[4] = {ut.cyan, ut.rose, C_AMBER, C_GOLD};
    char tok[32];
    int32_t x, n = 0, w = 0, k;
    const char *p;
    g_rect(0, STAGE_Y1, SW, SH - STAGE_Y1, ut.night);
    g_rect(0, STAGE_Y1, SW, 1, ut.rose_d);
    for (p = txt; *p; p++) w += 8;
    x = SW / 2 - w / 2;
    for (p = txt, k = 0; ; p++) {
        if (*p == 0 || (p[0] == ' ' && p[1] == ' ')) {
            tok[n] = 0;
            if (n) x = g_text(x, STAGE_Y1 + 2, &FONT_S, tok, COL[k & 3]);
            k++; n = 0;
            if (!*p) break;
            while (p[1] == ' ') { p++; x += 8; }
            x += 8;
            continue;
        }
        if (n < 31) tok[n++] = *p;
    }
}

/* ---- a lacquer card with an engraved neon rule: help, messages */
static void ui_card(int32_t x, int32_t y, int32_t w, int32_t h)
{
    g_rect_a(x + 3, y + 4, w, h, ut.night, 140);  /* a soft shadow */
    g_cab_panel(x, y, w, h, ut.rose);
}
/* instructions: a Deco title, the text in cream, the way out */
static void ui_help(const char *title, const char *text, const char *ctrl, const char *foot)
{                                               /* the card: a heading, a line or two of what to do, then the controls, one each */
    int32_t x = 8, y = STAGE_Y0 + 6, w = SW - 16, h = STAGE_Y1 - STAGE_Y0 - 10, n = 0, i, cols, cy;
    const char *it[6]; uint8_t il[6];
    const char *p = ctrl;
    ui_card(x, y, w, h);
    g_rrect(x + 10, y + 8, w - 20, 18, 4, ut.panel2);
    g_text_c(SW / 2, y + 9, &FONT_S, title, ut.cyan);
    g_text_wrap(x + 12, y + 32, w - 24, 16, &FONT_S, text, -1, ut.cream);
    while (p && *p && n < 6) {                  /* the controls: split at two spaces */
        const char *q = p;
        while (*q && !(q[0] == ' ' && q[1] == ' ')) q++;
        it[n] = p; il[n] = (uint8_t)(q - p); n++;
        while (*q == ' ') q++;
        p = q;
    }
    cols = 1;
    cy = y + h - 30 - n * 15;
    g_rect(x + 12, cy - 6, w - 24, 1, ut.rose_d);
    for (i = 0; i < n; i++) {                   /* each: its control lit, what it does after */
        char a[24], b[24];
        uint32_t k = 0, m = 0, sp = 0;
        int32_t cx = x + 14 + (cols > 1 ? (i % 2) * (w / 2) : 0), ry = cy + i * 15;
        while (k < il[i] && it[i][k] != ' ' && k < 23u) { a[k] = it[i][k]; k++; }
        a[k] = 0;
        sp = k;
        while (sp < il[i] && it[i][sp] == ' ') sp++;
        while (sp + m < il[i] && m < 23u) { b[m] = it[i][sp + m]; m++; }
        b[m] = 0;
        g_text(cx, ry, &FONT_S, a, ut.cyan);
        g_text(cx + text_w(&FONT_S, a) + 6, ry, &FONT_S, b, ut.ink2);
    }
    g_rect(x + 12, y + h - 26, w - 24, 1, ut.rose_d);
    g_text_c(SW / 2, y + h - 22, &FONT_S, foot, ut.rose);
}

/* ---- the plaque: the job is done. A lacquer plate on gold chains drops in, bounces; a gold sunburst and the
 * word in Deco capitals */
static void ui_plaque(uint32_t t_ms, const char *word)
{                                               /* a neon card drops in on two wires, the word lit on it */
    int32_t t = (int32_t)(t_ms * 256u / 700u), y = -60 + (ease_bounce(t) * 122 >> 8), x = 30, w = SW - 60, h = 54;
    if (t <= 0) return;
    g_line(x + 20, 0, x + 20, y, C_CYAN_D);
    g_line(x + w - 21, 0, x + w - 21, y, C_CYAN_D);
    g_rect_a(x + 4, y + 5, w, h, C_NIGHT, 160);
    g_cab_panel(x, y, w, h, C_ROSE);
    g_deco_c(SW / 2 + 1, y + 15, 24, word, C_ROSE_D);
    g_deco_c(SW / 2, y + 14, 24, word, C_CREAM);
    {   /* a glint sweeping across once it has landed */
        int32_t gx = ((int32_t)t_ms - 900) * 240 / 500 + x, i;
        if (gx > x + 6 && gx < x + w - 6)
            for (i = 4; i < h - 4; i++) { px_blend(gx + i / 3, y + i, C_CYAN, 140); px_blend(gx + i / 3 + 1, y + i, C_CYAN, 70); }
    }
}

/* ---- progress kept in flash */
#define MO_MAGIC 0x374C424Fu                    /* "OBL7": Obscura Line's save (older saves start over) */
static struct {
    uint32_t magic;
    uint8_t unlocked;                           /* (unused: the map decides) */
    uint8_t heard_music;                        /* the music box has sung: the title plays it */
    uint32_t done;                              /* bit c: chapter c solved once */
    uint16_t repairs;                           /* robots mended in the Workshop */
    uint8_t features;                           /* F_*: what the train has (unlocked by puzzles) */
    uint8_t music_off;                          /* the cab's music muted */
    uint32_t driven;                            /* bit l: leg l driven once */
    uint8_t tier[32];                           /* each leg's best: 0 none, 1 F .. 7 S */
    uint8_t finished;                           /* the game finished once: the title's menu */
    uint8_t en_tier[16], en_mx[16];             /* the Enoden: each stretch's best tier, the most points it had */
    int8_t en_pts[16];                          /* ... and its best points */
    uint32_t endless_m;                         /* the endless line: the farthest run, metres */
    uint8_t sig[8];                             /* the player's melody (MIDI; 0: not yet made) */
    uint8_t mb_tune;                            /* the music box's tune (Mercury's clock chimes it first), 0: not chosen */
    uint8_t loop_seen;                          /* the loop's scene shown (after its last line was driven) */
    uint8_t at_st, at;                          /* where the player was: the station; 0 the map, 1+c chapter c, 64+l driving leg l */
} save = {MO_MAGIC, 0, 0, 0, 0, 0, 0, 0, {0}, 0, {0}, {0}, {0}, 0, {0}, 0, 0, 0, 0};
typedef char save_size_check[sizeof save == 120u ? 1 : -1];   /* (at_st, at sit in what was padding: older saves still load) */
static uint32_t save_dirty, save_t;
static void save_mark(void) { save_dirty = 1; save_t = fm1_ms; }
static void save_service(void)                  /* only in silence: an erase stalls the sound */
{
    if (save_dirty && fm1_ms - save_t > 1500u && au_quiet()) {
        settings_save(&save, sizeof save);
        save_dirty = 0;
    }
}

/* ---- a chapter's mechanism */
typedef struct {                                /* a mechanism type's behaviour; its words are the chapter's (script.txt) */
    uint32_t buttons;                           /* button LEDs lit while playing */
    uint32_t plaque_ms;                         /* the mechanism's own finale runs this long before the plaque */
    void (*enter)(void);                        /* a new puzzle, from the chapter's settings (PRM, LST) */
    int (*update)(void);                        /* a frame of play: 1 = solved */
    void (*draw)(uint32_t won_ms);              /* the stage (STAGE_Y0..STAGE_Y1); won_ms > 0: the finale */
    void (*leave)(void);
    uint8_t grab;                               /* 1: it uses every button (HOME must be held to leave, EDIT is its own) */
} game_t;
#define BIT(b) (1u << (b))
