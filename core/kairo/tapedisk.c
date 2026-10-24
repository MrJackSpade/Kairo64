/* A T64 tape container's programs written onto a new D64 disk, for a C64 whose KERNAL VICE cannot
 * load T64 files on (kairo_c64.h KairoC64_TapeToDisk). A T64 holds no tape signal, only its
 * programs: VICE loads them by trapping Commodore's KERNAL tape routines, which other KERNALs
 * such as the Open ROMs do not have. On a disk, any KERNAL loads them from drive 8. Both sides
 * are VICE's own: its T64 reader (tape/t64.c, which also repairs the wrong end addresses many
 * T64 files have) and its disk writer, as its c1541 tool uses them. */

#include "vice.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "diskimage.h"
#include "kairo_c64.h"
#include "lib.h"
#include "t64.h"
#include "vdrive-iec.h"
#include "vdrive-internal.h"
#include "vdrive.h"

/* The secondary address that saves a program file (SAVE"name",8). */
#define SAVE_SECONDARY 1

/* The length of a T64 record's PETSCII name without the padding (spaces or shifted spaces). */
static unsigned int name_length(const uint8_t *name)
{
    unsigned int length = T64_REC_CBMNAME_LEN;

    while (length > 0 && (name[length - 1] == 0x20 || name[length - 1] == 0xa0 || name[length - 1] == 0)) {
        length--;
    }
    return length;
}

/* Writes the T64's program number index to the disk; false with error set. */
static bool copy_program(t64_t *t64, int index, vdrive_t *vdrive, char *error, size_t error_size)
{
    t64_file_record_t *record = &t64->file_records[index];
    unsigned int length = name_length(record->cbm_name);
    unsigned int size = (unsigned int)(record->end_addr - record->start_addr);
    uint8_t *program;
    unsigned int i;
    bool written = true;

    if (length == 0) {
        snprintf(error, error_size, "program %d of the tape has no name", index + 1);
        return false;
    }
    program = lib_malloc(size ? size : 1);
    if (t64_seek_to_file(t64, index) < 0 || t64_read(t64, program, size) != (int)size) {
        snprintf(error, error_size, "could not read program %d of the tape", index + 1);
        lib_free(program);
        return false;
    }
    if (vdrive_iec_open(vdrive, record->cbm_name, length, SAVE_SECONDARY, NULL) != SERIAL_OK) {
        snprintf(error, error_size, "could not create program %d of the tape on the disk (a name used twice?)",
                 index + 1);
        lib_free(program);
        return false;
    }
    /* A program file starts with the address it loads at. */
    written = vdrive_iec_write(vdrive, (uint8_t)(record->start_addr & 0xff), SAVE_SECONDARY) == SERIAL_OK &&
              vdrive_iec_write(vdrive, (uint8_t)(record->start_addr >> 8), SAVE_SECONDARY) == SERIAL_OK;
    for (i = 0; written && i < size; i++) {
        written = vdrive_iec_write(vdrive, program[i], SAVE_SECONDARY) == SERIAL_OK;
    }
    vdrive_iec_close(vdrive, SAVE_SECONDARY);
    lib_free(program);
    if (!written) {
        snprintf(error, error_size, "the disk had no room for program %d of the tape", index + 1);
    }
    return written;
}

bool KairoC64_TapeToDisk(const char *tape, const char *disk, char *error, size_t error_size)
{
    unsigned int read_only = 1;
    t64_t *t64;
    vdrive_t *vdrive;
    int index;
    int copied = 0;
    bool ok = true;

    t64 = t64_open(tape, &read_only);
    if (t64 == NULL) {
        snprintf(error, error_size, "%s is not a T64 tape VICE can read", tape);
        return false;
    }
    if (vdrive_internal_create_format_disk_image(disk, "KAIRO64,64", DISK_IMAGE_TYPE_D64) < 0) {
        snprintf(error, error_size, "could not create the disk %s", disk);
        t64_close(t64);
        return false;
    }
    vdrive = vdrive_internal_open_fsimage(disk, 0);
    if (vdrive == NULL) {
        snprintf(error, error_size, "could not open the disk %s", disk);
        t64_close(t64);
        return false;
    }
    /* In the tape's order, so the first program is the disk's first file, which LOAD"*" loads. */
    for (index = 0; ok && index < t64->header.num_entries; index++) {
        if (t64->file_records[index].entry_type != T64_FILE_RECORD_NORMAL) {
            continue;
        }
        ok = copy_program(t64, index, vdrive, error, error_size);
        copied += ok;
    }
    vdrive_internal_close_disk_image(vdrive);
    t64_close(t64);
    if (ok && copied == 0) {
        snprintf(error, error_size, "%s holds no programs", tape);
        ok = false;
    }
    return ok;
}
