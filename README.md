## Cesty na build soubory

[build]   juce_vst3_helper.vcxproj .\build\Debug\juce_vst3_helper.exe  
[build]   IthacaPlayer.vcxproj -> .\build\IthacaPlayer_artefacts\Debug\IthacaPlayer_SharedCode.lib  
[build]   IthacaPlayer_VST3.vcxproj -> .\build\IthacaPlayer_artefacts\Debug\VST3\IthacaPlayer.vst3\Contents\x86_64-win\IthacaPlayer.vst3  
[build]   IthacaPlayer_Standalone.vcxproj -> .\build\IthacaPlayer_artefacts\Debug\Standalone\IthacaPlayer.exe  

## Cesta na aplikacni data

WIN+R: %APPDATA%\IthacaPlayer  

C:\Users\[uživatel]\AppData\Roaming\IthacaPlayer  

Zde se ukládají generované WAV soubory pro samples (v podsložce "instrument"), logy (IthacaPlayer.log) a další data.

## Tail aplikacniho logu

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

Audio plugin synthesizer implementovaný v JUCE frameworku, inspirovaný hardwarovými syntezátory s modulární architekturou. Podporuje MIDI vstup, generování audio samplů (fallback na sine vlny), dynamic levels (0-7) pro velocity mapping a stereo samples.  

## Architektura Systému  

### Přehled Komponent  

```
AudioPluginAudioProcessor (Main Controller)
├── SampleLibrary (Pre-computed Audio Storage)
│   └── SampleLoader (Loading/Generating/Resampling WAV)
├── MidiStateManager (MIDI Event Processing)
├── VoiceManager (Voice Allocation & Control)
├── Logger (Debug & Monitoring with Circular Buffer)
└── PluginEditor (GUI Interface with Logging)
```


## Core Komponenty  

### 1. SampleLibrary  
**Účel:** Správa pre-computed audio samplů pro MIDI noty (21-108) s podporou 8 dynamic levels.  
**Klíčové vlastnosti:**  
- Automatické generování sine vln jako fallback.  
- Ukládání/resampling WAV souborů do %APPDATA%\IthacaPlayer\instrument\ (podpora 44.1kHz a 48kHz).  
- Velocity mapping na dynamic levels (0-7).  
- Podpora mono/stereo samplů.  

### 2. SampleLoader  
**Účel:** Načítání/generování samplů s resamplingem.  
**Klíčové vlastnosti:**  
- Naming convention: m[nota]-vel[level]-[sr].wav (např. m060-vel3-44.wav).  
- Fallback: Pokud soubor chybí, generuje sine pro 44.1kHz, resampluje na 48kHz a ukládá.  
- Statistiky loadingu (načtené/generované soubory, paměť).  

### 3. MidiStateManager  
**Účel:** Správa MIDI stavu (noty, velocity, controllery).  
**Klíčové vlastnosti:**  
- Kruhové fronty pro note-on/off (thread-safe s mutex a atomic).  
- Inicializace default controller hodnot (např. volume=100).  
- Podpora 16 kanálů.  

### 4. VoiceManager  
**Účel:** Alokace a kontrola hlasů (polyfonie).  
**Klíčové vlastnosti:**  
- Až 16 hlasů s dynamic level selection.  
- Enhanced voice stealing (nejstarší hlas s nejvyšším progressem).  
- Stereo rendering (mix/duplicate kanálů).  
- Statistiky: Aktivní hlasy, podle levels, průměrný progress, stolen voices.  

### 5. Logger  
**Účel:** Logování s kruhovým bufferem a GUI display.  
**Klíčové vlastnosti:**  
- Kruhový buffer (100 vstupů) pro efektivní paměť.  
- Zápis do souboru (IthacaPlayer.log v %APPDATA%).  
- Podpora úrovní (info/debug/error/warn).  
- Thread-safe s mutex.  
- Integrace s GUI pro real-time display.  

### 6. PluginProcessor  
**Účel:** Hlavní audio procesor (JUCE-based).  
**Klíčové vlastnosti:**  
- Robustní error handling s try-catch a recovery.  
- Inicializace jen při změně sample rate/bufferu.  
- Podpora VST3/AU/Standalone.  

### 7. PluginEditor  
**Účel:** GUI rozhraní pro debugging a monitoring.  
**Klíčové vlastnosti:**  
- Real-time log display s matrix theme (zelený text na tmavém pozadí, monospace font).  
- Toggle tlačítko pro zapnutí/vypnutí logování.  
- Tlačítko pro vyčištění logů.  
- Gradient pozadí a nadpis pro lepší vizuál.  
- Velikost okna: 1024x600 pro pohodlné čtení logů.  

## Změny a Opravy  
- Odstraněny warningy C4244 (explicitní casts pro MIDI hodnoty).  
- Nahrazeno ScopedPointer unique_ptr (moderní C++).  
- Přidána thread-safety (atomic, mutex pro fronty a stavy).  
- Optimalizace: Kruhové buffery, fallback na nižší/vyšší dynamic levels.  
- GUI vylepšení: Matrix styl, toggle/clear funkce, rozšířené logování při resize/paint.  
- Známé limity: Generované samples jsou mono/sine (bez ADSR envelope), max polyfonie 16, žádná reálná modulace (pouze sine).  

## Plánované Rozšíření  
- ADSR envelope pro voices.  
