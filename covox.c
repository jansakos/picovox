#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "device.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pio/covox.pio.h"

// Variables for PIO - each device simulated has its own
static PIO used_pio;
static uint8_t used_sm;
static uint used_offset;

static void choose_sm(void) {
    used_pio = pio0;
    used_sm = pio_claim_unused_sm(used_pio, false);

    if (used_sm == -1) { // If no free sm on PIO0, try PIO1
        used_pio = pio1;
        used_sm = pio_claim_unused_sm(used_pio, false);
    }

#ifdef PICO_RP2350
    if (used_sm == -1) { // If no free sm on PIO1, try PIO2 (not on Pico1)
        used_pio = pio2;
        used_sm = pio_claim_unused_sm(used_pio, false);
    }
#endif
}

bool load_covox(Device *self) {
    choose_sm();
    if (used_sm == -1) { // If not any free sm, abort
        return false;
    }

    used_offset = pio_add_program(used_pio, &covox_program);
    
}

bool unload_covox(Device *self) {

}

size_t generate_covox(Device *self, int16_t *left_sample, int16_t *right_sample) {
    if (!pio_sm_is_rx_fifo_empty(g_pio, g_sm)) { // If FIFO is empty (no new sample), we can keep the old values
        int16_t current_sample = (((pio_sm_get(g_pio, g_sm) >> 24) & 0xFF) - 128) << 8;
        *left_sample = current_sample;
        *right_sample = current_sample;
    }
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