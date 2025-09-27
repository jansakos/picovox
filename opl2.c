#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "device.h"
#include "opl/opl.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/multicore.h"
#include "opl2.pio.h"
#include "pico/time.h"

#include "pico/stdlib.h"

#define LPT_BASE_PIN 0
#define LPT_INIT_PIN 12
#define SAMPLE_RATE 96000

// Buffer storing samples generated
#define OPL_RINGBUFFER_SIZE 4096

// Sample repeated 3 times -> for 96kHz, only 32 kHz needed (still high quality, but fast enough)
#define SAMPLE_REPEAT 2

// Variables for PIO - each device simulated has its own
static PIO used_pio;
static int8_t used_sm;
static int used_offset;

// Killswitch for core1
volatile bool stop_core1 = false;

static int16_t last_sample = 0;
static int8_t sample_used = 0;

// Ringbuffer definitions
static int16_t ringbuffer[OPL_RINGBUFFER_SIZE];
static volatile uint16_t ringbuffer_head = 0;
static volatile uint16_t ringbuffer_tail = 0;
static bool ringbuffer_full = false;

static bool ringbuffer_is_empty(void) {
    return (ringbuffer_head == ringbuffer_tail) && !ringbuffer_full;
}

static int16_t ringbuffer_pop(void) {
    if (ringbuffer_is_empty()) {
        return 0;
    }

    int16_t popped_sample = ringbuffer[ringbuffer_tail];
    ringbuffer_tail = (ringbuffer_tail + 1) & (OPL_RINGBUFFER_SIZE - 1);

    if (ringbuffer_full) {
        ringbuffer_full = false;
    }

    return popped_sample;
}

static void ringbuffer_push(int16_t sample) {
    if (ringbuffer_full) {
        return;
    }

    uint16_t next_index = (ringbuffer_head + 1) & (OPL_RINGBUFFER_SIZE - 1);
    if (next_index != ringbuffer_tail) {
        ringbuffer[ringbuffer_head] = sample;
        ringbuffer_head = next_index;
    } else {
        ringbuffer_full = true;
    }
}

static void load_new_instruction(int16_t *register_address) {
    if (pio_sm_is_rx_fifo_empty(used_pio, used_sm)) {
        return;
    }
    uint16_t new_instruction = (pio_sm_get(used_pio, used_sm) >> 23);
    if ((new_instruction & 1) == 0) {
        *register_address = (new_instruction >> 1) & 255;
    } else {
        OPL_Pico_WriteRegister(*register_address, ((new_instruction >> 1) & 255));
    }
}

static void core1_operation(void) {
    OPL_Pico_Init(0);
    int16_t current_sample = 0;
    int16_t register_address = 0;

    while (!stop_core1) {
        while ((!pio_sm_is_rx_fifo_empty(used_pio, used_sm))) {
            load_new_instruction(&register_address);
        }
        OPL_Pico_simple(&current_sample, 1);
        while (ringbuffer_full) {
            load_new_instruction(&register_address);
        }
        ringbuffer_push(current_sample << 1);
    }
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
    ringbuffer_head = 0;
    ringbuffer_tail = 0;
    ringbuffer_full = false;
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

    if (sample_used >= SAMPLE_REPEAT) {
        while (ringbuffer_is_empty()) {
            tight_loop_contents();
        }
        last_sample = ringbuffer_pop();
        sample_used = 0;
    }

    *left_sample = last_sample;
    *right_sample = last_sample;
    sample_used++;
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