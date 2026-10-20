"""Verify the SDK's debug PKG against its staged files (run inside build image)."""
from pathlib import Path
import hashlib
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / "JellyFin-PS3.pkg"
STAGING = ROOT / "obj/pkg"
CONTENT_ID = "UP0001-JFPS30000_00-0000000000000000"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


with tempfile.TemporaryDirectory() as scratch:
    subprocess.run(["pkg", "--extract", str(PACKAGE)], cwd=scratch, check=True)
    extracted = Path(scratch) / CONTENT_ID
    for relative in ("PARAM.SFO", "ICON0.PNG", "USRDIR/EBOOT.BIN"):
        expected = (STAGING / relative).read_bytes()
        actual = (extracted / relative).read_bytes()
        # The SDK records file sizes rounded up to 64 bytes, so its extractor
        # returns the file followed by up to 63 zero bytes of alignment padding.
        require(actual[:len(expected)] == expected, f"Payload mismatch: {relative}")
        padding = actual[len(expected):]
        require(len(padding) < 64 and not any(padding), f"Invalid padding: {relative}")

    sfo = (extracted / "PARAM.SFO").read_bytes()
    magic, _, key_start, data_start, count = struct.unpack_from("<4s4I", sfo)
    require(magic == b"\x00PSF", "Invalid SFO magic")
    values = {}
    for i in range(count):
        key_off, fmt, length, _, data_off = struct.unpack_from("<HHIII", sfo, 20 + 16 * i)
        key_pos = key_start + key_off
        key = sfo[key_pos:sfo.index(0, key_pos)].decode()
        if fmt == 0x204:
            pos = data_start + data_off
            values[key] = sfo[pos:pos + length].rstrip(b"\0").decode()
    for key, expected in {"CATEGORY": "AT", "TITLE": "JellyFin [PS3]",
                          "TITLE_ID": "JFPS30000", "APP_VER": "03.33"}.items():
        require(values.get(key) == expected, f"Unexpected {key}: {values.get(key)}")
    require((extracted / "USRDIR/EBOOT.BIN").read_bytes()[:4] == b"SCE\0",
            "Executable is not a SELF")
    themes = sorted((ROOT / "themes/jw-notes").glob("*.ini"))
    require(themes, "No themes in themes/jw-notes")
    for theme in themes:
        packed = extracted / "USRDIR/themes" / theme.name
        require(packed.exists(), f"Theme missing from package: {theme.name}")
        expected = theme.read_bytes()
        actual = packed.read_bytes()
        require(actual[:len(expected)] == expected and not any(actual[len(expected):]),
                f"Theme payload mismatch: {theme.name}")

for suffix in (".pkg", ".gnpdrm.pkg"):
    path = ROOT / ("JellyFin-PS3" + suffix)
    data = path.read_bytes()
    require(data[:4] == b"\x7fPKG", f"Invalid package header: {path.name}")
    require(struct.unpack_from(">Q", data, 0x18)[0] == len(data),
            f"Package size mismatch: {path.name}")
    finalized = bool(struct.unpack_from(">I", data, 4)[0] & 0x80000000)
    require(finalized == (suffix == ".gnpdrm.pkg"), f"Incorrect package type: {path.name}")
    require(data[0x30:0x60].rstrip(b"\0").decode() == CONTENT_ID,
            f"Incorrect content ID: {path.name}")
    print(f"{hashlib.sha256(data).hexdigest()}  {path.name}")
print("PKG payloads match staging; TV category, version, executable and themes verified.")
