/* SPDX-License-Identifier: GPL-3.0-only */
/* Does every piece of text fit its box? Measured with the game's own fonts and its own boxes:
 *   story pages   at most 3 lines in the panel under the picture (212 px wide)
 *   cards         the heading inside its bar; the text above the controls; each control line inside the card
 *   headers       "numeral title" whole in the cab's header (it would be cut)
 *   footers       the chapter's and the screens' footers on the bottom bar
 *   credits       each line on the screen
 * Prints each that does not fit; exits 1 if any.
 *
 *   gcc -O2 -DDINGHY_HOST -Dmain=host_main -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o build/host/text_fit tests/text_fit.c
 *   build/host/text_fit */
#include "../firmware/src/obscura.c"
#undef main

static int bad;
static void fail(const char *what, const char *where, const char *s, const char *why)
{
    printf("  %-14s %-24s %-40s %s\n", what, where, why, s);
    bad++;
}
static int32_t wrap_lines(int32_t w, const char *s)   /* g_text_wrap's own breaking */
{
    int32_t lines = 0, cols = w / FONT_S.adv[0];
    if (cols > 63) cols = 63;
    while (*s) {
        int32_t n = 0, brk = -1;
        while (s[n] && s[n] != '\n' && n < cols) { if (s[n] == ' ') brk = n; n++; }
        if (s[n] && s[n] != '\n' && n == cols && brk > 0) n = brk;
        lines++;
        s += n;
        while (*s == ' ') s++;
        if (*s == '\n') s++;
    }
    return lines;
}
static void pages(const char *where, const page_t *p, uint32_t n)
{
    uint32_t i;
    char why[64];
    for (i = 0; i < n; i++) {
        int32_t l = wrap_lines(212, p[i].text);
        if (l > 3) { snprintf(why, sizeof why, "page: %d lines, 3 fit", (int)l); fail("story page", where, p[i].text, why); }
    }
}
static void card(const char *where, const char *title, const char *text, const char *ctrl)
{
    int32_t x = 8, y = STAGE_Y0 + 6, w = SW - 16, h = STAGE_Y1 - STAGE_Y0 - 10, n = 0, cy, l;
    const char *p = ctrl;
    char why[80];
    if (!title || !*title) return;
    if (text_w(&FONT_S, title) > w - 24) { snprintf(why, sizeof why, "card heading: %d px, %d fit", (int)text_w(&FONT_S, title), (int)(w - 24)); fail("card", where, title, why); }
    while (p && *p && n < 6) {
        const char *q = p;
        char a[64], b[64];
        uint32_t k = 0, m = 0;
        while (*q && !(q[0] == ' ' && q[1] == ' ')) q++;
        while (p + k < q && p[k] != ' ' && k < 63u) { a[k] = p[k]; k++; }
        a[k] = 0;
        while (p + k < q && p[k] == ' ') k++;
        while (p + k + m < q && m < 63u) { b[m] = p[k + m]; m++; }
        b[m] = 0;
        if (strlen(a) > 23u || strlen(b) > 23u) fail("card control", where, p, "cut at 23 characters");
        else if (x + 14 + text_w(&FONT_S, a) + 6 + text_w(&FONT_S, b) > x + w - 6) {
            snprintf(why, sizeof why, "control: %d px, %d fit", (int)(text_w(&FONT_S, a) + 6 + text_w(&FONT_S, b)), (int)(w - 20));
            fail("card control", where, p, why);
        }
        n++;
        while (*q == ' ') q++;
        p = q;
    }
    if (text_w(&FONT_S, T_HELP_FOOT) > w - 24) fail("card", where, T_HELP_FOOT, "card's last line: wider than the card");
    cy = y + h - 30 - n * 15;
    l = wrap_lines(w - 24, text);
    if (y + 32 + l * 16 > cy - 6) { snprintf(why, sizeof why, "card text: %d lines, %d fit", (int)l, (int)((cy - 6 - y - 32) / 16)); fail("card", where, text, why); }
}
static void header(const char *where, const char *num, const char *title)
{
    char t[80], why[64];
    snprintf(t, sizeof t, "%s%s%s", num && *num ? num : "", num && *num ? " " : "", title);
    if (26 + text_w(&FONT_S, t) > SW - 4) { snprintf(why, sizeof why, "header: %d px, %d fit", (int)text_w(&FONT_S, t), SW - 30); fail("header", where, t, why); }
}
static void footer(const char *where, const char *s)
{
    char why[64];
    if ((int32_t)strlen(s) * 8 > SW) { snprintf(why, sizeof why, "footer: %d px, %d fit", (int)strlen(s) * 8, SW); fail("footer", where, s, why); }
}
int main(void)
{
    uint32_t c, i;
    for (c = 0; c < NCHAP; c++) {
        const chapter_t *ch = &CHAP[c];
        pages(ch->name, ch->in, ch->nin);
        pages(ch->name, ch->out, ch->nout);
        card(ch->name, ch->help_title, ch->help, ch->ctrl);
        if (ch->name && *ch->name) header(ch->name, ch->num, ch->name);
        if (ch->footer && *ch->footer) footer(ch->name, ch->footer);
        if (ch->word && strlen(ch->word) > 10u) fail("plaque", ch->name, ch->word, "plaque: over 10 characters");
    }
    card("workshop", WORKSHOP_CH.help_title, WORKSHOP_CH.help, WORKSHOP_CH.ctrl);
    card("Enoden", T_EN_TITLE, T_EN_HELP, T_EN_CTRL);
    card("endless", T_EL_TITLE, T_EL_HELP, T_EL_CTRL);
    {   /* the first drive's lessons: the heading in its bar, the text in the 4 lines above the beads */
        static const char *const TT[4] = {T_TUT_POWER_T, T_TUT_LIMIT_T, T_TUT_STOP_T, T_TUT_SCORE_T}, *const TX[4] = {T_TUT_POWER, T_TUT_LIMIT, T_TUT_STOP, T_TUT_SCORE};
        for (i = 0; i < 4u; i++) {
            if (text_w(&FONT_S, TT[i]) > SW - 40) fail("lesson", TT[i], TT[i], "heading wider than its bar");
            if (wrap_lines(SW - 40, TX[i]) > 4) fail("lesson", TT[i], TX[i], "text: over 4 lines");
        }
    }
    for (i = 0; i < NTRLEGS; i++) header("line name", "", TR_LEGS[i].line);
    for (i = 0; i < NCREDITS; i++) {
        int32_t w = CREDITS[i].style == 3u ? 0 : CREDITS[i].style == 0u ? deco_w(CREDITS[i].text, 11) : text_w(&FONT_S, CREDITS[i].text);
        char why[64];
        if (w > SW - 8) { snprintf(why, sizeof why, "credits: %d px, %d fit", (int)w, SW - 8); fail("credits", "", CREDITS[i].text, why); }
    }
    printf(bad ? "%d DO NOT FIT\n" : "all text fits\n", bad);
    return bad ? 1 : 0;
}
