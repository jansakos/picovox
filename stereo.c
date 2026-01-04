#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "device.h"
#include "pio_manager.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "stereo.pio.h"

// Variables for PIO - each device simulated has its own
static PIO sound_left_pio;
static int8_t sound_left_sm;
static int sound_left_offset;

static PIO sound_right_pio;
static int8_t sound_right_sm;
static int sound_right_offset;

static PIO detection_pio;
static int8_t detection_sm;
static int detection_offset;

bool load_stereo(Device *self) {
    sound_left_offset = pio_manager_load(&sound_left_pio, &sound_left_sm, &stereo_left_program);
    if (sound_left_offset < 0) {
        return false;
    }

    sound_right_offset = pio_manager_load(&sound_right_pio, &sound_right_sm, &stereo_right_program);
    if (sound_right_offset < 0) {
        return false;
    }

    detection_offset = pio_manager_load(&detection_pio, &detection_sm, &stereo_detection_program);
    if (detection_offset < 0) {
        return false;
    }

    pio_sm_config used_config_left = stereo_left_program_get_default_config(sound_left_offset);
    sm_config_set_in_pins(&used_config_left, LPT_BASE_PIN);
    sm_config_set_fifo_join(&used_config_left, PIO_FIFO_JOIN_RX);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) { // Sets pins to use PIO
        pio_gpio_init(sound_left_pio, i);
    }

    pio_gpio_init(sound_left_pio, LPT_AUTOFEED_PIN);

    pio_sm_set_consecutive_pindirs(sound_left_pio, sound_left_sm, LPT_BASE_PIN, 8, false); // Sets pins in PIO to be inputs

    if (pio_sm_init(sound_left_pio, sound_left_sm, sound_left_offset, &used_config_left) < 0) {
        return false;
    }

    pio_sm_set_enabled(sound_left_pio, sound_left_sm, true);

    pio_sm_config used_config_right = stereo_right_program_get_default_config(sound_right_offset);
    sm_config_set_in_pins(&used_config_right, LPT_BASE_PIN);
    sm_config_set_fifo_join(&used_config_right, PIO_FIFO_JOIN_RX);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) { // Sets pins to use PIO
        pio_gpio_init(sound_right_pio, i);
    }

    pio_gpio_init(sound_right_pio, LPT_STROBE_PIN);

    pio_sm_set_consecutive_pindirs(sound_right_pio, sound_right_sm, LPT_BASE_PIN, 8, false); // Sets pins in PIO to be inputs

    if (pio_sm_init(sound_right_pio, sound_right_sm, sound_right_offset, &used_config_right) < 0) {
        return false;
    }

    pio_sm_set_enabled(sound_right_pio, sound_right_sm, true);

    pio_sm_config used_config_detection = stereo_detection_program_get_default_config(detection_offset);
    sm_config_set_in_pins(&used_config_detection, LPT_BASE_PIN + 7);
    sm_config_set_out_pins(&used_config_detection, LPT_BUSY_PIN, 1);

    pio_gpio_init(detection_pio, LPT_BASE_PIN + 7);
    pio_gpio_init(detection_pio, LPT_BUSY_PIN);

    pio_sm_set_consecutive_pindirs(detection_pio, detection_sm, LPT_BASE_PIN + 7, 1, false);
    pio_sm_set_consecutive_pindirs(detection_pio, detection_sm, LPT_BUSY_PIN, 1, true);

    if (pio_sm_init(detection_pio, detection_sm, detection_offset, &used_config_detection) < 0) {
        return false;
    }

    pio_sm_set_enabled(detection_pio, detection_sm, true);

    return true;
}

bool unload_stereo(Device *self) {
    pio_sm_set_enabled(sound_left_pio, sound_left_sm, false);
    pio_sm_set_enabled(sound_right_pio, sound_right_sm, false);
    pio_sm_set_enabled(detection_pio, detection_sm, false);
    pio_manager_unload(sound_left_pio, sound_left_sm, sound_left_offset, &stereo_left_program);
    pio_manager_unload(sound_right_pio, sound_right_sm, sound_right_offset, &stereo_right_program);
    pio_manager_unload(detection_pio, detection_sm, detection_offset, &stereo_detection_program);


    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) {
        gpio_deinit(i);
    }
    gpio_deinit(LPT_STROBE_PIN);
    gpio_deinit(LPT_AUTOFEED_PIN);
    gpio_deinit(LPT_BUSY_PIN);
    return true;
}

size_t generate_stereo(Device *self, int16_t *left_sample, int16_t *right_sample) {
    while (pio_sm_is_rx_fifo_empty(sound_left_pio, sound_left_sm) ^ pio_sm_is_rx_fifo_empty(sound_right_pio, sound_right_sm)) {
        tight_loop_contents();
    }
    if (!pio_sm_is_rx_fifo_empty(sound_left_pio, sound_left_sm)) {
        *left_sample = (((pio_sm_get(sound_left_pio, sound_left_sm) >> 24) & 0xFF) - 128) << 8;
        *right_sample = (((pio_sm_get(sound_right_pio, sound_right_sm) >> 24) & 0xFF) - 128) << 8;
    }
    return 0;
}

Device *create_stereo() {
    Device *stereo_struct = calloc(1, sizeof(Device));
    if (stereo_struct == NULL) {
        return NULL;
    }

    stereo_struct->load_device = load_stereo;
    stereo_struct->unload_device = unload_stereo;
    stereo_struct->generate_sample = generate_stereo;

    return stereo_struct;
}