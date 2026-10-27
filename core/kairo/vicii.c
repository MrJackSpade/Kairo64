/* The VIC-II as the game set it, for the host (KairoC64_RasterIrqLines). Built into the C64 library,
 * whose sources see the cycle-exact VIC-II (viciisc) that x64sc runs. */

#include "vice.h"

#include <stdbool.h>
#include <string.h>

#include "kairo_c64.h"
#include "viciitypes.h"

/* The raster lines an enabled raster interrupt waited on since the host last took them, one bit
 * each: the compare is nine bits ($D012 with $D011 bit 7). */
static uint8_t raster_irq_lines[512 / 8];

void KairoC64_RasterLine(void)
{
    /* $D01A bit 0 enables the raster interrupt. */
    if (vicii.regs[0x1a] & 0x01) {
        const unsigned int line = vicii.raster_irq_line & 511;
        raster_irq_lines[line >> 3] |= (uint8_t)(1 << (line & 7));
    }
}

void KairoC64_RasterIrqLines(uint8_t lines[64])
{
    memcpy(lines, raster_irq_lines, sizeof raster_irq_lines);
    memset(raster_irq_lines, 0, sizeof raster_irq_lines);
}
