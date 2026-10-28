/* Kairo64's interface to VICE (third_party/vice), between the C files in core/kairo/, which build
 * into VICE's libraries, and the frame-driven machine (c64_machine.cpp).
 *
 * VICE runs on a thread of its own, started once per process by KairoC64_Run, and hands control
 * to the host at the end of every frame (KairoC64Host_Vbl). The KairoC64_* calls other than
 * KairoC64_Run are made on the host's thread while VICE waits there, so exactly one thread
 * touches VICE's state at a time. A machine for a game is VICE reconfigured and reset; VICE itself
 * is never torn down. */
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

/* Starts VICE with its command line and runs it until it ends; returns only when VICE stops for
 * good, which ends the process's use of it (KairoC64Host_Fatal says why). [user_dir] is VICE's
 * folder for its cache, configuration and state (kairo/archdep_user_paths.c); it must outlive
 * VICE. */
void KairoC64_Run(const char *user_dir, int argc, char **argv);

/* The guest time of one frame, from the machine's refresh rate. */
double KairoC64_FrameSeconds(void);
/* The latest picture: RGBA rows of *width pixels, *height rows. */
const uint8_t *KairoC64_Screen(int *width, int *height);
int KairoC64_SampleRate(void);
/* Takes up to max_frames interleaved stereo frames produced so far. */
uint32_t KairoC64_ReadAudio(int16_t *stereo, uint32_t max_frames);

/* Sets a VICE resource; false when VICE refused the value. */
bool KairoC64_SetInt(const char *name, int value);
bool KairoC64_SetString(const char *name, const char *value);

/* Sets the whole machine to a C64 model by VICE's name for it (its -model option: "c64",
 * "c64c", "c64ntsc", "c64cntsc" and the rest): VIC-II, SID, CIAs and board. It names the model's
 * own ROMs too, so the ROMs are set after it. False for a name VICE does not have. */
bool KairoC64_SetModel(const char *name);
/* Loads the KERNAL, BASIC and character ROM files, and with drive1541 the 1541's
 * DOS ROM into drive 8, whether or not a name changed: VICE loads a ROM only when its name
 * changes, and a loaded state puts the ROMs it was saved with in their place, so the next machine
 * with the same names would start on the state's ROMs (an Open ROMs game after a state saved on
 * the player's firmware stopped at the Open KERNAL's "ROM MISMATCH"). False when one failed. */
bool KairoC64_LoadRoms(const char *kernal, const char *basic, const char *chargen, bool drive1541);
/* A hard reset: the power switch, with every disk and tape left in. */
void KairoC64_PowerCycle(void);
/* Ejects every disk and tape and forgets any autostart in progress. */
void KairoC64_EjectAll(void);
/* Starts a game image (a disk, tape or program) as VICE's autostart does: loads its program named
 * program, or its first for "", and runs it. False when VICE could not. */
bool KairoC64_Autostart(const char *path, const char *program);
/* Inserts a disk image in drive 8 or 9, or ejects it for ""; false when VICE refused it. */
bool KairoC64_AttachDisk(int unit, const char *path);
/* Writes the programs of the T64 tape container tape onto a new D64 disk at disk, in the tape's
 * order, for a KERNAL VICE cannot load T64 files on (the Open ROMs: VICE loads T64 programs by
 * trapping Commodore's KERNAL). False with a message in error when it could not. */
bool KairoC64_TapeToDisk(const char *tape, const char *disk, char *error, size_t error_size);
/* Whether VICE's autostart is still loading the game's first program. */
bool KairoC64_Autostarting(void);
/* Whether the screen editor waits for a line, as at BASIC's READY. prompt: no autostart is typing,
 * the cursor blinks for input (BLNSW, $CC, is 0) and the keyboard buffer is empty (NDX, $C6). */
bool KairoC64_WaitsForLine(void);
/* Whether the datasette's tape is moving: a tape in, the motor on and a button down
 * (kairo/uistatusbar.c). */
bool KairoC64_TapeMoving(void);
/* Whether drive 8 is loading: the C64 and the drive have moved the serial bus's lines within the
 * last second of guest time, with an emulated 1541's motor on. */
bool KairoC64_DiskLoading(void);
/* At the end of every raster line (kairo/vsync.c): samples the serial bus for KairoC64_DiskLoading. */
void KairoC64_BusLine(void);

/* A key by its place in the keyboard matrix: row 0 to 7, column 0 to 7. */
void KairoC64_Key(int row, int column, bool down);
/* The RESTORE key, which is wired to the NMI rather than the matrix. */
void KairoC64_Restore(bool down);
/* A control of the joystick in port 1 or 2: up, down, left, right, fire. */
void KairoC64_Joystick(int port, int control, bool down);

/* At the end of every raster line (kairo/vsync.c): notes the line an enabled raster interrupt
 * waits on (kairo/vicii.c). */
void KairoC64_RasterLine(void);
/* The raster lines enabled raster interrupts waited on since the last call, one bit per line
 * (line n is bit n % 8 of byte n / 8), and forgets them. A PAL C64 draws lines 0 to 311 and an
 * NTSC one 0 to 262 (an early NTSC one to 261): a game that interrupts on line 263 or later was
 * written for PAL. */
void KairoC64_RasterIrqLines(uint8_t lines[64]);
/* The 1351 mouse: motion in C64 pixels and button 1 (left) or 2 (right). */
void KairoC64_MouseMove(int dx, int dy);
void KairoC64_MouseButton(int button, bool down);
/* Lets go of every key, RESTORE and both joysticks. */
void KairoC64_ReleaseInputs(void);

/* The text screen's 40 x 25 cells as ASCII, row after row, for tests: letters, digits and
 * punctuation as the C64 shows them, "." for graphics characters. */
#define KAIRO_C64_SCREEN_CELLS 1000
void KairoC64_ScreenText(char text[KAIRO_C64_SCREEN_CELLS + 1]);

/* Asks VICE to save or load the whole machine at the next instruction boundary, which comes
 * within the next frame; KairoC64_SnapshotFailed then says whether it failed. */
void KairoC64_RequestSave(const char *path);
void KairoC64_RequestLoad(const char *path);
bool KairoC64_SnapshotFailed(void);

/* What VICE hands the host, implemented by c64_machine.cpp. */

/* On VICE's thread at the end of a frame: waits until the host lets VICE run again. */
void KairoC64Host_Vbl(void);
/* VICE stopped for good: the first message is the cause. */
void KairoC64Host_Fatal(const char *message);
/* A line of VICE's log, with its level ("Error", "Warning" or "" for messages). */
void KairoC64Host_Log(const char *level, const char *text);
/* The guest's CPU stopped (a JAM instruction): the game cannot continue. */
void KairoC64Host_Jam(const char *message);

#ifdef __cplusplus
}
#endif

#endif
