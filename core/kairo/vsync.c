/* Kairo64's end of frame for VICE, in place of VICE's vsync.c. VICE paces itself to the host's
 * clock there, sleeping between frames; in Kairo64 the host paces the machine by its audio
 * device (shared/frontend FrameMachine), so the end of each frame does VICE's frame work and
 * then hands the machine to the host (KairoC64Host_Vbl) until it asks for the next frame.
 *
 * The rest is VICE's vsync.c as it was: the callbacks queued for the next vsync, the Speed and
 * InitialWarpMode resources and the warp switch, which other parts of VICE read and set. */

#include "vice.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "cmdline.h"
#include "kairo_c64.h"
#include "kbdbuf.h"
#include "lib.h"
#include "log.h"
#include "machine.h"
#include "maincpu.h"
#include "monitor.h"
#include "resources.h"
#include "sound.h"
#include "types.h"
#include "videoarch.h"
#include "vsync.h"
#include "vsyncapi.h"

log_t vsync_log = LOG_DEFAULT;

/* ------------------------------------------------------------------------- */
/* Callbacks for the next vsync, as in VICE's vsync.c. */

typedef struct callback_queue_s {
    vsync_callback_t *queue;
    int size_max;
    int size;
} callback_queue_t;

static callback_queue_t callback_queues[2];
static callback_queue_t *callback_queue = callback_queues;
static int callback_queue_index;

void vsync_on_vsync_do(vsync_callback_func_t callback_func, void *callback_param)
{
    if (callback_queue->size == callback_queue->size_max) {
        callback_queue->size_max++;
        callback_queue->queue = lib_realloc(callback_queue->queue, callback_queue->size_max * sizeof(vsync_callback_t));
    }
    callback_queue->queue[callback_queue->size].callback = callback_func;
    callback_queue->queue[callback_queue->size].param = callback_param;
    callback_queue->size++;
}

/* Runs the queued callbacks, and the ones they queue, until none are left. */
static void execute_vsync_callbacks(void)
{
    int i;
    callback_queue_t *executing_queue;

    while (callback_queue->size) {
        executing_queue = callback_queue;
        callback_queue_index = 1 - callback_queue_index;
        callback_queue = &callback_queues[callback_queue_index];
        for (i = 0; i < executing_queue->size; i++) {
            executing_queue->queue[i].callback(executing_queue->queue[i].param);
        }
        executing_queue->size = 0;
    }
}

/* ------------------------------------------------------------------------- */
/* Speed and warp. The host runs frames as it needs them, so neither changes the pace; warp still
 * reaches the sound system, as in VICE. */

static int relative_speed;
static int warp_enabled;
static int initial_warp_mode_resource;

static int set_relative_speed(int val, void *param)
{
    if (val == 0) {
        val = 100;
    }
    relative_speed = val;
    sound_set_relative_speed(relative_speed);
    return 0;
}

void vsync_set_warp_mode(int val)
{
    warp_enabled = val ? 1 : 0;
    sound_set_warp_mode(warp_enabled);
}

int vsync_get_warp_mode(void)
{
    return warp_enabled;
}

static int set_initial_warp_mode_resource(int val, void *param)
{
    initial_warp_mode_resource = val ? 1 : 0;
    return 0;
}

static const resource_int_t resources_int[] = {
    { "Speed", 100, RES_EVENT_SAME, NULL,
      &relative_speed, set_relative_speed, NULL },
    { "InitialWarpMode", 0, RES_EVENT_STRICT, (resource_value_t)0,
      &initial_warp_mode_resource, set_initial_warp_mode_resource, NULL },
    RESOURCE_INT_LIST_END
};

int vsync_resources_init(void)
{
    return resources_register_int(resources_int);
}

static const cmdline_option_t cmdline_options[] = {
    { "-speed", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "Speed", NULL,
      "<percent or negative fps>", "Limit emulation speed to specified value" },
    CMDLINE_LIST_END
};

int vsync_cmdline_options_init(void)
{
    return cmdline_register_options(cmdline_options);
}

/* ------------------------------------------------------------------------- */

static double refresh_frequency;
static void (*vsync_hook)(void);

void vsync_set_machine_parameter(double refresh, long cycles)
{
    refresh_frequency = refresh;
}

double vsync_get_refresh_frequency(void)
{
    return refresh_frequency;
}

double KairoC64_FrameSeconds(void)
{
    return 1.0 / refresh_frequency;
}

void vsync_init(void (*hook)(void))
{
    vsync_log = log_open("VSync");
    vsync_set_warp_mode(initial_warp_mode_resource);
    vsync_hook = hook;
}

void vsync_shutdown(void)
{
    int i;

    for (i = 0; i < 2; i++) {
        lib_free(callback_queues[i].queue);
        callback_queues[i].queue = NULL;
        callback_queues[i].size = callback_queues[i].size_max = 0;
    }
}

void vsync_suspend_speed_eval(void)
{
}

void vsync_reset_hook(void)
{
    execute_vsync_callbacks();
}

/* There are no speed metrics: the host measures its own pace. */
void vsyncarch_get_metrics(double *cpu_percent, double *emulated_fps, int *is_warp_enabled)
{
    *cpu_percent = relative_speed;
    *emulated_fps = refresh_frequency;
    *is_warp_enabled = warp_enabled;
}

/* At the end of every raster line: the sound produced so far goes to the sound device. */
void vsync_do_end_of_line(void)
{
    sound_flush();
}

/* Every frame is drawn. */
bool vsync_should_skip_frame(struct video_canvas_s *canvas)
{
    return false;
}

/* At the end of every frame: VICE's frame work, then the host's turn. */
void vsync_do_vsync(struct video_canvas_s *c)
{
    monitor_vsync_hook();
    vsyncarch_presync();
    vsync_hook();
    vsyncarch_postsync();
    execute_vsync_callbacks();
    kbdbuf_flush();
    KairoC64Host_Vbl();
}
