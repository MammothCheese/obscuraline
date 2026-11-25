#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Mechanica Obscura: compiles content/puzzles.txt and content/script.txt into firmware/gen/mo_content.h.

    python3 tools/content.py            (build.sh and tests/run_host.sh run it for you)
    python3 tools/content.py --types    (every mechanism type and its settings, with defaults and limits)

puzzles.txt says WHICH chapters there are, in play order, which mechanism each one uses and how that puzzle is
set up. script.txt holds EVERY word that shows up: chapter titles, help cards, footers, plaque words, the story
pages, the in-game messages and the credits. Both are checked here; a mistake stops the build with the file and
line, never a broken firmware. The schema below is the framework: a new chapter of an existing type needs only
text; a new TYPE needs its C file (firmware/src/g_*.c) and an entry here (see content/README.txt).
"""
import re, sys
from math import gcd
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))   # (tools/slider.py)
PUZ = ROOT / "content/puzzles.txt"
LINES = ROOT / "content/lines.txt"
ENODEN = ROOT / "content/enoden.txt"
SCR = ROOT / "content/script.txt"
OUT = ROOT / "firmware/gen/mo_content.h"

# ---- the mechanism types and their settings.
# kind: int (one number), range (a..b), list (numbers on one line), lists (a key repeated: one list a line,
# each of `sub` numbers), strings (a key repeated: one text a line). lo/hi bound every number.
TYPES = {
    "story": {"doc": "pages only, no puzzle (the prologue, interludes)", "c": None, "set": {}},
    "escapement": {"doc": "regulate a balance by ear and a timegraph (rate on K1, beat error on K2)", "c": "G_ESCAPEMENT", "set": {
        "rate_error": ("range", (4, 11), 1, 16, "hidden rate error, regulator detents (each 0.3 ms a beat)"),
        "beat_error": ("range", (3, 7), 1, 12, "hidden beat error, stud detents (each 3 ms between tick and tock)"),
        "beat_ms": ("int", 200, 100, 500, "the master clock's beat in ms (200 = 18000 bph)"),
        "hold_ticks": ("int", 14, 4, 60, "true beats in a row that solve it"),
    }},
    "watch": {"doc": "a timegrapher: K1 speed (flatten the trace), K2 balance (join its two lines), K3 power (into the marked range)", "c": "G_WATCH", "set": {
        "hold_ms": ("int", 3000, 1000, 10000, "ms all three must stay right"),
    }},
    "ghost": {"doc": "a signal box hearing four decades (K1..K4): tune each to the message of one conversation across time", "c": "G_GHOST", "set": {
        "channels": ("int", 4, 4, 4, "decades (four)"),
    }},
    "photo": {"doc": "restore an old glass negative: exposure, focus, contrast, development (K1..K4)", "c": "G_PHOTO", "set": {
        "hold_ms": ("int", 1200, 300, 4000, "ms the print must stay right"),
    }},
    "gearboard": {"doc": "build a train of gears on a board of pegs to turn the hand(s) the right way (K1 K2 move, K3 gear)", "c": "G_GEARBOARD", "set": {
        "hands": ("int", 1, 1, 2, "hands to drive (two: the train must branch)"),
        "chain": ("int", 1, 0, 2, "1 medium gears only, 2 small and large in turn, 0 either"),
        "rusted": ("range", (2, 3), 0, 6, "rusted gears that block or jam other routes"),
        "spares": ("range", (1, 2), 0, 4, "gears in stock beyond those the solution needs"),
    }},
    "musicbox": {"doc": "re-pin a music box cylinder by ear (K1 turns, keys set pins, REC hears the tune)", "c": "G_MUSICBOX", "set": {
        "tune": ("lists", [[3, 5, 8, 5, 4, 6, 5, 0, 3, 5, 7, 6, 5, 0, 8, 0], [1, 5, 3, 5, 2, 5, 4, 5, 3, 6, 5, 4, 3, 2, 1, 0],
                           [8, 7, 6, 5, 6, 5, 4, 3, 4, 2, 7, 5, 8, 0, 0, 0], [3, 0, 3, 4, 5, 0, 3, 0, 6, 5, 4, 3, 2, 0, 1, 0]],
                 0, 8, "tunes to pick from: 16 rows each, 1..8 = tine C..C, 0 = rest", 16),
        "missing": ("range", (5, 6), 1, 12, "pins that fell out"),
        "step_ms": ("int", 270, 120, 600, "ms a row when cranked"),
        "comb": ("list", [72, 74, 76, 77, 79, 81, 83, 84], 36, 108, "the eight tines' MIDI notes"),
    }},
    "bird": {"doc": "call and response with a singing automaton on five keys", "c": "G_BIRD", "set": {
        "notes": ("list", [84, 86, 88, 91, 93], 48, 108, "the bird's five MIDI notes (keys C D E G A)"),
        "rounds": ("int", 4, 1, 6, "songs to answer"),
        "first_len": ("int", 3, 2, 6, "notes in the first song (each song one longer)"),
        "show_rounds": ("int", 1, 0, 6, "songs in which the keys light with the notes"),
    }},
    "rhythm": {"doc": "play the lead over the line's backing: a melody down six rails (keys C4..A4), some notes held", "c": "G_RHYTHM", "set": {
        "bars": ("int", 16, 4, 80, "bars of the song (16 sixteenths each, at the song's tempo)"),
        "density": ("int", 1, 1, 3, "1 two notes a bar, 2 most beats, 3 beats and some offbeats"),
        "pass": ("int", 70, 0, 95, "the share of the points to pass, in % (perfect 2, good 1; 0: none)"),
        "style": ("int", 0, 0, 2, "0 the synthwave rails, 1 the seaside tram (bells, REC cues), 2 the festival train (drums, lanterns)"),
        "song": ("int", 0, 0, 12, "the backing: a song of the line (MU_SONGS)"),
    }},
    "switches": {"doc": "a train controller's screen: throw levers (keys C..A) to route a train to its platform", "c": "G_SWITCHES", "set": {
        "rounds": ("int", 2, 1, 4, "routes to set"),
        "big": ("int", 0, 0, 1, "1: more tracks and levers (a big station's control)"),
    }},
    "exchange": {"doc": "a telephone exchange: patch incoming lines (white keys) to stations (black keys) by their call signs", "c": "G_EXCHANGE", "set": {
        "lines": ("int", 6, 3, 6, "lines on the board"),
        "rounds": ("int", 2, 1, 2, "boards (the second: only the rhythm comes down the wire)"),
    }},
    "radio": {"doc": "a broken radio: frequency, bandwidth, gain and a notch (K1..K4) to pull one voice out of the noise", "c": "G_RADIO", "set": {
        "lock_ms": ("int", 2000, 500, 6000, "ms the voice must stay clear to lock"),
    }},
    "pantograph": {"doc": "keep the bow on an irregular overhead wire: height and tension (K1, K2)", "c": "G_PANTO", "set": {
        "dist": ("int", 1200, 400, 3600, "metres of wire to run"),
        "pass": ("int", 85, 50, 99, "the share of the run in contact, in %"),
    }},
    "sequencer": {"doc": "a junction timetable as a step sequencer: trains through one diamond, sixteen steps, no conflicts", "c": "G_SEQ", "set": {
        "trains": ("int", 2, 2, 3, "trains in the first timetable (each next one a train more, 3 at most: each stands 2 steps in the diamond twice a loop)"),
        "rounds": ("int", 2, 1, 2, "timetables"),
    }},
    "shunting": {"doc": "a shunting yard: three sidings and a headshunt; make up the train asked for in siding A", "c": "G_SHUNT", "set": {
        "rounds": ("int", 1, 1, 2, "trains to make up (the second has six wagons, four to order)"),
    }},
    "safe": {"doc": "open a combination lock by listening for the gates", "c": "G_SAFE", "set": {
        "numbers": ("int", 40, 20, 60, "numbers on the dial"),
        "wheels": ("int", 3, 1, 5, "wheels (directions alternate: right, left, right...)"),
        "near": ("int", 2, 0, 3, "numbers either side of a gate that already clunk, fainter"),
    }},
    "reeds": {"doc": "tune reeds to a chord by ear: beating and a strobe (K1..K4)", "c": "G_REEDS", "set": {
        "chord": ("lists", [[0, 4, 7, 11], [0, 3, 7, 10], [0, 4, 7, 9], [0, 5, 7, 12], [0, 3, 7, 12]], 0, 24,
                  "chord shapes to pick from: 4 intervals in semitones above the root", 4),
        "root": ("range", (55, 60), 36, 84, "the chord's root, MIDI note"),
        "error": ("range", (50, 240), 10, 400, "hidden mistuning in cents (made a multiple of step)"),
        "step": ("int", 10, 5, 50, "cents a detent"),
    }},
    "cipher": {"doc": "read a message enciphered on three wheels in turn (K1..K3)", "c": "G_CIPHER", "set": {
        "message": ("strings", ["MEET UNDER THE OLD OPERA", "WE DANCE BELOW THE TOWER", "BRING THE BOX AT NIGHT",
                                "THE CLOCK WILL SHOW THE HOUR", "FOLLOW THE CANAL TO THE GATE"], 0, 28,
                    "messages to pick from: capitals and spaces, 28 characters at most"),
        "shift": ("range", (3, 23), 1, 25, "each wheel's hidden turn"),
    }},
    "slider": {"doc": "a sliding-panel cabinet: free the key panel through the opening (K1 picks, K2 slides)", "c": "G_SLIDER", "set": {
        "layout": ("grids", ["..DD.. ..AAAE XXFG.E ..FG.. HI.GC. HIBBC."], 0, 0,
                   "layouts to pick from: six rows of six, '.' empty, X the key panel (third row), letters the panels"),
        "moves_max": ("int", 30, 1, 60, "refuse a layout whose shortest solution is longer than this"),
    }},
    "astro": {"doc": "set a tower clock to a sign: the hour (rung on bells), the moon's phase, a constellation (K1 K2 K3)", "c": "G_ASTRO", "set": {
        "bells": ("int", 1, 0, 1, "1: the hour is rung on bells (REC), count the strokes; 0: it is written"),
    }},
    "rings": {"doc": "a gravity maze: a ball falls through rings with gaps and barriers (K1..K4 turn them)", "c": "G_RINGS", "set": {
        "rings": ("int", 4, 2, 4, "rings, outer to inner (the ball must pass every one)"),
    }},
    "robot": {"doc": "a daily robot: calibration (SELECT PRESETS ALGORITHM, OCT), relays (K1-4, FX..REC), voice box (keys)", "c": "G_ROBOT", "set": {
        "layout": ("range", (1, 7), 1, 7, "1 calibration, 2 relays, 3 voice box, 4 = 1+2, 5 = 1+3, 6 = 2+3, 7 all (a range picks one)"),
        "cal_turns": ("int", 6, 1, 20, "random dial turns that put the gauges out"),
        "relay_presses": ("range", (3, 5), 1, 12, "random presses that open relays"),
        "code_len": ("range", (3, 4), 2, 5, "notes in the voice box code"),
    }},
    "sequence": {"doc": "several puzzles in a row (each its own block), then the dance: the masterwork", "c": "G_DANCE", "set": {
        "stages": ("ids", [], 0, 0, "the ids of the puzzle blocks to play in turn (blocks used here are not chapters themselves)"),
    }},
    "master": {"doc": "three stages: tempo by ear, dance steps onto a cam, a song on the comb", "c": "G_MASTER", "set": {
        "tempo": ("list", [72, 84, 96, 108, 120], 56, 144, "tempi the score may ask for (multiples of 4)"),
        "song": ("list", [3, 5, 8, 5, 4, 6, 5, 3, 5, 7, 6, 5, 8], 1, 8, "the song to play, tines 1..8 (C..C)"),
        "turns": ("int", 2, 0, 4, "turns in the dance"),
    }},
    "travel": {"doc": "drive a leg of the line from the cab and stop at the marker (content/lines.txt)", "c": "G_TRAVEL", "set": {
        "leg": ("leg", "", 0, 0, "the [leg id] in content/lines.txt to drive"),
    }},
}
GLOBAL = {"title_tune": ("list", [3, 5, 8, 5, 4, 6, 5, 0, 3, 5, 7, 6, 5, 0, 8, 0], 0, 8,
                         "the tune the title plays once the music box has sung (tines, 0 = rest)")}
ARTS = ["platform", "train", "cab", "signalbox", "workshop", "map", "timetable", "pocketwatch", "harbour", "archive", "tunnel",
        "observatory", "depot", "ghost", "door", "heart", "sakura", "keeper", "darktrain", "office", "seatram", "festtram"]   # tools/style/line_art.py ARTS, same order


class Err(Exception):
    pass


def sections(path):
    """[name] blocks of key = value lines, comments '#', with line numbers"""
    out, cur = [], None
    for n, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.split(" #")[0].rstrip() if not raw.lstrip().startswith("#") else ""
        if not line.strip():
            continue
        m = re.fullmatch(r"\[(.+)\]", line.strip())
        if m:
            cur = {"name": m.group(1).strip(), "line": n, "items": []}
            out.append(cur)
            continue
        if cur is None:
            raise Err(f"{path.name}:{n}: a line before any [section]")
        cur["items"].append((n, line.strip()))
    return out


def nums(s, where):
    try:
        return [int(v) for v in s.replace(",", " ").split()]
    except ValueError:
        raise Err(f"{where}: numbers expected, got '{s}'")


def parse_setting(kind, spec, vals, where):
    _, default, lo, hi, _doc, *sub = spec
    def chk(v):
        if not lo <= v <= hi:
            raise Err(f"{where}: {v} is outside {lo}..{hi}")
        return v
    if kind == "int":
        v = nums(vals[-1], where)
        if len(v) != 1: raise Err(f"{where}: one number")
        return chk(v[0])
    if kind == "range":
        m = re.fullmatch(r"\s*(-?\d+)\s*\.\.\s*(-?\d+)\s*", vals[-1]) or re.fullmatch(r"\s*(-?\d+)\s*", vals[-1])
        if not m: raise Err(f"{where}: a range a..b (or one number)")
        a = int(m.group(1)); b = int(m.group(2)) if m.lastindex == 2 else a
        if a > b: raise Err(f"{where}: {a}..{b} runs backwards")
        return (chk(a), chk(b))
    if kind == "list":
        v = [chk(x) for x in nums(vals[-1], where)]
        if not v: raise Err(f"{where}: at least one number")
        return v
    if kind == "lists":
        res = []
        for s in vals:
            v = [chk(x) for x in nums(s, where)]
            if len(v) != sub[0]: raise Err(f"{where}: each line needs {sub[0]} numbers, got {len(v)}")
            res.append(v)
        return res
    if kind == "ids":
        ids = vals[-1].split()
        for x in ids:
            if not re.fullmatch(r"[a-z][a-z0-9_]*", x): raise Err(f"{where}: '{x}' is not a block id")
        if not ids: raise Err(f"{where}: at least one id")
        return ids
    if kind == "grids":
        import slider
        res = []
        for s2 in vals:
            try:
                b = slider.parse(s2)
            except ValueError as e:
                raise Err(f"{where}: '{s2}': {e}")
            d = slider.solve(b)
            if d is None: raise Err(f"{where}: '{s2}' cannot be solved")
            res.append(" ".join(s2.split()))
        return res
    if kind == "leg":
        v = vals[-1].strip()
        if not re.fullmatch(r"[a-z][a-z0-9_]*", v): raise Err(f"{where}: '{v}' is not a leg id")
        return v
    if kind == "strings":
        for s in vals:
            if len(s) > hi: raise Err(f"{where}: '{s}' is longer than {hi}")
            if not re.fullmatch(r"[A-Z ]+", s): raise Err(f"{where}: '{s}': capitals and spaces only")
        return list(vals)
    raise Err(kind)


def load_puzzles():
    chapters, glob = [], {k: v[1] for k, v in GLOBAL.items()}
    for sec in sections(PUZ):
        where0 = f"puzzles.txt:{sec['line']}"
        if sec["name"] == "global":
            for n, line in sec["items"]:
                k, _, v = line.partition("=")
                k = k.strip()
                if k not in GLOBAL: raise Err(f"puzzles.txt:{n}: unknown global '{k}'")
                glob[k] = parse_setting(GLOBAL[k][0], GLOBAL[k], [v.strip()], f"puzzles.txt:{n}")
            continue
        cid = sec["name"]
        if not re.fullmatch(r"[a-z][a-z0-9_]*", cid): raise Err(f"{where0}: chapter id '{cid}': lower case, digits, _")
        raw = {}
        for n, line in sec["items"]:
            k, eq, v = line.partition("=")
            if not eq: raise Err(f"puzzles.txt:{n}: 'key = value' expected")
            raw.setdefault(k.strip(), []).append((n, v.strip()))
        if "type" not in raw: raise Err(f"{where0}: [{cid}] has no type")
        station = raw.pop("station")[-1][1] if "station" in raw else ""
        unl = raw.pop("unlocks")[-1][1].split() if "unlocks" in raw else []
        opens = raw.pop("opens")[-1][1].split() if "opens" in raw else []
        for u in unl:
            if u not in FEATURES: raise Err(f"{where0}: unlocks '{u}' (one of: {' '.join(FEATURES)})")
        typ = raw.pop("type")[-1][1]
        if typ not in TYPES: raise Err(f"{where0}: unknown type '{typ}' (python3 tools/content.py --types)")
        sch = TYPES[typ]["set"]
        setv = {}
        for k, spec in sch.items():
            if k in raw:
                lines = raw.pop(k)
                setv[k] = parse_setting(spec[0], spec, [v for _, v in lines], f"puzzles.txt:{lines[0][0]}")
            else:
                setv[k] = spec[1]
        if raw:
            k = next(iter(raw))
            raise Err(f"puzzles.txt:{raw[k][0][0]}: '{k}' is not a setting of type {typ}")
        validate(typ, setv, f"{where0} [{cid}]")
        if cid == "workshop" and typ != "robot": raise Err(f"{where0}: [workshop] must be type robot")
        chapters.append({"id": cid, "type": typ, "set": setv, "line": sec["line"], "station": station, "unlocks": unl, "opens": opens})
    ids = {c["id"]: c for c in chapters}
    staged = set()
    for c in chapters:
        if c["type"] == "sequence":
            if not c["set"]["stages"]: raise Err(f"puzzles.txt:{c['line']}: [{c['id']}] needs stages")
            for x in c["set"]["stages"]:
                if x not in ids: raise Err(f"puzzles.txt:{c['line']}: stage '{x}' is not a block in puzzles.txt")
                if ids[x]["type"] in ("story", "sequence", "robot"): raise Err(f"puzzles.txt:{c['line']}: stage '{x}' must be a puzzle")
                staged.add(x)
    for c in chapters: c["stage"] = c["id"] in staged
    if not [c for c in chapters if c["id"] != "workshop" and not c["stage"]]: raise Err("puzzles.txt: no chapters")
    if len([c for c in chapters if not c.get("stage") and c["id"] != "workshop"]) > 32: raise Err("puzzles.txt: 32 chapters at most (the save keeps 32 bits)")
    return chapters, glob


def validate(typ, s, where):
    if typ == "slider":
        import slider
        for g in s["layout"]:
            d = slider.solve(slider.parse(g))
            if d > s["moves_max"]: raise Err(f"{where}: '{g}' needs {d} moves (moves_max {s['moves_max']})")
    if typ == "master":
        if any(t % 4 for t in s["tempo"]): raise Err(f"{where}: tempi must be multiples of 4 (the governor's step)")
        if len(s["song"]) > 24: raise Err(f"{where}: the song: 24 notes at most")
    if typ == "musicbox":
        for t in s["tune"]:
            if sum(1 for v in t if v) <= s["missing"][1]: raise Err(f"{where}: a tune has fewer notes than pins to remove")
    if typ == "bird" and len(s["notes"]) != 5: raise Err(f"{where}: notes needs five MIDI notes")
    if typ == "musicbox" and len(s["comb"]) != 8: raise Err(f"{where}: comb needs eight MIDI notes")
    if typ == "reeds" and s["error"][0] < s["step"]: raise Err(f"{where}: the smallest error must be at least one step")


def load_script(chapters):
    ui, credits, chap = {}, [], {}
    for sec in sections(SCR):
        name, where0 = sec["name"], f"script.txt:{sec['line']}"
        if name == "ui":
            for n, line in sec["items"]:
                k, eq, v = line.partition("=")
                if not eq or not re.fullmatch(r"[a-z0-9_]+", k.strip()): raise Err(f"script.txt:{n}: 'key = text' expected")
                ui[k.strip()] = v.strip()
        elif name == "credits":
            for n, line in sec["items"]:
                style, _, text = line.partition(":")
                style = style.strip()
                if style not in ("gold", "text", "neon", "gap"): raise Err(f"script.txt:{n}: credits lines are gold:, text:, neon: or gap:")
                credits.append((style, text.strip()))
        elif name.startswith("chapter "):
            cid = name.split(None, 1)[1].strip()
            c = {"pages_in": [], "pages_out": [], "line": sec["line"]}
            for n, line in sec["items"]:
                m = re.fullmatch(r"(in|out)\s*:\s*([a-z]+)\s*\|\s*(robot|-)?\s*\|\s*(.+)", line)
                if m:
                    art, voice, text = m.group(2), m.group(3), m.group(4).strip()
                    if art not in ARTS: raise Err(f"script.txt:{n}: no story art '{art}' (one of: {' '.join(ARTS)})")
                    if len(text) > 150: raise Err(f"script.txt:{n}: a page holds about 150 characters ({len(text)})")
                    c["pages_" + m.group(1)].append((art, voice == "robot", text))
                    continue
                k, eq, v = line.partition("=")
                if not eq: raise Err(f"script.txt:{n}: 'key = text' or 'in: art | robot | text' expected")
                c[k.strip()] = v.strip()
            chap[cid] = c
        else:
            raise Err(f"{where0}: unknown section [{name}]")
    for ch in chapters:
        c = chap.get(ch["id"])
        if c is None: raise Err(f"script.txt: no [chapter {ch['id']}] for the chapter in puzzles.txt:{ch['line']}")
        need = ["title"] + ([] if ch["type"] == "story" else ["word"] if ch["type"] == "sequence" else ["help_title", "help", "footer", "word"])
        for k in need:
            if not c.get(k): raise Err(f"script.txt:{c['line']}: [chapter {ch['id']}] needs '{k}'")
        if ch["type"] == "story" and not c["pages_in"] and ch["id"] != "workshop": raise Err(f"script.txt:{c['line']}: a story chapter needs pages")
        if len(c.get("footer", "")) > 30: raise Err(f"script.txt:{c['line']}: footer longer than 30 characters")
        if len(c.get("controls", "").split("  ")) > 6: raise Err(f"script.txt:{c['line']}: six controls at most")
        if len(c.get("help", "")) > 160: raise Err(f"script.txt:{c['line']}: help longer than 160 characters (the card holds 8 lines)")
        if len(c.get("word", "")) > 10: raise Err(f"script.txt:{c['line']}: plaque word longer than 10 characters")
        ch["text"] = c
    for k in chap:
        if k not in [c["id"] for c in chapters]: raise Err(f"script.txt: [chapter {k}] is not in puzzles.txt")
    return ui, credits


FEATURES = ["key", "stopwatch", "horn", "whistle", "lights", "call", "synth", "voice"]   # what puzzles unlock in the train (F_* in C)
ZONES = ["country", "town", "river", "tunnel", "street", "sea", "houses", "sakura", "neon",
         "industrial", "riverside", "onsen", "fields", "woods", "rock", "mvillage", "beach", "festival"]
# a station's look: the zone round its platform (the first and last stretch of every leg that starts or ends there),
# its platform's length, and whether it has a canopy (2: a long one, 1: a short one, 0: a shelter only)
STYLES = {
    "metropolis": ("neon", 160, 2), "town": ("town", 120, 1), "industrial": ("industrial", 120, 1),
    "riverside": ("riverside", 80, 1), "onsen": ("onsen", 100, 1), "mvillage": ("mvillage", 70, 0),
    "mountain": ("rock", 50, 0), "woodland": ("woods", 45, 0), "rural": ("fields", 60, 0), "sakura": ("sakura", 90, 1),
    "underground": ("tunnel", 100, 2), "enoden": ("houses", 70, 1),
}
SKIES = ["day", "dusk", "night", "rain", "sunset", "dawn"]
EVENTS = {"oncoming": 1, "fans": 2, "check": 3, "whistle": 4}
MAXEV = 12
LEG_KEYS = ("from", "to", "dist", "time", "sky", "track", "platform", "loop", "limit", "curve", "grade", "zone",
            "event", "crossing", "line", "back_time", "back_sky", "synth", "music", "ride", "bars", "density", "pass", "rain")


def load_lines(LINES=LINES, returns=True):
    """content/lines.txt: [station id] name = ...; [leg id] from, to, dist, time, sky, track, platform, loop,
    limit / curve / grade / zone (at:value pairs), event (at:kind[:arg]), crossing (metres)"""
    stations, legs = [], []
    if not LINES.exists():
        return stations, legs
    for sec in sections(LINES):
        kind, _, sid = sec["name"].partition(" ")
        sid = sid.strip()
        where0 = f"lines.txt:{sec['line']}"
        if kind not in ("station", "leg") or not re.fullmatch(r"[a-z][a-z0-9_]*", sid):
            raise Err(f"{where0}: [station id] or [leg id] expected")
        kv = {}
        for n, line in sec["items"]:
            k, eq, v = line.partition("=")
            if not eq: raise Err(f"lines.txt:{n}: 'key = value' expected")
            kv[k.strip()] = (n, v.strip())
        if kind == "station":
            name = kv.get("name", (0, sid.upper()))[1]
            if not re.fullmatch(r"[A-Z0-9 .'-]{1,18}", name): raise Err(f"{where0}: name: 1..18 capitals, digits, spaces, . ' -")
            n, pv = kv.get("pos", (sec["line"], "50 50"))
            try: px, py = (int(v) for v in pv.split())
            except ValueError: raise Err(f"lines.txt:{n}: pos = x y (0..100 each)")
            if not (0 <= px <= 100 and 0 <= py <= 100): raise Err(f"lines.txt:{n}: pos = x y, each 0..100")
            hidden = kv.get("hidden", (0, "0"))[1] == "1"
            n, sty = kv.get("style", (sec["line"], "town"))
            if sty not in STYLES: raise Err(f"lines.txt:{n}: style '{sty}' (one of: {' '.join(STYLES)})")
            stations.append({"id": sid, "name": name, "x": px, "y": py, "hidden": int(hidden), "style": sty})
            continue
        sids = [x["id"] for x in stations]

        def get(k, d=None):
            if k in kv: return kv[k]
            if d is None: raise Err(f"{where0}: [leg {sid}] needs '{k}'")
            return (sec["line"], d)

        def num(k, lo, hi, d=None):
            n, v = get(k, d)
            try: x = int(v)
            except ValueError: raise Err(f"lines.txt:{n}: {k}: a number")
            if not lo <= x <= hi: raise Err(f"lines.txt:{n}: {k} {x} is outside {lo}..{hi}")
            return x

        def ival(b, n, k):
            try: return int(b)
            except ValueError: raise Err(f"lines.txt:{n}: {k}: '{b}' is not a number")

        def zval(b, n, k):
            if b not in ZONES: raise Err(f"lines.txt:{n}: zone '{b}' (one of: {' '.join(ZONES)})")
            return ZONES.index(b)

        def pairs(k, d, conv, lo, hi):
            n, v = get(k, d)
            out = []
            for tok in v.split():
                a, colon, b = tok.partition(":")
                if not colon: raise Err(f"lines.txt:{n}: {k}: at:value pairs ('{tok}')")
                try: at = int(a)
                except ValueError: raise Err(f"lines.txt:{n}: {k}: '{a}' is not a metre")
                val = conv(b, n, k)
                if not lo <= val <= hi: raise Err(f"lines.txt:{n}: {k}: {val} is outside {lo}..{hi}")
                if out and at <= out[-1][0]: raise Err(f"lines.txt:{n}: {k}: the metres must rise")
                out.append((at, val))
            if not out or out[0][0] != 0: raise Err(f"lines.txt:{n}: {k}: the first pair starts at 0")
            return out

        for k2 in kv:
            if k2 not in LEG_KEYS: raise Err(f"lines.txt:{kv[k2][0]}: unknown key '{k2}'")
        frm, to = get("from")[1], get("to")[1]
        for x in (frm, to):
            if x not in sids: raise Err(f"{where0}: no [station {x}] above it")
        dist = num("dist", 100, 9000)
        leg = {"id": sid, "from": sids.index(frm), "to": sids.index(to), "dist": dist, "time": num("time", 10, 900), "back_time": num("back_time", 0, 900, "0"),
               "plat": num("platform", 40, 200, "75"), "loop": num("loop", 0, 1, "0"),
               "lim": pairs("limit", None, ival, 5, 999), "cur": pairs("curve", "0:0", ival, -120, 120),
               "grd": pairs("grade", "0:0", ival, -40, 40), "zon": pairs("zone", "0:town", zval, 0, 99)}
        n, v = get("sky", "dusk")
        if v not in SKIES: raise Err(f"lines.txt:{n}: sky '{v}' (one of: {' '.join(SKIES)})")
        leg["sky"] = SKIES.index(v)
        n, v = get("back_sky", SKIES[leg["sky"]])
        if v not in SKIES: raise Err(f"lines.txt:{n}: back_sky '{v}' (one of: {' '.join(SKIES)})")
        leg["back_sky"] = SKIES.index(v)
        leg["music"] = num("music", 0, 12, "0")  # the song: 0..12 (7 the heart's, 8 the beach, 11 the tram's, 12 the festival's)
        n, v = get("ride", "-")                  # a ride: the line is a rhythm game, not a drive
        if v not in ("-", "tram", "festival"): raise Err(f"lines.txt:{n}: ride = tram or festival")
        leg["ride"] = ("-", "tram", "festival").index(v)
        leg["rbars"] = num("bars", 8, 80, "40"); leg["rdens"] = num("density", 1, 3, "2"); leg["rpass"] = num("pass", 0, 95, "0")
        n, v = get("rain", "random")
        if v not in ("random", "never"): raise Err(f"lines.txt:{n}: rain = random or never")
        leg["norain"] = int(v == "never")
        n, v = get("synth", "-")                 # a synth break: a stretch at one limit where the keys play into the music
        leg["brks"] = []                         # (several: start:end start:end, up to 3)
        if v != "-":
            for part in v.split():
                try: a, b = (int(x) for x in part.split(":"))
                except ValueError: raise Err(f"lines.txt:{n}: synth = start:end ... (metres)")
                if not (100 <= a < b <= dist - 150) or b - a < 200: raise Err(f"lines.txt:{n}: synth = start:end, 200 m or more, from 100 m to 150 m short of the stop")
                if leg["brks"] and a < leg["brks"][-1][1] + 150: raise Err(f"lines.txt:{n}: synth breaks in order, 150 m apart")
                leg["brks"].append((a, b))
            if len(leg["brks"]) > 3: raise Err(f"lines.txt:{n}: 3 synth breaks at most")
        leg["brk"] = leg["brks"][0] if leg["brks"] else (0, 0)
        n, v = get("track", "single")
        if v not in ("single", "double"): raise Err(f"lines.txt:{n}: track = single or double")
        leg["dbl"] = int(v == "double")
        evs = []
        n, v = get("event", "-")
        for tok in ([] if v == "-" else v.split()):
            parts = tok.split(":")
            try: at = int(parts[0])
            except ValueError: raise Err(f"lines.txt:{n}: event '{tok}': at:kind[:arg]")
            if len(parts) < 2 or parts[1] not in EVENTS: raise Err(f"lines.txt:{n}: event '{tok}': kinds {' '.join(EVENTS)}")
            arg = ival(parts[2], n, "event") if len(parts) > 2 else 0
            if parts[1] == "check" and not 5 <= arg <= 160: raise Err(f"lines.txt:{n}: a check needs its speed (at:check:40)")
            if parts[1] == "oncoming" and not 0 <= arg <= 160: raise Err(f"lines.txt:{n}: oncoming speed 0..160 (0: waiting)")
            if not 0 <= at <= dist + 100: raise Err(f"lines.txt:{n}: event at {at} m: past the end")
            evs.append((at, EVENTS[parts[1]], arg))
        if len(evs) > 8: raise Err(f"lines.txt:{n}: 8 events at most (whistle boards are added on their own)")
        leg["ev"] = evs or [(65535, 0, 0)]
        n, v = get("crossing", "-")
        leg["xing"] = [ival(x, n, "crossing") for x in ([] if v == "-" else v.split())] or [65535]
        n, v = get("line", "TSURU LOOP")
        if not re.fullmatch(r"[A-Z0-9 .'-]{1,16}", v): raise Err(f"lines.txt:{n}: line: 1..16 capitals, digits, spaces")
        leg["line"] = v
        leg["chap"] = None
        legs.append(leg)
    if len(stations) > 32: raise Err("lines.txt: 32 stations at most")
    finish_legs(stations, legs, returns)
    if len(legs) > 32: raise Err(f"lines.txt: {len(legs)} legs with their returns, 32 at most (the save keeps 32)")
    return stations, legs


def seg_mirror(segs, dist, neg=False):
    """[(at, v)] covering 0..dist, as driven the other way"""
    ends = [a for a, _ in segs[1:]] + [dist]
    out = [(dist - e, -v if neg else v) for (a, v), e in zip(segs, ends)]
    out.sort()
    res = []
    for a, v in out:
        if res and res[-1][1] == v: continue
        if res and a == res[-1][0]: res[-1] = (a, v); continue
        res.append((a, v))
    res[0] = (0, res[0][1])
    return res


def splice_zones(z, dist, z_from, z_to):
    """the stations' own surroundings at each end of a leg"""
    a, b = min(280, dist * 3 // 10), max(dist - 320, dist * 6 // 10)
    mid = []
    for i, (at, v) in enumerate(z):
        end = z[i + 1][0] if i + 1 < len(z) else dist
        lo, hi = max(at, a), min(end, b)
        if lo < hi: mid.append((lo, v))
    out = [(0, z_from)]
    for at, v in mid + [(b, z_to)]:
        if v == out[-1][1]: continue
        out.append((at, v))
    return out


def finish_legs(stations, legs, returns=True):
    """the returns of legs that have none, the stations' looks at the ends, and whistle boards"""
    pairs = {(l["from"], l["to"]) for l in legs}
    loops = {l["to"] for l in legs if l["loop"]}
    for l in []:                                                                  # (every line one way now: no returns made)
        if (l["to"], l["from"]) in pairs: continue
        d = l["dist"]
        r = dict(l, id=l["id"] + "_back", **{"from": l["to"], "to": l["from"]})
        r["lim"] = seg_mirror(l["lim"], d)
        r["cur"] = seg_mirror(l["cur"], d, neg=True)
        r["grd"] = seg_mirror(l["grd"], d, neg=True)
        r["zon"] = seg_mirror(l["zon"], d)
        r["loop"] = int(r["to"] in loops)
        evs = []
        for at, t, arg in l["ev"]:
            if t == 2 and at <= d: evs.append((max(0, d - at), t, arg))           # railfans, where they stood
            if t == 1 and arg: evs.append((max(0, d - at - 400), t, arg))        # a train coming the other way
        for i in range(1, len(r["lim"])):                                         # a check where each limit falls
            if r["lim"][i][1] < r["lim"][i - 1][1]: evs.append((r["lim"][i][0], 3, r["lim"][i][1]))
        r["ev"] = sorted(evs)[:8] or [(65535, 0, 0)]
        r["xing"] = sorted(d - x for x in l["xing"] if x != 65535) or [65535]
        r["chap"] = None
        if l["back_time"]: r["time"] = l["back_time"]                             # its own timetable, if given
        if l["brk"][1]: r["brk"] = (d - l["brk"][1], d - l["brk"][0])          # the synth break, where it falls this way
        r["sky"] = l["back_sky"]                                                  # and its own sky
        if r["lim"] and any(v >= 999 for _, v in r["lim"]): r["lim"] = [(x, min(v, 120)) for x, v in r["lim"]]   # (no limit only on the way in)
        legs.append(r)
        pairs.add((r["from"], r["to"]))
    for l in legs:
        st_f, st_t = stations[l["from"]], stations[l["to"]]
        zf, zt = STYLES[st_f["style"]][0], STYLES[st_t["style"]][0]
        if returns and not l.get("ride"):                                         # (the Enoden and the rides keep their own scenery to the platform)
            l["zon"] = [(a, ZONES.index(v) if isinstance(v, str) else v) for a, v in
                        splice_zones([(a, ZONES[v]) for a, v in l["zon"]], l["dist"], zf, zt)]
        l["plat"] = STYLES[st_t["style"]][1]
        l["canopy"] = STYLES[st_t["style"]][2]
        l["style_from"], l["style_to"] = list(STYLES).index(st_f["style"]), list(STYLES).index(st_t["style"])
        if st_t.get("hidden"): l["loop"] = 0
        if l["loop"] and not any(t == 1 and arg == 0 for _, t, arg in l["ev"]):
            l["ev"] = [e for e in l["ev"] if e[1]] + [(l["dist"] - 50, 1, 0)]  # a train waiting in the loop
        evs = [e for e in l["ev"] if e[1]]
        tz, rz = ZONES.index("tunnel"), ZONES.index("river")
        for i in range(1, len(l["zon"])):                                         # whistle boards: tunnels, bridges
            at, v = l["zon"][i]
            if v in (tz, rz) and l["zon"][i - 1][1] not in (tz, rz) and at >= 200 and at < l["dist"] - 60:
                evs.append((at - 30, 4, 0))
        if st_t.get("hidden") or st_f.get("hidden"): evs = []; l["loop"] = 0     # (the heart: nothing asked, no one waiting)
        for a, b in l.get("brks", []):                                            # each synth break: one limit, nothing else to do
            lims = [v for at, v in l["lim"] if at <= a]
            if not lims or any(a < at < b for at, _ in l["lim"]):
                raise Err(f"lines.txt: [leg {l['id']}]: the synth break {a}:{b} must sit inside one speed limit")
            if lims[-1] < 40: raise Err(f"lines.txt: [leg {l['id']}]: the synth break {a}:{b} sits in a {lims[-1]} km/h limit (40 or more)")
            if any(b <= at < b + 300 and v < lims[-1] for at, v in l["lim"]): raise Err(f"lines.txt: [leg {l['id']}]: a lower limit within 300 m after the synth break {a}:{b} (the cruise needs room to slow)")
            evs = [e for e in evs if not (a - 60 <= e[0] <= b + 20)]
        l["ev"] = sorted(evs)[:MAXEV] or [(65535, 0, 0)]


def resolve_lines(chapters, stations, legs):
    lids, sids = [l["id"] for l in legs], [s["id"] for s in stations]
    for c in chapters:
        if c["type"] == "travel":
            if c["set"]["leg"] not in lids: raise Err(f"puzzles.txt:{c['line']}: [{c['id']}] leg '{c['set']['leg']}' is not in lines.txt")
            c["set"]["leg"] = lids.index(c["set"]["leg"])
            legs[c["set"]["leg"]]["chap"] = c["id"]
            if not c["station"]: c["station"] = sids[legs[c["set"]["leg"]]["to"]]
        if c["station"] and c["station"] not in sids: raise Err(f"puzzles.txt:{c['line']}: station '{c['station']}' is not in lines.txt")
        for o in c.get("opens", []):
            if o not in lids: raise Err(f"puzzles.txt:{c['line']}: opens '{o}': no [leg {o}] in lines.txt")
        c["st"] = sids.index(c["station"]) if c["station"] else 255


def emit_en(o, stations, legs):
    """the Enoden: its own stations and legs (EN_*), the same leg type"""
    o.append("static const char *const EN_STATION[] = {" + (", ".join(cstr(s["name"]) for s in stations) or '""') + "};")
    for i, l in enumerate(legs):
        for k in ("lim", "cur", "grd", "zon"):
            o.append(f"static const tr_seg_t EL{i}_{k.upper()}[] = {{" + ", ".join(f"{{{a}, {v}}}" for a, v in l[k]) + "};")
        o.append(f"static const tr_ev_t EL{i}_EV[] = {{" + ", ".join(f"{{{a}, {t}, {g}}}" for a, t, g in l["ev"]) + "};")
        o.append(f"static const uint16_t EL{i}_XG[] = {{" + ", ".join(map(str, l["xing"])) + "};")
    o.append("static const tr_leg_t EN_LEGS[] = {")
    for i, l in enumerate(legs):
        o.append(f"    {{{l['dist']}, {l['time']}, EL{i}_LIM, EL{i}_CUR, EL{i}_GRD, EL{i}_ZON, EL{i}_EV, EL{i}_XG, "
                 f"{len(l['lim'])}, {len(l['cur'])}, {len(l['grd'])}, {len(l['zon'])}, {len(l['ev'])}, {len(l['xing'])}, "
                 f"{l['sky']}, {l['loop']}, 0, {l['plat']}, {l['from']}, {l['to']}, {cstr(l['line'])}, "
                 f"{l['canopy']}, {l['style_from']}, {l['style_to']}, {{0}}, 0, 0, 0, 0, 0, 0, 0}},")
    if not legs: o.append("    {0}")
    o.append("};")
    o.append(f"#define NENLEGS {len(legs)}u")


def emit_lines(o, stations, legs):
    o.append("typedef struct { uint16_t at; int16_t v; } tr_seg_t;")
    o.append("typedef struct { uint16_t at; uint8_t type, arg; } tr_ev_t;")
    o.append("typedef struct {\n    uint16_t dist, time_s;\n    const tr_seg_t *lim, *cur, *grd, *zon;\n    const tr_ev_t *ev;\n"
             "    const uint16_t *xing;\n    uint8_t nlim, ncur, ngrd, nzon, nev, nxing, sky, loop, dbl, plat, st_from, st_to;\n    const char *line;\n"
             "    uint8_t canopy, style_from, style_to;\n    uint16_t brk[6]; uint8_t nbrk;             /* the synth breaks: start, end each (nbrk of them) */\n    uint8_t music;                              /* the song (MU_SONGS) */\n    uint8_t ride, rbars, rdens, rpass;          /* a ride (1 tram, 2 festival): the rhythm game's bars, density, pass */\n    uint8_t norain;                             /* 1: never rains (the first lines) */\n} tr_leg_t;")
    o.append("enum { " + ", ".join(f"STY_{k.upper()}" for k in STYLES) + " };")
    o.append("typedef struct { uint8_t x, y, hidden, style; } tr_stpos_t;   /* the map: 0..100 each; hidden: until the rest is done */")
    o.append("static const tr_stpos_t TR_STPOS[] = {" + (", ".join(f"{{{s['x']}, {s['y']}, {s['hidden']}, STY_{s['style'].upper()}}}" for s in stations) or "{0}") + "};")
    o.append("static const char *const TR_STATION[] = {" + (", ".join(cstr(s["name"]) for s in stations) or '""') + "};")
    o.append(f"#define NSTATION {len(stations)}u")
    for i, l in enumerate(legs):
        for k in ("lim", "cur", "grd", "zon"):
            o.append(f"static const tr_seg_t TL{i}_{k.upper()}[] = {{" + ", ".join(f"{{{a}, {v}}}" for a, v in l[k]) + "};")
        o.append(f"static const tr_ev_t TL{i}_EV[] = {{" + ", ".join(f"{{{a}, {t}, {g}}}" for a, t, g in l["ev"]) + "};")
        o.append(f"static const uint16_t TL{i}_XG[] = {{" + ", ".join(map(str, l["xing"])) + "};")
    o.append("static const tr_leg_t TR_LEGS[] = {")
    for i, l in enumerate(legs):
        o.append(f"    {{{l['dist']}, {l['time']}, TL{i}_LIM, TL{i}_CUR, TL{i}_GRD, TL{i}_ZON, TL{i}_EV, TL{i}_XG, "
                 f"{len(l['lim'])}, {len(l['cur'])}, {len(l['grd'])}, {len(l['zon'])}, {len(l['ev'])}, {len(l['xing'])}, "
                 f"{l['sky']}, {l['loop']}, {l['dbl']}, {l['plat']}, {l['from']}, {l['to']}, {cstr(l['line'])}, "
                 f"{l['canopy']}, {l['style_from']}, {l['style_to']}, {{{', '.join(str(x) for ab in (l.get('brks', []) + [(0, 0)] * 3)[:3] for x in ab)}}}, {len(l.get('brks', []))}, {l['music']}, {l.get('ride', 0)}, {l.get('rbars', 0)}, {l.get('rdens', 0)}, {l.get('rpass', 0)}, {l.get('norain', 0)}}},")
    if not legs: o.append("    {0}")
    o.append("};")
    o.append(f"#define NTRLEGS {len(legs)}u")
    # the loop: the lines that lie on a circuit (not a bridge: take one away and its two stations still join up).
    # Found from the track, not the names, so renaming a line cannot change which lines make the loop.
    pairs = {tuple(sorted((l["from"], l["to"]))) for l in legs}
    def joined(a, b, skip):
        seen, todo = {a}, [a]
        while todo:
            x = todo.pop()
            for p in pairs:
                if p == skip or x not in p: continue
                y = p[1] if p[0] == x else p[0]
                if y not in seen: seen.add(y); todo.append(y)
        return b in seen
    inloop = [int(joined(l["from"], l["to"], tuple(sorted((l["from"], l["to"]))))) for l in legs]
    o.append("static const uint8_t TR_LEG_LOOP[] = {" + (", ".join(map(str, inloop)) or "0") + "};   /* 1: the leg is part of the loop */")
    o.append("#define TR_MAXEV_C " + str(MAXEV) + "u")


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def emit(chapters, glob, ui, credits, stations=(), legs=(), en=((), ())):
    o = ["/* SPDX-License-Identifier: GPL-3.0-only */",
         "/* generated by tools/content.py from content/puzzles.txt and content/script.txt: do not edit */",
         "#pragma once"]
    tlist = list(TYPES)
    o.append("enum { " + ", ".join(f"MECH_{t.upper()}" for t in tlist) + ", MECH_COUNT };")
    for t in tlist:                               # setting indices: numbers (ranges take two), lists, strings
        ni = li = 0
        for k, spec in TYPES[t]["set"].items():
            kind = spec[0]
            if kind in ("int", "leg"): o.append(f"#define P_{t.upper()}_{k.upper()} {ni}"); ni += 1
            elif kind == "range": o.append(f"#define P_{t.upper()}_{k.upper()}_LO {ni}\n#define P_{t.upper()}_{k.upper()}_HI {ni + 1}"); ni += 2
            elif kind in ("list", "lists"): o.append(f"#define L_{t.upper()}_{k.upper()} {li}"); li += 1
    o.append("#define MECH_GAMES {" + ", ".join(("&" + TYPES[t]["c"]) if TYPES[t]["c"] else "0" for t in tlist) + "}")
    o.append("typedef struct { const int16_t *v; uint16_t n; } mo_list_t;")
    o.append("typedef struct { int16_t art; uint8_t robot; const char *text; } page_t;")
    o.append("typedef struct {\n    const char *num, *name, *help_title, *help, *footer, *word, *done;\n    uint8_t type;\n"
             "    const page_t *in; uint8_t nin; const page_t *out; uint8_t nout;\n    const int32_t *prm; const mo_list_t *lst;\n"
             "    const char *const *str; uint8_t nstr;\n    const uint8_t *stages; uint8_t nstages;\n    const char *ctrl;                           /* the card's controls (two spaces between them) */\n} chapter_t;")
    for k, v in ui.items():
        o.append(f"#define T_{k.upper()} {cstr(v)}")
    for i, ch in enumerate(chapters):
        t, s, tx = ch["type"], ch["set"], ch["text"]
        p = f"CH{i}"
        for side in ("in", "out"):
            pages = tx["pages_" + side]
            if pages:
                o.append(f"static const page_t {p}_{side.upper()}[] = {{")
                for art, robot, text in pages:
                    o.append(f"    {{BG_ART_{art.upper()}, {int(robot)}, {cstr(text)}}},")
                o.append("};")
        prm, lists, strs = [], [], []
        for k, spec in TYPES[t]["set"].items():
            kind, v = spec[0], s[k]
            if kind in ("int", "leg"): prm.append(v)
            elif kind == "range": prm += list(v)
            elif kind == "list": lists.append(v)
            elif kind == "lists": lists.append([x for row in v for x in row])
            elif kind == "ids": pass
            elif kind == "strings": strs += v
            elif kind == "grids": strs += [x.replace(" ", "") for x in v]
        o.append(f"static const int32_t {p}_PRM[{max(1, len(prm))}] = {{{', '.join(map(str, prm)) or '0'}}};")
        for j, l in enumerate(lists):
            o.append(f"static const int16_t {p}_L{j}[{len(l)}] = {{{', '.join(map(str, l))}}};")
        o.append(f"static const mo_list_t {p}_LST[{max(1, len(lists))}] = {{" +
                 (", ".join(f"{{{p}_L{j}, {len(l)}u}}" for j, l in enumerate(lists)) or "{0, 0}") + "};")
        if strs:
            o.append(f"static const char *const {p}_STR[{len(strs)}] = {{{', '.join(cstr(x) for x in strs)}}};")
    def row(i, ch):
        tx, p = ch["text"], f"CH{i}"
        nin, nout = len(tx["pages_in"]), len(tx["pages_out"])
        nstr = sum(len(ch["set"][k]) for k, sp in TYPES[ch["type"]]["set"].items() if sp[0] in ("strings", "grids"))
        return "{" + ", ".join([cstr(tx.get("numeral", "")), cstr(tx["title"]), cstr(tx.get("help_title", "")),
                                cstr(tx.get("help", "")), cstr(tx.get("footer", "")), cstr(tx.get("word", "")),
                                cstr(tx.get("done", "")), f"MECH_{ch['type'].upper()}",
                                f"{p}_IN" if nin else "0", str(nin), f"{p}_OUT" if nout else "0", str(nout),
                                f"{p}_PRM", f"{p}_LST", f"{p}_STR" if nstr else "0", str(nstr),
                                f"{p}_STG" if ch["type"] == "sequence" else "0", str(len(ch["set"]["stages"])) if ch["type"] == "sequence" else "0",
                                cstr(tx.get("controls", tx.get("footer", "")))]) + "}"
    stage_list = [c["id"] for c in chapters if c["stage"]]
    for i, ch in enumerate(chapters):
        if ch["type"] == "sequence":
            o.append(f"static const uint8_t CH{i}_STG[{len(ch['set']['stages'])}] = {{{', '.join(str(stage_list.index(x)) for x in ch['set']['stages'])}}};")
    o.append("static const chapter_t STAGES[] = {" + (", ".join(row(i, ch) for i, ch in enumerate(chapters) if ch["stage"]) or "{0}") + "};")
    ws = [i for i, ch in enumerate(chapters) if ch["id"] == "workshop"]
    if ws:
        o.append("#define HAVE_WORKSHOP 1")
        o.append(f"static const chapter_t WORKSHOP_CH = {row(ws[0], chapters[ws[0]])};")
    else:
        o.append("#define HAVE_WORKSHOP 0")
    emit_lines(o, stations, legs)
    emit_en(o, en[0], en[1])
    o.append("enum { " + ", ".join(f"F_{f.upper()} = {1 << i}" for i, f in enumerate(FEATURES)) + " };")
    pl = [ch for ch in chapters if ch["id"] != "workshop" and not ch["stage"]]
    gate = []
    for l in legs:                                                              # each leg's gate: the chapter that opens it (255: none)
        base = l["id"][:-5] if l["id"].endswith("_back") else l["id"]
        g = [i for i, ch in enumerate(pl) if base in ch.get("opens", [])]
        gate.append(g[0] if g else 255)
    o.append("static const uint8_t TR_LEG_GATE[] = {" + (", ".join(map(str, gate)) or "255") + "};   /* each leg: the chapter whose solving opens it (255: open) */")
    o.append("static const uint8_t CHAP_UNLOCK[] = {" + (", ".join(str(sum(1 << FEATURES.index(u) for u in ch.get("unlocks", []))) for ch in chapters
             if ch["id"] != "workshop" and not ch["stage"]) or "0") + "};   /* each chapter: the train features it unlocks */")
    o.append("static const uint8_t CHAP_STATION[] = {" + (", ".join(str(ch.get("st", 255)) for ch in chapters
             if ch["id"] != "workshop" and not ch["stage"]) or "255") + "};   /* each chapter's station (255: none) */")
    o.append("static const chapter_t CHAP[] = {")
    for i, ch in enumerate(chapters):
        if ch["id"] == "workshop" or ch["stage"]: continue
        o.append("    " + row(i, ch) + ",")
    o.append("};")
    o.append("#define NCHAP (sizeof(CHAP) / sizeof(CHAP[0]))")
    chap_ids = [ch["id"] for ch in chapters if ch["id"] != "workshop" and not ch["stage"]]
    o.append("static const uint8_t TR_LEG_CHAP[] = {" + (", ".join(str(chap_ids.index(l["chap"]) if l.get("chap") in chap_ids else 255) for l in legs) or "255") + "};")
    tt = glob["title_tune"]
    o.append(f"static const int16_t TITLE_TUNE[{len(tt)}] = {{{', '.join(map(str, tt))}}};")
    o.append(f"#define TITLE_TUNE_N {len(tt)}u")
    o.append("typedef struct { uint8_t style; const char *text; } credit_t;   /* style 0 gold, 1 text, 2 neon, 3 gap */")
    o.append("static const credit_t CREDITS[] = {")
    for st, tx in credits:
        o.append(f"    {{{['gold', 'text', 'neon', 'gap'].index(st)}, {cstr(tx)}}},")
    o.append("};")
    o.append("#define NCREDITS (sizeof(CREDITS) / sizeof(CREDITS[0]))")
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:   # (LF on Windows too)
        f.write("\n".join(o) + "\n")


def show_types():
    for t, d in TYPES.items():
        print(f"type = {t}\n    {d['doc']}")
        for k, (kind, default, lo, hi, doc, *sub) in d["set"].items():
            dv = default if kind != "range" else f"{default[0]}..{default[1]}"
            if kind == "lists": dv = f"{len(default)} lines of {sub[0]}"
            if kind in ("strings", "grids"): dv = f"{len(default)} lines"
            print(f"    {k:13s} {kind:7s} {lo}..{hi}  default {dv}\n                  {doc}")
        print()


def main():
    if "--types" in sys.argv:
        show_types(); return
    try:
        chapters, glob = load_puzzles()
        stations, legs = load_lines()
        en = load_lines(ENODEN, returns=False)
        resolve_lines(chapters, stations, legs)
        ui, credits = load_script(chapters)
    except Err as e:
        print(f"content: {e}", file=sys.stderr)
        sys.exit(1)
    emit(chapters, glob, ui, credits, stations, legs, en)
    print(f"content: {len(chapters)} chapters, {len(stations)} stations, {len(legs)} legs -> {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
