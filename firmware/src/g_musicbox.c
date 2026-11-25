/* SPDX-License-Identifier: GPL-3.0-only */
/* Chapter III: THE SILENT CYLINDER. Re-pin a music box.
 *
 * The cylinder turns toward you; its pins pluck the eight tines of the comb (C to C). Sixteen rows go round
 * once per tune. Some of the original pins (dark) survive; the rest fell out. The comb still rings faintly
 * where they once struck: REC hums that worn memory (of the row, where a pin is missing). Turn the cylinder with any knob and press
 * a lit key to set a pin on the row at the comb (again to take it out). PLAY cranks the box and says how many
 * rows sing true. The tune and the missing pins are new each time.
 *
 * The keyboard sounds at once (kb_hook, also while the screen draws), so it plays like the comb itself. */

#define MB_STEPS 16
#define MB_STEP_MS mb.step_ms                   /* puzzles.txt step_ms */
static const uint8_t MB_KEY[8] = {7, 9, 11, 12, 14, 16, 18, 19};   /* C4 D4 E4 F4 G4 A4 B4 C5 on the panel */
static const uint8_t MB_NOTE[8] = {72, 74, 76, 77, 79, 81, 83, 84};   /* the default comb (the title, the masterwork) */
static const char *const MB_NAME[8] = {"C", "D", "E", "F", "G", "A", "B", "C"};
/* the tunes are puzzles.txt `tune` lines: 16 rows, 1..8 = tine, 0 = rest */

static struct {
    uint8_t tune[MB_STEPS];                     /* the tune drawn for this puzzle */
    uint8_t comb[8];                            /* the tines' notes */
    uint32_t step_ms;
    uint8_t orig[MB_STEPS];                     /* surviving pins (0 = none) */
    uint8_t pin[MB_STEPS];                      /* pins set by the player */
    int32_t target;                             /* the row at the comb */
    int32_t pos;                                /* the drawn rotation, rows Q8 */
    uint32_t play_at;                           /* frame time the crank started (0: still) */
    uint8_t playing;                            /* 1 crank, 2 worn memory */
    uint32_t pluck_t[8];                        /* when a tine was last plucked (ms) */
    int32_t true_rows;                          /* after a crank: rows that sang true (-1: not cranked) */
    uint16_t row_ok;                            /* after a crank: bit r = row r sang true */
    uint32_t row_ok_t;
    uint8_t missing[MB_STEPS];                  /* rows whose pin fell out (marked on the map) */
    uint32_t rattle_t;
    uint32_t notes_t;                           /* the finale's floating notes */
} mb;

static uint8_t mb_row(int32_t s) { s &= MB_STEPS - 1; return mb.orig[s] ? mb.orig[s] : mb.pin[s]; }

static void mb_key(uint32_t k)                  /* kb_hook: a tine sounds the moment its key goes down */
{
    uint32_t c;
    for (c = 0; c < 8u; c++)
        if (MB_KEY[c] == k) { sfx_tine(mb.comb[c], 100, 0, (uint8_t)(70u + c * 16u), 2); mb.pluck_t[c] = fm1_ms; }
}

static void mb_enter(void)
{
    const mo_list_t *tl = LST(L_MUSICBOX_TUNE);
    uint32_t i, miss = 0, want = (uint32_t)prm_pick(P_MUSICBOX_MISSING_LO, P_MUSICBOX_MISSING_HI), pick;
    if (!save.mb_tune) save.mb_tune = (uint8_t)(1u + rndn(tl->n / MB_STEPS));   /* (Mercury's clock chimed this one) */
    pick = (uint32_t)(save.mb_tune - 1u) % (tl->n / MB_STEPS);
    for (i = 0; i < MB_STEPS; i++) mb.tune[i] = (uint8_t)tl->v[pick * MB_STEPS + i];
    for (i = 0; i < 8u; i++) mb.comb[i] = (uint8_t)LST(L_MUSICBOX_COMB)->v[i];
    mb.step_ms = (uint32_t)PRM(P_MUSICBOX_STEP_MS);
    for (i = 0; i < MB_STEPS; i++) { mb.orig[i] = mb.tune[i]; mb.pin[i] = 0; mb.missing[i] = 0; }
    mb.row_ok = 0; mb.row_ok_t = 0;
    while (miss < want) {                       /* pins that fell out: only where a note was */
        i = rndn(MB_STEPS);
        if (mb.orig[i]) { mb.orig[i] = 0; mb.missing[i] = 1; miss++; }
    }
    mb.target = 0; mb.pos = 0; mb.playing = 0; mb.true_rows = -1; mb.notes_t = 0;
    kb_hook = mb_key;
}

static void mb_crank(int worn)
{
    uint32_t t0 = au_time() + ms_frames(120), s;
    au_cut(2);
    for (s = 0; s < MB_STEPS; s++) {
        uint32_t at = t0 + ms_frames(s * MB_STEP_MS);
        uint8_t c = worn ? mb.tune[s] : mb_row((int32_t)s);
        if (!c) continue;
        if (worn) sfx_tine_worn(mb.comb[c - 1u], at, 2);
        else sfx_tine(mb.comb[c - 1u], 96, at, (uint8_t)(70u + (c - 1u) * 16u), 2);
    }
    mb.play_at = t0;
    mb.playing = (uint8_t)(worn ? 2 : 1);
}

static int mb_update(void)
{
    uint32_t c, k;
    int32_t e = enc_main() + in.enc[ENC_KNOB2] + in.enc[ENC_KNOB3] + in.enc[ENC_KNOB4] + in.enc[ENC_ALGORITHM] + in.enc[ENC_PRESETS];   /* (any knob turns the cylinder) */
    led_btn_mask = BIT(BTN_EDIT) | BIT(BTN_PLAY) | BIT(BTN_REC);
    led_btn_blink = mb.playing == 1 ? BIT(BTN_PLAY) : mb.playing == 2 ? BIT(BTN_REC) : 0u;
    if (mb.playing) {
        uint32_t el = au_time() - mb.play_at;
        if ((int32_t)el > 0 && el > ms_frames(MB_STEPS * MB_STEP_MS + 200u)) {
            mb.playing = 0;
            mb.pos = mb.target * 256;
            if (led_btn_blink & BIT(BTN_PLAY)) {
                int32_t ok = 0;
                for (k = 0; k < MB_STEPS; k++) ok += mb_row((int32_t)k) == mb.tune[k];
                mb.true_rows = ok;
                mb.row_ok = 0; mb.row_ok_t = fm1_ms;
                for (k = 0; k < MB_STEPS; k++) if (mb_row((int32_t)k) == mb.tune[k]) mb.row_ok |= (uint16_t)(1u << k);
                led_btn_blink = 0;
                if (ok == MB_STEPS) return 1;
            }
            led_btn_blink = 0;
        }
        if (PRESSED(BTN_PLAY) || PRESSED(BTN_REC)) { au_cut(2); mb.playing = 0; mb.pos = mb.target * 256; }
        return 0;
    }
    if (e) {
        mb.target += e;
        sfx_click(0, 55, 128);
        sfx_tick(0, 100, 30, 128, 0);
    }
    for (k = 0; k < 27u; k++) {                 /* a lit key at the comb row: set or lift a pin */
        if (!(in.kdown >> k & 1u)) continue;
        for (c = 0; c < 8u; c++) if (MB_KEY[c] == k) break;
        if (c == 8u) continue;
        {
            int32_t s = mb.target & (MB_STEPS - 1);
            if (mb.orig[s]) { mb.rattle_t = fm1_ms; sfx_thunk(0, 60, 25, 128); continue; }   /* an old pin: fast */
            mb.pin[s] = mb.pin[s] == c + 1u ? 0 : (uint8_t)(c + 1u);
            sfx_click(0, 80, 128);
            mb.true_rows = -1;
        }
    }
    if (PRESSED(BTN_PLAY)) mb_crank(0);
    else if (PRESSED(BTN_REC)) {
        int32_t sr = mb.target & (MB_STEPS - 1);
        if (mb.missing[sr]) {                   /* on a missing row: the comb's memory of just this row, after two before */
            uint32_t t0 = au_time() + ms_frames(60);
            int32_t r;
            au_cut(2);
            for (r = 2; r >= 1; r--) { uint8_t c = mb.tune[(sr - r) & (MB_STEPS - 1)]; if (c) sfx_tine(mb.comb[c - 1u], 40, t0 + ms_frames((uint32_t)(2 - r) * 300u), 128, 2); }
            if (mb.tune[sr]) sfx_tine_worn(mb.comb[mb.tune[sr] - 1u], t0 + ms_frames(650), 2);
            mb.rattle_t = 0;
        } else mb_crank(1);                     /* anywhere else: the whole worn tune */
    }
    return 0;
}

#define MB_CY 98                                /* the comb's row: the cylinder's front */
#define MB_R 54
#define MB_X0 29                                /* the first tine; the cylinder spans the screen */
#define MB_DX 26
#define MB_SY 176                               /* the beat strip: the sixteen rows, laid out left to right */
#define MB_SX 16
#define MB_SW 13
static void mb_draw(uint32_t won)
{
    int32_t a, s, c, pos, row_now;
    uint32_t now = au_time();
    if (won && !mb.playing) {                   /* the finale: it plays, twice */
        if (!mb.notes_t) { mb.notes_t = fm1_ms; mb_crank(0); mb.playing = 3; }
    }
    if (mb.playing) {
        int32_t el = (int32_t)(now - mb.play_at);
        pos = el > 0 ? (int32_t)((uint32_t)el * 256u / ms_frames(MB_STEP_MS)) : 0;
        if (mb.playing == 3 && el > (int32_t)ms_frames(MB_STEPS * MB_STEP_MS + 300u)) { mb_crank(0); mb.playing = 3; }
        s = pos >> 8;                           /* pluck: light the tine the comb row is striking (and its key) */
        if (s < MB_STEPS) {
            uint8_t cc = mb.playing == 2 ? mb.tune[s] : mb_row(s);
            if (cc && (pos & 255) < 60) mb.pluck_t[cc - 1u] = fm1_ms;
        }
    } else {
        int32_t d = mb.target * 256 - mb.pos;
        mb.pos += d / 3 + (d > 0) - (d < 0);
        pos = mb.pos;
    }
    row_now = !mb.playing ? (mb.target & (MB_STEPS - 1)) : ((pos >> 8) & (MB_STEPS - 1));
    /* the box opened, its lid gone: black lacquer, a walnut rim */
    g_rect(0, STAGE_Y0, SW, STAGE_Y1 - STAGE_Y0, RGB(10, 6, 12));
    g_rframe(4, STAGE_Y0 + 2, SW - 8, STAGE_Y1 - STAGE_Y0 - 4, 6, 3, RGB(90, 48, 30));
    /* the cylinder, wide: shaded by its angle, band by band from the back to the comb */
    for (a = -36; a <= 90; a += 2) {
        uint32_t th = (uint32_t)(a * 182);
        int32_t y0 = MB_CY - ((MB_R * mo_sin(th)) >> 15), y1 = MB_CY - ((MB_R * mo_sin(th + 364u)) >> 15);
        int32_t lit = (mo_cos(th - 5460u) >> 7) + 40;
        g_rect(MB_X0 - 15, y1, MB_DX * 7 + 30, y0 - y1 + 1, mix565(C_BRASS_DD, C_GOLD, (uint32_t)clampi(lit, 0, 256)));
    }
    g_rect(MB_X0 - 20, MB_CY - MB_R, 5, MB_R + 36, C_BRASS_D);                   /* its end caps */
    g_rect(MB_X0 + MB_DX * 7 + 15, MB_CY - MB_R, 5, MB_R + 36, C_BRASS_D);
    for (s = (pos >> 8) - 4; s <= (pos >> 8) + 9; s++) {   /* its rows: an engraved line each, the pins */
        int32_t th = (s * 256 - pos) * 4096 / 256, y, sc;
        uint8_t o, p;
        if (th < -6500 || th > 16000) continue;
        y = MB_CY - ((MB_R * mo_sin((uint32_t)th)) >> 15);
        sc = mo_cos((uint32_t)th) >> 12;          /* 0..8: foreshortening */
        g_rect(MB_X0 - 13, y, MB_DX * 7 + 26, 1, mix565(C_BRASS_D, C_BRASS, 120));
        o = mb.orig[s & (MB_STEPS - 1)];
        p = mb.pin[s & (MB_STEPS - 1)];
        if (o) { g_disc(MB_X0 + (o - 1) * MB_DX, y, 2 + sc / 2, C_BRASS_DD); g_disc(MB_X0 + (o - 1) * MB_DX - 1, y - 1, sc / 4, C_BRASS_D); }   /* an old pin, worn */
        if (p) { g_disc(MB_X0 + (p - 1) * MB_DX, y, 2 + sc / 2, C_STEEL_D); g_disc(MB_X0 + (p - 1) * MB_DX - 1, y - 1, sc / 3, C_CYAN); }    /* yours: blued steel */
        if (!o && !p && mb.missing[s & (MB_STEPS - 1)] && sc > 3)                  /* where one fell out: an empty socket on the row */
            g_rframe(MB_X0 - 13, y - 2, MB_DX * 7 + 26, 5, 2, 1, C_AMBER);
        if ((s & (MB_STEPS - 1)) == 0) g_rect(MB_X0 - 15, y - 1, 4, 3, C_RUBY);     /* the tune's start */
    }
    if (!mb.playing) { g_rect(MB_X0 - 19, MB_CY - 1, 3, 3, C_GLOW); g_rect(MB_X0 + MB_DX * 7 + 16, MB_CY - 1, 3, 3, C_GLOW); }   /* the row at the comb */
    /* the comb: a steel base, eight teeth up to the pins; a plucked tooth trembles; the letters under it */
    g_rect(MB_X0 - 16, MB_CY + 28, MB_DX * 7 + 32, 12, C_STEEL_D);
    g_rect(MB_X0 - 16, MB_CY + 28, MB_DX * 7 + 32, 2, C_STEEL_L);
    for (c = 0; c < 8; c++) {
        int32_t x = MB_X0 + c * MB_DX, wob = 0;
        uint32_t el = fm1_ms - mb.pluck_t[c];
        if (el < 400u) wob = (mo_sin(el * 9000u) * (int32_t)(400u - el) / 400) >> 14;
        g_thick(x, MB_CY + 29, x + wob, MB_CY + 2, 6, el < 400u ? C_STEEL_L : C_STEEL);
        g_text_c(x, MB_CY + 42, &FONT_S, MB_NAME[c], el < 400u ? C_CREAM : C_PAPER);
    }
    /* the beat strip: every row of the tune at once. Gold, an old pin; cyan, yours; an amber ?, one still to set;
     * after a crank each row says true (green) or not (red); the row at the comb underlined */
    for (s = 0; s < MB_STEPS; s++) {
        int32_t x = MB_SX + s * MB_SW, y = MB_SY, cur = s == row_now;
        uint8_t o = mb.orig[s], p = mb.pin[s], n = o ? o : p;
        uint16_t fill = o ? C_GOLD : p ? C_CYAN : RGB(28, 20, 34);
        int result = mb.row_ok_t && fm1_ms - mb.row_ok_t < 5000u && !mb.playing;
        if (mb.playing && cur) fill = C_CREAM;
        g_rrect(x, y, MB_SW - 2, 22, 2, fill);
        if (n) { char t[2] = {MB_NAME[n - 1u][0], 0}; g_text_c(x + (MB_SW - 2) / 2, y + 3, &FONT_S, t, RGB(16, 10, 20)); }
        else if (mb.missing[s]) { g_rframe(x, y, MB_SW - 2, 22, 2, 1, C_AMBER); g_text_c(x + (MB_SW - 2) / 2, y + 3, &FONT_S, "?", C_AMBER); }
        else g_disc(x + (MB_SW - 2) / 2, y + 11, 1, C_PAPER3);   /* (a rest: nothing played here) */
        if (result) g_rect(x, y + 24, MB_SW - 2, 3, mb.row_ok >> s & 1u ? C_VERD : C_RUBY);
        if (cur && !mb.playing) g_rect(x - 1, y + 29, MB_SW, 3, C_ROSE);
        if (!(s & 3)) g_rect(x - 2, y - 4, 1, 30, RGB(70, 50, 60));   /* (the bars: four rows each) */
    }
    /* the words: the row, what is wrong with it, the last crank's count */
    {
        char t[24], *q = t;
        if (mb.playing == 2) g_text_c(120, STAGE_Y0 + 6, &FONT_S, T_MB_MEMORY, C_ROSE);
        else if (!mb.playing) {
            q = str_cat(q, T_MB_ROW " "); fmt_u(q, (uint32_t)row_now + 1u);
            g_text(14, STAGE_Y0 + 6, &FONT_S, t, C_CREAM);
            if (mb.missing[row_now] && !mb.pin[row_now]) g_text(86, STAGE_Y0 + 6, &FONT_S, T_MB_MISSING, C_AMBER);
        }
        if (mb.true_rows >= 0 && !won && mb.playing != 2) {
            q = t; fmt_u(q, (uint32_t)mb.true_rows); while (*q) q++;
            q = str_cat(q, "/16 " T_MB_TRUE);
            g_text_r(SW - 14, STAGE_Y0 + 6, &FONT_S, t, mb.true_rows == MB_STEPS ? C_VERD : C_GLOW);
        }
        if (fm1_ms - mb.rattle_t < 700u) g_text_c(120, MB_SY - 18, &FONT_S, T_MB_OLD_PIN, C_GLOW);
    }
    if (won) {                                  /* the finale: notes rise from the comb */
        uint32_t i;
        for (i = 0; i < 10u; i++) {
            uint32_t el = (fm1_ms - mb.notes_t + i * 377u) % 2600u;
            int32_t x = MB_X0 + (int32_t)((i * 7u) % 8u) * MB_DX + ((mo_sin(el * 50u + i * 9000u) * 8) >> 15);
            int32_t y = MB_CY + 20 - (int32_t)el * 110 / 2600;
            uint16_t col = mix565(C_GLOW, RGB(70, 86, 108), el * 256u / 2600u);
            g_disc(x, y, 3, col);
            g_thick(x + 3, y, x + 3, y - 9, 2, col);
            g_thick(x + 3, y - 9, x + 7, y - 6, 2, col);
        }
    }
    for (c = 0; c < 8; c++) {                   /* key LEDs: the comb's keys glow; a struck tine lights its key */
        fm1_led_key_glow(KEY_LED(MB_KEY[c]), 1);
        fm1_led_key(KEY_LED(MB_KEY[c]), fm1_ms - mb.pluck_t[c] < 160u || (!mb.playing && mb_row(mb.target) == c + 1));
    }
}

static void mb_leave(void) { kb_hook = 0; au_cut(2); ui_keys_clear(); led_btn_blink = 0; }
static const game_t G_MUSICBOX = {BIT(BTN_EDIT) | BIT(BTN_PLAY) | BIT(BTN_REC), 5200,
                                  mb_enter, mb_update, mb_draw, mb_leave};
