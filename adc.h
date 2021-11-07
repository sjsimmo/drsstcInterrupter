#ifndef  ADC_H
#define ADC_H

#include <stdint.h>

namespace adc {
    void init();
    uint16_t get(uint8_t);
} 

#endif