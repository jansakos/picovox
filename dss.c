#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "pio_manager.h"
#include "ringbuffer.h"
#include "device.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "pico/time.h"
#include "pico/stdlib.h"
#include "dss.pio.h"

#define DSS_RINGBUFFER_SIZE 16
#define SAMPLES_REPEAT 14 // Approximation of 7kHz (sample rate is 96000 => 6,86 kHz, within specified +-5 % tolerance)

// Variables for PIO - each device simulated has its own
static PIO used_pio;
static int8_t used_sm;
static int used_offset;
static int8_t used_pio_irq;

// Definitions for repeating the sample
repeating_timer_t dss_buffer_timer;
static int16_t current_sample = 0;
static int16_t repeated_sample = 0;
static int8_t sample_used = 0;
static bool is_new_sample = true;

void __isr ringbuffer_filler(void) {
    while (!pio_sm_is_rx_fifo_empty(used_pio, used_sm)) {
        int16_t pushed_data = (((pio_sm_get(used_pio, used_sm) >> 24) & 0xFF) - 128);

        if (!ringbuffer_push(pushed_data)) {
            gpio_put(LPT_ACK_PIN, true);
        }
    }
}

static bool new_sample(repeating_timer_t *timer_for_buffer) {
    if (ringbuffer_empty()) {
        current_sample = 0;
        is_new_sample = true;
        return true;
    }

    if (ringbuffer_full()) {
        gpio_put(LPT_ACK_PIN, false);
    }

    ringbuffer_pop(&current_sample);
    current_sample = current_sample << 8;
    is_new_sample = true;
    return true;
}

bool load_dss(Device *self) {
    ringbuffer_init(DSS_RINGBUFFER_SIZE);

    used_offset = pio_manager_load(&used_pio, &used_sm, &dss_program);
    if (used_offset < 0) {
        return false;
    }

    used_pio_irq = pio_manager_get_irq(used_pio);

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
    cancel_repeating_timer(&dss_buffer_timer);
    pio_sm_set_enabled(used_pio, used_sm, false);
    pio_set_irq0_source_enabled(used_pio, irq_sources[used_sm], false);
    irq_set_enabled(used_pio_irq, false);
    irq_remove_handler(used_pio_irq, ringbuffer_filler);
    pio_manager_unload(used_pio, used_sm, used_offset, &dss_program);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) {
        gpio_deinit(i);
    }
    gpio_deinit(LPT_ACK_PIN);
    gpio_deinit(LPT_SELIN_PIN);
    return true;
}

size_t generate_dss(Device *self, int16_t *left_sample, int16_t *right_sample) {
    if (sample_used >= SAMPLES_REPEAT) {
        while(!is_new_sample) {
            tight_loop_contents();
        }
        repeated_sample = current_sample;
        sample_used = 0;
        is_new_sample = false;
    }
    *left_sample = repeated_sample;
    *right_sample = repeated_sample;
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