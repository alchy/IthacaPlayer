#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Logger.h"

/**
 * ActiveNote - struktura reprezentující aktivní MIDI notu.
 * Obsahuje základní atributy pro správu stavu noty.
 */
struct ActiveNote {
    uint8_t key;           // MIDI nota (0-127)
    uint8_t velocity;      // Velocity (0-127)
    uint8_t channel;       // MIDI channel (0-15)
    bool isActive;         // Flag indikující aktivní stav noty
    uint32_t triggerTime;  // Timestamp spuštění noty (pro voice stealing)
    
    ActiveNote() : key(0), velocity(0), channel(0), isActive(false), triggerTime(0) {}
};

/**
 * MidiStateManager - třída pro centrální správu MIDI stavu.
 * Zpracovává MIDI události, ukládá stavy not a controllerů, používá fronty pro události.
 */
class MidiStateManager 
{
public:
    MidiStateManager();
    
    // Metoda pro přidání note-on události do stavu a fronty.
    void putNoteOn(uint8_t channel, uint8_t key, uint8_t velocity);
    
    // Metoda pro přidání note-off události do stavu a fronty.
    void putNoteOff(uint8_t channel, uint8_t key);
    
    // Metoda pro vytažení note-on klávesy z fronty pro daný channel.
    uint8_t popNoteOn(uint8_t channel);   // Vrací key nebo 0xff pokud žádný není
    
    // Metoda pro vytažení note-off klávesy z fronty pro daný channel.
    uint8_t popNoteOff(uint8_t channel);  // Vrací key nebo 0xff pokud žádný není
    
    // Metoda pro získání velocity aktivní noty pro daný channel a key.
    uint8_t getVelocity(uint8_t channel, uint8_t key) const;
    
    // Metoda pro nastavení hodnoty pitch wheel.
    void setPitchWheel(int16_t pitchWheelValue);
    
    // Metoda pro získání aktuální hodnoty pitch wheel.
    int16_t getPitchWheel() const { return pitchWheel_; }
    
    // Metoda pro nastavení hodnoty MIDI controlleru pro daný channel.
    void setControllerValue(uint8_t channel, uint8_t controller, uint8_t value);
    
    // Metoda pro získání hodnoty MIDI controlleru pro daný channel.
    uint8_t getControllerValue(uint8_t channel, uint8_t controller) const;
    
    // Metoda pro logování aktuálně aktivních not.
    void logActiveNotes() const;
    
    // Metoda pro získání počtu aktivních not.
    int getActiveNoteCount() const;
    
    // Hlavní metoda pro zpracování MIDI bufferu z JUCE.
    void processMidiBuffer(const juce::MidiBuffer& midiBuffer);
    
private:
    // Metoda pro vyhledání slotu pro konkrétní notu v poli aktivních not.
    int findNoteSlot(uint8_t channel, uint8_t key) const;
    
    // Metoda pro vyhledání volného slotu v poli aktivních not.
    int findFreeSlot() const;
    
    // Konstanta definující maximální počet aktivních not (fixed velikost).
    static const int MAX_ACTIVE_NOTES = 128;
    ActiveNote activeNotes_[MAX_ACTIVE_NOTES];
    
    // Hodnota pitch wheel (signed pro rozsah -8192 až +8191).
    int16_t pitchWheel_;
    
    // Dvourozměrné pole pro ukládání hodnot MIDI controllerů (channel x controller).
    uint8_t controllerValues_[16][128];  // [channel][controller]
    
    // Struktura pro circular buffer fronty not (velikost 256 pro uint8 wrap-around).
    struct NoteQueue {
        uint8_t notes[256];         // Fixed array pro noty
        uint8_t readIndex;          // Index pro čtení (uint8 pro automatický wrap-around)
        uint8_t writeIndex;         // Index pro zápis (uint8 pro automatický wrap-around)
        uint8_t count;              // Počet položek v bufferu
        
        NoteQueue() : readIndex(0), writeIndex(0), count(0) {
            for (int i = 0; i < 256; i++) notes[i] = 0xff;
        }
    };
    
    NoteQueue noteOnQueue_[16];   // Fronty pro note-on události (jedna pro každý channel)
    NoteQueue noteOffQueue_[16];  // Fronty pro note-off události (jedna pro každý channel)
    
    // Reference na logger pro protokolování událostí.
    Logger& logger_;
    
    // Pomocná metoda pro přidání položky do circular bufferu.
    void pushToQueue(NoteQueue& queue, uint8_t note);
    
    // Pomocná metoda pro vytažení položky z circular bufferu.
    uint8_t popFromQueue(NoteQueue& queue);
};