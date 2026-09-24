#!/usr/bin/env python3
# trunk-ignore-all(ruff/F821)
# trunk-ignore-all(flake8/F821): For SConstruct imports
import glob
import json
import os

Import("env")


def remove_factory_bin(source, target, env):
    """Remove dangerous factory.bin immediately after app bin creation."""
    build_dir = env.subst("$BUILD_DIR")
    progname = env.subst("${PROGNAME}")

    for pattern in [
        os.path.join(build_dir, f"{progname}.factory.bin"),
        os.path.join(build_dir, "*.factory.bin"),
    ]:
        for factory_bin in glob.glob(pattern):
            if os.path.exists(factory_bin):
                try:
                    os.remove(factory_bin)
                    print(
                        f"[WG1200 Safety] Removed {os.path.basename(factory_bin)} (factory images overwrite stock WeatherXM / esp_secure_cert)."
                    )
                except OSError:
                    pass


def wg1200_post_mtjson_cleanup(source, target, env):
    """Clean up factory.bin, littlefs.bin, and manifest entries after mtjson completes."""
    build_dir = env.subst("$BUILD_DIR")
    progname = env.subst("${PROGNAME}")

    # 1. Clean up any leftover factory.bin
    remove_factory_bin(source, target, env)

    # 2. Remove littlefs.bin (WG1200 initializes LittleFS at runtime; no static preload needed)
    for pattern in [
        os.path.join(build_dir, "littlefs-*.bin"),
        os.path.join(build_dir, f"{progname.replace('firmware-', 'littlefs-')}.bin"),
    ]:
        for lfs_bin in glob.glob(pattern):
            if os.path.exists(lfs_bin):
                try:
                    os.remove(lfs_bin)
                    print(
                        f"[WG1200 Safety] Removed {os.path.basename(lfs_bin)} (WG1200 initializes LittleFS at runtime)."
                    )
                except OSError:
                    pass

    # 3. Clean up mt.json references to removed files
    mt_json_path = os.path.join(build_dir, f"{progname}.mt.json")
    if os.path.exists(mt_json_path):
        try:
            with open(mt_json_path, "r") as jf:
                data = json.load(jf)
            if "files" in data:
                data["files"] = [
                    f
                    for f in data["files"]
                    if not (
                        f.get("name", "").endswith(".factory.bin")
                        or f.get("name", "").startswith("littlefs-")
                    )
                ]
            with open(mt_json_path, "w") as jf:
                json.dump(data, jf, indent=2)
            print(
                f"[WG1200 Safety] Updated {os.path.basename(mt_json_path)} to omit factory.bin and littlefs.bin."
            )
        except Exception as e:
            print(f"[WG1200 Safety] Note: Failed updating mt.json: {e}")


# Run factory.bin removal right after binary creation
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", remove_factory_bin)

# Run complete cleanup after mtjson target finishes
env.AddPostAction("mtjson", wg1200_post_mtjson_cleanup)
