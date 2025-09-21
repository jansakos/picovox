#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "device.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "ftl.pio.h"

#define LPT_BASE_PIN 1
#define LPT_SELIN 11
#define LPT_PAPEREND 10
#define SAMPLE_RATE 44100

// Variables for PIO - each device simulated has its own
static PIO sound_pio;
static uint8_t sound_sm;
static uint sound_offset;

static PIO detection_pio;
static uint8_t detection_sm;
static uint detection_offset;

static void choose_sm(PIO *pio_to_assign, uint8_t *sm_to_assign, const pio_program_t *assigned_program) {
    *pio_to_assign = pio1;
    *sm_to_assign = pio_claim_unused_sm(*pio_to_assign, false);

#ifdef PICO_RP2350
    if (sound_sm < 0 || !pio_can_add_program(*pio_to_assign, assigned_program)) { // If no free sm or memory on PIO1, try PIO2 (not on Pico1)
        *pio_to_assign = pio2;
        *sm_to_assign = pio_claim_unused_sm(*pio_to_assign, false);
    }
#endif
}

bool load_ftl(Device *self) {
    choose_sm(&sound_pio, &sound_sm, &ftl_sound_program);
    if (sound_sm < 0 || !pio_can_add_program(sound_pio, &ftl_sound_program)) { // If not any PIO with free sm and enough memory, abort
        return false;
    }

    sound_offset = pio_add_program(sound_pio, &ftl_sound_program);
    if (sound_offset < 0) {
        return false;
    }

    choose_sm(&detection_pio, &detection_sm, &ftl_detection_program);
    if (detection_sm < 0 || !pio_can_add_program(detection_pio, &ftl_detection_program)) { // If not any PIO with free sm and enough memory, abort
        return false;
    }

    detection_offset = pio_add_program(detection_pio, &ftl_detection_program);
    if (detection_offset < 0) {
        return false;
    }

    pio_sm_config used_config_sound = ftl_sound_program_get_default_config(sound_offset);
    sm_config_set_in_pins(&used_config_sound, LPT_BASE_PIN);
    sm_config_set_fifo_join(&used_config_sound, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&used_config_sound, (((float) clock_get_hz(clk_sys)) / (SAMPLE_RATE * 2)));

    pio_sm_config used_config_detection = ftl_detection_program_get_default_config(detection_offset);
    sm_config_set_in_pins(&used_config_detection, LPT_SELIN);
    sm_config_set_out_pins(&used_config_detection, LPT_PAPEREND, 1);
    sm_config_set_clkdiv(&used_config_detection, 10.0);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) { // Sets pins to use PIO
        pio_gpio_init(sound_pio, i);
    }
    pio_gpio_init(detection_pio, LPT_PAPEREND);
    pio_gpio_init(detection_pio, LPT_SELIN);

    pio_sm_set_consecutive_pindirs(sound_pio, sound_sm, LPT_BASE_PIN, 8, false); // Sets pins in PIO to be inputs/outputs
    pio_sm_set_consecutive_pindirs(detection_pio, detection_sm, LPT_SELIN, 1, false);
    pio_sm_set_consecutive_pindirs(detection_pio, detection_sm, LPT_PAPEREND, 1, true);

    if (pio_sm_init(sound_pio, sound_sm, sound_offset, &used_config_sound) < 0) {
        return false;
    }

    if (pio_sm_init(detection_pio, detection_sm, detection_offset, &used_config_detection) < 0) {
        return false;
    }

    pio_sm_set_enabled(sound_pio, sound_sm, true);
    pio_sm_set_enabled(detection_pio, detection_sm, true);
    return true;
}

bool unload_ftl(Device *self) {
    pio_sm_set_enabled(sound_pio, sound_sm, false);
    pio_sm_set_enabled(detection_pio, detection_sm, false);
    pio_sm_unclaim(sound_pio, sound_sm);
    pio_sm_unclaim(detection_pio, detection_sm);
    pio_remove_program(sound_pio, &ftl_sound_program, sound_offset);
    pio_remove_program(detection_pio, &ftl_detection_program, detection_offset);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) {
        gpio_deinit(i);
    }
    gpio_deinit(LPT_PAPEREND);
    gpio_deinit(LPT_SELIN);
    return true;
}

size_t generate_ftl(Device *self, int16_t *left_sample, int16_t *right_sample) {
    while (pio_sm_is_rx_fifo_empty(sound_pio, sound_sm)) {
        tight_loop_contents();
    }
    int16_t current_sample = (((pio_sm_get(sound_pio, sound_sm) >> 24) & 0xFF) - 128) << 8;
    *left_sample = current_sample;
    *right_sample = current_sample;
    return 0;
}

Device *create_ftl() {
    Device *ftl_struct = calloc(1, sizeof(Device));
    if (ftl_struct == NULL) {
        return NULL;
    }

    ftl_struct->load_device = load_ftl;
    ftl_struct->unload_device = unload_ftl;
    ftl_struct->generate_sample = generate_ftl;

    return ftl_struct;
}