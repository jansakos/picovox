#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "device.h"
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

Device *create_covox() {
    Device *covox_struct = calloc(1, sizeof(Device));
    if (covox_struct == NULL) {
        return NULL;
    }

    covox_struct->load_device = load_covox();
    covox_struct->unload_device = unload_covox();
    covox_struct->generate_sample = generate_covox();

    return covox_struct;
}