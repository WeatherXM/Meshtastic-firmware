#!/usr/bin/env python3
"""Bump the version number or vendor revision in version.properties."""
import re
import sys

bump_rev = any(arg in sys.argv for arg in ("--rev", "--wxm", "--revision"))

lines = None

with open('version.properties', 'r', encoding='utf-8') as f:
    lines = f.readlines()

with open('version.properties', 'w', encoding='utf-8') as f:
    for line in lines:
        stripped = line.lstrip()
        if bump_rev and (stripped.startswith("rev = ") or stripped.startswith("revision = ")):
            prefix, val = line.split(" = ", 1)
            val = re.sub(r"(\d+)(?=\s*$)", lambda m: str(int(m.group(1)) + 1), val.strip())
            f.write(f"{prefix} = {val}\n")
        elif not bump_rev and stripped.startswith("build = "):
            words = line.split(" = ")
            ver = f"build = {int(words[1]) + 1}"
            f.write(f"{ver}\n")
        else:
            f.write(line)
