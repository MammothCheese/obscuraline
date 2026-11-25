# SPDX-License-Identifier: GPL-3.0-only
"""Obscura Line: the story's vignettes, in the "neon clockwork" style: synthwave night (indigo, magenta, a striped sun,
the snow-capped mountain), cyberpunk wet neon, and the line's old brass machinery. Used by bake.py.

Each art is drawn at 3x in the 240 x 168 band (rows 22..168 show on a story page, under the
header and above the text). The story they serve: a loop line closed for decades, its stations each holding a
mechanism the line needs to run again, a train that runs at night with no one aboard, and something under
Akari Heights that keeps the line's time.
"""
import math, random
from mo_style import *

NIGHT = (10, 6, 26); INDIGO = (22, 10, 56); VIOLET = (70, 26, 112); PLUM = (40, 14, 70)
MAG = (255, 60, 200); MAG_D = (120, 20, 100); PINK = (255, 140, 196); SAKURA = (255, 156, 210); SAK_D = (214, 92, 168)
NEON_C = (60, 232, 255); NEON_CD = (20, 92, 140); WARM = (255, 196, 120); LAMP = (255, 226, 160)
RAIL = (190, 170, 230); SNOW = (252, 214, 244)


# ---------------------------------------------------------------- pieces
def sky(img, y0, y1, top=INDIGO, mid=(140, 34, 130), hor=(255, 116, 140)):
    yh = (y0 + y1) / 2
    vgrad(img, y0, yh, top, mid)
    vgrad(img, yh, y1, mid, hor)
    return img


def stars(img, n, y1, seed=1):
    rnd = random.Random(seed)
    d = ImageDraw.Draw(img)
    for _ in range(n):
        x, y = rnd.uniform(0, 240), rnd.uniform(0, y1)
        c = rnd.choice([(255, 240, 255), (180, 236, 255), (255, 200, 240)])
        d.rectangle([S(x), S(y), S(x) + SS - 1, S(y) + SS - 1], fill=c)
    return img


def sun(img, cx, cy, r, horizon):
    """the synthwave sun: gold to magenta, cut by bars that thicken toward the horizon"""
    d = ImageDraw.Draw(img)
    for y in range(S(cy - r), min(S(horizon), S(cy + r))):
        dy = (y / SS) - cy
        if abs(dy) > r: continue
        hw = math.sqrt(max(0, r * r - dy * dy))
        t = (dy + r) / (2 * r)
        if dy > -r * 0.35:                                  # the bars
            k = (dy + r * 0.35) / (r * 1.35)
            if ((y / SS) % 5) < 1.0 + k * 3.2: continue
        c = tuple(int(a + (b - a) * t) for a, b in zip((255, 230, 120), (255, 50, 170)))
        d.line([(S(cx - hw), y), (S(cx + hw), y)], fill=c)
    return glow(img, cx, cy, r * 1.6, (255, 80, 160), 0.35)


def mountain(img, cx, base, h, w, body=(60, 22, 96), snow=SNOW, seed=2):
    d = ImageDraw.Draw(img)
    rnd = random.Random(seed)
    pts = [(S(cx - w), S(base))]
    for k in range(-10, 11):
        x = cx + k * w / 10
        y = base - h * (1 - abs(k) / 10) + (rnd.uniform(-1, 1) if abs(k) > 1 else 0)
        pts.append((S(x), S(y)))
    pts.append((S(cx + w), S(base)))
    d.polygon(pts, fill=body)
    if snow is None:
        return img
    cap = [(S(cx - w * 0.26), S(base - h * 0.74))]
    for k in range(7):                                       # a ragged snow line
        cap.append((S(cx - w * 0.26 + k * w * 0.087), S(base - h * 0.74 + (3 if k % 2 else -1))))
    cap += [(S(cx + w * 0.26), S(base - h * 0.74)), (S(cx), S(base - h))]
    d.polygon(cap, fill=snow)
    return img


def city(img, y_base, seed, hmin=10, hmax=40, body=(26, 10, 46), neon=True):
    img = skyline(img, y_base, seed, 0, hmin, hmax, body, [WARM, NEON_C, PINK, (255, 220, 160)], 0.22, step=True, zig=True)
    if neon:
        d = ImageDraw.Draw(img)
        rnd = random.Random(seed + 7)
        for _ in range(5):                                   # rooftop neon edges
            x = rnd.uniform(0, 220); y = y_base - rnd.uniform(hmin, hmax)
            d.line([(S(x), S(y)), (S(x + rnd.uniform(8, 18)), S(y))], fill=rnd.choice([MAG, NEON_C]), width=S(0.8))
    return img


def branch(img, x, y, seed=3, flip=False, size=1.0, n=60):
    """a cherry branch reaching in from an edge, heavy with blossom"""
    rnd = random.Random(seed)
    d = ImageDraw.Draw(img)
    sx = -1 if flip else 1
    pts = [(x, y)]
    cx, cy = x, y
    for k in range(6):
        cx += sx * rnd.uniform(10, 18) * size; cy += rnd.uniform(-4, 6) * size
        pts.append((cx, cy))
    for a, b in zip(pts, pts[1:]):
        d.line([(S(a[0]), S(a[1])), (S(b[0]), S(b[1]))], fill=(50, 20, 40), width=S(2.2 * size))
    for _ in range(n):
        px, py = rnd.choice(pts)
        px += rnd.uniform(-14, 14) * size; py += rnd.uniform(-12, 10) * size
        r = rnd.uniform(1.6, 3.4) * size
        c = rnd.choice([SAKURA, PINK, SAK_D, (255, 200, 230)])
        d.ellipse([S(px - r), S(py - r), S(px + r), S(py + r)], fill=c)
    return img


def petals(img, n, seed=4, y0=0, y1=168):
    rnd = random.Random(seed)
    d = ImageDraw.Draw(img)
    for _ in range(n):
        x, y = rnd.uniform(0, 240), rnd.uniform(y0, y1)
        d.ellipse([S(x), S(y), S(x + 1.6), S(y + 1.0)], fill=rnd.choice([SAKURA, (255, 210, 236)]))
    return img


def cat(d, x, y, s=1.0, col=(12, 6, 20)):
    """a black cat, sitting, seen from behind-left"""
    d.ellipse([S(x - 5 * s), S(y - 9 * s), S(x + 5 * s), S(y)], fill=col)
    d.ellipse([S(x - 3.5 * s), S(y - 15 * s), S(x + 3.5 * s), S(y - 8 * s)], fill=col)
    d.polygon([(S(x - 3.5 * s), S(y - 13 * s)), (S(x - 3 * s), S(y - 18 * s)), (S(x - 0.8 * s), S(y - 14.5 * s))], fill=col)
    d.polygon([(S(x + 3.5 * s), S(y - 13 * s)), (S(x + 3 * s), S(y - 18 * s)), (S(x + 0.8 * s), S(y - 14.5 * s))], fill=col)
    d.line([(S(x + 4 * s), S(y - 1 * s)), (S(x + 10 * s), S(y - 2 * s)), (S(x + 12 * s), S(y - 7 * s))], fill=col, width=S(1.4 * s))


def train_front(img, cx, base, s=1.0, body=(70, 50, 96), band=MAG, lit=True, ghost=False, heads=True):
    """an old commuter car's face: a rounded roof, two windows and a door window, a destination board, headlights"""
    d = ImageDraw.Draw(img)
    w, h = 44 * s, 52 * s
    x0, y0 = cx - w / 2, base - h
    d.rounded_rectangle([S(x0), S(y0), S(x0 + w), S(base)], radius=S(8 * s), fill=body)
    d.rectangle([S(x0 + 2 * s), S(y0 + 2 * s), S(x0 + w - 2 * s), S(y0 + 6 * s)], fill=(30, 20, 40))
    d.rectangle([S(cx - 9 * s), S(y0 + 2.5 * s), S(cx + 9 * s), S(y0 + 5.5 * s)], fill=WARM)   # the destination board
    wy0, wy1 = y0 + 10 * s, y0 + 26 * s
    for (a, b) in ((x0 + 4 * s, cx - 8 * s), (cx - 6 * s, cx + 6 * s), (cx + 8 * s, x0 + w - 4 * s)):
        d.rounded_rectangle([S(a), S(wy0), S(b), S(wy1 if a != cx - 6 * s else base - 8 * s)], radius=S(2 * s),
                            fill=(255, 210, 150) if lit else (30, 30, 50))
    d.rectangle([S(x0), S(y0 + 30 * s), S(x0 + w), S(y0 + 34 * s)], fill=band)
    for hx in (x0 + 8 * s, x0 + w - 8 * s):
        d.ellipse([S(hx - 3 * s), S(base - 12 * s), S(hx + 3 * s), S(base - 6 * s)], fill=(255, 250, 230) if heads else (40, 36, 56))
    d.rectangle([S(x0 + 4 * s), S(base - 3 * s), S(x0 + w - 4 * s), S(base)], fill=(20, 14, 26))
    if heads:
        for hx in (x0 + 8 * s, x0 + w - 8 * s):
            img = glow(img, hx, base - 9 * s, 10 * s, (255, 240, 210), 0.9 if not ghost else 0.5)
    return img


def carriage(d, x, y, l, h, body=(60, 44, 86), band=MAG, lit=True):
    d.rounded_rectangle([S(x), S(y), S(x + l), S(y + h)], radius=S(3), fill=body)
    for k in range(int(l // 9)):
        wx = x + 3 + k * 9
        d.rectangle([S(wx), S(y + h * 0.2), S(wx + 5), S(y + h * 0.5)], fill=(255, 210, 150) if lit and k % 4 != 3 else (30, 26, 44))
    d.rectangle([S(x), S(y + h * 0.62), S(x + l), S(y + h * 0.7)], fill=band)


def wet(img, y_water, y_end, darken=0.6):
    return reflect(img, y_water, y_end, darken, 2)


def lantern(img, x, y, col=LAMP, r=4):
    d = ImageDraw.Draw(img)
    d.line([(S(x), S(y - 12)), (S(x), S(y - 4))], fill=(30, 20, 30), width=S(0.6))
    d.polygon([(S(x - r), S(y - 4)), (S(x + r), S(y - 4)), (S(x + r * 0.6), S(y + r)), (S(x - r * 0.6), S(y + r))], fill=(70, 40, 30))
    d.ellipse([S(x - r * 0.7), S(y - 3), S(x + r * 0.7), S(y + r * 0.8)], fill=col)
    return glow(img, x, y, r * 6, col, 0.55)


def track_persp(img, vx, vy, y1, col=RAIL, sleepers=True, gauge=26):
    """a track running away to a vanishing point"""
    d = ImageDraw.Draw(img)
    for k in range(14 if sleepers else 0):
        t = (k / 14) ** 2
        y = vy + (y1 - vy) * t
        hw = gauge * 1.6 * t
        d.line([(S(vx - hw), S(y)), (S(vx + hw), S(y))], fill=(46, 26, 50), width=S(0.5 + 2 * t))
    for sx in (-1, 1):
        d.line([(S(vx), S(vy)), (S(vx + sx * gauge), S(y1))], fill=col, width=S(1.1))
    return img


def floor_grid(img, y0, y1, vx=120, col=(120, 30, 150), lines=10):
    d = ImageDraw.Draw(img)
    for k in range(-14, 15):
        d.line([(S(vx + k * 5), S(y0)), (S(vx + k * 36), S(y1))], fill=col, width=1)
    for k in range(lines):
        y = y0 + (y1 - y0) * (k / lines) ** 2
        d.line([(0, S(y)), (W, S(y))], fill=col, width=1)
    return img


def board(d, x, y, w, h, line=NEON_C, fill=(12, 6, 24)):
    d.rectangle([S(x), S(y), S(x + w), S(y + h)], fill=fill, outline=line, width=S(0.7))


# ---------------------------------------------------------------- the vignettes
def art_platform(img):
    """Akari Heights at night: the canopy, its neon tube, a lamp, a bench, the black cat; rain on the boards"""
    sky(img, 0, 90, (12, 6, 34), (70, 20, 90), (200, 60, 140))
    stars(img, 40, 40, 11)
    img = mountain(img, 196, 88, 40, 70, (54, 20, 86))
    img = city(img, 92, 21, 8, 30)
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(92), W, S(168)], fill=(28, 16, 40))                   # the platform
    d.rectangle([0, S(92), W, S(94)], fill=(250, 200, 40))                  # its yellow edge
    d.polygon([(0, S(10)), (S(180), S(10)), (S(150), S(26)), (0, S(26))], fill=(36, 18, 52))   # the canopy
    d.line([(0, S(26)), (S(150), S(26))], fill=MAG, width=S(1.2))
    for x in (20, 80, 140):
        d.rectangle([S(x), S(26), S(x + 3), S(92)], fill=(44, 26, 60))
    d.rectangle([S(30), S(20), S(110), S(22)], fill=NEON_C)
    img = glow(img, 70, 22, 40, (60, 200, 255), 0.35)
    d = ImageDraw.Draw(img)
    board(d, 100, 36, 34, 12, line=(250, 245, 255), fill=(248, 240, 250))   # the station's name board
    d.rectangle([S(100), S(46), S(134), S(48)], fill=MAG)
    d.rectangle([S(42), S(78), S(74), S(81)], fill=(120, 40, 90))           # a bench
    d.line([(S(44), S(81)), (S(44), S(88))], fill=(80, 30, 60), width=S(1)); d.line([(S(72), S(81)), (S(72), S(88))], fill=(80, 30, 60), width=S(1))
    cat(d, 100, 92, 1.0)
    img = lantern(img, 160, 40)
    img = branch(img, 240, 8, 31, flip=True, size=1.0)
    img = wet(img, 94, 168, 0.55)
    img = rain(img, 70, (170, 180, 240), 60, seed=5)
    img = petals(img, 30, 6)
    return bloom(img, 180, 3, 0.7)


def art_train(img):
    """the night train at the platform, alone, its lights on"""
    sky(img, 0, 96, (14, 6, 38), (110, 30, 120), (255, 110, 140))
    stars(img, 30, 30, 2)
    img = sun(img, 70, 82, 22, 96)
    img = mountain(img, 60, 96, 34, 60)
    img = city(img, 96, 9, 6, 26)
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(96), W, S(168)], fill=(24, 12, 36))
    img = track_persp(img, 150, 92, 168, gauge=60)
    d = ImageDraw.Draw(img)
    carriage(d, 168, 58, 90, 34)
    img = train_front(img, 150, 100, 1.0)
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(100), S(96), S(168)], fill=(36, 20, 50)); d.line([(0, S(100)), (S(96), S(100))], fill=(250, 200, 40), width=S(1))
    img = branch(img, 0, 6, 12, size=1.1)
    img = wet(img, 102, 168, 0.6)
    img = petals(img, 40, 9)
    return bloom(img, 180, 3, 0.8)


def art_darktrain(img):
    """the train at Kuro's platform, under the station roof: every light off, the doors shut; moonlight through the roof"""
    vgrad(img, 0, 168, (8, 4, 20), (18, 10, 34))
    d = ImageDraw.Draw(img)
    for k in range(9):                                                      # the roof: trusses over the hall
        x = k * 30 - 10
        d.line([(S(x), S(0)), (S(x + 40), S(24))], fill=(30, 18, 50), width=S(2))
        d.line([(S(x + 40), S(0)), (S(x), S(24))], fill=(30, 18, 50), width=S(2))
    d.rectangle([0, S(22), W, S(26)], fill=(36, 22, 58))
    for x in (70, 150):                                                     # moonlight through gaps in the roof
        d.polygon([(S(x), S(0)), (S(x + 10), S(0)), (S(x + 40), S(168)), (S(x + 18), S(168))], fill=(26, 22, 56))
    for x in (10, 230):                                                     # the hall's pillars
        d.rectangle([S(x - 4), S(24), S(x + 4), S(100)], fill=(30, 18, 46))
    d.rectangle([0, S(98), W, S(168)], fill=(22, 12, 32))                   # the platforms, either side
    d.rectangle([0, S(98), S(84), S(100)], fill=(120, 100, 30)); d.rectangle([S(156), S(98), W, S(100)], fill=(120, 100, 30))
    img = track_persp(img, 120, 70, 168, col=(80, 70, 110), gauge=46)
    d = ImageDraw.Draw(img)
    carriage(d, 136, 54, 60, 30, body=(40, 30, 58), band=(90, 30, 80), lit=False)
    img = train_front(img, 120, 104, 1.05, body=(46, 34, 66), band=(100, 30, 90), lit=False, heads=False)
    d = ImageDraw.Draw(img)
    board(d, 30, 40, 40, 12, line=(120, 110, 140), fill=(60, 56, 72))       # the station's name board, unlit
    d.rectangle([S(30), S(50), S(70), S(52)], fill=(90, 30, 80))
    img = lantern(img, 196, 44, (150, 120, 90), 2)                           # one lamp, barely
    img = glow(img, 104, 60, 50, (120, 120, 220), 0.12)
    return bloom(img, 200, 2, 0.4)


def art_cab(img):
    """the driver's seat: the windscreen onto the neon track, the brass console, the controller"""
    sky(img, 0, 70, (16, 6, 40), (120, 30, 130), (255, 120, 150))
    img = sun(img, 120, 62, 16, 70)
    img = city(img, 70, 4, 6, 22)
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(70), W, S(168)], fill=(18, 8, 32))
    img = floor_grid(img, 70, 100)
    img = track_persp(img, 120, 70, 100, NEON_C, gauge=40)
    d = ImageDraw.Draw(img)
    d.polygon([(0, 0), (S(30), 0), (S(18), S(100)), (0, S(100))], fill=(16, 8, 24))           # the cab's pillars
    d.polygon([(W, 0), (S(210), 0), (S(222), S(100)), (W, S(100))], fill=(16, 8, 24))
    d.rectangle([0, 0, W, S(6)], fill=(16, 8, 24))
    d.polygon([(0, S(94)), (W, S(94)), (W, S(168)), (0, S(168))], fill=(34, 18, 46))          # the console
    d.line([(0, S(94)), (W, S(94))], fill=(120, 80, 60), width=S(1))
    for (cx, r, col) in ((60, 11, NEON_C), (96, 9, MAG), (190, 10, WARM)):                      # dials
        d.ellipse([S(cx - r - 2), S(108 - r - 2), S(cx + r + 2), S(108 + r + 2)], fill=GOLD_D)
        d.ellipse([S(cx - r), S(108 - r), S(cx + r), S(108 + r)], fill=(14, 8, 24))
        d.arc([S(cx - r + 2), S(108 - r + 2), S(cx + r - 2), S(108 + r - 2)], 150, 390, fill=col, width=S(1))
        d.line([(S(cx), S(108)), (S(cx + r * 0.6), S(108 - r * 0.5))], fill=(255, 240, 230), width=S(0.8))
    d.rectangle([S(130), S(98), S(168), S(122)], fill=(24, 12, 34), outline=(120, 80, 60), width=S(0.6))   # the controller
    d.line([(S(150), S(118)), (S(142), S(100))], fill=BRASS, width=S(2.4))
    d.ellipse([S(138), S(96), S(146), S(104)], fill=MAG)
    img = glow(img, 142, 100, 10, (255, 60, 200), 0.5)
    return bloom(img, 180, 3, 0.7)


def art_signalbox(img):
    """inside the signal box: the lever frame, the track diagram lit with what the line is doing"""
    vgrad(img, 0, 168, (20, 10, 40), (44, 18, 60))
    d = ImageDraw.Draw(img)
    board(d, 30, 10, 180, 50, NEON_CD, (10, 6, 22))                       # the diagram
    d.line([(S(40), S(30)), (S(200), S(30))], fill=MAG_D, width=S(1.4))
    d.line([(S(40), S(44)), (S(90), S(44)), (S(110), S(30))], fill=MAG_D, width=S(1.4))
    d.line([(S(130), S(30)), (S(150), S(44)), (S(200), S(44))], fill=MAG_D, width=S(1.4))
    for (x, y, c) in ((60, 30, NEON_C), (100, 37, MAG), (140, 37, NEON_C), (180, 44, (255, 80, 80)), (180, 30, NEON_C)):
        d.ellipse([S(x - 2.5), S(y - 2.5), S(x + 2.5), S(y + 2.5)], fill=c)
        img = glow(img, x, y, 6, c, 0.6)
        d = ImageDraw.Draw(img)
    for x in (16, 224):                                                   # signal lamps
        d.rectangle([S(x - 4), S(14), S(x + 4), S(38)], fill=(14, 8, 20))
        d.ellipse([S(x - 2.5), S(17), S(x + 2.5), S(22)], fill=(255, 80, 80) if x < 100 else (30, 30, 30))
        d.ellipse([S(x - 2.5), S(29), S(x + 2.5), S(34)], fill=(30, 30, 30) if x < 100 else NEON_C)
    d.rectangle([S(20), S(96), S(220), S(168)], fill=(50, 26, 40))         # the frame
    d.rectangle([S(20), S(94), S(220), S(98)], fill=GOLD_D)
    for k in range(12):
        x = 30 + k * 16
        lean = 6 if k in (2, 5, 9) else -2
        col = [(200, 40, 60), (40, 80, 200), (230, 190, 60), (40, 40, 40)][k % 4]
        d.line([(S(x), S(96)), (S(x + lean), S(64))], fill=(200, 196, 210), width=S(2))
        d.rectangle([S(x + lean - 2.5), S(60), S(x + lean + 2.5), S(66)], fill=col)
        d.rectangle([S(x - 3), S(98), S(x + 3), S(102)], fill=(250, 240, 220))
    img = lantern(img, 120, 76, WARM, 3)
    return bloom(img, 180, 3, 0.6)


def art_workshop(img):
    """the line's workshop: an arched window onto the yard, the bench, gears, a blueprint, a lamp"""
    vgrad(img, 0, 168, (34, 14, 50), (60, 24, 66))
    win = new(NIGHT)
    sky(win, 0, 90, (14, 6, 36), (100, 28, 120), (255, 110, 150))
    win = sun(win, 70, 80, 16, 90)
    win = city(win, 90, 33, 6, 28)
    dw = ImageDraw.Draw(win)
    carriage(dw, 20, 70, 110, 18)
    m = Image.new("L", img.size, 0)
    ImageDraw.Draw(m).rounded_rectangle([S(14), S(6), S(124), S(92)], radius=S(40), fill=255)
    img.paste(win, (0, 0), m)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([S(14), S(6), S(124), S(92)], radius=S(40), outline=(80, 40, 70), width=S(2))
    for x in (51, 87): d.line([(S(x), S(8)), (S(x), S(92))], fill=(80, 40, 70), width=S(1.4))
    d.line([(S(14), S(50)), (S(124), S(50))], fill=(80, 40, 70), width=S(1.4))
    img = branch(img, 14, 10, 8, size=0.8, n=40)
    d = ImageDraw.Draw(img)
    gear(d, 184, 34, 22, 20, 0.2, BRASS, GOLD_D, None, 5, 3)               # a great gear on the wall
    gear(d, 212, 66, 12, 12, 0.5, (200, 160, 90), GOLD_D, None, 0, 2.4)
    d.rectangle([0, S(96), W, S(168)], fill=(70, 34, 40))                   # the bench
    d.line([(0, S(96)), (W, S(96))], fill=(140, 80, 70), width=S(1))
    d.polygon([(S(130), S(104)), (S(200), S(100)), (S(206), S(118)), (S(136), S(122))], fill=(200, 220, 255))   # a blueprint
    for k in range(4):
        d.line([(S(140), S(106 + k * 4)), (S(196), S(103 + k * 4))], fill=(60, 90, 180), width=1)
    d.ellipse([S(160), S(104), S(176), S(118)], outline=(60, 90, 180), width=1)
    gear(d, 40, 108, 8, 10, 0.3, BRASS, GOLD_D, None, 0, 2)
    gear(d, 58, 112, 5, 8, 0.1, (220, 210, 200), (120, 110, 100), None, 0, 1.6)
    img = lantern(img, 150, 62, WARM, 5)
    return bloom(img, 180, 3, 0.6)


def art_map(img):
    """the map from the safe: the line as the map screen shows it, the loop, its branches, and in the middle a '?'"""
    vgrad(img, 0, 168, (16, 8, 36), (30, 12, 50))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([S(30), S(4), S(210), S(102)], radius=S(6), fill=(18, 10, 44), outline=MAG_D, width=S(1.2))
    P = {"iid": (22, 50), "shi": (32, 16), "ish": (12, 4), "fun": (2, 30), "tak": (56, 8), "tow": (78, 24), "awa": (82, 62),
         "keg": (98, 84), "ika": (64, 92), "nag": (34, 88), "oly": (10, 90), "hrt": (52, 52)}
    xy = {k: (40 + v[0] * 1.6, 10 + v[1] * 0.86) for k, v in P.items()}
    loop = ["iid", "shi", "tak", "tow", "awa", "ika", "nag", "iid"]
    for i in range(len(loop) - 1):                                          # the Tsuru loop: cyan to magenta
        a, b = xy[loop[i]], xy[loop[i + 1]]
        t = i / (len(loop) - 1)
        col = tuple(int(NEON_C[j] * (1 - t) + MAG[j] * t) for j in range(3))
        d.line([(S(a[0]), S(a[1])), (S(b[0]), S(b[1]))], fill=col, width=S(1.6))
    for a, b in (("shi", "ish"), ("ish", "fun")):                           # the shore branch: amber
        d.line([(S(xy[a][0]), S(xy[a][1])), (S(xy[b][0]), S(xy[b][1]))], fill=(255, 168, 60), width=S(1.6))
    for (a, b), col in ((("awa", "keg"), (255, 214, 120)), (("nag", "oly"), (255, 130, 200))):   # the two rides: zigzags
        (x0, y0), (x1, y1) = xy[a], xy[b]
        ln = ((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5; nx, ny = -(y1 - y0) / ln, (x1 - x0) / ln
        pts = [(S(x0 + (x1 - x0) * k / 6 + nx * (1.6 if k % 2 else -1.6) * (0 < k < 6)), S(y0 + (y1 - y0) * k / 6 + ny * (1.6 if k % 2 else -1.6) * (0 < k < 6))) for k in range(7)]
        d.line(pts, fill=col, width=S(1.3))
    a, b = xy["shi"], xy["hrt"]                                             # the spur under Pacific: dashed, faint
    for k in range(6):
        f0, f1 = k / 6, (k + 0.5) / 6
        d.line([(S(a[0] + (b[0] - a[0]) * f0), S(a[1] + (b[1] - a[1]) * f0)), (S(a[0] + (b[0] - a[0]) * f1), S(a[1] + (b[1] - a[1]) * f1))], fill=(120, 30, 100), width=S(1))
    for k, (x, y) in xy.items():
        if k == "hrt": continue
        d.rounded_rectangle([S(x - 3.5), S(y - 3.5), S(x + 3.5), S(y + 3.5)], radius=S(1.5), fill=(18, 10, 44), outline=NEON_C, width=S(1))
    deco(d, xy["hrt"][0] - 4, xy["hrt"][1] - 7, "?", 12, MAG, stroke=1.2)
    img = glow(img, xy["hrt"][0], xy["hrt"][1], 20, (255, 60, 200), 0.35)
    img = glow(img, 120, 54, 90, (90, 200, 255), 0.1)
    return bloom(img, 190, 3, 0.6)


def art_timetable(img):
    """the departures board in the hall: split-flap rows, the last line blank, a light flickering"""
    vgrad(img, 0, 168, (16, 8, 34), (36, 14, 56))
    d = ImageDraw.Draw(img)
    board(d, 20, 8, 200, 84, (120, 80, 60), (8, 6, 14))
    rows = ["22:10 PACIFIC", "22:24 MERIDIAN", "22:41 CHROME", "23:02 LIBERTY", "23:30 AVALON", "--:-- SHINZO"]
    for i, r in enumerate(rows):
        y = 12 + i * 14
        for j, ch in enumerate(r):
            x = 32 + j * 8
            d.rectangle([S(x), S(y), S(x + 7), S(y + 13)], fill=(26, 20, 30))
            d.line([(S(x), S(y + 6.5)), (S(x + 7), S(y + 6.5))], fill=(8, 6, 12), width=1)
        img = term(img, 32, y - 1, r, WARM if i < 5 else MAG)
        d = ImageDraw.Draw(img)
    d.rectangle([0, S(100), W, S(168)], fill=(30, 16, 40))
    for x in (40, 120, 200):
        img = glow(img, x, 100, 30, (255, 150, 200), 0.2)
    return bloom(img, 190, 3, 0.6)


def art_pocketwatch(img):
    """the driver's pocket watch on the cab's console, its chain trailing, stopped at 23:47, the neon line behind"""
    sky(img, 0, 66, (14, 6, 40), (120, 30, 130), (255, 120, 150))
    stars(img, 24, 40, 8)
    img = sun(img, 196, 58, 14, 66)
    img = city(img, 66, 14, 6, 24)
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(66), W, S(96)], fill=(18, 8, 32))
    img = floor_grid(img, 66, 96)
    img = track_persp(img, 120, 66, 96, NEON_C, gauge=36)
    d = ImageDraw.Draw(img)
    d.polygon([(0, 0), (S(24), 0), (S(14), S(96)), (0, S(96))], fill=(16, 8, 24))          # the cab's pillars
    d.polygon([(W, 0), (S(216), 0), (S(226), S(96)), (W, S(96))], fill=(16, 8, 24))
    d.polygon([(0, S(92)), (W, S(92)), (W, S(168)), (0, S(168))], fill=(34, 18, 46))        # the console
    d.line([(0, S(92)), (W, S(92))], fill=(120, 80, 60), width=S(1))
    cx, cy, r = 120, 106, 38
    img = glow(img, cx, cy, 70, (255, 60, 200), 0.35)
    d = ImageDraw.Draw(img)
    for k in range(16):                                                    # the chain, trailing off to the left
        t = k / 15
        x, y = cx - 30 - t * 96, cy - 26 + t * 50 + 10 * math.sin(t * 3.0)
        d.ellipse([S(x - 2.4), S(y - 1.6), S(x + 2.4), S(y + 1.6)], outline=GOLD_D, width=S(0.9))
    d.ellipse([S(cx - 6), S(cy - r - 20), S(cx + 6), S(cy - r - 8)], outline=BRASS, width=S(1.6))   # the bow
    d.rectangle([S(cx - 3), S(cy - r - 9), S(cx + 3), S(cy - r + 1)], fill=GOLD_D)                 # the crown
    d.rectangle([S(cx - 5), S(cy - r - 12), S(cx + 5), S(cy - r - 7)], fill=BRASS)
    d.ellipse([S(cx - r - 3), S(cy - r - 3), S(cx + r + 3), S(cy + r + 3)], fill=GOLD_D)    # the case
    d.ellipse([S(cx - r - 1), S(cy - r - 1), S(cx + r + 1), S(cy + r + 1)], fill=BRASS)
    d.ellipse([S(cx - r + 2), S(cy - r + 2), S(cx + r - 2), S(cy + r - 2)], fill=(14, 8, 24))   # the face
    for k in range(60):                                                    # seconds: a tick each, a long one each five
        a = 2 * math.pi * k / 60 - math.pi / 2
        r0 = r - 9 if k % 5 == 0 else r - 6
        d.line([(S(cx + r0 * math.cos(a)), S(cy + r0 * math.sin(a))), (S(cx + (r - 3) * math.cos(a)), S(cy + (r - 3) * math.sin(a)))],
               fill=MAG if k % 15 == 0 else SNOW, width=S(1.0 if k % 5 == 0 else 0.5))
    d.ellipse([S(cx - 9), S(cy + 8), S(cx + 9), S(cy + 26)], outline=NEON_CD, width=S(0.8))   # the small seconds dial, still
    d.line([(S(cx), S(cy + 17)), (S(cx), S(cy + 10))], fill=NEON_C, width=S(0.9))
    for ang, l, wdt, col in ((-90 + 30 * 11.78, r - 18, 2.4, SNOW), (-90 + 6 * 47, r - 8, 1.4, MAG)):   # the hands, stopped at 23:47
        a = math.radians(ang)
        d.line([(S(cx), S(cy)), (S(cx + l * math.cos(a)), S(cy + l * math.sin(a)))], fill=col, width=S(wdt))
    d.ellipse([S(cx - 3), S(cy - 3), S(cx + 3), S(cy + 3)], fill=MAG)
    d.ellipse([S(cx - 1.2), S(cy - 1.2), S(cx + 1.2), S(cy + 1.2)], fill=(255, 230, 250))
    d.chord([S(cx - r + 4), S(cy - r + 4), S(cx + r - 4), S(cy + r - 4)], 200, 250, fill=(60, 40, 90))   # a glint on the glass
    img = glow(img, cx, cy, 24, (255, 80, 200), 0.25)
    return bloom(img, 180, 3, 0.6)


def art_office(img):
    """Kuro's station office after dark: the platform's neon through the window, a desk lamp left on, the safe"""
    vgrad(img, 0, 124, (18, 8, 34), (34, 14, 50))                          # the wall
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(124), W, S(168)], fill=(24, 12, 32))                 # the floor, boards
    for k in range(7):
        y = 124 + k * 7
        d.line([(0, S(y)), (W, S(y))], fill=(34, 18, 42), width=S(0.6))
    wx0, wy0, wx1, wy1 = 16, 30, 96, 96                                    # the window onto the platform
    vgrad(img, wy0, 78, (16, 6, 44), (220, 70, 150), wx0, wx1)
    d = ImageDraw.Draw(img)
    d.rectangle([S(wx0), S(78), S(wx1), S(wy1)], fill=(26, 14, 38))        # the platform outside
    d.rectangle([S(wx0), S(78), S(wx1), S(79.5)], fill=(250, 200, 40))
    img = glow(img, 51, 46, 22, (60, 200, 255), 0.3)
    d = ImageDraw.Draw(img)
    d.rectangle([S(31), S(38), S(71), S(55)], fill=(8, 4, 18), outline=NEON_C, width=S(1.0))   # the station's sign
    img = term(img, 35, 39, "KURO", NEON_C)
    d = ImageDraw.Draw(img)
    d.rectangle([S(84), S(56), S(86), S(78)], fill=(40, 24, 56))           # a lamp post, lit
    img = glow(img, 85, 56, 5, (255, 220, 160), 0.5)
    d = ImageDraw.Draw(img)
    d.rectangle([S(wx0), S(wy0), S(wx1), S(wy1)], outline=(60, 34, 70), width=S(2.4))   # the frame, the bars
    d.line([(S(wx0), S(63)), (S(wx1), S(63))], fill=(60, 34, 70), width=S(1.6))
    d.polygon([(S(96), S(96)), (S(112), S(124)), (S(40), S(124)), (S(16), S(96))], fill=(30, 16, 48))   # its light on the floor
    board(d, 174, 34, 46, 40, line=(90, 60, 110), fill=(26, 18, 30))       # a timetable on the wall
    for k in range(6):
        d.line([(S(178), S(40 + k * 5.5)), (S(178 + 30 + (k * 7) % 10), S(40 + k * 5.5))], fill=(120, 110, 160), width=S(0.6))
    d.rectangle([S(4), S(104), S(96), S(109)], fill=(56, 28, 62))          # the desk
    d.rectangle([S(8), S(109), S(14), S(140)], fill=(40, 20, 48)); d.rectangle([S(86), S(109), S(92), S(140)], fill=(40, 20, 48))
    d.polygon([(S(22), S(104)), (S(40), S(100)), (S(56), S(103)), (S(38), S(107))], fill=SNOW)   # papers
    d.line([(S(76), S(104)), (S(76), S(84))], fill=GOLD_D, width=S(1.2))   # the lamp
    d.line([(S(76), S(84)), (S(66), S(78))], fill=GOLD_D, width=S(1.2))
    d.polygon([(S(60), S(80)), (S(72), S(74)), (S(70), S(84))], fill=BRASS)
    img = glow(img, 64, 90, 12, (255, 196, 120), 0.35)
    d = ImageDraw.Draw(img)
    sx0, sy0, sx1, sy1 = 120, 62, 196, 146                                 # the safe: heavy, brass-trimmed, shut
    d.rectangle([S(sx0 + 4), S(sy1), S(sx1 + 10), S(sy1 + 6)], fill=(12, 6, 18))   # its shadow
    d.rectangle([S(sx0), S(sy0), S(sx1), S(sy1)], fill=(44, 24, 60), outline=GOLD, width=S(1.6))
    d.rectangle([S(sx0 + 7), S(sy0 + 8), S(sx1 - 7), S(sy1 - 10)], fill=(30, 20, 40), outline=(80, 50, 96), width=S(0.8))
    rays(d, 158, sy0 + 8, 9, 14, MAG_D, MAG_D, 20, 160, 0.4)
    for y in (sy0 + 16, sy1 - 18):                                         # hinges
        d.rectangle([S(sx0 - 3), S(y), S(sx0 + 2), S(y + 8)], fill=BRASS)
    dx, dy = 148, 104                                                      # the dial
    d.ellipse([S(dx - 14), S(dy - 14), S(dx + 14), S(dy + 14)], fill=GOLD)
    d.ellipse([S(dx - 12), S(dy - 12), S(dx + 12), S(dy + 12)], fill=(16, 10, 24))
    for k in range(20):
        a = 2 * math.pi * k / 20
        r0 = 8 if k % 5 == 0 else 9.5
        d.line([(S(dx + r0 * math.cos(a)), S(dy + r0 * math.sin(a))), (S(dx + 11 * math.cos(a)), S(dy + 11 * math.sin(a)))], fill=SNOW, width=S(0.6))
    d.ellipse([S(dx - 4), S(dy - 4), S(dx + 4), S(dy + 4)], fill=BRASS)
    d.polygon([(S(dx - 2), S(dy - 16)), (S(dx + 2), S(dy - 16)), (S(dx), S(dy - 13))], fill=MAG)
    hx, hy = 176, 104                                                      # the handle: three spokes
    for k in range(3):
        a = math.radians(-90 + k * 120 + 15)
        d.line([(S(hx), S(hy)), (S(hx + 9 * math.cos(a)), S(hy + 9 * math.sin(a)))], fill=BRASS, width=S(1.6))
        d.ellipse([S(hx + 9 * math.cos(a) - 2), S(hy + 9 * math.sin(a) - 2), S(hx + 9 * math.cos(a) + 2), S(hy + 9 * math.sin(a) + 2)], fill=GOLD)
    d.ellipse([S(hx - 3), S(hy - 3), S(hx + 3), S(hy + 3)], fill=GOLD_D)
    img = glow(img, dx, dy, 26, (255, 60, 200), 0.4)
    return bloom(img, 180, 3, 0.6)


def tram_side(img, x0, yb, l=150, upper=(236, 220, 186), lower=(132, 34, 48), lamp=WARM, lanterns=False):
    """a vintage tram from the side: two-tone, varnished, its windows lit, a trolley pole up to the wire, a headlamp
    on the right, its door open and lit (ready to board)"""
    d = ImageDraw.Draw(img)
    h, top = 50, yb - 50
    d.rounded_rectangle([S(x0 + 10), S(top - 7), S(x0 + l - 10), S(top + 2)], radius=S(4), fill=(70, 46, 40))   # the clerestory roof
    d.rounded_rectangle([S(x0), S(top), S(x0 + l), S(yb)], radius=S(9), fill=upper)
    d.rectangle([S(x0), S(top + 27), S(x0 + l), S(yb - 4)], fill=lower)
    d.rectangle([S(x0), S(top + 26), S(x0 + l), S(top + 27.6)], fill=GOLD)
    for k in range(7):                                                     # the windows, lit warm
        wx = x0 + 10 + k * 18
        if wx + 12 > x0 + l - 26: break
        d.rounded_rectangle([S(wx), S(top + 6), S(wx + 12), S(top + 21)], radius=S(2), fill=(255, 196, 110), outline=(96, 54, 40), width=S(1.0))
        d.line([(S(wx + 6), S(top + 6)), (S(wx + 6), S(top + 21))], fill=(150, 100, 70), width=S(0.6))
    dx = x0 + l - 24                                                       # the door, open: light spilling out
    d.rectangle([S(dx), S(top + 5), S(dx + 12), S(yb - 4)], fill=(255, 226, 170))
    img = glow(img, dx + 6, yb - 6, 16, (255, 200, 120), 0.45)
    d = ImageDraw.Draw(img)
    d.rectangle([S(dx - 1), S(yb - 4), S(dx + 13), S(yb - 2)], fill=(120, 90, 60))   # its step
    for wx in (x0 + 22, x0 + l - 22):                                      # the bogies
        d.rounded_rectangle([S(wx - 16), S(yb - 3), S(wx + 16), S(yb + 4)], radius=S(2), fill=(26, 16, 26))
        for o in (-9, 9): d.ellipse([S(wx + o - 4), S(yb), S(wx + o + 4), S(yb + 8)], fill=(40, 30, 40), outline=(90, 70, 80), width=S(0.6))
    d.line([(S(x0 + l * 0.55), S(top - 7)), (S(x0 + l * 0.85), S(top - 38))], fill=(40, 30, 40), width=S(1.4))   # the trolley pole
    d.ellipse([S(x0 + l + 1), S(yb - 22), S(x0 + l + 7), S(yb - 14)], fill=(255, 248, 220))   # the headlamp, lit
    img = glow(img, x0 + l + 6, yb - 18, 14, (255, 236, 190), 0.8)
    if lanterns:                                                           # the festival's: lanterns along the roof
        d = ImageDraw.Draw(img)
        d.line([(S(x0 + 4), S(top - 3)), (S(x0 + l - 4), S(top - 3))], fill=(60, 30, 40), width=S(0.6))
        for k in range(8):                                                 # (small, their light kept soft: the tram stays readable)
            lx = x0 + 10 + k * (l - 20) / 7
            d.line([(S(lx), S(top - 3)), (S(lx), S(top))], fill=(60, 30, 40), width=S(0.5))
            d.ellipse([S(lx - 2.4), S(top), S(lx + 2.4), S(top + 5)], fill=(255, 80 + (k % 3) * 50, 60))
        img = glow(img, x0 + l / 2, top + 2, l * 0.45, (255, 110, 70), 0.18)
    return img


def palm(img, x, yb, h=60, flip=False):
    d = ImageDraw.Draw(img)
    sx = -1 if flip else 1
    pts = [(x + sx * 6 * math.sin(t * 1.4) * (t), yb - h * t) for t in [k / 12 for k in range(13)]]
    for a, b in zip(pts, pts[1:]): d.line([(S(a[0]), S(a[1])), (S(b[0]), S(b[1]))], fill=(24, 10, 30), width=S(3.2))
    tx, ty = pts[-1]
    for a in range(-160, 30, 28):                                          # the fronds
        r = math.radians(a)
        ex, ey = tx + 26 * math.cos(r), ty + 12 * math.sin(r) + 8
        d.line([(S(tx), S(ty)), (S((tx + ex) / 2), S(min(ty, ey) - 4)), (S(ex), S(ey))], fill=(24, 10, 30), width=S(2.2), joint="curve")
    return img


def art_seatram(img):
    """the seaside tram at its stop on the front at dusk: varnish and brass, lamps lit, door open, the sea behind"""
    sky(img, 0, 96, (40, 14, 70), (200, 60, 120), (255, 170, 110))
    stars(img, 12, 34, 21)
    img = sun(img, 70, 90, 20, 96)
    d = ImageDraw.Draw(img)
    vgrad(img, 96, 122, (190, 70, 120), (60, 30, 90))                      # the sea
    d = ImageDraw.Draw(img)
    for k in range(6): d.line([(S(52 - k * 3), S(99 + k * 4)), (S(88 + k * 3), S(99 + k * 4))], fill=(255, 190, 120), width=S(0.8))   # the sun on the water
    d.rectangle([0, S(122), W, S(168)], fill=(36, 18, 44))                # the promenade
    d.rectangle([0, S(122), W, S(124)], fill=(120, 70, 100))
    d.line([(0, S(152)), (W, S(152))], fill=RAIL, width=S(1.2))           # the rails, set in the road
    d.line([(0, S(156)), (W, S(156))], fill=RAIL, width=S(1.2))
    d.line([(0, S(52)), (W, S(52))], fill=(40, 24, 50), width=S(0.8))     # the wire
    img = palm(img, 16, 124, 74)
    img = palm(img, 226, 124, 64, flip=True)
    d = ImageDraw.Draw(img)
    d.rectangle([S(206), S(112), S(208), S(150)], fill=(60, 40, 60))      # the stop: a post, its sign
    d.rounded_rectangle([S(197), S(104), S(217), S(114)], radius=S(2), fill=(250, 240, 220), outline=MAG, width=S(0.6))
    d.rectangle([S(201), S(108), S(213), S(110)], fill=(40, 120, 170))
    img = tram_side(img, 34, 150, 150)
    return bloom(img, 185, 3, 0.7)


def art_festtram(img):
    """the festival tram in Union's sidings at night, hung with lanterns under the cherries, its cab lit, waiting"""
    vgrad(img, 0, 120, (10, 6, 30), (60, 20, 80))
    stars(img, 40, 70, 22)
    img = city(img, 108, 61, 6, 22)
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(108), W, S(168)], fill=(30, 16, 40))                # the yard
    for y in (128, 160):                                                  # the sidings either side
        d.line([(0, S(y)), (W, S(y))], fill=(110, 90, 140), width=S(0.8)); d.line([(0, S(y + 3)), (W, S(y + 3))], fill=(110, 90, 140), width=S(0.8))
    d.line([(0, S(150)), (W, S(150))], fill=RAIL, width=S(1.2)); d.line([(0, S(154)), (W, S(154))], fill=RAIL, width=S(1.2))
    for k in range(9):                                                    # lanterns strung across the yard
        x = 8 + k * 28
        y = 40 + 8 * math.sin(k * 0.8)
        img = lantern(img, x, y, (255, 120 + (k % 3) * 40, 80), 3)
    d = ImageDraw.Draw(img)
    for a, b in ((0, 36), (36, 44), (44, 36), (36, 44)): pass
    d.line([(0, S(36)), (S(240), S(36))], fill=(50, 26, 40), width=S(0.6))
    img = branch(img, 0, 8, 51, size=1.2, n=80)
    img = branch(img, 240, 20, 52, flip=True, size=1.1, n=70)
    img = tram_side(img, 40, 148, 150, upper=(210, 192, 168), lower=(170, 30, 44), lanterns=True)
    img = petals(img, 50, 53, 30, 168)
    img = wet(img, 156, 168, 0.5)
    return bloom(img, 190, 3, 0.6)


def art_harbour(img):
    """the harbour at sunset: cranes, freight wagons on the quay, the sun on the water"""
    sky(img, 0, 76, (40, 14, 70), (200, 60, 120), (255, 170, 110))
    img = sun(img, 160, 66, 24, 76)
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(76), W, S(168)], fill=(60, 20, 100))
    for k in range(12):                                                   # the sun's road on the sea
        y = 78 + k * 3.2
        w = 6 + k * 2.5
        d.line([(S(160 - w), S(y)), (S(160 + w), S(y))], fill=(255, 140, 180) if k % 2 == 0 else (200, 80, 160), width=S(0.8))
    for (x, h) in ((20, 54), (60, 66)):                                    # cranes
        d.line([(S(x), S(76)), (S(x), S(76 - h))], fill=(26, 10, 40), width=S(2.4))
        d.line([(S(x - 10), S(76 - h)), (S(x + 50), S(76 - h + 4))], fill=(26, 10, 40), width=S(2))
        d.line([(S(x + 40), S(76 - h + 4)), (S(x + 40), S(60))], fill=(26, 10, 40), width=S(0.6))
        d.ellipse([S(x - 1.5), S(76 - h - 4), S(x + 1.5), S(76 - h - 1)], fill=(255, 60, 80))
    d.rectangle([0, S(96), S(150), S(168)], fill=(30, 14, 40))              # the quay
    for k in range(3):
        carriage(d, 4 + k * 46, 82, 42, 14, body=[(90, 40, 60), (40, 60, 90), (90, 70, 40)][k], band=(30, 14, 40), lit=False)
    img = wet(img, 98, 168, 0.5)
    return bloom(img, 190, 3, 0.6)


def art_archive(img):
    """the line's archive: shelves of ledgers to the ceiling, a reading lamp, a drawer left open, a key in it"""
    vgrad(img, 0, 168, (20, 8, 40), (40, 16, 60))
    d = ImageDraw.Draw(img)
    rnd = random.Random(14)
    for sx in (0, 160):
        for row in range(5):
            y = 6 + row * 18
            d.rectangle([S(sx), S(y + 16), S(sx + 80), S(y + 18)], fill=(80, 40, 50))
            x = sx + 2
            while x < sx + 78:
                w = rnd.uniform(3, 6)
                d.rectangle([S(x), S(y + rnd.uniform(2, 6)), S(x + w), S(y + 16)],
                            fill=rnd.choice([(120, 40, 60), (60, 50, 110), (150, 110, 60), (60, 90, 100), (90, 30, 80)]))
                x += w + 0.6
    d.rectangle([S(84), S(70), S(156), S(168)], fill=(70, 34, 40))          # the desk
    d.rectangle([S(96), S(92), S(144), S(104)], fill=(50, 24, 30))          # a drawer, open
    d.rectangle([S(96), S(92), S(144), S(96)], fill=(110, 60, 50))
    d.ellipse([S(112), S(96), S(120), S(102)], outline=GOLD, width=S(1))    # the key
    d.line([(S(120), S(99)), (S(132), S(99))], fill=GOLD, width=S(1))
    d.line([(S(128), S(99)), (S(128), S(102))], fill=GOLD, width=S(0.8))
    img = glow(img, 122, 99, 10, (255, 200, 100), 0.6)
    d = ImageDraw.Draw(img)
    d.polygon([(S(100), S(70)), (S(140), S(70)), (S(136), S(64)), (S(104), S(64))], fill=(230, 214, 200))   # a ledger, open
    d.line([(S(120), S(64)), (S(120), S(70))], fill=(120, 100, 100), width=1)
    img = lantern(img, 150, 40, WARM, 4)
    img = glow(img, 120, 66, 40, (255, 170, 120), 0.3)
    return bloom(img, 190, 3, 0.5)


def art_tunnel(img):
    """the tunnel mouth in the hill: rain, a neon strip over the portal, a sealed side door with light behind it"""
    sky(img, 0, 50, (10, 6, 30), (60, 20, 90), (130, 40, 120))
    img = mountain(img, 120, 60, 50, 160, (40, 16, 66), None)
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(40), W, S(168)], fill=(34, 14, 50))
    d.rounded_rectangle([S(70), S(36), S(170), S(168)], radius=S(46), fill=(6, 2, 12))   # the mouth
    d.rounded_rectangle([S(66), S(32), S(174), S(168)], radius=S(50), outline=(90, 60, 100), width=S(2.4))
    d.line([(S(80), S(30)), (S(160), S(30))], fill=NEON_C, width=S(1.2))
    img = glow(img, 120, 30, 30, (60, 200, 255), 0.4)
    img = track_persp(img, 120, 72, 168, RAIL, gauge=34)
    d = ImageDraw.Draw(img)
    d.rectangle([S(190), S(70), S(210), S(104)], fill=(60, 40, 50), outline=GOLD_D, width=S(0.8))   # the sealed door
    for k in range(3): d.line([(S(190), S(76 + k * 10)), (S(210), S(76 + k * 10))], fill=(40, 26, 30), width=S(0.8))
    d.line([(S(190), S(104)), (S(210), S(104))], fill=WARM, width=S(0.8))
    img = glow(img, 200, 104, 10, (255, 200, 120), 0.6)
    img = rain(img, 90, (170, 180, 240), 70, seed=8)
    return bloom(img, 180, 3, 0.6)


def art_observatory(img):
    """the observatory on the hill: the dome open to the stars, the telescope, the line curving below"""
    sky(img, 0, 100, (6, 4, 24), (40, 14, 80), (120, 40, 130))
    stars(img, 120, 90, 3)
    d = ImageDraw.Draw(img)
    for k in range(5):                                                    # star trails
        r = 30 + k * 12
        d.arc([S(180 - r), S(10 - r), S(180 + r), S(10 + r)], 90, 170, fill=(110, 90, 180), width=1)
    d.polygon([(0, S(168)), (0, S(80)), (S(80), S(64)), (S(170), S(74)), (W, S(94)), (W, S(168))], fill=(24, 10, 40))   # the hill
    d.rectangle([S(60), S(50), S(120), S(70)], fill=(70, 50, 90))
    d.pieslice([S(56), S(26), S(124), S(74)], 180, 360, fill=(110, 90, 140))
    d.polygon([(S(84), S(28)), (S(96), S(28)), (S(96), S(50)), (S(84), S(50))], fill=(20, 10, 34))
    d.line([(S(90), S(44)), (S(112), S(18))], fill=GOLD, width=S(3))
    img = glow(img, 90, 50, 14, (255, 200, 120), 0.4)
    d = ImageDraw.Draw(img)
    d.arc([S(-40), S(86), S(280), S(150)], 190, 350, fill=NEON_C, width=S(0.8))   # the line below
    carriage(d, 150, 96, 40, 8, lit=True)
    return bloom(img, 180, 3, 0.6)


def art_depot(img):
    """the depot at dawn: the turntable, the roundhouse's doors, an old engine waiting, mist"""
    sky(img, 0, 70, (80, 90, 180), (200, 150, 210), (255, 200, 200))
    img = mountain(img, 60, 70, 30, 70, (120, 100, 170), (255, 236, 246))
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(70), W, S(168)], fill=(70, 50, 90))
    d.rectangle([S(120), S(30), S(240), S(80)], fill=(90, 50, 80))          # the roundhouse
    for k in range(4):
        x = 126 + k * 28
        d.rounded_rectangle([S(x), S(46), S(x + 22), S(80)], radius=S(10), fill=(30, 20, 40))
    d.rectangle([S(120), S(26), S(240), S(30)], fill=(60, 30, 60))
    d.ellipse([S(20), S(84), S(120), S(116)], fill=(50, 34, 60), outline=GOLD_D, width=S(1))   # the turntable
    d.line([(S(24), S(100)), (S(116), S(100))], fill=RAIL, width=S(1.4))
    for k in range(5):
        a = math.radians(-150 + k * 30)
        d.line([(S(70 + 50 * math.cos(a)), S(100 + 16 * math.sin(a))), (S(130 + 30 * k), S(80))], fill=(150, 130, 170), width=1)
    img = train_front(img, 154, 80, 0.55, (60, 40, 50), GOLD, lit=False)
    img = fog(img, 70, 110, (230, 190, 230), 0.45, 4)
    return img


def art_ghost(img):
    """the train that runs at night with no one aboard: pale, through the mist, its windows empty"""
    sky(img, 0, 100, (6, 4, 22), (20, 10, 50), (50, 20, 80))
    stars(img, 50, 50, 17)
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(90), W, S(168)], fill=(12, 6, 26))
    img = track_persp(img, 120, 60, 168, (80, 70, 120), gauge=50)
    ghost = new(NIGHT)
    ghost = train_front(ghost, 120, 98, 0.9, (120, 220, 255), NEON_C, lit=False, ghost=True)
    a = np.asarray(img).astype(np.float32)
    g = np.asarray(ghost).astype(np.float32)
    mask = (g.max(axis=2) > 30)[..., None]
    a = np.where(mask, a * 0.45 + g * 0.55, a)
    img = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
    img = glow(img, 120, 70, 40, (60, 200, 255), 0.4)
    img = fog(img, 76, 116, (90, 150, 200), 0.5, 7)
    return bloom(img, 160, 4, 0.9)


def art_door(img):
    """a door under the platform, at the end of a stair: light at its edges, a mechanism set in it"""
    vgrad(img, 0, 168, (10, 4, 24), (30, 12, 46))
    d = ImageDraw.Draw(img)
    for k in range(7):                                                    # the stair down
        y = 80 + k * 6
        d.rectangle([S(60 - k * 4), S(y), S(180 + k * 4), S(y + 6)], fill=(40 + k * 4, 20 + k * 2, 60 + k * 3))
    d.rectangle([S(84), S(10), S(156), S(80)], fill=(40, 22, 40))
    d.rectangle([S(84), S(10), S(156), S(80)], outline=GOLD_D, width=S(1.4))
    d.line([(S(120), S(10)), (S(120), S(80))], fill=(20, 10, 20), width=S(1))
    gear(d, 120, 40, 14, 16, 0.2, BRASS, GOLD_D, None, 6, 2.4)
    for (x0, y0, x1, y1) in ((84, 10, 156, 10), (84, 80, 156, 80), (84, 10, 84, 80), (156, 10, 156, 80)):
        d.line([(S(x0), S(y0)), (S(x1), S(y1))], fill=(255, 200, 150), width=S(0.6))
    img = glow(img, 120, 80, 50, (255, 160, 200), 0.35)
    img = glow(img, 120, 40, 20, (255, 210, 140), 0.3)
    return bloom(img, 180, 3, 0.7)


def art_heart(img):
    """the heart of the line, under Akari Heights: a vast clockwork, rails converging on it, its core lit"""
    vgrad(img, 0, 168, (8, 4, 20), (30, 10, 50))
    d = ImageDraw.Draw(img)
    for k in range(8):                                                    # rails converging on it
        a = math.radians(200 + k * 20)
        d.line([(S(120), S(60)), (S(120 + 200 * math.cos(a)), S(60 - 120 * math.sin(a)))], fill=(70, 40, 100), width=S(1))
    gear(d, 120, 60, 44, 36, 0.1, (150, 110, 60), (90, 60, 30), None, 8, 4)
    gear(d, 62, 40, 22, 20, 0.4, BRASS, GOLD_D, None, 5, 3)
    gear(d, 182, 82, 26, 22, 0.2, BRASS, GOLD_D, None, 6, 3)
    gear(d, 196, 26, 14, 14, 0.5, (200, 170, 110), GOLD_D, None, 0, 2.4)
    d.ellipse([S(104), S(44), S(136), S(76)], fill=(20, 8, 30))
    d.ellipse([S(110), S(50), S(130), S(70)], fill=MAG)
    d.ellipse([S(116), S(56), S(124), S(64)], fill=(255, 230, 250))
    img = glow(img, 120, 60, 40, (255, 60, 200), 0.6)
    img = glow(img, 120, 60, 80, (60, 200, 255), 0.2)
    return bloom(img, 170, 4, 0.9)


def art_sakura(img):
    """the line under the cherries at dusk: petals on the rails, the mountain, the sun going down"""
    sky(img, 0, 86, (30, 10, 70), (170, 50, 140), (255, 140, 140))
    img = sun(img, 120, 74, 22, 86)
    img = mountain(img, 120, 86, 36, 90)
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(86), W, S(168)], fill=(50, 22, 70))
    img = track_persp(img, 120, 86, 168, RAIL, gauge=46)
    img = branch(img, 0, 4, 41, size=1.3, n=90)
    img = branch(img, 240, 14, 42, flip=True, size=1.2, n=80)
    img = petals(img, 90, 43, 40, 168)
    return bloom(img, 185, 3, 0.7)


def art_keeper(img):
    """the old stationmaster, a figure with a lantern at the end of the platform, in the rain"""
    sky(img, 0, 96, (10, 6, 30), (60, 20, 90), (160, 50, 130))
    img = city(img, 96, 51, 8, 36)
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(96), W, S(168)], fill=(26, 14, 38))
    d.line([(0, S(96)), (W, S(96))], fill=(250, 200, 40), width=S(1))
    x, y = 150, 96                                                        # the figure: coat, cap, lantern
    for (ox, col) in ((1.2, MAG), (-1.2, NEON_CD)):                       # a rim of neon from the signs behind
        d.polygon([(S(x - 9 + ox), S(y)), (S(x - 6 + ox), S(y - 30)), (S(x + 6 + ox), S(y - 30)), (S(x + 9 + ox), S(y))], fill=col)
        d.ellipse([S(x - 5 + ox), S(y - 40), S(x + 5 + ox), S(y - 29)], fill=col)
        d.rectangle([S(x - 7 + ox), S(y - 42), S(x + 7 + ox), S(y - 39)], fill=col)
    d.polygon([(S(x - 9), S(y)), (S(x - 6), S(y - 30)), (S(x + 6), S(y - 30)), (S(x + 9), S(y))], fill=(14, 6, 22))
    d.ellipse([S(x - 5), S(y - 40), S(x + 5), S(y - 29)], fill=(14, 6, 22))
    d.rectangle([S(x - 7), S(y - 42), S(x + 7), S(y - 39)], fill=(14, 6, 22))
    d.line([(S(x - 6), S(y - 24)), (S(x - 14), S(y - 14))], fill=(14, 6, 22), width=S(2.2))
    img = lantern(img, x - 15, y - 10, WARM, 3)
    img = wet(img, 98, 168, 0.6)
    img = rain(img, 100, (170, 180, 240), 70, seed=12)
    return bloom(img, 180, 3, 0.7)


ARTS = {
    "platform": art_platform, "train": art_train, "cab": art_cab, "signalbox": art_signalbox,
    "workshop": art_workshop, "map": art_map, "timetable": art_timetable, "pocketwatch": art_pocketwatch,
    "harbour": art_harbour, "archive": art_archive, "tunnel": art_tunnel, "observatory": art_observatory,
    "depot": art_depot, "ghost": art_ghost, "door": art_door, "heart": art_heart, "sakura": art_sakura,
    "keeper": art_keeper, "darktrain": art_darktrain, "office": art_office, "seatram": art_seatram, "festtram": art_festtram,
}
