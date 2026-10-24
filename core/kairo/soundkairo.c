/* Kairo64's sound device for VICE: VICE's mixed output is kept until the host takes it
 * (KairoC64_ReadAudio) between frames. The local change vice-kairo-sound lists it first in
 * VICE's sound devices (sound.c), and Kairo64 starts VICE with -sounddev kairo.
 *
 * VICE writes on its thread during a frame and the host reads on its own between frames; the two
 * never run at once (kairo_c64.h), so the buffer needs no lock. */

#include "vice.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "kairo_c64.h"
#include "lib.h"
#include "resources.h"
#include "sound.h"

/* Stereo frames written and not yet taken: count frames from the start of buffer. */
static int16_t *buffer;
static size_t count;
static size_t capacity;
static int channels;

static int kairo_init(const char *param, int *speed, int *fragsize, int *fragnr, int *channels_wanted)
{
    /* The host plays stereo; VICE mixes a single SID to one channel unless asked for two. */
    channels = *channels_wanted;
    count = 0;
    return 0;
}

/* nr samples: nr / channels frames. */
static int kairo_write(int16_t *samples, size_t nr)
{
    size_t frames = nr / (size_t)channels;
    size_t i;

    if (count + frames > capacity) {
        capacity = (count + frames) * 2;
        buffer = lib_realloc(buffer, capacity * 2 * sizeof(int16_t));
    }
    for (i = 0; i < frames; i++) {
        int16_t left = samples[i * (size_t)channels];
        int16_t right = channels > 1 ? samples[i * (size_t)channels + 1] : left;
        buffer[(count + i) * 2] = left;
        buffer[(count + i) * 2 + 1] = right;
    }
    count += frames;
    return 0;
}

static void kairo_close(void)
{
    count = 0;
}

static const sound_device_t kairo_device = {
    "kairo",
    kairo_init,
    kairo_write,
    NULL,
    NULL,
    NULL,
    kairo_close,
    NULL,
    NULL,
    0,
    2,
    /* The host's audio device paces the machine; VICE need not. */
    true
};

int sound_init_kairo_device(void)
{
    return sound_register_device(&kairo_device);
}

/* VICE opens its sound device at the first sound it makes, after the host has opened its own, so
 * the rate is the resource the device takes as it is (kairo_init). */
int KairoC64_SampleRate(void)
{
    int rate;

    resources_get_int("SoundSampleRate", &rate);
    return rate;
}

uint32_t KairoC64_ReadAudio(int16_t *stereo, uint32_t max_frames)
{
    size_t taken = count < max_frames ? count : max_frames;

    memcpy(stereo, buffer, taken * 2 * sizeof(int16_t));
    memmove(buffer, buffer + taken * 2, (count - taken) * 2 * sizeof(int16_t));
    count -= taken;
    return (uint32_t)taken;
}
