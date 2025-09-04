#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Logger.h"

/**
 * ActiveNote - reprezentuje aktivní MIDI notu
 * Inspirováno vaší Note strukturou v MidiParser
 */
struct ActiveNote {
    uint8_t key;           // MIDI nota (0-127)
    uint8_t velocity;      // Velocity (0-127)
    uint8_t channel;       // MIDI channel (0-15)
    bool isActive;         // Flag pro aktivní stav
    uint32_t triggerTime;  // Timestamp spuštění (pro voice stealing)
    
    ActiveNote() : key(0), velocity(0), channel(0), isActive(false), triggerTime(0) {}
};

/**
 * MidiStateManager - centrální správa MIDI stavu
 * Inspirováno vaší ActiveKeys třídou s podobným rozhraním
 * Kombinuje funkčnost vašeho MidiParser + ActiveKeys
 */
class MidiStateManager 
{
public:
    MidiStateManager();
    
    // Metody inspirované vaším ActiveKeys interface
    void putNoteOn(uint8_t channel, uint8_t key, uint8_t velocity);
    void putNoteOff(uint8_t channel, uint8_t key);
    
    // Pop methods podobné vašemu pop_map_note_on/off pattern
    uint8_t popNoteOn(uint8_t channel);   // Vrací key nebo 0xff pokud žádný není
    uint8_t popNoteOff(uint8_t channel);  // Vrací key nebo 0xff pokud žádný není
    
    // Velocity lookup podobné vašemu get_map_velocity
    uint8_t getVelocity(uint8_t channel, uint8_t key) const;
    
    // Pitch wheel handling (podobné vašemu pitchwheel management)
    void setPitchWheel(int16_t pitchWheelValue);
    int16_t getPitchWheel() const { return pitchWheel_; }
    
    // Control Change handling (inspirováno vaší ControlChange strukturou)
    void setControllerValue(uint8_t channel, uint8_t controller, uint8_t value);
    uint8_t getControllerValue(uint8_t channel, uint8_t controller) const;
    
    // Debug metody
    void logActiveNotes() const;
    int getActiveNoteCount() const;
    
    // Processing MIDI buffer (hlavní vstupní bod pro MIDI data)
    void processMidiBuffer(const juce::MidiBuffer& midiBuffer);
    
private:
    // Internal note management (podobné vašemu circular buffer přístupu)
    void updateNoteQueues();
    int findNoteSlot(uint8_t channel, uint8_t key) const;
    int findFreeSlot() const;
    
    // Member variables podobné vašim buffer strukturám
    static const int MAX_ACTIVE_NOTES = 128;
    ActiveNote activeNotes_[MAX_ACTIVE_NOTES];
    
    // Pitch wheel state (podobné vaší note.pitchwheel struktuře)
    int16_t pitchWheel_;
    
    // Control change state (podobné vaší cc struktur)
    uint8_t controllerValues_[16][128];  // [channel][controller]
    
    // Queue management pro note on/off events (inspirováno vaším buffer pattern)
    struct NoteQueue {
        uint8_t notes[128];
        int readIndex;
        int writeIndex;
        int count;
        
        NoteQueue() : readIndex(0), writeIndex(0), count(0) {
            for (int i = 0; i < 128; i++) notes[i] = 0xff;
        }
    };
    
    NoteQueue noteOnQueue_[16];   // Queue pro každý MIDI channel
    NoteQueue noteOffQueue_[16];  // Oddělené queues pro on/off events
    
    // Logger instance
    Logger& logger_;
    
    // Internal helper methods
    void pushToQueue(NoteQueue& queue, uint8_t note);
    uint8_t popFromQueue(NoteQueue& queue);
};