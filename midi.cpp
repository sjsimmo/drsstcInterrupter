#include "midi.h"

#define F_CPU 16000000UL
#define BAUD 31250UL

#include <util/setbaud.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>

namespace Midi 
{

    /* MIDI BYTE RANGES */
    #define DATA_B     0b00000000 // Minimum value for a data byte
    #define DATA_T     0b01111111 // Maximum value for a data byte
    #define VOICE_B    0b10000000 // Minimum value for a channel voice byte
    #define VOICE_T    0b11101111 // Maximum value for a channel voice byte
    #define MODE_B     0b10110000 // Value for a channel mode byte
    #define MODE_T     0b10111111 // Value for a channel mode byte
    #define COMMON_B   0b11110000 // Minimum value for a system common byte
    #define COMMON_T   0b11110111 // Maximum value for a system common byte
    #define REALTIME_B 0b11111000 // Minimum value for a system realtime byte
    #define REALTIME_T 0b11111111 // Maximum value for a system realtime byte

    #define NOTE_OFF_B 0b10000000 // Minimum value for a note off byte
    #define NOTE_OFF_T 0b10001111 // Maximum value for a note off byte
    #define NOTE_ON_B  0b10010000 // Minimum value for a note on byte
    #define NOTE_ON_T  0b10011111 // Maximum value for a note on byte

    #define STOP_BT    0b11111100
    #define RESET_BT   0b11111111

    uint8_t runningStatus = 0b11111111;
    volatile uint8_t serialBuffer[256];
    volatile uint8_t serialBufferHead = 0, serialBufferTail = 0;

    void init() {
        // Set Baud Rate
        UBRR0H = UBRRH_VALUE;
        UBRR0L = UBRRL_VALUE;
        #if USE_2X
            UCSR0A |= _BV(U2X0);
        #else 
            UCSR0A &= ~(_BV(U2X0));
        #endif
        // 8-bit data format
        UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
        // Enable RX with Interrupt 
        UCSR0B = _BV(RXEN0) | _BV(RXCIE0);
        set_sleep_mode(SLEEP_MODE_IDLE);
        sei();   
    }

    ISR(USART_RX_vect) {
        //Put message in buffer
        serialBuffer[serialBufferHead++] = UDR0; 
    }

    uint8_t readByte() {
        return serialBuffer[serialBufferTail++];
    }

    uint8_t peekByte() {
        return serialBuffer[serialBufferTail];
    }

    uint8_t availableBytes() {
        return serialBufferHead-serialBufferTail;
    }

    Message getMessage() {
        Message message;
        do {
            message.type = NONE;
            // Get Bytes
            uint8_t bytesAvailable = availableBytes(); 
            if (bytesAvailable >= 1) {
                uint8_t midiByte = peekByte();
                // Process data byte
                if (DATA_B <= midiByte && midiByte <= DATA_T) {
                    if (NOTE_OFF_B <= runningStatus && runningStatus <= NOTE_OFF_T) {
                        if (bytesAvailable >= 2) {
                            message.type = NOTE_OFF;
                            message.channel = runningStatus&0b00001111;
                            message.pitch = readByte();
                            message.velocity = readByte();
                        }
                    }
                    else if (NOTE_ON_B <= runningStatus && runningStatus <= NOTE_ON_T) {
                        if (bytesAvailable >= 2) {
                            message.type = NOTE_ON;
                            message.channel = runningStatus&0b00001111;
                            message.pitch = readByte();
                            message.velocity = readByte();
                        }
                    }
                    else if (MODE_B <= runningStatus && runningStatus <= MODE_T) {
                        message.channel = runningStatus&0b00001111;
                        message.pitch = readByte();
                        message.velocity = readByte();
                        if (message.pitch == 120 && message.velocity == 0) {
                            message.type = SOUND_OFF;
                        }
                        else if (message.pitch == 121) {
                            message.type = CHAN_RESET;
                        }
                        else if (123 <= message.pitch && message.pitch <= 127) {
                            message.type = NOTES_OFF;
                        }
                    }
                    else {
                        readByte();
                        message.type = UNKNOWN;
                        //TODO - Key Pressure, Control Change, Program Change, Channel Pressure, Pitch Bend
                    }
                }
                // Process channel mode byte
                else if (MODE_B <= runningStatus && runningStatus <= MODE_T) {
                    runningStatus = readByte();
                    message.type = PROCESSING;
                    //TODO - Sound Off, Reset, Local Control, Notes Off 
                }
                // Process channel voice byte
                else if (VOICE_B <= midiByte && midiByte <= VOICE_T) { 
                    runningStatus = readByte();
                    message.type = PROCESSING;
                }
                // Process common byte
                else if (COMMON_B <= midiByte && midiByte <= COMMON_T) {
                    runningStatus = readByte();
                    message.type = UNKNOWN;
                    //TODO - list
                }
                // Process system real-time byte
                else if (REALTIME_B <= midiByte && midiByte <= REALTIME_T) {
                    readByte();
                    if (midiByte == STOP_BT) {
                        message.type = STOP;
                    }
                    else if (midiByte == RESET_BT) {
                        message.type = RESET;
                    }
                    else {
                        message.type = UNKNOWN;
                        //TODO - list
                    }
                }
            }
        } while (message.type == PROCESSING);
        
        return message;
    }
}