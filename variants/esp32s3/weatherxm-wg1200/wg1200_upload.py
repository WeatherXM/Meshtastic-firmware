#!/usr/bin/env python3
import glob
import os
import subprocess
import sys


def find_signing_key():
    candidates = [
        os.environ.get("WG1200_SECURE_BOOT_KEY", ""),
        os.path.expanduser(
            "~/Documents/mesh/WG1400/wg1200-firmware/secrets/wg1200_secure_boot_key.pem"
        ),
        os.path.abspath(
            os.path.join(
                os.path.dirname(__file__), "secrets", "wg1200_secure_boot_key.pem"
            )
        ),
        os.path.abspath(
            os.path.join(os.path.dirname(__file__), "wg1200_secure_boot_key.pem")
        ),
    ]
    for c in candidates:
        if c and os.path.isfile(c):
            return c
    return None


def find_serial_port():
    ports = glob.glob("/dev/cu.usbserial*")
    if ports:
        return ports[0]
    return "/dev/cu.usbserial-2110"


def main():
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <upload_port> <upload_speed> <firmware_bin>")
        sys.exit(1)

    upload_port = sys.argv[1].strip("\"'")
    upload_speed = sys.argv[2].strip("\"'")
    firmware_bin = sys.argv[3].strip("\"'")

    # Fallback if port wasn't resolved by PlatformIO
    if not upload_port or upload_port.startswith("$"):
        upload_port = find_serial_port()

    if not upload_speed or upload_speed.startswith("$"):
        upload_speed = "460800"

    if not os.path.isfile(firmware_bin):
        print(f"Error: Firmware binary not found: {firmware_bin}")
        sys.exit(1)

    key_path = find_signing_key()
    if not key_path:
        print("\n[ERROR] WeatherXM WG1200 Secure Boot V2 key not found!")
        print(
            "Please set the WG1200_SECURE_BOOT_KEY environment variable or place key at:"
        )
        print(
            "  ~/Documents/mesh/WG1400/wg1200-firmware/secrets/wg1200_secure_boot_key.pem\n"
        )
        sys.exit(1)

    build_dir = os.path.dirname(firmware_bin)
    signed_bin = os.path.join(build_dir, "firmware_signed.bin")

    print("\n" + "=" * 60)
    print(" WeatherXM WG1200 Secure Boot V2 Signing & Flash")
    print(f" Signing Key : {key_path}")
    print(f" Source Bin  : {firmware_bin}")
    print(f" Signed Bin  : {signed_bin}")
    print(f" Port        : {upload_port}")
    print(f" Baud        : {upload_speed}")
    print(" Target Addr : 0x20000 (factory app partition)")
    print("=" * 60 + "\n")

    # Step 1: Sign data with Secure Boot V2
    sign_cmd = [
        sys.executable,
        "-m",
        "espsecure",
        "sign-data",
        "--version",
        "2",
        "--keyfile",
        key_path,
        "--output",
        signed_bin,
        firmware_bin,
    ]
    print("--> Signing binary with Secure Boot V2...")
    res = subprocess.run(sign_cmd)
    if res.returncode != 0:
        print("[ERROR] Signing failed!")
        sys.exit(res.returncode)

    # Step 2: Verify signature
    verify_cmd = [
        sys.executable,
        "-m",
        "espsecure",
        "verify-signature",
        "--version",
        "2",
        "--keyfile",
        key_path,
        signed_bin,
    ]
    res = subprocess.run(verify_cmd, capture_output=True, text=True)
    if "successful" not in res.stdout:
        print(f"[ERROR] Signature verification failed:\n{res.stdout}\n{res.stderr}")
        sys.exit(1)
    print("--> Signature verified successfully.")

    # Step 3: Flash to offset 0x20000
    print(f"\n--> Flashing {signed_bin} to offset 0x20000 on {upload_port}...")
    flash_cmd = [
        sys.executable,
        "-m",
        "esptool",
        "--chip",
        "esp32s3",
        "-b",
        upload_speed,
        "--port",
        upload_port,
        "--before",
        "default-reset",
        "--after",
        "hard-reset",
        "write_flash",
        "0x20000",
        signed_bin,
    ]
    res = subprocess.run(flash_cmd)
    if res.returncode != 0:
        print("[ERROR] Flashing failed!")
        sys.exit(res.returncode)

    print("\n[SUCCESS] WG1200 firmware flashed and verified successfully!\n")


if __name__ == "__main__":
    main()
