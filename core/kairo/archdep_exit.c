/* Kairo64's exit for VICE, in place of arch/shared/archdep_exit.c, whose exit() would end the
 * whole app. VICE exits when it cannot go on (a fatal error, or the headless UI's answer to a
 * request to quit): the machine ends with an error, and VICE's thread returns from KairoC64_Run.
 * VICE's state is then left as it was, so the process cannot run another machine. */

#include "vice.h"

#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>

#include "archdep_exit.h"
#include "kairo_c64.h"

extern jmp_buf kairo_c64_exit;

static bool is_exiting;

bool archdep_is_exiting(void)
{
    return is_exiting;
}

void archdep_vice_exit(int exit_code)
{
    char message[64];

    is_exiting = true;
    snprintf(message, sizeof message, "VICE stopped (exit code %d)", exit_code);
    KairoC64Host_Fatal(message);
    longjmp(kairo_c64_exit, 1);
}
