#!/usr/bin/env python3
# trunk-ignore-all(ruff/F821)
# trunk-ignore-all(flake8/F821): For SConstruct imports
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

# Ensure meshtastic-device-ui grants WeatherXMTFTView access to ui_set_active
tft_header_pattern = os.path.join(
    libdeps_dir,
    "*",
    "meshtastic-device-ui",
    "include",
    "graphics",
    "view",
    "TFT",
    "TFTView_320x240.h",
)

for path in glob.glob(tft_header_pattern):
    if os.path.exists(path):
        with open(path, "r") as f:
            content = f.read()
        if "WeatherXMTFTView" not in content:
            target = "friend class ViewFactory;\n"
            replacement = (
                "friend class ViewFactory;\n    friend class WeatherXMTFTView;\n"
            )
            if target in content:
                content = content.replace(target, replacement, 1)
                with open(path, "w") as f:
                    f.write(content)
                print(f"Patched {path} with WeatherXMTFTView friend class")
