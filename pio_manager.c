#include "pio_manager.h"
#include "hardware/pio.h"

const pio_interrupt_source_t irq_sources[] = { 
    pis_sm0_rx_fifo_not_empty, 
    pis_sm1_rx_fifo_not_empty, 
    pis_sm2_rx_fifo_not_empty, 
    pis_sm3_rx_fifo_not_empty
};

int pio_manager_load(PIO *pio, uint8_t *state_machine, const pio_program_t *assigned_program) {
    *pio = pio1;
    int raw_sm = pio_claim_unused_sm(*pio, false);

#ifdef PICO_RP2350
    if (raw_sm < 0 || !pio_can_add_program(*pio, assigned_program)) { // If no free sm or memory on PIO1, try PIO2 (not on Pico1)
        *pio = pio2;
        raw_sm = pio_claim_unused_sm(*pio, false);
    }
#endif
    if (raw_sm < 0 || !pio_can_add_program(*pio, assigned_program)) { // If not any PIO with free sm and enough memory, abort
        return -1;
    }

    *state_machine = raw_sm;

    return pio_add_program(*pio, assigned_program);
}

void pio_manager_unload(PIO pio, uint8_t state_machine, int offset, const pio_program_t *running_program) {
    pio_remove_program_and_unclaim_sm(running_program, pio, state_machine, offset);
}

int8_t pio_manager_get_irq(PIO pio) {
    if (pio == pio1) {
        return PIO1_IRQ_0;
    } 
    if (pio == pio0) {
        return PIO0_IRQ_0;
    }
    if (pio == pio2) {
        return PIO2_IRQ_0;
    }
}