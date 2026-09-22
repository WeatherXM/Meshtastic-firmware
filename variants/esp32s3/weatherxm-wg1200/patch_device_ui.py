#!/usr/bin/env python3
import glob
import os

Import("env")

# Ensure meshtastic-device-ui has GFX_DRIVER_INC support in DisplayDriverFactory.cpp
libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")
pattern = os.path.join(
    libdeps_dir,
    "*",
    "meshtastic-device-ui",
    "source",
    "graphics",
    "driver",
    "DisplayDriverFactory.cpp",
)

for path in glob.glob(pattern):
    if os.path.exists(path):
        with open(path, "r") as f:
            content = f.read()
        if "GFX_DRIVER_INC" not in content:
            target = "#ifndef ARCH_PORTDUINO\n"
            replacement = (
                target + "#ifdef GFX_DRIVER_INC\n#include GFX_DRIVER_INC\n#endif\n"
            )
            if target in content:
                content = content.replace(target, replacement, 1)
                with open(path, "w") as f:
                    f.write(content)
                print(f"Patched {path} with GFX_DRIVER_INC support")
