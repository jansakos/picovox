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
     * @return true if devices is loaded, false if anything failed.
     */
    bool (*load_device)(struct Device *self);

    /**
     * @brief Method unloads all the things needed after using the device (such as the PIO program etc.)
     * 
     * @param self is a pointer to the simulated device itself.
     * 
     * @return true if devices is unloaded, false if anything failed.
     */
    bool (*unload_device)(struct Device *self);

    /**
     * @brief Function generates a sound sample based on data sent from LPT.
     * 
     * @param self is a pointer to the simulated device itself.
     * @param raw_data is a number received from the RX FIFO.
     * 
     * @return true if devices is loaded, false if anything failed.
     */
    int16_t (*generate_sample)(struct Device *self, uint32_t raw_data);
} Device;

/**
 * @brief Declarations for creating instances of all types of modes.
 * 
 * Each mode is stored in its own file, implementing all the functions of struct above.
 */
Device *create_covox();
Device *create_stereo();
Device *create_dss();
Device *create_ftl();
Device *create_opl2lpt();
Device *create_cmslpt();
Device *create_tndlpt();

#endif