#include "device.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pio/covox.pio.h"

bool load_covox(Device *self) {

}

bool unload_covox(Device *self) {

}

size_t generate_covox(Device *self, uint32_t raw_data, int16_t *left_sample, int16_t *right_sample) {
    int16_t current_sample = (((raw_data >> 24) & 0xFF) - 128) << 8;
    *left_sample = current_sample;
    *right_sample = current_sample;
    return 0;
}