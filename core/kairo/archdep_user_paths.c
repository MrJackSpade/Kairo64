/* VICE's per-user directories, for its cache (the autostart disk image), configuration
 * (resources, fliplists, joystick maps, RTC) and state (its default log): all are the machine's
 * home folder, which KairoC64_Run is given, on every platform. VICE's own choice is the XDG
 * folders under $HOME, which an Android app cannot write to, or Windows' AppData. Replaces
 * arch/shared/archdep_user_cache_path.c, archdep_user_config_path.c and
 * archdep_user_state_path.c. */

#include "vice.h"

#include "archdep_user_cache_path.h"
#include "archdep_user_config_path.h"
#include "archdep_user_state_path.h"

/* The machine's home folder, set by KairoC64_Run before VICE starts; the host owns it. */
const char *kairo_c64_user_dir;

const char *archdep_user_cache_path(void)
{
    return kairo_c64_user_dir;
}

const char *archdep_user_config_path(void)
{
    return kairo_c64_user_dir;
}

const char *archdep_user_state_path(void)
{
    return kairo_c64_user_dir;
}

/* The folder is the host's: nothing to free. */
void archdep_user_cache_path_free(void)
{
}

void archdep_user_config_path_free(void)
{
}

void archdep_user_state_path_free(void)
{
}
