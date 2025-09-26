#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "device.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "pico/time.h"
#include "pico/stdlib.h"
#include "dss.pio.h"

#define LPT_BASE_PIN 1
#define LPT_SELIN_PIN 11
#define LPT_ACK_PIN 9
#define SAMPLE_RATE 96000

#define RINGBUFFER_SIZE 16

#define SAMPLES_REPEAT 14 // Approximation of 7kHz - frequency of undestructive reading

pio_interrupt_source_t irq_sources[] = { 
    pis_sm0_rx_fifo_not_empty, 
    pis_sm1_rx_fifo_not_empty, 
    pis_sm2_rx_fifo_not_empty, 
    pis_sm3_rx_fifo_not_empty};

// Variables for PIO - each device simulated has its own
static PIO used_pio;
static int8_t used_sm;
static int used_offset;
static int8_t used_pio_irq;

// Definitions for repeating the sample
repeating_timer_t dss_buffer_timer;
static int16_t current_sample = 0;
int8_t sample_used = 0;
uint32_t raw_sample = 0;
bool need_faster = true;

// Ringbuffer definitions
static uint32_t ringbuffer[RINGBUFFER_SIZE];
static volatile uint8_t ringbuffer_head = 0;
static volatile uint8_t ringbuffer_tail = 0;
static volatile uint8_t ringbuffer_reader = 0;
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
        }
    }
}

bool ringbuffer_is_empty(void) {
    return (ringbuffer_head == ringbuffer_tail) && !ringbuffer_full;
}

uint32_t ringbuffer_pop(void) {
    if (ringbuffer_is_empty()) {
        return 2147483648;
    }
    uint32_t popped_sample = ringbuffer[ringbuffer_tail];
    ringbuffer_tail = (ringbuffer_tail + 1) & (RINGBUFFER_SIZE - 1);

    if (ringbuffer_full) {
        ringbuffer_full = false;
        gpio_put(LPT_ACK_PIN, 0);
    }

    return popped_sample;
}

bool ringbuffer_read(uint32_t *sample) {
    if (ringbuffer_is_empty()) {
        sample_used = 0;
        *sample = 2147483648;
        return true;
    }

    if (ringbuffer_reader == ringbuffer_head) {
        need_faster = false;
        return false;
    }

    if (ringbuffer_reader == ringbuffer_tail) {
        need_faster = true;
    }

    *sample = ringbuffer[ringbuffer_reader];
    ringbuffer_reader = (ringbuffer_reader + 1) & (RINGBUFFER_SIZE - 1);
    sample_used = 0;
    return true;
}

bool new_sample(repeating_timer_t *timer_for_buffer) {
    current_sample = (((ringbuffer_pop() >> 24) & 0xFF) - 128) << 8;
    return true;
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

    add_repeating_timer_us(1000000 / 7000, new_sample, NULL, &dss_buffer_timer);
    return true;
}

bool unload_dss(Device *self) {
    pio_sm_set_enabled(used_pio, used_sm, false);
    pio_set_irq0_source_enabled(used_pio, irq_sources[used_sm], false);
    irq_set_enabled(used_pio_irq, false);
    irq_remove_handler(used_pio_irq, ringbuffer_filler);
    pio_remove_program_and_unclaim_sm(&dss_program, used_pio, used_sm, used_offset);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) {
        gpio_deinit(i);
    }
    gpio_deinit(LPT_ACK_PIN);
    gpio_deinit(LPT_SELIN_PIN);
    return true;
}

size_t generate_dss(Device *self, int16_t *left_sample, int16_t *right_sample) {
    if (sample_used >= SAMPLES_REPEAT || (need_faster && sample_used >= SAMPLES_REPEAT - 1)) {
        if (ringbuffer_read(&raw_sample)) {
            current_sample = (((raw_sample >> 24) & 0xFF) - 128) << 8;
        } else {
            sleep_us(950); // Ugly timing correction - probably some calculation should be made for it to be precise
        }
    }

    *left_sample = current_sample;
    *right_sample = current_sample;
    sample_used++;
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