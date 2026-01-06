#ifndef SQUARE_C_H
#define SQUARE_C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * square_c files are written by me (jansakos) as a way to minimize use of any other language except C in the main code.
 * This is therefore C API that should handle all the needed operations for simulation of Tandy and CMS with C-compatible functions.
 * Since it is just API for the square.c library, both square_c.cpp and square_c.h are also licensed under the BSD 3-clause license.
 */

/**
 * Tandy part (chip SN76486)
 */

typedef struct tandy_t tandy_t;

/**
 * @brief Creates a new Tandy sound device.
 * 
 * @return pointer to the created (and loaded) device.
 */
tandy_t *tandy_create(void);

/**
 * @brief Sends new data into Tandy.
 * @note The address is not needed for this process, since it's always the same.
 * 
 * @param tandy is a pointer to the loaded Tandy device.
 * @param data is raw data, input to the Tandy device.
 */
void tandy_write(tandy_t *tandy, uint8_t data);

/**
 * @brief Gets a sample from the Tandy device.
 * 
 * @param tandy is a pointer to the loaded Tandy device.
 *
 * @return one sample from the simulated Tandy.
 */
int32_t tandy_get_sample(tandy_t *tandy);

/**
 * @brief Destroys (unloads) given Tandy device.
 * 
 * @param tandy is a pointer to the loaded Tandy device.
 */
void tandy_destroy(tandy_t *tandy);

/**
 * CMS part (two chips SAA-1099)
 */

typedef struct cms_t cms_t;

/**
 * @brief Creates a new CMS sound device.
 * 
 * @return pointer to the created (and loaded) device.
 */
cms_t *cms_create(void);

/**
 * @brief Sends new data into CMS.
 * 
 * @param cms is a pointer to the loaded CMS device.
 * @param address is an address where data should be placed.
 * @param data is raw data, input to the CMS device.
 */
void cms_write(cms_t *cms, uint32_t address, uint8_t data);

/**
 * @brief Gets a sample from the CMS device.
 * 
 * @param cms is a pointer to the loaded CMS device.
 * @param left is a pointer where left sample should be stored.
 * @param right is a pointer where right sample should be stored.
 */
void cms_get_sample(cms_t *cms, int16_t *left, int16_t *right);

/**
 * @brief Destroys (unloads) given CMS device.
 * 
 * @param cms is a pointer to the loaded CMS device.
 */
void cms_destroy(cms_t *cms);

#ifdef __cplusplus
}
#endif

#endif // SQUARE_C_H