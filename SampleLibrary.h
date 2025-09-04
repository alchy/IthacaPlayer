#pragma once

#include <juce_core/juce_core.h>
#include <memory>
#include <array>
#include <mutex>
#include "Logger.h"
#include "SampleLoader.h"

/**
 * @struct SampleSegment
 * @brief Kontejner pro 8 dynamic levels jedné MIDI noty s stereo support
 * 
 * Každá MIDI nota má 8 dynamic levels (vel0-vel7) s různými amplitudami.
 * Každý level může mít jinou délku a může být mono nebo stereo.
 */
struct SampleSegment
{
    std::array<std::unique_ptr<float[]>, 8> dynamicLayers;      // 8 dynamic levels
    std::array<uint32_t, 8> layerLengthSamples;                // Délka každého levelu
    std::array<bool, 8> layerAllocated;                        // Zda je level alokován
    std::array<bool, 8> layerIsStereo;                         // Zda je level stereo
    uint8_t midiNote;                                           // MIDI nota tohoto segmentu
    
    SampleSegment() : layerLengthSamples{}, layerAllocated{}, layerIsStereo{}, midiNote(0) {}
    
    /**
     * @brief Vrátí délku konkrétního dynamic levelu
     */
    uint32_t getLayerLength(uint8_t dynamicLevel) const {
        return (dynamicLevel < 8) ? layerLengthSamples[dynamicLevel] : 0;
    }
    
    /**
     * @brief Vrátí data konkrétního dynamic levelu
     */
    const float* getLayerData(uint8_t dynamicLevel) const {
        return (dynamicLevel < 8 && layerAllocated[dynamicLevel]) 
               ? dynamicLayers[dynamicLevel].get() : nullptr;
    }
    
    /**
     * @brief Zkontroluje zda je dynamic level dostupný
     */
    bool isLayerAvailable(uint8_t dynamicLevel) const {
        return (dynamicLevel < 8) && layerAllocated[dynamicLevel];
    }
    
    /**
     * @brief Zkontroluje zda je dynamic level stereo
     */
    bool isLayerStereo(uint8_t dynamicLevel) const {
        return (dynamicLevel < 8) && layerIsStereo[dynamicLevel];
    }
    
    /**
     * @brief Uloží sample do konkrétního dynamic levelu
     */
    void storeLayer(uint8_t dynamicLevel, std::unique_ptr<float[]> data, uint32_t length, bool isStereo) {
        if (dynamicLevel < 8) {
            dynamicLayers[dynamicLevel] = std::move(data);
            layerLengthSamples[dynamicLevel] = length;
            layerAllocated[dynamicLevel] = true;
            layerIsStereo[dynamicLevel] = isStereo;
        }
    }
    
    /**
     * @brief Resetuje všechny dynamic levels
     */
    void reset() {
        for (int i = 0; i < 8; ++i) {
            dynamicLayers[i].reset();
            layerLengthSamples[i] = 0;
            layerAllocated[i] = false;
            layerIsStereo[i] = false;
        }
        midiNote = 0;
    }
    
    /**
     * @brief Spočítá celkovou spotřebu paměti tohoto segmentu
     */
    size_t getMemoryUsage() const {
        size_t total = 0;
        for (int i = 0; i < 8; ++i) {
            if (layerAllocated[i]) {
                total += layerLengthSamples[i] * sizeof(float) * (layerIsStereo[i] ? 2 : 1);
            }
        }
        return total;
    }
};

/**
 * @struct LoadingStats
 * @brief Rozšířené statistiky loading procesu pro SampleLibrary
 */
struct SampleLibraryStats
{
    int totalSamples;           // Celkový počet načtených samples
    int loadedFromFiles;        // Počet načtených z WAV souborů
    int generatedSines;         // Počet vygenerovaných sine waves
    int savedToFiles;           // Počet uložených generovaných souborů
    size_t totalMemoryUsed;     // Celková spotřeba paměti v bajtech
    double loadingTimeSeconds;  // Celkový čas loading
    
    SampleLibraryStats() : totalSamples(0), loadedFromFiles(0), generatedSines(0), 
                          savedToFiles(0), totalMemoryUsed(0), loadingTimeSeconds(0.0) {}
    
    /**
     * @brief Vrátí lidsky čitelný popis statistik
     */
    juce::String getDescription() const {
        return "Samples: " + juce::String(totalSamples) + 
               " (WAV: " + juce::String(loadedFromFiles) + 
               ", Generated: " + juce::String(generatedSines) + 
               ", Saved: " + juce::String(savedToFiles) + ")" +
               ", Memory: " + juce::String(totalMemoryUsed / (1024*1024)) + "MB" +
               ", Time: " + juce::String(loadingTimeSeconds, 2) + "s";
    }
};

/**
 * @class SampleLibrary
 * @brief Refaktorovaná sample library s podporou dynamic levels a hybridního loading
 * 
 * Nové vlastnosti:
 * - 8 dynamic levels pro každou MIDI notu (vel0-vel7)
 * - Hybridní loading: WAV soubory + fallback sine generation
 * - Variable length samples (každý level může mít jinou délku)
 * - Stereo/mono support s automatickou konverzí
 * - Automatické ukládání vygenerovaných samples
 * - Thread-safe přístup s mutexem
 * - Detailní loading statistiky
 */
class SampleLibrary
{
public:
    SampleLibrary();
    ~SampleLibrary() = default;

    // === Hlavní rozhraní ===
    
    /**
     * @brief Inicializuje sample library s hybridním loading systémem
     * @param sampleRate Target sample rate
     * @param progressCallback Callback pro progress reporting
     */
    void initialize(double sampleRate, 
                   std::function<void(int, int, const juce::String&)> progressCallback = nullptr);

    /**
     * @brief Vyčistí všechny samples (uvolní paměť)
     */
    void clear();

    // === Rozšířené API pro dynamic levels s stereo support ===
    
    /**
     * @brief Vrátí sample data pro konkrétní notu a dynamic level
     * @param midiNote MIDI nota (21-108)
     * @param dynamicLevel Dynamic level (0-7)
     * @return Ukazatel na audio data nebo nullptr
     */
    const float* getSampleData(uint8_t midiNote, uint8_t dynamicLevel) const;
    
    /**
     * @brief Vrátí délku sample pro konkrétní notu a dynamic level
     * @param midiNote MIDI nota
     * @param dynamicLevel Dynamic level
     * @return Délka v samples nebo 0
     */
    uint32_t getSampleLength(uint8_t midiNote, uint8_t dynamicLevel) const;
    
    /**
     * @brief Zkontroluje dostupnost konkrétního dynamic levelu
     * @param midiNote MIDI nota
     * @param dynamicLevel Dynamic level
     * @return true pokud je dostupný
     */
    bool isNoteAvailable(uint8_t midiNote, uint8_t dynamicLevel) const;
    
    /**
     * @brief Zkontroluje zda je sample stereo
     * @param midiNote MIDI nota
     * @param dynamicLevel Dynamic level
     * @return true pokud je stereo
     */
    bool isSampleStereo(uint8_t midiNote, uint8_t dynamicLevel) const;

    // === Backward compatibility (používá dynamic level 0) ===
    
    const float* getSampleData(uint8_t midiNote) const {
        return getSampleData(midiNote, 0);
    }
    
    uint32_t getSampleLength(uint8_t midiNote) const {
        return getSampleLength(midiNote, 0);
    }
    
    bool isNoteAvailable(uint8_t midiNote) const {
        return isNoteAvailable(midiNote, 0);
    }

    // === Utility metody ===
    
    /**
     * @brief Mapuje MIDI velocity na dynamic level
     * @param velocity MIDI velocity (0-127)
     * @return Dynamic level (0-7)
     */
    static uint8_t velocityToDynamicLevel(uint8_t velocity);
    
    /**
     * @brief Vrátí loading statistiky
     */
    const SampleLibraryStats& getLoadingStats() const { return loadingStats_; }
    
    /**
     * @brief Vrátí celkovou spotřebu paměti
     */
    size_t getTotalMemoryUsage() const;
    
    /**
     * @brief Vrátí počet dostupných not
     */
    int getAvailableNoteCount() const;
    
    /**
     * @brief Vrátí detailní informace o dostupných dynamic levels
     */
    struct AvailabilityInfo {
        int totalNotes;
        int notesWithAnyLevel;
        std::array<int, 8> levelCounts;  // Počet not pro každý level
        int monoSamples;
        int stereoSamples;
        
        AvailabilityInfo() : totalNotes(0), notesWithAnyLevel(0), levelCounts{}, 
                            monoSamples(0), stereoSamples(0) {}
    };
    
    AvailabilityInfo getAvailabilityInfo() const;

    // === Konstanty ===
    
    static constexpr uint8_t MIN_NOTE = 21;        // A0
    static constexpr uint8_t MAX_NOTE = 108;       // C8
    static constexpr uint8_t NUM_DYNAMIC_LEVELS = 8;
    static constexpr double SAMPLE_SECONDS = 12.0; // Délka generovaných samples

private:
    // === Private členové ===
    
    mutable std::mutex accessMutex_;                        // Thread safety
    std::array<SampleSegment, 128> sampleSegments_;        // Storage pro všechny MIDI noty
    double sampleRate_{44100.0};                           // Current sample rate
    Logger& logger_;                                        // Reference na logger
    SampleLibraryStats loadingStats_;                      // Loading statistiky
    
    // === Private metody ===
    
    /**
     * @brief Uloží načtený sample do interní struktury
     */
    void storeSample(const LoadedSample& sample);
    
    /**
     * @brief Validuje MIDI notu a dynamic level
     */
    bool isValidNote(uint8_t midiNote) const {
        return midiNote >= MIN_NOTE && midiNote <= MAX_NOTE;
    }
    
    bool isValidDynamicLevel(uint8_t dynamicLevel) const {
        return dynamicLevel < NUM_DYNAMIC_LEVELS;
    }
};