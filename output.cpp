#include "output.h"

#include <avr/interrupt.h>
#include <avr/sleep.h>

#define F_CPU 16000000UL
#define CYCLES_PER_US (F_CPU/1000000UL/8UL)

namespace output {
    volatile uint8_t outPin;
    volatile uint32_t periodThr=0;
    volatile uint16_t ontimeThr=0;
    volatile uint32_t periodCount=0;
    volatile bool makePulse = false;
    volatile bool enableOut = false;

    

    void init(uint8_t pin) {
        outPin = pin;
        
        cli();
        TCCR1A = 0;
        TCCR1B = _BV(WGM12) | _BV(CS11); // divide 64 Prescaler
        TCNT1 = 0;
        OCR1A = 2000;
        OCR1B = 0;
        TIMSK1 = _BV(OCIE1A) | _BV(OCIE1B);
        sei();
        set_sleep_mode(SLEEP_MODE_IDLE);

        periodThr = 65535;
    }

    void set(uint32_t period, uint16_t ontime) {
        periodThr = period*CYCLES_PER_US;
        ontimeThr = ontime*CYCLES_PER_US;
        enableOut = ontime>0;
    }

    ISR(TIMER1_COMPA_vect) {
        if (makePulse) {
            PORTD |= _BV(PD1);
            makePulse = false; 
        }

        if (periodCount<0x10000) {
            makePulse = enableOut;
            OCR1A = periodCount;
            //OCR1B = ontimeThr;
            periodCount = periodThr;
        }
        else {
            periodCount-=0x10000;
            OCR1A = 0xFFFF;
        }
    }

    ISR(TIMER1_COMPB_vect) {
        PORTD &= ~_BV(PD1);
        OCR1B = ontimeThr;
    }
} 
