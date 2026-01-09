#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "pio_manager.h"
#include "ringbuffer.h"
#include "device.h"
#include "square/square_c.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "tandy.pio.h"

#define TND_RINGBUFFER_SIZE 2048
#define TND_DETECTION_FREQ_HZ 4000000

// Variables for PIO - each device simulated has its own
static PIO sound_pio;
static int8_t sound_sm;
static int sound_offset;

static PIO detection_pio;
static int8_t detection_sm;
static int detection_offset;

// Killswitch for core1
static volatile bool stop_core1 = false;

static bool sample_used = false;

static void load_new_instruction(tandy_t *device) {
    if (pio_sm_is_rx_fifo_empty(sound_pio, sound_sm)) {
        return;
    }

    tandy_write(device, pio_sm_get(sound_pio, sound_sm) >> 24);
}

static void reset_chip(tandy_t **device) {
    if (device == NULL) {
        return;
    }

    tandy_destroy(*device);
    *device = tandy_create();
}

static void core1_operation(void) {
    tandy_t *device = tandy_create();
    int16_t current_sample = 0;

    while (!stop_core1) {
        while ((!pio_sm_is_rx_fifo_empty(sound_pio, sound_sm))) {
            load_new_instruction(device);
        }
        current_sample = tandy_get_sample(device);
        while (ringbuffer_full() && !stop_core1) {
            load_new_instruction(device);
        }
        ringbuffer_push(current_sample);
    }
    tandy_destroy(device);
}

bool load_tandy(Device *self) {

    ringbuffer_init(TND_RINGBUFFER_SIZE);

    sound_offset = pio_manager_load(&sound_pio, &sound_sm, &tandy_sound_program);
    if (sound_offset < 0) {
        return false;
    }

    detection_offset = pio_manager_load(&detection_pio, &detection_sm, &tandy_detection_program);
    if (detection_offset < 0) {
        return false;
    }

    pio_sm_config sound_config = tandy_sound_program_get_default_config(sound_offset);
    sm_config_set_in_pins(&sound_config, LPT_BASE_PIN);
    sm_config_set_fifo_join(&sound_config, PIO_FIFO_JOIN_RX);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) { // Sets pins to use PIO
        pio_gpio_init(sound_pio, i);
    }

    pio_gpio_init(sound_pio, LPT_STROBE_PIN);
    pio_gpio_init(sound_pio, LPT_INIT_PIN);

    gpio_init(LPT_SELIN_PIN);

    pio_sm_set_consecutive_pindirs(sound_pio, sound_sm, LPT_BASE_PIN, 8, false); // Sets pins in PIO to be inputs

    if (pio_sm_init(sound_pio, sound_sm, sound_offset, &sound_config) < 0) {
        return false;
    }

    pio_sm_set_enabled(sound_pio, sound_sm, true);

    pio_sm_config detection_config = tandy_detection_program_get_default_config(detection_offset);
    sm_config_set_set_pins(&detection_config, LPT_ACK_PIN, 1);
    sm_config_set_clkdiv(&detection_config, clock_get_hz(clk_sys) / TND_DETECTION_FREQ_HZ);

    pio_gpio_init(detection_pio, LPT_STROBE_PIN);
    pio_gpio_init(detection_pio, LPT_INIT_PIN);
    pio_gpio_init(detection_pio, LPT_ACK_PIN);

    pio_sm_set_consecutive_pindirs(detection_pio, detection_sm, LPT_ACK_PIN, 1, true); // Sets pins in PIO to be inputs

    if (pio_sm_init(detection_pio, detection_sm, detection_offset, &detection_config) < 0) {
        return false;
    }

    pio_sm_set_enabled(detection_pio, detection_sm, true);

    stop_core1 = false;
    multicore_reset_core1();
    multicore_launch_core1(core1_operation);
    return true;
}

bool unload_tandy(Device *self) {
    stop_core1 = true;
    pio_sm_set_enabled(sound_pio, sound_sm, false);
    pio_manager_unload(sound_pio, sound_sm, sound_offset, &tandy_sound_program);

    pio_sm_set_enabled(detection_pio, detection_sm, false);
    pio_manager_unload(detection_pio, detection_sm, detection_offset, &tandy_detection_program);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) {
        gpio_deinit(i);
    }
    gpio_deinit(LPT_STROBE_PIN);
    gpio_deinit(LPT_ACK_PIN);
    gpio_deinit(LPT_INIT_PIN);
    gpio_deinit(LPT_SELIN_PIN);

    return true;
}

size_t generate_tandy(Device *self, int16_t *left_sample, int16_t *right_sample) {
    if (!sample_used) {
        sample_used = true;
        return 0;
    }
    sample_used = false;
    int16_t curr_sample = 0;
    while (ringbuffer_empty()) {
        tight_loop_contents();
    }
    if (!ringbuffer_pop(&curr_sample)) {
        curr_sample = 0;
    }

    *left_sample = curr_sample;
    *right_sample = curr_sample;
    return 0;
}

Device *create_tandy() {
    Device *tandy_struct = calloc(1, sizeof(Device));
    if (tandy_struct == NULL) {
        return NULL;
    }

    tandy_struct->load_device = load_tandy;
    tandy_struct->unload_device = unload_tandy;
    tandy_struct->generate_sample = generate_tandy;

    return tandy_struct;
}