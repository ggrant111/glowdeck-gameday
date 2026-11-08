#!/usr/bin/env python3
"""Package GlowDeck CYD + Waveshare-S3 factory/OTA images for ESP Web Tools.

This deliberately generates a *factory* image at 0x0, and a separate app-only
OTA image. Factory flashing overwrites NVS regions even without a chip erase.
Run only after both PlatformIO environments have built successfully.

Example:
    python scripts/package_web_release.py --version v2026.11.08-beta.1
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
CONFIGS = (
    {"env": "cyd", "name": "glowdeck-gameday-cyd", "chip": "esp32",
     "family": "ESP32", "boot_offset": 0x1000, "flash_size": "4MB",
     "flash_freq": "40m"},
    {"env": "s3_waveshare_35", "name": "glowdeck-gameday-s3-waveshare-35",
     "chip": "esp32s3", "family": "ESP32-S3", "boot_offset": 0x0,
     "flash_size": "16MB", "flash_freq": "80m"},
)


def required(path: Path) -> Path:
    if not path.is_file() or path.stat().st_size == 0:
        raise RuntimeError("Missing or empty build component: " + str(path))
    return path


def boot_app0() -> Path:
    candidates = [
        ROOT / ".pio/build/cyd/boot_app0.bin",
        ROOT / ".pio/build/s3_waveshare_35/boot_app0.bin",
        Path.home() / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise RuntimeError(
        "Missing Arduino OTA boot_app0.bin (install espressif32 with PlatformIO)."
    )


def package(version: str, out: Path) -> None:
    if not re.fullmatch(r"v[0-9]{4}\.[0-9]{2}\.[0-9]{2}(?:-(?:alpha|beta|rc)\.[0-9]+)?", version):
        raise RuntimeError("Use a date version like v2026.11.08-beta.1")

    release = out / "release"
    site = out / "site"
    site_fw = site / "firmware"
    release.mkdir(parents=True, exist_ok=True)
    site_fw.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(required(ROOT / "web-installer/index.html"), site / "index.html")
    ota_boot = boot_app0()
    manifest = {
        "name": "GlowDeck GameDay",
        "version": version,
        "new_install_prompt_erase": True,
        "new_install_improv_wait_time": 0,
        "builds": [],
    }

    published = []
    for target in CONFIGS:
        build = ROOT / ".pio" / "build" / target["env"]
        app = required(build / "firmware.bin")
        part = required(build / "partitions.bin")
        boot = required(build / "bootloader.bin")
        ota_name = target["name"] + "-ota.bin"
        factory_name = target["name"] + "-factory.bin"
        factory = release / factory_name

        # Keep generated bootloader header safe for in-browser flash-mode use.
        command = [
            sys.executable, "-m", "esptool", "--chip", target["chip"],
            "merge_bin", "-o", str(factory),
            "--flash_mode", "dio", "--flash_freq", target["flash_freq"],
            "--flash_size", target["flash_size"],
            hex(target["boot_offset"]), str(boot),
            "0x8000", str(part),
            "0xe000", str(ota_boot),
            "0x10000", str(app),
        ]
        subprocess.run(command, cwd=ROOT, check=True)

        data = required(factory).read_bytes()
        if len(data) > (4 * 1024 * 1024 if target["family"] == "ESP32" else 16 * 1024 * 1024):
            raise RuntimeError("Factory image exceeds board flash capacity: " + factory_name)
        for addr, magic in (
            (target["boot_offset"], b"\xe9"),
            (0x8000, b"\xaa\x50"),
            (0x10000, b"\xe9"),
        ):
            if data[addr : addr + len(magic)] != magic:
                raise RuntimeError(f"{factory_name}: unexpected image bytes at {addr:#x}")
        if data[0x9000:0x9000 + 0x1000] != b"\xff" * 0x1000:
            raise RuntimeError("Unexpected nonblank NVS padding; refuse to publish")
        shutil.copyfile(app, release / ota_name)
        shutil.copyfile(factory, site_fw / factory_name)
        published.extend((factory_name, ota_name))
        manifest["builds"].append({
            "chipFamily": target["family"],
            "improv": False,
            "parts": [{"path": "firmware/" + factory_name, "offset": 0}],
        })
        print(f"{target['family']}: {factory_name} ({len(data)} bytes), OTA {app.stat().st_size} bytes")

    (site / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    # Optional installation package for mirrors/other HTTPS hosts. Do not treat
    # the ZIP or a firmware.bin file as the factory image itself.
    with zipfile.ZipFile(release / f"glowdeck-gameday-{version}-web-installer.zip", "w", zipfile.ZIP_DEFLATED) as zf:
        for path in sorted(site.rglob("*")):
            if path.is_file():
                zf.write(path, path.relative_to(site).as_posix())

    checksums = []
    for path in sorted(release.iterdir()):
        if path.is_file() and path.name != "SHA256SUMS.txt":
            checksums.append(hashlib.sha256(path.read_bytes()).hexdigest() + "  " + path.name)
    (release / "SHA256SUMS.txt").write_text("\n".join(checksums) + "\n", encoding="ascii")
    print("Manifest:\n" + json.dumps(manifest, indent=2))
    print("Generated release files: " + ", ".join(p.name for p in sorted(release.iterdir())))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--out", type=Path, default=ROOT / "dist/web-release")
    args = parser.parse_args()
    package(args.version, args.out)
