#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Mechanica Obscura: bakes the static backdrops into firmware/gen/mo_art.h.

Each scene's still art (the city, the rooms, the plates and cases the mechanisms sit in) is drawn here at 3x,
reduced to 240 x 240, quantized to a palette of up to 64 colours and stored as LZ4 blocks of 8-bit indices
(Dinghy's tools/lz4blk.py). The firmware decodes one into a 57.6 KB index buffer when a scene starts and
expands it through the palette into each frame; the mechanisms, hands, lights and text are drawn live on top.
Also writes the backdrops as PNGs into docs/style/bg/ to look at.

    python3 tools/style/bake.py
"""
import math, random, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mo_style import *
import lz4blk
import act2_art
import line_art                                 # Obscura Line's story vignettes

BG = OUT / "bg"

# ---------------------------------------------------------------- shared pieces
def robot_city(img, seed=5, signs=True, warm=False):
    """a city by and for machines: stepped towers with masts, gears set in their walls, conveyors and pipes
    bridging them, charging signs, rows of identical units marching on a gantry"""
    top, mid = ((40, 26, 18), (90, 54, 30)) if warm else ((8, 12, 26), (34, 30, 54))
    vgrad(img, 0, 150, top, mid)
    img = skyline(img, 132, seed, 0, 40, 104, (24, 26, 44) if not warm else (70, 46, 30),
                  [AMBER, (110, 140, 170), CYAN_D], 0.18)
    img = fog(img, 70, 130, (70, 66, 96) if not warm else (150, 100, 60), 0.5, seed)
    d = ImageDraw.Draw(img)
    rnd = random.Random(seed)
    # the near towers: dark, stepped, each with a mast and a red beacon
    xs = [-6, 34, 92, 150, 196]
    for i, x in enumerate(xs):
        w = rnd.randint(30, 44); h = rnd.randint(70, 118); t = 140 - h
        body = (12, 14, 24) if not warm else (40, 26, 18)
        d.rectangle([S(x), S(t), S(x + w), S(150)], fill=body)
        for k in range(1, 4):
            d.rectangle([S(x + w * .14 * k), S(t - k * 7), S(x + w - w * .14 * k), S(t - (k - 1) * 7)], fill=body)
        d.line([(S(x + w / 2), S(t - 21)), (S(x + w / 2), S(t - 40))], fill=body, width=S(1.2))
        d.ellipse([S(x + w / 2 - 1.5), S(t - 42), S(x + w / 2 + 1.5), S(t - 39)], fill=RED)
        for wy in range(int(t + 4), 148, 5):                 # window strips: machines need little light
            if rnd.random() < 0.5:
                d.rectangle([S(x + 3), S(wy), S(x + w - 3), S(wy) + SS - 1], fill=(30, 44, 64) if not warm else (90, 60, 30))
        if i in (1, 3):                                        # a great gear set in the wall
            gear(d, x + w / 2, t + 24, 12, 16, 0.3 * i, (60, 50, 34), (34, 28, 20), None, 4, 2.6)
    # conveyors and pipes between them
    for (y, x0, x1) in ((96, 20, 110), (78, 120, 210), (112, 60, 190)):
        d.rectangle([S(x0), S(y), S(x1), S(y + 3)], fill=(30, 30, 40))
        for x in range(x0, x1, 5):
            d.line([(S(x), S(y)), (S(x + 2), S(y + 3))], fill=(50, 50, 60), width=1)
        for x in range(x0 + 4, x1 - 4, 12):                    # crates of parts riding on them
            d.rectangle([S(x), S(y - 3), S(x + 4), S(y)], fill=(60, 50, 40))
    d.line([(S(0), S(64)), (S(240), S(58))], fill=(26, 26, 36), width=S(2))
    # a gantry with marching units, eyes lit
    d.rectangle([S(0), S(124), S(240), S(127)], fill=(20, 22, 32))
    for k in range(18):
        x = 6 + k * 13
        d.rectangle([S(x), S(116), S(x + 5), S(124)], fill=(18, 20, 30))
        d.ellipse([S(x), S(111), S(x + 5), S(116)], fill=(18, 20, 30))
        d.rectangle([S(x + 1), S(113), S(x + 4), S(114)], fill=CYAN if not warm else AMBER)
    if signs:
        img = neon_sign(img, 40, 46, "LADEN", CYAN, h=7)
        img = neon_sign(img, 160, 34, "WARTUNG", AMBER, h=6)
        img = neon_sign(img, 214, 70, "OEL", ROSE, h=7, vertical=True)
    return img

def lacquer_floor(img, y0=150, y1=240):
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(y0), W, S(y1)], fill=(6, 8, 14))
    return img

def workshop_interior(img):
    """Tobias's workshop: the one warm room. Amber lamp, a wall of clocks, a bench; the cold city through the window"""
    vgrad(img, 0, 150, (40, 24, 16), (70, 42, 24))
    d = ImageDraw.Draw(img)
    # the window, cyan city beyond
    win = Image.new("RGB", img.size, INK)
    win = robot_city(win, 3, signs=False)
    m = Image.new("L", img.size, 0); ImageDraw.Draw(m).rectangle([S(150), S(28), S(226), S(104)], fill=255)
    img.paste(win, (0, 0), m)
    d = ImageDraw.Draw(img)
    for x in (150, 188, 226):
        d.line([(S(x), S(28)), (S(x), S(104))], fill=(60, 36, 20), width=S(2.2))
    for y in (28, 66, 104):
        d.line([(S(150), S(y)), (S(226), S(y))], fill=(60, 36, 20), width=S(2.2))
    # the wall of clocks
    rnd = random.Random(8)
    for (cx, cy, r) in ((24, 44, 13), (56, 36, 9), (84, 52, 15), (40, 76, 10), (118, 38, 11), (70, 88, 8), (120, 82, 12)):
        d.ellipse([S(cx - r - 2), S(cy - r - 2), S(cx + r + 2), S(cy + r + 2)], fill=GOLD_D)
        d.ellipse([S(cx - r), S(cy - r), S(cx + r), S(cy + r)], fill=CREAM)
        for k in range(12):
            a = 2 * math.pi * k / 12
            d.line([(S(cx + r * .78 * math.cos(a)), S(cy + r * .78 * math.sin(a))), (S(cx + r * .92 * math.cos(a)), S(cy + r * .92 * math.sin(a)))], fill=INK, width=1)
        a1, a2 = rnd.uniform(0, 6.28), rnd.uniform(0, 6.28)
        d.line([(S(cx), S(cy)), (S(cx + r * .7 * math.cos(a1)), S(cy + r * .7 * math.sin(a1)))], fill=INK, width=S(0.8))
        d.line([(S(cx), S(cy)), (S(cx + r * .5 * math.cos(a2)), S(cy + r * .5 * math.sin(a2)))], fill=INK, width=S(1.1))
    # the bench, tools, parts
    d.rectangle([0, S(112), W, S(150)], fill=(80, 46, 26))
    d.line([(0, S(112)), (W, S(112))], fill=(130, 84, 50), width=S(1.2))
    for x in range(0, 240, 30):
        d.line([(S(x), S(113)), (S(x + 12), S(150))], fill=(70, 40, 22), width=1)
    gear(d, 44, 122, 10, 14, 0.4, BRASS, GOLD_D, None, 4, 2.2)
    gear(d, 62, 128, 6, 9, 0.1, (210, 200, 180), (120, 110, 90), None, 0, 1.6)
    d.ellipse([S(96), S(118), S(108), S(130)], outline=GOLD_D, width=S(1.5))
    d.line([(S(106), S(128)), (S(120), S(138))], fill=(40, 30, 24), width=S(2.5))
    d.line([(S(140), S(130)), (S(176), S(122))], fill=STEEL_L, width=S(1))
    # the lamp
    d.line([(S(200), S(0)), (S(200), S(84))], fill=(40, 30, 24), width=S(0.8))
    d.polygon([(S(190), S(96)), (S(210), S(96)), (S(204), S(84)), (S(196), S(84))], fill=GOLD_D)
    img = glow(img, 200, 100, 60, (255, 170, 80), 0.6)
    img = glow(img, 200, 98, 10, (255, 220, 150), 0.9)
    return img

def music_box_closeup(img, note=False):
    img = workshop_interior(img)
    img = Image.eval(img, lambda v: int(v * 0.55))
    d = ImageDraw.Draw(img)
    # the box on the bench, lid up: rose velvet and a gold sunburst inlay
    d.polygon([(S(54), S(66)), (S(186), S(66)), (S(176), S(22)), (S(64), S(22))], fill=(90, 20, 40))
    rays(d, 120, 66, 18, 50, (130, 34, 60), (100, 24, 46), 180, 360)
    d.arc([S(90), S(36), S(150), S(96)], 180, 360, fill=GOLD, width=S(0.8))
    d.line([(S(54), S(66)), (S(186), S(66))], fill=GOLD, width=S(1))
    d.rectangle([S(50), S(66), S(190), S(122)], fill=(70, 38, 22))
    d.rectangle([S(56), S(70), S(184), S(116)], fill=(12, 8, 12))
    for i in range(20):                                       # the cylinder
        th = -0.6 + i * 0.1; y0 = 94 - 18 * math.sin(th); y1 = 94 - 18 * math.sin(th + 0.1)
        lit = max(0, math.cos(th - 0.6))
        d.rectangle([S(62), S(y1), S(150), S(y0)], fill=tuple(int(a + (b - a) * lit) for a, b in zip((60, 38, 14), (226, 172, 92))))
    for k in range(10):
        d.ellipse([S(66 + k * 8.5), S(84 + (k * 7) % 14), S(68.4 + k * 8.5), S(86.4 + (k * 7) % 14)], fill=GOLD_DD)
    d.rectangle([S(62), S(104), S(150), S(110)], fill=(40, 60, 90))
    for k in range(10):
        d.line([(S(66 + k * 9), S(104)), (S(66 + k * 9), S(96))], fill=STEEL_L, width=S(1.4))
    d.rectangle([S(158), S(80), S(162), S(110)], fill=GOLD_D)
    d.line([(S(160), S(80)), (S(172), S(72))], fill=GOLD_D, width=S(1.5))
    img = glow(img, 120, 80, 50, (255, 140, 120), 0.35)
    if note:
        d = ImageDraw.Draw(img)
        d.polygon([(S(96), S(116)), (S(150), S(112)), (S(154), S(140)), (S(98), S(144))], fill=CREAM)
        for k in range(4):
            d.line([(S(102), S(121 + k * 5)), (S(146 - (k == 3) * 22), S(118 + k * 5))], fill=(90, 70, 60), width=S(0.6))
        img = glow(img, 124, 128, 20, (255, 230, 180), 0.3)
    return img

def ballroom(img, wide=True):
    vgrad(img, 0, 240, (24, 18, 22), (40, 30, 30))
    d = ImageDraw.Draw(img)
    for r in range(160, 50, -12):
        d.arc([S(120 - r * 1.3), S(110 - r), S(120 + r * 1.3), S(110 + r)], 180, 360, fill=(62, 50, 42), width=S(1.2))
    for k in range(-6, 7):
        a = math.radians(270 + k * 13)
        d.line([(S(120 + 62 * 1.3 * math.cos(a)), S(110 + 62 * math.sin(a))), (S(120 + 160 * 1.3 * math.cos(a)), S(110 + 160 * math.sin(a)))], fill=(62, 50, 42), width=S(1))
    d.polygon([(S(150), S(0)), (S(196), S(0)), (S(176), S(40)), (S(160), S(26))], fill=(14, 18, 30))     # the broken vault, night beyond
    for x in (8, 46, 194, 232):
        d.rectangle([S(x - 6), S(70), S(x + 6), S(222)], fill=(70, 62, 58))
        d.line([(S(x - 3), S(70)), (S(x - 1), S(222))], fill=(96, 86, 80), width=S(0.8))
        d.rectangle([S(x - 8), S(66), S(x + 8), S(72)], fill=GOLD_D)
    d.rectangle([S(0), S(200), S(240), S(240)], fill=(34, 26, 24))
    for x in range(-40, 280, 16):
        d.polygon([(S(x), S(200)), (S(x + 8), S(200)), (S(120 + (x + 8 - 120) * 2.2), S(240)), (S(120 + (x - 120) * 2.2), S(240))], fill=(56, 44, 38))
    d.polygon([(S(60), S(206)), (S(90), S(204)), (S(84), S(214))], fill=(110, 130, 150))   # glass on the floor
    d.polygon([(S(170), S(212)), (S(196), S(208)), (S(188), S(220))], fill=(110, 130, 150))
    for (cx, cy, s) in ((60, 46, 0.7), (180, 46, 0.7), (120, 34, 1.0)):
        d = ImageDraw.Draw(img)
        d.line([(S(cx), S(22)), (S(cx), S(cy - 10 * s))], fill=GOLD_D, width=S(0.6))
        for k, (rw, yy) in enumerate(((18, -6), (14, 0), (9, 6))):
            d.ellipse([S(cx - rw * s), S(cy + yy * s - 2), S(cx + rw * s), S(cy + yy * s + 2)], outline=GOLD, width=S(0.6))
            n = int(9 * s) + 3
            for j in range(n):
                x = cx + rw * s * math.cos(2 * math.pi * j / n)
                d.ellipse([S(x - 0.8), S(cy + yy * s - 0.8), S(x + 0.8), S(cy + yy * s + 0.8)], fill=GOLD_L)
        img = glow(img, cx, cy, 24 * s, (255, 170, 80), 0.5)
    img = rain(img, 50, (200, 220, 240), 30, seed=6, ang=0.1)
    return img

def portrait_robot(img, kind):
    """the clients, drawn as Weimar portraits: a coloured wall, a strong diagonal, a mechanical face"""
    d = ImageDraw.Draw(img)
    if kind == "courier":
        # derived from the Neue Sachlichkeit portrait, not a copy: a standing courier inspecting its own
        # chronometer through its lens; harlequin tunic, a messenger's cap, a rainy window behind
        vgrad(img, 0, 150, (196, 96, 112), (120, 46, 66))
        d = ImageDraw.Draw(img)
        d.rectangle([S(150), S(20), S(226), S(108)], fill=(20, 26, 44))
        win = robot_city(Image.new("RGB", img.size, INK), 9, signs=False)
        m = Image.new("L", img.size, 0); ImageDraw.Draw(m).rectangle([S(154), S(24), S(222), S(104)], fill=255)
        img.paste(win, (0, 0), m)
        img = rain(img, 30, (200, 220, 255), 70, seed=2)
        d = ImageDraw.Draw(img)
        d.line([(S(188), S(24)), (S(188), S(104))], fill=(90, 30, 46), width=S(1.6))
        # the body: tall harlequin tunic, a satchel strap, the messenger's badge
        body = [(S(40), S(150)), (S(54), S(84)), (S(72), S(72)), (S(108), S(72)), (S(124), S(86)), (S(132), S(150))]
        d.polygon(body, fill=(30, 12, 18))
        lay = Image.new("L", img.size, 0); ImageDraw.Draw(lay).polygon(body, fill=255)
        pat = Image.new("RGB", img.size, (30, 12, 18)); dp = ImageDraw.Draw(pat)
        for y in range(60, 160, 12):
            for x in range(30, 140, 12):
                dp.polygon([(S(x), S(y - 6)), (S(x + 6), S(y)), (S(x), S(y + 6)), (S(x - 6), S(y))], fill=(200, 50, 60) if ((x + y) // 12) % 2 else (40, 14, 22))
        img.paste(pat, (0, 0), lay)
        d = ImageDraw.Draw(img)
        d.line([(S(60), S(80)), (S(122), S(146))], fill=(120, 76, 44), width=S(3))
        d.ellipse([S(64), S(96), S(78), S(110)], fill=GOLD); d.ellipse([S(67), S(99), S(75), S(107)], fill=GOLD_D)
        # a raised arm holding the chronometer up to the lens
        d.polygon([(S(110), S(84)), (S(124), S(82)), (S(136), S(56)), (S(128), S(52))], fill=(170, 40, 50))
        d.ellipse([S(128), S(38), S(146), S(56)], fill=GOLD_D); d.ellipse([S(130), S(40), S(144), S(54)], fill=CREAM)
        d.line([(S(137), S(47)), (S(140), S(42))], fill=INK, width=S(0.8))
        d.line([(S(137), S(36)), (S(137), S(32))], fill=GOLD, width=S(1))
        for k in range(3):
            d.line([(S(128), S(54 - k * 2)), (S(122 + k), S(46 - k * 3))], fill=STEEL_L, width=S(1.2))
        # the head: a smooth Deco mask turned three-quarters, a messenger's peaked cap, the lens
        d.rectangle([S(82), S(62), S(96), S(74)], fill=STEEL)
        d.polygon([(S(72), S(34)), (S(104), S(30)), (S(110), S(48)), (S(102), S(66)), (S(86), S(70)), (S(74), S(60))], fill=(156, 176, 192))
        d.polygon([(S(68), S(36)), (S(110), S(28)), (S(108), S(18)), (S(76), S(20))], fill=(30, 30, 44))
        d.polygon([(S(96), S(28)), (S(124), S(26)), (S(112), S(32))], fill=(20, 20, 30))
        d.ellipse([S(94), S(38), S(108), S(52)], outline=GOLD, width=S(1.2), fill=(250, 220, 140))
        d.ellipse([S(98), S(42), S(104), S(48)], fill=RED)
        d.line([(S(78), S(46)), (S(88), S(45))], fill=INK, width=S(1.1))
        d.line([(S(86), S(60)), (S(96), S(58))], fill=(90, 100, 120), width=S(1.2))
        for k in range(4):
            d.line([(S(76 + k * 4), S(64)), (S(76 + k * 4), S(68))], fill=GOLD_D, width=S(0.5))
        img = glow(img, 101, 45, 10, (255, 80, 60), 0.6)
        img = glow(img, 137, 47, 14, (255, 220, 160), 0.3)
    elif kind == "lamp":
        # the lamplighter at noon: an amber smog street, the lamps burning anyway
        vgrad(img, 0, 150, (150, 100, 50), (200, 140, 70))
        img = skyline(img, 140, 21, 0, 50, 120, (130, 84, 44), [(160, 110, 60)], 0.0)
        img = fog(img, 40, 150, (210, 150, 80), 0.6, 3)
        d = ImageDraw.Draw(img)
        for x in (24, 196):
            d.line([(S(x), S(140)), (S(x), S(54))], fill=(60, 36, 20), width=S(2))
            d.polygon([(S(x - 5), S(54)), (S(x + 5), S(54)), (S(x + 3), S(44)), (S(x - 3), S(44))], fill=(60, 36, 20))
            img = glow(img, x, 50, 12, (255, 220, 140), 0.9)
            d = ImageDraw.Draw(img)
        d.rectangle([0, S(140), W, S(150)], fill=(90, 60, 34))
        # a tall, thin unit, a pole with a hook, an arm too long
        d.rectangle([S(104), S(66), S(130), S(140)], fill=(40, 30, 26))
        for k in range(6):
            d.line([(S(104), S(74 + k * 10)), (S(130), S(74 + k * 10))], fill=(70, 50, 36), width=1)
        d.ellipse([S(104), S(34), S(130), S(66)], fill=(54, 40, 32))
        d.rectangle([S(110), S(46), S(124), S(50)], fill=GOLD_L)
        d.line([(S(130), S(80)), (S(160), S(60)), (S(162), S(22))], fill=(40, 30, 26), width=S(2.2))
        d.arc([S(156), S(14), S(168), S(26)], 180, 360, fill=(40, 30, 26), width=S(1.6))
        img = glow(img, 117, 48, 10, (255, 230, 150), 0.6)
    elif kind == "salvage":
        # the scrapyard: heaps of brass, a crane, a boxy unit holding out the wooden box
        vgrad(img, 0, 150, (30, 40, 40), (70, 80, 70))
        img = fog(img, 60, 150, (110, 120, 100), 0.5, 5)
        d = ImageDraw.Draw(img)
        rnd = random.Random(4)
        for k in range(60):
            x, y = rnd.uniform(0, 240), rnd.uniform(100, 150)
            gear(d, x, y, rnd.uniform(3, 9), rnd.randint(8, 14), rnd.uniform(0, 6), (110, 90, 60), (70, 56, 40), None, 0, 1.4)
        d.polygon([(S(0), S(150)), (S(0), S(118)), (S(40), S(104)), (S(90), S(124)), (S(150), S(110)), (S(240), S(128)), (S(240), S(150))], fill=(60, 50, 36))
        d.line([(S(200), S(150)), (S(214), S(20)), (S(150), S(30))], fill=(30, 34, 34), width=S(2))
        d.line([(S(160), S(29)), (S(160), S(60))], fill=(30, 34, 34), width=S(0.6))
        d.rectangle([S(80), S(66), S(124), S(124)], fill=(150, 130, 100))
        d.rectangle([S(84), S(70), S(120), S(120)], fill=(120, 104, 80))
        d.rectangle([S(84), S(34), S(120), S(64)], fill=(150, 130, 100))
        d.rectangle([S(90), S(44), S(114), S(52)], fill=(20, 20, 20))
        d.ellipse([S(93), S(45), S(99), S(51)], fill=AMBER); d.ellipse([S(105), S(45), S(111), S(51)], fill=AMBER)
        d.line([(S(124), S(80)), (S(146), S(96))], fill=(90, 80, 64), width=S(4))
        d.rectangle([S(140), S(92), S(162), S(106)], fill=(110, 66, 40))
        d.line([(S(140), S(92)), (S(162), S(92))], fill=GOLD, width=S(0.8))
        img = glow(img, 102, 48, 14, (255, 160, 60), 0.4)
    return img

# ---------------------------------------------------------------- the backdrops
def bg_title():
    img = new()
    vgrad(img, 0, 240, (6, 8, 16), (28, 24, 36))
    d = ImageDraw.Draw(img)
    rays(d, 120, 104, 36, 270, (60, 44, 20), (14, 14, 22))
    img = glow(img, 120, 90, 70, (120, 80, 30), 0.7)
    img = skyline(img, 210, 11, 0, 60, 120, (18, 20, 32), [AMBER, CYAN_D, (110, 90, 60)], 0.15)
    d = ImageDraw.Draw(img)
    # the great engine: a tower of setbacks with pistons and drive belts, a clock for a heart
    for k, (w, y) in enumerate([(90, 168), (70, 136), (54, 108), (38, 84), (24, 64)]):
        d.rectangle([S(120 - w / 2), S(y), S(120 + w / 2), S(240)], fill=(16 + k * 3, 18 + k * 3, 28 + k * 4))
        d.line([(S(120 - w / 2), S(y)), (S(120 + w / 2), S(y))], fill=GOLD_D, width=S(0.8))
    for x in range(-42, 43, 7):
        d.line([(S(120 + x), S(168)), (S(120 + x), S(240))], fill=(30, 32, 46), width=1)
    d.line([(S(120), S(64)), (S(120), S(58))], fill=GOLD_D, width=S(1.2))
    for side in (-1, 1):                                       # pistons in their cylinders, flanking
        x = 120 + side * 56
        d.rectangle([S(x - 7), S(130), S(x + 7), S(210)], fill=(26, 28, 40))
        d.rectangle([S(x - 5), S(150), S(x + 5), S(156)], fill=GOLD_D)
        d.line([(S(x), S(150)), (S(x), S(120))], fill=STEEL_L, width=S(2))
    # big background wheels driving the tower, joined by belts
    gear(d, 44, 150, 34, 30, 0.1, (40, 34, 26), (26, 22, 18), None, 6, 4)
    gear(d, 196, 156, 28, 24, 0.4, (40, 34, 26), (26, 22, 18), None, 5, 4)
    d.line([(S(44), S(116)), (S(120), S(96))], fill=(30, 26, 22), width=S(1.5))
    d.line([(S(196), S(128)), (S(120), S(112))], fill=(30, 26, 22), width=S(1.5))
    # the clock face (its hands are live), gold rim
    gear(d, 120, 112, 17, 28, 0.2, GOLD, GOLD_D, CREAM, spokes=0)
    d.ellipse([S(106), S(98), S(134), S(126)], fill=CREAM)
    for k in range(12):
        a = 2 * math.pi * k / 12
        d.line([(S(120 + 10.5 * math.cos(a)), S(112 + 10.5 * math.sin(a))), (S(120 + 13 * math.cos(a)), S(112 + 13 * math.sin(a)))], fill=INK, width=S(0.8))
    img = glow(img, 120, 112, 26, (255, 200, 120), 0.45)
    # patrol airships, searchlights sweeping the engine
    img = beam(img, 40, 74, 62, 12, 170, (200, 230, 255), 30)
    img = beam(img, 200, 60, 116, 10, 190, (255, 210, 160), 24)
    d = ImageDraw.Draw(img)
    airship(d, 22, 66, 36, (10, 12, 20), RED)
    airship(d, 186, 52, 28, (10, 12, 20), CYAN)
    img = fog(img, 180, 240, (60, 50, 70), 0.5, 4)
    d = ImageDraw.Draw(img)
    # the title band: black lacquer, gold rules, Deco capitals
    d.rectangle([0, S(6), W, S(52)], fill=(6, 8, 14))
    for y, c in ((6, GOLD), (8, GOLD_D), (50, GOLD_D), (52, GOLD)):
        d.line([(0, S(y)), (W, S(y))], fill=c, width=S(0.7))
    deco_c(d, 120, 12, "MECHANICA", 18, GOLD, stroke=2.2, gap=3.2)
    deco_c(d, 120, 36, "OBSCURA", 10, GOLD_L, stroke=1.2, gap=5)
    img = bloom(img, 200, 3, 0.6)
    img = rain(img, 120, (180, 200, 230), 36)
    return img

def bg_story(art):
    img = new()
    if art in line_art.ARTS:
        img = line_art.ARTS[art](img)
    elif art in ("city", "lens"):
        img = robot_city(img, 5)
        d = ImageDraw.Draw(img)
        d.rectangle([S(8), S(32), S(40), S(70)], fill=(20, 40, 60))    # the regime's face
        d.ellipse([S(14), S(36), S(34), S(64)], fill=(40, 90, 120))
        d.ellipse([S(18), S(46), S(22), S(50)], fill=CYAN); d.ellipse([S(26), S(46), S(30), S(50)], fill=CYAN)
        img = glow(img, 24, 50, 18, (40, 160, 200), 0.45)
        img = beam(img, 150, 40, 112, 10, 110, (220, 240, 255), 36)
        d = ImageDraw.Draw(img)
        airship(d, 132, 32, 32, (8, 10, 18), RED)
        img = bloom(img, 190, 3, 0.7)
        img = reflect(img, 128, 142, 0.7, 2)
        img = rain(img, 160, (170, 190, 230), 46)
        if art == "lens":
            a = np.asarray(img).astype(np.float32) * 0.45
            img = Image.fromarray(a.astype(np.uint8))
            d = ImageDraw.Draw(img)
            d.ellipse([S(110), S(28), S(190), S(108)], fill=STEEL)
            d.ellipse([S(116), S(34), S(184), S(102)], fill=(50, 16, 20))
            d.ellipse([S(128), S(46), S(172), S(90)], fill=RED)
            d.ellipse([S(142), S(60), S(158), S(76)], fill=GOLD_L)
            for k in range(8):
                a2 = 2 * math.pi * k / 8
                d.line([(S(150 + 30 * math.cos(a2)), S(68 + 30 * math.sin(a2))), (S(150 + 39 * math.cos(a2)), S(68 + 39 * math.sin(a2)))], fill=STEEL_L, width=S(1))
            img = glow(img, 150, 68, 40, (255, 40, 40), 0.5)
    elif art in ("courier", "lamp", "salvage"):
        img = portrait_robot(img, art)
    elif art == "workshop":
        img = workshop_interior(img)
    elif art in ("box", "note"):
        img = music_box_closeup(img, art == "note")
    elif art == "conservatory":
        img = ballroom(img)
        d = ImageDraw.Draw(img)
        cx, cy = 120, 104
        d.ellipse([S(cx - 16), S(cy - 30), S(cx + 16), S(cy + 2)], outline=GOLD_D, width=S(0.8))
        for k in range(-3, 4):
            d.line([(S(cx + k * 4.5), S(cy - 14 - math.sqrt(max(0, 256 - (k * 4.5) ** 2)))), (S(cx + k * 4.5), S(cy + 14))], fill=GOLD_D, width=1)
        d.ellipse([S(cx - 5), S(cy - 2), S(cx + 5), S(cy + 8)], fill=BRASS)
    elif art == "bird":
        img = ballroom(img)
        d = ImageDraw.Draw(img)
        cx, cy = 120, 96
        d.ellipse([S(cx - 30), S(cy - 50), S(cx + 30), S(cy + 10)], fill=(18, 30, 36))
        d.rectangle([S(cx - 30), S(cy - 20), S(cx + 30), S(cy + 34)], fill=(18, 30, 36))
        d.ellipse([S(cx - 10), S(cy - 2), S(cx + 10), S(cy + 16)], fill=BRASS)
        d.ellipse([S(cx + 4), S(cy - 12), S(cx + 16), S(cy)], fill=BRASS)
        d.polygon([(S(cx + 15), S(cy - 8)), (S(cx + 24), S(cy - 10)), (S(cx + 15), S(cy - 4))], fill=COPPER)
        d.arc([S(cx - 30), S(cy - 50), S(cx + 30), S(cy + 10)], 180, 360, fill=CYAN, width=S(0.8))
        for k in range(-4, 5):
            x = cx + k * 7
            d.line([(S(x), S(cy - 20 - math.sqrt(max(0, 900 - (k * 7) ** 2)))), (S(x), S(cy + 34))], fill=CYAN_D, width=1)
        d.rectangle([S(cx - 32), S(cy + 34), S(cx + 32), S(cy + 38)], fill=GOLD_D)
    elif art == "safe":
        img = ballroom(img)
        img = Image.eval(img, lambda v: int(v * 0.6))
        d = ImageDraw.Draw(img)
        d.rectangle([S(70), S(40), S(170), S(140)], fill=(10, 10, 14))
        rays(d, 120, 90, 28, 70, (70, 50, 20), (14, 14, 18))
        d.rectangle([S(70), S(40), S(170), S(140)], outline=GOLD_D, width=S(1))
        d.ellipse([S(100), S(70), S(140), S(110)], fill=GOLD)
        d.ellipse([S(108), S(78), S(132), S(102)], fill=(26, 24, 26))
        d.ellipse([S(116), S(86), S(124), S(94)], fill=GOLD_D)
        img = glow(img, 120, 90, 50, (120, 80, 30), 0.4)
    elif art == "patrol": img = act2_art.art_patrol(img, workshop_interior)
    elif art == "margins": img = act2_art.art_margins(img, workshop_interior)
    elif art == "theatre": img = act2_art.art_theatre(img)
    elif art == "tower": img = act2_art.art_tower(img, robot_city)
    elif art == "door": img = act2_art.art_door(img)
    elif art == "underhall": img = act2_art.art_underhall(img)
    elif art == "dance":
        vgrad(img, 0, 150, (20, 14, 10), (50, 34, 20))
        d = ImageDraw.Draw(img)
        rays(d, 120, 150, 30, 200, (90, 64, 26), (24, 18, 12), 180, 360)
        for k in range(5):                                     # pages of a score, fanned out
            a = -0.5 + k * 0.25
            cx, cy = 120 + math.sin(a) * 70, 120 - math.cos(a) * 20
            pts = [(-22, -30), (22, -30), (22, 30), (-22, 30)]
            rot = [(S(cx + x * math.cos(a) - y * math.sin(a)), S(cy + x * math.sin(a) + y * math.cos(a))) for x, y in pts]
            d.polygon(rot, fill=(220, 206, 176))
            for l in range(6):
                y = -24 + l * 9
                p0 = (S(cx - 18 * math.cos(a) - y * math.sin(a)), S(cy - 18 * math.sin(a) + y * math.cos(a)))
                p1 = (S(cx + 18 * math.cos(a) - y * math.sin(a)), S(cy + 18 * math.sin(a) + y * math.cos(a)))
                d.line([p0, p1], fill=(120, 100, 80), width=1)
        img = glow(img, 120, 100, 70, (255, 200, 120), 0.4)
    shifted = new()                              # the header covers the top 22 px: move the art into view
    shifted.paste(img.crop((0, 0, W, S(152))), (0, S(16)))   # (art rows 0..152 -> screen 16..168: the story page's window)
    shifted = lacquer_floor(shifted, 168)
    return shifted

def bg_escapement():
    img = new((10, 10, 14))
    d = ImageDraw.Draw(img)
    d.rectangle([S(0), S(22), S(240), S(132)], fill=(118, 90, 48))
    for k in range(-20, 40):
        x = k * 9
        d.polygon([(S(x), S(22)), (S(x + 5), S(22)), (S(x + 5 - 40), S(132)), (S(x - 40), S(132))], fill=(136, 104, 58))
    for y in range(30, 130, 7):
        for x in range(150, 240, 7):
            d.ellipse([S(x - 3), S(y - 3), S(x + 3), S(y + 3)], outline=(152, 120, 70), width=1)
    d.ellipse([S(16), S(28), S(116), S(128)], fill=(70, 52, 28))      # the balance's recess
    d.ellipse([S(20), S(32), S(112), S(124)], fill=(90, 68, 36))
    d.polygon([(S(110), S(70)), (S(150), S(64)), (S(150), S(90)), (S(110), S(86))], fill=(100, 78, 42))  # the bridge
    for (x, y) in ((10, 30), (230, 30), (10, 126), (144, 126), (150, 60)):
        d.ellipse([S(x - 4), S(y - 4), S(x + 4), S(y + 4)], fill=STEEL); d.line([(S(x - 3), S(y)), (S(x + 3), S(y))], fill=INK, width=S(0.8))
    deco(d, 158, 116, "CAL K7", 6, (90, 66, 32), stroke=0.9)
    img = glow(img, 66, 78, 60, (60, 40, 10), 0.35)
    d = ImageDraw.Draw(img)
    panel(d, 4, 134, 232, 86, CYAN, fans=False)
    d.rectangle([S(10), S(140), S(176), S(214)], fill=(4, 18, 22))
    for x in range(10, 177, 12):
        d.line([(S(x), S(140)), (S(x), S(214))], fill=(10, 50, 58), width=1)
    for y in range(140, 215, 9):
        d.line([(S(10), S(y)), (S(176), S(y))], fill=(10, 50, 58), width=1)
    d.line([(S(10), S(177)), (S(176), S(177))], fill=(20, 90, 100), width=S(0.6))
    img = term(img, 14, 141, "TIMEGRAPH", (40, 140, 150))
    return img

def bg_gears():
    img = workshop_interior(new())
    img = Image.eval(img, lambda v: int(v * 0.42))
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(100), W, S(186)], fill=(30, 18, 12))           # the bench, dark, for the train
    for x in range(0, 240, 24):
        d.line([(S(x), S(100)), (S(x + 10), S(186))], fill=(38, 24, 16), width=1)
    d.line([(0, S(100)), (W, S(100))], fill=(80, 50, 30), width=S(0.8))
    panel(d, 4, 186, 232, 34, CYAN, fans=False, double=False)
    return img

def bg_musicbox():
    img = new((14, 8, 12))
    d = ImageDraw.Draw(img)
    d.polygon([(S(10), S(24)), (S(230), S(24)), (S(218), S(88)), (S(22), S(88))], fill=(90, 20, 40))
    rays(d, 120, 88, 22, 120, (130, 34, 60), (100, 24, 46), 180, 360)
    for r in (30, 46):
        d.arc([S(120 - r), S(88 - r), S(120 + r), S(88 + r)], 180, 360, fill=GOLD, width=S(0.8))
    deco_c(d, 120, 32, "OBSCURA", 8, GOLD, stroke=1, gap=3)
    d.line([(S(22), S(88)), (S(218), S(88))], fill=GOLD, width=S(1))
    img = glow(img, 120, 70, 50, (255, 120, 140), 0.3)
    d = ImageDraw.Draw(img)
    d.rectangle([S(6), S(90), S(234), S(222)], fill=(54, 30, 20))
    d.rectangle([S(12), S(96), S(228), S(216)], fill=(10, 8, 12))
    for (x, y) in ((6, 90), (222, 90), (6, 210), (222, 210)):
        d.rectangle([S(x), S(y), S(x + 12), S(y + 12)], fill=GOLD_D)
    panel(d, 188, 98, 38, 114, ROSE, fans=False, double=False)
    return img

def bg_bird():
    img = ballroom(new())
    return img

def bg_safe():
    img = new()
    d = ImageDraw.Draw(img)
    d.rectangle([S(6), S(24), S(234), S(222)], fill=(14, 14, 18))
    rays(d, 120, 108, 40, 200, (70, 50, 20), (14, 14, 18))
    img = glow(img, 120, 108, 90, (90, 60, 20), 0.3)
    d = ImageDraw.Draw(img)
    for (x0, x1) in ((6, 28), (212, 234)):
        d.rectangle([S(x0), S(24), S(x1), S(222)], fill=(20, 22, 28))
        for y in range(34, 222, 22):
            d.ellipse([S((x0 + x1) / 2 - 3), S(y - 3), S((x0 + x1) / 2 + 3), S(y + 3)], fill=GOLD_D)
    d.ellipse([S(120 - 72), S(108 - 72), S(120 + 72), S(108 + 72)], fill=(8, 8, 10))
    panel(d, 148, 184, 82, 36, ROSE, fans=False, double=False)
    return img

def bg_end():
    img = new()
    vgrad(img, 0, 240, (6, 8, 16), (26, 20, 24))
    d = ImageDraw.Draw(img)
    rays(d, 120, 240, 40, 300, (44, 32, 16), (10, 10, 16), 180, 360)
    return img

STORY_ARTS = list(line_art.ARTS)   # Obscura Line: the line's story (tools/content.py ARTS: the same names, the same order)
STRIP = (22, 146)            # story art: only the window between the header and the text panel is kept (22..168)
import cab_art                                  # Obscura Line: the puzzles' backgrounds, in the cab's style
BACKDROPS = [("ESC", cab_art.bg_escapement), ("GEARS", cab_art.bg_gears), ("MBOX", cab_art.bg_musicbox),
             ("BIRD", cab_art.bg_bird), ("SAFE", cab_art.bg_safe)] + \
            [("ART_" + a.upper(), (lambda a=a: bg_story(a))) for a in STORY_ARTS]

def to565(c):
    r, g, b = c
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

def main():
    BG.mkdir(parents=True, exist_ok=True)
    out = ["/* SPDX-License-Identifier: GPL-3.0-only */",
           "/* generated by tools/style/bake.py: do not edit. Backdrops: a palette (RGB565), LZ4 blocks of 8-bit indices, rows y0..y0+h */",
           "#pragma once", "typedef struct { const uint16_t *pal; const uint8_t *lz; uint32_t lz_n; uint8_t ncol, y0, h, bits; } mo_bg_t;"]
    enum, table, total = [], [], 0
    for name, fn in BACKDROPS:
        img = fn().resize((240, 240), Image.LANCZOS)
        strip = name.startswith("ART_")
        y0, h = STRIP if strip else (0, 240)
        # story strips: 24 colours (their accents matter); the title: 32; game backdrops, which the live
        # mechanisms sit on: 16 colours packed two pixels a byte (bits 4)
        ncol, bits = (24, 8) if strip else (32, 8) if name == "TITLE" else (16, 4)
        img = img.crop((0, y0, 240, y0 + h))
        q = img.quantize(colors=ncol, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        pal = (q.getpalette() + [0] * 768)[:ncol * 3]
        idx = bytes(q.get_flattened_data()) if hasattr(q, "get_flattened_data") else bytes(q.getdata())
        if bits == 4:
            idx = bytes((idx[i] << 4) | idx[i + 1] for i in range(0, len(idx), 2))
        lz = lz4blk.compress(idx)
        assert lz4blk.decompress(lz) == idx
        q.convert("RGB").resize((480, 2 * h), Image.NEAREST).save(BG / f"{name.lower()}.png")
        total += len(lz) + 2 * ncol
        print(f"{name:16s} {len(lz) / 1024:6.1f} KB")
        p565 = [to565(tuple(pal[i * 3:i * 3 + 3])) for i in range(ncol)]
        out.append(f"static const uint16_t BG_{name}_PAL[{ncol}] = {{{', '.join(f'0x{v:04X}' for v in p565)}}};")
        out.append(f"static const uint8_t BG_{name}_LZ[{len(lz)}] = {{")
        for i in range(0, len(lz), 24):
            out.append("    " + ", ".join(str(b) for b in lz[i:i + 24]) + ",")
        out.append("};")
        enum.append(f"BG_{name}")
        table.append(f"    {{BG_{name}_PAL, BG_{name}_LZ, {len(lz)}u, {ncol}u, {y0}u, {h}u, {bits}u}},")
    out.append("enum { " + ", ".join(enum) + ", BG_COUNT };")
    out.append(f"#define BG_ART_FIRST BG_ART_{STORY_ARTS[0].upper()}   /* story art from here on: drawn in the new style, not graded */")
    out.append("static const mo_bg_t MO_BG[BG_COUNT] = {")
    out += table
    out.append("};")
    with open(ROOT / "firmware/gen/mo_art.h", "w", encoding="utf-8", newline="\n") as f:   # (LF on Windows too)
        f.write("\n".join(out) + "\n")
    print(f"total {total / 1024:.0f} KB -> firmware/gen/mo_art.h")

if __name__ == "__main__":
    main()
