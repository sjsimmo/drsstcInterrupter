#ifndef  OUTPUT_H
#define OUTPUT_H

#include <stdint.h>

namespace output {
    void init(uint8_t pin);
    void set(uint32_t period, uint16_t ontime);
} 

#endif