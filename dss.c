#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "device.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "dss.pio.h"

#define LPT_BASE_PIN 1
#define SELIN_PIN 11
#define ACK_PIN 9
#define SAMPLE_RATE 96000

// Variables for PIO - each device simulated has its own
static PIO used_pio;
static uint8_t used_sm;
static uint used_offset;
 
static void choose_sm(void) {
    used_pio = pio1;
    used_sm = pio_claim_unused_sm(used_pio, false);

#ifdef PICO_RP2350
    if (used_sm < 0 || !pio_can_add_program(used_pio, &dss_program)) { // If no free sm or memory on PIO1, try PIO2 (not on Pico1)
        used_pio = pio2;
        used_sm = pio_claim_unused_sm(used_pio, false);
    }
#endif
}

bool load_dss(Device *self) {
    choose_sm();
    if (used_sm < 0 || !pio_can_add_program(used_pio, &dss_program)) { // If not any PIO with free sm and enough memory, abort
        return false;
    }

    used_offset = pio_add_program(used_pio, &dss_program);
    if (used_offset < 0) {
        return false;
    }

    pio_sm_config used_config = dss_program_get_default_config(used_offset);
    sm_config_set_in_pins(&used_config, LPT_BASE_PIN);
    sm_config_set_fifo_join(&used_config, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&used_config, (((float) clock_get_hz(clk_sys)) / (SAMPLE_RATE * 2)));

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

bool unload_dss(Device *self) {
    pio_sm_set_enabled(used_pio, used_sm, false);
    pio_remove_program_and_unclaim_sm(&dss_program, used_pio, used_sm, used_offset);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) {
        gpio_deinit(i);
    }
    return true;
}

// Each sample is generated 14 times -> frequency of 6,86 kHz (within 7kHz +- 5 % specs)
size_t generate_dss(Device *self, int16_t *left_sample, int16_t *right_sample) {
    while (pio_sm_is_rx_fifo_empty(used_pio, used_sm)) {
        tight_loop_contents();
    }
    int16_t current_sample = (((pio_sm_get(used_pio, used_sm) >> 24) & 0xFF) - 128) << 8;
    *left_sample = current_sample;
    *right_sample = current_sample;
    return 0;
}

Device *create_dss() {
    Device *dss_struct = calloc(1, sizeof(Device));
    if (dss_struct == NULL) {
        return NULL;
    }

    dss_struct->load_device = load_dss;
    dss_struct->unload_device = unload_dss;
    dss_struct->generate_sample = generate_dss;

    return dss_struct;
}