#!/usr/bin/env python3
"""Applies Kairo64's local changes to the third-party snapshots in third_party/.

Usage: local_patches.py [--check]

Each change replaces text that must occur exactly once in its file. A change already applied is
skipped, so the script can run on a fresh import or on the committed tree; --check fails unless
every change is applied. docs/source-import.md describes each change.
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / 'third_party'

# VICE's sound devices are a fixed table in sound.c. Kairo64's device (core/kairo/soundkairo.c)
# hands the mixed sound to the host; it comes first, so it is the default.
VICE_KAIRO_SOUND = [
    ('vice/src/sound.h',
     'int sound_init_dummy_device(void);\n',
     'int sound_init_dummy_device(void);\n'
     'int sound_init_kairo_device(void); /* Kairo64: core/kairo/soundkairo.c */\n'),
    ('vice/src/sound.c',
     'static const sound_register_devices_t sound_register_devices[] = {\n\n',
     'static const sound_register_devices_t sound_register_devices[] = {\n\n'
     '    /* Kairo64: the host\'s sound (core/kairo/soundkairo.c) */\n'
     '    { "kairo", "Kairo64 sound output", sound_init_kairo_device, SOUND_PLAYBACK_DEVICE },\n\n'),
]

# A JAM instruction stops the C64's CPU (or a drive's). The headless UI only logs it, and the
# machine would go on running a CPU that does nothing. Kairo64 ends the game with the reason
# (KairoC64Host_Jam); the next game's power cycle clears the jam.
VICE_JAM = [
    ('vice/src/arch/headless/ui.c',
     '#include "ui.h"\n',
     '#include "ui.h"\n'
     '#include "kairo_c64.h" /* Kairo64: KairoC64Host_Jam */\n'),
    ('vice/src/arch/headless/ui.c',
     '    log_warning(LOG_DEFAULT, "%s", buffer);\n\n    lib_free(buffer);\n\n'
     '    /* the headless UI ignores this */\n    return MACHINE_JAM_ACTION_QUIT; /* quit emulator */\n',
     '    log_warning(LOG_DEFAULT, "%s", buffer);\n'
     '    KairoC64Host_Jam(buffer); /* Kairo64: the game ends with the reason */\n\n    lib_free(buffer);\n\n'
     '    return UI_JAM_NONE;\n'),
]

# VICE's autostart changes drive 8's type to one that reads the image it starts, when the current
# type cannot. With no true drive (type 0) every image "cannot", so it would turn VICE's virtual
# drive into an emulated 1541 whenever the 1541 ROM is loaded, against the game's Drive setting.
# The virtual drive reads every image, so autostart changes only a true drive's type.
VICE_AUTOSTART_KEEPS_VIRTUAL_DRIVE = [
    ('vice/src/autostart.c',
     '                /* change drive type only when image does not work in current drive */\n'
     '                if (chk < 0) {\n',
     '                /* change drive type only when image does not work in current drive */\n'
     '                /* Kairo64: and only a true drive\'s; the virtual drive (none) reads every image */\n'
     '                if (chk < 0 && drive_get_disk_drive_type(unit - DRIVE_UNIT_MIN) != DRIVE_TYPE_NONE) {\n'),
]

# A program (PRG or P00) starts from a disk VICE writes it to, in the format of drive 8's type.
# VICE's virtual drive has no type ("none"), so no format was found and no program started; it
# reads a D64 like any other disk.
VICE_PROGRAM_DISK_FOR_VIRTUAL_DRIVE = [
    ('vice/src/autostart-prg.c',
     '    switch (drive_get_disk_drive_type(unit - 8)) {\n    case DRIVE_TYPE_1540:\n',
     '    switch (drive_get_disk_drive_type(unit - 8)) {\n'
     '    case DRIVE_TYPE_NONE: /* Kairo64: the virtual drive reads a D64 like any other disk */\n'
     '    case DRIVE_TYPE_1540:\n'),
]

# VICE's autostart loads from the first tape with a bare LOAD, which Commodore's BASIC sends to the
# tape. The Open ROMs' BASIC sends it to the last drive (8 or above), so a tape never loaded on
# them. Autostart names the tape, device 1, as it already names the second tape, device 2.
VICE_TAPE_LOAD_NAMES_DEVICE = [
    ('vice/src/autostart.c',
     '                if (autostart_program_name) {\n'
     '                    tmp = util_concat("LOAD\\"", autostart_program_name, "\\"\\r", NULL);\n'
     '                } else {\n'
     '                    tmp = lib_strdup("LOAD\\r");\n'
     '                }\n',
     '                /* Kairo64: device 1 by name; the Open ROMs\' LOAD defaults to a drive */\n'
     '                if (autostart_program_name) {\n'
     '                    tmp = util_concat("LOAD\\"", autostart_program_name, "\\",1\\r", NULL);\n'
     '                } else {\n'
     '                    tmp = lib_strdup("LOAD\\"\\",1\\r");\n'
     '                }\n'),
]

PATCHES = {
    'vice-autostart-keeps-virtual-drive': VICE_AUTOSTART_KEEPS_VIRTUAL_DRIVE,
    'vice-program-disk-for-virtual-drive': VICE_PROGRAM_DISK_FOR_VIRTUAL_DRIVE,
    'vice-jam': VICE_JAM,
    'vice-tape-load-names-device': VICE_TAPE_LOAD_NAMES_DEVICE,
    'vice-kairo-sound': VICE_KAIRO_SOUND,
}


def main():
    check = sys.argv[1:] == ['--check']
    missing = []
    for name, changes in PATCHES.items():
        for rel, old, new in changes:
            path = ROOT / rel
            # VICE's sources are ASCII and Latin-1; Latin-1 keeps every byte as it is.
            with open(path, encoding='latin-1', newline='') as f:
                text = f.read()
            if text.count(new) == 1:
                continue
            if check:
                missing.append(f'{name}: {rel}')
                continue
            if text.count(old) != 1:
                raise SystemExit(f'patch {name}: {rel} does not contain the text to replace exactly once')
            with open(path, 'w', encoding='latin-1', newline='') as f:
                f.write(text.replace(old, new))
            print(f'applied {name} to {rel}')
    if missing:
        raise SystemExit('not applied: ' + ', '.join(missing))


if __name__ == '__main__':
    main()
