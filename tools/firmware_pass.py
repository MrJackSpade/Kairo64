#!/usr/bin/env python3
"""Find the firmware each library game needs: the least of the player's firmware it runs on.

Usage: firmware_pass.py <seconds> [--workers N] [--scanned]

Boots every game the library lists (each game's set entries and lone images) on the host with the
core's boot_test (.build/host/boot_test.exe, under shared/tools/run-limited.ps1) for <seconds> of
guest time, on the model its catalog record asks for, in order of how much of the player's
firmware a setup needs, until one runs it:

  open        the Open ROMs the app ships and VICE's virtual drive: no firmware
  open+1541   the Open ROMs and an emulated 1541: the 1541 drive ROM
  own         the player's KERNAL, BASIC and character ROMs and the virtual drive
  own+1541    the player's ROMs and an emulated 1541

A boot fails when the machine stops (a JAM), the load has not finished, or the text screen shows a
BASIC error ("?FILE NOT FOUND ERROR", "?LOAD ERROR" and the rest) or BASIC's READY. with no sound
since the load. catalog/research/firmware-v1.json records the first setup that runs a game that
needs more than the first, so tools/catalog_config.MACHINE gives it "system": "own" and/or
"drive": "1541"; a game none runs is recorded as failing, for review. Entries already decided
there are kept as they are.

Results are kept per game in .downloads/firmware-pass/results.json, so a stopped run carries on,
and the last frame of every game that needs firmware or fails everywhere is kept beside it for
review. The firmware is bios/ (kernal, basic, chargen and dos1541). --scanned boots every medium
the scans found instead, on the model its name asks for, for a pass that starts before the
research does; the list then takes the results of the library's entries.
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
import threading
import zipfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import catalog_config

OUT = ROOT / ".downloads/firmware-pass"
LIST = ROOT / "catalog/research/firmware-v1.json"
SYSTEMS = ["open", "open+1541", "own", "own+1541"]
# What a setup asks of the game's settings (C64Catalog's system and drive options).
SETTINGS = {"open": {}, "open+1541": {"drive": "1541"}, "own": {"system": "own"},
            "own+1541": {"system": "own", "drive": "1541"}}
BASIC_ERROR = re.compile(r"\?[A-Z ]+ ERROR")
# How far the sound must move after the load for a game sitting at READY. to count as running.
SOUND_RANGE = 2000


def scanned_media():
    """{content ID: (scan root, archive path, member or None)} for every medium the scans found."""
    where = {}
    for set_name, (index_path, _) in catalog_config.SCANS.items():
        index = json.loads((ROOT / index_path).read_text(encoding="utf-8"))
        for archive in index["archives"]:
            if archive["status"] != "ready":
                continue
            for medium in archive["disks"]:
                where.setdefault(medium["contentId"], (Path(index["root"]), archive["path"], medium["entry"], set_name))
    return where


def all_scanned_media():
    """{content ID: (scan root, archive path, member or None, model)} for every medium the scans
    found, each on the model its name asks for (catalog_config.MACHINE): for a pass started before
    the catalog's research, whose results the library's entries then reuse."""
    return {content_id: (root, archive, member,
                         catalog_config.MACHINE(member or archive, set_name, content_id).get("model", "c64"))
            for content_id, (root, archive, member, set_name) in scanned_media().items()}


def library_media():
    """{content ID: (scan root, archive path, member or None, model)} for every library entry."""
    research = json.loads((ROOT / "catalog/research/games-v1.json").read_text(encoding="utf-8"))
    where = {content_id: fields[:3] for content_id, fields in scanned_media().items()}
    media = {}
    for game in research["games"].values():
        if game.get("hidden") or not game.get("contentIds"):
            continue
        behind = {disk for fields in game.get("diskSets", {}).values() for disk in fields["setDisks"]}
        behind |= set(game.get("diskHidden", []))
        for content_id in game["contentIds"]:
            if content_id in behind:
                continue
            model = game.get("diskMachine", {}).get(content_id, {}).get("model", "c64")
            media[content_id] = (*where[content_id], model)
    return media


def extract(root, archive, member, work):
    """The medium as work/game.<ext>: a short path, out of its archive if it is in one."""
    source = root / archive
    name = member or archive
    target = work / ("game" + Path(name).suffix.lower())
    if member is None:
        shutil.copyfile(source, target)
    elif source.suffix.lower() == ".zip":
        with zipfile.ZipFile(source) as archive_file, archive_file.open(member) as stream, target.open("wb") as out:
            shutil.copyfileobj(stream, out)
    else:
        raise RuntimeError(f"cannot extract {member} from {source}: only ZIP archives are read")
    return target


def boot(system, path, model, seconds, work):
    """The outcome of one boot: {"ok": bool, "why": text, "range": int}."""
    for old in list(work.glob("*.bmp")) + list(work.glob("*.txt")):
        old.unlink()
    result = subprocess.run(["pwsh", "-NoProfile", "-File", str(ROOT / "shared/tools/run-limited.ps1"),
                             str(ROOT / ".build/host/boot_test.exe"), str(ROOT / "bios"), str(work), str(path),
                             str(seconds), system, "1", "", model],
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    lines = result.stdout.splitlines() + result.stderr.splitlines()
    for line in lines:
        if line.startswith("run 0: could not start") or line.startswith("run 0: stopped after"):
            return {"ok": False, "why": line.split(": ", 2)[-1], "range": 0}
    summary = next((line for line in lines if line.startswith("run 0:") and " range after load " in line), None)
    if summary is None:
        crash = next((line for line in lines if line.startswith("exit -")), None)
        if crash:
            return {"ok": False, "why": f"the core crashed ({crash})", "range": 0}
        raise RuntimeError(f"boot_test gave no result for {path} on {system}:\n" + "\n".join(lines[-20:]))
    played = int(summary.split(" range after load ")[1].split(",")[0])
    if summary.endswith("(still loading)"):
        return {"ok": False, "why": f"still loading after {seconds} s", "range": played}
    screen = (work / "screen-0.txt").read_text(encoding="ascii")
    error = BASIC_ERROR.search(screen)
    if error:
        return {"ok": False, "why": f"BASIC says {error.group(0)}", "range": played}
    if "READY." in screen and played < SOUND_RANGE:
        return {"ok": False, "why": "back at BASIC's READY. without a sound", "range": played}
    return {"ok": True, "why": "runs", "range": played}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("seconds", type=int)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--scanned", action="store_true",
                        help="boot every medium the scans found, not only the library's entries")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    results_path = OUT / "results.json"
    results = json.loads(results_path.read_text(encoding="utf-8")) if results_path.is_file() else {}
    lock = threading.Lock()
    media = all_scanned_media() if args.scanned else library_media()
    todo = [(content_id, *fields) for content_id, fields in sorted(media.items()) if content_id not in results]
    free = [OUT / f"work-{slot}" for slot in range(args.workers)]

    def run(item):
        content_id, root, archive, member, model = item
        with lock:
            work = free.pop()
        try:
            if work.exists():
                shutil.rmtree(work)
            work.mkdir(parents=True)
            path = extract(root, archive, member, work)
            record = {"model": model, "boots": {}}
            for system in SYSTEMS:
                outcome = boot(system, path, model, args.seconds, work)
                record["boots"][system] = outcome
                if outcome["ok"]:
                    break
            if not record["boots"]["open"]["ok"]:
                (work / "frame-0.bmp").replace(OUT / f"{content_id.split(':')[1]}.bmp")
        finally:
            with lock:
                free.append(work)
        with lock:
            results[content_id] = record
            if len(results) % 25 == 0:
                results_path.write_text(json.dumps(results, indent=1, sort_keys=True) + "\n", encoding="utf-8")
                print(f"{len(results)} games booted", flush=True)

    print(f"{len(media)} library games; {len(todo)} to boot", flush=True)
    with ThreadPoolExecutor(args.workers) as pool:
        for _ in pool.map(run, todo):
            pass
    results_path.write_text(json.dumps(results, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    if args.scanned:
        # Disks behind a set entry (a second side, a save disk) are not games of their own; only
        # a pass over the library's entries decides the list.
        print(f"{len(results)} media booted; run without --scanned after the research to list the games")
        return

    listed = json.loads(LIST.read_text(encoding="utf-8")) if LIST.is_file() else {"games": {}, "schemaVersion": 1}
    counts = {}
    for content_id, record in sorted((key, value) for key, value in results.items() if key in media):
        boots = record["boots"]
        runs_on = next((system for system in SYSTEMS if boots.get(system, {}).get("ok")), None)
        counts[runs_on or "none"] = counts.get(runs_on or "none", 0) + 1
        if content_id in listed["games"] or runs_on == "open":
            continue
        failure = boots["open"]["why"]
        if runs_on is None:
            listed["games"][content_id] = {"fails": f"firmware_pass.py: fails on every setup; on the Open ROMs: {failure}"}
        else:
            listed["games"][content_id] = dict(SETTINGS[runs_on], why=(
                f"firmware_pass.py: on the Open ROMs and the virtual drive it fails ({failure}); runs on {runs_on}"))
    listed["games"] = dict(sorted(listed["games"].items()))
    LIST.write_text(json.dumps(listed, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    print(f"{len(results)} games booted; runs on: {counts}; {len(listed['games'])} listed")


if __name__ == "__main__":
    main()
