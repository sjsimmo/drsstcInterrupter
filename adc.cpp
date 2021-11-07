#include "adc.h"

#include <avr/io.h>

namespace adc {
    void init() {
        // Configure ADC
        ADCSRA = _BV(ADEN) | _BV(ADPS0) | _BV(ADPS1) | _BV(ADPS2);
    }

    uint16_t get(uint8_t pin) {
        ADMUX = _BV(REFS0) | pin; // Select ADC
        ADCSRA |= _BV(ADSC); // Start conversion
        loop_until_bit_is_clear(ADCSRA, ADSC);
        return ADC; // Get result
    }
} 