#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "pio_manager.h"
#include "ringbuffer.h"
#include "device.h"
#include "square/square_c.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "cms.pio.h"

#define CMS_RINGBUFFER_SIZE 2048

// Variables for PIO - each device simulated has its own
static PIO first_pio;
static int8_t first_sm;
static int first_offset;

static PIO second_pio;
static int8_t second_sm;
static int second_offset;

// Killswitch for core1
static volatile bool stop_core1 = false;

static void write_to_chip(gameblaster_t *device, int16_t current_instruction, bool first) {
    uint32_t address = 0x220;
    uint8_t data = 0;
    if (!first) {
        address += 0x2;
    }

#if LPT_STROBE_SWAPPED
    if ((current_instruction & 1) == 1) {
        address += 0x1;
        data = (current_instruction >> 1) & 255;
    } else {
        data = (current_instruction >> 1) & 255;
    }
#else
    if (((current_instruction >> 8) & 1) == 1) {
        address += 0x1;
        data = (current_instruction) & 255;
    } else {
        data = (current_instruction) & 255;
    }
#endif

    gameblaster_write(device, address, data);
}

static void load_new_instruction(gameblaster_t *device) {
    if (!pio_sm_is_rx_fifo_empty(first_pio, first_sm)) {
        write_to_chip(device, pio_sm_get(first_pio, first_sm) >> 23, true);
    }
    if (!pio_sm_is_rx_fifo_empty(second_pio, second_sm)) {
        write_to_chip(device, pio_sm_get(second_pio, second_sm) >> 23, false);
    }
}

static void core1_operation(void) {
    gameblaster_t *device = gameblaster_create();
    int32_t current_left_sample = 0;
    int32_t current_right_sample = 0;
    int16_t register_address = 0;

    while (!stop_core1) {
        while ((!pio_sm_is_rx_fifo_empty(first_pio, first_sm)) || (!pio_sm_is_rx_fifo_empty(second_pio, second_sm))) {
            load_new_instruction(device);
        }
        gameblaster_get_sample(device, &current_left_sample, &current_right_sample);
        while (ringbuffer_full() && !stop_core1) {
            load_new_instruction(device);
        }
        ringbuffer_push(current_left_sample);
        ringbuffer_push(current_right_sample);
    }
    gameblaster_destroy(device);
}

bool load_cms(Device *self) {

    ringbuffer_init(CMS_RINGBUFFER_SIZE);

    first_offset = pio_manager_load(&first_pio, &first_sm, &cms_one_program);
    if (first_offset < 0) {
        return false;
    }

    second_offset = pio_manager_load(&second_pio, &second_sm, &cms_two_program);
    if (second_offset < 0) {
        return false;
    }

    pio_sm_config first_config = cms_one_program_get_default_config(first_offset);
#if LPT_STROBE_SWAPPED
    sm_config_set_in_pins(&first_config, LPT_STROBE_PIN);
#else
    sm_config_set_in_pins(&first_config, LPT_BASE_PIN);
#endif
    sm_config_set_fifo_join(&first_config, PIO_FIFO_JOIN_RX);
    sm_config_set_jmp_pin(&first_config, LPT_AUTOFEED_PIN);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) { // Sets pins to use PIO
        pio_gpio_init(first_pio, i);
    }

    pio_gpio_init(first_pio, LPT_STROBE_PIN);
    pio_gpio_init(first_pio, LPT_INIT_PIN);
    pio_gpio_init(first_pio, LPT_AUTOFEED_PIN);

#if LPT_STROBE_SWAPPED
    pio_sm_set_consecutive_pindirs(first_pio, first_sm, LPT_STROBE_PIN, 9, false); // Sets pins in PIO to be inputs
#else
    pio_sm_set_consecutive_pindirs(first_pio, first_sm, LPT_BASE_PIN, 9, false); // Sets pins in PIO to be inputs
#endif

    if (pio_sm_init(first_pio, first_sm, first_offset, &first_config) < 0) {
        return false;
    }

    pio_sm_set_enabled(first_pio, first_sm, true);

    pio_sm_config second_config = cms_two_program_get_default_config(second_offset);
#if LPT_STROBE_SWAPPED
    sm_config_set_in_pins(&second_config, LPT_STROBE_PIN);
#else
    sm_config_set_in_pins(&second_config, LPT_BASE_PIN);
#endif
    sm_config_set_fifo_join(&second_config, PIO_FIFO_JOIN_RX);
    sm_config_set_jmp_pin(&second_config, LPT_SELIN_PIN);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) { // Sets pins to use PIO
        pio_gpio_init(second_pio, i);
    }

    pio_gpio_init(second_pio, LPT_STROBE_PIN);
    pio_gpio_init(second_pio, LPT_INIT_PIN);
    pio_gpio_init(second_pio, LPT_SELIN_PIN);

#if LPT_STROBE_SWAPPED
    pio_sm_set_consecutive_pindirs(second_pio, second_sm, LPT_STROBE_PIN, 9, false); // Sets pins in PIO to be inputs
#else
    pio_sm_set_consecutive_pindirs(second_pio, second_sm, LPT_BASE_PIN, 9, false); // Sets pins in PIO to be inputs
#endif

    if (pio_sm_init(second_pio, second_sm, second_offset, &second_config) < 0) {
        return false;
    }

    pio_sm_set_enabled(second_pio, second_sm, true);

    stop_core1 = false;
    multicore_reset_core1();
    multicore_launch_core1(core1_operation);
    return true;
}

bool unload_cms(Device *self) {
    stop_core1 = true;
    pio_sm_set_enabled(first_pio, first_sm, false);
    pio_manager_unload(first_pio, first_sm, first_offset, &cms_one_program);

    pio_sm_set_enabled(second_pio, second_sm, false);
    pio_manager_unload(second_pio, second_sm, second_offset, &cms_two_program);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) {
        gpio_deinit(i);
    }
    gpio_deinit(LPT_STROBE_PIN);
    gpio_deinit(LPT_AUTOFEED_PIN);
    gpio_deinit(LPT_INIT_PIN);
    gpio_deinit(LPT_SELIN_PIN);

    return true;
}

size_t generate_cms(Device *self, int16_t *left_sample, int16_t *right_sample) {
    while (ringbuffer_empty()) {
        tight_loop_contents();
    }
    if (!ringbuffer_pop(left_sample)) {
        *left_sample = 0;
    }
    if (!ringbuffer_pop(right_sample)) {
        *right_sample = 0;
    }

    return 0;
}

Device *create_cms() {
    Device *cms_struct = calloc(1, sizeof(Device));
    if (cms_struct == NULL) {
        return NULL;
    }

    cms_struct->load_device = load_cms;
    cms_struct->unload_device = unload_cms;
    cms_struct->generate_sample = generate_cms;

    return cms_struct;
}