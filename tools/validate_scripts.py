#!/usr/bin/env python3
"""Prüft alle Gesten in firmware/UselessMachine/scripts.h.

Expandiert die Makros mit dem C++-Präprozessor der ESP32-Toolchain aus dem
Arduino-Boardpaket (oder einem beliebigen g++/clang++ im PATH) und kontrolliert
Opcodes, Argumentbereiche, LOOP/NEXT-Paare und die Größe jedes Skripts.
"""
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKETCH = os.path.join(ROOT, "firmware", "UselessMachine")

OPS = ["OP_END", "OP_MOVE", "OP_MOVER", "OP_REL", "OP_WAIT", "OP_NAP", "OP_WIGGLE",
       "OP_JITTER", "OP_LOOP", "OP_NEXT", "OP_CHANCE", "OP_PUSH", "OP_HOME"]
LEN = [1, 4, 5, 4, 3, 3, 4, 3, 3, 1, 2, 3, 3]


def find_cpp():
    home = os.path.expanduser("~")
    patterns = [
        home + "/Library/Arduino15/packages/esp32/tools/*/*/bin/*-elf-g++",
        home + "/.arduino15/packages/esp32/tools/*/*/bin/*-elf-g++",
        home + "/AppData/Local/Arduino15/packages/esp32/tools/*/*/bin/*-elf-g++.exe",
    ]
    for p in patterns:
        hits = sorted(glob.glob(p))
        if hits:
            return hits[-1]
    return shutil.which("g++") or shutil.which("clang++")


def main():
    cpp = find_cpp()
    if not cpp:
        sys.exit("Kein C++-Präprozessor gefunden (ESP32-Boardpaket oder g++ installieren).")
    with tempfile.TemporaryDirectory() as stub:
        with open(os.path.join(stub, "Arduino.h"), "w") as f:
            f.write("#pragma once\n#include <stdint.h>\n")
        src = subprocess.run([cpp, "-E", "-P", "-I", stub, "-I", SKETCH, "-x", "c++",
                              os.path.join(SKETCH, "scripts.h")],
                             capture_output=True, text=True, check=True).stdout

    env = {n: i for i, n in enumerate(OPS)}
    errors = total = count = 0
    for name, body in re.findall(r"const uint8_t (\w+)\[\] = \{(.*?)\};", src, re.S):
        body = re.sub(r"\(uint8_t\)\(int8_t\)\((-?\d+)\)", lambda m: str(int(m.group(1)) & 0xFF), body)
        vals = [eval(x.strip(), {}, env) for x in body.split(",") if x.strip()]
        problems = []
        if any(not 0 <= v <= 255 for v in vals):
            problems.append("Wert passt nicht in 1 Byte")
        pc = depth = 0
        while pc < len(vals):
            op = vals[pc]
            if op >= len(OPS):
                problems.append(f"ungültiger Opcode {op} @{pc}")
                break
            a = vals[pc + 1:pc + LEN[op]]
            if len(a) != LEN[op] - 1:
                problems.append(f"{OPS[op]} abgeschnitten @{pc}")
                break
            n = OPS[op]
            if n == "OP_MOVE" and (a[0] > 100 or a[2] > 3):
                problems.append(f"MOVE-Argumente {a}")
            if n == "OP_MOVER" and (a[0] > a[1] or a[1] > 100 or a[3] > 3):
                problems.append(f"MOVER-Argumente {a}")
            if n in ("OP_WAIT", "OP_NAP", "OP_LOOP") and a[0] > a[1]:
                problems.append(f"{n}: min > max {a}")
            if n == "OP_CHANCE" and (a[0] > 100 or vals[pc + 2] == 0):
                problems.append("CHANCE ungültig oder direkt vor END")
            if n == "OP_LOOP":
                depth += 1
            if n == "OP_NEXT":
                depth -= 1
                if depth < 0:
                    problems.append("NEXT ohne LOOP")
            if n == "OP_END":
                if pc != len(vals) - 1:
                    problems.append("END ist nicht der letzte Befehl")
                break
            pc += LEN[op]
        if not vals or vals[-1] != 0:
            problems.append("END fehlt")
        if depth:
            problems.append("LOOP ohne NEXT")
        count += 1
        total += len(vals)
        status = "ok" if not problems else "FEHLER: " + "; ".join(problems)
        errors += bool(problems)
        print(f"  {name:12s} {len(vals):3d} B  {status}")

    print(f"\n{count} Gesten, {total} Bytes, {errors} fehlerhaft")
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
