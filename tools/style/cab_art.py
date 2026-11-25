# SPDX-License-Identifier: GPL-3.0-only
"""Obscura Line: the puzzles' baked backgrounds, in the cab's style (deep violet consoles, rounded screens with
magenta rims, neon edges, brass lit from below). Each keeps the geometry its mechanism draws onto (see the g_*.c
named in each docstring); used by bake.py in place of Mechanica's."""
import math, random
from mo_style import *
import line_art as la

PANEL = (18, 10, 44); PANEL2 = (28, 16, 64); WALL = (22, 12, 42); RIM = (255, 60, 200); RIM_D = (112, 20, 96)
CY = (60, 232, 255); CY_D = (20, 92, 140); GLASS = (6, 20, 30); BRASS_L = (255, 210, 130)


def rrect(d, x, y, w, h, r, fill=None, outline=None, width=1.0):
    d.rounded_rectangle([S(x), S(y), S(x + w), S(y + h)], radius=S(r), fill=fill,
                        outline=outline, width=S(width) if outline else 0)


def cab_panel(d, x, y, w, h, rim=RIM, fill=PANEL, r=7):
    """the frame's rounded panel: a magenta rim, a faint cyan line inside"""
    rrect(d, x, y, w, h, r, fill=fill, outline=rim, width=2)
    rrect(d, x + 3, y + 3, w - 6, h - 6, r - 2, outline=(40, 70, 110), width=0.6)


def wall(img, y0=0, y1=240, seed=1):
    """the cab's inside wall: violet, a faint grid of panel seams, rivets"""
    vgrad(img, y0, y1, (26, 14, 50), (16, 8, 32))
    d = ImageDraw.Draw(img)
    for x in range(0, 240, 30):
        d.line([(S(x), S(y0)), (S(x), S(y1))], fill=(34, 20, 62), width=1)
    for y in range(y0, y1, 30):
        d.line([(0, S(y)), (W, S(y))], fill=(34, 20, 62), width=1)
        for x in range(4, 240, 30):
            d.ellipse([S(x - 1), S(y + 3), S(x + 1), S(y + 5)], fill=(60, 40, 90))
    return img


def screen(d, x, y, w, h, glass=GLASS, grid=(14, 50, 64), step=(12, 9)):
    rrect(d, x - 2, y - 2, w + 4, h + 4, 4, fill=(10, 6, 20))
    rrect(d, x, y, w, h, 3, fill=glass)
    if grid:
        for gx in range(int(x), int(x + w), step[0]):
            d.line([(S(gx), S(y + 1)), (S(gx), S(y + h - 1))], fill=grid, width=1)
        for gy in range(int(y), int(y + h), step[1]):
            d.line([(S(x + 1), S(gy)), (S(x + w - 1), S(gy))], fill=grid, width=1)


def bg_escapement():
    """g_escapement.c: the balance at (66, 78) r 34 (its scale to r 49), the fork at 128, the escape wheel at 172,
    the master lamp at (216, 42); the timegraph's glass 10..176 x 140..214 (centre line 177), the knobs at x 206"""
    img = wall(new())
    d = ImageDraw.Draw(img)
    cab_panel(d, 4, 25, 232, 106)
    for r, c in ((52, (34, 18, 70)), (46, (24, 12, 52)), (40, (16, 8, 36))):   # the balance's well
        d.ellipse([S(66 - r), S(78 - r), S(66 + r), S(78 + r)], fill=c)
    d.ellipse([S(66 - 52), S(78 - 52), S(66 + 52), S(78 + 52)], outline=RIM_D, width=S(1))
    d.polygon([(S(110), S(70)), (S(150), S(64)), (S(150), S(90)), (S(110), S(86))], fill=PANEL2)   # the bridge
    d.line([(S(110), S(70)), (S(150), S(64))], fill=(70, 44, 100), width=S(0.8))
    d.ellipse([S(150), S(56), S(194), S(100)], fill=(24, 12, 52))
    for (x, y) in ((12, 32), (228, 32), (12, 124), (228, 124), (150, 60)):
        d.ellipse([S(x - 3), S(y - 3), S(x + 3), S(y + 3)], fill=(90, 70, 130))
        d.line([(S(x - 2), S(y)), (S(x + 2), S(y))], fill=(20, 10, 30), width=S(0.6))
    d.ellipse([S(204), S(30), S(228), S(54)], fill=(10, 6, 20), outline=RIM_D, width=S(0.8))   # the master lamp's bezel
    img = glow(img, 66, 78, 54, (200, 40, 160), 0.22)
    d = ImageDraw.Draw(img)
    cab_panel(d, 4, 134, 232, 86)
    screen(d, 10, 140, 166, 74)
    d.line([(S(10), S(177)), (S(176), S(177))], fill=(30, 120, 140), width=S(0.6))
    img = term(img, 14, 141, "TIMEGRAPH", (60, 170, 190))
    d = ImageDraw.Draw(img)
    rrect(d, 184, 140, 46, 74, 4, fill=PANEL2)
    return img


def bg_gears():
    """g_gearboard.c (and the robots, dimmed): the board of pegs drawn live over y 22..186; the stock at 186..220"""
    img = wall(new())
    d = ImageDraw.Draw(img)
    for k in range(10):                                                    # the signal box's levers, a row high up
        x = 14 + k * 23
        d.line([(S(x), S(48)), (S(x + (4 if k % 3 == 1 else -1)), S(26))], fill=(150, 140, 170), width=S(1.6))
        d.rectangle([S(x - 3), S(22), S(x + 3), S(28)], fill=[(200, 40, 90), (40, 80, 200), (230, 190, 60)][k % 3])
    d.rectangle([0, S(46), W, S(52)], fill=(50, 30, 70))
    cab_panel(d, 4, 54, 232, 130, rim=RIM_D, fill=(14, 8, 34))           # the board
    for y in range(64, 180, 12):
        d.line([(S(10), S(y)), (S(230), S(y))], fill=(22, 14, 46), width=1)
    img = glow(img, 120, 120, 90, (140, 30, 160), 0.18)
    d = ImageDraw.Draw(img)
    cab_panel(d, 4, 186, 232, 34)
    return img


def bg_musicbox():
    """g_musicbox.c: the lid 10..230 x 24..88 (painted), the box 6..234 x 90..222, its well 12..228 x 96..216, the
    rows' panel at (188, 98, 38, 114)"""
    img = new((14, 8, 30))
    lid = new((14, 8, 30))
    la.sky(lid, 24, 88, (20, 8, 56), (150, 36, 140), (255, 130, 140))
    lid = la.sun(lid, 120, 80, 22, 88)
    lid = la.mountain(lid, 64, 88, 22, 50)
    d = ImageDraw.Draw(lid)
    for k in range(-10, 11):
        d.line([(S(120 + k * 5), S(82)), (S(120 + k * 20), S(88))], fill=(200, 60, 170), width=1)
    deco_c(d, 120, 30, "OBSCURA", 8, (255, 230, 250), stroke=1, gap=3)
    m = Image.new("L", img.size, 0)
    ImageDraw.Draw(m).polygon([(S(10), S(24)), (S(230), S(24)), (S(218), S(88)), (S(22), S(88))], fill=255)
    img.paste(lid, (0, 0), m)
    d = ImageDraw.Draw(img)
    d.line([(S(10), S(24)), (S(230), S(24)), (S(218), S(88)), (S(22), S(88)), (S(10), S(24))], fill=BRASS_L, width=S(0.8))
    rrect(d, 6, 90, 228, 132, 8, fill=(46, 22, 60), outline=RIM, width=2)
    rrect(d, 12, 96, 216, 120, 5, fill=(10, 6, 20))
    for (x, y) in ((8, 92), (220, 92), (8, 208), (220, 208)):
        d.rounded_rectangle([S(x), S(y), S(x + 12), S(y + 12)], radius=S(3), fill=(150, 100, 60))
    cab_panel(d, 188, 98, 38, 114)
    return img


def bg_bird():
    """g_bird.c: the stationmaster's office at night; the cage is drawn live in the middle (about 70..170 x 30..190)"""
    img = wall(new())
    d = ImageDraw.Draw(img)
    win = new()                                                            # a great arched window onto the rain
    la.sky(win, 0, 150, (12, 6, 34), (90, 24, 110), (230, 80, 140))
    win = la.sun(win, 170, 130, 26, 150)
    win = la.city(win, 150, 23, 14, 60)
    win = rain(win, 120, (170, 180, 240), 70, seed=4)
    m = Image.new("L", img.size, 0)
    ImageDraw.Draw(m).rounded_rectangle([S(28), S(26), S(212), S(150)], radius=S(70), fill=255)
    img.paste(win, (0, 0), m)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([S(28), S(26), S(212), S(150)], radius=S(70), outline=(80, 50, 110), width=S(3))
    d.line([(S(120), S(26)), (S(120), S(150))], fill=(80, 50, 110), width=S(2))
    d.line([(S(28), S(100)), (S(212), S(100))], fill=(80, 50, 110), width=S(2))
    d.rectangle([0, S(150), W, S(240)], fill=(30, 16, 48))                 # the floor, tiled, glossy
    for x in range(-40, 280, 16):
        d.line([(S(x), S(150)), (S(120 + (x - 120) * 2.4), S(240))], fill=(44, 24, 66), width=1)
    for y in (162, 178, 200, 228):
        d.line([(0, S(y)), (W, S(y))], fill=(44, 24, 66), width=1)
    img = reflect(img, 150, 200, 0.4, 2)
    for x in (14, 226):
        img = la.lantern(img, x, 60, (255, 210, 150), 4)
    return img


def bg_safe():
    """g_safe.c: the dial in the well at (120, 108) r 72; the readout panel at (148, 184, 82, 36)"""
    img = wall(new())
    d = ImageDraw.Draw(img)
    cab_panel(d, 6, 24, 228, 198, rim=RIM_D, fill=(16, 8, 36))
    for k in range(36):                                                    # a radial neon grid behind the dial
        a = 2 * math.pi * k / 36
        d.line([(S(120 + 76 * math.cos(a)), S(108 + 76 * math.sin(a))), (S(120 + 160 * math.cos(a)), S(108 + 160 * math.sin(a)))],
               fill=(46, 20, 80), width=1)
    for (x0, x1) in ((8, 28), (212, 232)):
        for y in range(36, 214, 20):
            d.ellipse([S((x0 + x1) / 2 - 2.5), S(y - 2.5), S((x0 + x1) / 2 + 2.5), S(y + 2.5)], fill=(90, 70, 130))
    d.ellipse([S(120 - 78), S(108 - 78), S(120 + 78), S(108 + 78)], outline=RIM, width=S(1.4))
    d.ellipse([S(120 - 72), S(108 - 72), S(120 + 72), S(108 + 72)], fill=(8, 4, 16))
    img = glow(img, 120, 108, 80, (200, 40, 160), 0.18)
    d = ImageDraw.Draw(img)
    cab_panel(d, 148, 184, 82, 36)
    return img
