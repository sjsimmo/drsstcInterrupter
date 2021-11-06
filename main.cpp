/* V1-1 Interrupter Firmware

 To compile and upload:
  - avr-g++ -mmcu=atmega328p -Wall -o main.elf main.cpp midi.cpp midi.h -O
  - avr-objcopy -j .text -j .data -O ihex main.elf main.hex
  - avrdude -c avrispmkii -p atmega328p -U flash:w:main.hex -P usb
 
*/

#define F_CPU 16000000UL

#include "midi.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <math.h>
#include <avr/sleep.h>

/* ATMEGA328P PINOUT */
#define RX_PIN PD0
#define OUT_PIN PD1
#define MODE_LED_PIN PD2 
#define MODE_SET_PIN PC0
#define VELOCITY_PIN PC1
#define PITCH_PIN PC2

/* SETTINGS */
#define MAX_ONTIME 200 // In uS
#define MAX_FREQ 500 // In Hz

/* VARIABLES */
uint8_t noteVelocities[128] = {0};
double transpose = 0; // number of octives to transpose up by
volatile bool enableOut = false;

/* ADC HELPER FUNCTIONS */
void initAdc() {
    // Configure ADC
    ADCSRA = _BV(ADEN) | _BV(ADPS0) | _BV(ADPS1) | _BV(ADPS2);
}

uint16_t readAdc(uint8_t pin) {
    ADMUX = _BV(REFS0) | pin; // Select ADC
    ADCSRA |= _BV(ADSC); // Start conversion
    loop_until_bit_is_clear(ADCSRA, ADSC);
    return ADC; // Get result
}

/* MIDI MODE INTERRUPTER */
void midiMode() {
    initAdc();
    // Start Midi Communication
    Midi::init();

    // Configure Timers for output setting
    cli();
    TCCR1A = 0;
    TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10); // divide 64 Prescaler
    TCNT1 = 0;
    sei();
    set_sleep_mode(SLEEP_MODE_IDLE);

    

    for(;;) {
        // Process midi messages  
        bool gotMessage = false;  
        Midi::Message message = Midi::getMessage();
        while (message.type != Midi::NONE) {
            gotMessage = true;
            switch (message.type) {
              case Midi::NOTE_ON:
                if (message.channel==0) {
                    noteVelocities[message.pitch] = message.velocity;
                }                
              break;
              case Midi::NOTE_OFF:
                if (message.channel==0) {
                    noteVelocities[message.pitch] = 0;
                } 
              break;
              case Midi::STOP:
              case Midi::RESET:
              case Midi::SOUND_OFF:
              case Midi::NOTES_OFF:
              case Midi:: CHAN_RESET:
                for(uint8_t i=0; i<128; i++) {
                    noteVelocities[i] = 0;
                }
            
              break;
              default:
               ;// TODO - Unhandled messages
            }
            message = Midi::getMessage();
        }
        
        // Get Knob Positions
        uint16_t velocity = readAdc(VELOCITY_PIN);
        uint16_t pitch = readAdc(PITCH_PIN);

        // Transpose
        int a4Frequency;
        if (pitch < 256) {
            a4Frequency = 55;
        } else if (pitch < 512) {
            a4Frequency = 110;
        } else if (pitch < 768) {
            a4Frequency = 220;
        } else {
            a4Frequency = 440;
        }

        // Find melody frequency
        int8_t upperPitch = 127;
        while (noteVelocities[upperPitch]==0 && upperPitch>=0) {
            upperPitch--;
        }
        enableOut = velocity>50 && upperPitch >= 0 && pow(2.0, (upperPitch-69)/12.0)*a4Frequency - 1 <= MAX_FREQ;
        
        if (enableOut) {
            OCR1A = F_CPU/64/(pow(2.0, (upperPitch-69)/12.0)*a4Frequency) - 1;
            OCR1B = sqrt(noteVelocities[upperPitch]/127.0)*MAX_ONTIME*velocity/973.0/(64*1000000/F_CPU);
            TIMSK1 = _BV(OCIE1A) | _BV(OCIE1B);
            if(TCNT1>OCR1A) {
                TCNT1 = 0;
                PORTD |= _BV(OUT_PIN);
            }
        }  
    }
}

/* MANUAL MODE INTERRUPTER */
void manualMode() {
    initAdc();
    // Configure Timers for output setting
    cli();
    TCCR1A = 0;
    TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10); // divide 64 Prescaler
    TCNT1 = 0;
    sei();
    set_sleep_mode(SLEEP_MODE_IDLE);

    for(;;) {
        _delay_ms(100);
        
        uint16_t velocity = readAdc(VELOCITY_PIN);
        uint16_t pitch = readAdc(PITCH_PIN);

        enableOut = velocity>50;
        if (enableOut) {
            enableOut = true;
            OCR1A = F_CPU/64/(pitch/1023.0*MAX_FREQ) - 1;
            OCR1B = (velocity-50)/973.0*MAX_ONTIME/(64*1000000/F_CPU);
            TIMSK1 = _BV(OCIE1A) | _BV(OCIE1B);
            if(TCNT1>OCR1A) {
                TCNT1 = 0;
                PORTD |= _BV(OUT_PIN);
            }
        }
    }
}

/* ISR ROUTINES */
ISR(TIMER1_COMPB_vect) {
    PORTD &= ~_BV(OUT_PIN);
    if (!enableOut) TIMSK1 = 0;
    //TODO - Here would be a good place to set OCR1A, OCR1B...
}

ISR(TIMER1_COMPA_vect) {
    PORTD |= _BV(OUT_PIN);     
}


int main() {
    // Configure Pins
    DDRD = _BV(OUT_PIN) | _BV(MODE_LED_PIN); 

    // Enter Routine
    _delay_ms(100);
    if (PINC & _BV(MODE_SET_PIN)) {
        PORTD |= _BV(MODE_LED_PIN);
        midiMode();
    } 
    else {
        manualMode(); 
    }
}