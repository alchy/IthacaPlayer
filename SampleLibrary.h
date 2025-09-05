#pragma once

#include <juce_core/juce_core.h>
#include <memory>
#include <array>
#include <mutex>
#include "Logger.h"
#include "SampleLoader.h"

/**
 * @struct SampleSegment
 * @brief Container for 8 dynamic levels of a single MIDI note with stereo support
 * 
 * Each MIDI note has 8 dynamic levels (vel0-vel7) with different amplitudes.
 * Each level can have different length and can be mono or stereo.
 */
struct SampleSegment
{
    std::array<std::unique_ptr<float[]>, 8> dynamicLayers;      // 8 dynamic levels
    std::array<uint32_t, 8> layerLengthSamples;                // Length of each level
    std::array<bool, 8> layerAllocated;                        // Whether level is allocated
    std::array<bool, 8> layerIsStereo;                         // Whether level is stereo
    uint8_t midiNote;                                           // MIDI note of this segment
    
    SampleSegment() : layerLengthSamples{}, layerAllocated{}, layerIsStereo{}, midiNote(0) {}
    
    /**
     * @brief Returns length of specific dynamic level
     */
    uint32_t getLayerLength(uint8_t dynamicLevel) const {
        return (dynamicLevel < 8) ? layerLengthSamples[dynamicLevel] : 0;
    }
    
    /**
     * @brief Returns data of specific dynamic level
     */
    const float* getLayerData(uint8_t dynamicLevel) const {
        return (dynamicLevel < 8 && layerAllocated[dynamicLevel]) 
               ? dynamicLayers[dynamicLevel].get() : nullptr;
    }
    
    /**
     * @brief Checks if dynamic level is available
     */
    bool isLayerAvailable(uint8_t dynamicLevel) const {
        return (dynamicLevel < 8) && layerAllocated[dynamicLevel];
    }
    
    /**
     * @brief Checks if dynamic level is stereo
     */
    bool isLayerStereo(uint8_t dynamicLevel) const {
        return (dynamicLevel < 8) && layerIsStereo[dynamicLevel];
    }
    
    /**
     * @brief Stores sample into specific dynamic level
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
     * @brief Resets all dynamic levels
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
     * @brief Calculates total memory usage of this segment
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
 * @brief Extended loading process statistics for SampleLibrary
 */
struct SampleLibraryStats
{
    int totalSamples;           // Total number of loaded samples
    int loadedFromFiles;        // Number loaded from WAV files
    int generatedSines;         // Number of generated sine waves
    int savedToFiles;           // Number of saved generated files
    size_t totalMemoryUsed;     // Total memory usage in bytes
    double loadingTimeSeconds;  // Total loading time
    
    SampleLibraryStats() : totalSamples(0), loadedFromFiles(0), generatedSines(0), 
                          savedToFiles(0), totalMemoryUsed(0), loadingTimeSeconds(0.0) {}
    
    /**
     * @brief Returns human-readable description of statistics
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
 * @brief Refactored sample library with dynamic levels support and hybrid loading
 * 
 * Key features:
 * - 8 dynamic levels per MIDI note (vel0-vel7)
 * - Hybrid loading: WAV files + fallback sine generation
 * - Variable length samples (each level can have different length)
 * - Stereo/mono support with automatic conversion
 * - Automatic saving of generated samples
 * - Thread-safe access with mutex
 * - Detailed loading statistics
 */
class SampleLibrary
{
public:
    SampleLibrary();
    ~SampleLibrary() = default;

    // === Main Interface ===
    
    /**
     * @brief Initializes sample library with hybrid loading system
     * @param sampleRate Target sample rate
     * @param progressCallback Callback for progress reporting
     */
    void initialize(double sampleRate, 
                   std::function<void(int, int, const juce::String&)> progressCallback = nullptr);

    /**
     * @brief Clears all samples (frees memory)
     */
    void clear();

    // === Extended API for dynamic levels with stereo support ===
    
    /**
     * @brief Returns sample data for specific note and dynamic level
     * @param midiNote MIDI note (21-108)
     * @param dynamicLevel Dynamic level (0-7)
     * @return Pointer to audio data or nullptr
     */
    const float* getSampleData(uint8_t midiNote, uint8_t dynamicLevel) const;
    
    /**
     * @brief Returns sample length for specific note and dynamic level
     * @param midiNote MIDI note
     * @param dynamicLevel Dynamic level
     * @return Length in samples or 0
     */
    uint32_t getSampleLength(uint8_t midiNote, uint8_t dynamicLevel) const;
    
    /**
     * @brief Checks availability of specific dynamic level
     * @param midiNote MIDI note
     * @param dynamicLevel Dynamic level
     * @return true if available
     */
    bool isNoteAvailable(uint8_t midiNote, uint8_t dynamicLevel) const;
    
    /**
     * @brief Checks if sample is stereo
     * @param midiNote MIDI note
     * @param dynamicLevel Dynamic level
     * @return true if stereo
     */
    bool isSampleStereo(uint8_t midiNote, uint8_t dynamicLevel) const;

    // === Backward compatibility (uses dynamic level 0) ===
    
    const float* getSampleData(uint8_t midiNote) const {
        return getSampleData(midiNote, 0);
    }
    
    uint32_t getSampleLength(uint8_t midiNote) const {
        return getSampleLength(midiNote, 0);
    }
    
    bool isNoteAvailable(uint8_t midiNote) const {
        return isNoteAvailable(midiNote, 0);
    }

    // === Utility methods ===
    
    /**
     * @brief Maps MIDI velocity to dynamic level
     * @param velocity MIDI velocity (0-127)
     * @return Dynamic level (0-7)
     */
    static uint8_t velocityToDynamicLevel(uint8_t velocity);
    
    /**
     * @brief Returns loading statistics
     */
    const SampleLibraryStats& getLoadingStats() const { return loadingStats_; }
    
    /**
     * @brief Returns total memory usage
     */
    size_t getTotalMemoryUsage() const;
    
    /**
     * @brief Returns count of available notes
     */
    int getAvailableNoteCount() const;
    
    /**
     * @brief Returns detailed information about available dynamic levels
     */
    struct AvailabilityInfo {
        int totalNotes;
        int notesWithAnyLevel;
        std::array<int, 8> levelCounts;  // Count of notes for each level
        int monoSamples;
        int stereoSamples;
        
        AvailabilityInfo() : totalNotes(0), notesWithAnyLevel(0), levelCounts{}, 
                            monoSamples(0), stereoSamples(0) {}
    };
    
    AvailabilityInfo getAvailabilityInfo() const;

    // === Constants ===
    
    static constexpr uint8_t MIN_NOTE = 21;        // A0
    static constexpr uint8_t MAX_NOTE = 108;       // C8
    static constexpr uint8_t NUM_DYNAMIC_LEVELS = 8;
    static constexpr double SAMPLE_SECONDS = 12.0; // Length of generated samples

private:
    // === Private members ===
    
    mutable std::mutex accessMutex_;                        // Thread safety
    std::array<SampleSegment, 128> sampleSegments_;        // Storage for all MIDI notes
    double sampleRate_{44100.0};                           // Current sample rate
    Logger& logger_;                                        // Reference to logger
    SampleLibraryStats loadingStats_;                      // Loading statistics
    
    // === Private methods ===
    
    /**
     * @brief Stores loaded sample into RAM-based internal structure
     * 
     * This method copies audio data from LoadedSample into the internal
     * RAM-based storage structure (sampleSegments_). It does NOT save
     * to disk - that's handled by SampleLoader.
     * 
     * @param sample Loaded sample to store in RAM
     */
    void storeSampleRam(const LoadedSample& sample);
    
    /**
     * @brief Validates MIDI note and dynamic level
     */
    bool isValidNote(uint8_t midiNote) const {
        return midiNote >= MIN_NOTE && midiNote <= MAX_NOTE;
    }
    
    bool isValidDynamicLevel(uint8_t dynamicLevel) const {
        return dynamicLevel < NUM_DYNAMIC_LEVELS;
    }
};