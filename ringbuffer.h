#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initializes ringbuffer with given size (elements not deleted, but head and tail reseted).
 *
 * @param size Size of the ringbuffer.
 * 
 * @return true if ringbuffer is loaded, false if not (wanted size too big).
 */
bool ringbuffer_init(size_t size);

/**
 * @brief Checks whether the ringbuffer is empty.
 *
 * @return true if ringbuffer is empty, else false.
 */
bool ringbuffer_empty();

/**
 * @brief Checks whether the ringbuffer is full.
 *
 * @return true if ringbuffer is full, else false.
 */
bool ringbuffer_full();

/**
 * @brief Pushes given data into ringbuffer.
 *
 * @param pushed_data is int16_t data that should be pushed into ringbuffer.
 * 
 * @return true if push was successful, false if not (rinbuffer full etc.).
 */
bool ringbuffer_push(int16_t pushed_data);

/**
 * @brief Pops oldest data from ringbuffer.
 *
 * @param popped_data is int16_t data that should be popped from ringbuffer.
 * 
 * @return true if pop was successful, false if not (rinbuffer empty etc.).
 */
bool ringbuffer_pop(int16_t *popped_data);

#endif // RINGBUFFER_H