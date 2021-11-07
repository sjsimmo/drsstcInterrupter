/* V0-2 Interrupter Firmware

 To compile and upload:
  - avr-g++ -mmcu=atmega328p -Wall -o main.elf *.cpp *.h -O
  - avr-objcopy -j .text -j .data -O ihex main.elf main.hex
  - avrdude -c avrispmkii -p atmega328p -U flash:w:main.hex -P usb

  avr-g++ -mmcu=atmega328p -Wall -o main.elf *.cpp *.h -O; avr-objcopy -j .text -j .data -O ihex main.elf main.hex; avrdude -c avrispmkii -p atmega328p -U flash:w:main.hex -P usb
 
*/

#define F_CPU 16000000UL

#include "adc.h"
#include "output.h"
#include "midi.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
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
#define MIN_PERIOD 2000 // In uS

/* VARIABLES */
uint8_t noteVelocities[128] = {0};
volatile bool enableOut = false;
volatile uint16_t periodOut = 0xFFFF;
volatile uint16_t ontimeOut = 0;

/* MIDI TUNING CONSTANTS */
// In uS based on a4=440Hz
const uint32_t MIDI_PERIOD[128] = {122312,115447,108968,102852,97079,91631,86488,81634,77052,72727,68645,64793,61156,57724,54484,51426,48540,45815,43244,40817,38526,36364,34323,32396,30578,28862,27242,25713,24270,22908,21622,20408,19263,18182,17161,16198,15289,14431,13621,12856,12135,11454,10811,10204,9631,9091,8581,8099,7645,7215,6810,6428,6067,5727,5405,5102,4816,4545,4290,4050,3822,3608,3405,3214,3034,2863,2703,2551,2408,2273,2145,2025,1911,1804,1703,1607,1517,1432,1351,1276,1204,1136,1073,1012,956,902,851,804,758,716,676,638,602,568,536,506,478,451,426,402,379,358,338,319,301,284,268,253,239,225,213,201,190,179,169,159,150,142,134,127,119,113,106,100,95,89,84,80};
// 0-1023
const uint16_t MIDI_ONTIME[128] = {0,91,128,157,182,203,222,240,257,272,287,301,314,327,340,352,363,374,385,396,406,416,426,435,445,454,463,472,480,489,497,505,514,521,529,537,545,552,560,567,574,581,588,595,602,609,616,622,629,635,642,648,655,661,667,673,679,685,691,697,703,709,715,721,726,732,737,743,749,754,759,765,770,776,781,786,791,797,802,807,812,817,822,827,832,837,842,847,852,856,861,866,871,875,880,885,889,894,899,903,908,912,917,921,926,930,935,939,943,948,952,956,961,965,969,973,978,982,986,990,994,999,1003,1007,1011,1015,1019,1023};

/* MIDI MODE INTERRUPTER */
void midiMode() {
    // Initialise modules
    adc::init();
    output::init(OUT_PIN);
    midi::init();
    
    for(;;) {
        // Process midi messages  
        bool gotMessage = false;  
        midi::Message message = midi::getMessage();
        while (message.type != midi::NONE) {
            gotMessage = true;
            switch (message.type) {
              case midi::NOTE_ON:
                noteVelocities[message.pitch] = message.velocity;              
              break;
              case midi::NOTE_OFF:
                noteVelocities[message.pitch] = 0;
              break;
              case midi::STOP:
              case midi::RESET:
              case midi::SOUND_OFF:
              case midi::NOTES_OFF:
              case midi::CHAN_RESET:
                for(uint8_t i=0; i<128; i++) {
                    noteVelocities[i] = 0;
                }
            
              break;
              default:
               ;// TODO - Unhandled messages
            }
            message = midi::getMessage();
        }
        
        // Get Knob Positions
        uint16_t velocity = adc::get(VELOCITY_PIN);
        uint16_t pitch = adc::get(PITCH_PIN);

        // Transpose
        uint8_t transpose;
        if (pitch < 256) {
            transpose = 8;
        } else if (pitch < 512) {
             transpose = 4;
        } else if (pitch < 768) {
             transpose = 2;
        } else {
             transpose = 1;
        }

        // Find melody frequency
        int8_t upperPitch = 127;
        while ((noteVelocities[upperPitch]==0 || MIDI_PERIOD[upperPitch]*transpose < MIN_PERIOD) && upperPitch>=0 ) {
            upperPitch--;
        }
        enableOut = velocity>0 && upperPitch >= 0;
        
        // Set output
        if (enableOut)
        output::set(MIDI_PERIOD[upperPitch]*transpose, ((uint32_t)MIDI_ONTIME[noteVelocities[upperPitch]]*velocity*MAX_ONTIME)/1023/1023);
        else 
        output::set(1000,0);
    }
}


/* MANUAL MODE INTERRUPTER */
void manualMode() {
    adc::init();
    output::init(OUT_PIN);

    for(;;) {
        _delay_ms(10);
        
        uint32_t velocity = adc::get(VELOCITY_PIN);
        uint32_t pitch = adc::get(PITCH_PIN);

        output::set(MIN_PERIOD*1025UL/(pitch+2), MAX_ONTIME*velocity/1023UL);
    }
}

/* PROGRAM ENTRY POINT */
int main() {
    // Configure Pins
    DDRD = _BV(OUT_PIN) | _BV(MODE_LED_PIN); 

    // Enter Routine
    _delay_ms(10);
    if (PINC & _BV(MODE_SET_PIN)) {
        PORTD |= _BV(MODE_LED_PIN);
        midiMode();
    } 
    else {
        manualMode(); 
    }
}
