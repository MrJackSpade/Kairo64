/* A blank, unformatted disk image a player makes a new game on (kairo_c64.h KairoC64_CreateBlankDisk),
 * written by VICE's own image creator, as its c1541 tool and its "create disk" dialog use it. The
 * player formats it in the drive the way a C64 owner did: OPEN15,8,15,"N:NAME,ID". */

#include "vice.h"

#include <stdbool.h>

#include "diskimage.h"
#include "kairo_c64.h"

bool KairoC64_CreateBlankDisk(const char *path)
{
    return disk_image_fsimage_create(path, DISK_IMAGE_TYPE_D64) == 0;
}
