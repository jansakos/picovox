#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "device.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "stereo.pio.h"

#define LPT_BASE_PIN 0
#define SAMPLE_RATE 96000

// Variables for PIO - each device simulated has its own
static PIO used_pio;
static int8_t used_sm;
static int used_offset;

static void choose_sm(void) {
    used_pio = pio1;
    used_sm = pio_claim_unused_sm(used_pio, false);

#ifdef PICO_RP2350
    if (used_sm < 0 || !pio_can_add_program(used_pio, &stereo_program)) { // If no free sm or memory on PIO1, try PIO2 (not on Pico1)
        used_pio = pio2;
        used_sm = pio_claim_unused_sm(used_pio, false);
    }
#endif
}

bool load_stereo(Device *self) {
    choose_sm();
    if (used_sm < 0 || !pio_can_add_program(used_pio, &stereo_program)) { // If not any PIO with free sm and enough memory, abort
        return false;
    }

    used_offset = pio_add_program(used_pio, &stereo_program);
    if (used_offset < 0) {
        return false;
    }

    pio_sm_config used_config = stereo_program_get_default_config(used_offset);
    sm_config_set_in_pins(&used_config, LPT_BASE_PIN);
    sm_config_set_fifo_join(&used_config, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&used_config, (((float) clock_get_hz(clk_sys)) / (SAMPLE_RATE * 32)));

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 9; i++) { // Sets pins to use PIO
        pio_gpio_init(used_pio, i);
    }

    pio_sm_set_consecutive_pindirs(used_pio, used_sm, LPT_BASE_PIN, 9, false); // Sets pins in PIO to be inputs

    gpio_pull_up(LPT_BASE_PIN); // Pullup for cases where channel switch is floating

    if (pio_sm_init(used_pio, used_sm, used_offset, &used_config) < 0) {
        return false;
    }

    pio_sm_set_enabled(used_pio, used_sm, true);
    return true;
}

bool unload_stereo(Device *self) {
    pio_sm_set_enabled(used_pio, used_sm, false);
    pio_remove_program_and_unclaim_sm(&stereo_program, used_pio, used_sm, used_offset);

    gpio_disable_pulls(LPT_BASE_PIN);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 9; i++) {
        gpio_deinit(i);
    }
    return true;
}

size_t generate_stereo(Device *self, int16_t *left_sample, int16_t *right_sample) {
    for (int i = 0; i < 16; i++) {
        while (pio_sm_is_rx_fifo_empty(used_pio, used_sm)) {
            tight_loop_contents();
        }
        uint32_t raw_data = pio_sm_get(used_pio, used_sm);
        int16_t current_sample = (((raw_data >> 24) & 0xFF) - 128) << 8;
        if (((raw_data >> 23) & 0x01) == 0) {
            *left_sample = current_sample;
        } else {
            *right_sample = current_sample;
        }
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