// Definitions for I2S library
#define PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE 26
#define PICO_AUDIO_I2S_DATA_PIN 28
#define PICO_AUDIO_PIO 0
#define PICO_AUDIO_DMA_IRQ 1
#define SAMPLES_PER_BUFFER 512
#define NUM_BUFFERS 8
#define SAMPLE_RATE 44100
#define CHANNEL_COUNT 2

#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "device.h"

static const int NUM_DEVICES = 1;
Device *devices[NUM_DEVICES];
uint8_t current_device = 0;

bool load_device_list() {
    devices[0] = create_covox();
/*    devices[1] = create_stereo();
    devices[2] = create_dss();
    devices[3] = create_ftl();
    devices[4] = create_opl2lpt();
    devices[5] = create_cmslpt();
    devices[6] = create_tndlpt();*/

    for (int i = 0; i < NUM_DEVICES; i++) {
        if (devices[i] == NULL) {
            return false;
        }
    }

    return true;
}

// Changes simulation to next device in enum
bool change_device(void) {
    if (!devices[current_device]->unload_device(devices[current_device])) {
        return false;
    }
    current_device = (current_device + 1) % NUM_DEVICES;
    if (!devices[current_device]->load_device(devices[current_device])) {
        return false;
    }
    return true;
}

const audio_format_t *setup_format(void) {
    const audio_format_t requested_format = {
        .sample_freq = SAMPLE_RATE,
        .channel_count = CHANNEL_COUNT,
        .format = AUDIO_BUFFER_FORMAT_PCM_S16
    };

    const audio_i2s_config_t config = {
        .data_pin = PICO_AUDIO_I2S_DATA_PIN,
        .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
        .dma_channel = 0,
        .pio_sm = 0
    };

    return audio_i2s_setup(&requested_format, &config);
}

audio_buffer_pool_t *load_audio(void) {
    const audio_format_t *audio_format = setup_format();

    if (audio_format == NULL) {
        return NULL;
    }

    const audio_buffer_format_t buffer_format = {
        .format = audio_format,
        .sample_stride = 4
    };

    audio_buffer_pool_t *buffer_pool = audio_new_producer_pool(&buffer_format, NUM_BUFFERS, SAMPLES_PER_BUFFER);

    if (!audio_i2s_connect(buffer_pool)) {
        return NULL;
    }

    audio_i2s_set_enabled(true);
    return buffer_pool;
}

int main()
{
    stdio_init_all();

    if (!load_device_list()) {
        return 1;
    }

    audio_buffer_pool_t *buffer_pool = load_audio();
    if (buffer_pool == NULL) {
        return 1;
    }

    if (!devices[current_device]->load_device(devices[current_device])) {
        return 1;
    }
    
}
