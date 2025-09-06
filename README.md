## Cesty na build soubory

[build]   juce_vst3_helper.vcxproj .\build\Debug\juce_vst3_helper.exe  
[build]   IthacaPlayer.vcxproj -> .\build\IthacaPlayer_artefacts\Debug\IthacaPlayer_SharedCode.lib  
[build]   IthacaPlayer_VST3.vcxproj -> .\build\IthacaPlayer_artefacts\Debug\VST3\IthacaPlayer.vst3\Contents\x86_64-win\IthacaPlayer.vst3  
[build]   IthacaPlayer_Standalone.vcxproj -> .\build\IthacaPlayer_artefacts\Debug\Standalone\IthacaPlayer.exe  

## Cesta na aplikační data

WIN+R: %APPDATA%\IthacaPlayer  

C:\Users\[uživatel]\AppData\Roaming\IthacaPlayer  

Zde se ukládají generované WAV soubory pro samples (v podsložce "instrument"), logy (IthacaPlayer.log) a další data.

## Tail aplikačního logu

```
Get-Content -Path C:\Users\nemej992\AppData\Roaming\IthacaPlayer\IthacaPlayer.log -Tail 10 -Wait
```

## MIDI tools

### VMPK

Virtuální MIDI klávesnice pro testování. Použijte počítačovou klávesnici nebo myš k hraní not a spojte s IthacaPlayer přes virtual MIDI porty.  

https://vmpk.sourceforge.io/#Download  
https://sourceforge.net/projects/vmpk/  

### loopMIDI

Nástroj pro vytváření virtuálních MIDI portů na Windows pro propojení aplikací (např. VMPK s IthacaPlayer).  

https://www.tobias-erichsen.de/software/loopmidi.html  

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

Audio plugin synthesizer implementovaný v JUCE frameworku s modulární architekturou a pokročilým voice managementem. Podporuje MIDI vstup, generování audio samplů (fallback na sine vlny), dynamic levels (0-7) pro velocity mapping a ADSR-ready voice systém.

## Architektura Systému  

### Přehled Komponent  

```
AudioPluginAudioProcessor (Main Controller)
├── SampleLibrary (Pre-computed Audio Storage)
│   └── SampleLoader (Loading/Generating/Resampling WAV)
├── MidiStateManager (MIDI Event Processing - Thread-Safe)
├── VoiceManager (Advanced Voice Allocation & ADSR-Ready)
├── Logger (Debug & Monitoring with Circular Buffer)
└── PluginEditor (GUI Interface with Full-Screen Image)
```

## Core Komponenty  

### 1. SampleLibrary  
**Účel:** Správa pre-computed audio samplů pro MIDI noty (21-108) s podporou 8 dynamic levels.  
**Klíčové vlastnosti:**  
- Automatické generování sine vln jako fallback.  
- Ukládání/resampling WAV souborů do %APPDATA%\IthacaPlayer\instrument\ (podpora 44.1kHz a 48kHz).  
- Velocity mapping na dynamic levels (0-7).  
- Podpora mono/stereo samplů s inteligentním fallback.  

### 2. SampleLoader  
**Účel:** Načítání/generování samplů s resamplingem a robustním error handlingem.  
**Klíčové vlastnosti:**  
- Naming convention: m[nota]-vel[level]-[sr].wav (např. m060-vel3-44.wav).  
- Fallback: Pokud soubor chybí, generuje sine pro 44.1kHz, resampluje na 48kHz a ukládá.  
- Spiral search pro dynamic levels s fallback na nižší/vyšší úrovně.  
- Statistiky loadingu (načtené/generované soubory, paměť).  

### 3. MidiStateManager (Opraveno)
**Účel:** Thread-safe správa MIDI stavu (noty, velocity, controllery).  
**Klíčové vlastnosti:**  
- **Thread-safe circular buffers** s mutex protection a atomic operacemi.  
- **Overflow handling** - automatické přepisování nejstarších záznamů.  
- **Robustní validace** všech MIDI vstupů s graceful error handling.  
- Inicializace standard controller hodnot (volume=100, pan=64, sustain=0).  
- Podpora 16 kanálů s kompletní error protection.  

### 4. VoiceManager (Kompletně refaktorován)
**Účel:** Pokročilý voice management s ADSR-ready architekturou.  
**Klíčové vlastnosti:**  
- **Voice State Management**: Inactive/Playing/Release states s automatickým lifecycle.  
- **Intelligent Voice Allocation**:
  1. Note restart detection (monofonie per nota)
  2. Free voice allocation (nejlepší dostupná)
  3. Release voice stealing (preferované)
  4. Playing voice stealing (last resort)
- **Release Counter System**: Minimalistická implementace s časovým limitem (100ms).  
- **Priority Queue Management**: Sophisticated priorita pro voice stealing.  
- **Comprehensive Statistics**: Real-time monitoring všech voice states.  
- **Stereo/Mono Rendering**: Optimalizované audio rendering s mix/duplicate logiku.  

### 5. Logger (Stabilizován)
**Účel:** Thread-safe logování s kruhovým bufferem.  
**Klíčové vlastnosti:**  
- Kruhový buffer (100 vstupů) s overflow protection.  
- Zápis do souboru (IthacaPlayer.log v %APPDATA%) s rotací.  
- Podpora úrovní (info/debug/error/warn) s intelligent filtering.  
- Thread-safe s mutex protection.  
- English debug messages, české komentáře.  

### 6. PluginProcessor (Robustní)
**Účel:** Hlavní audio procesor s enhanced error handling.  
**Klíčové vlastnosti:**  
- **Robustní MIDI validation** s range checking a error recovery.  
- **Thread-safe initialization** jen při změně sample rate/bufferu.  
- **Graceful error handling** s try-catch protection.  
- Podpora VST3/AU/Standalone formátů.  

### 7. PluginEditor (Vylepšeno)
**Účel:** GUI rozhraní s full-screen image support.  
**Klíčové vlastnosti:**  
- **Full-screen background image** s stretch-to-fit scaling.  
- **Overlay controls** s transparentním pozadím pro lepší čitelnost.  
- Toggle tlačítko pro zapnutí/vypnutí logování.  
- Bílý text s semi-transparentním pozadím pro kontrast.  
- Zvětšená velikost okna (400x600) pro lepší UX.  

## ADSR Koncept a Budoucí Rozšíření

### Aktuální implementace: Release Counter
- **Minimalistická varianta** pro rychlé testování a stabilizaci.
- **Release timer**: 100ms (4800 samples @ 48kHz) bez fade-out.
- **Automatické cleanup**: Voices se uvolňují po vypršení release času.
- **Thread-safe lifecycle**: Smooth přechody mezi voice states.

### Plánovaná ADSR architektura
```cpp
// Budoucí modulární design:
class ADSREnvelope {
    // Separátní ADSR logika
    EnvelopePhase: Attack/Decay/Sustain/Release
    Parameters: attackTime, decayTime, sustainLevel, releaseTime
    Exponenciální decay algoritmus (násobení 0.99, 0.98...)
};

// Integrace do SynthVoice:
- Block-based decay s konfigurovatelným koeficientem
- Per-sample nebo per-block aplikace envelope
- Modulární použití pro amplitude/filter/pitch modulation
```

### Navržené varianty implementace:
1. **Block-based decay**: Konstantní gain per block s exponenciálním decay
2. **Per-sample envelope**: Přesnější ale CPU náročnější
3. **Hybrid approach**: Kompromis mezi kvalitou a výkonem

## Kritické opravy a vylepšení

### Stability fixes:
- **Circular buffer overflow** - opravena logika v MidiStateManager
- **Race conditions** - eliminovány v voice allocation
- **Memory safety** - robustní null pointer checks
- **Voice lifecycle** - automatické cleanup release voices

### Performance optimalizace:
- **Intelligent voice stealing** preferuje release voices před playing
- **Spiral search fallback** pro dynamic levels
- **Cached statistics** pro real-time monitoring
- **Reduced logging noise** s batch processing

### Audio kvalita:
- **Stereo/mono handling** - všechny kombinace správně ošetřeny
- **Sample rate flexibility** - podpora 44.1kHz i 48kHz
- **Dynamic level mapping** - přirozené chování podle velocity

## Známé limity a plánovaná rozšíření

### Současné limity:
- Generované samples jsou mono sine vlny (bez harmonických)
- Release fáze bez fade-out (jen časový limit)
- Maximální polyfonie 16 voices
- Žádná reálná modulace (LFO, filter)

### Plánovaná rozšíření:
- **Plný ADSR envelope systém** s konfigurovatelnou křivkou
- **Filter section** s cutoff/resonance modulation
- **LFO system** pro vibrato/tremolo efekty
- **Multi-sample support** pro realistické nástroje
- **Effects chain** (reverb, delay, chorus)

## Binarni data

V adresáři decorators se nacházejí binary data pro GUI:

- **BinaryData.h/.cpp**: Auto-generované soubory obsahující embedovaný obrázek
- **ithaca-player-1.jpg**: Background image (309202 bytes)
- **Full-screen scaling**: Automatické roztažení na celou plochu GUI

## Development notes

### Testing workflow:
1. Použijte VMPK + loopMIDI pro MIDI testování
2. Sledujte real-time log: `Get-Content -Path %APPDATA%\IthacaPlayer\IthacaPlayer.log -Tail 10 -Wait`
3. Monitorujte voice allocation v debug výstupu
4. Testujte voice stealing při > 16 simultánních notách

### Debug priorities:
- Voice state transitions (Playing → Release → Inactive)
- MIDI validation a overflow handling
- Memory usage při loading velkých sample sad
- Audio quality při voice stealing

Projekt je připraven pro ADSR implementaci s modulární architekturou.

---

# Poznamky k build JUCE

https://cmake.org/download/
https://trirpi.github.io/posts/developing-audio-plugins-with-juce-and-visual-studio-code/


# pridani JUCE

```
git submodule add https://github.com/juce-framework/JUCE.git JUCE
```

```
git submodule update --init
```

```
cd JUCE
cmake -B build
cmake -B build -DJUCE_BUILD_EXTRAS=ON
cmake --build build --target AudioPluginHost
```

# Visual Studio Code

Build the Project 
- Terminal > Run Build Task (or press Ctrl+Shift+B)

Run Without Debugging 
- 