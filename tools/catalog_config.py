"""Kairo64's catalog settings: C64 disk, tape, program and cartridge media, artwork references, the
platform's slugs and the model a game's name asks for."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "shared/tools"))
import content_ids
from launchbox_artwork import References

APP = "kairo64"
PLATFORMS = ("c64",)
ARTWORK_CODE = 64
ARTWORK = References("Kairo64", (ARTWORK_CODE,))

# The images VICE's autostart starts (MainActivity's DiskImageLibrary formats).
SUFFIXES = {".d64": "disk", ".g64": "disk", ".d71": "disk", ".d81": "disk",
            ".t64": "tape", ".tap": "tape", ".prg": "prg", ".p00": "prg", ".crt": "cart"}
LIMITS = {"disk": 2 * 1024**2, "tape": 64 * 1024**2, "prg": 64 * 1024, "cart": 16 * 1024**2}

KINDS = set(LIMITS)


def kind(name):
    return SUFFIXES.get(Path(name).suffix.lower())


MEDIA = content_ids.Media(kind, content_ids.same_file, LIMITS.__getitem__, 1, seven_zip=True)

MOBYGAMES = "c64"
# The standard sets: the No-Intro-named collection (loose disks and tapes) and the collection of
# ZIPs, each scanned into its own index; many games are in both.
SCANS = {"ghostware": (".downloads/scan/ghostware.json", ("c64",)),
         "collection": (".downloads/scan/collection.json", ("c64",))}

# The settings a game can ask for, as C64Catalog's options name them.
MACHINE_OPTIONS = {
    "model": {"c64", "c64c", "c64ntsc", "c64cntsc", "c64old", "c64oldntsc", "drean"},
    "system": {"openroms", "own"},
    "drive": {"virtual", "1541"},
    "ports": {"1", "2", "mouse"},
}

# No-Intro's regions of NTSC releases: their games are timed for a 60 Hz NTSC C64.
_NTSC = re.compile(r"\((?:USA|Japan|Canada|USA, Canada)\)")

# Games that need the player's firmware, the 1541 or another program than their media's first
# (tools/firmware_pass.py): {content ID: {"system": "own", "drive": "1541", "program": "NAME",
# "why": ...}}; a game listed as failing everywhere keeps the defaults.
_FIRMWARE = json.loads((ROOT / "catalog/research/firmware-v1.json").read_text(encoding="utf-8"))["games"]

# The launch fields a disk's record may carry (shared app_catalog): the program autostart loads
# and runs instead of the first (C64Catalog.program), by its name; no quote, which ends the name.
LAUNCH_FIELDS = {"program": r"[ !#-~]{1,16}"}


def MACHINE(name, set_name, content_id):
    """The settings a release asks for: an NTSC C64 for a USA or Japanese release, which is timed
    for one (every other release starts on the default PAL C64), and the firmware, drive and start
    program firmware-v1.json found it needs."""
    settings = {"model": "c64ntsc"} if _NTSC.search(name) else {}
    needs = _FIRMWARE.get(content_id, {})
    settings.update({option: needs[option] for option in ("system", "drive") if option in needs})
    if "program" in needs:
        settings["launch"] = {"program": needs["program"]}
    return settings


# Disk sets (shared/tools/disk_sets.py): a game starts from its first disk; the others are changed
# in drive 8 during play, so none is inserted beside it.
BOOT_KINDS = ["disk", "tape", "prg", "cart"]
COMPANION = {}
