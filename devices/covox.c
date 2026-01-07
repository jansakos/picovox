#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "pio_manager.h"
#include "device.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "covox.pio.h"

// Variables for PIO - each device simulated has its own
static PIO used_pio;
static int8_t used_sm;
static int used_offset;

bool load_covox(Device *self) {

    used_offset = pio_manager_load(&used_pio, &used_sm, &covox_program);
    if (used_offset < 0) {
        return false;
    }

    pio_sm_config used_config = covox_program_get_default_config(used_offset);
    sm_config_set_in_pins(&used_config, LPT_BASE_PIN);
    sm_config_set_fifo_join(&used_config, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&used_config, (((float) clock_get_hz(clk_sys)) / (SAMPLE_RATE * 2 * 3)));

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) { // Sets pins to use PIO
        pio_gpio_init(used_pio, i);
    }

    pio_sm_set_consecutive_pindirs(used_pio, used_sm, LPT_BASE_PIN, 8, false); // Sets pins in PIO to be inputs

    if (pio_sm_init(used_pio, used_sm, used_offset, &used_config) < 0) {
        return false;
    }

    pio_sm_set_enabled(used_pio, used_sm, true);
    return true;
}

bool unload_covox(Device *self) {
    pio_sm_set_enabled(used_pio, used_sm, false);
    pio_manager_unload(used_pio, used_sm, used_offset, &covox_program);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) {
        gpio_deinit(i);
    }
    return true;
}

static inline int16_t read_sample(void) {
    while (pio_sm_is_rx_fifo_empty(used_pio, used_sm)) {
        tight_loop_contents();
    }
    return (((pio_sm_get(used_pio, used_sm) >> 24) & 0xFF) - 128) << 8;
}

static inline int16_t best_sample(int16_t a, int16_t b, int16_t c) {
    int16_t max_ab = a;
    int16_t min_ab = b;
    if (a < b) {
        max_ab = b;
        min_ab = a;
    }

    if (c >= max_ab) {
        return max_ab;
    }

    if (c >= min_ab) {
        return c;
    }

    return min_ab;
}

size_t generate_covox(Device *self, int16_t *left_sample, int16_t *right_sample) {
    int16_t sample1 = read_sample();
    int16_t sample2 = read_sample();
    int16_t sample3 = read_sample();
    int16_t current_sample = best_sample(sample1, sample2, sample3);
    *left_sample = current_sample;
    *right_sample = current_sample;
    return 0;
}

Device *create_covox() {
    Device *covox_struct = calloc(1, sizeof(Device));
    if (covox_struct == NULL) {
        return NULL;
    }

    covox_struct->load_device = load_covox;
    covox_struct->unload_device = unload_covox;
    covox_struct->generate_sample = generate_covox;

    return covox_struct;
}