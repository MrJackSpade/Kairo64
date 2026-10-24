/* Kairo64's mouse driver for VICE, in place of the headless UI's, which drops the button
 * functions VICE's mouse emulation gives it: the host's buttons reach them
 * (KairoC64_MouseButton). Motion goes to VICE's mouse_move directly (kairo/control.c). */

#include "vice.h"

#include <stdbool.h>
#include <stddef.h>

#include "kairo_c64.h"
#include "mouse.h"
#include "mousedrv.h"

static const mouse_func_t *buttons;

void mousedrv_init(void)
{
}

void mousedrv_mouse_changed(void)
{
}

int mousedrv_resources_init(const mouse_func_t *funcs)
{
    buttons = funcs;
    return 0;
}

int mousedrv_cmdline_options_init(void)
{
    return 0;
}

void KairoC64_MouseButton(int button, bool down)
{
    if (button == 1) {
        buttons->mbl(down);
    } else if (button == 2) {
        buttons->mbr(down);
    }
}
