#!/usr/bin/env python3
"""Validate a completed browser-flasher release package before publication."""
import hashlib
import json
from pathlib import Path
import sys
import zipfile


def validate(root: Path):
    release, site = root / "release", root / "site"
    manifest = json.loads((site / "manifest.json").read_text(encoding="utf-8"))
    assert manifest["name"] == "GlowDeck GameDay"
    assert manifest["version"].startswith("v")
    assert manifest["new_install_prompt_erase"] is True
    assert manifest["new_install_improv_wait_time"] == 0
    builds = {b["chipFamily"]: b for b in manifest["builds"]}
    assert set(builds) == {"ESP32", "ESP32-S3"}, builds
    for family, boot_addr, size in (("ESP32", 0x1000, 4 * 1024**2),
                                   ("ESP32-S3", 0, 16 * 1024**2)):
        build = builds[family]
        assert build["improv"] is False
        assert len(build["parts"]) == 1
        part = build["parts"][0]
        assert part["offset"] == 0
        assert part["path"].startswith("firmware/")
        image_file = site / part["path"]
        payload = image_file.read_bytes()
        assert 0x10000 < len(payload) <= size
        assert payload[boot_addr] == 0xE9, (family, "bootloader")
        assert payload[0x8000:0x8002] == b"\xaa\x50", (family, "partition table")
        assert payload[0x10000] == 0xE9, (family, "app header")
        assert payload[0x9000:0xa000] == b"\xff" * 0x1000, (family, "NVS padding")
        assert (release / image_file.name).read_bytes() == payload
        ota_path = release / image_file.name.replace("-factory.bin", "-ota.bin")
        assert ota_path.read_bytes()[0] == 0xE9, (family, "OTA image")
        assert len(ota_path.read_bytes()) < len(payload)

    html = (site / "index.html").read_text(encoding="utf-8")
    for required in ("esp-web-install-button", 'manifest="./manifest.json"',
                     "factory", "OTA", "Chrome", "Waveshare", "3.5B"):
        assert required in html, required

    sums = (release / "SHA256SUMS.txt").read_text(encoding="ascii").splitlines()
    for entry in sums:
        digest, filename = entry.split("  ", 1)
        assert hashlib.sha256((release / filename).read_bytes()).hexdigest() == digest

    zipfile_path = release / f"glowdeck-gameday-{manifest['version']}-web-installer.zip"
    with zipfile.ZipFile(zipfile_path) as zf:
        assert "index.html" in zf.namelist()
        assert "manifest.json" in zf.namelist()
        for build in builds.values():
            assert build["parts"][0]["path"] in zf.namelist()
    print("PASS: factory chip layouts, OTA separation, manifests, Web Serial button, SHA256, ZIP")


if __name__ == "__main__":
    validate(Path(sys.argv[1] if len(sys.argv) > 1 else "dist/web-release"))
