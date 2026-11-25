#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Writes tests/walkthrough.txt: a host-mode script through the whole game, built from content/puzzles.txt and
script.txt (so it follows any edit): every story page, every puzzle (a look, then SAVE held 2 s to skip it, its
finale), every stage of a sequence, the credits. Screenshots at the interesting moments.

    python3 tests/walkthrough.py && sh tests/run_host.sh tests/walkthrough.txt
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
import content

PLAY, SAVE = 12, 9
ev = []
t = 400


def at(dt, s):
    global t
    t += dt
    ev.append(f"{t} {s}")


def shot(dt=0): at(dt, "shot")


def pages(n):                                   # let each page type, take a picture, go on
    global t
    for _ in range(n):
        t += 3600
        ev.append(f"{t} shot")
        ev.append(f"{t + 50} btn {PLAY}")
        t += 300


def puzzle():                                   # the card, a look, a few turns, skip, the finale, on
    shot(400); at(300, f"btn {PLAY}")
    at(300, "enc 2 2"); at(300, "enc 3 1"); at(300, "enc 4 1"); shot(1200)
    at(300, f"btndown {SAVE}"); at(2100, f"btnup {SAVE}")
    shot(1500); shot(2500)
    at(4500, f"btn {PLAY}")                      # (after the longest finale and its plaque)


chapters, _ = content.load_puzzles()
content.load_script(chapters)
by_id = {c["id"]: c for c in chapters}
shot(100)
at(600, f"btn {PLAY}")
for ch in chapters:
    if ch["id"] == "workshop" or ch["stage"]:
        continue
    tx = ch["text"]
    pages(len(tx["pages_in"]))
    if ch["type"] == "sequence":
        for sid in ch["set"]["stages"]:
            puzzle()
        shot(2000); shot(2500); at(3500, f"btn {PLAY}")   # the sequence's own finale
    elif ch["type"] != "story":
        puzzle()
    pages(len(tx["pages_out"]))
shot(1500); shot(5000)                          # the credits
at(1000, "end")
open(Path(__file__).parent / "walkthrough.txt", "w").write("\n".join(ev) + "\n")
print(len(ev), "events, ends at", t)
