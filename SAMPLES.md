# Kompletní Specifikace: Hybridní Sample Loading Systém s Dynamic Layers

## Přehled systému

Refaktoring IthacaPlayer pro podporu external WAV samples s 8 úrovněmi dynamiky, fallback sine generace a variable sample length support.

## File System & Naming Convention

### Directory struktura
```
%APPDATA%\Roaming\IthacaPlayer\instrument\
├── m021-vel0.lau    // A0, dynamic level 0
├── m021-vel1.lau    // A0, dynamic level 1
├── m060-vel0.lau    // Middle C, dynamic level 0
├── m060-vel7.lau    // Middle C, dynamic level 7
└── m108-vel7.lau    // C8, dynamic level 7
```

### Naming pattern
- **Format:** `mXXX-velY.lau`
- **XXX:** MIDI note (021-108, zero-padded)
- **Y:** Dynamic level (0-7)
- **Extension:** `.lau` (custom identifier)

## Core Architecture Changes

### 1. Nový SampleLoader modul

**SampleLoader.h**
```cpp
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <memory>
#include <functional>

struct LoadedSample {
    std::unique_ptr<float[]> audioData;
    uint32_t lengthSamples;
    uint8_t midiNote;
    uint8_t dynamicLevel;
    bool isGenerated;
    juce::String sourcePath;
};

struct FileAnalysis {
    uint32_t originalLengthSamples;
    uint32_t targetLengthSamples;
    double originalSampleRate;
    bool needsResampling;
    size_t memoryRequired;
    bool isValid;
};

class SampleLoader {
public:
    using ProgressCallback = std::function<void(int current, int total, const juce::String& status)>;
    
    SampleLoader(double sampleRate);
    
    // Main interface
    std::vector<LoadedSample> loadInstrument(
        const juce::File& instrumentDirectory,
        ProgressCallback progressCallback = nullptr
    );
    
    // Utility methods
    static juce::File getDefaultInstrumentDirectory();
    static juce::String generateFilename(uint8_t midiNote, uint8_t dynamicLevel);
    static bool parseFilename(const juce::String& filename, uint8_t& midiNote, uint8_t& dynamicLevel);
    
private:
    double sampleRate_;
    juce::AudioFormatManager formatManager_;
    
    // Analysis & validation
    FileAnalysis analyzeWavFile(const juce::File& file);
    bool validateFileAnalysis(const FileAnalysis& analysis);
    
    // Loading methods
    LoadedSample loadWavFile(const juce::File& file, uint8_t midiNote, uint8_t dynamicLevel);
    LoadedSample generateSineWave(uint8_t midiNote, uint8_t dynamicLevel);
    
    // Processing
    std::unique_ptr<float[]> resampleIfNeeded(
        const float* sourceData, 
        uint32_t sourceLength, 
        double sourceSampleRate,
        uint32_t& outputLength
    );
};
```

**SampleLoader.cpp klíčové metody**
```cpp
FileAnalysis SampleLoader::analyzeWavFile(const juce::File& file) {
    FileAnalysis analysis{};
    
    // 1. Analýza BEZ načítání dat
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager_.createReaderFor(file));
    if (!reader) {
        analysis.isValid = false;
        return analysis;
    }
    
    // 2. Získání metadata
    analysis.originalLengthSamples = static_cast<uint32_t>(reader->lengthInSamples);
    analysis.originalSampleRate = reader->sampleRate;
    analysis.needsResampling = (analysis.originalSampleRate != sampleRate_);
    
    // 3. Výpočet target délky
    if (analysis.needsResampling) {
        analysis.targetLengthSamples = static_cast<uint32_t>(
            analysis.originalLengthSamples * (sampleRate_ / analysis.originalSampleRate)
        );
    } else {
        analysis.targetLengthSamples = analysis.originalLengthSamples;
    }
    
    // 4. Memory requirement
    analysis.memoryRequired = analysis.targetLengthSamples * sizeof(float);
    analysis.isValid = validateFileAnalysis(analysis);
    
    return analysis;
}

LoadedSample SampleLoader::loadWavFile(const juce::File& file, uint8_t midiNote, uint8_t dynamicLevel) {
    LoadedSample result;
    
    // 1. Analýza první
    FileAnalysis analysis = analyzeWavFile(file);
    if (!analysis.isValid) {
        throw std::runtime_error("Invalid WAV file: " + file.getFullPathName().toStdString());
    }
    
    // 2. Alokace přesné velikosti
    result.audioData = std::make_unique<float[]>(analysis.targetLengthSamples);
    result.lengthSamples = analysis.targetLengthSamples;
    result.midiNote = midiNote;
    result.dynamicLevel = dynamicLevel;
    result.isGenerated = false;
    result.sourcePath = file.getFullPathName();
    
    // 3. Načtení dat
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager_.createReaderFor(file));
    
    if (analysis.needsResampling) {
        // Resample data
        // ... resampling logic
    } else {
        // Direct load
        reader->read(result.audioData.get(), 1, 0, analysis.originalLengthSamples);
    }
    
    return result;
}
```

### 2. Rozšířený SampleLibrary

**SampleLibrary.h změny**
```cpp
struct SampleSegment {
    std::array<std::unique_ptr<float[]>, 8> dynamicLayers;
    std::array<uint32_t, 8> layerLengthSamples;  // Variable length per layer
    uint8_t midiNote;
    std::array<bool, 8> layerAllocated{false};
    
    uint32_t getLayerLength(uint8_t dynamicLevel) const {
        return (dynamicLevel < 8) ? layerLengthSamples[dynamicLevel] : 0;
    }
    
    const float* getLayerData(uint8_t dynamicLevel) const {
        return (dynamicLevel < 8 && layerAllocated[dynamicLevel]) 
               ? dynamicLayers[dynamicLevel].get() : nullptr;
    }
};

class SampleLibrary {
public:
    // Rozšířené API
    const float* getSampleData(uint8_t midiNote, uint8_t dynamicLevel) const;
    uint32_t getSampleLength(uint8_t midiNote, uint8_t dynamicLevel) const;
    uint8_t velocityToDynamicLevel(uint8_t velocity) const;
    
    // Loading statistics
    struct LoadingStats {
        int totalSamples;
        int loadedFromFiles;
        int generatedSines;
        size_t totalMemoryUsed;
        double loadingTimeSeconds;
    };
    
    LoadingStats getLoadingStats() const { return loadingStats_; }
    
private:
    LoadingStats loadingStats_;
    void storeSample(const LoadedSample& sample);
    
    static constexpr std::array<float, 8> DYNAMIC_AMPLITUDES = {
        0.05f, 0.12f, 0.22f, 0.35f, 0.50f, 0.68f, 0.85f, 1.00f
    };
};
```

**SampleLibrary.cpp změny**
```cpp
void SampleLibrary::initialize(double sampleRate) {
    auto startTime = juce::Time::getMillisecondCounterHiRes();
    
    sampleRate_ = sampleRate;
    clear();
    
    // Use SampleLoader
    SampleLoader loader(sampleRate);
    juce::File instrumentDir = SampleLoader::getDefaultInstrumentDirectory();
    
    // Ensure directory exists
    if (!instrumentDir.exists()) {
        instrumentDir.createDirectory();
        logger_.log("SampleLibrary", "info", "Created instrument directory: " + instrumentDir.getFullPathName());
    }
    
    auto progressCallback = [this](int current, int total, const juce::String& status) {
        logger_.log("SampleLibrary", "info", 
                   "Progress: " + juce::String(current) + "/" + juce::String(total) + " - " + status);
    };
    
    std::vector<LoadedSample> loadedSamples = loader.loadInstrument(instrumentDir, progressCallback);
    
    // Store samples
    loadingStats_.totalSamples = static_cast<int>(loadedSamples.size());
    for (const auto& sample : loadedSamples) {
        storeSample(sample);
        if (sample.isGenerated) {
            loadingStats_.generatedSines++;
        } else {
            loadingStats_.loadedFromFiles++;
        }
        loadingStats_.totalMemoryUsed += sample.lengthSamples * sizeof(float);
    }
    
    loadingStats_.loadingTimeSeconds = (juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0;
    
    logger_.log("SampleLibrary", "info", 
               "Loading complete: " + juce::String(loadingStats_.loadedFromFiles) + " WAV files, " + 
               juce::String(loadingStats_.generatedSines) + " generated, " +
               juce::String(loadingStats_.totalMemoryUsed / (1024*1024)) + "MB, " +
               juce::String(loadingStats_.loadingTimeSeconds, 2) + "s");
}

uint8_t SampleLibrary::velocityToDynamicLevel(uint8_t velocity) const {
    // Map velocity 0-127 to dynamic level 0-7
    return std::min(7, velocity / 16);
}
```

### 3. Voice Management úpravy

**SynthVoice.h změny**
```cpp
class SynthVoice {
private:
    uint8_t currentDynamicLevel_{0};
    uint32_t currentSampleLength_{0};  // Variable per sample
    
public:
    void start(uint8_t midiNote, uint8_t velocity, const SampleLibrary& library);
    uint8_t getDynamicLevel() const { return currentDynamicLevel_; }
};
```

**SynthVoice.cpp změny**
```cpp
void SynthVoice::start(uint8_t midiNote, uint8_t velocity, const SampleLibrary& library) {
    reset();
    
    midiNote_ = midiNote;
    velocity_ = velocity;
    currentDynamicLevel_ = library.velocityToDynamicLevel(velocity);
    
    // Get specific dynamic layer
    sampleData_ = library.getSampleData(midiNote, currentDynamicLevel_);
    currentSampleLength_ = library.getSampleLength(midiNote, currentDynamicLevel_);
    
    if (!sampleData_ || currentSampleLength_ == 0) {
        logger_.log("SynthVoice", "error", 
                   "Invalid sample for note " + juce::String((int)midiNote) + 
                   " level " + juce::String((int)currentDynamicLevel_));
        isActive_ = false;
        return;
    }
    
    position_ = 0;
    isActive_ = true;
    
    logger_.log("SynthVoice", "debug", 
               "Started note " + juce::String((int)midiNote) + 
               " level " + juce::String((int)currentDynamicLevel_) +
               " length " + juce::String(currentSampleLength_) + " samples");
}

void SynthVoice::render(float* outputBuffer, int numSamples) {
    if (!isActive_ || sampleData_ == nullptr || currentSampleLength_ == 0)
        return;
    
    // No real-time gain - pre-computed in dynamic layers
    for (int i = 0; i < numSamples; ++i) {
        if (position_ >= currentSampleLength_) {
            stop();  // Natural end based on actual sample length
            break;
        }
        outputBuffer[i] += sampleData_[position_];
        ++position_;
    }
}
```

## Dynamic Level Mapping

### Velocity → Dynamic Level
```cpp
uint8_t velocityToDynamicLevel(uint8_t velocity) {
    return std::min(7, velocity / 16);
}
```

### Dynamic Amplitudes
```cpp
static constexpr std::array<float, 8> DYNAMIC_AMPLITUDES = {
    0.05f,  // vel0 - pppp (velocity 1-16)
    0.12f,  // vel1 - ppp  (velocity 17-32)
    0.22f,  // vel2 - pp   (velocity 33-48)
    0.35f,  // vel3 - p    (velocity 49-64)
    0.50f,  // vel4 - mp   (velocity 65-80)
    0.68f,  // vel5 - mf   (velocity 81-96)
    0.85f,  // vel6 - f    (velocity 97-112)
    1.00f   // vel7 - ff   (velocity 113-127)
};
```

## Implementation Strategy

### Phase 1: SampleLoader Infrastructure
1. Create SampleLoader.h/cpp
2. Implement file analysis without loading
3. Add WAV format support
4. Test with single sample

### Phase 2: Dynamic Layers Integration
1. Modify SampleSegment structure
2. Update SampleLibrary API
3. Implement velocity→dynamic level mapping
4. Test with sine generation fallback

### Phase 3: Voice System Updates
1. Update SynthVoice for dynamic levels
2. Remove real-time gain calculation
3. Add variable length support
4. Test complete audio chain

### Phase 4: Error Handling & Optimization
1. Comprehensive validation
2. Memory usage optimization
3. Loading progress reporting
4. Cross-platform path handling

## Memory & Performance Characteristics

### Memory Usage
- **Variable allocation:** Based on actual sample lengths
- **Typical usage:** 200-800MB (vs 1.6GB fixed)
- **Peak usage:** Up to 4GB for very long samples
- **Optimization:** Shared data for identical samples

### Performance Benefits
- **Faster rendering:** No real-time multiplication
- **Natural endings:** Samples end when audio ends
- **Flexible dynamics:** 8 distinct amplitude levels
- **Efficient fallback:** Sine generation only when needed

### File System Benefits
- **User-friendly:** Drop WAV files in known location
- **Incremental:** Add samples as needed
- **Backward compatible:** Works without any external files
- **Debuggable:** Clear file naming and loading logs

## Technical Requirements

### WAV File Specifications
- **Format:** Standard WAV (16-bit, 24-bit, 32-bit float)
- **Channels:** Mono preferred (stereo will be converted)
- **Sample Rate:** Any (will be resampled to project rate)
- **Length:** Variable (0.1s minimum, no maximum)

### Loading Process
1. **Analyze** file metadata (length, sample rate)
2. **Validate** file integrity and specifications
3. **Calculate** memory requirements
4. **Allocate** exact memory needed
5. **Load** and optionally resample audio data
6. **Fallback** to sine generation if file missing/invalid

### Error Handling
- **Invalid files:** Skip with warning, generate sine fallback
- **Memory errors:** Graceful degradation, detailed logging
- **Directory issues:** Auto-create missing directories
- **Loading failures:** Continue with available samples

Tento systém umožňuje postupný přechod od synthetic sine waves k real instrument samples se zachováním plné funkcionality a performance optimizations.