#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include "Logger.h"

/**
 * ActiveNote - struktura reprezentující aktivní MIDI notu
 * Optimalizovaná pro rychlý přístup a cache efficiency
 */
struct ActiveNote {
    uint8_t key;                // MIDI nota (0-127)
    uint8_t velocity;           // Velocity (0-127)
    uint8_t channel;            // MIDI channel (0-15)
    bool isActive;              // Flag indikující aktivní stav noty
    uint32_t triggerTime;       // Timestamp spuštění noty (pro voice stealing)
    
    // Konstruktor s výchozími hodnotami
    ActiveNote() : key(0), velocity(0), channel(0), isActive(false), triggerTime(0) {}
    
    // Reset metoda pro opětovné použití
    void reset() {
        key = 0;
        velocity = 0;
        channel = 0;
        isActive = false;
        triggerTime = 0;
    }
};

/**
 * NoteQueue - optimalizovaný circular buffer pro MIDI noty
 * Používá uint8 pro automatický wrap-around
 */
struct NoteQueue {
    std::array<uint8_t, 256> notes;     // Fixed array pro noty
    std::atomic<uint8_t> writeIndex{0}; // Thread-safe write index
    std::atomic<uint8_t> count{0};      // Thread-safe počítadlo
    uint8_t readIndex{0};               // Read index (protected by mutex)
    
    NoteQueue() {
        notes.fill(0xff);  // Vyplnění invalid hodnotou
    }
    
    // Reset metoda
    void reset() {
        writeIndex.store(0);
        count.store(0);
        readIndex = 0;
        notes.fill(0xff);
    }
};

/**
 * MidiStateManager - centrální správa MIDI stavu
 * OPRAVA: Unified mutex strategy pro thread safety
 */
class MidiStateManager 
{
public:
    MidiStateManager();
    ~MidiStateManager() = default;
    
    // Hlavní MIDI processing metoda
    void processMidiBuffer(const juce::MidiBuffer& midiBuffer);
    
    // Note management - OPRAVA: Thread-safe s konzistentním lockingem
    void putNoteOn(uint8_t channel, uint8_t key, uint8_t velocity);
    void putNoteOff(uint8_t channel, uint8_t key);
    
    // Queue access pro VoiceManager - OPRAVA: Thread-safe
    uint8_t popNoteOn(uint8_t channel);   // Vrací key nebo 0xff pokud žádný není
    uint8_t popNoteOff(uint8_t channel);  // Vrací key nebo 0xff pokud žádný není
    
    // Note state queries - OPRAVA: Thread-safe
    uint8_t getVelocity(uint8_t channel, uint8_t key) const;
    bool isNoteActive(uint8_t channel, uint8_t key) const;
    
    // Pitch wheel management
    void setPitchWheel(int16_t pitchWheelValue);
    int16_t getPitchWheel() const { return pitchWheel_.load(); }
    
    // Controller management - OPRAVA: Thread-safe
    void setControllerValue(uint8_t channel, uint8_t controller, uint8_t value);
    uint8_t getControllerValue(uint8_t channel, uint8_t controller) const;
    
    // Utility methods
    void logActiveNotes() const;
    int getActiveNoteCount() const;
    void resetAllNotes();  // Emergency reset
    
    // Statistics
    uint32_t getTotalMidiMessages() const { return totalMidiMessages_.load(); }

private:
    // Note storage
    static const int MAX_ACTIVE_NOTES = 128;
    std::array<ActiveNote, MAX_ACTIVE_NOTES> activeNotes_;
    
    // MIDI state
    std::atomic<int16_t> pitchWheel_{0};                    // Thread-safe pitch wheel
    std::array<std::array<uint8_t, 128>, 16> controllerValues_;  // [channel][controller]
    
    // Event queues pro každý MIDI channel
    std::array<NoteQueue, 16> noteOnQueues_;
    std::array<NoteQueue, 16> noteOffQueues_;
    
    // OPRAVA: Unified thread safety - jeden mutex pro všechny MIDI operace
    mutable std::mutex midiMutex_;         // Unified mutex pro všechny MIDI operace
    
    // Statistics a debugging
    std::atomic<uint32_t> totalMidiMessages_{0};
    Logger& logger_;
    
    // Helper methods
    int findNoteSlot(uint8_t channel, uint8_t key) const;
    int findFreeSlot() const;
    void pushToQueue(NoteQueue& queue, uint8_t note);
    uint8_t popFromQueue(NoteQueue& queue);
    
    // Validation helpers
    bool isValidChannel(uint8_t channel) const { return channel < 16; }
    bool isValidKey(uint8_t key) const { return key < 128; }
    bool isValidController(uint8_t controller) const { return controller < 128; }
    
    // Kopírování zakázáno
    MidiStateManager(const MidiStateManager&) = delete;
    MidiStateManager& operator=(const MidiStateManager&) = delete;
};