// Definitions for I2S library
#define PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE 26
#define PICO_AUDIO_I2S_DATA_PIN 28
#define PICO_AUDIO_PIO 0
#define PICO_AUDIO_DMA_IRQ 1
#define SAMPLES_PER_BUFFER 512
#define NUM_BUFFERS 8

// Definitions for PIO programs
#define LPT_STROBE_PIN 0
#define LPT_DATA_BASE 1
#define SAMPLE_RATE 44100

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pio/covox.pio.h"
#include "pico/audio_i2s.h"

typedef enum {
    COVOX, 
    STEREO, 
    DSS, 
    FTL, 
    OPL2LPT, 
    CMSLPT, 
    TNDLPT, 
    COUNT // COUNT is used for checking count of devices available
} simulated_device;

int main()
{
    stdio_init_all();

    
}
