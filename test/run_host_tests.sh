#!/bin/sh
# Host-Tests: Firmware-Logik mit simulierter Zeit und Hardware, ASan + UBSan.
# Drei Varianten: ohne und mit Tempolimit (SPEED_LIMIT_PCT_S).
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/test/host/build"
CXX=${CXX:-c++}
mkdir -p "$OUT"
FLAGS="-std=c++17 -g -O1 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter
       -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
       -I$ROOT/test/host -I$ROOT/firmware/UselessMachine"
status=0
for variant in "nolimit:" "limit200:-DSPEED_LIMIT_PCT_S=200.0f" "limit10:-DSPEED_LIMIT_PCT_S=10.0f"; do
  name=${variant%%:*}
  def=${variant#*:}
  $CXX $FLAGS $def "$ROOT/test/host/test_main.cpp" -o "$OUT/test_$name"
  echo "== $name"
  "$OUT/test_$name" || status=1
done
echo "== Skript-Prüfung"
python3 "$ROOT/tools/validate_scripts.py" || status=1
exit $status
