#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "device.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "dss.pio.h"

#define LPT_BASE_PIN 1
#define LPT_SELIN_PIN 11
#define LPT_ACK_PIN 9
#define SAMPLE_RATE 96000

#define RINGBUFFER_SIZE 16

pio_irq_source_t irq_sources[] = { 
    pis_sm0_rx_fifo_not_empty, 
    pis_sm1_rx_fifo_not_empty, 
    pis_sm2_rx_fifo_not_empty, 
    pis_sm3_rx_fifo_not_empty};

// Variables for PIO - each device simulated has its own
static PIO used_pio;
static uint8_t used_sm;
static uint used_offset;
static uint8_t used_pio_irq;

static uint32_t ringbuffer[RINGBUFFER_SIZE];
static volatile uint8_t ringbuffer_head = 0;
static volatile uint8_t ringbuffer_tail = 0;
static bool ringbuffer_full = false;

void __isr ringbuffer_filler(void) {
    while (!pio_sm_is_rx_fifo_empty(used_pio, used_sm)) {
        uint32_t given_data = pio_sm_get(used_pio, used_sm);

        uint8_t next_index = (ringbuffer_head + 1) & (RINGBUFFER_SIZE - 1);
        if (next_index != ringbuffer_tail) {
            ringbuffer[ringbuffer_head] = given_data;
            ringbuffer_head = next_index;
        }

        if (((ringbuffer_head + 1) & (RINGBUFFER_SIZE - 1)) == ringbuffer_tail) {
            gpio_put(LPT_ACK_PIN, 1);
            ringbuffer_full = true;
        } elif (ringbuffer_full) {
            gpio_put(LPT_ACK_PIN, 0);
            ringbuffer_full = false;
        }
    }
}

static void choose_sm(void) {
    used_pio = pio1;
    used_sm = pio_claim_unused_sm(used_pio, false);
    used_pio_irq = PIO1_IRQ_0;

#ifdef PICO_RP2350
    if (used_sm < 0 || !pio_can_add_program(used_pio, &dss_program)) { // If no free sm or memory on PIO1, try PIO2 (not on Pico1)
        used_pio = pio2;
        used_sm = pio_claim_unused_sm(used_pio, false);
        used_pio_irq = PIO2_IRQ_0;
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
    sm_config_set_jmp_pin(&used_config, LPT_SELIN_PIN);
    sm_config_set_clkdiv(&used_config, 10.0);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) { // Sets pins to use PIO
        pio_gpio_init(used_pio, i);
    }
    pio_gpio_init(used_pio, LPT_SELIN_PIN);
    gpio_init(LPT_ACK_PIN);
    gpio_set_dir(LPT_ACK_PIN, true);

    pio_sm_set_consecutive_pindirs(used_pio, used_sm, LPT_BASE_PIN, 8, false); // Sets pins in PIO to be inputs

    irq_set_exclusive_handler(used_pio_irq, ringbuffer_filler);
    irq_set_enabled(used_pio_irq, true);

    pio_set_irq0_source_enabled(used_pio, irq_sources[used_sm], true);

    if (pio_sm_init(used_pio, used_sm, used_offset, &used_config) < 0) {
        return false;
    }

    pio_sm_set_enabled(used_pio, used_sm, true);
    return true;
}

bool unload_dss(Device *self) {
    pio_set_irq0_source_enabled(used_pio, irq_sources[used_sm], false);
    irq_set_enabled(used_pio_irq, false);
    irq_set_exclusive_handler(used_pio_irq, NULL);
    pio_sm_set_enabled(used_pio, used_sm, false);
    pio_remove_program_and_unclaim_sm(&dss_program, used_pio, used_sm, used_offset);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) {
        gpio_deinit(i);
    }
    gpio_deinit(LPT_ACK_PIN);
    gpio_deinit(LPT_SELIN_PIN);
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