#!/usr/bin/env python3
"""Decide a game's C64 model by running it: PAL or NTSC (Kairo64#14).

Usage: model_pass.py <seconds> [--tape-seconds N] [--workers N]

The catalog starts a USA, Japanese or Canadian release on an NTSC C64 and every other on PAL
(tools/catalog_config.py, by the release's name). A name can be wrong: 1943: The Battle of Midway
(USA) is a PAL game (Kairo64#13). This pass boots every library entry the catalog starts on NTSC on
both a PAL and an NTSC C64, with the firmware and drive its record asks for, through the core's
boot_test (.build/host/boot_test.exe) and the firmware pass's judgement of a boot
(firmware_pass.boot): whether the load ends and the game then runs.

boot_test also reports every raster line the game's enabled raster interrupts waited on after the
load. A PAL C64 draws lines 0 to 311 and an NTSC one 0 to 262: a game that interrupts on line 263 or
later was written for PAL (on NTSC that line never comes).

The decision for each entry:
  runs on PAL only                         PAL
  runs on NTSC only                        NTSC (its name's model)
  runs on both, with a PAL-only raster     PAL
  runs on both otherwise, or on neither    its name's model
"Runs" is the firmware pass's certain run (a still, silent picture counts only where neither model
runs for certain). catalog/research/firmware-v1.json records a model that differs from the name's,
as "model" with its evidence in "modelWhy", which tools/catalog_config.MACHINE applies; the firmware
pass keeps both when it rewrites its entries. Entries decided by hand (a "why" or "modelWhy" that
does not start with this tool's name) are kept. Results are kept per entry in
.downloads/model-pass/results.json, so a stopped run carries on.
"""
import argparse
import json
import re
import shutil
import sys
import threading
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import firmware_pass

OUT = ROOT / ".downloads/model-pass"
LIST = firmware_pass.LIST
# The pass's version: results from an older one are booted again.
PASS = 1
RASTER = re.compile(r"raster interrupts after load: (\d+) lines, highest (\d+)")
# The highest raster line an NTSC C64 draws; an interrupt below it can come on PAL and NTSC alike.
NTSC_LAST_LINE = 262
PAL = "c64"
TOOL = "model_pass.py:"


def setup(content_id, listed):
    """The firmware pass's setup for an entry (boot_test's <system>): the player's ROMs where the
    research says it needs them, and the 1541 where it needs that."""
    needs = listed.get(content_id, {})
    system = "own" if needs.get("system") == "own" else "open"
    return system + ("+1541" if needs.get("drive") == "1541" else "")


def highest_raster_line(work):
    """The highest raster line the boot's interrupts waited on after the load, from boot_test's output
    (kept by boot as work/boot.txt); None without any."""
    found = RASTER.search((work / "boot.txt").read_text(encoding="utf-8"))
    return int(found.group(2)) if found and int(found.group(1)) else None


def boot(system, path, model, seconds, work):
    """firmware_pass.boot with the raster report: the outcome plus "raster", the highest raster line."""
    outcome = firmware_pass.boot(system, path, model, seconds, work, keep_output=work / "boot.txt")
    outcome["raster"] = highest_raster_line(work)
    return outcome


def decide(record, named):
    """The model an entry's boots ask for, and why; named is the model its name gives it."""
    boots = record["boots"]
    pal, ntsc = boots[PAL], boots[named]
    certain = {model: boot["ok"] and not boot["doubtful"] for model, boot in boots.items()}
    if not any(certain.values()):
        certain = {model: boot["ok"] for model, boot in boots.items()}
    rasters = [boot["raster"] for boot in boots.values() if boot["raster"] is not None]
    pal_raster = max(rasters, default=0) > NTSC_LAST_LINE
    if certain[PAL] and not certain[named]:
        return PAL, f"{TOOL} runs on PAL; on NTSC: {ntsc['why']}"
    if certain[named] and not certain[PAL]:
        return named, f"{TOOL} runs on NTSC; on PAL: {pal['why']}"
    if certain[PAL] and pal_raster:
        return PAL, f"{TOOL} runs on both; its raster interrupts reach line {max(rasters)}, which only PAL draws"
    return named, None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("seconds", type=int)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--tape-seconds", type=int, help="guest seconds for a tape (TAP); <seconds> otherwise")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    results_path = OUT / "results.json"
    results = json.loads(results_path.read_text(encoding="utf-8")) if results_path.is_file() else {}
    listed = json.loads(LIST.read_text(encoding="utf-8"))
    lock = threading.Lock()
    # The entries whose name gives them another model than PAL, whatever was decided for them since.
    media = firmware_pass.library_media()
    named_models = {content_id: firmware_pass.catalog_config.region_model(fields[2] or fields[1])
                    for content_id, fields in media.items()}
    media = {content_id: fields for content_id, fields in media.items() if named_models[content_id] != PAL}

    def seconds_for(name):
        return args.tape_seconds if args.tape_seconds and name.lower().endswith(".tap") else args.seconds

    todo = [(content_id, *fields) for content_id, fields in sorted(media.items())
            if results.get(content_id, {}).get("pass") != PASS]
    free = [OUT / f"work-{slot}" for slot in range(args.workers)]
    booted = [0]

    def run(item):
        content_id, root, archive, member, _ = item
        with lock:
            work = free.pop()
        try:
            if work.exists():
                shutil.rmtree(work)
            work.mkdir(parents=True)
            path = firmware_pass.extract(root, archive, member, work)
            seconds = seconds_for(member or archive)
            system = setup(content_id, listed["games"])
            named = named_models[content_id]
            record = {"pass": PASS, "system": system, "named": named, "seconds": seconds,
                      "boots": {model: boot(system, path, model, seconds, work) for model in (PAL, named)}}
            record["model"], record["why"] = decide(record, named)
        finally:
            with lock:
                free.append(work)
        with lock:
            results[content_id] = record
            booted[0] += 1
            if booted[0] % 25 == 0:
                results_path.write_text(json.dumps(results, indent=1, sort_keys=True) + "\n", encoding="utf-8")
                print(f"{booted[0]} of {len(todo)} booted", flush=True)

    print(f"{len(media)} entries named for another model than PAL; {len(todo)} to boot", flush=True)
    with ThreadPoolExecutor(args.workers) as pool:
        for _ in pool.map(run, todo):
            pass
    results_path.write_text(json.dumps(results, indent=1, sort_keys=True) + "\n", encoding="utf-8")

    counts = {}
    for content_id in sorted(media):
        record = results[content_id]
        entry = listed["games"].get(content_id, {})
        hand = entry.get("modelWhy") and not entry["modelWhy"].startswith(TOOL)
        key = f"{record['named']} -> {record['model']}"
        counts[key] = counts.get(key, 0) + 1
        if hand:
            continue  # decided by hand
        entry.pop("model", None)
        entry.pop("modelWhy", None)
        if record["model"] != record["named"]:
            entry["model"], entry["modelWhy"] = record["model"], record["why"]
        if entry:
            listed["games"][content_id] = entry
        else:
            listed["games"].pop(content_id, None)
    listed["games"] = dict(sorted(listed["games"].items()))
    LIST.write_text(json.dumps(listed, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    print(f"{len(media)} entries decided: {counts}")


if __name__ == "__main__":
    main()
