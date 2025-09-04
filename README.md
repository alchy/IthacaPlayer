## Cesty na build soubory

[build]   juce_vst3_helper.vcxproj .\build\Debug\juce_vst3_helper.exe
[build]   IthacaPlayer.vcxproj -> .\build\IthacaPlayer_artefacts\Debug\IthacaPlayer_SharedCode.lib
[build]   IthacaPlayer_VST3.vcxproj -> .\build\IthacaPlayer_artefacts\Debug\VST3\IthacaPlayer.vst3\Contents\x86_64-win\IthacaPlayer.vst3
[build]   IthacaPlayer_Standalone.vcxproj -> .\build\IthacaPlayer_artefacts\Debug\Standalone\IthacaPlayer.exe


## Nastavení vývojového prostředí

Pro kompilaci C++ projektu s CMakeLists.txt ve Visual Studio Code (VS Code) proveďte následující kroky. Předpokládá se instalace Visual Studio Build Tools (s MSVC kompilátorem: https://visualstudio.microsoft.com/cs/visual-cpp-build-tools/) a CMake.

### Požadavky
- Visual Studio Build Tools (MSVC kompilátor).
- CMake nainstalovaný a přidán do PATH (např. C:\Program Files\CMake\bin).
- VS Code.

### Kroky nastavení
1. **Instalace rozšíření ve VS Code**:
   - Otevřete Extensions (Ctrl+Shift+X).
   - Nainstalujte: C/C++ (od Microsoftu pro podporu C++ syntaxe, IntelliSense a ladění) a CMake Tools (od Microsoftu pro integraci CMake).
   - Restartujte VS Code.
2. **Otevření projektu**:
   - Přejděte na File > Open Folder a vyberte složku s CMakeLists.txt.
3. **Výběr kompilátoru (kit)**:
   - V Command Palette (Ctrl+Shift+P) napište "CMake: Select a Kit".
   - Vyberte "amd64" (64-bit) nebo ekvivalent podle potřeby (např. x64 pro moderní systémy).
4. **Konfigurace projektu**:
   - V Command Palette napište "CMake: Configure". To vygeneruje build soubory (obvykle ve složce "build").
5. **Build projektu**:
   - V Command Palette napište "CMake: Build" nebo použijte Shift+Ctrl+B (nyní nabídne CMake úlohy).
6. **Debugování (volitelně)**:
   - Nastavte breakpointy a spusťte "CMake: Debug" v Command Palette.

---

# IthacaPlayer - Software Synthesizer

Audio plugin synthesizer implementovaný v JUCE frameworku, inspirovaný hardwarovými syntezátory s modulární architekturou.

## Architektura Systému

### Přehled Komponent

```
AudioPluginAudioProcessor (Main Controller)
├── SampleLibrary (Pre-computed Audio Storage)
├── MidiStateManager (MIDI Event Processing)  
├── VoiceManager (Voice Allocation & Control)
└── Logger (Debug & Monitoring)
```

## Core Komponenty

### 1. SampleLibrary
**Účel:** Správa pre-computed audio sampelů pro jednotlivé MIDI noty

**Klíčové vlastnosti:**
- **Static allocation:** 292MB RAM pro 128 MIDI not × 12 sekund × sample rate
- **Pre-computed sine waves:** Generování při inicializaci místo realtime syntézy
- **Memory management:** Per-nota allocation s bezpečným uvolňováním
- **Rozšiřitelnost:** Připraveno pro načítání WAV souborů

**API:**
```cpp
SampleLibrary(double sampleRate)
bool generateSineWaveForNote(uint8_t midiNote, float frequency)
const float* getSampleData(uint8_t midiNote)
uint32_t getSampleLength(uint8_t midiNote)
bool isNoteAvailable(uint8_t midiNote)
```

**Implementační detaily:**
- Každý sample má pevnou délku 12 sekund
- Amplitude 0.3f pro prevenci clippingu
- Thread-safe přístup k sample datům

### 2. MidiStateManager
**Účel:** Centrální správa MIDI stavu a událostí

**Inspirováno:** Hardware MidiParser + ActiveKeys pattern
- Circular buffer approach pro MIDI zpracování
- Queue-based event distribution
- State tracking pro aktivní noty a controllery

**Klíčové struktury:**
```cpp
struct ActiveNote {
    uint8_t key, velocity, channel;
    bool isActive;
    uint32_t triggerTime; // Pro voice stealing
};
```

**Queue Management:**
- Oddělené queues pro Note On/Off události
- Per-channel event routing (16 MIDI kanálů)
- Pop/push pattern podobný hardware implementaci

**API:**
```cpp
void processMidiBuffer(const juce::MidiBuffer& midiBuffer)
uint8_t popNoteOn(uint8_t channel)   // Returns key nebo 0xff
uint8_t popNoteOff(uint8_t channel)  // Returns key nebo 0xff
void setPitchWheel(int16_t value)
void setControllerValue(uint8_t channel, uint8_t controller, uint8_t value)
```

### 3. VoiceManager
**Účel:** Polyphonic voice allocation a audio generování

**Inspirováno:** Hardware Performer class
- 16 polyphonic hlasů
- Sophisticated voice stealing algorithm
- Queue-based priority system

**Voice Allocation Algorithm:**
```cpp
int getFreeVoice(uint8_t note) {
    // 1. Hledá existující hlas s touto notou
    // 2. Hledá neaktivní hlas s nejvyšší queue pozicí  
    // 3. Krade aktivní hlas s nejvyšší queue pozicí
}
```

**Queue Management (mixleQueue):**
Algoritmus převzatý z HW implementace:
1. Vybraný hlas → queue pozice 0
2. Ostatní hlasy → pozice++
3. Komprese queue pozic > původní pozice

**SynthVoice vlastnosti:**
- Sample position tracking
- Velocity scaling
- Pitch wheel support
- No-loop playback (12s sample se přehraje jednou)

### 4. Logger System
**Účel:** Real-time debugging a monitoring

**Vlastnosti:**
- Thread-safe logování z audio vlákna
- Sliding window buffer (100 zpráv)
- Kategorizace: info/debug/warn
- GUI integration přes MessageManager

**Usage Pattern:**
```cpp
Logger::getInstance().log("Component/method", "severity", "message");
```

## Audio Processing Flow

### Main Processing Loop (processBlock)

```cpp
void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) {
    // 1. MIDI Processing
    midiStateManager_->processMidiBuffer(midiMessages);
    
    // 2. Voice Management  
    voiceManager_->processMidiEvents(*midiStateManager_);
    
    // 3. Audio Generation
    voiceManager_->generateAudio(channelData, numSamples, *sampleLibrary_);
    
    // 4. Refresh Cycle
    voiceManager_->refresh();
}
```

### Initialization Sequence

```
Constructor:
├── Create SampleLibrary (dummy sample rate)
├── Create MidiStateManager  
└── Create VoiceManager (16 voices)

prepareToPlay:
├── Reinit SampleLibrary (correct sample rate)
├── Generate sine wave pro Middle C (nota 60)
└── Mark synthInitialized = true
```

## Memory Management

### Sample Library Storage
- **Celková alokace:** ~292MB pro kompletní library
- **Current prototype:** Pouze 1 nota (Middle C) = ~2.3MB
- **Allocation strategy:** On-demand per nota
- **Deallocation:** Automatic při destrukci

### Voice Memory
- **16 SynthVoice objektů:** Minimální memory footprint  
- **Sample position tracking:** uint32_t per voice
- **State variables:** note, velocity, gate status

## MIDI Implementation

### Podporované MIDI zprávy:
- **Note On/Off:** Kompletní support včetně Note On velocity 0
- **Pitch Wheel:** 14-bit resolution (-8192 až +8191)
- **Control Change:** 128 controllers × 16 kanálů
- **Channel support:** 16 MIDI kanálů

### MIDI Routing:
- **Prototype:** Pouze kanál 0 aktivní
- **Full version:** Všech 16 kanálů připraveno

## Build System

### CMake Configuration:
```cmake
IS_SYNTH TRUE
NEEDS_MIDI_INPUT TRUE  
FORMATS AU VST3 Standalone
```

### Source Files:
```
Core Audio:
├── PluginProcessor.h/cpp
├── PluginEditor.h/cpp

Synth Engine:
├── SampleLibrary.h/cpp
├── MidiStateManager.h/cpp
├── VoiceManager.h/cpp

Utilities:
└── Logger.h/cpp
```

## Development Workflow

### Current Prototype Status:
- **Working:** MIDI input, voice allocation, sine wave playback
- **Limitation:** Pouze Middle C (nota 60) generuje audio
- **Voice count:** 16 polyphonic
- **Sample length:** 12 sekund per nota

### Next Development Steps:
1. **Full note range:** Generate sine waves pro všech 128 not
2. **WAV loading:** Implementace načítání externích sampelů
3. **ADSR envelope:** Note-off handling s envelope
4. **Effects:** Reverb, filter, modulation
5. **Preset system:** Save/load configurations

### Debug Features:
- **Real-time logging:** Vše se loguje do GUI
- **MIDI monitoring:** Detailní MIDI event tracking  
- **Voice status:** Queue positions, active notes
- **Performance:** Audio block processing statistics

## Technical Specifications

### Audio:
- **Sample rates:** 44.1kHz - 192kHz support
- **Bit depth:** 32-bit float processing
- **Latency:** Buffer size dependent (~10ms při 480 samples/48kHz)
- **Polyphony:** 16 hlasů

### MIDI:
- **Input latency:** Sub-millisecond  
- **Jitter:** Minimal díky JUCE MIDI buffering
- **Throughput:** Unlimited MIDI events per block

### Memory:
- **Runtime:** ~292MB při full library
- **Prototype:** ~2.3MB current usage
- **Stack:** Minimal - většinou heap allocation

### Performance:
- **CPU usage:** Low - pre-computed samples
- **Real-time safe:** Ano - žádné allokace v audio vlákně
- **Thread safety:** Logger + MIDI state management

## Architecture Decisions

### Pre-computed vs Real-time:
**Volba:** Pre-computed samples
**Důvod:** Konzistentní CPU usage, možnost komplexních waveforms
**Trade-off:** Vysoká paměť vs stabilní performance

### Voice Stealing Algorithm:
**Volba:** Queue-based priority system  
**Důvod:** Zachování hardware workflow, předvídatelné chování
**Benefit:** Longest-idle voice stealing

### MIDI State Management:
**Volba:** Centrální state s queue distribution
**Důvod:** Thread safety, clean separation of concerns
**Pattern:** Hardware MidiParser + ActiveKeys adaptace

### Logging Strategy:
**Volba:** Comprehensive real-time logging
**Důvod:** Complex debugging požadavky
**Implementation:** Thread-safe async GUI updates