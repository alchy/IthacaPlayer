### 🎼 LFO & Modulation System (Production-Grade)

#### Advanced LFO Architecture (Inspired by Production Synthesizer)
```cpp
class LFOProcessor {
    struct LFOState {
        float stepPersistent = 0.0f;        // Continuous LFO phase
        float speedParameter = 0.0f;        // LFO speed control (0-127 from MIDI)
        float amplitudeParameter = 0.0f;    // LFO depth control
        float currentModulation = 0.0f;     // Current LFO output value
        
        // Per-voice fine tuning (inspired by production code)
        int16_t voiceTuning[16] = {0};      // Individual voice pitch offsets
        
        bool processIRQ = false;            // Interrupt flag for processing
    } lfo;
    
    // Ticker-based LFO processing (25ms intervals - production tested)
    juce::Timer lfoTimer;
    
public:
    LFOProcessor() {
        // Start LFO timer at 40Hz (25ms intervals - same as production)
        lfoTimer.startTimer(25);
    }
    
    void timerCallback() override {
        lfo.processIRQ = true;
        processLFO();
    }
    
    void processLFO() {
        if (!lfo.processIRQ) return;
        
        // Calculate LFO step increment
        float step = 0.0f;
        if (lfo.speedParameter != 0) {
            step = (0.003141592f * lfo.speedParameter) / 2.0f;
        }
        
        // Update persistent phase
        lfo.stepPersistent += step;
        if (lfo.stepPersistent > 2.0f * 3.141592f) {  // 2π wrap
            lfo.stepPersistent = 0.0f;
        }
        
        // Generate LFO output (sine wave)
        lfo.currentModulation = std::sin(lfo.stepPersistent) * 
                               (lfo.amplitudeParameter / 2.0f);
        
        lfo.processIRQ = false;
    }
    
    // Per-voice tuning system (advanced feature from production)
    void updateVoiceTuning(int voiceIndex, int16_t octaveTuning, int16_t fineTuning) {
        if (voiceIndex >= 0 && voiceIndex < 16) {
            // Convert tuning parameters to pitch offset
            // octaveTuning: ±512 = ±4 semitones, fineTuning: ±512 = ±1 semitone
            lfo.voiceTuning[voiceIndex] = ((octaveTuning - 512) * (0x80 * 12)) + 
                                         (fineTuning - 512);
        }
    }
    
    float getCurrentModulation() const { return lfo.currentModulation; }
    int16_t getVoiceTuning(int voiceIndex) const { 
        return (voiceIndex >= 0 && voiceIndex < 16) ? lfo.voiceTuning[voiceIndex] : 0; 
    }
    
    void setSpeed(float speed) { lfo.speedParameter = speed; }
    void setAmplitude(float amplitude) { lfo.amplitudeParameter = amplitude; }
};
```

### 🔍 Sample Selection & Quality Algorithm

#### Intelligent Sample Selection (Enhanced from Python Logic)
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

### 🎯 Performance Monitoring & Diagnostics

#### Real-Time Performance Monitor (Production-Ready)
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
        
        // Real-time diagnostics
        juce::Time lastUpdateTime;
        int totalAudioBlocks = 0;
        int totalMidiEvents = 0;
        int cacheHits = 0;
        int cacheMisses = 0;
    } metrics;
    
    // Performance thresholds (configurable)
    struct PerformanceThresholds {
        double maxCpuUsage = 80.0;          // 80% CPU usage warning
        double maxLatency = 20.0;           // 20ms latency warning
        size_t maxMemoryMB = 1024;          // 1GB memory warning
        double minCacheHitRatio = 0.90;     // 90% cache hit ratio minimum
    } thresholds;
    
    juce::Timer performanceTimer;
    
public:
    PerformanceMonitor() {
        // Update metrics every 1 second
        performanceTimer.startTimer(1000);
        metrics.lastUpdateTime = juce::Time::getCurrentTime();
    }
    
    void timerCallback() override {
        updateMetrics();
        checkPerformanceHealth();
    }
    
    void recordAudioBlock(double cpuUsage, double latency) {
        metrics.totalAudioBlocks++;
        metrics.audioThreadCpuUsage = cpuUsage;
        metrics.averageAudioLatency = latency;
        
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
    
    void updateMetrics() {
        auto now = juce::Time::getCurrentTime();
        double deltaTime = (now - metrics.lastUpdateTime).inSeconds();
        
        if (deltaTime > 0) {
            // Calculate events per second
            metrics.midiEventsPerSecond = static_cast<int>(metrics.totalMidiEvents / deltaTime);
            
            // Calculate cache hit ratio
            int totalCacheAccess = metrics.cacheHits + metrics.cacheMisses;
            if (totalCacheAccess > 0) {
                metrics.cacheHitRatio = static_cast<double>(metrics.cacheHits) / totalCacheAccess;
            }
            
            // Update memory usage
            metrics.memoryUsageMB = getCurrentMemoryUsage();
            
            // Reset counters
            metrics.totalMidiEvents = 0;
            metrics.cacheHits = 0;
            metrics.cacheMisses = 0;
            metrics.lastUpdateTime = now;
        }
    }
    
    bool checkPerformanceHealth() const {
        bool isHealthy = true;
        
        if (metrics.audioThreadCpuUsage > thresholds.maxCpuUsage) {
            DBG("WARNING: High CPU usage: " << metrics.audioThreadCpuUsage << "%");
            isHealthy = false;
        }
        
        if (metrics.averageAudioLatency > thresholds.maxLatency) {
            DBG("WARNING: High audio latency: " << metrics.averageAudioLatency << "ms");
            isHealthy = false;
        }
        
        if (metrics.memoryUsageMB > thresholds.maxMemoryMB) {
            DBG("WARNING: High memory usage: " << metrics.memoryUsageMB << "MB");
            isHealthy = false;
        }
        
        if (metrics.cacheHitRatio < thresholds.minCacheHitRatio) {
            DBG("WARNING: Low cache hit ratio: " << (metrics.cacheHitRatio * 100) << "%");
            isHealthy = false;
        }
        
        return isHealthy;
    }
    
    juce::String generatePerformanceReport() const {
        juce::String report;
        report << "=== IthacaPlayer Performance Report ===\n";
        report << "Audio Thread CPU: " << juce::String(metrics.audioThreadCpuUsage, 1) << "%\n";
        report << "Average Latency: " << juce::String(metrics.averageAudioLatency, 2) << "ms\n";
        report << "Memory Usage: " << metrics.memoryUsageMB << "MB\n";
        report << "Active Voices: " << metrics.activeVoices << "/" << MAX_VOICES << "\n";
        report << "MIDI Events/sec: " << metrics.midiEventsPerSecond << "\n";
        report << "Cache Hit Ratio: " << juce::String(metrics.cacheHitRatio * 100, 1) << "%\n";
        report << "Dropped Blocks: " << metrics.droppedAudioBlocks << "\n";
        report << "Health Status: " << (checkPerformanceHealth() ? "GOOD" : "WARNING") << "\n";
        
        return report;
    }
    
private:
    size_t getCurrentMemoryUsage() const {
        // Platform-specific memory usage detection
        #ifdef JUCE_WINDOWS
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                return pmc.WorkingSetSize / (1024 * 1024);  // Convert to MB
            }
        #endif
        return 0;  // Fallback if detection fails
    }
};

// Performance profiling macros for debug builds
#ifdef ITHACA_DEBUG_BUILD
#define ITHACA_PROFILE_SCOPE(name) \
    juce::ScopedValueSetter<juce::int64> profiler( \
        juce::Time::getHighResolutionTicks(), \
        [](juce::int64 start) { \
            auto elapsed = juce::Time::getHighResolutionTicks() - start; \
            DBG("PERF: " name " took " << \
                juce::Time::highResolutionTicksToSeconds(elapsed) * 1000.0 << "ms"); \
        })

#define ITHACA_LOG_PERFORMANCE(operation) \
    do { \
        auto start = juce::Time::getHighResolutionTicks(); \
        operation; \
        auto elapsed = juce::Time::getHighResolutionTicks() - start; \
        DBG("PERF: " #operation " took " << \
            juce::Time::highResolutionTicksToSeconds(elapsed) * 1000.0 << "ms"); \
    } while(0)
#else
#define ITHACA_PROFILE_SCOPE(name)
#define ITHACA_LOG_PERFORMANCE(operation) operation
#endif
```

### 🛡️ Advanced Error Recovery System

#### Production-Grade Error Handler
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
        DBG("Attempting audio device recovery...");
        
        // Try to restart audio device manager
        auto& deviceManager = juce::AudioDeviceManager::getInstance();
        
        // Get current setup
        auto currentSetup = deviceManager.getAudioDeviceSetup();
        
        // Try to reinitialize with same settings
        juce::String error = deviceManager.initialise(0, 2, nullptr, true);
        if (error.isEmpty()) {
            reportError(ErrorCategory::Audio, ErrorSeverity::Info,
                       "Audio device recovery successful");
            return true;
        }
        
        // Try with default device
        currentSetup.outputDeviceName = "";
        error = deviceManager.setAudioDeviceSetup(currentSetup, true);
        if (error.isEmpty()) {
            reportError(ErrorCategory::Audio, ErrorSeverity::Warning,
                       "Recovered using default audio device");
            return true;
        }
        
        return false;
    }
    
    static bool attemptMidiDeviceRecovery() {
        DBG("Attempting MIDI device recovery...");
        
        // Try to find alternative MIDI input device
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
        DBG("Attempting memory recovery...");
        
        // Force garbage collection and cache cleanup
        // This would trigger cache eviction, buffer pool cleanup, etc.
        
        reportError(ErrorCategory::Memory, ErrorSeverity::Info,
                   "Memory recovery attempted - cache cleaned");
        return true;
    }
    
    static bool attemptCacheRecovery() {
        DBG("Attempting cache recovery...");
        
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
IthacaErrorHandler::ErrorCallback IthacaErrorHandler::errorCallback = nullptr;
std::atomic<int> IthacaErrorHandler::errorCount{0};
std::atomic<int> IthacaErrorHandler::recoveryAttempts{0};

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
```# IthacaPlayer - MIDI Samplovací Přehrávač v JUCE

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

## 📋 Complete Implementation Roadmap

### 🎯 Phase 1: Core Functionality (Minimal Viable Prototype)

#### Essential Classes Implementation Order
```cpp
// 1. Foundation Classes (1-2 days)
namespace IthacaPlayer {
    // Config.h - Global constants and configuration
    struct Config {
        static constexpr int VELOCITY_LEVELS = 8;
        static constexpr int MIDI_VELOCITY_MAX = 127;
        static constexpr int MAX_PITCH_SHIFT = 12;
        static constexpr Range<int> MIDI_NOTE_RANGE{21, 109};
        static constexpr const char* TEMP_DIR_NAME = "samples_tmp";
        static constexpr int MAX_VOICES = 16;
    };
    
    // AudioFile.h/.cpp - Sample file representation
    class AudioFile {
        juce::File filepath;
        int midiNote;
        juce::String noteName;
        int dbLevel;
    public:
        static std::unique_ptr<AudioFile> fromFile(const juce::File& file);
        bool isValid() const;
    };
}

// 2. Core Processing Classes (2-3 days)
class VelocityMapper {
    std::map<std::pair<int, int>, juce::File> velocityMap;
    std::set<int> availableNotes;
public:
    void buildVelocityMap(const juce::File& inputDir);
    juce::File getSampleForVelocity(int note, int velocity);
};

class CacheManager {
    juce::File cacheDirectory;
public:
    void buildCacheIfNeeded(const juce::File& sourceDir);
    bool isCacheValid(const juce::File& sourceDir);
};

// 3. Audio Engine Classes (3-4 days)
class SamplerVoice : public juce::SynthesiserVoice {
    VelocityMapper& velocityMapper;
    std::unique_ptr<juce::AudioFormatReader> currentReader;
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, 
                   juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
};

class Sampler : public juce::Synthesiser {
    AdvancedVoiceManager voiceManager;
public:
    Sampler(const juce::File& sampleDir);
    int getFreeVoice(uint8_t note);  // Using production mixle_queue algorithm
};
```

#### VST3 Plugin Wrapper (1 day)
```cpp
class IthacaPlayerProcessor : public juce::AudioProcessor {
    std::unique_ptr<Sampler> sampler;
    std::unique_ptr<PerformanceMonitor> performanceMonitor;
    
public:
    void processBlock(juce::AudioBuffer<float>& buffer, 
                     juce::MidiBuffer& midiMessages) override {
        ITHACA_PROFILE_SCOPE("processBlock");
        
        // Process MIDI events
        for (auto metadata : midiMessages) {
            auto message = metadata.getMessage();
            sampler->handleMidiMessage(message);
        }
        
        // Render audio
        sampler->renderNextBlock(buffer, 0, buffer.getNumSamples());
        
        // Monitor performance
        performanceMonitor->recordAudioBlock(getCurrentCPUUsage(), 
                                           getCurrentLatency());
    }
};

class IthacaPlayerEditor : public juce::AudioProcessorEditor {
    juce::TextEditor debugOutput;
    juce::TextButton clearCacheButton;
    
public:
    IthacaPlayerEditor(IthacaPlayerProcessor& processor);
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Simple debug GUI for prototype
    void appendDebugMessage(const juce::String& message);
};
```

**Phase 1 Goal**: VST3 plugin that loads samples, maps velocity, and plays basic notes

---

### 🚀 Phase 2: Audio Quality & Advanced Features (2-3 weeks)

#### High-Quality Sample Generation
```cpp
class SampleGenerator {
    VelocityMapper& velocityMapper;
    LFOProcessor lfoProcessor;
    
public:
    void generateMissingNotes(const juce::File& tempDir);
    bool pitchShiftSample(const juce::File& input, const juce::File& output, 
                         double semitones);
    
private:
    // Use JUCE ResamplingAudioSource for pitch shifting
    bool resampleWithJUCE(const juce::File& input, const juce::File& output, 
                          double ratio);
};
```

#### Advanced Voice Management Integration
```cpp
// Integrate production-tested algorithms
class EnhancedSampler : public Sampler {
    AdvancedVoiceManager voiceManager;
    IntelligentSampleSelector sampleSelector;
    
public:
    void noteOn(int midiChannel, int midiNoteNumber, float velocity) override {
        // Use proven getFreeVoice algorithm
        int voice = voiceManager.getFreeVoice(midiNoteNumber);
        
        // Intelligent sample selection with fallbacks
        auto sample = sampleSelector.selectOptimalSample(midiNoteNumber, 
                                                         velocity * 127);
        
        if (voice != -1 && sample.exists()) {
            // Start voice with selected sample
            getVoice(voice)->startNote(midiNoteNumber, velocity, 
                                     getSound(0).get(), -1);
        }
    }
};
```

**Phase 2 Goal**: Production-quality audio with intelligent sample selection and robust voice management

---

### 🎛️ Phase 3: Polish & Production Features (1-2 weeks)

#### Complete Error Handling & Recovery
```cpp
// Integrate comprehensive error handling
class ProductionIthacaPlayer {
    IthacaErrorHandler errorHandler;
    PerformanceMonitor performanceMonitor;
    
public:
    void initialize(const juce::File& sampleDirectory) {
        ITHACA_SAFE_CALL(
            buildSampleCache(sampleDirectory),
            FileIO,
            "Failed to build sample cache"
        );
    }
    
    void handleCriticalError(const juce::String& error) {
        errorHandler.reportError(IthacaErrorHandler::ErrorCategory::Critical,
                                IthacaErrorHandler::ErrorSeverity::Critical,
                                error);
        // Graceful shutdown procedures
    }
};
```

#### Configuration System
```cpp
class IthacaSettings {
    struct Settings {
        juce::File sampleDirectory;
        bool cleanTempOnStartup = false;
        int maxVoices = 16;
        double pitchWheelRange = 2.0;
        bool enableAdvancedLogging = false;
        
        // Performance settings
        int audioBufferSize = 256;
        double sampleRate = 44100.0;
        size_t maxMemoryUsageMB = 1024;
    } settings;
    
public:
    bool loadFromFile();
    bool saveToFile();
    void resetToDefaults();
    bool validateSettings() const;
};
```

**Phase 3 Goal**: Production-ready plugin with comprehensive error handling, performance monitoring, and user configuration

---

## 🚀 Implementation Strategy Summary

### **Development Priorities**:
1. ✅ **Core Audio Pipeline** - Sample loading → Velocity mapping → Basic playback
2. ✅ **Production Voice Management** - Implement proven `mixle_queue` algorithm
3. ✅ **Robust Error Handling** - Handle all failure scenarios gracefully
4. ✅ **Performance Optimization** - Real-time monitoring and optimization
5. ✅ **User Experience** - Configuration, debugging, and ease of use

### **Key Success Factors**:
- **Preserve Production Algorithms**: Keep proven voice stealing and MIDI parsing logic
- **JUCE Best Practices**: Leverage JUCE strengths while maintaining performance
- **Incremental Development**: Each phase produces working, testable code
- **Performance First**: Monitor and optimize from day one
- **Error Resilience**: Handle real-world failure scenarios

### **Testing Strategy**:
```cpp
// Unit tests for critical components
class VoiceManagementTest : public juce::UnitTest {
public:
    void runTest() override {
        beginTest("Voice stealing algorithm");
        
        AdvancedVoiceManager manager;
        
        // Test voice allocation patterns
        for (int i = 0; i < 20; i++) {  // More notes than voices
            int voice = manager.getFreeVoice(60 + (i % 12));
            expect(voice >= 0 && voice < MAX_VOICES, 
                  "Valid voice returned");
        }
        
        // Verify queue integrity
        expect(manager.verifyQueueIntegrity(), "Queue state valid");
    }
};
```

---

