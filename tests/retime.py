#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Sets every leg's timetable in content/lines.txt to the autopilot's time + 2 s (tests/legtime.c drives them all).

    python tests/retime.py        (builds and runs build/host/legtime, rewrites time / back_time, twice)
    python tests/retime.py enoden (the Enoden's stretches in content/enoden.txt instead: the autopilot's time + 5 s)
"""
import os, re, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))


EN = len(sys.argv) > 1 and sys.argv[1] == "enoden"
FILE = ROOT / ("content/enoden.txt" if EN else "content/lines.txt")
SLACK = 5 if EN else 2


def run():
    subprocess.run([sys.executable, str(ROOT / "tools/content.py")], check=True, capture_output=True)
    subprocess.run(["gcc", "-O2", "-w", "-DDINGHY_HOST", "-Dmain=host_main", '-DAPP_VER="dev"', "-Ifirmware/hal", "-Ifirmware/src",
                    "-Ifirmware/gen", "-o", "build/host/legtime", "tests/legtime.c"], check=True, cwd=ROOT)
    out = subprocess.run([str(ROOT / "build/host/legtime")], capture_output=True, text=True, cwd=ROOT,
                         env=dict(os.environ, DENSHA_AUTOPLAY="1", **({"LEG_ENODEN": "1"} if EN else {}))).stdout
    return re.findall(r"travel (.+?)>(.+?): .*?(-?\d+) s to spare", out)


def main():
    import content
    for it in range(2):
        st, legs = content.load_lines(FILE, returns=not EN) if EN else content.load_lines()
        names = [s["name"] for s in st]
        res = run()
        txt = FILE.read_text(encoding="utf-8")
        changed = 0
        for a, b, spare in res:
            spare = int(spare)
            fwd = [l for l in legs if names[l["from"]] == a and names[l["to"]] == b]
            if not fwd: continue
            l = fwd[0]
            back = l["id"].endswith("_back")
            lid = l["id"][:-5] if back else l["id"]
            key = "back_time" if back else "time"
            new = l["time"] - spare + SLACK
            m = re.search(r"(\[leg " + lid + r"\]\n(?:(?!\[)[^\n]*\n)*)", txt)
            blk = m.group(1)
            if re.search(r"^" + key + r" *= *\d+", blk, re.M):
                nb = re.sub(r"^(" + key + r" *= *)\d+", lambda mm: mm.group(1) + str(new), blk, count=1, flags=re.M)
            else:
                nb = re.sub(r"^(time *= *\d+\n)", lambda mm: mm.group(1) + f"back_time = {new}\n", blk, count=1, flags=re.M)
            if nb != blk: changed += 1
            txt = txt.replace(blk, nb)
            print(f"  {a:>14} > {b:<14} spare {spare:+4d} s  {key} {l['time']} -> {new}")
        FILE.write_text(txt, encoding="utf-8", newline="\n")
        print(f"pass {it + 1}: {changed} changed")
        if not changed: break
    subprocess.run([sys.executable, str(ROOT / "tools/content.py")], check=True)


if __name__ == "__main__":
    main()
