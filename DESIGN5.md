# IthacaPlayer - MIDI Samplovací Přehrávač v JUCE

## 📖 Úvod do projektu

### Co je IthacaPlayer?

**IthacaPlayer** je pokročilý softwarový samplovací přehrávač, který umožňuje přehrávání audio vzorků spouštěných MIDI vstupem. Projekt kombinuje moderní C++ technologie s frameworkem JUCE pro vytvoření profesionálního nástroje s důrazem na kvalitu zvuku a real-time výkon.

### Klíčové vlastnosti

- **🎹 Polyfonní přehrávání** - až 16 současných hlasů s inteligentním voice stealing
- **🎯 Dynamické mapování velocity** - přesné mapování MIDI velocity na audio vzorky podle dB úrovní
- **🔧 Automatické generování vzorků** - vytváření chybějících not pomocí pitch-shiftingu
- **🎼 Komplexní MIDI zpracování** - podpora Note On/Off, Pitch Wheel, Control Change, SysEx
- **💾 Inteligentní cache systém** - perzistentní ukládání generovaných vzorků
- **🚀 Cross-platform kompatibilita** - Windows, macOS, Linux prostřednictvím JUCE

### Formát audio vzorků

IthacaPlayer pracuje se vzorky v pojmenovací konvenci:
```
mNNN-NOTA-DbLvl-X.wav
```

**Příklady:**
- `m060-C_4-DbLvl-20.wav` - MIDI nota 60 (C4), DbLvl -20dB
- `m072-C_5-DbLvl-0.wav` - MIDI nota 72 (C5), DbLvl 0dB (plná hlasitost)

### Inspirace a filozofie

Projekt je inspirován produkčními syntezátory a důraz klade na:
- **Proven algorithms** - použití battle-tested algoritmů z produkčních systémů
- **Real-time performance** - optimalizace pro nízkou latenci a vysoký výkon
- **Robust error handling** - graceful handling všech možných chybových stavů
- **User experience** - jednoduché použití s mocnými funkcemi pod povrchem

---

## 🎯 Cílová skupina a použití

### Pro koho je IthacaPlayer určen?

- **Hudební producenti** - potřebující kvalitní sample playback v DAW
- **Live performeři** - vyžadující spolehlivý nástroj pro živé vystoupení  
- **Sound designeři** - pracující s rozsáhlými kolekcemi vzorků
- **Vývojáři audio software** - hledající referenční implementaci

### Typické use cases

1. **Piano sample libraries** - přehrávání multi-velocity piano vzorků
2. **Orchestrální nástroje** - strings, brass, woodwinds s velocity switching
3. **Drum machines** - percussion vzorky s velocity sensitivity
4. **Sound effects** - environmental sounds, foley, cinematic textures

---

## 📊 Technické požadavky a limity

### Performance cíle

| Metrika | Cílová hodnota | Popis |
|---------|----------------|-------|
| **Audio latence** | 5-20 ms | Přijatelná latence pro real-time performance |
| **Memory usage** | 512MB - 4GB | Flexibilní podle velikosti sample kolekce |
| **Startup time** | První spuštění: neomezeno<br>Další spuštění: <3s | Cache build vs. rychlé načtení |
| **Voice polyphony** | 16 hlasů | Optimální balance výkon/kvalita |
| **Pitch shift range** | ±12 půltónů | Jedna oktáva maximum pro zachování kvality |

### Podporované platformy

- **Primary target**: Windows 10/11 x64
- **Build system**: Visual Studio 2022, CMake
- **Audio framework**: JUCE (latest stable)
- **Plugin formats**: VST3, Standalone aplikace

### Hardware požadavky

- **CPU**: Multi-core procesor (Intel i5/AMD Ryzen 5 nebo lepší)
- **RAM**: Minimálně 4GB (doporučeno 8GB+)
- **Storage**: SSD doporučeno pro sample loading
- **Audio interface**: ASIO kompatibilní (doporučeno)

---

## 🏗️ Architektura systému

### Přehled komponent

IthacaPlayer se skládá z několika klíčových subsystémů:

```
┌─────────────────────────────────────────────────────────┐
│                    IthacaPlayer                         │
├─────────────────────────────────────────────────────────┤
│  User Interface (VST3 Plugin / Standalone)             │
├─────────────────────────────────────────────────────────┤
│  Audio Engine                                           │
│  ├── MIDI Processor ──┐                                 │
│  ├── Voice Manager ───┼── Core Audio Pipeline           │
│  ├── Sample Engine ───┘                                 │
│  └── Effects Chain (future)                             │
├─────────────────────────────────────────────────────────┤
│  Sample Management                                       │
│  ├── Velocity Mapper                                    │
│  ├── Sample Generator                                    │
│  ├── Cache Manager                                       │
│  └── File System                                        │
├─────────────────────────────────────────────────────────┤
│  Foundation Layer                                        │
│  ├── Configuration                                      │
│  ├── Error Handling                                     │
│  ├── Performance Monitor                                │
│  └── Logging System                                     │
├─────────────────────────────────────────────────────────┤
│              JUCE Framework                             │
└─────────────────────────────────────────────────────────┘
```

### Datový tok

1. **Inicializace**: Načtení vzorků → Vytvoření velocity mapy → Cache validation/build
2. **Runtime**: MIDI vstup → Voice allocation → Sample selection → Audio rendering → Output

### Threading model

- **Main Thread**: UI, konfigurace, file operations
- **Audio Thread**: Real-time audio processing (highest priority)
- **Worker Thread**: Sample generation, cache management
- **MIDI Thread**: Integrován s audio thread (JUCE standard)

---

## 🎼 Principy MIDI zpracování

### Podporované MIDI události

| MIDI Event | Implementace | Popis |
|------------|--------------|-------|
| **Note On** | ✅ Full | Spuštění noty s velocity mapováním |
| **Note Off** | ✅ Full | Zastavení noty s release handling |
| **Pitch Wheel** | ✅ Full | Globální pitch modulation všech hlasů |
| **Control Change** | 🔶 Basic | Debug logging, mapování na parametry |
| **System Exclusive** | 🔶 Basic | Custom parameter control |
| **Program Change** | ❌ Future | Instrument switching |

### Voice management filosofie

**Cíl**: Inteligentní alokace hlasů pro naturální hudební výraz

**Strategie:**
1. **Note restart** - Stejná nota restartuje existující hlas
2. **LRU allocation** - Nejstarší uvolněný hlas má prioritu  
3. **Intelligent stealing** - Pokročilý algoritmus pro voice stealing
4. **Graceful degradation** - Smooth handling při vyčerpání hlasů

---

## 🎯 Sample Management

### Dynamické velocity mapping systém

**Princip**: Dynamické mapování MIDI velocity (0-127) na audio vzorky podle jejich skutečně dostupných dB úrovní

**Algoritmus (inspirovaný Python kódem):**
1. **Scan directory** - Nalezení všech .wav souborů
2. **Parse metadata** - Extrakce MIDI nota + dB level z názvu souboru
3. **Group by note** - Seskupení vzorků podle MIDI noty
4. **Sort by dB** - Seřazení podle dB úrovně (vzestupně - od nejtišších)
5. **Dynamic velocity ranges** - Rozdělení velocity 0-127 podle skutečného počtu vzorků

**Příklad dynamického mapování:**

```
Nota 60 (C4) má 3 vzorky:          Velocity rozsahy:
├── m060-C_4-DbLvl-30.wav          ├── Velocity 0-42    → DbLvl-30 (nejtiší)
├── m060-C_4-DbLvl-15.wav          ├── Velocity 43-85   → DbLvl-15 (střední)  
└── m060-C_4-DbLvl-0.wav           └── Velocity 86-127  → DbLvl-0  (nejhlasitější)

Nota 72 (C5) má 5 vzorků:          Velocity rozsahy:
├── m072-C_5-DbLvl-40.wav          ├── Velocity 0-25    → DbLvl-40
├── m072-C_5-DbLvl-25.wav          ├── Velocity 26-50   → DbLvl-25
├── m072-C_5-DbLvl-15.wav          ├── Velocity 51-75   → DbLvl-15
├── m072-C_5-DbLvl-8.wav           ├── Velocity 76-100  → DbLvl-8
└── m072-C_5-DbLvl-0.wav           └── Velocity 101-127 → DbLvl-0
```

**Výhody dynamického přístupu:**
- ✅ **Adaptabilní** - Funguje s jakýmkoliv počtem velocity vrstev (2-16+)
- ✅ **Optimální využití** - Každý dostupný vzorek má svůj velocity rozsah
- ✅ **Naturální response** - Plynulé přechody mezi úrovněmi
- ✅ **Scalable** - Automatické přizpůsobení různým sample kolekcím

### Sample generation strategie

**Problém**: Často nemáme vzorky pro všechny MIDI noty (21-108)

**Řešení**: Inteligentní generování pomocí pitch-shiftingu

**Algoritmus:**
1. **Detekce chybějících not** - Scan MIDI range 21-108
2. **Hledání source vzorků** - Nejbližší dostupná nota v range ±12 půltónů
3. **Pitch shifting** - JUCE ResamplingAudioSource pro kvalitní resampling
4. **Multi-velocity generation** - Zachování všech velocity úrovní
5. **Cache persistence** - Uložení pro rychlé další použití

**Kvalita vs. Performance trade-off:**
- **Piano vzorky**: Simple resampling je dostačující (minimal artifacts)
- **Duration change**: Akceptováno pro realistický piano sound
- **±12 půltónů limit**: Tvrdý limit pro zachování kvality

---

## 💾 Cache Management

### Filosofie cache systému

**Cíl**: První spuštění = dlouhé (s progress), další spuštění = rychlé

### Cache umístění (Windows)

```
%APPDATA%\IthacaPlayer\samples_tmp\
├── m021-A_0-DbLvl-20-v32.wav
├── m022-Bb_0-DbLvl-20-v32.wav
├── m023-B_0-DbLvl-20-v32.wav
└── ...
```

### Cache lifecycle

1. **Startup check** - Existence cache directory
2. **Validation** - Quick check cache integrity
3. **Build if needed** - Generate missing samples with progress logging
4. **Load existing** - Fast mapping of cached samples
5. **No auto-invalidation** - User manual delete only

### Error handling

- **Corrupt source sample** → Log error + terminate (critical)
- **Insufficient disk space** → Log error + terminate (critical)  
- **Cache corruption** → Clear cache + rebuild automatically

---

## 🔧 Development Environment

### Required tools

- **Visual Studio 2022** (latest version)
- **JUCE Framework** (7.x latest stable)
- **CMake** (3.22+)
- **Git** pro version control

### Project structure

```
IthacaPlayer/
├── Source/
│   ├── Core/           # Config, AudioFile, základní třídy
│   ├── Audio/          # Sampler, SamplerVoice, audio processing
│   ├── MIDI/           # MIDI processing, voice management  
│   ├── Cache/          # Cache management, sample generation
│   ├── Utils/          # Error handling, performance monitoring
│   └── Plugin/         # VST3 wrapper, UI components
├── Resources/          # Assets, presets, documentation
├── Tests/              # Unit tests, integration tests
├── CMakeLists.txt      # Build configuration
└── README.md          # This document
```

### Build configuration

- **Target**: Windows 10+ x64 exclusively
- **Optimization**: /O2 /arch:AVX2 /fp:fast (Release)
- **Standards**: C++17 minimum
- **Dependencies**: JUCE only (self-contained)

---

# 📚 Detailní implementační specifikace

*Následující sekce obsahují podrobné technické detaily pro vývojáře*

---

## 🔍 Core Components

### Config System

Centralizované konstanty a konfigurace aplikace.

```cpp
namespace IthacaPlayer::Config {
    // Performance targets
    constexpr int MIN_AUDIO_LATENCY_MS = 5;
    constexpr int MAX_AUDIO_LATENCY_MS = 20;
    constexpr size_t MIN_MEMORY_USAGE_MB = 512;
    constexpr size_t MAX_MEMORY_USAGE_MB = 4096;
    
    // Audio processing
    constexpr int VELOCITY_LEVELS = 8;
    constexpr int MIDI_VELOCITY_MAX = 127;
    constexpr int MAX_PITCH_SHIFT = 12;
    constexpr Range<int> MIDI_NOTE_RANGE{21, 109};  // A0-C8
    constexpr int MAX_VOICES = 16;
    
    // File system
    constexpr const char* TEMP_DIR_NAME = "samples_tmp";
    
    // Windows optimizations
    constexpr bool ENABLE_WINDOWS_OPTIMIZATIONS = true;
    constexpr bool USE_WASAPI_EXCLUSIVE = true;
}
```

### AudioFile Class

Immutable reprezentace audio vzorku s metadaty.

```cpp
class AudioFile {
    juce::File filepath;
    int midiNote;           // 0-127
    juce::String noteName;  // e.g., "C_4", "Bb_5"
    int dbLevel;            // Negative or 0 (0 = full volume)
    
public:
    // Factory method with regex parsing
    static std::unique_ptr<AudioFile> fromFile(const juce::File& file);
    
    // Validation
    bool isValid() const;
    
    // Getters
    const juce::File& getFile() const { return filepath; }
    int getMidiNote() const { return midiNote; }
    const juce::String& getNoteName() const { return noteName; }
    int getDbLevel() const { return dbLevel; }
    
private:
    AudioFile(juce::File file, int note, juce::String name, int db)
        : filepath(std::move(file)), midiNote(note), 
          noteName(std::move(name)), dbLevel(db) {}
};
```

---

## 🎭 Advanced MIDI Processing

### State Machine MIDI Parser

Robustní MIDI parser inspirovaný produkčními embedded systémy.

```cpp
class AdvancedMidiProcessor {
    // Robust MIDI parsing state machine
    struct MidiParsingState {
        uint8_t currentByte;
        uint8_t messageCount;           // Byte counter for multi-byte messages
        uint8_t messageType;            // Extracted message type
        uint8_t midiChannel;            // MIDI channel (0-15)
        bool runningStatus;             // MIDI running status support
    } parsingState;
    
    struct MidiNote {
        uint8_t key;
        uint8_t velocity;
        uint8_t pitchWheelLSB;
        uint8_t pitchWheelMSB;
        int16_t pitchWheel;             // Combined pitch wheel value
    } currentNote;
    
public:
    // Robust byte-by-byte MIDI parsing (real-time safe)
    void processMidiByte(uint8_t byte) {
        if (byte & 0x80) {  // Status byte detected
            parsingState.messageCount = 0;
            parsingState.runningStatus = false;
            
            if ((byte & 0xF0) == 0xF0) {
                // System message - all bits significant
                parsingState.messageType = byte;
            } else {
                // Channel message - extract type and channel
                parsingState.messageType = byte & 0xF0;
                parsingState.midiChannel = byte & 0x0F;
            }
        }
        
        // Route to appropriate handler
        if ((parsingState.messageType & 0xF0) == 0xF0) {
            processSystemMessage(byte);
        } else {
            processChannelMessage(byte);
        }
    }
    
private:
    void processChannelMessage(uint8_t byte);
    void processNoteOn(uint8_t byte);
    void processPitchWheel(uint8_t byte);
    void processControlChange(uint8_t byte);
    
    // Virtual handlers for JUCE integration
    virtual void triggerNoteOn(int channel, int note, int velocity) = 0;
    virtual void triggerNoteOff(int channel, int note) = 0;
    virtual void triggerPitchWheel(int channel, int pitchWheel) = 0;
    virtual void triggerControlChange(int channel, int controller, int value) = 0;
};
```

### Production-Grade Voice Management

Battle-tested voice stealing algoritmus z produkčního synthesizeru.

```cpp
class AdvancedVoiceManager {
    static constexpr int MAX_VOICES = 16;
    
    struct VoiceState {
        uint8_t currentNote = 0;        // MIDI note currently playing
        uint8_t velocity = 0;           // Note velocity
        uint8_t gateState = 0;          // 0 = off, 1 = on
        uint8_t queuePosition = 0;      // Position in voice priority queue
        bool isActive = false;          // Voice activity flag
        int16_t pitchWheel = 0;         // Per-voice pitch wheel
        int16_t modulation = 0;         // Per-voice modulation
    } voices[MAX_VOICES];
    
public:
    // Core voice allocation using battle-tested algorithm
    int getFreeVoice(uint8_t targetNote) {
        // Priority 1: Reuse voice playing same note (note restart)
        for (int voice = 0; voice < MAX_VOICES; voice++) {
            if (voices[voice].currentNote == targetNote && voices[voice].isActive) {
                return voice;  // Restart existing voice
            }
        }
        
        // Priority 2: Find free voice with highest queue position (oldest released)
        int bestCandidate = -1;
        for (int voice = 0; voice < MAX_VOICES; voice++) {
            if (voices[voice].gateState == 0) {  // Voice is released
                if (bestCandidate == -1 || 
                    voices[voice].queuePosition > voices[bestCandidate].queuePosition) {
                    bestCandidate = voice;
                }
            }
        }
        
        // Priority 3: Voice stealing - take voice with highest queue (oldest playing)
        if (bestCandidate == -1) {
            for (int voice = 0; voice < MAX_VOICES; voice++) {
                if (voices[voice].queuePosition == MAX_VOICES - 1) {
                    bestCandidate = voice;
                    break;  // Found the oldest voice
                }
            }
        }
        
        // Update voice queue using proven mixle_queue algorithm
        if (bestCandidate != -1) {
            mixleQueue(voices[bestCandidate].queuePosition);
        }
        
        return bestCandidate;
    }
    
private:
    // CRITICAL: Proven queue management algorithm from production synthesizer
    void mixleQueue(uint8_t queueNumber) {
        // Phase 1: Move selected voice to bottom of queue (position 0)
        // and increment all other voices' queue positions
        for (int voice = 0; voice < MAX_VOICES; voice++) {
            if (voices[voice].queuePosition == queueNumber) {
                voices[voice].queuePosition = 0;  // Move to bottom
            } else {
                voices[voice].queuePosition++;   // Move others up
            }
        }
        
        // Phase 2: Compact queue by removing gaps above the original position
        for (int voice = 0; voice < MAX_VOICES; voice++) {
            if (voices[voice].queuePosition > queueNumber) {
                voices[voice].queuePosition--;
            }
        }
    }
};
```

---

## 🎼 Audio Processing Pipeline

### Pitch Shifting Strategy

Piano-optimized resampling pomocí JUCE ResamplingAudioSource.

```cpp
class PitchShifter {
public:
    // Piano-optimized pitch shifting via resampling
    static bool pitchShiftSample(const juce::File& input, 
                                 const juce::File& output, 
                                 double semitones) {
        const double ratio = std::pow(2.0, semitones / 12.0);
        
        // Load audio with JUCE
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        
        auto reader = formatManager.createReaderFor(input);
        if (!reader) return false;
        
        // JUCE ResamplingAudioSource for high-quality resampling
        juce::ResamplingAudioSource resampler(reader.get(), false, reader->numChannels);
        resampler.setResamplingRatio(1.0 / ratio); // Inverse ratio for pitch shift
        resampler.prepareToPlay(8192, reader->sampleRate);
        
        // Process resampled audio
        juce::AudioBuffer<float> outputBuffer(reader->numChannels, 
                                           static_cast<int>(reader->lengthInSamples * ratio));
        
        juce::AudioSourceChannelInfo channelInfo;
        channelInfo.buffer = &outputBuffer;
        channelInfo.startSample = 0;
        channelInfo.numSamples = outputBuffer.getNumSamples();
        
        resampler.getNextAudioBlock(channelInfo);
        
        // Save result (note: duration changes with pitch shift - this is expected!)
        return saveBufferToFile(outputBuffer, output, reader->sampleRate, 
                               reader->bitsPerSample);
    }
};
```

### Intelligent Sample Selection

Multi-fallback systém pro optimální výběr vzorků.

```cpp
class IntelligentSampleSelector {
    VelocityMapper& velocityMapper;
    
public:
    // Enhanced sample selection with fallback strategies
    juce::File selectOptimalSample(int midiNote, int velocity) {
        // Strategy 1: Direct mapping (fastest path)
        auto directSample = velocityMapper.getSampleForVelocity(midiNote, velocity);
        if (directSample.exists()) {
            return directSample;
        }
        
        // Strategy 2: Find nearest velocity level for same note
        auto nearestVelocitySample = findNearestVelocityLevel(midiNote, velocity);
        if (nearestVelocitySample.exists()) {
            return nearestVelocitySample;
        }
        
        // Strategy 3: Find nearest note with similar velocity
        auto nearestNoteSample = findNearestNoteWithVelocity(midiNote, velocity);
        if (nearestNoteSample.exists()) {
            return nearestNoteSample;
        }
        
        // Strategy 4: Emergency fallback - any sample for the note
        return findAnySampleForNote(midiNote);
    }
    
private:
    juce::File findNearestVelocityLevel(int midiNote, int targetVelocity);
    juce::File findNearestNoteWithVelocity(int targetNote, int velocity);
    juce::File findAnySampleForNote(int midiNote);
    int dbLevelToVelocity(int dbLevel);
};
```

---

## 🛡️ Error Handling & Performance

### Production-Grade Error Handler

Comprehensive error handling s automatic recovery.

```cpp
class IthacaErrorHandler {
public:
    enum class ErrorCategory {
        FileIO, Audio, MIDI, Memory, Performance, Configuration, Cache
    };
    
    enum class ErrorSeverity {
        Info, Warning, Error, Critical
    };
    
    using ErrorCallback = std::function<void(ErrorCategory, ErrorSeverity, const juce::String&)>;
    
    static void reportError(ErrorCategory category, ErrorSeverity severity, 
                          const juce::String& message);
    
    // Specific error reporting methods
    static void reportCorruptSample(const juce::File& file);
    static void reportDiskSpaceError(const juce::File& directory);
    static void reportAudioDeviceFailure(const juce::String& deviceName);
    static void reportMidiDeviceDisconnection(const juce::String& deviceName);
    
private:
    static bool attemptRecovery(ErrorCategory category, const juce::String& context);
    static bool attemptAudioDeviceRecovery();
    static bool attemptMidiDeviceRecovery();
    static bool attemptMemoryRecovery();
};

// Convenient error reporting macro
#define ITHACA_SAFE_CALL(operation, errorCategory, errorMessage) \
    try { \
        operation; \
    } catch (const std::exception& e) { \
        IthacaErrorHandler::reportError( \
            IthacaErrorHandler::ErrorCategory::errorCategory, \
            IthacaErrorHandler::ErrorSeverity::Error, \
            juce::String(errorMessage) + ": " + juce::String(e.what())); \
        return false; \
    }
```

### Real-Time Performance Monitor

```cpp
class PerformanceMonitor {
    struct PerformanceMetrics {
        double audioThreadCpuUsage = 0.0;
        double averageAudioLatency = 0.0;
        size_t memoryUsageMB = 0;
        int droppedAudioBlocks = 0;
        int midiEventsPerSecond = 0;
        double cacheHitRatio = 0.0;
        int activeVoices = 0;
    } metrics;
    
    struct PerformanceThresholds {
        double maxCpuUsage = 80.0;          // 80% CPU usage warning
        double maxLatency = 20.0;           // 20ms latency warning
        size_t maxMemoryMB = 1024;          // 1GB memory warning
        double minCacheHitRatio = 0.90;     // 90% cache hit ratio minimum
    } thresholds;
    
public:
    void recordAudioBlock(double cpuUsage, double latency);
    void recordMidiEvent();
    void recordCacheAccess(bool hit);
    bool checkPerformanceHealth() const;
    juce::String generatePerformanceReport() const;
};
```

---

## 📋 Implementation Roadmap

### 🎯 Phase 1: Core Functionality

**Cíl**: Funkční VST3 plugin s basic sample playback

#### Implementační pořadí:
1. **Config.h** - Základní konstanty
2. **AudioFile.cpp** - File parsing s regex
3. **VelocityMapper.cpp** - Core dynamické velocity mapping
4. **CacheManager.cpp** - Windows APPDATA cache
5. **SamplerVoice.cpp** - Basic JUCE voice
6. **Simple Sampler** - Integration s JUCE Synthesiser
7. **VST3 Wrapper** - Minimal processor + editor

**Deliverable**: Plugin který načte vzorky a přehraje basic noty

### 🚀 Phase 2: Audio Quality

**Cíl**: Production-quality audio s inteligentní sample selection

#### Klíčové features:
1. **PitchShifter** - JUCE ResamplingAudioSource implementation
2. **SampleGenerator** - Intelligent missing note generation
3. **AdvancedVoiceManager** - Production mixle_queue algorithm
4. **IntelligentSampleSelector** - Multi-fallback sample selection
5. **Enhanced Error Handling** - Robust error recovery

**Deliverable**: High-quality sampler s intelligent features

### 🎛️ Phase 3: Polish & Production

**Cíl**: Production-ready plugin

#### Features:
1. **PerformanceMonitor** - Real-time performance tracking
2. **Advanced MIDI** - CC mapping, SysEx support
3. **Configuration System** - User settings persistence
4. **Enhanced GUI** - Beyond debug output
5. **Unit Tests** - Comprehensive testing suite

**Deliverable**: Production-ready VST3 plugin

---

## 🚀 Getting Started

### Prerequisites

1. **Visual Studio 2022** s C++ workload
2. **JUCE Framework** (download z juce.com)
3. **CMake** 3.22+ (volitelné, můžeme použít Projucer)

### Quick Start

1. **Clone/setup project structure**
2. **Create JUCE project** pomocí Projucer
3. **Implement Config.h** - první třída
4. **Setup basic AudioFile parsing**
5. **Build minimal VST3** s debug output

### Development workflow

1. **Implement třídu podle roadmap**
2. **Unit test** každé komponenty
3. **Integration test** s real sample files
4. **Performance validation** s PerformanceMonitor
5. **Error scenario testing**

---
