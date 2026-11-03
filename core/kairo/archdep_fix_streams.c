/* Kairo64's archdep_fix_streams, in place of arch/shared/archdep_fix_streams.c: the host owns its
 * streams. VICE's, on Windows, closes the process's standard output handle when it is not a file
 * (a pipe), which silenced everything the shared PC boot test printed after VICE started; on other
 * systems it does nothing, as this does everywhere. */

#include "vice.h"

#include "archdep_fix_streams.h"

void archdep_fix_streams(void)
{
}
