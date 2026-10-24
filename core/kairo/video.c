/* Kairo64's video output for VICE, in place of the headless UI's (arch/headless/video.c), which
 * draws nothing: the VIC-II's frames are rendered into an RGBA buffer the host shows
 * (KairoC64_Screen). The canvas follows the size of the emulated screen, as VICE's GTK UI does,
 * and VICE's renderer writes the palette's colors as RGBA bytes, as its DirectX renderer does. */

#include "vice.h"

#include <stdint.h>
#include <string.h>

#include "cmdline.h"
#include "kairo_c64.h"
#include "lib.h"
#include "machine.h"
#include "palette.h"
#include "resources.h"
#include "video.h"
#include "videoarch.h"

/* The picture, rows of width RGBA pixels. */
static uint8_t *screen;
static int screen_width;
static int screen_height;

static const cmdline_option_t cmdline_options[] = {
    CMDLINE_LIST_END
};

static const resource_int_t resources_int[] = {
    RESOURCE_INT_LIST_END
};

int video_arch_get_active_chip(void)
{
    return VIDEO_CHIP_VICII;
}

void video_arch_canvas_init(struct video_canvas_s *canvas)
{
}

int video_arch_cmdline_options_init(void)
{
    return cmdline_register_options(cmdline_options);
}

int video_arch_resources_init(void)
{
    return resources_register_int(resources_int);
}

void video_arch_resources_shutdown(void)
{
}

/* The canvas takes the size of whatever the VIC-II shows (video_viewport_resize). */
char video_canvas_can_resize(video_canvas_t *canvas)
{
    return 1;
}

video_canvas_t *video_canvas_create(video_canvas_t *canvas, unsigned int *width, unsigned int *height,
                                    int mapped)
{
    canvas->created = 1;
    return canvas;
}

void video_canvas_destroy(struct video_canvas_s *canvas)
{
}

/* Sizes the picture to the canvas, and clears it when the size changes. */
static void size_screen(video_canvas_t *canvas)
{
    int width = (int)canvas->draw_buffer->canvas_physical_width;
    int height = (int)canvas->draw_buffer->canvas_physical_height;
    if (width == screen_width && height == screen_height) {
        return;
    }
    lib_free(screen);
    screen_width = width;
    screen_height = height;
    screen = lib_calloc((size_t)width * (size_t)height, 4);
}

void video_canvas_refresh(struct video_canvas_s *canvas, unsigned int xs, unsigned int ys, unsigned int xi,
                          unsigned int yi, unsigned int w, unsigned int h)
{
    size_screen(canvas);
    if (screen_width <= 0 || screen_height <= 0) {
        return;
    }
    if (xi + w > (unsigned int)screen_width) {
        w = (unsigned int)screen_width - xi;
    }
    if (yi + h > (unsigned int)screen_height) {
        h = (unsigned int)screen_height - yi;
    }
    video_canvas_render(canvas, screen, (int)w, (int)h, (int)xs, (int)ys, (int)xi, (int)yi, screen_width * 4);
}

void video_canvas_resize(struct video_canvas_s *canvas, char resize_canvas)
{
    size_screen(canvas);
}

/* The palette as RGBA bytes: red in the lowest byte, then green, blue and an opaque alpha. */
int video_canvas_set_palette(struct video_canvas_s *canvas, struct palette_s *palette)
{
    unsigned int i;
    video_render_color_tables_t *color_tables = &canvas->videoconfig->color_tables;

    canvas->palette = palette;
    if (palette == NULL) {
        return 0;
    }
    for (i = 0; i < palette->num_entries; i++) {
        palette_entry_t color = palette->entries[i];
        uint32_t code = color.red | ((uint32_t)color.green << 8) | ((uint32_t)color.blue << 16) | (0xffU << 24);
        video_render_setphysicalcolor(canvas->videoconfig, (int)i, code, 32);
    }
    for (i = 0; i < 256; i++) {
        video_render_setrawrgb(color_tables, i, i, i << 8, i << 16);
    }
    video_render_setrawalpha(color_tables, 0xffU << 24);
    video_render_initraw(canvas->videoconfig);
    return 0;
}

int video_init(void)
{
    return 0;
}

void video_shutdown(void)
{
}

const uint8_t *KairoC64_Screen(int *width, int *height)
{
    *width = screen_width;
    *height = screen_height;
    return screen;
}
