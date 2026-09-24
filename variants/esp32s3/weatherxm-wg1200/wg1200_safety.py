#!/usr/bin/env python3
# trunk-ignore-all(ruff/F821)
# trunk-ignore-all(flake8/F821): For SConstruct imports
import configparser
import glob
import hashlib
import json
import os
import re
import subprocess
import sys

Import("env")

# Locate helper functions from wg1200_upload.py
project_dir = env.get("PROJECT_DIR", os.getcwd())
variant_dir = os.path.join(project_dir, "variants", "esp32s3", "weatherxm-wg1200")
if variant_dir not in sys.path:
    sys.path.insert(0, variant_dir)

try:
    from wg1200_upload import find_signing_key, resolve_python_and_tool
except ImportError:
    find_signing_key = None
    resolve_python_and_tool = None


def get_clean_version(env, progname):
    """Retrieve clean short version string (e.g. '2.8.1') without commit hash."""
    version_file = os.path.join(project_dir, "version.properties")
    if os.path.isfile(version_file):
        try:
            cp = configparser.RawConfigParser()
            cp.read(version_file)
            maj = cp.get("VERSION", "major").strip()
            min = cp.get("VERSION", "minor").strip()
            bld = cp.get("VERSION", "build").strip()
            return f"{maj}.{min}.{bld}"
        except Exception:
            pass
    m = re.search(r"(\d+\.\d+\.\d+)", progname)
    return m.group(1) if m else "2.8.1"


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


def sign_release_bin(source, target, env):
    """Automatically sign the release binary with WeatherXM Secure Boot V2."""
    build_dir = env.subst("$BUILD_DIR")
    progname = env.subst("${PROGNAME}")
    pioenv = env.get("PIOENV")

    source_bin = os.path.join(build_dir, f"{progname}.bin")
    if not os.path.isfile(source_bin):
        candidates = glob.glob(os.path.join(build_dir, f"firmware-{pioenv}-*.bin"))
        candidates = [
            c
            for c in candidates
            if not c.endswith("-signed.bin") and not c.endswith(".factory.bin")
        ]
        if candidates:
            source_bin = candidates[0]
        elif source and os.path.isfile(str(source[0])):
            source_bin = str(source[0])
        elif target and os.path.isfile(str(target[0])):
            source_bin = str(target[0])

    if not os.path.isfile(source_bin):
        return

    version_short = get_clean_version(env, progname)
    release_signed_name = f"firmware-{pioenv}-{version_short}-signed.bin"
    release_signed_bin = os.path.join(build_dir, release_signed_name)

    # Clean up duplicate commit-trace signed bins if present
    for dup in glob.glob(os.path.join(build_dir, f"firmware-{pioenv}-*.*-signed.bin")):
        if os.path.abspath(dup) != os.path.abspath(release_signed_bin):
            try:
                os.remove(dup)
                print(
                    f"[WG1200 Safety] Removed duplicate commit-trace binary: {os.path.basename(dup)}"
                )
            except OSError:
                pass

    # Check if signed binary already exists and is newer than source_bin
    if os.path.isfile(release_signed_bin):
        if os.path.getmtime(release_signed_bin) >= os.path.getmtime(source_bin):
            return

    # Find key and espsecure
    signing_key = find_signing_key() if find_signing_key else None
    espsecure_cmd = (
        resolve_python_and_tool("espsecure")
        if resolve_python_and_tool
        else ["espsecure"]
    )

    if not signing_key:
        print(
            "[WG1200 Signing] WARNING: Secure Boot V2 signing key not found. Skipping release signing."
        )
        return

    print(
        f"\n[WG1200 Signing] Signing {os.path.basename(source_bin)} with Secure Boot V2..."
    )
    sign_cmd = espsecure_cmd + [
        "sign_data",
        "--version",
        "2",
        "--keyfile",
        signing_key,
        "--output",
        release_signed_bin,
        source_bin,
    ]
    res = subprocess.run(sign_cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(
            f"[WG1200 Signing] ERROR: espsecure sign_data failed:\n{res.stdout}\n{res.stderr}"
        )
        return

    verify_cmd = espsecure_cmd + [
        "verify_signature",
        "--version",
        "2",
        "--keyfile",
        signing_key,
        release_signed_bin,
    ]
    res = subprocess.run(verify_cmd, capture_output=True, text=True)
    if "successful" not in res.stdout and "valid" not in res.stdout:
        print(
            f"[WG1200 Signing] ERROR: Signature verification failed:\n{res.stdout}\n{res.stderr}"
        )
        return

    print(
        f"[WG1200 Signing] Generated signed release binary:\n  -> {release_signed_name} (Verified RSA SBv2)"
    )


def wg1200_post_mtjson_cleanup(source, target, env):
    """Clean up factory.bin, littlefs.bin, and update mt.json after mtjson completes."""
    build_dir = env.subst("$BUILD_DIR")
    progname = env.subst("${PROGNAME}")
    pioenv = env.get("PIOENV")

    # 1. Clean up any leftover factory.bin
    remove_factory_bin(source, target, env)

    # 2. Ensure signed release bin exists
    sign_release_bin(source, target, env)

    # 3. Remove littlefs.bin (WG1200 initializes LittleFS at runtime; no static preload needed)
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

    # 4. Clean up mt.json references to removed files and include signed release binary
    mt_json_path = os.path.join(build_dir, f"{progname}.mt.json")
    version_short = get_clean_version(env, progname)
    release_signed_name = f"firmware-{pioenv}-{version_short}-signed.bin"
    release_signed_bin = os.path.join(build_dir, release_signed_name)

    if os.path.exists(mt_json_path):
        try:
            with open(mt_json_path, "r") as jf:
                data = json.load(jf)
            if "files" in data:
                # Filter out unsafe factory and littlefs binaries and old duplicate signed bins
                data["files"] = [
                    f
                    for f in data["files"]
                    if not (
                        f.get("name", "").endswith(".factory.bin")
                        or f.get("name", "").startswith("littlefs-")
                        or (
                            f.get("name", "").endswith("-signed.bin")
                            and f.get("name") != release_signed_name
                        )
                    )
                ]
                # If signed release bin exists, add/update it as app0 in manifest
                if os.path.isfile(release_signed_bin):
                    with open(release_signed_bin, "rb") as rf:
                        rdata = rf.read()
                    signed_md5 = hashlib.md5(rdata).hexdigest()
                    signed_size = len(rdata)

                    existing_entry = next(
                        (
                            f
                            for f in data["files"]
                            if f.get("name") == release_signed_name
                        ),
                        None,
                    )
                    if existing_entry:
                        existing_entry["md5"] = signed_md5
                        existing_entry["bytes"] = signed_size
                        existing_entry["part_name"] = "app0"
                    else:
                        data["files"].append(
                            {
                                "name": release_signed_name,
                                "md5": signed_md5,
                                "bytes": signed_size,
                                "part_name": "app0",
                            }
                        )

            with open(mt_json_path, "w") as jf:
                json.dump(data, jf, indent=2)
            print(
                f"[WG1200 Safety] Updated {os.path.basename(mt_json_path)} with {release_signed_name}."
            )
        except Exception as e:
            print(f"[WG1200 Safety] Note: Failed updating mt.json: {e}")


# Run factory.bin removal and binary signing right after binary creation
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", remove_factory_bin)
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", sign_release_bin)

# Run complete cleanup and manifest update after mtjson target finishes
env.AddPostAction("mtjson", wg1200_post_mtjson_cleanup)
