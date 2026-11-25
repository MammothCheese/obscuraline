/* SPDX-License-Identifier: GPL-3.0-only */
/* MECHANICA OBSCURA: a story told in clockwork, for the M-VAVE FM-1.
 *
 * A city ruled by machines that have no use for art. Tobias Wren mends their clockwork, until a broken box
 * in a salvage heap turns out to be a music box. Each chapter is a mechanism to understand: regulate an
 * escapement by ear and by trace, build a gear train to a ratio, re-pin a music box cylinder, answer a
 * singing bird, listen a strongbox open.
 *
 * Built on Dinghy (zvenson/dinghy), itself Felucca's platform (Leo Kuroshita, Hügelton Instruments) as SLOOP
 * (isod89) carries it: platform.c is theirs and unchanged; it keeps the update path and the USB rescue.
 *
 *   mo_gfx.c     drawing: full-frame canvas, AA discs/rings, gears, text on paper
 *   mo_audio.c   sound: a scheduled, sample-true event engine (ticks, tines, chimes, whistles)
 *   mo_ui.c      panel input, the frame of each scene, help cards, the completion plaque, story pages
 *   g_*.c        the chapters' mechanisms
 *
 * Integers only (no FPU), no 64-bit division: docs/RULES.md. tests/host.c runs it all on a computer. */
#ifndef APP_NAME
#define APP_NAME "OBSCURA"
#endif
#define STORE_MAGIC 0x4C53424Fu                 /* "OBSL": our settings, never another Dinghy fork's (Mechanica: "MECO") */
#define CV_MAX (240u * 240u)                    /* one full frame in the pool (113 KB of 336) */
static void app_idle(void);
#define LCD_IDLE app_idle                       /* while a strip goes out: the keys still sound at once */
#include "platform.c"

#include "mo_gfx.c"
#include "mo_deco.c"
#include "mo_audio.c"
#include "mo_ui.c"
#include "mo_content.h"                         /* content/puzzles.txt + script.txt, via tools/content.py */
/* the chapter being played: the mechanisms read their settings from it */
static const chapter_t *cur_ch = &CHAP[0];
#define PRM(k) (cur_ch->prm[k])                 /* a number setting, P_<TYPE>_<NAME> */
#define LST(k) (&cur_ch->lst[k])                /* a list setting, L_<TYPE>_<NAME>: ->v[], ->n */
static int32_t prm_pick(int lo_k, int hi_k)     /* a value inside a range setting */
{
    int32_t lo = PRM(lo_k), hi = PRM(hi_k);
    return lo + (int32_t)rndn((uint32_t)(hi - lo + 1));
}
#include "g_escapement.c"
#include "g_gearboard.c"
#include "g_musicbox.c"
#include "g_bird.c"
#include "g_safe.c"
#include "g_reeds.c"
#include "g_cipher.c"
#include "g_slider.c"
#include "g_astro.c"
#include "g_rings.c"
#include "g_robot.c"
#include "g_master.c"
#include "g_travel.c"                          /* Obscura Line: the train, a leg of the line */
#include "g_rhythm.c"
#include "g_switch.c"                          /* the signal control: set the route */
#include "g_shunt.c"                           /* the depot yard: shunting */
#include "g_exchange.c"                        /* the telephone exchange: patch the stations by ear */
#include "g_radio.c"                           /* the railway radio: tune one voice out of the noise */
#include "g_panto.c"                           /* the pantograph: keep the bow on the wire */
#include "g_seq.c"                             /* the railway sequencer: a timetable as a step sequencer */
#include "g_ghost.c"                           /* the ghost signal: one conversation across four decades */
#include "g_photo.c"                           /* the negative photograph: print it again */
#include "g_watch.c"                           /* the driver's watch: time it by ear */
/* play along with the line's music (uses the drive's backing) */
#include "mo_story.c"
#include "mo_main.c"
