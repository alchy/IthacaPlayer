# IthacaPlayer - MIDI Samplovací Přehrávač v JUCE

## Přehled Projektu

**IthacaPlayer** je softwarový samplovací přehrávač vytvořený pomocí frameworku [JUCE](https://github.com/juce-framework/JUCE/), určený pro přehrávání zvukových vzorků ve formátu `.wav` spouštěných MIDI vstupem. 

### Klíčové Vlastnosti

- **Polyfonní přehrávání** až 16 hlasů současně
- **Dynamické mapování velocity** na základě dB úrovní vzorků
- **Inteligentní generování chybějících vzorků** pomocí pitch-shiftingu
- **Komplexní MIDI zpracování** (Note On/Off, Pitch Wheel, CC, SysEx)
- **Cross-platform kompatibilita** (Windows, macOS, Linux)
- **Inspirace produkčními systémy** a JUCE best practices

### Formát Vzorků

Vzorky musí dodržovat pojmenovací konvenci:
```
mNNN-NOTA-DbLvl-X.wav
```
- `NNN`: MIDI nota (000-127)
- `NOTA`: Název noty (např. C_4, Bb_5)
- `X`: dB úroveň (negativní hodnoty, 0 = plná hlasitost)
- Příklad: `m060-C_4-DbLvl-20.wav`

## Architektura Systému

### Hierarchie Tříd

```
IthacaPlayer
├── Core Components
│   ├── Config                 # Konfigurace konstant
│   ├── AudioFile             # Reprezentace vzorku + metadata
│   └── DirectoryManager      # Správa dočasných souborů
│
├── Sample Processing Pipeline
│   ├── VelocityMapper        # Mapování velocity → vzorky
│   ├── SampleGenerator       # Generování chybějících not
│   └── AudioProcessor        # Základní audio operace
│
├── MIDI & Audio Engine
│   ├── MidiProcessor         # MIDI události a parsování
│   ├── Sampler              # Koordinátor hlasů a zvuků
│   ├── SamplerVoice         # Individuální hlas pro přehrávání
│   └── SamplerSound         # Definice rozsahu not
│
└── Application Layer
    └── MainAudioComponent    # Hlavní audio interface
```

### Datový Tok

1. **Inicializace**: Načtení vzorků → Vytvoření velocity mapy
2. **Preprocessing**: Generování chybějících not → Cache management
3. **Runtime**: MIDI vstup → Mapování velocity → Přehrávání vzorku

## Detailní Specifikace Tříd

### 📋 Config (`Config.h`)

**Účel**: Centralizované konstanty pro konfiguraci systému

```cpp
namespace Config {
    constexpr int VELOCITY_LEVELS = 8;           // Počet velocity vrstev
    constexpr int MIDI_VELOCITY_MAX = 127;       // Maximální MIDI velocity
    constexpr int MAX_PITCH_SHIFT = 12;          // Limit pitch-shift (půltóny)
    constexpr Range<int> MIDI_NOTE_RANGE{21, 109}; // A0–C8
    constexpr const char* TEMP_DIR_NAME = "samples_tmp";
}
```

### 🎵 AudioFile (`AudioFile.h/.cpp`)

**Účel**: Immutable reprezentace audio vzorku s metadaty

| Metoda | Parametry | Návratová Hodnota | Popis |
|--------|-----------|-------------------|-------|
| `AudioFile()` | `File, int, String, int` | - | Konstruktor s validací |
| `fromFile()` | `File` | `unique_ptr<AudioFile>` | Factory method s regex parsing |
| `isValid()` | - | `bool` | Validace metadat |

**Validační Pravidla**:
- MIDI nota v rozsahu 0-127
- dB úroveň ≤ 0 (negativní nebo nula)
- Existující WAV soubor

### 🎯 VelocityMapper (`VelocityMapper.h/.cpp`)

**Účel**: Mapování MIDI velocity na optimální vzorek podle dB úrovní

| Metoda | Parametry | Návratová Hodnota | Popis |
|--------|-----------|-------------------|-------|
| `buildVelocityMap()` | `File inputDir` | `void` | Scan & build kompletní mapy |
| `getSampleForVelocity()` | `int note, int velocity` | `File` | O(1) lookup vzorku |
| `addGeneratedSample()` | `int note, pair<int,int> range, File` | `void` | Přidání cache vzorku |
| `getAvailableNotes()` | - | `set<int>` | Dostupné MIDI noty |

**Algoritmus Mapování**:
1. Seřazení vzorků podle dB úrovně (ascending)
2. Rozdělení velocity rozsahu 0-127 na N segmentů
3. Mapování každého segmentu na odpovídající dB úroveň

### 🔧 SampleGenerator (`SampleGenerator.h/.cpp`)

**Účel**: Inteligentní generování chybějících vzorků s cache managementem

| Metoda | Parametры | Návratová Hodnota | Popis |
|--------|-----------|-------------------|-------|
| `generateMissingNotes()` | `File tempDir` | `void` | Batch generování + cache |
| `generateNoteWithVelocities()` | `int note, vector<int> sources` | `bool` | Multi-velocity generování |
| `pitchShiftSample()` | `File in, File out, double ratio` | `bool` | High-quality resampling |
| `findOptimalSource()` | `int targetNote` | `int` | Hledání nejlepšího zdroje |

**Optimalizace**:
- **Cache-first approach**: Kontrola existujících vzorků před generováním
- **Quality preservation**: Zachování originálních dB úrovní
- **Batch processing**: Efektivní zpracování všech velocity úrovní najednou

### 🎹 MidiProcessor (`MidiProcessor.h/.cpp`)

**Účel**: Robustní zpracování MIDI událostí s error handling

| MIDI Event | Handler Method | Parametры | Akce |
|------------|----------------|-----------|------|
| Note On | `handleNoteOn()` | `channel, note, velocity` | Trigger sampler voice |
| Note Off | `handleNoteOff()` | `channel, note, velocity` | Release voice |
| Pitch Wheel | `handlePitchWheel()` | `channel, value` | Modulate active voices |
| Control Change | `handleControlChange()` | `channel, cc, value` | Parameter mapping |
| System Exclusive | `handleSysEx()` | `data[]` | Advanced control |

**Features**:
- **Running Status** support pro efektivní MIDI stream
- **Timestamp-based** event scheduling
- **Error Recovery** při poškozených MIDI datech

### 🔊 Sampler Engine

#### SamplerVoice (`SamplerVoice.h/.cpp`)

**Účel**: Jednotlivý polyfonní hlas s kompletním lifecycle managementem

| Fáze | Metoda | Parametры | Popis |
|------|--------|-----------|-------|
| **Allocation** | `canPlaySound()` | `SynthesiserSound*` | Voice compatibility check |
| **Trigger** | `startNote()` | `note, velocity, sound, pitch` | Sample loading & playback start |
| **Rendering** | `renderNextBlock()` | `AudioBuffer&, start, count` | Real-time audio generation |
| **Release** | `stopNote()` | `velocity, allowTailOff` | Graceful voice termination |

**Advanced Features**:
- **Dynamic sample loading** při startNote()
- **Seamless looping** pro dlouhé noty
- **Real-time pitch modulation** přes pitch wheel
- **Volume envelope** pro smooth attack/release

#### Sampler (`Sampler.h/.cpp`) 

**Účel**: Koordinátor hlasů s inteligentním voice stealing

| Algoritmus | Implementace | Optimalizace |
|------------|--------------|--------------|
| **Voice Allocation** | Round-robin + LRU | Minimální audio glitches |
| **Voice Stealing** | Oldest note priority | Zachování důležitých not |
| **Polyphony Management** | 16 hlasů max | CPU/Memory balance |

### 🎛️ MainAudioComponent (`MainAudioComponent.h/.cpp`)

**Účel**: Hlavní audio interface s JUCE AudioAppComponent

| Audio Callback | Implementace | Optimalizace |
|----------------|--------------|--------------|
| `prepareToPlay()` | Sample rate setup + buffer allocation | Lock-free initialization |
| `getNextAudioBlock()` | Sampler rendering + output mixing | Real-time thread safety |
| `releaseResources()` | Cleanup + resource deallocation | Graceful shutdown |

## Implementační Detaily

### 🔄 Velocity Mapping Algoritmus

```cpp
// Pseudo-kód pro optimální velocity mapping
vector<pair<int,int>> VelocityMapper::calculateVelocityRanges(
    const vector<int>& dbLevels) {
    
    sort(dbLevels.begin(), dbLevels.end()); // Ascending dB order
    
    const int totalRange = Config::MIDI_VELOCITY_MAX;
    const int numLevels = dbLevels.size();
    const double step = static_cast<double>(totalRange) / numLevels;
    
    vector<pair<int,int>> ranges;
    for (int i = 0; i < numLevels; ++i) {
        int start = static_cast<int>(i * step);
        int end = (i == numLevels - 1) ? totalRange - 1 : 
                  static_cast<int>((i + 1) * step) - 1;
        ranges.emplace_back(start, end);
    }
    return ranges;
}
```

### 🎼 Pitch Shifting Strategy (Piano-Optimized)

**Approach**: Simple resampling s JUCE `ResamplingAudioSource` - optimalizováno pro piano vzorky

```cpp
// Piano-optimized pitch shifting via resampling
bool SampleGenerator::pitchShiftSample(const File& input, 
                                       const File& output, 
                                       double semitones) {
    const double ratio = std::pow(2.0, semitones / 12.0);
    
    // Load audio with JUCE
    AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    auto reader = formatManager.createReaderFor(input);
    if (!reader) return false;
    
    // JUCE ResamplingAudioSource for high-quality resampling
    ResamplingAudioSource resampler(reader.get(), false, reader->numChannels);
    resampler.setResamplingRatio(1.0 / ratio); // Inverse ratio for pitch shift
    resampler.prepareToPlay(8192, reader->sampleRate);
    
    // Process resampled audio
    AudioBuffer<float> outputBuffer(reader->numChannels, 
                                   static_cast<int>(reader->lengthInSamples * ratio));
    
    AudioSourceChannelInfo channelInfo;
    channelInfo.buffer = &outputBuffer;
    channelInfo.startSample = 0;
    channelInfo.numSamples = outputBuffer.getNumSamples();
    
    resampler.getNextAudioBlock(channelInfo);
    
    // Save result (note: duration changes with pitch shift - this is expected!)
    return saveBufferToFile(outputBuffer, output, reader->sampleRate, 
                           reader->bitsPerSample);
}
```

**Pitch Shift Characteristics**:
- ✅ **Duration changes** s pitch shift (realistic pro piano)
- ✅ **Minimální artefakty** pro ±1-3 půltóny (typický range)
- ✅ **Hard limit 12 půltónů** (oktáva) - nad tím se negeneruje
- ✅ **Preserves all velocity levels** z source noty

**Sample Generation Logic** (zachováno z Python):
```cpp
// Zachování Python algoritmu pro výběr source samples
void SampleGenerator::generateNoteWithVelocities(int targetNote, 
                                                 const vector<int>& availableNotes) {
    // 1. Najdi nejbližší dostupnou notu
    int sourceNote = findNearestNote(targetNote, availableNotes);
    
    // 2. Získej VŠECHNY velocity levels pro source notu
    auto sourceSamples = velocityMapper.getSamplesForNote(sourceNote);
    
    // 3. Pro každý source sample vytvoř resampled version
    for (const auto& sourceSample : sourceSamples) {
        double semitoneShift = targetNote - sourceNote;
        if (abs(semitoneShift) <= Config::MAX_PITCH_SHIFT) {
            generateResampledSample(targetNote, sourceSample, semitoneShift);
        }
    }
    
    // 4. Zachová se STEJNÝ počet velocity levels jako má source nota
}
```

### 🧠 Intelligent Caching System

```cpp
// Smart caching with metadata preservation
class SampleCache {
    struct CacheEntry {
        File originalSource;
        int sourceMidiNote;
        int targetMidiNote; 
        int dbLevel;
        double pitchRatio;
        TimeStamp generated;
        bool isValid() const;
    };
    
    std::map<std::pair<int,int>, CacheEntry> cache; // (note, velocity) -> entry
    
public:
    bool hasCachedSample(int note, int velocity) const;
    File getCachedSample(int note, int velocity) const;
    void addToCache(int note, int velocity, const CacheEntry& entry);
    void invalidateOutdated(TimeStamp threshold);
};
```

## Optimalizace & Performance

### 🚀 Real-time Audio Optimizations

- **Lock-free data structures** pro MIDI → Audio komunikaci
- **Pre-allocated buffers** pro elimináciu alokací v audio threadu
- **SIMD instructions** pro kritické audio operace
- **Branch prediction optimization** v hot paths

### 💾 Memory Management

- **Smart pointer hierarchie** pro automatic resource management
- **Object pooling** pro často používané objekty (AudioBuffer, etc.)
- **Memory-mapped file I/O** pro velké sample kolekce
- **Circular buffer design** pro MIDI event queue

### 🔧 Error Handling & Robustness

```cpp
// Comprehensive error handling strategy
class ErrorHandler {
public:
    enum class Severity { Info, Warning, Error, Critical };
    
    static void report(Severity level, const String& component, 
                      const String& message, const String& context = {});
    
    static bool attemptRecovery(const String& operation);
    static void logPerformanceMetrics();
};

#define ITHACA_TRY_CATCH(operation, fallback) \
    try { \
        operation; \
    } catch (const std::exception& e) { \
        ErrorHandler::report(ErrorHandler::Severity::Error, \
                           __FUNCTION__, e.what()); \
        fallback; \
    }
```

## Testování & Validace

### 🧪 Unit Testing Framework

```cpp
// Test suite pro kritické komponenty
class VelocityMapperTest : public UnitTest {
public:
    void runTest() override {
        beginTest("Velocity mapping accuracy");
        
        // Test data setup
        VelocityMapper mapper;
        mapper.buildVelocityMap(getTestSampleDirectory());
        
        // Validate mapping consistency
        expectEquals(mapper.getSampleForVelocity(60, 0).exists(), true);
        expectEquals(mapper.getSampleForVelocity(60, 127).exists(), true);
        
        // Performance benchmarks
        auto start = Time::getCurrentTime();
        for (int i = 0; i < 10000; ++i) {
            mapper.getSampleForVelocity(60, i % 128);
        }
        auto elapsed = Time::getCurrentTime() - start;
        expect(elapsed.inMilliseconds() < 100, "Lookup performance");
    }
};
```

### 📊 Performance Profiling

- **Audio thread latency** monitoring
- **Memory allocation** tracking v real-time paths
- **Cache hit/miss ratios** pro sample lookup
- **MIDI timing accuracy** measurements

## Deployment & Distribution

### 📦 Build Configuration

```cmake
# CMakeLists.txt excerpt for cross-platform build
find_package(JUCE REQUIRED 
    COMPONENTS 
        juce_core
        juce_audio_basics
        juce_audio_formats
        juce_audio_devices
        juce_audio_utils
        juce_audio_processors
)

juce_add_plugin(IthacaPlayer
    VERSION 1.0.0
    COMPANY_NAME "Your Company"
    PLUGIN_MANUFACTURER_CODE YCmp
    PLUGIN_CODE Itha
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "IthacaPlayer"
)

target_compile_features(IthacaPlayer PRIVATE cxx_std_17)
target_compile_definitions(IthacaPlayer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
```

### 🔧 Configuration Management

```cpp
// Uživatelské nastavení s persistence
class IthacaConfig {
    struct Settings {
        String sampleDirectory;
        bool cleanTempOnStartup = false;
        int maxVoices = 16;
        double pitchWheelRange = 2.0; // semitones
        bool enableAdvancedLogging = false;
    };
    
    Settings settings;
    File configFile;
    
public:
    void loadFromFile();
    void saveToFile() const;
    void resetToDefaults();
    
    // Getters/setters with validation
    void setSampleDirectory(const String& path);
    String getSampleDirectory() const { return settings.sampleDirectory; }
    // ... další nastavení
};
```

---

## 🏗️ Build System & Deployment

### Build Configuration
```cmake
# CMakeLists.txt for IthacaPlayer
cmake_minimum_required(VERSION 3.22)
project(IthacaPlayer VERSION 1.0.0)

# Target: Windows 10+ x64 only
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_GENERATOR_PLATFORM x64)

# JUCE Framework (latest version)
find_package(JUCE CONFIG REQUIRED)

juce_add_plugin(IthacaPlayer
    VERSION 1.0.0
    COMPANY_NAME "Your Company"
    PLUGIN_MANUFACTURER_CODE YCmp
    PLUGIN_CODE Itha
    FORMATS VST3                    # VST3 only for now
    PRODUCT_NAME "IthacaPlayer"
    DESCRIPTION "MIDI Sampler Player"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

# JUCE Modules (minimal set for audio + MIDI)
target_link_libraries(IthacaPlayer PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_audio_devices
    juce::juce_audio_utils
    juce::juce_audio_processors
)

# Windows-specific optimizations
target_compile_definitions(IthacaPlayer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    WIN32_LEAN_AND_MEAN=1
)

# Release optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(IthacaPlayer PRIVATE 
        /O2 /arch:AVX2 /fp:fast)
endif()
```

### Development Environment
- **Visual Studio 2022** (latest)
- **Windows 10/11 x64** target
- **JUCE 7.x+** (latest stable)
- **C++17** standard minimum

### Plugin Architecture
```cpp
// Minimal VST3 plugin structure
class IthacaPlayerProcessor : public juce::AudioProcessor {
    std::unique_ptr<Sampler> sampler;
    std::unique_ptr<MidiProcessor> midiProcessor;
    
public:
    // VST3 AudioProcessor interface
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    // ... minimal VST3 implementation
};

// Simple debug GUI (optional for prototype)
class IthacaPlayerEditor : public juce::AudioProcessorEditor {
    juce::TextEditor debugOutput;
    
public:
    // Minimal GUI: just debug text output window
    void paint(juce::Graphics&) override;
    void resized() override;
    
    // Log debug messages to GUI
    void appendDebugMessage(const juce::String& message);
};
```

### Distribution Strategy
- **ZIP distribution** (no installer yet)
- **Self-contained**: No external dependencies beyond system DLLs
- **Portable**: Can run from any folder with sample directory
- **Configuration**: Sample directory via command line or config file

## 📋 Implementation Roadmap

### 🎯 Phase 1: Core Functionality (Minimal Viable Prototype)
- [ ] **Config** constants definition
- [ ] **AudioFile** parsing with regex validation
- [ ] **VelocityMapper** basic velocity ranges  
- [ ] **Simple MidiProcessor** (Note On/Off, Pitch Wheel)
- [ ] **SamplerVoice** basic sample playback
- [ ] **Cache management** (APPDATA storage)
- [ ] **VST3 wrapper** (headless or simple debug GUI)

**Goal**: Playing piano samples via MIDI input

### 🚀 Phase 2: Audio Quality
- [ ] **JUCE ResamplingAudioSource** pitch shifting
- [ ] **SampleGenerator** with intelligent source selection
- [ ] **Voice stealing** algorithm (mixle_queue)
- [ ] **Error handling** (corrupt files, disk space)

**Goal**: High-quality sample generation and robust playback

### 🎛️ Phase 3: Polish & Optimization  
- [ ] **Performance monitoring** and optimization
- [ ] **Advanced MIDI** (CC, SysEx logging)
- [ ] **Configuration system** (sample directory selection)
- [ ] **Plugin GUI** (beyond debug output)
- [ ] **Unit tests** and validation

**Goal**: Production-ready plugin

---

## 🚀 Ready to Start Implementation

**Máme kompletní specifikaci!** Zadání je nyní připraveno pro přímou C++ implementaci s:

✅ **Konkrétní performance targets** (5-20ms latency, 512MB-4GB memory)  
✅ **Pitch shifting strategy** (JUCE ResamplingAudioSource pro piano)  
✅ **Voice management** (16 hlasů + mixle_queue stealing)  
✅ **MIDI processing** (JUCE-integrated, auto-detect devices)  
✅ **Cache management** (Windows APPDATA, persistent, no limits)  
✅ **Build system** (VS2022, Windows 10+ x64, VST3, latest JUCE)

**Chcete začít s konkrétní implementací některé třídy, nebo máte ještě otázky k zadání?**
