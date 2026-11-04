/* The VICE library's one entry point (kairo_c64.h): its functions. Built into the library itself,
 * not into one of VICE's archives, so that linking it pulls in the glue and VICE behind them. */

#include "kairo_c64.h"

const struct KairoC64Api *KairoC64_Api(void)
{
    static const struct KairoC64Api api = {
        .run = KairoC64_Run,
        .quit = KairoC64_Quit,
        .frame_seconds = KairoC64_FrameSeconds,
        .screen = KairoC64_Screen,
        .sample_rate = KairoC64_SampleRate,
        .read_audio = KairoC64_ReadAudio,
        .set_int = KairoC64_SetInt,
        .set_string = KairoC64_SetString,
        .set_model = KairoC64_SetModel,
        .load_roms = KairoC64_LoadRoms,
        .power_cycle = KairoC64_PowerCycle,
        .autostart = KairoC64_Autostart,
        .attach_disk = KairoC64_AttachDisk,
        .attach_tape = KairoC64_AttachTape,
        .attach_cartridge = KairoC64_AttachCartridge,
        .attach_ide64 = KairoC64_AttachIde64,
        .tape_to_disk = KairoC64_TapeToDisk,
        .create_blank_disk = KairoC64_CreateBlankDisk,
        .autostarting = KairoC64_Autostarting,
        .cancel_autostart = KairoC64_CancelAutostart,
        .waits_for_line = KairoC64_WaitsForLine,
        .frame_done = KairoC64_FrameDone,
        .mouse_reads = KairoC64_MouseReads,
        .tape_moving = KairoC64_TapeMoving,
        .disk_loading = KairoC64_DiskLoading,
        .key = KairoC64_Key,
        .restore = KairoC64_Restore,
        .joystick = KairoC64_Joystick,
        .raster_irq_lines = KairoC64_RasterIrqLines,
        .mouse_move = KairoC64_MouseMove,
        .mouse_button = KairoC64_MouseButton,
        .screen_text = KairoC64_ScreenText,
        .request_save = KairoC64_RequestSave,
        .request_load = KairoC64_RequestLoad,
        .snapshot_failed = KairoC64_SnapshotFailed,
    };
    return &api;
}
