#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "device.h"
#include "pio_manager.h"
#include "ringbuffer.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "pico/time.h"
#include "stereo.pio.h"

#define STEREO_RINGBUFFER_SIZE 2048

// Variables for PIO - each device simulated has its own
static PIO sound_left_pio;
static int8_t sound_left_sm;
static int sound_left_offset;

static PIO sound_right_pio;
static int8_t sound_right_sm;
static int sound_right_offset;

static PIO detection_pio;
static int8_t detection_sm;
static int detection_offset;

static int16_t last_left_sample = 0;
static int16_t last_right_sample = 0;
static uint pwm_slice;

static void get_samples(void) {
    pwm_clear_irq(pwm_slice);
    if (!pio_sm_is_rx_fifo_empty(sound_left_pio, sound_left_sm)) {
        last_left_sample = (((pio_sm_get(sound_left_pio, sound_left_sm) >> 24) & 0xFF) - 128) << 8;
    }
    if (!pio_sm_is_rx_fifo_empty(sound_right_pio, sound_right_sm)) {
        last_right_sample = (((pio_sm_get(sound_right_pio, sound_right_sm) >> 24) & 0xFF) - 128) << 8;
    }
    ringbuffer_push(last_left_sample);
    ringbuffer_push(last_right_sample);
}

bool load_stereo(Device *self) {
    ringbuffer_init(STEREO_RINGBUFFER_SIZE);

    sound_left_offset = pio_manager_load(&sound_left_pio, &sound_left_sm, &stereo_left_program);
    if (sound_left_offset < 0) {
        return false;
    }

    sound_right_offset = pio_manager_load(&sound_right_pio, &sound_right_sm, &stereo_right_program);
    if (sound_right_offset < 0) {
        return false;
    }

    detection_offset = pio_manager_load(&detection_pio, &detection_sm, &stereo_detection_program);
    if (detection_offset < 0) {
        return false;
    }

    pio_sm_config used_config_left = stereo_left_program_get_default_config(sound_left_offset);
    sm_config_set_in_pins(&used_config_left, LPT_BASE_PIN);
    sm_config_set_fifo_join(&used_config_left, PIO_FIFO_JOIN_RX);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) { // Sets pins to use PIO
        pio_gpio_init(sound_left_pio, i);
    }

    pio_gpio_init(sound_left_pio, LPT_AUTOFEED_PIN);

    pio_sm_set_consecutive_pindirs(sound_left_pio, sound_left_sm, LPT_BASE_PIN, 8, false); // Sets pins in PIO to be inputs

    if (pio_sm_init(sound_left_pio, sound_left_sm, sound_left_offset, &used_config_left) < 0) {
        return false;
    }

    pio_sm_set_enabled(sound_left_pio, sound_left_sm, true);

    pio_sm_config used_config_right = stereo_right_program_get_default_config(sound_right_offset);
    sm_config_set_in_pins(&used_config_right, LPT_BASE_PIN);
    sm_config_set_fifo_join(&used_config_right, PIO_FIFO_JOIN_RX);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) { // Sets pins to use PIO
        pio_gpio_init(sound_right_pio, i);
    }

    pio_gpio_init(sound_right_pio, LPT_STROBE_PIN);

    pio_sm_set_consecutive_pindirs(sound_right_pio, sound_right_sm, LPT_BASE_PIN, 8, false); // Sets pins in PIO to be inputs

    if (pio_sm_init(sound_right_pio, sound_right_sm, sound_right_offset, &used_config_right) < 0) {
        return false;
    }

    pio_sm_set_enabled(sound_right_pio, sound_right_sm, true);

    pio_sm_config used_config_detection = stereo_detection_program_get_default_config(detection_offset);
    sm_config_set_in_pins(&used_config_detection, LPT_BASE_PIN + 7);
    sm_config_set_out_pins(&used_config_detection, LPT_BUSY_PIN, 1);

    pio_gpio_init(detection_pio, LPT_BASE_PIN + 7);
    pio_gpio_init(detection_pio, LPT_BUSY_PIN);

    pio_sm_set_consecutive_pindirs(detection_pio, detection_sm, LPT_BASE_PIN + 7, 1, false);
    pio_sm_set_consecutive_pindirs(detection_pio, detection_sm, LPT_BUSY_PIN, 1, true);

    if (pio_sm_init(detection_pio, detection_sm, detection_offset, &used_config_detection) < 0) {
        return false;
    }

    pio_sm_set_enabled(detection_pio, detection_sm, true);

    gpio_set_function(PICO_UNUSED_PIN, GPIO_FUNC_PWM);
    pwm_slice = pwm_gpio_to_slice_num(PICO_UNUSED_PIN);
    uint16_t top = (clock_get_hz(clk_sys) + SAMPLE_RATE / 2) / SAMPLE_RATE - 1;
    pwm_set_clkdiv(pwm_slice, 1.0f);
    pwm_set_wrap(pwm_slice, top);
    pwm_set_gpio_level(PICO_UNUSED_PIN, top/2);
    pwm_clear_irq(pwm_slice);
    pwm_set_irq_enabled(pwm_slice, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, get_samples);
    irq_set_enabled(PWM_IRQ_WRAP, true);
    pwm_set_enabled(pwm_slice, true);

    return true;
}

bool unload_stereo(Device *self) {
    pwm_set_enabled(pwm_slice, false);
    irq_set_enabled(PWM_IRQ_WRAP, false);
    irq_remove_handler(PWM_IRQ_WRAP, get_samples);

    pio_sm_set_enabled(sound_left_pio, sound_left_sm, false);
    pio_sm_set_enabled(sound_right_pio, sound_right_sm, false);
    pio_sm_set_enabled(detection_pio, detection_sm, false);
    pio_manager_unload(sound_left_pio, sound_left_sm, sound_left_offset, &stereo_left_program);
    pio_manager_unload(sound_right_pio, sound_right_sm, sound_right_offset, &stereo_right_program);
    pio_manager_unload(detection_pio, detection_sm, detection_offset, &stereo_detection_program);


    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) {
        gpio_deinit(i);
    }
    gpio_deinit(LPT_STROBE_PIN);
    gpio_deinit(LPT_AUTOFEED_PIN);
    gpio_deinit(LPT_BUSY_PIN);
    return true;
}

size_t generate_stereo(Device *self, int16_t *left_sample, int16_t *right_sample) {  
    while (!ringbuffer_pop(left_sample)) {
        tight_loop_contents();
    }
    ringbuffer_pop(right_sample);
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