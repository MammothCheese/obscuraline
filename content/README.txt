OBSCURA LINE - THE CONTENT FOLDER
=================================

  puzzles.txt   the chapters, in play order: each one's mechanism type and how that puzzle is set up
  script.txt    every word on screen: titles, help cards, footers, plaque words, story pages, messages, credits
  lines.txt     the line: its stations and each stretch (limits, curves, scenery, events, timetables)
  enoden.txt    the Enoden coast line, in the same form

Edit them, then build as usual:

  ./build.sh                                   the firmware (runs tools/content.py first)
  sh tests/run_host.sh tests/walkthrough.txt   the whole game on a computer, with screenshots

tools/content.py checks both files and stops with "file:line: what is wrong" before anything is built.
It writes firmware/gen/mo_content.h (generated: never edit it by hand).

  python3 tools/content.py --types             every mechanism type, its settings, defaults and limits


EDIT A PUZZLE
-------------
Change its settings in puzzles.txt (ranges give a fresh puzzle each play) and its words in script.txt.
If you change a count the help card mentions ("four reeds", "six bolts"), change the help text too.


ADD A PUZZLE OF AN EXISTING TYPE
--------------------------------
1. puzzles.txt: copy a block of that type, give it a new [id], put it where it belongs in the story.
2. script.txt:  add [chapter id] with numeral, title, help_title, help, footer, word, and in:/out: pages.
3. Build. That is all: the title screen, saving, skipping and the story all follow.

At most 16 chapters. Story pages use the existing pictures (the art names are listed at the top of
script.txt); a new picture needs a vignette in tools/style/bake.py (STORY_ARTS, same order as ARTS in
tools/content.py) and `python3 tools/style/bake.py`.


ADD A NEW TYPE OF MECHANISM (C code)
------------------------------------
1. firmware/src/g_<name>.c: the mechanism, modelled on any g_*.c:
     enter()   a new puzzle: read settings with PRM(P_<TYPE>_<NAME>), prm_pick(P_..._LO, P_..._HI),
               LST(L_<TYPE>_<NAME>)->v[] / ->n, cur_ch->str[] (string lists), cur_ch->done
     update()  one frame of play (in.enc[], in.press, in.kdown ...); return 1 when solved
     draw(won) the scene; won > 0 is the finale's milliseconds
     leave()   tidy up (sounds, key LEDs), or 0
   and   static const game_t G_<NAME> = {buttons lit, finale ms, enter, update, draw, leave};
   Every word it shows: a T_<KEY> from script.txt [ui].
2. tools/content.py: add "<name>" to TYPES with its settings (kind, default, lo, hi, description).
   The order of TYPES is the order of MECH_* in C.
3. firmware/src/obscura.c: #include "g_<name>.c" with the others.
4. firmware/src/mo_story.c: add &G_<NAME> to MECH_GAME[] in the same place as in TYPES.
5. firmware/src/mo_main.c: a case in mo_cheat() that puts it in a solved state (hold SAVE to skip).
6. Use it in puzzles.txt (type = <name>) with its words in script.txt.

The chip's rules hold for all C code: integers only, no 64-bit division (docs/RULES.md).


THE TYPES TODAY
---------------
  story       pages only                         reeds       tune reeds by ear (needle, beating, strobe)
  escapement  regulate a balance (timegrapher)   cipher      three-wheel cipher disk
  gears       gear train to a ratio              slider      sliding-panel cabinet (tools/slider.py)
  musicbox    re-pin a cylinder by ear           astro       hands of coprime periods, cranks
  bird        call and response (tines)          rings       gravity maze through turning rings
  safe        listen a combination lock open     movement    build a watch train by eye
  robot       the daily robots (Workshop)        master      tempo, dance steps, song

THE WORKSHOP
------------
A [workshop] block in puzzles.txt (type robot) and [chapter workshop] in script.txt make the endless daily
robots, chosen from the title screen once chapter I is done. Robot layouts follow the FM-1's parts:
1 calibration (SELECT PRESETS ALGORITHM, OCT- OCT+), 2 relays (K1-K4, FX..GLO, HOME..REC), 3 voice box
(the keys), 4 = 1+2, 5 = 1+3, 6 = 2+3, 7 all. The first three robots are one part each, then `layout` picks.

NEW CABINET LAYOUTS
-------------------
python3 tools/slider.py 8 20 prints layouts whose shortest solution is 8..20 moves; paste them as `layout`
lines. Every layout is solved when the content compiles (an unsolvable one stops the build).
