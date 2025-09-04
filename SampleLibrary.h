#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include "Logger.h"

// Konstanta inspirovaná vaším přístupem k DCO_PER_VOICE_DEFAULT
#define MAX_SAMPLE_LENGTH_SECONDS 12.0f
#define MIDI_NOTE_COUNT 128
#define SAMPLE_NOTE_FOR_PROTOTYPE 60  // Middle C pro prototyp

/**
 * SampleSegment - reprezentuje jeden pre-computed sample pro jednu MIDI notu
 * Inspirováno vaší DeviceDCO strukturou pro jednotlivé generátory
 */
struct SampleSegment {
    float* sampleData;           // Pointer na audio data (podobné vašim DCO buffer pointerům)
    uint32_t lengthSamples;      // Délka v samples
    uint8_t midiNote;           // MIDI nota (0-127)
    bool isAllocated;           // Flag pro alokaci (podobné vašim device type checks)
    
    SampleSegment() : sampleData(nullptr), lengthSamples(0), midiNote(0), isAllocated(false) {}
};

/**
 * SampleLibrary - centrální správce pre-computed samples
 * Inspirováno vaším Performer pattern - centrální orchestrace zdrojů
 */
class SampleLibrary 
{
public:
    SampleLibrary(double sampleRate);
    ~SampleLibrary();
    
    // Inicializace podobná vaší scan_bus() metodě
    void initializeLibrary();
    
    // Generování sine wave pro jednu notu (podobné vašemu build_dumb_voices)
    bool generateSineWaveForNote(uint8_t midiNote, float frequency);
    
    // Získání sample dat pro playback (podobné vašemu get/pop pattern)
    const float* getSampleData(uint8_t midiNote) const;
    uint32_t getSampleLength(uint8_t midiNote) const;
    bool isNoteAvailable(uint8_t midiNote) const;
    
    // Utility methods
    double getSampleRate() const { return sampleRate_; }
    uint32_t getMaxSampleLength() const { return maxSampleLength_; }
    
private:
    // Vypočet délky bufferu pro danou sample rate
    void calculateBufferSizes();
    
    // Alokace paměti (podobné vašemu memory management přístupu)
    bool allocateSegment(uint8_t midiNote);
    void deallocateSegment(uint8_t midiNote);
    
    // Generování sine wave dat
    void fillSineWaveData(float* buffer, uint32_t length, float frequency, double sampleRate);
    
    // Member variables podobné vašim global arrays dco[16], chain[64]
    SampleSegment segments_[MIDI_NOTE_COUNT];
    double sampleRate_;
    uint32_t maxSampleLength_;         // Maximální délka v samples pro 12s
    
    // Logger instance pro debugging (podobně jako v main kódu)
    Logger& logger_;
};