#include "config.h"

// Definitions for I2S library
#define PICO_AUDIO_PIO 0
#define PICO_AUDIO_DMA_IRQ 1
#define SAMPLES_PER_BUFFER 512
#define NUM_BUFFERS 10
#define CHANNEL_COUNT 2

#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "device.h"
#include "hardware/clocks.h"

// Time stored for software debounce
volatile absolute_time_t last_change_press;

#define NUM_DEVICES 6
Device *devices[NUM_DEVICES];
int8_t current_device = 0;
int8_t wanted_device = 0;

bool load_device_list() {
    devices[0] = create_covox();
    devices[1] = create_stereo();
    devices[2] = create_ftl();
    devices[3] = create_dss();
    devices[4] = create_opl2();
    devices[5] = create_tnd();
    /*devices[6] = create_cms();*/

    for (int i = 0; i < NUM_DEVICES; i++) {
        if (devices[i] == NULL) {
            return false;
        }
    }

    return true;
}

void request_change_device(uint gpio, uint32_t events) {

    if (get_absolute_time() - last_change_press < 500000) {
        return;
    }
    last_change_press = get_absolute_time();

    wanted_device = (wanted_device + 1) % NUM_DEVICES;
}

audio_format_t requested_format = {
    .sample_freq = SAMPLE_RATE,
    .channel_count = CHANNEL_COUNT,
    .format = AUDIO_BUFFER_FORMAT_PCM_S16
};

const audio_format_t *setup_format(void) {
    static audio_i2s_config_t config = {
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

    static audio_buffer_format_t buffer_format = {
        .format = &requested_format,
        .sample_stride = 4
    };

    audio_buffer_pool_t *buffer_pool = audio_new_producer_pool(&buffer_format, NUM_BUFFERS, SAMPLES_PER_BUFFER);

    if (!audio_i2s_connect(buffer_pool)) {
        return NULL;
    }
    

    audio_i2s_set_enabled(true);
    return buffer_pool;
}

void load_change_device_irq(void) {
    gpio_init(CHANGE_BUTTON_PIN);
    gpio_set_dir(CHANGE_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(CHANGE_BUTTON_PIN);

    gpio_set_irq_enabled_with_callback(CHANGE_BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &request_change_device);

    last_change_press = get_absolute_time();
}

bool change_device(void) {
    if (!devices[current_device]->unload_device(devices[current_device])) {
        printf("Could not unload device %d\n", current_device);
        wanted_device = 0;
        return false;
    }
    printf("Unloaded device %d\n", current_device);

    current_device = wanted_device;

    if (!devices[current_device]->load_device(devices[current_device])) {
        printf("Could not load device %d\n", current_device);
        wanted_device = 0;
        return false;
    }
    printf("Switched to %d", current_device);
    return true;
}

int main()
{
    stdio_init_all();
    set_sys_clock_khz(250000, true);
    sleep_ms(1000);

    if (!load_device_list()) {
        return 1;
    }

    if (!devices[current_device]->load_device(devices[current_device])) {
        return 1;
    }

    audio_buffer_pool_t *buffer_pool = load_audio();
    if (buffer_pool == NULL) {
        return 1;
    }

    load_change_device_irq();
    
    int16_t left_sample = 0;
    int16_t right_sample = 0;
    audio_buffer_t *buffer = NULL;

    while(true) {
        if (current_device != wanted_device) {
            change_device();
        }
        while ((buffer = take_audio_buffer(buffer_pool, false)) == NULL) {
            tight_loop_contents();
        }

        int16_t *samples = (int16_t *)buffer->buffer->bytes;

        for (uint i = 0; i < buffer->max_sample_count; i++) {
            devices[current_device]->generate_sample(devices[current_device], &left_sample, &right_sample);
            samples[2 * i]     = left_sample;
            samples[2 * i + 1] = right_sample;
        }

        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(buffer_pool, buffer);
    }
    
}