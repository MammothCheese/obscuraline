# SPDX-License-Identifier: GPL-3.0-only
"""Mechanica Obscura: Act Two's story vignettes (used by bake.py). Drawn in the 0..126 band like Act One's."""
import math, random
from mo_style import *


def art_patrol(img, workshop_interior):
    img = workshop_interior(img)
    img = Image.eval(img, lambda v: int(v * 0.5))
    d = ImageDraw.Draw(img)
    # the patrol unit in the doorway: tall, armoured, one great lens; rain behind it
    d.rectangle([S(150), S(10), S(232), S(126)], fill=(14, 20, 34))
    img = rain(img, 40, (170, 190, 230), 70, seed=9)
    d = ImageDraw.Draw(img)
    d.polygon([(S(160), S(126)), (S(166), S(52)), (S(184), S(40)), (S(206), S(40)), (S(222), S(52)), (S(228), S(126))], fill=(26, 32, 46))
    for k in range(5):
        d.line([(S(168), S(62 + k * 12)), (S(220), S(62 + k * 12))], fill=(44, 52, 70), width=S(0.8))
    d.ellipse([S(178), S(14), S(212), S(48)], fill=(30, 36, 50))
    d.ellipse([S(184), S(20), S(206), S(42)], fill=STEEL_L)
    d.ellipse([S(187), S(23), S(203), S(39)], fill=(60, 16, 20))
    d.ellipse([S(191), S(27), S(199), S(35)], fill=RED)
    img = glow(img, 195, 31, 22, (255, 40, 40), 0.6)
    img = beam(img, 195, 31, 200, 18, 120, (255, 60, 60), 34)
    d = ImageDraw.Draw(img)
    d.polygon([(S(40), S(116)), (S(110), S(116)), (S(104), S(108)), (S(46), S(108))], fill=(170, 160, 130))   # the cloth over the music
    d.rectangle([S(40), S(116), S(110), S(122)], fill=(200, 190, 160))
    return img


def art_margins(img, workshop_interior):
    img = workshop_interior(img)
    img = Image.eval(img, lambda v: int(v * 0.45))
    d = ImageDraw.Draw(img)
    for k, (x, y, a) in enumerate(((40, 70, -0.12), (110, 64, 0.06))):
        def tr(px, py):
            return (S(x + 30 + px * math.cos(a) - py * math.sin(a)), S(y + py * math.cos(a) + px * math.sin(a)))
        d.polygon([tr(-34, -40), tr(34, -40), tr(34, 40), tr(-34, 40)], fill=(222, 208, 176))
        for l in range(9):                                   # rows of letters in the margin
            for m in range(6):
                if (l * 7 + m * 3 + k) % 4:
                    p0 = tr(-30 + m * 4, -34 + l * 8)
                    d.rectangle([p0[0], p0[1], p0[0] + SS + 1, p0[1] + SS + 1], fill=(90, 70, 60))
        hx, hy = tr(14, -18)                                 # a dancer, in ink
        d.ellipse([hx - S(5), hy - S(5), hx + S(5), hy + S(5)], fill=(120, 60, 50))
        d.line([tr(14, -12), tr(12, 14)], fill=(120, 60, 50), width=S(1.5))
        d.line([tr(13, -6), tr(26, -14)], fill=(120, 60, 50), width=S(1.2))
        d.line([tr(12, 14), tr(4, 30)], fill=(120, 60, 50), width=S(1.2))
        d.line([tr(12, 14), tr(22, 28)], fill=(120, 60, 50), width=S(1.2))
    for r, c in ((26, (200, 186, 150)), (19, (176, 160, 124)), (12, (150, 134, 100))):   # three paper wheels
        d.ellipse([S(202 - r), S(96 - r), S(202 + r), S(96 + r)], fill=c, outline=(90, 70, 60), width=1)
    img = glow(img, 120, 70, 70, (255, 180, 100), 0.3)
    return img


def art_theatre(img):
    vgrad(img, 0, 140, (20, 10, 14), (40, 18, 22))
    d = ImageDraw.Draw(img)
    d.arc([S(10), S(-70), S(230), S(150)], 180, 360, fill=GOLD_D, width=S(5))     # the proscenium
    for side in (-1, 1):                                   # curtains, rose velvet, folds
        for k in range(7):
            x = 120 + side * (80 + k * 6)
            col = (150 - k * 10, 30, 50) if k % 2 else (110 - k * 8, 22, 40)
            d.rectangle([S(min(x, x + side * 6)), S(6), S(max(x, x + side * 6)), S(126)], fill=col)
    d.rectangle([S(0), S(108), S(240), S(126)], fill=(70, 44, 26))                # the stage
    for x in range(0, 240, 12):
        d.line([(S(x), S(108)), (S(x), S(126))], fill=(56, 34, 20), width=1)
    d.rectangle([S(96), S(56), S(144), S(110)], fill=(70, 40, 24))                # the cabinet
    for k in range(5):
        d.rectangle([S(102), S(62 + k * 9), S(138), S(66 + k * 9)], fill=GOLD_D)
    img = beam(img, 120, 0, 90, 30, 130, (255, 220, 160), 46)
    img = glow(img, 120, 80, 40, (255, 190, 120), 0.3)
    return img


def art_tower(img, robot_city):
    img = robot_city(img, 13, signs=False)
    d = ImageDraw.Draw(img)
    d.rectangle([S(92), S(4), S(148), S(132)], fill=(16, 18, 28))
    cx, cy = 120, 52
    d.ellipse([S(cx - 26), S(cy - 26), S(cx + 26), S(cy + 26)], fill=GOLD_D)
    d.ellipse([S(cx - 24), S(cy - 24), S(cx + 24), S(cy + 24)], fill=(30, 50, 90))
    d.ellipse([S(cx - 16), S(cy - 16), S(cx + 16), S(cy + 16)], outline=GOLD, width=S(0.8))
    for k in range(12):
        a = 2 * math.pi * k / 12
        d.line([(S(cx + 20 * math.cos(a)), S(cy + 20 * math.sin(a))), (S(cx + 24 * math.cos(a)), S(cy + 24 * math.sin(a)))], fill=GOLD_L, width=S(0.8))
    for k in range(7):
        a = 2 * math.pi * k / 7
        x, y = cx + 11 * math.cos(a), cy + 11 * math.sin(a)
        d.ellipse([S(x - 1.2), S(y - 1.2), S(x + 1.2), S(y + 1.2)], fill=CREAM)
    d.ellipse([S(cx - 3), S(cy - 3), S(cx + 3), S(cy + 3)], fill=GOLD)
    img = glow(img, cx, cy, 34, (120, 140, 220), 0.3)
    return img


def art_door(img):
    vgrad(img, 0, 140, (14, 12, 12), (30, 24, 20))
    d = ImageDraw.Draw(img)
    for y in range(0, 140, 8):                             # brick
        for x in range((y // 8 % 2) * 10 - 10, 240, 20):
            d.rectangle([S(x), S(y), S(x + 18), S(y + 6)], fill=(44, 30, 26))
    cx, cy = 120, 66
    img = glow(img, cx, cy, 70, (40, 160, 200), 0.35)
    d = ImageDraw.Draw(img)
    for k, r in enumerate((52, 40, 28, 16)):
        d.ellipse([S(cx - r), S(cy - r), S(cx + r), S(cy + r)], outline=GOLD if k % 2 == 0 else GOLD_D, width=S(5))
        a = 0.8 + k * 1.7
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        d.ellipse([S(x - 3), S(y - 3), S(x + 3), S(y + 3)], fill=(10, 30, 40))
    d.ellipse([S(cx - 7), S(cy - 7), S(cx + 7), S(cy + 7)], fill=(30, 26, 22))
    return img


def art_underhall(img):
    vgrad(img, 0, 140, (16, 10, 8), (44, 28, 18))
    d = ImageDraw.Draw(img)
    for k in range(5):                                     # brick vaults
        x = 24 + k * 48
        d.arc([S(x - 30), S(-10), S(x + 30), S(60)], 180, 360, fill=(70, 44, 30), width=S(2))
        d.rectangle([S(x - 30), S(24), S(x - 26), S(126)], fill=(60, 38, 26))
    d.rectangle([S(0), S(110), S(240), S(126)], fill=(50, 34, 24))
    rnd = random.Random(12)
    for k in range(9):                                     # people, waiting
        x = 16 + k * 26 + rnd.randint(-4, 4); h = rnd.randint(26, 34)
        if 100 < x < 140: continue
        col = (40 + rnd.randint(0, 30), 24, 30 + rnd.randint(0, 30))
        d.ellipse([S(x - 4), S(110 - h - 8), S(x + 4), S(110 - h)], fill=col)
        d.polygon([(S(x - 6), S(110)), (S(x - 4), S(110 - h)), (S(x + 4), S(110 - h)), (S(x + 6), S(110))], fill=col)
    for x in (40, 100, 160, 210):                          # candles
        d.line([(S(x), S(96)), (S(x), S(104))], fill=CREAM, width=S(1))
        img = glow(img, x, 94, 10, (255, 170, 80), 0.7)
        d = ImageDraw.Draw(img)
    d.rectangle([S(110), S(80), S(130), S(110)], fill=(70, 50, 30))   # the automaton conductor, arms raised, silent
    d.ellipse([S(114), S(50), S(126), S(62)], fill=GOLD_D)
    d.rectangle([S(115), S(62), S(125), S(80)], fill=GOLD_D)
    d.line([(S(115), S(66)), (S(104), S(52))], fill=GOLD_D, width=S(2))
    d.line([(S(125), S(66)), (S(136), S(52))], fill=GOLD_D, width=S(2))
    return img
