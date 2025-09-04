#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <memory>
#include <functional>
#include <vector>
#include "Logger.h"

/**
 * @struct LoadedSample
 * @brief Kontejner pro jeden načtený nebo vygenerovaný sample s metadata
 */
struct LoadedSample {
    std::unique_ptr<float[]> audioData;    // Audio data (interleaved pro stereo)
    uint32_t lengthSamples;                // Délka v samples (per channel)
    uint8_t midiNote;                      // MIDI nota (21-108)
    uint8_t dynamicLevel;                  // Dynamic level (0-7)
    uint8_t numChannels;                   // Počet kanálů (1=mono, 2=stereo)
    bool isGenerated;                      // true = vygenerovaný sine, false = načtený z WAV
    juce::String sourcePath;               // Cesta k source souboru
    double originalSampleRate;             // Původní sample rate (pro debug)
    
    LoadedSample() : lengthSamples(0), midiNote(0), dynamicLevel(0), numChannels(1),
                     isGenerated(false), originalSampleRate(0.0) {}
                     
    /**
     * @brief Vrátí celkovou velikost dat v bytes
     */
    size_t getDataSize() const {
        return lengthSamples * numChannels * sizeof(float);
    }
    
    /**
     * @brief Zkontroluje zda je sample stereo
     */
    bool isStereo() const {
        return numChannels == 2;
    }
};

/**
 * @struct FileAnalysis
 * @brief Analýza WAV souboru před načtením (pro optimalizaci paměti)
 */
struct FileAnalysis {
    uint32_t originalLengthSamples;
    uint32_t targetLengthSamples;
    double originalSampleRate;
    bool needsResampling;
    size_t memoryRequired;
    bool isValid;
    juce::String errorMessage;
    
    FileAnalysis() : originalLengthSamples(0), targetLengthSamples(0), 
                     originalSampleRate(0.0), needsResampling(false), 
                     memoryRequired(0), isValid(false) {}
};

/**
 * @struct LoadingStats
 * @brief Statistiky loading procesu
 */
struct LoadingStats {
    int totalExpected;          // Očekávaný počet souborů (88 not × 8 levelů)
    int filesLoaded;            // Počet načtených WAV souborů
    int filesGenerated;         // Počet vygenerovaných sine waves
    int filesSaved;             // Počet uložených generovaných souborů
    size_t totalMemoryUsed;     // Celková spotřeba paměti
    double loadingTimeSeconds;  // Celkový čas loading
    
    LoadingStats() : totalExpected(0), filesLoaded(0), filesGenerated(0), 
                     filesSaved(0), totalMemoryUsed(0), loadingTimeSeconds(0.0) {}
};

/**
 * @class SampleLoader
 * @brief Správce načítání/ukládání audio samples s support pro dynamic levels
 * 
 * Implementuje hybridní systém:
 * 1. Pokusí se načíst WAV soubor z %APPDATA%/IthacaPlayer/instrument/
 * 2. Pokud neexistuje, vygeneruje sine wave a uloží ho
 * 3. Podporuje 8 dynamic levels (vel0-vel7) pro každou MIDI notu
 * 4. Automatické resampling na target sample rate
 * 5. Zachování stereo formátu pokud existuje
 */
class SampleLoader
{
public:
    using ProgressCallback = std::function<void(int current, int total, const juce::String& status)>;
    
    /**
     * @brief Konstruktor s target sample rate
     * @param sampleRate Cílový sample rate pro všechny samples
     */
    explicit SampleLoader(double sampleRate);
    
    /**
     * @brief Načte kompletní instrument (všechny noty × všechny dynamic levels)
     * @param instrumentDirectory Directory s .lau soubory
     * @param progressCallback Callback pro progress reporting
     * @return Vektor načtených samples
     */
    std::vector<LoadedSample> loadInstrument(
        const juce::File& instrumentDirectory,
        ProgressCallback progressCallback = nullptr
    );
    
    /**
     * @brief Načte jeden konkrétní sample (buď z souboru nebo vygeneruje)
     * @param instrumentDirectory Directory s .lau soubory
     * @param midiNote MIDI nota (21-108)
     * @param dynamicLevel Dynamic level (0-7)
     * @return Načtený sample
     */
    LoadedSample loadSingleSample(
        const juce::File& instrumentDirectory,
        uint8_t midiNote, 
        uint8_t dynamicLevel
    );
    
    // === Utility Methods ===
    
    /**
     * @brief Vrátí default instrument directory (%APPDATA%/IthacaPlayer/instrument/)
     */
    static juce::File getDefaultInstrumentDirectory();
    
    /**
     * @brief Vygeneruje název souboru podle naming convention
     * @param midiNote MIDI nota
     * @param dynamicLevel Dynamic level
     * @return Název souboru (např. "m060-vel3.lau")
     */
    static juce::String generateFilename(uint8_t midiNote, uint8_t dynamicLevel);
    
    /**
     * @brief Parsuje název souboru a extrahuje MIDI notu a dynamic level
     * @param filename Název souboru
     * @param midiNote [out] Extrahovaná MIDI nota
     * @param dynamicLevel [out] Extrahovaný dynamic level
     * @return true pokud parsing úspěšný
     */
    static bool parseFilename(const juce::String& filename, uint8_t& midiNote, uint8_t& dynamicLevel);
    
    /**
     * @brief Mapuje velocity (0-127) na dynamic level (0-7)
     * @param velocity MIDI velocity
     * @return Dynamic level
     */
    static uint8_t velocityToDynamicLevel(uint8_t velocity);
    
    /**
     * @brief Vrátí amplitude pro daný dynamic level
     * @param dynamicLevel Dynamic level (0-7)
     * @return Amplitude (0.05f - 1.0f)
     */
    static float getDynamicAmplitude(uint8_t dynamicLevel);
    
    /**
     * @brief Vrátí loading statistiky
     */
    const LoadingStats& getLoadingStats() const { return loadingStats_; }

private:
    double sampleRate_;                           // Target sample rate
    juce::AudioFormatManager formatManager_;     // JUCE audio format manager
    Logger& logger_;                             // Reference na logger
    LoadingStats loadingStats_;                  // Loading statistiky
    
    // === Private Methods ===
    
    /**
     * @brief Analyzuje WAV soubor bez načtení dat (pro memory planning)
     */
    FileAnalysis analyzeWavFile(const juce::File& file);
    
    /**
     * @brief Validuje výsledky file analýzy
     */
    bool validateFileAnalysis(const FileAnalysis& analysis);
    
    /**
     * @brief Načte WAV soubor s optional resampling
     */
    LoadedSample loadWavFile(const juce::File& file, uint8_t midiNote, uint8_t dynamicLevel);
    
    /**
     * @brief Vygeneruje sine wave pro danou notu a dynamic level
     */
    LoadedSample generateSineWave(uint8_t midiNote, uint8_t dynamicLevel);
    
    /**
     * @brief Uloží vygenerovaný sample do .lau souboru
     */
    bool saveGeneratedSample(const LoadedSample& sample, const juce::File& targetFile);
    
    /**
     * @brief Provede resampling pokud je potřeba
     */
    std::unique_ptr<float[]> resampleIfNeeded(
        const float* sourceData, 
        uint32_t sourceLength, 
        double sourceSampleRate,
        uint32_t& outputLength
    );
    
    /**
     * @brief Vrátí frekvenci pro MIDI notu
     */
    double getFrequencyForNote(uint8_t midiNote) const;
    
    // === Constants ===
    
    static constexpr uint8_t MIN_NOTE = 21;        // A0
    static constexpr uint8_t MAX_NOTE = 108;       // C8
    static constexpr uint8_t NUM_DYNAMIC_LEVELS = 8;
    static constexpr double SAMPLE_SECONDS = 12.0; // Délka generovaných samples
    
    // Dynamic amplitudes pro jednotlivé úrovně
    static const float DYNAMIC_AMPLITUDES[8];
};