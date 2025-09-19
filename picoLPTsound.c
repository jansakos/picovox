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

// Definitions for PIO programs
#define LPT_STROBE_PIN 0
#define LPT_DATA_BASE 1

#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pio/covox.pio.h"
#include "pico/audio_i2s.h"

typedef enum {
    COVOX, 
    STEREO, 
    DSS, 
    FTL, 
    OPL2LPT, 
    CMSLPT, 
    TNDLPT, 
    COUNT // COUNT is used for checking count of devices available
} simulated_device;

simulated_device current_device = COVOX;

// Changes simulation to next device in enum
bool change_device(void) {
    unload_device(current_device);
    current_device = (current_device + 1) % COUNT;
    load_device(current_device);
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

    load_device(current_device);

    audio_buffer_pool_t *buffer_pool = load_audio();
    if (buffer_pool = NULL) {
        return 1;
    }
    
}
