/* Kairo64's output for VICE's log, in place of arch/shared/archdep_default_logger.c, which
 * writes to the console: each line goes to the host (KairoC64Host_Log), which keeps the errors
 * where the platform's logs are read. VICE's log file, when it has one, gets every line too. */

#include "vice.h"

#include "archdep_default_logger.h"
#include "kairo_c64.h"

int archdep_default_logger(const char *level_string, const char *txt)
{
    KairoC64Host_Log(level_string ? level_string : "", txt);
    return 0;
}

int archdep_default_logger_is_terminal(void)
{
    return 0;
}
