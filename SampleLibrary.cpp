#include "SampleLibrary.h"
#include <cmath>

SampleLibrary::SampleLibrary()
    : logger_(Logger::getInstance())
{
    // nic dál v konstruktoru - actual allocation happens in initialize()
}

/*
 * initialize
 *  - uloží sampleRate a vygeneruje všechno (MIN_NOTE..MAX_NOTE)
 *  - pokud generování nějaké noty selže, loguje a pokračuje (nepřeruší ostatní)
 *  - OPRAVA: Odebrán lock mutexu, protože inicializace je single-threaded (z prepareToPlay),
 *    což zabraňuje deadlocku při rekurzivním zamykání v generateSampleForNote.
 */
void SampleLibrary::initialize(double sampleRate)
{
    if (sampleRate <= 0.0) {
        logger_.log("SampleLibrary/initialize", "error", "Invalid sampleRate: " + juce::String(sampleRate));
        throw std::invalid_argument("Invalid sampleRate");
    }

    sampleRate_ = sampleRate;
    clear();

    logger_.log("SampleLibrary/initialize", "info",
                "Inicializace sample library se sampleRate=" + juce::String(sampleRate_));

    int success = 0;
    int fail = 0;
    for (uint8_t n = MIN_NOTE; n <= MAX_NOTE; ++n) {
        if (generateSampleForNote(n)) ++success;
        else ++fail;
    }

    logger_.log("SampleLibrary/initialize", "info",
                "Generování samplů dokončeno. Success: " + juce::String(success) +
                " Fail: " + juce::String(fail));
}


/*
 * clear - vymaže interní data
 */
void SampleLibrary::clear()
{
    std::lock_guard<std::mutex> lock(accessMutex_);
    for (auto& seg : sampleSegments_)
        seg.reset();

    logger_.log("SampleLibrary/clear", "debug", "SampleLibrary cleared");
}

/*
 * generateSampleForNote
 *  - Vygeneruje sinusovku délky SAMPLE_SECONDS pro danou notu.
 *  - Vrací true pokud uspěje.
 *  - Nově: Přidány debug logy kolem alokace pro lepší sledování paměti (každá nota zvlášť).
 */
bool SampleLibrary::generateSampleForNote(uint8_t note)
{
    // Lokální kalkulace bez držení locku po dobu generování (alokačně-intenzivní)
    double freq = getFrequencyForNote(note);
    uint32_t sampleLength = static_cast<uint32_t>(sampleRate_ * SAMPLE_SECONDS);

    if (sampleLength < 1) {
        logger_.log("SampleLibrary/generateSampleForNote", "error",
                    "Invalid sample length for note " + juce::String((int)note));
        return false;
    }

    // Debug log před alokací (pro sledování malloc-like)
    size_t allocSize = static_cast<size_t>(sampleLength) * sizeof(float);
    logger_.log("SampleLibrary/generateSampleForNote", "debug",
                "Začínám alokaci pro notu " + juce::String((int)note) +
                ", velikost: " + juce::String(allocSize) + " bajtů");

    std::unique_ptr<float[]> tmpData;
    try {
        tmpData = std::make_unique<float[]>(sampleLength);
        // Debug log po úspěšné alokaci
        logger_.log("SampleLibrary/generateSampleForNote", "debug",
                    "Alokace úspěšná pro notu " + juce::String((int)note));
    } catch (const std::bad_alloc&) {
        logger_.log("SampleLibrary/generateSampleForNote", "error",
                    "Allocation failed for note " + juce::String((int)note));
        return false;
    }

    const double twoPi = 2.0 * juce::MathConstants<double>::pi;
    const double phaseInc = twoPi * freq / sampleRate_;

    for (uint32_t i = 0; i < sampleLength; ++i) {
        double phase = phaseInc * static_cast<double>(i);
        // Explicit cast -> potlačí warning C4244
        tmpData[i] = SAMPLE_AMPLITUDE * static_cast<float>(std::sin(phase));
    }

    // Commit: Uložení do interní struktury pod lockem (atomic-ish)
    {
        std::lock_guard<std::mutex> lock(accessMutex_);
        SampleSegment& seg = sampleSegments_[note];
        seg.sampleData = std::move(tmpData);
        seg.lengthSamples = sampleLength;
        seg.midiNote = note;
        seg.isAllocated = true;
    }

    logger_.log("SampleLibrary/generateSampleForNote", "debug",
                "Vzorek vygenerován pro notu " + juce::String((int)note) +
                " freq=" + juce::String(freq, 2) +
                " samples=" + juce::String(sampleLength));
    return true;
}

/*
 * getSampleData / getSampleLength / isNoteAvailable
 *  - vrací read-only data (chráněné mutexem)
 */
const float* SampleLibrary::getSampleData(uint8_t midiNote) const
{
    std::lock_guard<std::mutex> lock(accessMutex_);
    if (midiNote < sampleSegments_.size() && sampleSegments_[midiNote].isAllocated)
        return sampleSegments_[midiNote].sampleData.get();
    return nullptr;
}

uint32_t SampleLibrary::getSampleLength(uint8_t midiNote) const
{
    std::lock_guard<std::mutex> lock(accessMutex_);
    if (midiNote < sampleSegments_.size() && sampleSegments_[midiNote].isAllocated)
        return sampleSegments_[midiNote].lengthSamples;
    return 0;
}

bool SampleLibrary::isNoteAvailable(uint8_t midiNote) const
{
    std::lock_guard<std::mutex> lock(accessMutex_);
    return midiNote < sampleSegments_.size() && sampleSegments_[midiNote].isAllocated;
}

double SampleLibrary::getFrequencyForNote(uint8_t midiNote) const
{
    // standardní formule A4=440Hz (MIDI 69)
    return 440.0 * std::pow(2.0, (static_cast<int>(midiNote) - 69) / 12.0);
}
