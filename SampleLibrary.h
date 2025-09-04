#pragma once

#include <juce_core/juce_core.h>
#include <memory>
#include <array>
#include "Logger.h"

/*
 * SampleSegment
 *   - vlastní jednoduché úložiště pro float vzorek (unique_ptr)
 *   - lengthSamples = délka v samplech
 */
struct SampleSegment
{
    std::unique_ptr<float[]> sampleData;
    uint32_t lengthSamples{0};
    uint8_t midiNote{0};
    bool isAllocated{false};

    void reset()
    {
        sampleData.reset();
        lengthSamples = 0;
        midiNote = 0;
        isAllocated = false;
    }
};

/*
 * SampleLibrary
 * - refaktorizovaná, thread-safe (interní mutex pro mutace)
 * - initialize(sampleRate) připraví (vygeneruje) všechny vzorky v rozsahu MIN_NOTE..MAX_NOTE
 * - poskytuje read-only přístup: getSampleData/getSampleLength/isNoteAvailable
 */
class SampleLibrary
{
public:
    SampleLibrary();
    ~SampleLibrary() = default;

    // Inicializace knihovny (nutné zavolat před použitím)
    // Vygeneruje všechny vzorky v rozsahu MIN_NOTE..MAX_NOTE (12 sekund každý)
    void initialize(double sampleRate);

    // Vyčistí všechny vzorky (uvolní paměť)
    void clear();

    // Generuje a uloží vzorek pro konkrétní notu (použito interně i externě)
    // Vrací true pokud generace proběhla úspěšně.
    bool generateSampleForNote(uint8_t note);

    // Read-only přístup
    const float* getSampleData(uint8_t midiNote) const;
    uint32_t getSampleLength(uint8_t midiNote) const;
    bool isNoteAvailable(uint8_t midiNote) const;

    // Konstanty
    static constexpr uint8_t MIN_NOTE = 21;   // A0
    static constexpr uint8_t MAX_NOTE = 108;  // C8
    static constexpr double SAMPLE_SECONDS = 12.0; // délka v sekundách

private:
    // interní helper pro frekvenci
    double getFrequencyForNote(uint8_t midiNote) const;

    mutable std::mutex accessMutex_;                // chrání sampleSegments_
    std::array<SampleSegment, 128> sampleSegments_; // úložiště pro všechny MIDInoty
    double sampleRate_{44100.0};
    Logger& logger_;
    static constexpr float SAMPLE_AMPLITUDE = 0.25f; // bezpečná amplitude
};
