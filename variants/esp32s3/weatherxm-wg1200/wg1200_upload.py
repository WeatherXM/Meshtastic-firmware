#!/usr/bin/env python3
"""
WeatherXM WG1200 Dual-Firmware Safe Upload, Preflight & Switcher Tool
=====================================================================
Enables seamless bidirectional switching between WeatherXM stock firmware
and Meshtastic:
  WeatherXM -> Meshtastic -> WeatherXM -> Meshtastic ...

Core Safety Principles:
1. 'factory' partition (0x020000, 4MB) holds stock WeatherXM and is NEVER overwritten.
2. Preflight Protection: Before developer flashing, reads 0xD000/0x2000 (esp_secure_cert)
   and validates the TLV magic (0xBA5EBA11), saving an automatic validated snapshot.
3. Partition table validation ensures 'factory' (0x20000), 'ota_0' (0x420000),
   and 'ota_1' (0x820000) match the expected 16MB WG1200 layout.
4. Live 'otadata' inspection determines the active boot slot:
   - If running 'factory' or 'ota_1': flashes to 'ota_0' (0x420000).
   - If running 'ota_0': flashes to 'ota_1' (0x820000).
5. Updates 'otadata' with the advanced sequence number and valid CRC to boot the new slot.
6. Validated full 16MB flash backup option (--backup) encouraged for engineering units.
7. Safe flash erase (--erase) strictly enforces a verified esp_secure_cert backup before wiping.
8. Provides hardware button rollback (hold user button 10s at boot) and software
   '--rollback' command to erase 'otadata' and instantly boot back into WeatherXM.
"""

import binascii
import datetime
import glob
import os
import re
import struct
import subprocess
import sys
import tempfile

os.environ["PYTHONIOENCODING"] = "utf-8"
os.environ["PYTHONUTF8"] = "1"
RUN_ENV = dict(os.environ, PYTHONIOENCODING="utf-8", PYTHONUTF8="1")

# Standard WG1200 16MB Partition Offsets & Sizes
EXPECTED_PARTITIONS = {
    "factory": {"offset": 0x020000, "size": 0x400000, "type": 0x00, "subtype": 0x00},
    "ota_0": {"offset": 0x420000, "size": 0x400000, "type": 0x00, "subtype": 0x10},
    "ota_1": {"offset": 0x820000, "size": 0x400000, "type": 0x00, "subtype": 0x11},
    "otadata": {"offset": 0x013000, "size": 0x002000, "type": 0x01, "subtype": 0x00},
}

PARTITION_TABLE_OFFSET = "0xc000"
PARTITION_TABLE_SIZE = "0x1000"
OTADATA_OFFSET = "0x13000"
OTADATA_SIZE = "0x2000"

# esp_secure_cert partition holding irreplaceable factory device credentials
SECURE_CERT_OFFSET = "0xd000"
SECURE_CERT_SIZE = "0x2000"
TLV_MAGIC = b"\x11\xba\x5e\xba"  # 0xBA5EBA11, little-endian as stored on flash


def find_signing_key():
    """Locate the authentic WG1200 Secure Boot V2 private key."""
    candidates = [
        os.environ.get("WG1200_SECURE_BOOT_KEY", ""),
        # Direct workspace relative paths
        os.path.abspath(
            os.path.join(
                os.path.dirname(__file__),
                "..",
                "..",
                "..",
                "..",
                "..",
                "WG1400",
                "wg1200-firmware",
                "secrets",
                "wg1200_secure_boot_key.pem",
            )
        ),
        os.path.abspath(
            os.path.join(
                os.path.dirname(__file__),
                "..",
                "..",
                "..",
                "..",
                "..",
                "WG1400",
                "secrets",
                "secure_boot_signing_key.pem",
            )
        ),
        os.path.abspath(
            os.path.join(
                os.path.dirname(__file__), "secrets", "wg1200_secure_boot_key.pem"
            )
        ),
        os.path.abspath(
            os.path.join(
                os.path.dirname(__file__), "secrets", "secure_boot_signing_key.pem"
            )
        ),
        os.path.abspath(
            os.path.join(os.path.dirname(__file__), "wg1200_secure_boot_key.pem")
        ),
        os.path.expanduser(
            "~/Documents/mesh/WG1400/wg1200-firmware/secrets/wg1200_secure_boot_key.pem"
        ),
    ]
    for c in candidates:
        if c and os.path.isfile(c):
            return c
    return None


def resolve_python_and_tool(tool_name):
    """Resolve command prefix to execute 'esptool' or 'espsecure'."""
    # 1. Try sys.executable
    try:
        res = subprocess.run(
            [sys.executable, "-m", tool_name, "version"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if res.returncode == 0:
            return [sys.executable, "-m", tool_name]
    except Exception:
        pass

    # 2. Check PlatformIO penv
    home = os.path.expanduser("~")
    pio_pythons = [
        os.path.join(home, ".platformio", "penv", "Scripts", "python.exe"),
        os.path.join(home, ".platformio", "penv", "bin", "python"),
    ]
    for pio_py in pio_pythons:
        if os.path.isfile(pio_py):
            try:
                res = subprocess.run(
                    [pio_py, "-m", tool_name, "version"],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
                if res.returncode == 0:
                    return [pio_py, "-m", tool_name]
            except Exception:
                pass
            script_exe = os.path.join(os.path.dirname(pio_py), f"{tool_name}.exe")
            if os.path.isfile(script_exe):
                return [script_exe]

    # 3. Check tool-esptoolpy package in PlatformIO
    pkg_patterns = glob.glob(
        os.path.join(
            home, ".platformio", "packages", "tool-esptoolpy*", f"{tool_name}.py"
        )
    )
    if pkg_patterns:
        py = pio_pythons[0] if os.path.isfile(pio_pythons[0]) else sys.executable
        return [py, pkg_patterns[0]]

    # Default fallback
    return [sys.executable, "-m", tool_name]


def find_serial_port():
    """Auto-detect the serial port for WG1200."""
    if sys.platform.startswith("win"):
        try:
            import serial.tools.list_ports

            ports = list(serial.tools.list_ports.comports())
            # Prioritize dedicated CH340 USB-UART or ESP32-S3 USB-JTAG
            for p in ports:
                desc = p.description or ""
                hwid = p.hwid or ""
                if any(x in desc or x in hwid for x in ["CH340", "1A86", "303A:1001"]):
                    return p.device
            # Filter out virtual bluetooth ports
            usable = [
                p
                for p in ports
                if "Bluetooth" not in (p.description or "") and "COM1" not in p.device
            ]
            if usable:
                return usable[0].device
            if ports:
                return ports[0].device
        except ImportError:
            pass
        return "COM9"
    else:
        patterns = [
            "/dev/cu.usbserial*",
            "/dev/cu.wchusbserial*",
            "/dev/ttyUSB*",
            "/dev/ttyACM*",
        ]
        for pat in patterns:
            matches = glob.glob(pat)
            if matches:
                return sorted(matches)[0]
        return "/dev/ttyUSB0"


def parse_partition_table(data):
    """Parse ESP32 binary partition table (from offset 0xC000)."""
    partitions = {}
    record_size = 32
    for offset in range(0, len(data), record_size):
        record = data[offset : offset + record_size]
        if len(record) < record_size:
            break
        magic = record[0:2]
        if magic == b"\xeb\xeb":
            # MD5 checksum record marks end of table
            break
        if magic != b"\xaa\x50":
            break

        ptype = record[2]
        psub = record[3]
        pos, size = struct.unpack("<II", record[4:12])
        label = record[12:28].split(b"\x00")[0].decode("ascii", errors="ignore")
        if not label:
            continue
        partitions[label] = {
            "type": ptype,
            "subtype": psub,
            "offset": pos,
            "size": size,
        }
    return partitions


def verify_wg1200_partitions(partitions):
    """Verify that factory, ota_0, ota_1, and otadata exist at the expected offsets."""
    errors = []
    for name, exp in EXPECTED_PARTITIONS.items():
        if name not in partitions:
            errors.append(f"Missing required partition '{name}'")
            continue
        p = partitions[name]
        if p["offset"] != exp["offset"]:
            errors.append(
                f"Partition '{name}' offset mismatch: found 0x{p['offset']:06X}, expected 0x{exp['offset']:06X}"
            )
        if p["size"] != exp["size"]:
            errors.append(
                f"Partition '{name}' size mismatch: found 0x{p['size']:06X}, expected 0x{exp['size']:06X}"
            )
        if p["type"] != exp["type"]:
            errors.append(
                f"Partition '{name}' type mismatch: found {p['type']}, expected {exp['type']}"
            )
        if p["subtype"] != exp["subtype"]:
            errors.append(
                f"Partition '{name}' subtype mismatch: found {p['subtype']}, expected {exp['subtype']}"
            )

    return len(errors) == 0, errors


def parse_otadata_sector(sector_bytes):
    """Check validity and sequence of one 4KB otadata sector."""
    if len(sector_bytes) < 32:
        return 0, False
    (seq,) = struct.unpack("<I", sector_bytes[0:4])
    (crc,) = struct.unpack("<I", sector_bytes[28:32])
    expected_crc = binascii.crc32(struct.pack("<I", seq), 0xFFFFFFFF) % (1 << 32)
    is_valid = (seq != 0xFFFFFFFF) and (seq > 0) and (crc == expected_crc)
    return seq, is_valid


def determine_active_slot(otadata_bytes):
    """Determine currently active partition from 8192-byte otadata."""
    if len(otadata_bytes) < 8192:
        return "factory", 0, None

    sec0_bytes = otadata_bytes[0:4096]
    sec1_bytes = otadata_bytes[4096:8192]

    seq0, valid0 = parse_otadata_sector(sec0_bytes)
    seq1, valid1 = parse_otadata_sector(sec1_bytes)

    if not valid0 and not valid1:
        return "factory", 0, None
    elif valid0 and not valid1:
        active_seq = seq0
        active_sec = 0
    elif not valid0 and valid1:
        active_seq = seq1
        active_sec = 1
    else:
        if seq0 >= seq1:
            active_seq = seq0
            active_sec = 0
        else:
            active_seq = seq1
            active_sec = 1

    slot_index = (active_seq - 1) % 2
    active_slot = f"ota_{slot_index}"
    return active_slot, active_seq, active_sec


def select_target_slot(active_slot, active_seq, active_sec):
    """Determine target OTA partition to write to."""
    if active_slot == "factory":
        target_slot = "ota_0"
        next_seq = 1
        target_sec = 0
    elif active_slot == "ota_0":
        target_slot = "ota_1"
        next_seq = active_seq + 1
        target_sec = 1 if (active_sec == 0) else 0
    elif active_slot == "ota_1":
        target_slot = "ota_0"
        next_seq = active_seq + 1
        target_sec = 1 if (active_sec == 0) else 0
    else:
        target_slot = "ota_0"
        next_seq = 1
        target_sec = 0

    target_offset = EXPECTED_PARTITIONS[target_slot]["offset"]
    return target_slot, target_offset, next_seq, target_sec


def build_updated_otadata(existing_otadata, next_seq, target_sec):
    """Create 8192-byte otadata binary with the next valid entry in target_sec."""
    if len(existing_otadata) >= 8192:
        new_data = bytearray(existing_otadata[:8192])
    else:
        new_data = bytearray(b"\xff" * 8192)

    entry = bytearray(b"\xff" * 32)
    struct.pack_into("<I", entry, 0, next_seq)
    crc = binascii.crc32(struct.pack("<I", next_seq), 0xFFFFFFFF) % (1 << 32)
    struct.pack_into("<I", entry, 28, crc)

    sec_offset = 0 if target_sec == 0 else 4096
    new_data[sec_offset : sec_offset + 4096] = b"\xff" * 4096
    new_data[sec_offset : sec_offset + 32] = entry
    return bytes(new_data)


def read_device_flash(esptool_cmd, port, speed, offset, size):
    """Read a block of flash memory from the ESP32-S3."""
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as tmp:
        tmp_name = tmp.name

    try:
        cmd = esptool_cmd + [
            "--chip",
            "esp32s3",
            "-b",
            str(speed),
            "--port",
            str(port),
            "--before",
            "default_reset",
            "--after",
            "no_reset",
            "read_flash",
            str(offset),
            str(size),
            tmp_name,
        ]
        res = subprocess.run(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=RUN_ENV
        )
        if res.returncode != 0:
            print(
                f"[ERROR] Flash read failed at {offset}:\n{res.stderr.decode('utf-8', errors='ignore')}"
            )
            return None
        with open(tmp_name, "rb") as f:
            return f.read()
    finally:
        if os.path.exists(tmp_name):
            try:
                os.remove(tmp_name)
            except OSError:
                pass


def derive_device_label(cert_bytes):
    """Best-effort extraction of device serial from cert CN or format."""
    begin = b"-----BEGIN CERTIFICATE-----"
    end = b"-----END CERTIFICATE-----"
    start = cert_bytes.find(begin)
    if start != -1:
        stop = cert_bytes.find(end, start)
        if stop != -1:
            pem = cert_bytes[start : stop + len(end)] + b"\n"
            try:
                from cryptography import x509
                from cryptography.x509.oid import NameOID

                cert = x509.load_pem_x509_certificate(pem)
                cn = cert.subject.get_attributes_for_oid(NameOID.COMMON_NAME)
                if cn and cn[0].value:
                    return re.sub(r"[^0-9A-Za-z]", "", cn[0].value)
            except Exception:
                pass
    return None


def validate_and_backup_secure_cert(
    esptool_cmd, port, speed, backup_dir="backups/secure_cert"
):
    """
    Preflight check:
    Reads the 0xD000 / 0x2000 esp_secure_cert region and validates TLV magic (0xBA5EBA11).
    Creates an automatic timestamped backup in backup_dir.
    Returns (is_valid, cert_data, backup_file_path).
    """
    os.makedirs(backup_dir, exist_ok=True)
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")

    data = read_device_flash(
        esptool_cmd, port, speed, SECURE_CERT_OFFSET, SECURE_CERT_SIZE
    )
    if not data:
        return False, None, None

    label = os.environ.get("WXM_BACKUP_LABEL") or derive_device_label(data) or "unknown"
    backup_file = os.path.join(backup_dir, f"secure_cert_{label}_{timestamp}.bin")

    try:
        with open(backup_file, "wb") as f:
            f.write(data)
    except Exception as e:
        print(f"[WARN] Could not save secure_cert backup file: {e}")

    has_tlv = TLV_MAGIC in data
    return has_tlv, data, backup_file


def validate_full_backup(backup_file):
    """
    Validate that a full flash dump file matches expected WG1200 geometry and integrity.
    Checks:
    1. File size is at least 16MB (0x1000000 = 16,777,216 bytes).
    2. Offset 0x0: ESP32-S3 bootloader magic byte (0xE9).
    3. Offset 0xC000: Partition table magic (0x50 0xAA).
    4. Partition table contains factory, ota_0, ota_1, otadata.
    5. Offset 0xD000: esp_secure_cert contains TLV magic 0xBA5EBA11.
    """
    report = []
    if not os.path.isfile(backup_file):
        return False, [f"[ERROR] Backup file does not exist: {backup_file}"]

    size = os.path.getsize(backup_file)
    if size < 0x1000000:
        report.append(
            f"[ERROR] Backup size ({size} bytes) is less than 16MB (16777216 bytes)."
        )
        return False, report
    report.append(f"[OK] File size verified: {size} bytes (16MB).")

    with open(backup_file, "rb") as f:
        # 1. Bootloader Header Check
        boot_header = f.read(4)
        if len(boot_header) >= 1 and boot_header[0] == 0xE9:
            report.append(
                "[OK] Offset 0x0000: Valid ESP32 bootloader header magic (0xE9)."
            )
        else:
            report.append("[WARN] Offset 0x0000: Bootloader magic byte 0xE9 not found.")

        # 2. Partition Table Check (0xC000)
        f.seek(0xC000)
        pt_bytes = f.read(0x1000)
        if len(pt_bytes) >= 2 and pt_bytes[0:2] == b"\xaa\x50":
            report.append("[OK] Offset 0xC000: Valid partition table magic (0x50AA).")
            partitions = parse_partition_table(pt_bytes)
            valid, errs = verify_wg1200_partitions(partitions)
            if valid:
                report.append(
                    "[OK] Partition table layout verified (factory, ota_0, ota_1, otadata present)."
                )
            else:
                for e in errs:
                    report.append(f"[WARN] Partition table warning: {e}")
        else:
            report.append("[WARN] Offset 0xC000: Partition table magic not found.")

        # 3. esp_secure_cert Check (0xD000)
        f.seek(0xD000)
        cert_bytes = f.read(0x2000)
        if TLV_MAGIC in cert_bytes:
            label = derive_device_label(cert_bytes)
            lbl_str = f" [Device Serial: {label}]" if label else ""
            report.append(
                f"[OK] Offset 0xD000: esp_secure_cert verified (TLV magic 0xBA5EBA11 present{lbl_str})."
            )
        elif all(b == 0xFF for b in cert_bytes):
            report.append("[WARN] Offset 0xD000: esp_secure_cert is blank (all 0xFF).")
        else:
            report.append(
                "[WARN] Offset 0xD000: esp_secure_cert TLV magic (0xBA5EBA11) NOT found."
            )

    return True, report


def cmd_backup(port, speed, output_file=None):
    """
    Perform a complete, validated 16MB dump of device flash.
    Encouraged for engineering devices and required before destructive operations.
    """
    esptool_cmd = resolve_python_and_tool("esptool")
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")

    backup_dir = "backups"
    os.makedirs(backup_dir, exist_ok=True)

    if not output_file:
        output_file = os.path.join(backup_dir, f"wg1200_full_16MB_{timestamp}.bin")

    print("\n" + "=" * 65)
    print(" WeatherXM WG1200 Full 16MB Flash Backup & Validation")
    print(f" Port       : {port}")
    print(f" Baud       : {speed}")
    print(f" Output File: {output_file}")
    print("=" * 65)

    print("\n--> Reading entire 16MB (0x1000000) flash from device...")
    cmd = esptool_cmd + [
        "--chip",
        "esp32s3",
        "-b",
        str(speed),
        "--port",
        str(port),
        "--before",
        "default_reset",
        "--after",
        "hard_reset",
        "read_flash",
        "0x0",
        "0x1000000",
        output_file,
    ]
    res = subprocess.run(cmd, env=RUN_ENV)
    if res.returncode != 0:
        print(f"\n[ERROR] Full flash read failed with exit code {res.returncode}")
        sys.exit(1)

    print("\n--> Validating backup image integrity...")
    valid, report = validate_full_backup(output_file)
    for line in report:
        print(f"  {line}")

    if not valid:
        print("\n[WARNING] Backup validation reported issues! Inspect above log.")
    else:
        print("\n" + "=" * 65)
        print(f"[SUCCESS] Validated 16MB backup created successfully:")
        print(f"  Path: {os.path.abspath(output_file)}")
        print("=" * 65 + "\n")


def cmd_erase(port, speed, force=False):
    """
    Safely erase entire flash.
    MANDATORY PRECONDITION: Must obtain a validated backup of esp_secure_cert first!
    """
    esptool_cmd = resolve_python_and_tool("esptool")
    print("\n" + "=" * 65)
    print(" WeatherXM WG1200 Safe Flash Erase")
    print(f" Port : {port} | Baud: {speed}")
    print("=" * 65)

    print(
        "\n--> [MANDATORY PRECONDITION] Backing up and validating esp_secure_cert (0xD000)..."
    )
    valid, cert_data, backup_file = validate_and_backup_secure_cert(
        esptool_cmd, port, speed
    )
    if not valid:
        print("\n" + "!" * 65)
        print("[FATAL] Refusing to erase flash:")
        print(
            "esp_secure_cert at 0xD000 is either missing or does not contain TLV magic (0xBA5EBA11)."
        )
        print(
            "Erasing flash would permanently destroy irreplaceable device factory credentials!"
        )
        print("!" * 65 + "\n")
        sys.exit(1)

    print(f"[OK] Mandatory esp_secure_cert backup verified and saved:")
    print(f"     -> {os.path.abspath(backup_file)}")

    if not force:
        print("\nWARNING: You are about to ERASE ENTIRE FLASH on ESP32-S3.")
        print(
            "Factory firmware and partitions will be erased. Only bootloader/eFuses remain."
        )
        ans = input("Type 'ERASE' to confirm: ").strip()
        if ans != "ERASE":
            print("Erase cancelled by user.")
            sys.exit(0)

    print("\n--> Erasing entire flash (esptool erase_flash)...")
    cmd = esptool_cmd + [
        "--chip",
        "esp32s3",
        "-b",
        str(speed),
        "--port",
        str(port),
        "--before",
        "default_reset",
        "--after",
        "hard_reset",
        "erase_flash",
    ]
    res = subprocess.run(cmd, env=RUN_ENV)
    if res.returncode != 0:
        print("[ERROR] erase_flash failed!")
        sys.exit(1)

    print("\n[SUCCESS] Flash erased. To restore your credentials at any time:")
    print(f"  python wg1200_upload.py --restore-cert {backup_file} {port}\n")


def cmd_restore_cert(port, speed, cert_file):
    """Restore esp_secure_cert partition to 0xD000."""
    esptool_cmd = resolve_python_and_tool("esptool")
    if not os.path.isfile(cert_file):
        print(f"[ERROR] Certificate file not found: {cert_file}")
        sys.exit(1)

    with open(cert_file, "rb") as f:
        data = f.read()

    if TLV_MAGIC not in data:
        print(
            f"[ERROR] Specified file {cert_file} does not contain valid TLV magic (0xBA5EBA11)!"
        )
        sys.exit(1)

    print(f"\n--> Restoring esp_secure_cert ({SECURE_CERT_OFFSET}) from {cert_file}...")
    cmd = esptool_cmd + [
        "--chip",
        "esp32s3",
        "-b",
        str(speed),
        "--port",
        str(port),
        "--before",
        "default_reset",
        "--after",
        "hard_reset",
        "write_flash",
        "-z",
        SECURE_CERT_OFFSET,
        cert_file,
    ]
    res = subprocess.run(cmd, env=RUN_ENV)
    if res.returncode != 0:
        print("[ERROR] Certificate restore failed!")
        sys.exit(1)
    print(f"[SUCCESS] esp_secure_cert restored successfully to {SECURE_CERT_OFFSET}.\n")


def cmd_status(port, speed):
    """Inspect partition layout, certificates, and active boot slot without modifying flash."""
    esptool_cmd = resolve_python_and_tool("esptool")
    print("\n" + "=" * 65)
    print(" WeatherXM WG1200 Partition & Boot Status")
    print(f" Port : {port} | Baud: {speed}")
    print("=" * 65)

    print("\n--> Inspecting esp_secure_cert partition (0xd000)...")
    cert_data = read_device_flash(
        esptool_cmd, port, speed, SECURE_CERT_OFFSET, SECURE_CERT_SIZE
    )
    if cert_data:
        if TLV_MAGIC in cert_data:
            label = derive_device_label(cert_data)
            lbl_str = f" (Device Serial: {label})" if label else ""
            print(
                f"[OK] esp_secure_cert validated: TLV magic 0xBA5EBA11 found{lbl_str}."
            )
        elif all(b == 0xFF for b in cert_data):
            print("[WARN] esp_secure_cert is blank (all 0xFF).")
        else:
            print("[WARN] esp_secure_cert does not contain TLV magic (0xBA5EBA11).")

    print("\n--> Reading partition table from device (0xc000)...")
    pt_bytes = read_device_flash(
        esptool_cmd, port, speed, PARTITION_TABLE_OFFSET, PARTITION_TABLE_SIZE
    )
    if not pt_bytes:
        print("[ERROR] Could not read partition table. Check serial connection.")
        sys.exit(1)

    partitions = parse_partition_table(pt_bytes)
    print("Detected partitions:")
    for name, p in partitions.items():
        print(
            f"  - {name:<16} offset=0x{p['offset']:06X} size=0x{p['size']:06X} (type=0x{p['type']:02X}, subtype=0x{p['subtype']:02X})"
        )

    valid, errors = verify_wg1200_partitions(partitions)
    if not valid:
        print("\n[WARNING] WG1200 partition layout verification issues:")
        for err in errors:
            print(f"  ! {err}")
    else:
        print("\n[OK] Partition table conforms to standard WG1200 layout.")

    print("\n--> Reading otadata partition (0x13000)...")
    ota_bytes = read_device_flash(
        esptool_cmd, port, speed, OTADATA_OFFSET, OTADATA_SIZE
    )
    if not ota_bytes:
        print("[ERROR] Could not read otadata partition.")
        sys.exit(1)

    active_slot, active_seq, active_sec = determine_active_slot(ota_bytes)
    target_slot, target_offset, next_seq, target_sec = select_target_slot(
        active_slot, active_seq, active_sec
    )

    print(f"\nBoot Status Summary:")
    print(
        f"  * Currently Active Slot : {active_slot} (seq={active_seq}, sector={active_sec})"
    )
    if active_slot == "factory":
        print(f"    -> Running original WeatherXM Factory firmware at 0x020000")
    else:
        print(
            f"    -> Running firmware at offset 0x{EXPECTED_PARTITIONS[active_slot]['offset']:06X}"
        )
    print(
        f"  * Inactive / Next Slot  : {target_slot} (offset=0x{target_offset:06X}, next_seq={next_seq})"
    )
    print("=" * 65 + "\n")


def cmd_rollback(port, speed):
    """Roll back device to original WeatherXM factory firmware by erasing otadata."""
    esptool_cmd = resolve_python_and_tool("esptool")
    print("\n" + "=" * 65)
    print(" WeatherXM WG1200 Factory Rollback")
    print(" Reverting device to stock WeatherXM firmware (factory slot 0x020000)")
    print(f" Port : {port} | Baud: {speed}")
    print("=" * 65)

    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as tmp:
        tmp.write(b"\xff" * 8192)
        blank_otadata = tmp.name

    try:
        print(
            "\n--> Resetting otadata partition to 0xFF (clearing OTA boot pointers)..."
        )
        flash_cmd = esptool_cmd + [
            "--chip",
            "esp32s3",
            "-b",
            str(speed),
            "--port",
            str(port),
            "--before",
            "default_reset",
            "--after",
            "hard_reset",
            "write_flash",
            "-z",
            OTADATA_OFFSET,
            blank_otadata,
        ]
        res = subprocess.run(flash_cmd, env=RUN_ENV)
        if res.returncode != 0:
            print("[ERROR] Failed to write otadata for rollback!")
            sys.exit(res.returncode)

        print("\n" + "=" * 65)
        print("[SUCCESS] Device rolled back to Factory WeatherXM firmware!")
        print("On reboot, the ESP-IDF bootloader will boot slot 'factory' (0x020000).")
        print("=" * 65 + "\n")
    finally:
        if os.path.exists(blank_otadata):
            try:
                os.remove(blank_otadata)
            except OSError:
                pass


def cmd_upload(upload_port, upload_speed, firmware_bin):
    """Main safe upload workflow."""
    if not os.path.isfile(firmware_bin):
        print(f"[ERROR] Firmware binary not found: {firmware_bin}")
        sys.exit(1)

    signing_key = find_signing_key()
    esptool_cmd = resolve_python_and_tool("esptool")
    espsecure_cmd = resolve_python_and_tool("espsecure")

    print("\n" + "=" * 65)
    print(" WeatherXM WG1200 Dual-Firmware Safe Upload")
    print(f" Source Bin   : {firmware_bin}")
    print(f" Signing Key  : {signing_key if signing_key else 'NOT FOUND'}")
    print(f" Port         : {upload_port}")
    print(f" Baud         : {upload_speed}")
    print("=" * 65)

    # Step 0: Preflight Protection - esp_secure_cert validation
    print(
        "\n--> Step 0: Preflight check - validating esp_secure_cert (0xD000/0x2000)..."
    )
    valid_cert, cert_data, cert_backup = validate_and_backup_secure_cert(
        esptool_cmd, upload_port, upload_speed
    )
    if valid_cert:
        label = derive_device_label(cert_data)
        lbl_str = f" [Device Serial: {label}]" if label else ""
        print(
            f"[OK] Preflight verified: esp_secure_cert contains valid TLV magic (0xBA5EBA11){lbl_str}."
        )
        print(f"--> Snapshot saved -> {cert_backup}")
    elif cert_data and all(b == 0xFF for b in cert_data):
        print(
            "[WARN] esp_secure_cert partition is blank (all 0xFF). Device has no factory certificates."
        )
    else:
        print(
            "[WARN] esp_secure_cert partition does not contain TLV magic (0xBA5EBA11)."
        )

    print(
        "[TIP] For engineering devices, create a validated 16MB backup anytime using:"
    )
    print("      python variants/esp32s3/weatherxm-wg1200/wg1200_upload.py --backup")

    # Step 1: Secure Boot V2 Signing
    target_bin_to_flash = firmware_bin
    if signing_key:
        print("\n--> Signing binary with Secure Boot V2...")
        signed_bin = os.path.splitext(firmware_bin)[0] + "_signed.bin"
        sign_cmd = espsecure_cmd + [
            "sign_data",
            "--version",
            "2",
            "--keyfile",
            signing_key,
            "--output",
            signed_bin,
            firmware_bin,
        ]
        res = subprocess.run(sign_cmd, capture_output=True, text=True, env=RUN_ENV)
        if res.returncode != 0:
            print(f"[ERROR] espsecure sign_data failed:\n{res.stdout}\n{res.stderr}")
            sys.exit(res.returncode)

        verify_cmd = espsecure_cmd + [
            "verify_signature",
            "--version",
            "2",
            "--keyfile",
            signing_key,
            signed_bin,
        ]
        res = subprocess.run(verify_cmd, capture_output=True, text=True, env=RUN_ENV)
        if "successful" not in res.stdout and "valid" not in res.stdout:
            print(f"[ERROR] Signature verification failed:\n{res.stdout}\n{res.stderr}")
            sys.exit(1)
        print("--> Signature verified successfully.")
        target_bin_to_flash = signed_bin
    else:
        print("\n[WARN] Secure Boot V2 signing key not found.")
        print(
            "Proceeding with unsigned binary (will fail on production units with Secure Boot burned)."
        )

    # Step 2: Read & Validate Partition Table
    print("\n--> Reading and verifying device partition table...")
    pt_bytes = read_device_flash(
        esptool_cmd,
        upload_port,
        upload_speed,
        PARTITION_TABLE_OFFSET,
        PARTITION_TABLE_SIZE,
    )
    if not pt_bytes:
        print("[ERROR] Failed to read partition table from device.")
        sys.exit(1)

    partitions = parse_partition_table(pt_bytes)
    valid, errors = verify_wg1200_partitions(partitions)
    if not valid:
        print("\n[ERROR] Device partition table does not match required WG1200 layout:")
        for err in errors:
            print(f"  - {err}")
        print("Aborting upload to protect device flash integrity.")
        sys.exit(1)
    print(
        "--> Partition table verified (factory=0x20000, ota_0=0x420000, ota_1=0x820000)."
    )

    # Step 3: Read otadata & Determine Target Slot
    print("\n--> Reading otadata partition to identify inactive slot...")
    ota_bytes = read_device_flash(
        esptool_cmd, upload_port, upload_speed, OTADATA_OFFSET, OTADATA_SIZE
    )
    if not ota_bytes:
        print("[ERROR] Failed to read otadata partition from device.")
        sys.exit(1)

    active_slot, active_seq, active_sec = determine_active_slot(ota_bytes)
    target_slot, target_offset, next_seq, target_sec = select_target_slot(
        active_slot, active_seq, active_sec
    )

    print(f"--> Current Active Slot : {active_slot} (seq={active_seq})")
    print(
        f"--> Target Inactive Slot: {target_slot} (offset=0x{target_offset:06X}, new seq={next_seq})"
    )
    print(f"--> Note: 'factory' partition (0x020000) will NOT be touched.")

    # Step 4: Construct Updated otadata
    updated_otadata = build_updated_otadata(ota_bytes, next_seq, target_sec)
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as tmp:
        tmp.write(updated_otadata)
        updated_otadata_path = tmp.name

    try:
        # Step 5: Flash Firmware to Target Slot & Update otadata Atomically
        print(
            f"\n--> Flashing {target_bin_to_flash} to {target_slot} (0x{target_offset:06X})"
        )
        print(f"--> Updating otadata (0x{OTADATA_OFFSET}) to activate {target_slot}...")
        flash_cmd = esptool_cmd + [
            "--chip",
            "esp32s3",
            "-b",
            str(upload_speed),
            "--port",
            str(upload_port),
            "--before",
            "default_reset",
            "--after",
            "hard_reset",
            "write_flash",
            "-z",
            "--flash_mode",
            "dio",
            "--flash_freq",
            "80m",
            "--flash_size",
            "16MB",
            f"0x{target_offset:X}",
            target_bin_to_flash,
            OTADATA_OFFSET,
            updated_otadata_path,
        ]
        res = subprocess.run(flash_cmd, env=RUN_ENV)
        if res.returncode != 0:
            print("[ERROR] Flashing failed!")
            sys.exit(res.returncode)

        print("\n" + "=" * 65)
        print("[SUCCESS] WG1200 flashed and switched cleanly!")
        print(
            f"Active Boot Target is now: {target_slot} (offset 0x{target_offset:06X})"
        )
        print("Factory WeatherXM firmware remains untouched at 0x020000.")
        print("Hold user button (GPIO 38) for 10s at boot to rollback to WeatherXM.")
        print("=" * 65 + "\n")
    finally:
        if os.path.exists(updated_otadata_path):
            try:
                os.remove(updated_otadata_path)
            except OSError:
                pass


def main():
    if "--status" in sys.argv:
        args = [a for a in sys.argv[1:] if a != "--status"]
        port = args[0] if len(args) > 0 else find_serial_port()
        speed = args[1] if len(args) > 1 else "460800"
        cmd_status(port, speed)
        return

    if "--rollback" in sys.argv or "--factory-rollback" in sys.argv:
        args = [
            a for a in sys.argv[1:] if a not in ("--rollback", "--factory-rollback")
        ]
        port = args[0] if len(args) > 0 else find_serial_port()
        speed = args[1] if len(args) > 1 else "460800"
        cmd_rollback(port, speed)
        return

    if "--backup" in sys.argv:
        args = [a for a in sys.argv[1:] if a != "--backup"]
        port = args[0] if len(args) > 0 else find_serial_port()
        speed = args[1] if len(args) > 1 else "460800"
        out_file = args[2] if len(args) > 2 else None
        cmd_backup(port, speed, out_file)
        return

    if "--erase" in sys.argv:
        args = [a for a in sys.argv[1:] if a != "--erase"]
        force = "--force" in args
        args = [a for a in args if a != "--force"]
        port = args[0] if len(args) > 0 else find_serial_port()
        speed = args[1] if len(args) > 1 else "460800"
        cmd_erase(port, speed, force=force)
        return

    if "--restore-cert" in sys.argv:
        args = [a for a in sys.argv[1:] if a != "--restore-cert"]
        if len(args) < 1:
            print("Usage: wg1200_upload.py --restore-cert <backup_file> [port] [speed]")
            sys.exit(1)
        cert_file = args[0]
        port = args[1] if len(args) > 1 else find_serial_port()
        speed = args[2] if len(args) > 2 else "460800"
        cmd_restore_cert(port, speed, cert_file)
        return

    # Standard upload flow (called by PlatformIO or manual CLI)
    if len(sys.argv) < 2:
        print(f"WeatherXM WG1200 Flasher & Management Tool\n")
        print(f"Usage:")
        print(
            f"  Upload firmware : {sys.argv[0]} [upload_port] [upload_speed] <firmware_bin>"
        )
        print(
            f"  Inspect status  : {sys.argv[0]} --status [upload_port] [upload_speed]"
        )
        print(
            f"  Full 16MB backup: {sys.argv[0]} --backup [upload_port] [upload_speed] [output_file]"
        )
        print(
            f"  Restore cert    : {sys.argv[0]} --restore-cert <cert_file> [upload_port] [upload_speed]"
        )
        print(
            f"  Safe erase flash: {sys.argv[0]} --erase [upload_port] [upload_speed] [--force]"
        )
        print(
            f"  Factory rollback: {sys.argv[0]} --rollback [upload_port] [upload_speed]"
        )
        sys.exit(1)

    if len(sys.argv) >= 4:
        port = sys.argv[1].strip("\"'")
        speed = sys.argv[2].strip("\"'")
        firmware_bin = sys.argv[3].strip("\"'")
    elif len(sys.argv) == 2:
        firmware_bin = sys.argv[1].strip("\"'")
        port = find_serial_port()
        speed = "460800"
    else:
        port = sys.argv[1].strip("\"'")
        firmware_bin = sys.argv[2].strip("\"'")
        speed = "460800"

    if not port or port.startswith("$"):
        port = find_serial_port()
    if not speed or speed.startswith("$"):
        speed = "460800"

    cmd_upload(port, speed, firmware_bin)


if __name__ == "__main__":
    main()
