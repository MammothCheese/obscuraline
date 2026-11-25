#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Mechanica Obscura: the sliding-panel cabinet's layouts. A solver (used by tools/content.py to check every
layout in puzzles.txt) and a generator of new ones:

    python3 tools/slider.py 8 20        print layouts needing 8..20 moves, ready to paste into puzzles.txt

A layout is six rows of six: '.' empty, 'X' the key panel (two long, in the third row, leaving by the opening
on the right), other letters the panels (two or three long, straight). A panel slides along its length only;
one move = one panel slid any distance.
"""
import random, sys
from collections import deque

N = 6


def parse(rows):
    """'AA...B ..C..B ...' -> {letter: (r, c, length, horizontal)}; raises ValueError with the reason"""
    rows = rows.split()
    if len(rows) != N or any(len(r) != N for r in rows):
        raise ValueError("six rows of six characters, separated by spaces")
    cells = {}
    for r, row in enumerate(rows):
        for c, ch in enumerate(row):
            if ch != ".":
                cells.setdefault(ch, []).append((r, c))
    if "X" not in cells:
        raise ValueError("no X (the key panel)")
    blocks = {}
    for ch, cs in cells.items():
        rs, cs2 = {r for r, _ in cs}, {c for _, c in cs}
        if len(cs) not in (2, 3):
            raise ValueError(f"panel {ch} is {len(cs)} long (2 or 3)")
        if len(rs) == 1 and sorted(cs2) == list(range(min(cs2), min(cs2) + len(cs))):
            blocks[ch] = (cs[0][0], min(cs2), len(cs), True)
        elif len(cs2) == 1 and sorted(rs) == list(range(min(rs), min(rs) + len(cs))):
            blocks[ch] = (min(rs), cs[0][1], len(cs), False)
        else:
            raise ValueError(f"panel {ch} is not a straight line")
    x = blocks["X"]
    if not (x[3] and x[0] == 2 and x[2] == 2):
        raise ValueError("X must lie across, two long, in the third row")
    return blocks


def solve(blocks, limit=200000):
    """minimum moves to bring X to the right edge, or None"""
    names = sorted(blocks)
    fixed = [(blocks[n][2], blocks[n][3], blocks[n][0] if blocks[n][3] else blocks[n][1]) for n in names]
    start = tuple(blocks[n][1] if blocks[n][3] else blocks[n][0] for n in names)
    xi = names.index("X")

    def grid(st):
        g = [[False] * N for _ in range(N)]
        for (ln, hz, fix), p in zip(fixed, st):
            for k in range(ln):
                if hz: g[fix][p + k] = True
                else: g[p + k][fix] = True
        return g

    seen, q = {start: 0}, deque([start])
    while q:
        st = q.popleft()
        d = seen[st]
        if st[xi] == N - 2:
            return d
        if len(seen) > limit:
            return None
        g = grid(st)
        for i, ((ln, hz, fix), p) in enumerate(zip(fixed, st)):
            for step in (-1, 1):
                np = p
                while True:
                    np += step
                    if np < 0 or np + ln > N: break
                    cell = (fix, np if step < 0 else np + ln - 1) if hz else (np if step < 0 else np + ln - 1, fix)
                    if g[cell[0]][cell[1]]: break
                    ns = st[:i] + (np,) + st[i + 1:]
                    if ns not in seen:
                        seen[ns] = d + 1
                        q.append(ns)
    return None


def generate(lo, hi, count=6, seed=1):
    rnd = random.Random(seed)
    found = []
    letters = "ABCDEFGHIJKLMNOPQRSTUVW"
    tries = 0
    while len(found) < count and tries < 200000:
        tries += 1
        g = [["."] * N for _ in range(N)]
        g[2][0] = g[2][1] = "X"
        for li in range(rnd.randint(7, 11)):
            ln = 3 if rnd.random() < 0.3 else 2
            hz = rnd.random() < 0.45
            for _ in range(30):
                r, c = rnd.randrange(N), rnd.randrange(N)
                cs = [(r, c + k) if hz else (r + k, c) for k in range(ln)]
                if any(rr >= N or cc >= N or g[rr][cc] != "." for rr, cc in cs): continue
                if hz and r == 2: continue      # nothing lies across the key's row
                for rr, cc in cs: g[rr][cc] = letters[li]
                break
        rows = " ".join("".join(r) for r in g)
        try:
            d = solve(parse(rows))
        except ValueError:
            continue
        if d is not None and lo <= d <= hi and rows not in [f[0] for f in found]:
            found.append((rows, d))
    return found


if __name__ == "__main__":
    lo, hi = (int(sys.argv[1]), int(sys.argv[2])) if len(sys.argv) > 2 else (8, 20)
    for rows, d in generate(lo, hi, 6, seed=int(sys.argv[3]) if len(sys.argv) > 3 else 1):
        print(f"layout      = {rows}      # {d} moves")
