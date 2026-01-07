#ifndef PIO_MANAGER_H
#define PIO_MANAGER_H

#include <stdint.h>
#include "hardware/pio.h"

extern const pio_interrupt_source_t irq_sources[4];

/**
 * @brief Loads PIO program into free PIO block and sets PIO, SM and offset.
 *
 * @param pio Pointer to set given PIO.
 * @param state_machine Pointer to set given state machine.
 * @param assigned_program Pointer to program that should be loaded.
 *
 * @return offset of the loaded program (>=0), negative number if error.
 */
int pio_manager_load(PIO *pio, uint8_t *state_machine, const pio_program_t *assigned_program);

/**
 * @brief Unloads PIO program from given PIO.
 *
 * @param pio PIO where program is running.
 * @param state_machine State machine where program is running.
 * @param offset Offset of the running program.
 * @param assigned_program Program that should be unloaded.
 */
void pio_manager_unload(PIO pio, uint8_t state_machine, int offset, const pio_program_t *running_program);

/**
 * @brief Returns correct IRQ based on PIO.
 *
 * @param pio PIO where program is running.
 *
 * @return irq constant of the given PIO.
 */
int8_t pio_manager_get_irq(PIO pio);

#endif // PIO_MANAGER_H