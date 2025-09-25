#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "device.h"
#include "emu8950/emu8950.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/multicore.h"
#include "opl2.pio.h"

#include "pico/stdlib.h"

#define LPT_BASE_PIN 0
#define LPT_INIT_PIN 12
#define SAMPLE_RATE 96000

// Variables for PIO - each device simulated has its own
static PIO used_pio;
static int8_t used_sm;
static int used_offset;

// Killswitch for core1
volatile bool stop_core1 = false;

OPL *simulated_opl;
volatile uint32_t register_address = 0;
volatile int16_t sample_from_core1 = 0;
volatile uint8_t value_for_reg = 0;
volatile bool should_read = false;

void core1_operation(void) {
    simulated_opl = OPL_new(3579545, 49715);
    OPL_setChipType(simulated_opl, 2); // Type 2 is YM3812*/

    while (!stop_core1) {
        if (should_read) {
            should_read = false;
            OPL_writeReg(simulated_opl, register_address, value_for_reg);
        }
        sample_from_core1 = OPL_calc(simulated_opl);
    }
    OPL_delete(simulated_opl);
}

static void choose_sm(void) {
    used_pio = pio1;
    used_sm = pio_claim_unused_sm(used_pio, false);

#ifdef PICO_RP2350
    if (used_sm < 0 || !pio_can_add_program(used_pio, &opl2_program)) { // If no free sm or memory on PIO1, try PIO2 (not on Pico1)
        used_pio = pio2;
        used_sm = pio_claim_unused_sm(used_pio, false);
    }
#endif
}

bool load_opl2(Device *self) {
    choose_sm();
    if (used_sm < 0 || !pio_can_add_program(used_pio, &opl2_program)) { // If not any PIO with free sm and enough memory, abort
        return false;
    }

    used_offset = pio_add_program(used_pio, &opl2_program);
    if (used_offset < 0) {
        return false;
    }

    pio_sm_config used_config = opl2_program_get_default_config(used_offset);
    sm_config_set_in_pins(&used_config, LPT_BASE_PIN);
    sm_config_set_fifo_join(&used_config, PIO_FIFO_JOIN_RX);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 9; i++) { // Sets pins to use PIO
        pio_gpio_init(used_pio, i);
    }
    pio_gpio_init(used_pio, LPT_INIT_PIN);

    pio_sm_set_consecutive_pindirs(used_pio, used_sm, LPT_BASE_PIN, 9, false); // Sets pins in PIO to be inputs

    if (pio_sm_init(used_pio, used_sm, used_offset, &used_config) < 0) {
        return false;
    }

    pio_sm_set_enabled(used_pio, used_sm, true);
    stop_core1 = false;
    multicore_launch_core1(core1_operation);
    return true;
}

bool unload_opl2(Device *self) {
    pio_sm_set_enabled(used_pio, used_sm, false);
    pio_remove_program_and_unclaim_sm(&opl2_program, used_pio, used_sm, used_offset);
    stop_core1 = true;

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 9; i++) {
        gpio_deinit(i);
    }
    gpio_deinit(LPT_INIT_PIN);
    return true;
}

size_t generate_opl2(Device *self, int16_t *left_sample, int16_t *right_sample) {
    if (!pio_sm_is_rx_fifo_empty(used_pio, used_sm)) {
        uint16_t new_instruction = (pio_sm_get(used_pio, used_sm) >> 23);
        if ((new_instruction & 256) == 0) {
            register_address = new_instruction & 255;
        } else {
            value_for_reg = new_instruction & 255;
            should_read = true;
        }
    }

    *left_sample = sample_from_core1;
    *right_sample = *left_sample;
    return 0;
}

Device *create_opl2() {
    Device *opl2_struct = calloc(1, sizeof(Device));
    if (opl2_struct == NULL) {
        return NULL;
    }

    opl2_struct->load_device = load_opl2;
    opl2_struct->unload_device = unload_opl2;
    opl2_struct->generate_sample = generate_opl2;

    return opl2_struct;
}