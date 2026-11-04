#!/usr/bin/env python3
"""Makes the shared PC boot test's CMD HD image (Kairo64#64, Kairo64#57): an empty .dhd that a C64
on Kairo64's own VICE sets up as a person does at the keyboard (core/test/cmdhd_install_run.cpp),
with CMD's HD Utilities disk in drive 9: CREATE SYS installs the HD's system (HDOS 1.92), then
HD-TOOLS, with the HD reset into configuration mode, makes partition 1, "KAIRO", native, 1024
blocks. The script ends on drive 8's directory listing it.

    make_cmdhd_image.py <kernal> <basic> <chargen> <cmdhd boot rom> <hd-utilities.d64> <output.dhd>

The ROMs are the player's (the boot ROM v2.80 is bootromCMDHD-v2-80.bin on the RetroBios share);
the HD Utilities disk is the one C64 OS's guide links (c64os.com, viceconfiguration_advanced_cmdhd).
VICE takes no paths with spaces. Build the runner first: the shared PC boot test builds it with
the app (.build/host-test/core/cmdhd_install_run.exe).
"""
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RUNNER = ROOT / ".build/host-test/core/cmdhd_install_run.exe"

SCRIPT = """\
# CREATE SYS: the empty image puts the HD in installation mode by itself.
wait 600 READY.
type LOAD"CREATE SYS",9{RETURN}
wait 3000 LOADING
type RUN{RETURN}
wait 600 PRESS <RETURN>
type {RETURN}
wait 600 SYSTEM STARTING BLOCK
type {RETURN}
wait 600 CLEAR AREA BELOW SYSTEM?
type Y
wait 30000 PRESS RESET ON HD TO REBOOT DOS
reset 8 0
run 600
# HD-TOOLS, with the HD reset holding Write Protect: configuration mode.
type LOAD"HD-TOOLS.64",9{RETURN}
wait 6000 LOADING
type RUN{RETURN}
run 600
reset 8 1
run 900
type {RETURN}
wait 3000 PRESS A KEY (1-6) FOR SELECTION
type 4
wait 1200 PRESS (RETURN) TO ACCEPT
# Number 001, type NATV, then 256 blocks raised three steps to 1024, and the name.
type {RETURN}
run 120
type {RETURN}
run 120
type +++
run 60
type {RETURN}
wait 600 ENTER PARTITION NAME
type KAIRO{RETURN}
wait 600 WRITE NEW TABLE TO HD (Y/N)?
type Y
wait 1800 PRESS ANY KEY TO CONTINUE
type {RETURN}
wait 600 PRESS A KEY (1-6) FOR SELECTION
type 6
wait 600 PRESS THE RESET BUTTON ON THE HD
reset 8 0
run 900
type {RETURN}
wait 1200 BYTES FREE
run 300
# The partition, as BASIC lists it.
type LOAD"$",8{RETURN}
wait 600 SEARCHING FOR $
run 1200
type LIST{RETURN}
wait 1200 960 BLOCKS FREE.
"""


def main(argv):
    if len(argv) != 7:
        raise SystemExit(__doc__)
    kernal, basic, chargen, rom, utilities, output = argv[1:]
    out = Path(output)
    if out.exists():
        raise SystemExit(f"{out} is there already")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(b"")
    with tempfile.TemporaryDirectory() as work:
        script = Path(work) / "script.txt"
        script.write_text(SCRIPT, encoding="ascii")
        home = Path(work) / "home"
        home.mkdir()
        run = subprocess.run([str(RUNNER), str(home), kernal, basic, chargen, rom, str(out), utilities, str(script)],
                             capture_output=True, text=True)
    if run.returncode != 0:
        out.unlink()
        raise SystemExit(f"the runner failed ({run.returncode}):\n{run.stdout[-3000:]}\n{run.stderr}")
    print(f"{out}: {out.stat().st_size} bytes")


if __name__ == "__main__":
    main(sys.argv)
