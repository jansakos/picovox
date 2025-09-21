#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Common interface for all the simulated devices.
 */
typedef struct Device {

    /**
     * @brief Method loads all the things needed for using the device (such as start PIO program, initialize registers etc.)
     * 
     * @param self is a pointer to the simulated device itself.
     * 
     * @return true if device is loaded, false if anything failed.
     */
    bool (*load_device)(struct Device *self);

    /**
     * @brief Method unloads all the things needed after using the device (such as the PIO program etc.)
     * 
     * @param self is a pointer to the simulated device itself.
     * 
     * @return true if device is unloaded, false if anything failed.
     */
    bool (*unload_device)(struct Device *self);

    /**
     * @brief Function reads data from PIO buffer and generates a sound sample based on data sent.
     * 
     * @param self is a pointer to the simulated device itself.
     * @param left_sample is a pointer to number where sample for left channel is placed.
     * @param right_sample is a pointer to number where sample for right channel is placed.
     * 
     * @return number of samples available in internal device buffer.
     */
    size_t (*generate_sample)(struct Device *self, int16_t *left_sample, int16_t *right_sample);
} Device;

/**
 * @brief Declarations for creating instances of all types of modes.
 * 
 * Each mode is stored in its own file, implementing all the functions of struct above.
 * Always check if return value is not NULL (allocation failed)!
 */
Device *create_covox();
Device *create_stereo();
Device *create_ftl();
/*Device *create_dss();
Device *create_opl2lpt();
Device *create_cmslpt();
Device *create_tndlpt();*/

#endif