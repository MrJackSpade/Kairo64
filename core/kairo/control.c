/* The host's control of VICE between frames (kairo_c64.h): settings, media, input and saved
 * states, through VICE's own interfaces. */

#include "vice.h"

#include <stdbool.h>
#include <string.h>

#include "attach.h"
#include "autostart.h"
#include "c64model.h"
#include "interrupt.h"
#include "joyport.h"
#include "joystick.h"
#include "kairo_c64.h"
#include "keyboard.h"
#include "lib.h"
#include "machine.h"
#include "mouse.h"
#include "resources.h"
#include "tape.h"

bool KairoC64_SetInt(const char *name, int value)
{
    return resources_set_int(name, value) == 0;
}

bool KairoC64_SetString(const char *name, const char *value)
{
    return resources_set_string(name, value) == 0;
}

/* The models Kairo64 offers, by VICE's -model names (c64/c64-cmdline-options.c). */
static const struct {
    const char *name;
    int model;
} models[] = {
    { "c64", C64MODEL_C64_PAL },
    { "c64c", C64MODEL_C64C_PAL },
    { "c64old", C64MODEL_C64_OLD_PAL },
    { "c64ntsc", C64MODEL_C64_NTSC },
    { "c64cntsc", C64MODEL_C64C_NTSC },
    { "c64oldntsc", C64MODEL_C64_OLD_NTSC },
    { "drean", C64MODEL_C64_PAL_N },
};

bool KairoC64_SetModel(const char *name)
{
    size_t i;

    for (i = 0; i < sizeof models / sizeof models[0]; i++) {
        if (strcmp(models[i].name, name) == 0) {
            c64model_set(models[i].model);
            return true;
        }
    }
    return false;
}

void KairoC64_PowerCycle(void)
{
    machine_trigger_reset(MACHINE_RESET_MODE_POWER_CYCLE);
}

void KairoC64_EjectAll(void)
{
    autostart_reset();
    file_system_detach_disk_all();
    tape_image_detach_all();
}

bool KairoC64_Autostart(const char *path)
{
    return autostart_autodetect(path, NULL, 0, AUTOSTART_MODE_RUN) == 0;
}

bool KairoC64_AttachDisk(int unit, const char *path)
{
    if (*path == '\0') {
        file_system_detach_disk((unsigned int)unit, 0);
        return true;
    }
    return file_system_attach_disk((unsigned int)unit, 0, path) == 0;
}

bool KairoC64_Booting(void)
{
    return autostart_in_progress() != 0;
}

void KairoC64_Key(int row, int column, bool down)
{
    keyboard_set_keyarr(row, column, down ? 1 : 0);
}

void KairoC64_Restore(bool down)
{
    machine_set_restore_key(down ? 1 : 0);
}

/* The joystick's bits, in KAIRO_C64_JOYSTICK_CONTROLS order. */
static const uint16_t joystick_bits[KAIRO_C64_JOYSTICK_CONTROLS] = {
    JOYPORT_UP, JOYPORT_DOWN, JOYPORT_LEFT, JOYPORT_RIGHT, JOYPORT_FIRE
};

void KairoC64_Joystick(int port, int control, bool down)
{
    unsigned int joyport = port == 1 ? JOYPORT_1 : JOYPORT_2;

    if (down) {
        joystick_set_value_or(joyport, joystick_bits[control]);
    } else {
        joystick_set_value_and(joyport, (uint16_t)~joystick_bits[control]);
    }
}

void KairoC64_MouseMove(int dx, int dy)
{
    mouse_move((float)dx, (float)dy);
}

void KairoC64_ReleaseInputs(void)
{
    keyboard_clear_keymatrix();
    machine_set_restore_key(0);
    joystick_clear_all();
}

/* Saved states. VICE saves and loads the machine between CPU instructions, so the request is a
 * trap the CPU runs at the next one. */

static char *snapshot_path;
static bool snapshot_failed;

static void save_trap(uint16_t addr, void *data)
{
    /* With the ROMs, so a state comes back on the firmware it was saved with; disks stay in
     * their own files. */
    snapshot_failed = machine_write_snapshot(snapshot_path, 1, 0, 0) < 0;
}

static void load_trap(uint16_t addr, void *data)
{
    snapshot_failed = machine_read_snapshot(snapshot_path, 0) < 0;
}

static void request(const char *path, void (*trap)(uint16_t, void *))
{
    lib_free(snapshot_path);
    snapshot_path = lib_strdup(path);
    snapshot_failed = true;
    interrupt_maincpu_trigger_trap(trap, NULL);
}

void KairoC64_RequestSave(const char *path)
{
    request(path, save_trap);
}

void KairoC64_RequestLoad(const char *path)
{
    request(path, load_trap);
}

bool KairoC64_SnapshotFailed(void)
{
    return snapshot_failed;
}
