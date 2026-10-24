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
 * good, which ends the process's use of it (KairoC64Host_Fatal says why). */
void KairoC64_Run(int argc, char **argv);

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
/* A hard reset: the power switch, with every disk and tape left in. */
void KairoC64_PowerCycle(void);
/* Ejects every disk and tape and forgets any autostart in progress. */
void KairoC64_EjectAll(void);
/* Starts a game image (a disk, tape or program) as VICE's autostart does: loads its first
 * program and runs it. False when VICE could not. */
bool KairoC64_Autostart(const char *path);
/* Inserts a disk image in drive 8 or 9, or ejects it for ""; false when VICE refused it. */
bool KairoC64_AttachDisk(int unit, const char *path);
/* Writes the programs of the T64 tape container tape onto a new D64 disk at disk, in the tape's
 * order, for a KERNAL VICE cannot load T64 files on (the Open ROMs: VICE loads T64 programs by
 * trapping Commodore's KERNAL). False with a message in error when it could not. */
bool KairoC64_TapeToDisk(const char *tape, const char *disk, char *error, size_t error_size);
/* Whether an autostart is still loading its program, which the host runs at full speed. */
bool KairoC64_Booting(void);

/* A key by its place in the keyboard matrix: row 0 to 7, column 0 to 7. */
void KairoC64_Key(int row, int column, bool down);
/* The RESTORE key, which is wired to the NMI rather than the matrix. */
void KairoC64_Restore(bool down);
/* A control of the joystick in port 1 or 2: up, down, left, right, fire. */
void KairoC64_Joystick(int port, int control, bool down);
/* The 1351 mouse: motion in C64 pixels and button 1 (left) or 2 (right). */
void KairoC64_MouseMove(int dx, int dy);
void KairoC64_MouseButton(int button, bool down);
/* Lets go of every key, RESTORE and both joysticks. */
void KairoC64_ReleaseInputs(void);

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
