#!/usr/bin/env python3
"""Find the firmware each library game needs: the least of the player's firmware it runs on.

Usage: firmware_pass.py <seconds> [--tape-seconds N] [--workers N] [--scanned]

Boots every game the library lists (each game's set entries and lone images) on the host with the
core's boot_test (.build/host/boot_test.exe, under shared/tools/run-limited.ps1) for <seconds> of
guest time, on the model its catalog record asks for, in order of how much of the player's
firmware a setup needs, until one runs it:

  open        the Open ROMs the app ships and VICE's virtual drive: no firmware
  open+1541   the Open ROMs and an emulated 1541: the 1541 drive ROM
  own         the player's KERNAL, BASIC and character ROMs and the virtual drive
  own+1541    the player's ROMs and an emulated 1541

After the load the pass presses fire, Space and fire again (5, 10 and 15 s on), as a player
leaves a title or an intro: a game that waits there and fails once it starts (Gobots on the Open
ROMs stops at "BRK AT $EA8A") then fails here too. A boot fails when the machine stops (a JAM),
the load has not finished, the text screen shows a BASIC error ("?FILE NOT FOUND ERROR",
"?LOAD ERROR" and the rest) or the Open ROMs' "BRK AT", or BASIC's READY. as its last line with no
sound since the load, or the display window stays blank (one colour inside the border, one
picture), with sound or without (Prince of Persia's music plays over a black screen on the Open
ROMs). A boot that
passes those runs for certain when it makes a sound or its picture changes after the load; one
that shows a single still picture in silence is doubtful (a title waiting for a key, or a game
stuck), so the next setups are tried too and the first that runs for certain wins; with none, the
first doubtful one does, marked for review. catalog/research/firmware-v1.json records the setup
that runs a game when it needs more than the first, so tools/catalog_config.MACHINE gives it
"system": "own" and/or "drive": "1541"; a game none runs is recorded as failing, for review.
The tool's own entries ("firmware_pass.py: ...") follow each run's results; entries decided by
hand are kept as they are. A hand entry may also name the "program" the game's medium starts
instead of its first (C64Catalog.program, boot_test's <program>), for a disk whose first file is
not the game.

Results are kept per game in .downloads/firmware-pass/results.json, so a stopped run carries on;
a game booted by an older pass (PASS below its version) is booted again from the first setup
that ran it then (the setups that failed stay failed; a tape is booted again on every setup). The last frame of every
game that needs firmware, fails everywhere or runs only doubtfully is kept beside it for review.
The firmware is bios/ (kernal, basic, chargen and dos1541). --scanned boots every medium the scans
found instead, on the model its name asks for, for a pass that starts before the research does;
the list then takes the results of the library's entries.
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
BOOT_TEST = ROOT / ".build/host/boot_test.exe"
SYSTEMS = ["open", "open+1541", "own", "own+1541"]
# What a setup asks of the game's settings (C64Catalog's system and drive options).
SETTINGS = {"open": {}, "open+1541": {"drive": "1541"}, "own": {"system": "own"},
            "own+1541": {"system": "own", "drive": "1541"}}
BASIC_ERROR = re.compile(r"\?[A-Z ]+ ERROR|BRK AT \$[0-9A-F]{4}")
# What the pass presses after the load (boot_test's <keys>, timed from the load's end): fire on the
# joystick (control 4), Space (keyboard matrix row 7, column 4), fire.
PRESSES = "+250:j:4,+500:7:4,+750:j:4"
# How far the sound must move after the load for a game to count as making a sound.
SOUND_RANGE = 2000
# The pass's version: results from an older one are booted again. 2: the core that loads tapes on
# the Open ROMs and starts a 1541 game after a virtual-drive one, and the picture checks.
PASS = 4
# Failures pass 3 judges anew: pass 2 failed a game whose screen showed READY. anywhere (a game
# written in BASIC shows it above what it prints after RUN) and counted text on one background
# colour as a blank screen.
RECHECK = {"back at BASIC's READY. without a sound", "a blank screen, still and silent"}
LOADING = re.compile(r"loading \(autostart, tape or disk\) for ([\d.]+) s of guest time, (still at the end|last ending)")
PICTURES = re.compile(r"pictures after load: (\d+) distinct of (\d+) seconds; the last has (\d+) colours, (\d+) in "
                      r"its display window")


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


def basic_idle(screen):
    """Whether BASIC sits at its prompt: READY. is the last text on the screen (a game written in
    BASIC also shows READY. above what it prints after RUN)."""
    rows = [row.strip() for row in screen.splitlines() if row.strip()]
    return bool(rows) and rows[-1] == "READY."


def boot(system, path, model, seconds, work):
    """The outcome of one boot: {"ok": bool, "doubtful": whether it ran only on a still, silent
    picture, "why": text, "range": int, "pictures": distinct pictures after the load, "colours": the
    last picture's, "frame": whether it ran to the end and wrote its last frame}. The frame is
    kept as work/frame-<system>.bmp."""
    for old in list(work.glob("*.bmp")) + list(work.glob("*.txt")):
        old.unlink()
    result = subprocess.run(["pwsh", "-NoProfile", "-File", str(ROOT / "shared/tools/run-limited.ps1"),
                             str(BOOT_TEST), str(ROOT / "bios"), str(work), str(path),
                             str(seconds), system, "1", PRESSES, model],
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    lines = result.stdout.splitlines() + result.stderr.splitlines()

    # How long the app would run the load flat out (the autostart, a moving tape, a disk load),
    # and whether that was still going at the end: a game the app would fast-forward in play.
    loading = next((LOADING.search(line) for line in lines if LOADING.search(line)), None)

    def outcome(ok, why, played=0, pictures=0, colours=0, frame=True, doubtful=False):
        if frame:
            (work / "frame-0.bmp").replace(work / f"frame-{system}.bmp")
        return {"ok": ok, "doubtful": doubtful, "why": why, "range": played, "pictures": pictures,
                "colours": colours, "frame": frame,
                "loading": float(loading.group(1)) if loading else None,
                "loadingAtEnd": bool(loading and loading.group(2) == "still at the end")}

    for line in lines:
        if line.startswith("run 0: could not start") or line.startswith("run 0: stopped after"):
            return outcome(False, line.split(": ", 2)[-1], frame=False)
    summary = next((line for line in lines if line.startswith("run 0:") and " range after load " in line), None)
    if summary is None:
        crash = next((line for line in lines if line.startswith("exit -")), None)
        if crash:
            return outcome(False, f"the core crashed ({crash})", frame=False)
        raise RuntimeError(f"boot_test gave no result for {path} on {system}:\n" + "\n".join(lines[-20:]))
    played = int(summary.split(" range after load ")[1].split(",")[0])
    found = next(PICTURES.search(line) for line in lines if PICTURES.search(line))
    pictures, colours = int(found.group(1)), int(found.group(4))
    if summary.endswith("(still loading)"):
        return outcome(False, f"still loading after {seconds} s", played, pictures, colours)
    screen = (work / "screen-0.txt").read_text(encoding="ascii")
    error = BASIC_ERROR.search(screen)
    if error:
        return outcome(False, f"BASIC says {error.group(0)}", played, pictures, colours)
    sound = played >= SOUND_RANGE
    if basic_idle(screen) and not sound:
        return outcome(False, "back at BASIC's READY. without a sound", played, pictures, colours)
    if pictures <= 1 and colours <= 1:
        return outcome(False, "a blank display window" + (" with a sound" if sound else ""), played, pictures, colours)
    if sound or pictures > 1:
        return outcome(True, "runs", played, pictures, colours)
    if colours <= 1:
        return outcome(False, "a blank screen, still and silent", played, pictures, colours)
    return outcome(True, "a still picture without a sound", played, pictures, colours, doubtful=True)


def runs_on(record):
    """The setup a game's record runs it on: the first that runs it for certain, else the first
    that runs it doubtfully, else None."""
    boots = record["boots"]
    certain = [system for system in SYSTEMS if boots.get(system, {}).get("ok") and not boots[system]["doubtful"]]
    doubtful = [system for system in SYSTEMS if boots.get(system, {}).get("ok") and boots[system]["doubtful"]]
    return (certain or doubtful or [None])[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("seconds", type=int)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--tape-seconds", type=int,
                        help="guest seconds for a tape (TAP), whose load alone can take minutes; <seconds> otherwise")
    parser.add_argument("--scanned", action="store_true",
                        help="boot every medium the scans found, not only the library's entries")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    results_path = OUT / "results.json"
    results = json.loads(results_path.read_text(encoding="utf-8")) if results_path.is_file() else {}
    lock = threading.Lock()
    media = all_scanned_media() if args.scanned else library_media()

    def seconds_for(name):
        return args.tape_seconds if args.tape_seconds and name.lower().endswith(".tap") else args.seconds

    def wanted(content_id, fields):
        """Whether a medium needs booting: never booted by this pass, or failed with less time than
        it is given now."""
        record = results.get(content_id, {})
        if record.get("pass") != PASS:
            return True
        return record["runsOn"] is None and record.get("seconds", args.seconds) < seconds_for(fields[2] or fields[1])

    todo = [(content_id, *fields) for content_id, fields in sorted(media.items()) if wanted(content_id, fields)]
    free = [OUT / f"work-{slot}" for slot in range(args.workers)]
    booted = [0]

    def run(item):
        content_id, root, archive, member, model = item
        with lock:
            work = free.pop()
        try:
            if work.exists():
                shutil.rmtree(work)
            work.mkdir(parents=True)
            path = extract(root, archive, member, work)
            seconds = seconds_for(member or archive)
            record = {"pass": PASS, "model": model, "seconds": seconds, "boots": {}}
            # An older pass's failures stand, except for the reasons a later pass judges anew
            # (RECHECK) and pass 1's tapes, which pass 2's core loads on the Open ROMs; its runs are
            # booted again, as pass 4's presses after the load can fail a game an older pass ran.
            # A setup's outcome is reused when it booted with as much time as it is given now.
            previous = results.get(content_id, {})
            earlier_pass = previous.get("pass", 1)
            earlier = previous.get("boots", {}) if previous.get("seconds", args.seconds) >= seconds else {}
            booted_now = []
            for system in SYSTEMS:
                old = earlier.get(system)
                if old is not None and (not old["ok"] or earlier_pass == PASS) and old["why"] not in RECHECK                         and not (path.suffix == ".tap" and earlier_pass == 1):
                    outcome = record["boots"][system] = dict(old, fromPass=old.get("fromPass", earlier_pass))
                    if outcome["ok"] and not outcome["doubtful"]:
                        break
                    continue
                outcome = boot(system, path, model, seconds, work)
                record["boots"][system] = outcome
                booted_now.append(system)
                if outcome["ok"] and not outcome["doubtful"]:
                    break
            chosen = runs_on(record)
            record["runsOn"] = chosen
            # For review, the frame of the setup that runs the game, or of the last setup booted
            # when none does (a machine that stops or cannot start writes none); an older pass's
            # frame stays when this one booted nothing.
            kept = OUT / f"{content_id.split(':')[1]}.bmp"
            frame = work / f"frame-{chosen or (booted_now or [''])[-1]}.bmp"
            if chosen == "open" and not record["boots"]["open"]["doubtful"]:
                if kept.is_file():
                    kept.unlink()
            elif frame.is_file():
                frame.replace(kept)
        finally:
            with lock:
                free.append(work)
        with lock:
            results[content_id] = record
            booted[0] += 1
            if booted[0] % 25 == 0:
                results_path.write_text(json.dumps(results, indent=1, sort_keys=True) + "\n", encoding="utf-8")
                print(f"{booted[0]} of {len(todo)} booted", flush=True)

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

    def own(entry):
        """Whether an entry is this tool's, which each run replaces; others were decided by hand."""
        return (entry.get("why") or entry.get("fails") or "").startswith("firmware_pass.py:")
    counts = {}
    for content_id, record in sorted((key, value) for key, value in results.items() if key in media):
        boots = record["boots"]
        chosen = record["runsOn"]
        key = (chosen or "none") + (" (doubtful)" if chosen and boots[chosen]["doubtful"] else "")
        counts[key] = counts.get(key, 0) + 1
        entry = listed["games"].get(content_id)
        if entry is not None and not own(entry):
            continue  # decided by hand
        if chosen == "open":
            listed["games"].pop(content_id, None)
            continue
        failure = boots["open"]["why"]
        if chosen is None:
            listed["games"][content_id] = {"fails": f"firmware_pass.py: fails on every setup; on the Open ROMs: {failure}"}
        else:
            listed["games"][content_id] = dict(SETTINGS[chosen], why=(
                f"firmware_pass.py: on the Open ROMs and the virtual drive: {failure}; on {chosen}: "
                f"{boots[chosen]['why']}"))
    listed["games"] = dict(sorted(listed["games"].items()))
    LIST.write_text(json.dumps(listed, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    print(f"{len(results)} games booted; runs on: {counts}; {len(listed['games'])} listed")


if __name__ == "__main__":
    main()
