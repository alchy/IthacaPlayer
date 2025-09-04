#include "SampleLibrary.h"
#include <cmath>

SampleLibrary::SampleLibrary(double sampleRate)
    : sampleRate_(sampleRate)
    , maxSampleLength_(0)
    , logger_(Logger::getInstance())
{
    calculateBufferSizes();
    
    // Inicializace všech segmentů
    for (int i = 0; i < MIDI_NOTE_COUNT; i++) {
        segments_[i] = SampleSegment();
        segments_[i].midiNote = i;
    }
    
    logger_.log("SampleLibrary/constructor", "info", "SampleLibrary inicializovana pro sample rate: " + juce::String(sampleRate, 1));
    logger_.log("SampleLibrary/constructor", "info", "Max sample length: " + juce::String(maxSampleLength_) + " samples");
}

SampleLibrary::~SampleLibrary()
{
    logger_.log("SampleLibrary/destructor", "info", "Uvolnovani Sample Library");
    
    // Uvolnění všech alokovaných segmentů
    for (int i = 0; i < MIDI_NOTE_COUNT; i++) {
        deallocateSegment(i);
    }
    
    logger_.log("SampleLibrary/destructor", "info", "Sample Library uvolnena");
}

void SampleLibrary::initializeLibrary()
{
    logger_.log("SampleLibrary/initializeLibrary", "info", "=== INICIALIZACE SAMPLE LIBRARY ===");
    logger_.log("SampleLibrary/initializeLibrary", "info", "Priprava pro " + juce::String(MIDI_NOTE_COUNT) + " MIDI not");
    logger_.log("SampleLibrary/initializeLibrary", "info", "Delka kazdeho sample: " + juce::String(MAX_SAMPLE_LENGTH_SECONDS) + " sekund");
    
    // Výpočet celkové paměti
    double totalMemoryMB = (maxSampleLength_ * MIDI_NOTE_COUNT * sizeof(float)) / (1024.0 * 1024.0);
    logger_.log("SampleLibrary/initializeLibrary", "info", "Potencialni pamet: " + juce::String(totalMemoryMB, 1) + " MB");
    
    logger_.log("SampleLibrary/initializeLibrary", "info", "Sample Library pripravena k pouziti");
}

bool SampleLibrary::generateSineWaveForNote(uint8_t midiNote, float frequency)
{
    if (midiNote >= MIDI_NOTE_COUNT) {
        logger_.log("SampleLibrary/generateSineWaveForNote", "warn", "MIDI nota mimo rozsah: " + juce::String(midiNote));
        return false;
    }
    
    logger_.log("SampleLibrary/generateSineWaveForNote", "info", "Generovani sine wave pro MIDI notu " + juce::String(midiNote) + 
                " (frekvence: " + juce::String(frequency, 2) + " Hz)");
    
    // Alokace paměti pro segment
    if (!allocateSegment(midiNote)) {
        logger_.log("SampleLibrary/generateSineWaveForNote", "warn", "Chyba pri alokaci pameti pro notu " + juce::String(midiNote));
        return false;
    }
    
    // Generování sine wave dat
    fillSineWaveData(segments_[midiNote].sampleData, segments_[midiNote].lengthSamples, frequency, sampleRate_);
    
    logger_.log("SampleLibrary/generateSineWaveForNote", "info", "Sample pro notu " + juce::String(midiNote) + " uspesne vygenerovan");
    logger_.log("SampleLibrary/generateSineWaveForNote", "info", "Delka: " + juce::String(segments_[midiNote].lengthSamples) + " samples");
    
    return true;
}

const float* SampleLibrary::getSampleData(uint8_t midiNote) const
{
    if (midiNote >= MIDI_NOTE_COUNT || !segments_[midiNote].isAllocated) {
        return nullptr;
    }
    
    return segments_[midiNote].sampleData;
}

uint32_t SampleLibrary::getSampleLength(uint8_t midiNote) const
{
    if (midiNote >= MIDI_NOTE_COUNT || !segments_[midiNote].isAllocated) {
        return 0;
    }
    
    return segments_[midiNote].lengthSamples;
}

bool SampleLibrary::isNoteAvailable(uint8_t midiNote) const
{
    if (midiNote >= MIDI_NOTE_COUNT) {
        return false;
    }
    
    return segments_[midiNote].isAllocated;
}

void SampleLibrary::calculateBufferSizes()
{
    maxSampleLength_ = static_cast<uint32_t>(MAX_SAMPLE_LENGTH_SECONDS * sampleRate_);
    
    logger_.log("SampleLibrary/calculateBufferSizes", "info", "Vypocitana max delka bufferu: " + juce::String(maxSampleLength_) + " samples");
}

bool SampleLibrary::allocateSegment(uint8_t midiNote)
{
    if (midiNote >= MIDI_NOTE_COUNT) {
        return false;
    }
    
    // Pokud už je alokován, nejdříve uvolni
    if (segments_[midiNote].isAllocated) {
        deallocateSegment(midiNote);
    }
    
    try {
        segments_[midiNote].sampleData = new float[maxSampleLength_];
        segments_[midiNote].lengthSamples = maxSampleLength_;
        segments_[midiNote].isAllocated = true;
        
        // Vyčištění bufferu
        for (uint32_t i = 0; i < maxSampleLength_; i++) {
            segments_[midiNote].sampleData[i] = 0.0f;
        }
        
        logger_.log("SampleLibrary/allocateSegment", "debug", "Segment pro notu " + juce::String(midiNote) + " alokovan");
        return true;
    }
    catch (const std::bad_alloc& e) {
        logger_.log("SampleLibrary/allocateSegment", "warn", "Chyba alokace pameti pro notu " + juce::String(midiNote));
        return false;
    }
}

void SampleLibrary::deallocateSegment(uint8_t midiNote)
{
    if (midiNote >= MIDI_NOTE_COUNT) {
        return;
    }
    
    if (segments_[midiNote].isAllocated && segments_[midiNote].sampleData != nullptr) {
        delete[] segments_[midiNote].sampleData;
        segments_[midiNote].sampleData = nullptr;
        segments_[midiNote].lengthSamples = 0;
        segments_[midiNote].isAllocated = false;
        
        logger_.log("SampleLibrary/deallocateSegment", "debug", "Segment pro notu " + juce::String(midiNote) + " uvolnen");
    }
}

void SampleLibrary::fillSineWaveData(float* buffer, uint32_t length, float frequency, double sampleRate)
{
    if (buffer == nullptr || length == 0) {
        return;
    }
    
    const float twoPi = 2.0f * 3.14159265359f;
    const float increment = twoPi * frequency / static_cast<float>(sampleRate);
    
    for (uint32_t i = 0; i < length; i++) {
        float phase = increment * static_cast<float>(i);
        buffer[i] = 0.3f * std::sin(phase); // 0.3f amplitude aby nebyl příliš hlasitý
    }
    
    logger_.log("SampleLibrary/fillSineWaveData", "debug", "Sine wave data vygenerovana: " + 
                juce::String(length) + " samples, frekvence " + juce::String(frequency, 2) + " Hz");
}