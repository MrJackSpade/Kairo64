/* Kairo64's interface to VICE (third_party/vice), between the C files in core/kairo/, which build
 * with VICE into the VICE library (kairo64_vice), and the frame-driven machine (c64_machine.cpp).
 *
 * VICE keeps its whole state in process-wide variables and was written to run once per process.
 * So that every machine starts from VICE's initial state, as a new VICE process would, VICE is a
 * library of its own and the machine loads a fresh copy of it for each run (the shared
 * kairo/fresh_library.h), discarding it afterwards: nothing one machine leaves in VICE can reach
 * the next (Kairo64#67). The library's only entry point is KairoC64_Api, which returns its
 * functions (struct KairoC64Api). The KairoC64_* functions below are those functions, inside the
 * library; KairoC64Host_* are the machine's side (struct KairoC64Host), which VICE's thread calls.
 *
 * VICE runs on a thread of its own (run) and hands control to the host at the end of every frame
 * (the host's vbl). Every function other than run is called on the host's thread while VICE waits
 * there, so exactly one thread touches VICE's state at a time. */
#ifndef KAIRO_C64_H
#define KAIRO_C64_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The joystick's controls, in the order of KAIRO_C64_JOYSTICK_* bits. */
#define KAIRO_C64_JOYSTICK_CONTROLS 5

/* The text screen's 40 x 25 cells. */
#define KAIRO_C64_SCREEN_CELLS 1000

/* The machine's side, called on VICE's thread with its context. */
struct KairoC64Host {
    void *context;
    /* A frame ended: returns when the host wants the next one, or wants VICE to quit (quit). */
    void (*vbl)(void *context);
    /* VICE stopped for good: the first message is the cause. run returns false after it. */
    void (*fatal)(void *context, const char *message);
    /* A line of VICE's log, with its level ("Error", "Warning" or "" for messages). */
    void (*log)(void *context, const char *level, const char *text);
    /* The guest's CPU stopped (a JAM instruction): the game cannot continue. */
    void (*jam)(void *context, const char *message);
};

struct KairoC64Api {
    /* Starts VICE with its command line (argv[0] is the program name) and runs it on the calling
     * thread until it quits. [user_dir] is VICE's folder for its cache, configuration and state
     * (kairo/archdep_user_paths.c); it must outlive the run. Returns true when VICE quit as asked
     * (quit), false after host->fatal said why it stopped. A library copy runs one machine. */
    bool (*run)(const char *user_dir, int argc, char **argv, const struct KairoC64Host *host);
    /* Asks VICE to quit: at the end of the frame it resumes, it shuts down and run returns. */
    void (*quit)(void);

    /* The guest time of one frame, from the machine's refresh rate. */
    double (*frame_seconds)(void);
    /* The latest picture: RGBA rows of *width pixels, *height rows. */
    const uint8_t *(*screen)(int *width, int *height);
    int (*sample_rate)(void);
    /* Takes up to max_frames interleaved stereo frames produced so far. */
    uint32_t (*read_audio)(int16_t *stereo, uint32_t max_frames);

    /* Sets a VICE resource; false when VICE refused the value. */
    bool (*set_int)(const char *name, int value);
    bool (*set_string)(const char *name, const char *value);
    /* Sets the whole machine to a C64 model by VICE's name for it (its -model option: "c64",
     * "c64c", "c64ntsc", "c64cntsc" and the rest): VIC-II, SID, CIAs and board. It names the
     * model's own ROMs too, so the ROMs are set after it. False for a name VICE does not have. */
    bool (*set_model)(const char *name);
    /* Loads the KERNAL, BASIC and character ROM files, and with drive1541 the 1541's DOS ROM into
     * drive 8, whether or not a name changed: VICE loads a ROM only when its name changes, and the
     * model (set_model) names its own. False when one failed. */
    bool (*load_roms)(const char *kernal, const char *basic, const char *chargen, bool drive1541);
    /* A hard reset: the power switch, with every disk and tape left in. */
    void (*power_cycle)(void);
    /* Starts a game image (a disk, tape or program) as VICE's autostart does: loads its program
     * named program, or its first for "", and runs it. False when VICE could not. */
    bool (*autostart)(const char *path, const char *program);
    /* Inserts a disk image in drive 8 or 9, or ejects it for ""; false when VICE refused it. */
    bool (*attach_disk)(int unit, const char *path);
    /* Puts a tape in the datasette, or takes it out for "". */
    bool (*attach_tape)(const char *path);
    /* Puts a cartridge in the port, or takes it out for "", and starts the machine again with it. */
    bool (*attach_cartridge)(const char *path);
    /* Puts the IDE64 cartridge with the IDEDOS ROM rom in the port, and starts the machine again
     * with it. */
    bool (*attach_ide64)(const char *rom);
    /* Writes the programs of the T64 tape container tape onto a new D64 disk at disk, in the
     * tape's order, for a KERNAL VICE cannot load T64 files on (the Open ROMs: VICE loads T64
     * programs by trapping Commodore's KERNAL). False with a message in error when it could not. */
    bool (*tape_to_disk)(const char *tape, const char *disk, char *error, size_t error_size);
    /* Writes a new, unformatted 1541 disk (D64) at path. Needs no running machine. False when VICE
     * could not write it. */
    bool (*create_blank_disk)(const char *path);
    /* Whether VICE's autostart is still loading the game's first program. */
    bool (*autostarting)(void);
    /* Ends VICE's autostart where it is: the medium it loads from has gone, so it cannot finish. */
    void (*cancel_autostart)(void);
    /* Whether the screen editor waits for a line at BASIC's READY. prompt: no autostart is typing,
     * READY. is on the line above the cursor, the cursor blinks for input (BLNSW, $CC, is 0, and
     * the last frame counted its countdown BLNCT, $CD) and the keyboard buffer is empty (NDX,
     * $C6). */
    bool (*waits_for_line)(void);
    /* Called after each frame: samples what waits_for_line compares between frames. */
    void (*frame_done)(void);
    /* How many times the guest has read the 1351 mouse's position (patch group vice-mouse-reads). */
    unsigned long (*mouse_reads)(void);
    /* Whether the datasette's tape is moving: a tape in, the motor on and a button down
     * (kairo/uistatusbar.c). */
    bool (*tape_moving)(void);
    /* Whether drive 8 is loading: the C64 and the drive have moved the serial bus's lines within
     * the last second of guest time, with an emulated 1541's motor on. */
    bool (*disk_loading)(void);

    /* A key by its place in the keyboard matrix: row 0 to 7, column 0 to 7. */
    void (*key)(int row, int column, bool down);
    /* The RESTORE key, which is wired to the NMI rather than the matrix. */
    void (*restore)(bool down);
    /* A control of the joystick in port 1 or 2: up, down, left, right, fire. */
    void (*joystick)(int port, int control, bool down);
    /* The raster lines enabled raster interrupts waited on since the last call, one bit per line
     * (line n is bit n % 8 of byte n / 8), and forgets them. A PAL C64 draws lines 0 to 311 and an
     * NTSC one 0 to 262 (an early NTSC one to 261): a game that interrupts on line 263 or later
     * was written for PAL. */
    void (*raster_irq_lines)(uint8_t lines[64]);
    /* The 1351 mouse: motion in C64 pixels and button 1 (left) or 2 (right). */
    void (*mouse_move)(int dx, int dy);
    void (*mouse_button)(int button, bool down);

    /* The text screen's cells as ASCII, row after row, for tests: letters, digits and punctuation
     * as the C64 shows them, "." for graphics characters. */
    void (*screen_text)(char text[KAIRO_C64_SCREEN_CELLS + 1]);

    /* Asks VICE to save or load the whole machine at the next instruction boundary, which comes
     * within the next frame; snapshot_failed then says whether it failed. */
    void (*request_save)(const char *path);
    void (*request_load)(const char *path);
    bool (*snapshot_failed)(void);
};

#ifdef _WIN32
#define KAIRO_C64_EXPORT __declspec(dllexport)
#else
#define KAIRO_C64_EXPORT __attribute__((visibility("default")))
#endif

/* The library's functions. */
#define KAIRO_C64_API_SYMBOL "KairoC64_Api"
KAIRO_C64_EXPORT const struct KairoC64Api *KairoC64_Api(void);

/* Inside the library: the functions KairoC64_Api returns (kairo/main.c, kairo/control.c and the
 * rest), and what the glue and VICE's local changes call on one another. */

bool KairoC64_Run(const char *user_dir, int argc, char **argv, const struct KairoC64Host *host);
void KairoC64_Quit(void);
/* Whether the host asked VICE to quit (KairoC64_Quit): the end of the frame shuts VICE down. */
bool KairoC64_Quitting(void);
double KairoC64_FrameSeconds(void);
const uint8_t *KairoC64_Screen(int *width, int *height);
int KairoC64_SampleRate(void);
uint32_t KairoC64_ReadAudio(int16_t *stereo, uint32_t max_frames);
bool KairoC64_SetInt(const char *name, int value);
bool KairoC64_SetString(const char *name, const char *value);
bool KairoC64_SetModel(const char *name);
bool KairoC64_LoadRoms(const char *kernal, const char *basic, const char *chargen, bool drive1541);
void KairoC64_PowerCycle(void);
bool KairoC64_Autostart(const char *path, const char *program);
bool KairoC64_AttachDisk(int unit, const char *path);
bool KairoC64_AttachTape(const char *path);
bool KairoC64_AttachCartridge(const char *path);
bool KairoC64_AttachIde64(const char *rom);
bool KairoC64_TapeToDisk(const char *tape, const char *disk, char *error, size_t error_size);
bool KairoC64_CreateBlankDisk(const char *path);
bool KairoC64_Autostarting(void);
void KairoC64_CancelAutostart(void);
bool KairoC64_WaitsForLine(void);
void KairoC64_FrameDone(void);
unsigned long KairoC64_MouseReads(void);
bool KairoC64_TapeMoving(void);
bool KairoC64_DiskLoading(void);
/* At the end of every raster line (kairo/vsync.c): samples the serial bus for
 * KairoC64_DiskLoading, and notes the line an enabled raster interrupt waits on (kairo/vicii.c). */
void KairoC64_BusLine(void);
void KairoC64_RasterLine(void);
void KairoC64_Key(int row, int column, bool down);
void KairoC64_Restore(bool down);
void KairoC64_Joystick(int port, int control, bool down);
void KairoC64_RasterIrqLines(uint8_t lines[64]);
void KairoC64_MouseMove(int dx, int dy);
void KairoC64_MouseButton(int button, bool down);
void KairoC64_ScreenText(char text[KAIRO_C64_SCREEN_CELLS + 1]);
void KairoC64_RequestSave(const char *path);
void KairoC64_RequestLoad(const char *path);
bool KairoC64_SnapshotFailed(void);

/* The host's calls (struct KairoC64Host), for VICE's thread: kairo/main.c forwards them to the
 * host run was given. */
void KairoC64Host_Vbl(void);
void KairoC64Host_Fatal(const char *message);
void KairoC64Host_Log(const char *level, const char *text);
void KairoC64Host_Jam(const char *message);

#ifdef __cplusplus
}
#endif

#endif
