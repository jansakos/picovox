#ifndef CONFIG_H
#define CONFIG_H

#define SAMPLE_RATE 96000

// Definitions of GPIO LPT pins
#define LPT_STROBE_PIN 0
#define LPT_BASE_PIN 1
#define LPT_ACK_PIN 9
#define LPT_PAPEREND_PIN 10
#define LPT_SELIN_PIN 11
#define LPT_INIT_PIN 12

// Definitions of GPIO I2S pins
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE 26
#define PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0
#define PICO_AUDIO_I2S_DATA_PIN 28

// Definitions of GPIO mode switch button (GPIO - ground)
#define CHANGE_BUTTON_PIN 17

#endif