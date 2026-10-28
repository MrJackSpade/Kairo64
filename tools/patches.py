"""Kairo64's local changes to the third-party snapshots in third_party/, which
shared/tools/local_patches.py applies and checks. docs/source-import.md describes each change."""

# VICE's sources are ASCII and Latin-1; Latin-1 keeps every byte as it is.
ENCODING = 'latin-1'

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

# A drive that true emulation turned off keeps its rotation clock and motor state, while a power
# cycle sets its CPU clock back to zero. Changing its type advanced the disk's rotation from that
# stale clock: the difference wrapped around and the catch-up never ended (Kairo64 hung starting a
# 1541 game after a virtual-drive one). A disabled drive's disk does not turn, and enabling a
# drive starts its rotation from the drive's own clock, as its CPU's already is.
VICE_DRIVE_ROTATION_AFTER_DISABLE = [
    ('vice/src/drive/drive.c',
     '    /* TODO: drive 1? */\n    rotation_rotate_disk(drive0);\n',
     '    /* TODO: drive 1? */\n'
     '    /* Kairo64: only a running drive\'s disk turns; a disabled one\'s rotation clock is stale */\n'
     '    if (drv->enable) {\n'
     '        rotation_rotate_disk(drive0);\n'
     '    }\n'),
    ('vice/src/drive/drive.c',
     '    /* resync */\n    drv->cpu->stop_clk = *(drv->clk_ptr);\n',
     '    /* resync */\n    drv->cpu->stop_clk = *(drv->clk_ptr);\n'
     '    /* Kairo64: and the rotation, which stood still while the drive was off */\n'
     '    rotation_begins(drv->drives[0]);\n'),
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

# The virtual drive follows a directory's chain of sectors until one has no next sector. A disk
# whose chain loops back (Quizam! (Europe) (Disk 3)) kept a search for a name it does not hold
# going for ever inside one emulated instruction, and the app froze. A chain cannot hold more
# sectors than a disk has track and sector numbers (256 x 256), so a search that reads more ends
# with an error, as one that cannot read a sector does.
VICE_DIRECTORY_LOOP = [
    ('vice/src/vdrive/vdrive-dir.c',
     '    unsigned int t, s, c;\n    uint8_t *dirbuf = NULL;\n',
     '    unsigned int t, s, c;\n    uint8_t *dirbuf = NULL;\n'
     '    unsigned int sectors_read = 0; /* Kairo64: a directory chain that loops */\n'),
    ('vice/src/vdrive/vdrive-dir.c',
     '            status = vdrive_read_sector(vdrive, dir->buffer, dir->track, dir->sector);\n'
     '            if (status != 0) {\n'
     '                return NULL; /* error */\n'
     '            }\n'
     '        }\n\n'
     '        if (vdrive_dir_name_match(',
     '            /* Kairo64: more sectors than a disk has numbers for: the chain loops */\n'
     '            if (++sectors_read > 256 * 256) {\n'
     '                log_error(vdrive_dir_log, "The directory\'s chain of sectors loops (at %u/%u).",\n'
     '                          dir->track, dir->sector);\n'
     '                return NULL; /* error */\n'
     '            }\n'
     '            status = vdrive_read_sector(vdrive, dir->buffer, dir->track, dir->sector);\n'
     '            if (status != 0) {\n'
     '                return NULL; /* error */\n'
     '            }\n'
     '        }\n\n'
     '        if (vdrive_dir_name_match('),
    ('vice/src/vdrive/vdrive-dir.c',
     '    static uint8_t return_slot[32];\n    vdrive_t *vdrive = dir->vdrive;\n\n'
     '#ifdef DEBUG_DRIVE\n'
     '    log_debug(LOG_DEFAULT, "DIR: vdrive_dir_find_next_slot start (t:%u/s:%u) #%u", dir->track, dir->sector, dir->slot);\n',
     '    static uint8_t return_slot[32];\n    vdrive_t *vdrive = dir->vdrive;\n'
     '    unsigned int sectors_read = 0; /* Kairo64: a directory chain that loops */\n\n'
     '#ifdef DEBUG_DRIVE\n'
     '    log_debug(LOG_DEFAULT, "DIR: vdrive_dir_find_next_slot start (t:%u/s:%u) #%u", dir->track, dir->sector, dir->slot);\n'),
    ('vice/src/vdrive/vdrive-dir.c',
     '            status = vdrive_read_sector(vdrive, dir->buffer, dir->track, dir->sector);\n'
     '            if (status != 0) {\n'
     '                return NULL; /* error */\n'
     '            }\n'
     '        }\n'
     '        if (vdrive_dir_part_name_match(',
     '            /* Kairo64: more sectors than a disk has numbers for: the chain loops */\n'
     '            if (++sectors_read > 256 * 256) {\n'
     '                log_error(vdrive_dir_log, "The partition table\'s chain of sectors loops (at %u/%u).",\n'
     '                          dir->track, dir->sector);\n'
     '                return NULL; /* error */\n'
     '            }\n'
     '            status = vdrive_read_sector(vdrive, dir->buffer, dir->track, dir->sector);\n'
     '            if (status != 0) {\n'
     '                return NULL; /* error */\n'
     '            }\n'
     '        }\n'
     '        if (vdrive_dir_part_name_match('),
]

PATCHES = {
    'vice-directory-loop': VICE_DIRECTORY_LOOP,
    'vice-autostart-keeps-virtual-drive': VICE_AUTOSTART_KEEPS_VIRTUAL_DRIVE,
    'vice-drive-rotation-after-disable': VICE_DRIVE_ROTATION_AFTER_DISABLE,
    'vice-program-disk-for-virtual-drive': VICE_PROGRAM_DISK_FOR_VIRTUAL_DRIVE,
    'vice-jam': VICE_JAM,
    'vice-tape-load-names-device': VICE_TAPE_LOAD_NAMES_DEVICE,
    'vice-kairo-sound': VICE_KAIRO_SOUND,
}
