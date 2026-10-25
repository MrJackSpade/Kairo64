/* The host's control of VICE between frames (kairo_c64.h): settings, media, input and saved
 * states, through VICE's own interfaces. */

#include "vice.h"

#include <stdbool.h>
#include <string.h>

#include "attach.h"
#include "autostart.h"
#include "c64model.h"
#include "drive.h"
#include "drivetypes.h"
#include "iecbus.h"
#include "maincpu.h"
#include "interrupt.h"
#include "joyport.h"
#include "joystick.h"
#include "kairo_c64.h"
#include "keyboard.h"
#include "lib.h"
#include "machine.h"
#include "mem.h"
#include "mouse.h"
#include "resources.h"
#include "tape.h"

/* The serial bus's lines as the C64 and drive 8 last drove them, and when one last changed:
 * sampled at the end of every raster line (KairoC64_BusLine). A load moves them all the time; a
 * drive that only spins, or a C64 that never talks to it, leaves them still. */
static uint8_t bus_cpu, bus_drive;
static CLOCK bus_changed;
static bool bus_ever_changed;

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
    /* The CPU's clock starts again; the bus has not moved on this machine yet. */
    bus_ever_changed = false;
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

bool KairoC64_Autostarting(void)
{
    return autostart_in_progress() != 0;
}

void KairoC64_BusLine(void)
{
    if (iecbus.cpu_bus != bus_cpu || iecbus.drv_bus[8] != bus_drive) {
        bus_cpu = iecbus.cpu_bus;
        bus_drive = iecbus.drv_bus[8];
        bus_changed = maincpu_clk;
        bus_ever_changed = true;
    }
}

bool KairoC64_DiskLoading(void)
{
    const diskunit_context_t *unit = diskunit_context[0];
    /* Talking within the last second of guest time: the gaps of a load (a seek, a sector's
     * search, the C64 storing what came) are far shorter. */
    const bool talking = bus_ever_changed && maincpu_clk >= bus_changed &&
                         maincpu_clk - bus_changed < (CLOCK)machine_get_cycles_per_second();

    if (unit->enable && unit->type != DRIVE_TYPE_NONE) {
        /* An emulated 1541 loads with its motor on. */
        return talking && (unit->drives[0]->byte_ready_active & BRA_MOTOR_ON) != 0;
    }
    /* VICE's virtual drive has no motor: the bus alone tells. */
    return talking;
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

/* The VIC-II's text screen: its bank from CIA 2 port A (the inverse of bits 0-1 of $DD00), its
 * screen at $D018's bits 4-7 in 1 KB steps; read through VICE's peeks, which change nothing. */
void KairoC64_ScreenText(char text[KAIRO_C64_SCREEN_CELLS + 1])
{
    const int io = 3, ram = 1;
    uint16_t bank = (uint16_t)((3 - (mem_bank_peek(io, 0xdd00, NULL) & 3)) * 0x4000);
    uint16_t screen = (uint16_t)(bank + ((mem_bank_peek(io, 0xd018, NULL) >> 4) & 0x0f) * 0x400);
    int i;

    for (i = 0; i < KAIRO_C64_SCREEN_CELLS; i++) {
        /* Screen codes, reversed or not: 0 is @, 1 to 26 the letters, 32 to 63 as ASCII. */
        uint8_t code = mem_bank_peek(ram, (uint16_t)(screen + i), NULL) & 0x7f;
        text[i] = code == 0 ? '@' : code <= 26 ? (char)('A' + code - 1) : (code >= 32 && code < 64) ? (char)code
                : code == 27 ? '[' : code == 29 ? ']' : '.';
    }
    text[KAIRO_C64_SCREEN_CELLS] = '\0';
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
