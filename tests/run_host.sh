#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Run firmware/src/obscura.c on this computer (tests/host.c): build/host/obscura.wav and the screens as PNG.
#   tests/run_host.sh tests/walkthrough.txt  the whole of Act One (SAVE solves each mechanism in host builds)
#   tests/run_host.sh [script]               your own script (tests/host.c says what a line can do)
#   BOARD=smk37 tests/run_host.sh [script]   the SMK-37 build (velocity, pads, faders, wheels, outer keys)
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host
${PYTHON:-python3} tools/content.py                 # content/*.txt -> firmware/gen/mo_content.h
N=obscura; D=
if [ "${BOARD:-fm1}" = smk37 ]; then N=obscura-smk37; D=-DBOARD_SMK37=1; fi
${CC:-cc} -O2 -Wall -Wno-unused-function -DDINGHY_HOST $D -DAPP_VER="\"${APP_VER:-dev}\"" -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o build/host/$N firmware/src/obscura.c
build/host/$N build/host/$N ${1:+"$1"}
${PYTHON:-python3} - "$N" <<'PY'
import sys
from pathlib import Path
from PIL import Image
for p in sorted(Path("build/host").glob(sys.argv[1] + "-*.ppm")):
    Image.open(p).resize((480, 480), Image.NEAREST).save(p.with_suffix(".png"))
    p.unlink()
    print("screen ->", p.with_suffix(".png"))
PY
