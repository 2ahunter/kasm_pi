#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* --- DAC80508 Register Map --- */
#define DAC80508_REG_NOP           0x00
#define DAC80508_REG_DEVICE_ID     0x01
#define DAC80508_REG_SYNC          0x02
#define DAC80508_REG_CONFIG        0x03
#define DAC80508_REG_GAIN          0x04
#define DAC80508_REG_TRIGGER       0x05
#define DAC80508_REG_BROADCAST     0x06
#define DAC80508_REG_STATUS        0x07
#define DAC80508_REG_DAC0          0x08
#define DAC80508_REG_DAC1          0x09
#define DAC80508_REG_DAC2          0x0A
#define DAC80508_REG_DAC3          0x0B
#define DAC80508_REG_DAC4          0x0C
#define DAC80508_REG_DAC5          0x0D
#define DAC80508_REG_DAC6          0x0E
#define DAC80508_REG_DAC7          0x0F
#define NUM_CHANNELS 8
#define PROTOCOL_VERSION 0
#define PROTOCOL_END_1 0xDE
#define PROTOCOL_END_2 0xAD
#define DAC_ZERO_CODE 65535/2 // Midpoint code for 16-bit DAC (0-65535)

typedef struct frame {
    uint8_t reg;      // DAC Register address
    uint16_t data;    // Command data 16 bits (0-65535)
} frame_t;

typedef struct command{
    uint8_t version;   // Protocol version
    uint32_t timestamp; // Timestamp in microseconds since app start
    frame_t frame[NUM_CHANNELS];
    uint8_t end_1;
    uint8_t end_2;
} command_t;


#endif //PROTOCOL_H