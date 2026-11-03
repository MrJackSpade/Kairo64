/* Kairo64's start of VICE, in place of the headless UI's main.c: VICE's main_program runs on the
 * thread that calls KairoC64_Run, and never returns while VICE runs. It comes back here when the
 * host asks VICE to quit (KairoC64_Quit: the end of a frame shuts VICE down, kairo/vsync.c) or when
 * VICE ends for good itself (archdep_vice_exit, kairo/archdep_exit.c). This file also holds the
 * host's calls VICE's thread makes; kairo/api.c the library's entry point. */

#include "vice.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#include "kairo_c64.h"
#include "log.h"
#include "machine.h"
#include "main.h"

/* Where the quit and archdep_vice_exit return to: the end of KairoC64_Run, with KAIRO_C64_QUIT for
 * a quit the host asked for. */
jmp_buf kairo_c64_exit;
#define KAIRO_C64_QUIT 2

/* VICE's per-user folder (kairo/archdep_user_paths.c). */
extern const char *kairo_c64_user_dir;

/* The machine running VICE in this copy of the library. */
static const struct KairoC64Host *host;
static bool quitting;

bool KairoC64_Run(const char *user_dir, int argc, char **argv, const struct KairoC64Host *run_host)
{
    if (host != NULL) {
        /* A library copy runs one machine (kairo_c64.h). */
        abort();
    }
    host = run_host;
    if (user_dir == NULL) {
        KairoC64Host_Fatal("VICE has no user folder");
        return false;
    }
    kairo_c64_user_dir = user_dir;
    const int jumped = setjmp(kairo_c64_exit);
    if (jumped != 0) {
        return jumped == KAIRO_C64_QUIT;
    }
    if (main_program(argc, argv) != 0) {
        KairoC64Host_Fatal("VICE could not start");
        return false;
    }
    /* main_program runs the machine until it exits (main_loop_forever). */
    KairoC64Host_Fatal("VICE's main loop ended");
    return false;
}

void KairoC64_Quit(void)
{
    quitting = true;
}

bool KairoC64_Quitting(void)
{
    return quitting;
}

/* VICE's shutdown, at the end of the frame the quit came in, or when VICE asks for it itself. */
void main_exit(void)
{
    log_message(LOG_DEFAULT, "\nExiting...");
    machine_shutdown();
}

/* At the end of the frame a quit came in (kairo/vsync.c): VICE shuts down and KairoC64_Run returns. */
void kairo_c64_quit_now(void)
{
    main_exit();
    longjmp(kairo_c64_exit, KAIRO_C64_QUIT);
}

void KairoC64Host_Vbl(void) { host->vbl(host->context); }
void KairoC64Host_Fatal(const char *message) { host->fatal(host->context, message); }
/* VICE logs with no machine running too (create_blank_disk): to stderr then, which the app's
 * frontend forwards to the platform's log. */
void KairoC64Host_Log(const char *level, const char *text)
{
    if (host == NULL) {
        fprintf(stderr, "VICE: %s%s\n", level, text);
        return;
    }
    host->log(host->context, level, text);
}
void KairoC64Host_Jam(const char *message) { host->jam(host->context, message); }
