#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Mechanica Obscura: art-direction mockups, "Weimar clockpunk".

Fuses 1920s Berlin (Metropolis-poster gold rays, stepped Deco towers, expressionist faces, cabaret pinks),
cyberpunk (night, fog, wet reflections, neon signage) and steampunk (brass, gears, airships, clock faces),
with UI panels like boutique effect pedals: black lacquer, engraved neon line-work, chunky knob markings.

Everything is rendered at 3x and reduced to the FM-1's 240 x 240, then quantized to RGB565, so what you see is
what the panel can show. Body text uses the firmware's own Terminus (parsed from felucca_font.h); titles use a
Deco stroke alphabet defined here as line segments, small enough to ship in the firmware.

Dense backdrops are meant as baked art (palettized + LZ4 in flash, decoded once per scene into a background
buffer); only the mechanisms, needles, lights and text are drawn live. `asset_kb()` estimates that cost.
"""
import math, random, re, zlib
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageChops

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "docs" / "style"
SS = 3                      # supersampling
W = 240 * SS

# ---- palette: night ink, Metropolis gold, brass, neon cyan, cabaret rose, amber, cream
INK = (8, 10, 18); NAVY = (16, 22, 40); STEEL = (38, 54, 80); STEEL_L = (90, 116, 150)
GOLD = (236, 186, 92); GOLD_L = (255, 226, 150); GOLD_D = (150, 100, 38); GOLD_DD = (80, 50, 18)
BRASS = (196, 148, 70); COPPER = (178, 100, 62)
CYAN = (80, 226, 255); CYAN_D = (22, 96, 130); ROSE = (240, 92, 126); ROSE_D = (110, 30, 52)
DIX = (214, 120, 128); AMBER = (255, 168, 60); CREAM = (242, 228, 196); RED = (210, 44, 52); VERD = (90, 200, 160)

def new(bg=INK):
    return Image.new("RGB", (W, W), bg)

def S(v):                   # 240-space -> supersampled
    return int(round(v * SS))

def vgrad(img, y0, y1, c0, c1, x0=0, x1=240):
    d = ImageDraw.Draw(img)
    for y in range(S(y0), S(y1)):
        t = (y - S(y0)) / max(1, S(y1) - S(y0))
        d.line([(S(x0), y), (S(x1) - 1, y)], fill=tuple(int(a + (b - a) * t) for a, b in zip(c0, c1)))

def glow(img, x, y, r, col, strength=1.0):
    """additive soft light (baked; on the device: a precomputed sprite blended in)"""
    a = np.asarray(img).astype(np.float32)
    yy, xx = np.ogrid[:W, :W]
    d2 = ((xx - S(x)) ** 2 + (yy - S(y)) ** 2) / float(S(r)) ** 2
    f = np.exp(-d2 * 2.2) * strength
    a += f[..., None] * np.array(col, np.float32)[None, None, :]
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))

def bloom(img, thresh=170, radius=6, amount=0.9, sat_only=True):
    a = np.asarray(img).astype(np.float32)
    lum = a.max(axis=2)
    m = np.clip((lum - thresh) / (255 - thresh), 0, 1)
    if sat_only:                             # neon and lamps bloom; brass and paper do not
        sat = (a.max(axis=2) - a.min(axis=2)) / np.maximum(a.max(axis=2), 1)
        m = m * np.clip((sat - 0.45) / 0.3, 0, 1)
    m = m[..., None] * a
    b = Image.fromarray(m.astype(np.uint8)).filter(ImageFilter.GaussianBlur(S(radius)))
    a += np.asarray(b).astype(np.float32) * amount
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))

def fog(img, y0, y1, col, alpha, seed=1):
    rnd = np.random.default_rng(seed)
    a = np.asarray(img).astype(np.float32)
    n = Image.fromarray((rnd.random((24, 24)) * 255).astype(np.uint8)).resize((W, W), Image.BICUBIC)
    n = np.asarray(n).astype(np.float32) / 255
    yy = np.arange(W)[:, None]
    band = np.clip(1 - np.abs((yy - S((y0 + y1) / 2)) / (S(y1 - y0) / 2)), 0, 1) ** 1.2
    f = (band * (0.55 + 0.45 * n) * alpha)[..., None]
    a = a * (1 - f) + np.array(col, np.float32) * f
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))

def overlay(img, draw_fn, alpha=255):
    lay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(lay))
    if alpha < 255:
        r, g, b, a = lay.split()
        a = a.point(lambda v: v * alpha // 255)
        lay = Image.merge("RGBA", (r, g, b, a))
    return Image.alpha_composite(img.convert("RGBA"), lay).convert("RGB")

def rays(d, cx, cy, n, r, c0, c1, a0=0, a1=360, width=0.45):
    """Metropolis sunburst: alternating wedges"""
    for i in range(n):
        a = math.radians(a0 + (a1 - a0) * i / n); b = math.radians(a0 + (a1 - a0) * (i + width) / n)
        d.polygon([(S(cx), S(cy)), (S(cx + r * math.cos(a)), S(cy + r * math.sin(a))),
                   (S(cx + r * math.cos(b)), S(cy + r * math.sin(b)))], fill=c0 if i % 2 == 0 else c1)

def gear(d, cx, cy, r, teeth, ang, face, dark, hub=None, spokes=5, th=3.0):
    pts = []
    for k in range(teeth):
        base = ang + 2 * math.pi * k / teeth; p = 2 * math.pi / teeth
        for (da, rr) in ((-0.32, r - th * .6), (-0.16, r + th), (0.16, r + th), (0.32, r - th * .6)):
            pts.append((S(cx + rr * math.cos(base + da * p)), S(cy + rr * math.sin(base + da * p))))
    d.polygon(pts, fill=dark)
    d.ellipse([S(cx - r + 1.5), S(cy - r + 1.5), S(cx + r - 1.5), S(cy + r - 1.5)], fill=face)
    if spokes and r > 12:
        ri = r * 0.72
        d.ellipse([S(cx - ri), S(cy - ri), S(cx + ri), S(cy + ri)], fill=dark)
        for s in range(spokes):
            a = ang + 2 * math.pi * s / spokes
            d.line([(S(cx), S(cy)), (S(cx + ri * math.cos(a)), S(cy + ri * math.sin(a)))], fill=face, width=S(max(2, r / 6)))
        d.ellipse([S(cx - r * .28), S(cy - r * .28), S(cx + r * .28), S(cy + r * .28)], fill=face)
    d.ellipse([S(cx - 2), S(cy - 2), S(cx + 2), S(cy + 2)], fill=hub or dark)

# ---- the firmware's own body font (Terminus 8x16, 4-bit alpha) from felucca_font.h
def _load_font():
    src = (ROOT / "firmware/gen/felucca_font.h").read_text()
    def arr(name):
        m = re.search(name + r"\[\d+\] = \{([^}]*)\}", src)
        return [int(v, 0) for v in m.group(1).replace("\n", " ").split(",") if v.strip()]
    return dict(data=arr("FONT_S_DATA"), off=arr("FONT_S_OFF"), adv=arr("FONT_S_ADV"), bw=arr("FONT_S_BW"), h=16, pad=2, first=32)
FONT = _load_font()

def term(img, x, y, s, col, glow_col=None):
    """draw Terminus text at 240-scale (each font pixel = SS x SS), blended"""
    a = np.asarray(img).astype(np.float32)
    f = FONT
    for ch in s:
        o = ord(ch)
        if o < 32 or o > 126: o = 63
        gi = o - f["first"]; w = f["bw"][gi]; bpr = (w + 1) // 2; base = f["off"][gi]
        for gy in range(16):
            for gx in range(w):
                v = f["data"][base + gy * bpr + gx // 2]
                v = (v & 15) if gx & 1 else (v >> 4)
                if v:
                    px, py = x - f["pad"] + gx, y + gy
                    if 0 <= px < 240 and 0 <= py < 240:
                        al = v / 15
                        sl = a[py * SS:(py + 1) * SS, px * SS:(px + 1) * SS]
                        sl[:] = sl * (1 - al) + np.array(col, np.float32) * al
        x += f["adv"][gi]
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))

def term_w(s): return 8 * len(s)

# ---- the Deco title alphabet: strokes on a 4 x 10 grid (tall, narrow, Metropolis-like); shippable as data
DECO = {
    "A": [[(0, 10), (2, 0), (4, 10)], [(1, 7), (3, 7)]],
    "B": [[(0, 0), (0, 10), (3, 10), (4, 8), (4, 6), (3, 5), (0, 5)], [(0, 0), (3, 0), (3.6, 1.5), (3.6, 3.5), (3, 5)]],
    "C": [[(4, 1), (3, 0), (1, 0), (0, 1.5), (0, 8.5), (1, 10), (3, 10), (4, 9)]],
    "D": [[(0, 0), (0, 10), (2.5, 10), (4, 8), (4, 2), (2.5, 0), (0, 0)]],
    "E": [[(4, 0), (0, 0), (0, 10), (4, 10)], [(0, 4), (3, 4)]],
    "F": [[(4, 0), (0, 0), (0, 10)], [(0, 4), (3, 4)]],
    "G": [[(4, 1), (3, 0), (1, 0), (0, 1.5), (0, 8.5), (1, 10), (4, 10), (4, 6), (2.2, 6)]],
    "H": [[(0, 0), (0, 10)], [(4, 0), (4, 10)], [(0, 4), (4, 4)]],
    "I": [[(0, 0), (0, 10)]],
    "K": [[(0, 0), (0, 10)], [(4, 0), (0, 6)], [(1.3, 4.6), (4, 10)]],
    "L": [[(0, 0), (0, 10), (4, 10)]],
    "M": [[(0, 10), (0, 0), (2, 7), (4, 0), (4, 10)]],
    "N": [[(0, 10), (0, 0), (4, 10), (4, 0)]],
    "O": [[(1, 0), (3, 0), (4, 1.5), (4, 8.5), (3, 10), (1, 10), (0, 8.5), (0, 1.5), (1, 0)]],
    "P": [[(0, 10), (0, 0), (3, 0), (4, 1.5), (4, 4), (3, 5.5), (0, 5.5)]],
    "R": [[(0, 10), (0, 0), (3, 0), (4, 1.5), (4, 4), (3, 5.5), (0, 5.5)], [(2, 5.5), (4, 10)]],
    "S": [[(4, 1), (3, 0), (1, 0), (0, 1.5), (0, 3.5), (4, 6.5), (4, 8.5), (3, 10), (1, 10), (0, 9)]],
    "T": [[(0, 0), (4, 0)], [(2, 0), (2, 10)]],
    "U": [[(0, 0), (0, 8.5), (1, 10), (3, 10), (4, 8.5), (4, 0)]],
    "V": [[(0, 0), (2, 10), (4, 0)]],
    "W": [[(0, 0), (0.8, 10), (2, 3), (3.2, 10), (4, 0)]],
    "Y": [[(0, 0), (2, 5), (4, 0)], [(2, 5), (2, 10)]],
    "X": [[(0, 0), (4, 10)], [(4, 0), (0, 10)]],
    "Z": [[(0, 0), (4, 0), (0, 10), (4, 10)], [(1, 5), (3, 5)]],
    "J": [[(4, 0), (4, 8.5), (3, 10), (1, 10), (0, 8.5)]],
    "Q": [[(1, 0), (3, 0), (4, 1.5), (4, 8.5), (3, 10), (1, 10), (0, 8.5), (0, 1.5), (1, 0)], [(2.5, 7.5), (4.4, 11)]],
    "'": [[(1, 0), (0.6, 2.5)]],
    " ": [],
    "I.": [],
}
def deco(d, x, y, s, h, col, w=None, gap=None, stroke=None):
    """title text, cap height h px (240-scale); returns width"""
    sc = h / 10; cw = 4 * sc; gap = gap if gap is not None else 2.2 * sc; stroke = stroke or max(1.0, h / 9)
    for ch in s:
        for line in DECO.get(ch, []):
            pts = [(S(x + px * sc + (stroke / 2 if ch in "I'" else 0)), S(y + py * sc)) for px, py in line]
            d.line(pts, fill=col, width=S(stroke), joint="curve")
        x += (stroke + gap) if ch in "I'" else (cw + gap) if ch != " " else cw
    return x

def deco_w(s, h, gap=None, stroke=None):
    sc = h / 10; cw = 4 * sc; gap = gap if gap is not None else 2.2 * sc; stroke = stroke or max(1.0, h / 9)
    return sum((stroke + gap) if ch in "I'" else (cw + gap) if ch != " " else cw for ch in s) - gap

def deco_c(d, cx, y, s, h, col, **k):
    deco(d, cx - deco_w(s, h, k.get("gap"), k.get("stroke")) / 2, y, s, h, col, **k)

# ---- the city: stepped Deco towers, lit windows, antennae, signs
def skyline(img, y_base, seed, n, hmin, hmax, body, lit_cols, density=0.35, step=True, zig=True):
    rnd = random.Random(seed)
    d = ImageDraw.Draw(img)
    x = -10
    while x < 250:
        w = rnd.randint(12, 30); h = rnd.randint(hmin, hmax); top = y_base - h
        d.rectangle([S(x), S(top), S(x + w), S(y_base)], fill=body)
        if step and rnd.random() < 0.7:          # Deco setbacks
            for k in range(1, rnd.randint(2, 4)):
                inset = w * 0.16 * k
                d.rectangle([S(x + inset), S(top - k * 6), S(x + w - inset), S(top - (k - 1) * 6)], fill=body)
            top -= 18
        if zig and rnd.random() < 0.3:
            d.line([(S(x + w / 2), S(top)), (S(x + w / 2), S(top - rnd.randint(8, 22)))], fill=body, width=S(1))
        for wy in range(int(top + 22), int(y_base - 2), 4):
            for wx in range(int(x + 2), int(x + w - 2), 3):
                if rnd.random() < density:
                    c = rnd.choice(lit_cols)
                    d.rectangle([S(wx), S(wy), S(wx) + SS - 1, S(wy) + SS - 1], fill=c)
        x += w + rnd.randint(-4, 3)
    return img

def rain(img, n, col, alpha, seed=3, ang=0.25):
    rnd = random.Random(seed)
    def f(d):
        for _ in range(n):
            x, y = rnd.uniform(-20, 240), rnd.uniform(-20, 240); l = rnd.uniform(6, 14)
            d.line([(S(x), S(y)), (S(x - l * ang), S(y + l))], fill=col + (alpha,), width=1)
    return overlay(img, f)

def reflect(img, y_water, y_end, darken=0.45, blur=3):
    a = img.crop((0, S(2 * y_water - y_end), W, S(y_water))).transpose(Image.FLIP_TOP_BOTTOM)
    a = a.filter(ImageFilter.GaussianBlur(S(blur) / 2)).resize((W, S(y_end - y_water)))
    a = np.asarray(a).astype(np.float32) * darken
    rows = np.arange(a.shape[0])[:, None, None]
    wob = (np.sin(rows / 2.5) * 0.12 + 0.88)
    a = a * wob
    base = np.asarray(img).astype(np.float32)
    base[S(y_water):S(y_end)] = base[S(y_water):S(y_end)] * 0.35 + a
    return Image.fromarray(np.clip(base, 0, 255).astype(np.uint8))

def airship(d, x, y, l, col, light=None):
    d.ellipse([S(x), S(y), S(x + l), S(y + l * 0.32)], fill=col)
    d.polygon([(S(x - 3), S(y + l * .05)), (S(x + l * .12), S(y + l * .16)), (S(x - 3), S(y + l * .27))], fill=col)
    d.rectangle([S(x + l * .35), S(y + l * .3), S(x + l * .62), S(y + l * .42)], fill=col)
    if light:
        for k in range(4):
            d.rectangle([S(x + l * .38 + k * l * .055), S(y + l * .34), S(x + l * .38 + k * l * .055) + SS, S(y + l * .37)], fill=light)

def beam(img, x0, y0, ang, spread, length, col, alpha):
    def f(d):
        a, b = math.radians(ang - spread / 2), math.radians(ang + spread / 2)
        d.polygon([(S(x0), S(y0)), (S(x0 + length * math.cos(a)), S(y0 + length * math.sin(a))),
                   (S(x0 + length * math.cos(b)), S(y0 + length * math.sin(b)))], fill=col + (alpha,))
    return overlay(img, f)

def neon_sign(img, x, y, text, col, h=9, vertical=False, frame=True):
    """a neon word in the Deco alphabet, with its halo"""
    d = ImageDraw.Draw(img)
    if vertical:
        for i, ch in enumerate(text):
            deco(d, x, y + i * (h + 3), ch, h, col, stroke=1.2)
    else:
        wv = deco(d, x, y, text, h, col, stroke=1.2) - x
        if frame:
            d.rectangle([S(x - 3), S(y - 3), S(x + wv + 1), S(y + h + 3)], outline=col, width=S(0.7))
    return img

# ---- pedal-style panels: black lacquer, engraved line-work in a neon colour, Deco corner fans
def panel(d, x, y, w, h, line=CYAN, fill=(6, 8, 14), fans=True, double=True):
    d.rectangle([S(x), S(y), S(x + w), S(y + h)], fill=fill)
    d.rectangle([S(x + 2), S(y + 2), S(x + w - 2), S(y + h - 2)], outline=line, width=S(0.7))
    if double:
        d.rectangle([S(x + 4), S(y + 4), S(x + w - 4), S(y + h - 4)], outline=tuple(c // 2 for c in line), width=S(0.5))
    if fans:
        for (cx, cy, a0) in ((x + 2, y + 2, 0), (x + w - 2, y + 2, 90), (x + w - 2, y + h - 2, 180), (x + 2, y + h - 2, 270)):
            for k in range(5):
                a = math.radians(a0 + 9 + k * 18)
                d.line([(S(cx), S(cy)), (S(cx + 9 * math.cos(a)), S(cy + 9 * math.sin(a)))], fill=line, width=1)
            d.arc([S(cx - 9), S(cy - 9), S(cx + 9), S(cy + 9)], a0, a0 + 90, fill=line, width=S(0.6))

def knob_mark(d, cx, cy, r, val, line, label=None, img=None):
    """a pedal knob as UI: dotted scale, chunky knob, pointer"""
    for k in range(11):
        a = math.radians(135 + 270 * k / 10)
        rr = r + 3.2
        d.ellipse([S(cx + rr * math.cos(a)) - SS // 2, S(cy + rr * math.sin(a)) - SS // 2,
                   S(cx + rr * math.cos(a)) + SS // 2, S(cy + rr * math.sin(a)) + SS // 2], fill=line)
    d.ellipse([S(cx - r), S(cy - r), S(cx + r), S(cy + r)], fill=(20, 20, 24), outline=(70, 70, 80), width=S(0.6))
    d.ellipse([S(cx - r * .72), S(cy - r * .72), S(cx + r * .72), S(cy + r * .72)], fill=(34, 34, 40))
    a = math.radians(135 + 270 * val)
    d.line([(S(cx + r * .2 * math.cos(a)), S(cy + r * .2 * math.sin(a))), (S(cx + r * .9 * math.cos(a)), S(cy + r * .9 * math.sin(a)))], fill=CREAM, width=S(1.3))

def header(img, num, title, line=GOLD):
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, W, S(19)], fill=(6, 8, 14))
    d.line([(0, S(19)), (W, S(19))], fill=line, width=S(0.8))
    d.line([(0, S(21)), (W, S(21))], fill=tuple(c // 2 for c in line), width=S(0.5))
    rays(d, 120, 19, 14, 30, tuple(c // 5 for c in line), (6, 8, 14), 180, 360, 0.5)
    d.rectangle([S(0), S(0), S(240), S(4)], fill=(6, 8, 14))
    wnum = 0
    if num:
        deco(d, 6, 5, num, 10, line, stroke=1.3)
    h = 10
    while deco_w(title, h, 1.6, 1.1) > 200 and h > 6: h -= 0.5
    deco_c(d, 126, 5 + (10 - h) / 2, title, h, line, stroke=1.1, gap=1.6)
    return img

def footer(img, items, line=CYAN):
    """knob legends, as printed under pedal knobs"""
    d = ImageDraw.Draw(img)
    d.rectangle([0, S(222), W, W], fill=(6, 8, 14))
    d.line([(0, S(222)), (W, S(222))], fill=line, width=S(0.6))
    x = 6
    for lab, col in items:
        img = term(img, x, 223, lab, col)
        x += term_w(lab) + 10
    return img

def finish(img, name, grain=0.0):
    small = img.resize((240, 240), Image.LANCZOS)
    a = np.asarray(small).astype(np.int32)
    if grain:
        rnd = np.random.default_rng(7)
        a = a + (rnd.standard_normal(a.shape[:2]) * grain)[..., None]
    a = np.clip(a, 0, 255).astype(np.int32)
    a565 = np.stack([(a[..., 0] >> 3) << 3, (a[..., 1] >> 2) << 2, (a[..., 2] >> 3) << 3], -1).astype(np.uint8)
    out = Image.fromarray(a565)
    OUT.mkdir(parents=True, exist_ok=True)
    out.save(OUT / f"{name}.png")
    out.resize((480, 480), Image.NEAREST).save(OUT / f"{name}@2x.png")
    return out

def asset_kb(im, colours=64):
    """the backdrop as it would ship: a palette of N colours, indices compressed (zlib as a stand-in for LZ4)"""
    q = im.quantize(colors=colours, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    raw = bytes(q.getdata())
    return len(zlib.compress(raw, 9)) / 1024, q
