/* Kairo64's start of VICE, in place of the headless UI's main.c: VICE's main_program runs on the
 * thread that calls KairoC64_Run, and never returns while VICE runs. When VICE ends for good
 * (archdep_vice_exit, kairo/archdep_exit.c), it comes back here. */

#include "vice.h"

#include <setjmp.h>

#include "kairo_c64.h"
#include "log.h"
#include "machine.h"
#include "main.h"

/* Where archdep_vice_exit returns to: the end of KairoC64_Run. */
jmp_buf kairo_c64_exit;

void KairoC64_Run(int argc, char **argv)
{
    if (setjmp(kairo_c64_exit) != 0) {
        return;
    }
    if (main_program(argc, argv) != 0) {
        KairoC64Host_Fatal("VICE could not start");
        return;
    }
    /* main_program runs the machine until it exits (main_loop_forever). */
    KairoC64Host_Fatal("VICE's main loop ended");
}

/* VICE's shutdown: Kairo64 keeps VICE for the life of its process, so this runs only if VICE
 * asks for it itself. */
void main_exit(void)
{
    log_message(LOG_DEFAULT, "\nExiting...");
    machine_shutdown();
}
