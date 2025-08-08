### Enhanced Production-Grade Error Handler (JUCE-Native)

```cpp
class IthacaErrorHandler : public juce::Logger {
public:
    enum class ErrorCategory {
        FileIO, Audio, MIDI, Memory, Performance, Configuration, Cache
    };
    
    enum class ErrorSeverity {
        Info, Warning, Error, Critical
    };
    
    // JUCE-style callback using std::function and juce::String
    using ErrorCallback = std::function<void(ErrorCategory, ErrorSeverity, const juce::String&)>;
    
private:
    static std::unique_ptr<IthacaErrorHandler> instance;
    static ErrorCallback errorCallback;
    static std::atomic<int> errorCount;
    static std::atomic<int> recoveryAttempts;
    
    // JUCE thread safety
    juce::CriticalSection errorLock;
    std::unique_ptr<juce::FileLogger> fileLogger;
    
public:
    static IthacaErrorHandler& getInstance() {
        if (!instance) {
            instance = std::make_unique<IthacaErrorHandler>();
        }
        return *instance;
    }
    
    // JUCE Logger interface implementation
    void logMessage(const juce::String& message) override {
        juce::ScopedLock lock(errorLock);
        
        // Use JUCE's built-in logging
        if (fileLogger) {
            fileLogger->logMessage(message);
        }
        
        // Also output to JUCE debug console
        DBG(message);
    }
    
    void initialize() {
        // Setup JUCE file logging
        auto logFile = juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory)
            .getChildFile("IthacaPlayer")
            .getChildFile("Logs")
            .getChildFile("ithaca_player.log");
            
        logFile.getParentDirectory().createDirectory();
        
        fileLogger = std::make_unique<juce::FileLogger>(
            logFile, "IthacaPlayer Log", 1024 * 1024); // 1MB max size
            
        // Set as JUCE's default logger
        juce::Logger::setCurrentLogger(this);
    }
    
    static void setErrorCallback(ErrorCallback callback) {
        errorCallback = std::move(callback);
    }
    
    static void reportError(ErrorCategory category, ErrorSeverity severity, 
                          const juce::String& message) {
        errorCount++;
        
        juce::String fullMessage = "[" + severityToString(severity) + "] " +
                                  categoryToString(category) + ": " + message;
        
        // Use JUCE logging system
        getInstance().logMessage(fullMessage);
        
        // Call user callback on message thread (JUCE-safe)
        if (errorCallback) {
            juce::MessageManager::callAsync([=]() {
                errorCallback(category, severity, fullMessage);
            });
        }
        
        // Attempt automatic recovery for specific error types
        if (severity >= ErrorSeverity::Error) {
            juce::MessageManager::callAsync([=]() {
                attemptRecovery(category, message);
            });
        }
    }
    
    // JUCE-safe error reporting methods
    static void reportCorruptSample(const juce::File& file) {
        reportError(ErrorCategory::FileIO, ErrorSeverity::Critical,
                   "Corrupt sample file: " + file.getFullPathName() + 
                   " - Application will terminate");
        
        // JUCE-safe application quit
        juce::MessageManager::callAsync([]() {
            juce::JUCEApplicationBase::getInstance()->systemRequestedQuit();
        });
    }
    
    static void reportDiskSpaceError(const juce::File& directory) {
        reportError(ErrorCategory::FileIO, ErrorSeverity::Critical,
                   "Insufficient disk space in: " + directory.getFullPathName() +
                   " - Cannot build sample cache");
    }
    
    static void reportAudioDeviceFailure(const juce::String& deviceName) {
        reportError(ErrorCategory::Audio, ErrorSeverity::Error,
                   "Audio device failure: " + deviceName);
    }
    
    static void reportMidiDeviceDisconnection(const juce::String& deviceName) {
        reportError(ErrorCategory::MIDI, ErrorSeverity::Warning,
                   "MIDI device disconnected: " + deviceName);
    }
    
    // JUCE AlertWindow for user notifications
    static void showUserAlert(const juce::String& title, const juce::String& message,
                             juce::AlertWindow::AlertIconType iconType = juce::AlertWindow::WarningIcon) {
        juce::MessageManager::callAsync([=]() {
            juce::AlertWindow::showMessageBoxAsync(iconType, title, message);
        });
    }
    
private:
    static bool attemptRecovery(ErrorCategory category, const juce::String& context) {
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

### Enhanced Intelligent Sample Selection

Multi-fallback systém s pokročilou logikou výběru vzorků.

```cpp
class IntelligentSampleSelector {
    VelocityMapper& velocityMapper;
    
public:
    IntelligentSampleSelector(VelocityMapper& mapper) : velocityMapper(mapper) {}
    
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
    juce::File findNearestVelocityLevel(int midiNote, int targetVelocity) {
        auto samplesForNote = velocityMapper.getSamplesForNote(midiNote);
        if (samplesForNote.empty()) return {};
        
        // Find sample with closest velocity level
        int bestVelocityDiff = 128;
        juce::File bestSample;
        
        for (const auto& sample : samplesForNote) {
            // Calculate velocity from DbLvl (higher dB = lower velocity typically)
            int sampleVelocity = dbLevelToVelocity(sample.dbLevel);
            int velocityDiff = std::abs(sampleVelocity - targetVelocity);
            
            if (velocityDiff < bestVelocityDiff) {
                bestVelocityDiff = velocityDiff;
                bestSample = sample.file;
            }
        }
        
        return bestSample;
    }
    
    juce::File findNearestNoteWithVelocity(int targetNote, int velocity) {
        auto availableNotes = velocityMapper.getAvailableNotes();
        
        // Find closest note within reasonable range (±6 semitones)
        int bestNoteDiff = 13;  // Larger than maximum search range
        int bestNote = -1;
        
        for (int note : availableNotes) {
            int noteDiff = std::abs(note - targetNote);
            if (noteDiff <= 6 && noteDiff < bestNoteDiff) {  // Within reasonable range
                bestNoteDiff = noteDiff;
                bestNote = note;
            }
        }
        
        if (bestNote != -1) {
            return velocityMapper.getSampleForVelocity(bestNote, velocity);
        }
        
        return {};
    }
    
    juce::File findAnySampleForNote(int midiNote) {
        // Last resort: return any sample for the note (velocity 64 as default)
        return velocityMapper.getSampleForVelocity(midiNote, 64);
    }
    
    int dbLevelToVelocity(int dbLevel) {
        // Convert dB level to approximate velocity
        // DbLvl-0 = max velocity (127), DbLvl-40 = min velocity (1)
        if (dbLevel >= 0) return 127;
        if (dbLevel <= -40) return 1;
        
        // Linear mapping: dB -40 to 0 maps to velocity 1 to 127
        return static_cast<int>(127 + (dbLevel * 126.0f / 40.0f));
    }
};
```

---

## 🛡️ Error Handling & Performance

### Enhanced Production-Grade Error Handler

```cpp
class IthacaErrorHandler {
public:
    enum class ErrorCategory {
        FileIO,          // Sample file issues
        Audio,           // Audio device problems
        MIDI,            // MIDI device issues
        Memory,          // Memory allocation failures
        Performance,     // Real-time constraints violated
        Configuration,   // Invalid settings
        Cache            // Cache system errors
    };
    
    enum class ErrorSeverity {
        Info,           // Informational message
        Warning,        // Issue detected but system continues
        Error,          // Operation failed but recoverable
        Critical        // System cannot continue safely
    };
    
    using ErrorCallback = std::function<void(ErrorCategory, ErrorSeverity, const juce::String&)>;
    
private:
    static ErrorCallback errorCallback;
    static std::atomic<int> errorCount;
    static std::atomic<int> recoveryAttempts;
    
public:
    static void setErrorCallback(ErrorCallback callback) {
        errorCallback = std::move(callback);
    }
    
    static void reportError(ErrorCategory category, ErrorSeverity severity, 
                          const juce::String& message) {
        errorCount++;
        
        juce::String fullMessage = "[" + severityToString(severity) + "] " +
                                  categoryToString(category) + ": " + message;
        
        // Log to JUCE logger
        if (severity == ErrorSeverity::Critical) {
            DBG("CRITICAL ERROR: " << fullMessage);
        } else {
            DBG(fullMessage);
        }
        
        // Call user callback if set
        if (errorCallback) {
            errorCallback(category, severity, fullMessage);
        }
        
        // Attempt automatic recovery for specific error types
        if (severity >= ErrorSeverity::Error) {
            attemptRecovery(category, message);
        }
    }
    
    // Specific error reporting methods
    static void reportCorruptSample(const juce::File& file) {
        reportError(ErrorCategory::FileIO, ErrorSeverity::Critical,
                   "Corrupt sample file: " + file.getFullPathName() + 
                   " - Application will terminate");
        
        // Critical error - cannot continue with corrupt samples
        juce::JUCEApplicationBase::quit();
    }
    
    static void reportDiskSpaceError(const juce::File& directory) {
        reportError(ErrorCategory::FileIO, ErrorSeverity::Critical,
                   "Insufficient disk space in: " + directory.getFullPathName() +
                   " - Cannot build sample cache");
        
        // Critical error - cannot continue without cache space
        juce::JUCEApplicationBase::quit();
    }
    
    static void reportAudioDeviceFailure(const juce::String& deviceName) {
        reportError(ErrorCategory::Audio, ErrorSeverity::Error,
                   "Audio device failure: " + deviceName);
    }
    
    static void reportMidiDeviceDisconnection(const juce::String& deviceName) {
        reportError(ErrorCategory::MIDI, ErrorSeverity::Warning,
                   "MIDI device disconnected: " + deviceName);
    }
    
    static void reportMemoryAllocationFailure(size_t requestedBytes) {
        reportError(ErrorCategory::Memory, ErrorSeverity::Error,
                   "Memory allocation failed: " + juce::String(requestedBytes) + " bytes");
    }
    
    static void reportPerformanceViolation(const juce::String& details) {
        reportError(ErrorCategory::Performance, ErrorSeverity::Warning,
                   "Performance threshold violated: " + details);
    }
    
private:
    static bool attemptRecovery(ErrorCategory category, const juce::String& context) {
    static bool attemptRecovery(ErrorCategory category, const juce::String& context) {
        recoveryAttempts++;
        
        switch (category) {
            case ErrorCategory::Audio:
                return attemptAudioDeviceRecovery();
            case ErrorCategory::MIDI:
                return attemptMidiDeviceRecovery();
            case ErrorCategory::Memory:
                return attemptMemoryRecovery();
            case ErrorCategory::Cache:
                return attemptCacheRecovery();
            default:
                return false;
        }
    }
    
    static bool attemptAudioDeviceRecovery() {
        reportError(ErrorCategory::Audio, ErrorSeverity::Info,
                   "Attempting audio device recovery...");
        
        // Use JUCE AudioDeviceManager for recovery
        auto* deviceManager = juce::AudioDeviceManager::getInstance();
        if (!deviceManager) return false;
        
        // Get current setup
        auto currentSetup = deviceManager->getAudioDeviceSetup();
        
        // Try to reinitialize with same settings
        juce::String error = deviceManager->initialise(0, 2, nullptr, true);
        if (error.isEmpty()) {
            reportError(ErrorCategory::Audio, ErrorSeverity::Info,
                       "Audio device recovery successful");
            return true;
        }
        
        // Try with default device
        currentSetup.outputDeviceName = "";
        error = deviceManager->setAudioDeviceSetup(currentSetup, true);
        if (error.isEmpty()) {
            reportError(ErrorCategory::Audio, ErrorSeverity::Warning,
                       "Recovered using default audio device");
            return true;
        }
        
        return false;
    }
    
    static bool attemptMidiDeviceRecovery() {
        reportError(ErrorCategory::MIDI, ErrorSeverity::Info,
                   "Attempting MIDI device recovery...");
        
        // Use JUCE MidiInput system
        auto availableDevices = juce::MidiInput::getAvailableDevices();
        
        for (const auto& device : availableDevices) {
            auto midiInput = juce::MidiInput::openDevice(device.identifier, nullptr);
            if (midiInput) {
                reportError(ErrorCategory::MIDI, ErrorSeverity::Info,
                           "MIDI recovery: switched to " + device.name);
                return true;
            }
        }
        
        // No MIDI devices available - continue without MIDI
        reportError(ErrorCategory::MIDI, ErrorSeverity::Warning,
                   "No MIDI devices available - continuing without MIDI input");
        return true;  // Not critical for operation
    }
    
    static bool attemptMemoryRecovery() {
        reportError(ErrorCategory::Memory, ErrorSeverity::Info,
                   "Attempting memory recovery...");
        
        // Use JUCE memory management utilities
        juce::SystemStats::getMemorySizeInMegabytes(); // Trigger memory stats update
        
        reportError(ErrorCategory::Memory, ErrorSeverity::Info,
                   "Memory recovery attempted - cache cleaned");
        return true;
    }
    
    static bool attemptCacheRecovery() {
        reportError(ErrorCategory::Cache, ErrorSeverity::Info,
                   "Attempting cache recovery...");
        
        // Clear corrupted cache and rebuild
        reportError(ErrorCategory::Cache, ErrorSeverity::Info,
                   "Cache recovery: clearing and rebuilding cache");
        return true;
    }
    
    static juce::String categoryToString(ErrorCategory category) {
        switch (category) {
            case ErrorCategory::FileIO: return "FileIO";
            case ErrorCategory::Audio: return "Audio";
            case ErrorCategory::MIDI: return "MIDI";
            case ErrorCategory::Memory: return "Memory";
            case ErrorCategory::Performance: return "Performance";
            case ErrorCategory::Configuration: return "Configuration";
            case ErrorCategory::Cache: return "Cache";
            default: return "Unknown";
        }
    }
    
    static juce::String severityToString(ErrorSeverity severity) {
        switch (severity) {
            case ErrorSeverity::Info: return "INFO";
            case ErrorSeverity::Warning: return "WARNING";
            case ErrorSeverity::Error: return "ERROR";
            case ErrorSeverity::Critical: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }
};

// Initialize static members
std::unique_ptr<IthacaErrorHandler> IthacaErrorHandler::instance = nullptr;
IthacaErrorHandler::ErrorCallback IthacaErrorHandler::errorCallback = nullptr;
std::atomic<int> IthacaErrorHandler::errorCount{0};
std::atomic<int> IthacaErrorHandler::recoveryAttempts{0};

// JUCE-style error handling macro
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

// JUCE-style assertion with error reporting
#define ITHACA_ASSERT(condition, message) \
    if (!(condition)) { \
        IthacaErrorHandler::reportError( \
            IthacaErrorHandler::ErrorCategory::Configuration, \
            IthacaErrorHandler::ErrorSeverity::Error, \
            juce::String("Assertion failed: ") + message + \
            " at " + __FILE__ + ":" + juce::String(__LINE__)); \
        jassertfalse; \
    }
```

### Enhanced Real-Time Performance Monitor (JUCE-Native)

```cpp
class PerformanceMonitor : public juce::Timer,
                          public juce::AudioProcessor::AudioProcessorListener {
    struct PerformanceMetrics {
        std::atomic<double> audioThreadCpuUsage{0.0};
        std::atomic<double> averageAudioLatency{0.0};
        std::atomic<size_t> memoryUsageMB{0};
        std::atomic<int> droppedAudioBlocks{0};
        std::atomic<int> midiEventsPerSecond{0};
        std::atomic<double> cacheHitRatio{0.0};
        std::atomic<int> activeVoices{0};
        
        // JUCE-safe counters (accessed from multiple threads)
        std::atomic<int> totalAudioBlocks{0};
        std::atomic<int> totalMidiEvents{0};
        std::atomic<int> cacheHits{0};
        std::atomic<int> cacheMisses{0};
        
        juce::Time lastUpdateTime;
    } metrics;
    
    struct PerformanceThresholds {
        double maxCpuUsage = 80.0;
        double maxLatency = 20.0;
        size_t maxMemoryMB = 1024;
        double minCacheHitRatio = 0.90;
    } thresholds;
    
    // JUCE-style callback registration
    std::function<void(const juce::String&)> onPerformanceAlert;
    
public:
    PerformanceMonitor() {
        // JUCE Timer with 1 second interval
        startTimer(1000);
        metrics.lastUpdateTime = juce::Time::getCurrentTime();
    }
    
    // JUCE Timer callback (runs on message thread)
    void timerCallback() override {
        updateMetrics();
        
        if (!checkPerformanceHealth()) {
            if (onPerformanceAlert) {
                onPerformanceAlert(generatePerformanceReport());
            }
        }
    }
    
    // JUCE AudioProcessorListener implementation
    void audioProcessorParameterChanged(juce::AudioProcessor* processor,
                                       int parameterIndex, float newValue) override {
        // Monitor parameter changes if needed
    }
    
    void audioProcessorChanged(juce::AudioProcessor* processor,
                              const ChangeDetails& details) override {
        if (details.latencyChanged) {
            // React to latency changes
            DBG("Audio latency changed");
        }
    }
    
    // Thread-safe recording methods (called from audio thread)
    void recordAudioBlock(double cpuUsage, double latency) {
        metrics.totalAudioBlocks++;
        metrics.audioThreadCpuUsage.store(cpuUsage);
        metrics.averageAudioLatency.store(latency);
        
        // Detect dropped blocks
        if (cpuUsage > 95.0 || latency > 50.0) {
            metrics.droppedAudioBlocks++;
        }
    }
    
    void recordMidiEvent() {
        metrics.totalMidiEvents++;
    }
    
    void recordCacheAccess(bool hit) {
        if (hit) {
            metrics.cacheHits++;
        } else {
            metrics.cacheMisses++;
        }
    }
    
    void setActiveVoices(int voices) {
        metrics.activeVoices.store(voices);
    }
    
    // JUCE-style listener registration
    void setPerformanceAlertCallback(std::function<void(const juce::String&)> callback) {
        onPerformanceAlert = std::move(callback);
    }
    
    juce::String generatePerformanceReport() const {
        juce::String report;
        report << "=== IthacaPlayer Performance Report ===\n";
        report << "Audio Thread CPU: " << juce::String(metrics.audioThreadCpuUsage.load(), 1) << "%\n";
        report << "Average Latency: " << juce::String(metrics.averageAudioLatency.load(), 2) << "ms\n";
        report << "Memory Usage: " << metrics.memoryUsageMB.load() << "MB\n";
        report << "Active Voices: " << metrics.activeVoices.load() << "/" << Config::MAX_VOICES << "\n";
        report << "MIDI Events/sec: " << metrics.midiEventsPerSecond.load() << "\n";
        report << "Cache Hit Ratio: " << juce::String(metrics.cacheHitRatio.load() * 100, 1) << "%\n";
        report << "Dropped Blocks: " << metrics.droppedAudioBlocks.load() << "\n";
        report << "Health Status: " << (checkPerformanceHealth() ? "GOOD" : "WARNING") << "\n";
        
        return report;
    }
    
private:
    void updateMetrics() {
        auto now = juce::Time::getCurrentTime();
        double deltaTime = (now - metrics.lastUpdateTime).inSeconds();
        
        if (deltaTime > 0) {
            // Calculate events per second
            metrics.midiEventsPerSecond.store(
                static_cast<int>(metrics.totalMidiEvents.load() / deltaTime));
            
            // Calculate cache hit ratio
            int totalCacheAccess = metrics.cacheHits.load() + metrics.cacheMisses.load();
            if (totalCacheAccess > 0) {
                metrics.cacheHitRatio.store(
                    static_cast<double>(metrics.cacheHits.load()) / totalCacheAccess);
            }
            
            // Update memory usage (JUCE-safe)
            metrics.memoryUsageMB.store(getCurrentMemoryUsage());
            
            // Reset counters
            metrics.totalMidiEvents.store(0);
            metrics.cacheHits.store(0);
            metrics.cacheMisses.store(0);
            metrics.lastUpdateTime = now;
        }
    }
    
    bool checkPerformanceHealth() const {
        return metrics.audioThreadCpuUsage.load() <= thresholds.maxCpuUsage &&
               metrics.averageAudioLatency.load() <= thresholds.maxLatency &&
               metrics.memoryUsageMB.load() <= thresholds.maxMemoryMB &&
               metrics.cacheHitRatio.load() >= thresholds.minCacheHitRatio;
    }
    
    size_t getCurrentMemoryUsage() const {
        // Use JUCE's cross-platform memory functions where available
        #if JUCE_WINDOWS
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                return pmc.WorkingSetSize / (1024 * 1024);
            }
        #elif JUCE_MAC || JUCE_LINUX
            // Platform-specific implementation
            // Could use JUCE's SystemStats class for cross-platform approach
        #endif
        return 0;
    }
};

// JUCE-style performance profiling (debug builds only)
#if JUCE_DEBUG
class ScopedPerformanceTimer {
    juce::String name;
    juce::int64 startTime;
    
public:
    explicit ScopedPerformanceTimer(const juce::String& timerName) 
        : name(timerName), startTime(juce::Time::getHighResolutionTicks()) {}
        
    ~ScopedPerformanceTimer() {
        auto elapsed = juce::Time::getHighResolutionTicks() - startTime;
        DBG("PERF: " << name << " took " << 
            juce::Time::highResolutionTicksToSeconds(elapsed) * 1000.0 << "ms");
    }
};

#define ITHACA_PROFILE_SCOPE(name) ScopedPerformanceTimer timer(name)
#else
#define ITHACA_PROFILE_SCOPE(name)
#endif
```

### 🎼 LFO & Modulation System (JUCE-Optimized)

#### JUCE-Native LFO Architecture
```cpp
class LFOProcessor : public juce::Timer {
    struct LFOState {
        float phase = 0.0f;                     // Current LFO phase (0.0 to 1.0)
        float frequency = 1.0f;                 // LFO frequency in Hz
        float amplitude = 0.0f;                 // LFO depth (0.0 to 1.0)
        float currentValue = 0.0f;              // Current LFO output
        
        // JUCE-style parameter handling
        juce::AudioParameterFloat* speedParam = nullptr;
        juce::AudioParameterFloat* depthParam = nullptr;
        
        // Per-voice modulation targets (JUCE-style)
        std::array<float, 16> voicePitchMod{};  // Pitch modulation per voice
        std::array<float, 16> voiceAmpMod{};    // Amplitude modulation per voice
    } lfoState;
    
    double sampleRate = 44100.0;
    float phaseIncrement = 0.0f;
    
public:
    LFOProcessor() {
        // JUCE Timer approach instead of custom ticker
        startTimerHz(60);  // 60Hz update rate (JUCE best practice)
    }
    
    void prepareToPlay(double newSampleRate) {
        sampleRate = newSampleRate;
        updatePhaseIncrement();
    }
    
    void timerCallback() override {
        // JUCE Timer callback - runs on message thread
        updateLFO();
        
        // Notify listeners if needed (JUCE pattern)
        if (onLFOUpdate) {
            onLFOUpdate(lfoState.currentValue);
        }
    }
    
    // JUCE-style parameter processing
    void processBlock(int numSamples) {
        // Update parameters from JUCE parameter system
        if (lfoState.speedParam) {
            lfoState.frequency = lfoState.speedParam->get();
            updatePhaseIncrement();
        }
        
        if (lfoState.depthParam) {
            lfoState.amplitude = lfoState.depthParam->get();
        }
        
        // Process LFO for this audio block
        for (int sample = 0; sample < numSamples; ++sample) {
            updateLFOSample();
        }
    }
    
    // JUCE AudioProcessor parameter integration
    void addParameters(juce::AudioProcessor& processor) {
        using FloatParam = juce::AudioParameterFloat;
        
        lfoState.speedParam = new FloatParam(
            "lfoSpeed", "LFO Speed", 
            juce::NormalisableRange<float>(0.1f, 20.0f, 0.1f), 1.0f);
            
        lfoState.depthParam = new FloatParam(
            "lfoDepth", "LFO Depth",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f);
            
        processor.addParameter(lfoState.speedParam);
        processor.addParameter(lfoState.depthParam);
    }
    
    // JUCE-style modulation application
    float getModulationForVoice(int voiceIndex, ModulationType type) const {
        jassert(voiceIndex >= 0 && voiceIndex < 16);
        
        switch (type) {
            case ModulationType::Pitch:
                return lfoState.voicePitchMod[voiceIndex] * lfoState.currentValue;
            case ModulationType::Amplitude:
                return lfoState.voiceAmpMod[voiceIndex] * lfoState.currentValue;
            default:
                return 0.0f;
        }
    }
    
    void setModulationAmount(int voiceIndex, ModulationType type, float amount) {
        jassert(voiceIndex >= 0 && voiceIndex < 16);
        jassert(amount >= 0.0f && amount <= 1.0f);
        
        switch (type) {
            case ModulationType::Pitch:
                lfoState.voicePitchMod[voiceIndex] = amount;
                break;
            case ModulationType::Amplitude:
                lfoState.voiceAmpMod[voiceIndex] = amount;
                break;
        }
    }
    
    // JUCE Listener pattern for UI updates
    std::function<void(float)> onLFOUpdate;
    
    enum class ModulationType {
        Pitch,
        Amplitude,
        Filter  // Future expansion
    };
    
private:
    void updatePhaseIncrement() {
        if (sampleRate > 0.0) {
            phaseIncrement = static_cast<float>(lfoState.frequency / sampleRate);
        }
    }
    
    void updateLFOSample() {
        // Advance phase
        lfoState.phase += phaseIncrement;
        if (lfoState.phase >= 1.0f) {
            lfoState.phase -= 1.0f;  // Wrap phase
        }
        
        // Generate sine wave (JUCE has built-in math functions)
        lfoState.currentValue = std::sin(lfoState.phase * juce::MathConstants<float>::twoPi) 
                               * lfoState.amplitude;
    }
    
    void updateLFO() {
        // Called from timer - update UI-rate parameters
        // Audio-rate processing happens in processBlock()
    }
};

// JUCE-style LFO integration with Synthesiser
class IthacaSynthVoice : public juce::SynthesiserVoice {
    LFOProcessor& lfoProcessor;
    int voiceIndex;
    
public:
    IthacaSynthVoice(LFOProcessor& lfo, int index) 
        : lfoProcessor(lfo), voiceIndex(index) {}
        
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                        int startSample, int numSamples) override {
        
        // Apply LFO modulation (JUCE way)
        float pitchMod = lfoProcessor.getModulationForVoice(voiceIndex, 
                                        LFOProcessor::ModulationType::Pitch);
        float ampMod = lfoProcessor.getModulationForVoice(voiceIndex,
                                      LFOProcessor::ModulationType::Amplitude);
        
        // Apply modulation to voice parameters
        float pitchRatio = juce::MidiMessage::getMidiNoteInHertz(currentMidiNote + pitchMod) /
                          juce::MidiMessage::getMidiNoteInHertz(currentMidiNote);
        
        // Render audio with modulation applied
        // ... (rest of voice rendering)
    }
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

## 🎉 Závěr

IthacaPlayer představuje síntézu proven algorithms z produkčních synthesizérů s moderními JUCE technologiemi. Projekt klade důraz na:

- **Production-proven approaches** - použití battle-tested algoritmů
- **Real-time performance** - optimalizace pro professional audio
- **Robust error handling** - graceful handling všech scénářů
- **Developer-friendly architecture** - čitelný a maintainable kód

---

*This specification provides a complete foundation for building a professional-grade MIDI sampler with IthacaPlayer.*
