#!/bin/sh
# Build the .fwsc on this Windows PC: copies the project into WSL Ubuntu (~/obscura), builds with the JieLi
# toolchain (~/.jieli/toolchain) and the AC79 SDK files (~/sdk), runs the platform tests and copies the package back
# into build/. Run from Windows, in this folder:  wsl -d Ubuntu -- sh wsl_build.sh [--release X.Y]
set -e
SRC="$(cd "$(dirname "$0")" && pwd)"
DST="$HOME/obscura"
rm -rf "$DST" && mkdir -p "$DST"
(cd "$SRC" && tar --exclude=./build --exclude=./.git -cf - .) | tar -xf - -C "$DST"
find "$DST" -type f \( -name "*.sh" -o -name "*.py" -o -name "*.c" -o -name "*.h" -o -name "*.S" -o -name "*.ld" -o -name "*.txt" \) -exec sed -i 's/\r$//' {} +
cd "$DST"
export JIELI_TOOLCHAIN="$HOME/.jieli/toolchain" AC79_SDK="$HOME/sdk"
sh build.sh "$@"
sh tests/run_tests.sh | tail -1
mkdir -p "$SRC/build"
cp build/obscura*.fwsc build/obscura.bin build/obscura.dis "$SRC/build/"
ls -l "$SRC"/build/obscura*.fwsc
