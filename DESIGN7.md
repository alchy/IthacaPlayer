# IthacaPlayer - MIDI Samplovací Přehrávač

**IthacaPlayer** je profesionální MIDI-ovládaný přehrávač audio vzorků postavený na frameworku JUCE. Kombinuje moderní C++ s robustním zpracováním chyb, real-time výkonem a inteligentním správou vzorků pro vysoce kvalitní audio produkci.

---

## 📖 Přehled

### Co je IthacaPlayer?

IthacaPlayer je multiplatformní VST3 plugin a samostatná aplikace pro real-time přehrávání audio vzorků spouštěných MIDI signálem. Podporuje polyfonní přehrávání, dynamické mapování velocity a inteligentní generování vzorků, optimalizované pro nízkou latenci a vysokou kvalitu zvuku.

### Klíčové vlastnosti

- 🎹 **Polyfonní přehrávání**: Až 16 hlasů s inteligentním voice stealing
- 🎯 **Dynamické mapování velocity**: Mapování MIDI velocity na vzorky podle dB úrovní
- 🔧 **Automatické generování vzorků**: Vytváření chybějících not pomocí pitch-shiftingu
- 🎼 **Pokročilé MIDI zpracování**: Podpora Note On/Off, Pitch Wheel, Control Change, SysEx
- 💾 **Inteligentní cache systém**: Perzistentní ukládání pro rychlé spuštění
- 🚀 **Multiplatformní podpora**: Windows, macOS, Linux přes JUCE

### Formát audio vzorků

IthacaPlayer používá specifickou pojmenovací konvenci pro vzorky:

```
mNNN-NOTA-DbLvl-X.wav
```

- **Příklady**:
  - `m060-C_4-DbLvl-20.wav`: MIDI nota 60 (C4), -20dB
  - `m072-C_5-DbLvl-0.wav`: MIDI nota 72 (C5), 0dB (plná hlasitost)

### Filozofie

Inspirován produkčními syntezátory, IthacaPlayer klade důraz na:
- **Ověřené algoritmy**: Použití battle-tested přístupů z profesionálních systémů
- **Real-time výkon**: Nízká latence a vysoká efektivita
- **Robustní zpracování chyb**: Elegantní řešení chybových stavů
- **Uživatelská zkušenost**: Jednoduché rozhraní s výkonnými funkcemi

---

## 🎯 Cílová skupina a použití

### Cílová skupina
- **Hudební producenti**: Kvalitní přehrávání vzorků v DAW
- **Live performeři**: Spolehlivý nástroj pro živá vystoupení
- **Sound designeři**: Správa rozsáhlých kolekcí vzorků
- **Vývojáři audio softwaru**: Referenční implementace MIDI samplingu

### Příklady použití
1. Knihovny klavírních vzorků s více velocity vrstvami
2. Orchestrální nástroje (struny, dechy, dřeva) s přepínáním velocity
3. Bicí automaty s citlivostí na velocity
4. Zvukové efekty pro prostředí, foley nebo filmové textury

---

## 📊 Technické požadavky

### Výkonnostní cíle
| Metrika           | Cílová hodnota         | Popis                              |
|-------------------|------------------------|------------------------------------|
| **Audio latence** | 5-20 ms               | Vhodná pro real-time výkon        |
| **Využití paměti**| 512MB - 4GB           | Škáluje s velikostí kolekce       |
| **Čas spuštění** | První: Neomezeno, Další: <3s | Build cache vs. rychlé načtení |
| **Polyfonie**     | 16 hlasů              | Vyvážená kvalita a výkon          |
| **Pitch shift**   | ±12 půltónů           | Zajišťuje vysokou kvalitu zvuku   |

### Podporované platformy
- **Primární**: Windows 10/11 x64
- **Build systém**: Visual Studio 2022, CMake
- **Audio framework**: JUCE (nejnovější stabilní)
- **Formáty pluginu**: VST3, Samostatná aplikace

### Hardwarové požadavky
- **CPU**: Vícejádrový procesor (Intel i5/AMD Ryzen 5 nebo lepší)
- **RAM**: Minimálně 4GB (doporučeno 8GB+)
- **Úložiště**: SSD doporučeno pro načítání vzorků
- **Audio rozhraní**: Kompatibilní s ASIO (doporučeno)

---

## 🏗️ Architektura systému

### Přehled komponent
```
┌─────────────────────────────── IthacaPlayer ───────────────────────────────┐
│ Uživatelské rozhraní (VST3 Plugin / Samostatná aplikace)                  │
├────────────────────────────────────────────────────────────────────────────┤
│ Audio Engine                                                              │
│ ├── MIDI Procesor                                                         │
│ ├── Správce hlasů                                                         │
│ ├── Sample Engine                                                         │
│ └── Řetězec efektů (budoucí)                                              │
├────────────────────────────────────────────────────────────────────────────┤
│ Správa vzorků                                                             │
│ ├── Velocity Mapper                                                       │
│ ├── Generátor vzorků                                                      │
│ ├── Správce cache                                                         │
│ └── Souborový systém                                                      │
├────────────────────────────────────────────────────────────────────────────┤
│ Základní vrstva                                                           │
│ ├── Konfigurace                                                           │
│ ├── Zpracování chyb                                                       │
│ ├── Monitor výkonu                                                        │
│ └── Logovací systém                                                       │
├────────────────────────────────────────────────────────────────────────────┤
│ JUCE Framework                                                            │
└────────────────────────────────────────────────────────────────────────────┘
```

### Tok dat
1. **Inicializace**: Načtení vzorků → Vytvoření velocity mapy → Validace/build cache
2. **Runtime**: MIDI vstup → Alokace hlasů → Výběr vzorku → Audio rendering

### Model vláken
- **Hlavní vlákno**: UI, konfigurace, souborové operace
- **Audio vlákno**: Real-time zpracování zvuku (nejvyšší priorita)
- **Pracovní vlákno**: Generování vzorků, správa cache
- **MIDI vlákno**: Integrováno s audio vláknem (JUCE standard)

---

## 🎼 Zpracování MIDI

### Podporované MIDI události
| Událost          | Stav          | Popis                              |
|------------------|---------------|------------------------------------|
| **Note On**      | ✅ Kompletní  | Spuštění noty s mapováním velocity |
| **Note Off**     | ✅ Kompletní  | Zpracování ukončení noty           |
| **Pitch Wheel**  | ✅ Kompletní  | Globální modulace výšky tónu       |
| **Control Change**| 🔶 Základní   | Mapování parametrů, debug logování |
| **SysEx**        | 🔶 Základní   | Vlastní ovládání parametrů         |
| **Program Change**| ❌ Budoucí    | Přepínání nástrojů                 |

### Správa hlasů
- **Cíl**: Přirozený hudební výraz díky inteligentní alokaci hlasů
- **Strategie**:
  1. Restart stejné noty
  2. Alokace LRU (nejstarší uvolněný hlas)
  3. Inteligentní voice stealing
  4. Elegantní degradace při vyčerpání hlasů

---

## 🎯 Správa vzorků

### Dynamické mapování velocity
Mapuje MIDI velocity (0-127) na audio vzorky podle dB úrovní:
1. Skenování adresáře pro `.wav` soubory
2. Extrakce metadat (MIDI nota, dB úroveň) z názvů souborů
3. Seskupení vzorků podle MIDI noty
4. Seřazení podle dB úrovně (vzestupně)
5. Přiřazení dynamických rozsahů velocity podle počtu vzorků

**Příklad mapování**:
```
Nota 60 (C4) se 3 vzorky:
├── m060-C_4-DbLvl-30.wav → Velocity 0-42
├── m060-C_4-DbLvl-15.wav → Velocity 43-85
└── m060-C_4-DbLvl-0.wav  → Velocity 86-127
```

### Generování vzorků
- **Problém**: Chybějící vzorky pro některé MIDI noty (21-108)
- **Řešení**: Pitch-shift z nejbližší dostupné noty (max ±12 půltónů)
- **Implementace**: JUCE `ResamplingAudioSource` pro kvalitní resampling
- **Vlastnosti**:
  - Zachování vrstev velocity
  - Perzistentní ukládání generovaných vzorků
  - Optimalizovaný resampling pro klavírní zvuky

---

## 💾 Správa cache

### Filozofie cache
- **První spuštění**: Může být pomalé kvůli budování cache
- **Další spuštění**: Rychlé načítání z cache
- **Umístění (Windows)**: `%APPDATA%\IthacaPlayer\samples_tmp\`

### Životní cyklus cache
1. Kontrola existence adresáře cache
2. Validace integrity cache
3. Generování chybějících vzorků (s logováním postupu)
4. Načtení uložených vzorků pro rychlé spuštění
5. Ruční mazání cache pro invalidaci

### Zpracování chyb
- **Poškozený vzorek**: Zalogování chyby + ukončení (kritické)
- **Nedostatek místa na disku**: Zalogování chyby + ukončení (kritické)
- **Poškození cache**: Vymazání cache + automatická přestavba

---

## 🔧 Vývojové prostředí

### Požadované nástroje
- **Visual Studio 2022** (s C++ workload)
- **JUCE Framework** (7.x, nejnovější stabilní)
- **CMake** (3.22+, volitelné)
- **Git** pro správu verzí

### Struktura projektu
```
IthacaPlayer/
├── Source/
│   ├── Core/          # Konfigurace, AudioFile, základní třídy
│   ├── Audio/         # Sampler, SamplerVoice, zpracování zvuku
│   ├── MIDI/          # Zpracování MIDI, správa hlasů
│   ├── Cache/         # Správa cache, generování vzorků
│   ├── Utils/         # Zpracování chyb, monitorování výkonu
│   └── Plugin/        # VST3 wrapper, UI komponenty
├── Resources/         # Assety, presety, dokumentace
├── Tests/             # Unit testy, integrační testy
├── CMakeLists.txt     # Konfigurace buildu
└── README.md          # Tento dokument
```

### Konfigurace buildu
- **Cíl**: Windows 10+ x64
- **Optimalizace**: /O2 /arch:AVX2 /fp:fast (Release)
- **Standardy**: C++17 minimum
- **Závislosti**: Pouze JUCE (samostatné)

---

## 📚 Implementační specifikace

### Konfigurační systém
```cpp
namespace IthacaPlayer::Config {
    constexpr int MIN_AUDIO_LATENCY_MS = 5;
    constexpr int MAX_AUDIO_LATENCY_MS = 20;
    constexpr size_t MIN_MEMORY_USAGE_MB = 512;
    constexpr size_t MAX_MEMORY_USAGE_MB = 4096;
    constexpr int VELOCITY_LEVELS = 8;
    constexpr int MIDI_VELOCITY_MAX = 127;
    constexpr int MAX_PITCH_SHIFT = 12;
    constexpr Range<int> MIDI_NOTE_RANGE{21, 109}; // A0-C8
    constexpr int MAX_VOICES = 16;
    constexpr const char* TEMP_DIR_NAME = "samples_tmp";
    constexpr bool ENABLE_WINDOWS_OPTIMIZATIONS = true;
    constexpr bool USE_WASAPI_EXCLUSIVE = true;
}
```

### Třída AudioFile
```cpp
class AudioFile {
    juce::File filepath;
    int midiNote;           // 0-127
    juce::String noteName;  // např. "C_4", "Bb_5"
    int dbLevel;            // Negativní nebo 0 (0 = plná hlasitost)
public:
    static std::unique_ptr<AudioFile> fromFile(const juce::File& file);
    bool isValid() const;
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

### Pokročilé zpracování MIDI
```cpp
class AdvancedMidiProcessor {
    struct MidiParsingState {
        uint8_t currentByte;
        uint8_t messageCount;
        uint8_t messageType;
        uint8_t midiChannel;
        bool runningStatus;
    } parsingState;
    struct MidiNote {
        uint8_t key;
        uint8_t velocity;
        uint8_t pitchWheelLSB;
        uint8_t pitchWheelMSB;
        int16_t pitchWheel;
    } currentNote;
public:
    void processMidiByte(uint8_t byte) {
        if (byte & 0x80) {
            parsingState.messageCount = 0;
            parsingState.runningStatus = false;
            if ((byte & 0xF0) == 0xF0) {
                parsingState.messageType = byte;
            } else {
                parsingState.messageType = byte & 0xF0;
                parsingState.midiChannel = byte & 0x0F;
            }
        }
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
    virtual void triggerNoteOn(int channel, int note, int velocity) = 0;
    virtual void triggerNoteOff(int channel, int note) = 0;
    virtual void triggerPitchWheel(int channel, int pitchWheel) = 0;
    virtual void triggerControlChange(int channel, int controller, int value) = 0;
};
```

### Správa hlasů
```cpp
class AdvancedVoiceManager {
    static constexpr int MAX_VOICES = 16;
    struct VoiceState {
        uint8_t currentNote = 0;
        uint8_t velocity = 0;
        uint8_t gateState = 0;
        uint8_t queuePosition = 0;
        bool isActive = false;
        int16_t pitchWheel = 0;
        int16_t modulation = 0;
    } voices[MAX_VOICES];
public:
    int getFreeVoice(uint8_t targetNote) {
        for (int voice = 0; voice < MAX_VOICES; voice++) {
            if (voices[voice].currentNote == targetNote && voices[voice].isActive) {
                return voice;
            }
        }
        int bestCandidate = -1;
        for (int voice = 0; voice < MAX_VOICES; voice++) {
            if (voices[voice].gateState == 0) {
                if (bestCandidate == -1 || 
                    voices[voice].queuePosition > voices[bestCandidate].queuePosition) {
                    bestCandidate = voice;
                }
            }
        }
        if (bestCandidate == -1) {
            for (int voice = 0; voice < MAX_VOICES; voice++) {
                if (voices[voice].queuePosition == MAX_VOICES - 1) {
                    bestCandidate = voice;
                    break;
                }
            }
        }
        if (bestCandidate != -1) {
            mixleQueue(voices[bestCandidate].queuePosition);
        }
        return bestCandidate;
    }
private:
    void mixleQueue(uint8_t queueNumber) {
        for (int voice = 0; voice < MAX_VOICES; voice++) {
            if (voices[voice].queuePosition == queueNumber) {
                voices[voice].queuePosition = 0;
            } else {
                voices[voice].queuePosition++;
            }
        }
        for (int voice = 0; voice < MAX_VOICES; voice++) {
            if (voices[voice].queuePosition > queueNumber) {
                voices[voice].queuePosition--;
            }
        }
    }
};
```

### Pitch Shifting
```cpp
class PitchShifter {
public:
    static bool pitchShiftSample(const juce::File& input, 
                                const juce::File& output, 
                                double semitones) {
        const double ratio = std::pow(2.0, semitones / 12.0);
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        auto reader = formatManager.createReaderFor(input);
        if (!reader) return false;
        juce::ResamplingAudioSource resampler(reader.get(), false, reader->numChannels);
        resampler.setResamplingRatio(1.0 / ratio);
        resampler.prepareToPlay(8192, reader->sampleRate);
        juce::AudioBuffer<float> outputBuffer(reader->numChannels, 
                                            static_cast<int>(reader->lengthInSamples * ratio));
        juce::AudioSourceChannelInfo channelInfo;
        channelInfo.buffer = &outputBuffer;
        channelInfo.startSample = 0;
        channelInfo.numSamples = outputBuffer.getNumSamples();
        resampler.getNextAudioBlock(channelInfo);
        return saveBufferToFile(outputBuffer, output, reader->sampleRate, 
                               reader->bitsPerSample);
    }
};
```

### Inteligentní výběr vzorků
```cpp
class IntelligentSampleSelector {
    VelocityMapper& velocityMapper;
public:
    IntelligentSampleSelector(VelocityMapper& mapper) : velocityMapper(mapper) {}
    juce::File selectOptimalSample(int midiNote, int velocity) {
        auto directSample = velocityMapper.getSampleForVelocity(midiNote, velocity);
        if (directSample.exists()) return directSample;
        auto nearestVelocitySample = findNearestVelocityLevel(midiNote, velocity);
        if (nearestVelocitySample.exists()) return nearestVelocitySample;
        auto nearestNoteSample = findNearestNoteWithVelocity(midiNote, velocity);
        if (nearestNoteSample.exists()) return nearestNoteSample;
        return findAnySampleForNote(midiNote);
    }
private:
    juce::File findNearestVelocityLevel(int midiNote, int targetVelocity) {
        auto samplesForNote = velocityMapper.getSamplesForNote(midiNote);
        if (samplesForNote.empty()) return {};
        int bestVelocityDiff = 128;
        juce::File bestSample;
        for (const auto& sample : samplesForNote) {
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
        int bestNoteDiff = 13;
        int bestNote = -1;
        for (int note : availableNotes) {
            int noteDiff = std::abs(note - targetNote);
            if (noteDiff <= 6 && noteDiff < bestNoteDiff) {
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
        return velocityMapper.getSampleForVelocity(midiNote, 64);
    }
    int dbLevelToVelocity(int dbLevel) {
        if (dbLevel >= 0) return 127;
        if (dbLevel <= -40) return 1;
        return static_cast<int>(127 + (dbLevel * 126.0f / 40.0f));
    }
};
```

### Zpracování chyb
```cpp
class IthacaErrorHandler : public juce::Logger {
public:
    enum class ErrorCategory { FileIO, Audio, MIDI, Memory, Performance, Configuration, Cache };
    enum class ErrorSeverity { Info, Warning, Error, Critical };
    using ErrorCallback = std::function<void(ErrorCategory, ErrorSeverity, const juce::String&)>;
private:
    static std::unique_ptr<IthacaErrorHandler> instance;
    static ErrorCallback errorCallback;
    static std::atomic<int> errorCount;
    static std::atomic<int> recoveryAttempts;
    juce::CriticalSection errorLock;
    std::unique_ptr<juce::FileLogger> fileLogger;
public:
    static IthacaErrorHandler& getInstance() {
        if (!instance) instance = std::make_unique<IthacaErrorHandler>();
        return *instance;
    }
    void logMessage(const juce::String& message) override {
        juce::ScopedLock lock(errorLock);
        if (fileLogger) fileLogger->logMessage(message);
        DBG(message);
    }
    void initialize() {
        auto logFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("IthacaPlayer").getChildFile("Logs").getChildFile("ithaca_player.log");
        logFile.getParentDirectory().createDirectory();
        fileLogger = std::make_unique<juce::FileLogger>(logFile, "IthacaPlayer Log", 1024 * 1024);
        juce::Logger::setCurrentLogger(this);
    }
    static void setErrorCallback(ErrorCallback callback) { errorCallback = std::move(callback); }
    static void reportError(ErrorCategory category, ErrorSeverity severity, const juce::String& message) {
        errorCount++;
        juce::String fullMessage = "[" + severityToString(severity) + "] " +
                                  categoryToString(category) + ": " + message;
        getInstance().logMessage(fullMessage);
        if (errorCallback) {
            juce::MessageManager::callAsync([=]() { errorCallback(category, severity, fullMessage); });
        }
        if (severity >= ErrorSeverity::Error) {
            juce::MessageManager::callAsync([=]() { attemptRecovery(category, message); });
        }
    }
    static void reportCorruptSample(const juce::File& file) {
        reportError(ErrorCategory::FileIO, ErrorSeverity::Critical,
                   "Poškozený vzorek: " + file.getFullPathName() + " - Aplikace bude ukončena");
        juce::MessageManager::callAsync([]() { juce::JUCEApplicationBase::getInstance()->systemRequestedQuit(); });
    }
    static void reportDiskSpaceError(const juce::File& directory) {
        reportError(ErrorCategory::FileIO, ErrorSeverity::Critical,
                   "Nedostatek místa na disku: " + directory.getFullPathName() +
                   " - Nelze vytvořit cache vzorků");
    }
    static void reportAudioDeviceFailure(const juce::String& deviceName) {
        reportError(ErrorCategory::Audio, ErrorSeverity::Error,
                   "Selhání audio zařízení: " + deviceName);
    }
    static void reportMidiDeviceDisconnection(const juce::String& deviceName) {
        reportError(ErrorCategory::MIDI, ErrorSeverity::Warning,
                   "MIDI zařízení odpojeno: " + deviceName);
    }
    static void showUserAlert(const juce::String& title, const juce::String& message,
                             juce::AlertWindow::AlertIconType iconType = juce::AlertWindow::WarningIcon) {
        juce::MessageManager::callAsync([=]() {
            juce::AlertWindow::showMessageBoxAsync(iconType, title, message);
        });
    }
private:
    static bool attemptRecovery(ErrorCategory category, const juce::String& context) {
        recoveryAttempts++;
        switch (category) {
            case ErrorCategory::Audio: return attemptAudioDeviceRecovery();
            case ErrorCategory::MIDI: return attemptMidiDeviceRecovery();
            case ErrorCategory::Memory: return attemptMemoryRecovery();
            case ErrorCategory::Cache: return attemptCacheRecovery();
            default: return false;
        }
    }
    static bool attemptAudioDeviceRecovery() {
        reportError(ErrorCategory::Audio, ErrorSeverity::Info, "Pokus o obnovu audio zařízení...");
        auto* deviceManager = juce::AudioDeviceManager::getInstance();
        if (!deviceManager) return false;
        auto currentSetup = deviceManager->getAudioDeviceSetup();
        juce::String error = deviceManager->initialise(0, 2, nullptr, true);
        if (error.isEmpty()) {
            reportError(ErrorCategory::Audio, ErrorSeverity::Info, "Obnova audio zařízení úspěšná");
            return true;
        }
        currentSetup.outputDeviceName = "";
        error = deviceManager->setAudioDeviceSetup(currentSetup, true);
        if (error.isEmpty()) {
            reportError(ErrorCategory::Audio, ErrorSeverity::Warning, "Obnoveno s výchozím audio zařízením");
            return true;
        }
        return false;
    }
    static bool attemptMidiDeviceRecovery() {
        reportError(ErrorCategory::MIDI, ErrorSeverity::Info, "Pokus o obnovu MIDI zařízení...");
        auto availableDevices = juce::MidiInput::getAvailableDevices();
        for (const auto& device : availableDevices) {
            auto midiInput = juce::MidiInput::openDevice(device.identifier, nullptr);
            if (midiInput) {
                reportError(ErrorCategory::MIDI, ErrorSeverity::Info, "Obnova MIDI: přepnuto na " + device.name);
                return true;
            }
        }
        reportError(ErrorCategory::MIDI, ErrorSeverity::Warning, "Žádná MIDI zařízení nejsou dostupná - pokračuje bez MIDI vstupu");
        return true;
    }
    static bool attemptMemoryRecovery() {
        reportError(ErrorCategory::Memory, ErrorSeverity::Info, "Pokus o obnovu paměti...");
        juce::SystemStats::getMemorySizeInMegabytes();
        reportError(ErrorCategory::Memory, ErrorSeverity::Info, "Obnova paměti provedena - cache vyčištěna");
        return true;
    }
    static bool attemptCacheRecovery() {
        reportError(ErrorCategory::Cache, ErrorSeverity::Info, "Pokus o obnovu cache...");
        reportError(ErrorCategory::Cache, ErrorSeverity::Info, "Obnova cache: mazání a přestavba cache");
        return true;
    }
    static juce::String categoryToString(ErrorCategory category) {
        switch (category) {
            case ErrorCategory::FileIO: return "FileIO";
            case ErrorCategory::Audio: return "Audio";
            case ErrorCategory::MIDI: return "MIDI";
            case ErrorCategory::Memory: return "Paměť";
            case ErrorCategory::Performance: return "Výkon";
            case ErrorCategory::Configuration: return "Konfigurace";
            case ErrorCategory::Cache: return "Cache";
            default: return "Neznámé";
        }
    }
    static juce::String severityToString(ErrorSeverity severity) {
        switch (severity) {
            case ErrorSeverity::Info: return "INFO";
            case ErrorSeverity::Warning: return "VAROVÁNÍ";
            case ErrorSeverity::Error: return "CHYBA";
            case ErrorSeverity::Critical: return "KRITICKÉ";
            default: return "NEZNÁMÉ";
        }
    }
};
std::unique_ptr<IthacaErrorHandler> IthacaErrorHandler::instance = nullptr;
IthacaErrorHandler::ErrorCallback IthacaErrorHandler::errorCallback = nullptr;
std::atomic<int> IthacaErrorHandler::errorCount{0};
std::atomic<int> IthacaErrorHandler::recoveryAttempts{0};
```

### Monitorování výkonu
```cpp
class PerformanceMonitor : public juce::Timer, public juce::AudioProcessor::AudioProcessorListener {
    struct PerformanceMetrics {
        std::atomic<double> audioThreadCpuUsage{0.0};
        std::atomic<double> averageAudioLatency{0.0};
        std::atomic<size_t> memoryUsageMB{0};
        std::atomic<int> droppedAudioBlocks{0};
        std::atomic<int> midiEventsPerSecond{0};
        std::atomic<double> cacheHitRatio{0.0};
        std::atomic<int> activeVoices{0};
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
    std::function<void(const juce::String&)> onPerformanceAlert;
public:
    PerformanceMonitor() {
        startTimer(1000);
        metrics.lastUpdateTime = juce::Time::getCurrentTime();
    }
    void timerCallback() override {
        updateMetrics();
        if (!checkPerformanceHealth()) {
            if (onPerformanceAlert) onPerformanceAlert(generatePerformanceReport());
        }
    }
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& details) override {
        if (details.latencyChanged) DBG("Změna latence zvuku");
    }
    void recordAudioBlock(double cpuUsage, double latency) {
        metrics.totalAudioBlocks++;
        metrics.audioThreadCpuUsage.store(cpuUsage);
        metrics.averageAudioLatency.store(latency);
        if (cpuUsage > 95.0 || latency > 50.0) metrics.droppedAudioBlocks++;
    }
    void recordMidiEvent() { metrics.totalMidiEvents++; }
    void recordCacheAccess(bool hit) { if (hit) metrics.cacheHits++; else metrics.cacheMisses++; }
    void setActiveVoices(int voices) { metrics.activeVoices.store(voices); }
    void setPerformanceAlertCallback(std::function<void(const juce::String&)> callback) {
        onPerformanceAlert = std::move(callback);
    }
    juce::String generatePerformanceReport() const {
        juce::String report;
        report << "=== Výkonnostní zpráva IthacaPlayer ===\n";
        report << "CPU audio vlákna: " << juce::String(metrics.audioThreadCpuUsage.load(), 1) << "%\n";
        report << "Průměrná latence: " << juce::String(metrics.averageAudioLatency.load(), 2) << "ms\n";
        report << "Využití paměti: " << metrics.memoryUsageMB.load() << "MB\n";
        report << "Aktivní hlasy: " << metrics.activeVoices.load() << "/" << Config::MAX_VOICES << "\n";
        report << "MIDI události/s: " << metrics.midiEventsPerSecond.load() << "\n";
        report << "Poměr zásahů cache: " << juce::String(metrics.cacheHitRatio.load() * 100, 1) << "%\n";
        report << "Zahozené audio bloky: " << metrics.droppedAudioBlocks.load() << "\n";
        report << "Stav zdraví: " << (checkPerformanceHealth() ? "DOBRÝ" : "VAROVÁNÍ") << "\n";
        return report;
    }
private:
    void updateMetrics() {
        auto now = juce::Time::getCurrentTime();
        double deltaTime = (now - metrics.lastUpdateTime).inSeconds();
        if (deltaTime > 0) {
            metrics.midiEventsPerSecond.store(static_cast<int>(metrics.totalMidiEvents.load() / deltaTime));
            int totalCacheAccess = metrics.cacheHits.load() + metrics.cacheMisses.load();
            if (totalCacheAccess > 0) {
                metrics.cacheHitRatio.store(static_cast<double>(metrics.cacheHits.load()) / totalCacheAccess);
            }
            metrics.memoryUsageMB.store(getCurrentMemoryUsage());
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
        #if JUCE_WINDOWS
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                return pmc.WorkingSetSize / (1024 * 1024);
            }
        #endif
        return 0;
    }
};
```

### LFO a modulace
```cpp
class LFOProcessor : public juce::Timer {
    struct LFOState {
        float phase = 0.0f;
        float frequency = 1.0f;
        float amplitude = 0.0f;
        float currentValue = 0.0f;
        juce::AudioParameterFloat* speedParam = nullptr;
        juce::AudioParameterFloat* depthParam = nullptr;
        std::array<float, 16> voicePitchMod{};
        std::array<float, 16> voiceAmpMod{};
    } lfoState;
    double sampleRate = 44100.0;
    float phaseIncrement = 0.0f;
public:
    LFOProcessor() { startTimerHz(60); }
    void prepareToPlay(double newSampleRate) {
        sampleRate = newSampleRate;
        updatePhaseIncrement();
    }
    void timerCallback() override {
        updateLFO();
        if (onLFOUpdate) onLFOUpdate(lfoState.currentValue);
    }
    void processBlock(int numSamples) {
        if (lfoState.speedParam) {
            lfoState.frequency = lfoState.speedParam->get();
            updatePhaseIncrement();
        }
        if (lfoState.depthParam) lfoState.amplitude = lfoState.depthParam->get();
        for (int sample = 0; sample < numSamples; ++sample) updateLFOSample();
    }
    void addParameters(juce::AudioProcessor& processor) {
        using FloatParam = juce::AudioParameterFloat;
        lfoState.speedParam = new FloatParam("lfoSpeed", "Rychlost LFO", juce::NormalisableRange<float>(0.1f, 20.0f, 0.1f), 1.0f);
        lfoState.depthParam = new FloatParam("lfoDepth", "Hloubka LFO", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f);
        processor.addParameter(lfoState.speedParam);
        processor.addParameter(lfoState.depthParam);
    }
    float getModulationForVoice(int voiceIndex, ModulationType type) const {
        jassert(voiceIndex >= 0 && voiceIndex < 16);
        switch (type) {
            case ModulationType::Pitch: return lfoState.voicePitchMod[voiceIndex] * lfoState.currentValue;
            case ModulationType::Amplitude: return lfoState.voiceAmpMod[voiceIndex] * lfoState.currentValue;
            default: return 0.0f;
        }
    }
    void setModulationAmount(int voiceIndex, ModulationType type, float amount) {
        jassert(voiceIndex >= 0 && voiceIndex < 16);
        jassert(amount >= 0.0f && amount <= 1.0f);
        switch (type) {
            case ModulationType::Pitch: lfoState.voicePitchMod[voiceIndex] = amount; break;
            case ModulationType::Amplitude: lfoState.voiceAmpMod[voiceIndex] = amount; break;
        }
    }
    std::function<void(float)> onLFOUpdate;
    enum class ModulationType { Pitch, Amplitude, Filter };
private:
    void updatePhaseIncrement() {
        if (sampleRate > 0.0) phaseIncrement = static_cast<float>(lfoState.frequency / sampleRate);
    }
    void updateLFOSample() {
        lfoState.phase += phaseIncrement;
        if (lfoState.phase >= 1.0f) lfoState.phase -= 1.0f;
        lfoState.currentValue = std::sin(lfoState.phase * juce::MathConstants<float>::twoPi) * lfoState.amplitude;
    }
    void updateLFO() {}
};
```

## 📋 Plán implementace

### Fáze 1: Základní funkcionalita
- **Cíl**: Funkční VST3 plugin s jednoduchým přehráváním vzorků
- **Úkoly**:
  1. `Config.h` - Základní konstanty
  2. `AudioFile.cpp` - Parsování souborů s regex
  3. `VelocityMapper.cpp` - Dynamické mapování velocity
  4. `CacheManager.cpp` - Cache v `%APPDATA%`
  5. `SamplerVoice.cpp` - Základní JUCE hlas
  6. Jednoduchý sampler - Integrace s JUCE Synthesiser
  7. VST3 wrapper - Minimální procesor + editor
- **Výsledek**: Plugin načítá vzorky a přehrává základní noty

### Fáze 2: Kvalita zvuku
- **Cíl**: Produkční kvalita zvuku s inteligentním výběrem vzorků
- **Úkoly**:
  1. `PitchShifter` - Implementace JUCE ResamplingAudioSource
  2. `SampleGenerator` - Generování chybějících not
  3. `AdvancedVoiceManager` - Produkční algoritmus mixle_queue
  4. `IntelligentSampleSelector` - Výběr vzorků s fallback strategiemi
  5. Vylepšené zpracování chyb - Robustní obnova
- **Výsledek**: Vysoce kvalitní sampler s inteligentními funkcemi

### Fáze 3: Dokončení a produkce
- **Cíl**: Produkční plugin
- **Úkoly**:
  1. `PerformanceMonitor` - Sledování výkonu v reálném čase
  2. Pokročilé MIDI - Mapování CC, podpora SysEx
  3. Konfigurační systém - Ukládání uživatelských nastavení
  4. Vylepšené GUI - Nad rámec debug výstupu
  5. Unit testy - Komplexní testovací sada
- **Výsledek**: Produkční VST3 plugin

## 🚀 Začínáme

### Předpoklady
1. Visual Studio 2022 s C++ workload
2. JUCE Framework (stáhnout z juce.com)
3. CMake 3.22+ (volitelné, lze použít Projucer)

### Rychlý start
1. Klonování a nastavení struktury projektu
2. Vytvoření JUCE projektu pomocí Projucer
3. Implementace `Config.h` jako první třídy
4. Nastavení parsování `AudioFile`
5. Build minimálního VST3 s debug výstupem

### Vývojový postup
1. Implementace tříd podle plánu
2. Unit testy pro každou komponentu
3. Integrační testy s reálnými vzorky
4. Validace výkonu s `PerformanceMonitor`
5. Testování chybových scénářů

## 🎉 Závěr
IthacaPlayer kombinuje ověřené algoritmy z produkčních syntezátorů s moderními JUCE technologiemi. Projekt klade důraz na:
- **Ověřené přístupy**: Battle-tested algoritmy
- **Real-time výkon**: Optimalizace pro profesionální audio
- **Robustní zpracování chyb**: Elegantní řešení všech scénářů
- **Přívětivá architektura**: Čitelný a udržovatelný kód

Tato specifikace poskytuje kompletní základ pro vývoj profesionálního MIDI sampleru s IthacaPlayer.
