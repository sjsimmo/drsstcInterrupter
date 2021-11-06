#include <stdint.h>

#ifndef MIDI_H
#define MIDI_H

namespace Midi
{
    enum MessageType {
        NONE,       // Incomplete midi message 
        PROCESSING, // Used internally by getMessage 
        UNKNOWN,    // Unhandled message
        STOP,       // Stop all
        RESET,      // Reset all
        NOTE_ON,
        NOTE_OFF,
        SOUND_OFF,
        NOTES_OFF,
        CHAN_RESET
    };

    struct Message {
        MessageType type;
        uint8_t channel;    // 0-15
        uint8_t pitch;      // 0-127
        uint8_t velocity;   // 0-127
    };

    Message getMessage();
    void init();
}

#endif