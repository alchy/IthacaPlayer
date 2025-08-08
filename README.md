# IthacaPlayer - MIDI Samplovací Přehrávač v JUCE

## Přehled

**IthacaPlayer** je softwarový samplovací přehrávač vytvořený pomocí frameworku [JUCE](https://github.com/juce-framework/JUCE/), určený pro přehrávání zvukových vzorků ve formátu `.wav`, spouštěných MIDI vstupem. Kombinuje správu vzorků a mapování velocity z Python skriptu (`sample-player.py`) s robustním zpracováním MIDI a správou hlasů prostřednictvím C++ tříd (`Performer`, `Midi`, `DeviceVoice`). Projekt podporuje polyfonní přehrávání, dynamické mapování velocity, generování chybějících vzorků posunem výšky tónu a zpracování MIDI událostí (Note On/Off, Pitch Wheel, Control Change, System Exclusive).

Implementace využívá JUCE moduly pro audio a MIDI zpracování, což zajišťuje kompatibilitu napříč platformami (Windows, macOS, Linux) a efektivní výkon v reálném čase. Inspirací jsou příklady z JUCE (např. `SynthesiserDemo` v `juce_audio_basics`) a diskuse na [JUCE fóru](https://forum.juce.com/).

## Hlavní funkce

- **Správa vzorků**: Načítá `.wav` soubory s metadaty (MIDI nota, název noty, úroveň dB) podle pojmenovací konvence (např. `m060-C_4-DbLvl-20.wav`).
- **Mapování velocity**: Mapuje MIDI velocity na vzorky podle úrovně dB (negativní hodnoty, např. `DbLvl-20`, `DbLvl-32`, kde 0 je plná hlasitost).
- **Generování vzorků**: Vytváří chybějící vzorky pro MIDI noty posunem výšky tónu a ukládá je do mezipaměti v dočasné složce.
- **Zpracování MIDI**: Podporuje události Note On/Off, Pitch Wheel, Control Change a System Exclusive s robustním parsováním inspirovaným třídou `MidiParser`.
- **Polyfonní přehrávání**: Umožňuje až 16 hlasů s dynamickou alokací a přebíráním hlasů (inspirováno třídou `Performer`).
- **Kompatibilita napříč platformami**: Vytvořeno pomocí JUCE pro Windows, macOS a Linux.
- **Logování**: Poskytuje podrobný výstup pro ladění pomocí třídy JUCE `Logger`.

## Architektura projektu

### Přehled struktury

IthacaPlayer kombinuje správu `.wav` vzorků s negativními dB úrovněmi a robustní zpracování MIDI prostřednictvím Python skriptu `sample-player.py` a C++ tříd (`Performer`, `Midi`, `DeviceVoice`). Projekt je modularizován do několika klíčových tříd, které zajišťují funkčnost správy vzorků, mapování velocity, generování vzorků a zpracování MIDI událostí.

### Třídy a jejich funkce

| Třída              | Hlavní soubory                | Popis                                                                 |
|--------------------|-------------------------------|----------------------------------------------------------------------|
| **Config**         | `Config.h`                    | Definuje konstanty (počet velocity vrstev, rozsah MIDI not, dočasná složka). |
| **AudioFile**      | `AudioFile.h`, `AudioFile.cpp`| Spravuje načítání a parsování `.wav` souborů s metadaty.              |
| **VelocityMapper** | `VelocityMapper.h`, `VelocityMapper.cpp` | Mapuje MIDI velocity na vzorky podle dB úrovní.                     |
| **SampleGenerator**| `SampleGenerator.h`, `SampleGenerator.cpp` | Generuje chybějící vzorky posunem výšky tónu.                      |
| **MidiProcessor**  | `MidiProcessor.h`, `MidiProcessor.cpp` | Zpracovává MIDI události (Note On/Off, Pitch Wheel, CC, SysEx).      |
| **SamplerVoice**   | `SamplerVoice.h`, `SamplerVoice.cpp` | Řídí přehrávání jednotlivých vzorků pro polyfonní hlasy.             |
| **Sampler**        | `Sampler.h`, `Sampler.cpp`    | Koordinuje hlasy a zvuky, spravuje přehrávání not.                    |
| **SamplerSound**   | `Sampler.h`                   | Definuje rozsah not pro sampler.                                      |
| **MainAudioComponent** | `MainAudioComponent.h`, `MainAudioComponent.cpp` | Inicializuje Sampler a MidiProcessor, spravuje zvukový výstup.   |

### Podrobný popis tříd

#### Config (`Config.h`)

Definuje konstanty pro konfiguraci projektu.

| Metoda/Konstanta      | Vstup                          | Výstup                              | Účel                                              |
|-----------------------|--------------------------------|-------------------------------------|--------------------------------------------------|
| `velocityLevels`      | Žádný                          | `int` (8)                           | Počet vrstev velocity pro mapování.              |
| `midiVelocityMax`     | Žádný                          | `int` (127)                         | Maximální hodnota MIDI velocity.                 |
| `maxPitchShift`       | Žádný                          | `int` (±12 půltónů)                | Omezení posunu výšky pro generování vzorků.      |
| `midiNoteRange`       | Žádný                          | `juce::Range<int>` (21–108)         | Rozsah MIDI not pro přehrávání a generování.     |
| `tempDirName`         | Žádný                          | `juce::String` (samples_tmp)        | Název dočasné složky pro generované vzorky.      |

#### AudioFile (`AudioFile.h`, `AudioFile.cpp`)

Spravuje načítání a parsování `.wav` souborů.

| Metoda                | Vstup                          | Výstup                              | Účel                                              |
|-----------------------|--------------------------------|-------------------------------------|--------------------------------------------------|
| Konstruktor           | `juce::File`, `int`, `juce::String`, `int` | Instance `AudioFile`         | Inicializuje objekt vzorku s metadaty.           |
| `fromFile`            | `juce::File`                   | `std::unique_ptr<AudioFile>` nebo `nullptr` | Parsuje název souboru (např. `m060-C_4-DbLvl-20.wav`). |

#### VelocityMapper (`VelocityMapper.h`, `VelocityMapper.cpp`)

Mapuje MIDI velocity na vzorky podle dB úrovní.

| Metoda                | Vstup                          | Výstup                              | Účel                                              |
|-----------------------|--------------------------------|-------------------------------------|--------------------------------------------------|
| Konstruktor           | Žádný                          | Prázdná instance                    | Inicializuje datové struktury pro mapování.      |
| `buildVelocityMap`    | `juce::File`                   | Žádný                               | Skenuje složku, parsuje vzorky, vytváří mapu.     |
| `getSampleForVelocity`| `int`, `int`                   | `juce::File` nebo prázdný soubor    | Vyhledá vzorek pro danou notu a velocity.        |
| `addGeneratedSample`  | `int`, `std::pair<int, int>`, `juce::File` | Žádný                        | Přidá vygenerovaný vzorek do mapy.               |
| `getVelocityRanges`   | `size_t`                       | `std::vector<std::pair<int, int>>`  | Rozdělí MIDI velocity (0–127) na vrstvy.         |

#### SampleGenerator (`SampleGenerator.h`, `SampleGenerator.cpp`)

Generuje chybějící vzorky posunem výšky tónu.

| Metoda                | Vstup                          | Výstup                              | Účel                                              |
|-----------------------|--------------------------------|-------------------------------------|--------------------------------------------------|
| Konstruktor           | `VelocityMapper&`              | Instance                            | Inicializuje s přístupem k mapování vzorků.      |
| `generateMissingNotes`| `juce::File`                   | Žádný                               | Generuje chybějící vzorky a ukládá je.           |
| `pitchShiftSample`    | `juce::File`, `juce::File`, `int` | Žádný                             | Posune výšku tónu a uloží nový vzorek.           |
| `findNearestAvailableNote` | `int`                     | `int` nebo `-1`                     | Vyhledá nejbližší dostupnou notu v rozsahu.      |

#### MidiProcessor (`MidiProcessor.h`, `MidiProcessor.cpp`)

Zpracovává MIDI události.

| Metoda                | Vstup                          | Výstup                              | Účel                                              |
|-----------------------|--------------------------------|-------------------------------------|--------------------------------------------------|
| Konstruktor           | `juce::Synthesiser&`           | Instance                            | Inicializuje s přístupem k sampleru.             |
| `handleIncomingMidiMessage` | `juce::MidiInput*`, `juce::MidiMessage&` | Žádný                       | Zpracovává MIDI události (Note On/Off, Pitch Wheel, CC, SysEx). |
| `start`               | `juce::String`                 | Žádný                               | Otevře a spustí MIDI vstup.                      |
| `stop`                | Žádný                          | Žádný                               | Zastaví a uzavře MIDI vstup.                     |

#### SamplerVoice (`SamplerVoice.h`, `SamplerVoice.cpp`)

Řídí přehrávání jednotlivých vzorků.

| Metoda                | Vstup                          | Výstup                              | Účel                                              |
|-----------------------|--------------------------------|-------------------------------------|--------------------------------------------------|
| Konstruktor           | `VelocityMapper&`              | Instance                            | Inicializuje hlas pro přehrávání vzorku.         |
| `canPlaySound`        | `juce::SynthesiserSound*`      | `bool`                              | Ověří, zda zvuk je `SamplerSound`.               |
| `startNote`           | `int`, `float`, `juce::SynthesiserSound*`, `int` | Žádný                       | Načte a připraví vzorek pro přehrávání.          |
| `stopNote`            | `float`, `bool`                | Žádný                               | Zastaví přehrávání vzorku.                       |
| `renderNextBlock`     | `juce::AudioBuffer<float>&`, `int`, `int` | Žádný                         | Renderuje vzorky do výstupního bufferu.          |
| `pitchWheelMoved`     | `int`                          | Žádný                               | Aplikuje modulaci pitch wheelu (volitelné).      |
| `controllerMoved`     | `int`, `int`                   | Žádný                               | Zpracovává Control Change (volitelné).           |

#### Sampler (`Sampler.h`, `Sampler.cpp`)

Koordinuje hlasy a zvuky.

| Metoda                | Vstup                          | Výstup                              | Účel                                              |
|-----------------------|--------------------------------|-------------------------------------|--------------------------------------------------|
| Konstruktor           | `juce::File`, `bool`           | Instance                            | Inicializuje mapu velocity, hlasy a zvuky.       |
| `addSound`            | `juce::SynthesiserSound*`      | Žádný                               | Přidá `SamplerSound`.                            |
| `addVoice`            | `juce::SynthesiserVoice*`      | Žádný                               | Přidá `SamplerVoice`.                            |
| `noteOn`              | `int`, `int`, `float`          | Žádný                               | Spustí notu na volném hlasu.                     |
| `noteOff`             | `int`, `int`, `float`, `bool`  | Žádný                               | Zastaví notu.                                    |
| `handlePitchWheel`    | `int`, `int`                   | Žádný                               | Aplikuje pitch wheel na všechny hlasy.           |

#### SamplerSound (`Sampler.h`)

Definuje rozsah not pro sampler.

| Metoda                | Vstup                          | Výstup                              | Účel                                              |
|-----------------------|--------------------------------|-------------------------------------|--------------------------------------------------|
| Konstruktor           | `juce::String`, `int`, `int`   | Instance                            | Definuje rozsah not pro sampler.                 |
| `appliesToNote`       | `int`                          | `bool`                              | Ověří, zda nota patří do rozsahu.                |
| `appliesToChannel`    | `int`                          | `bool`                              | Potvrdí applicabilitu pro všechny kanály.        |

#### MainAudioComponent (`MainAudioComponent.h`, `MainAudioComponent.cpp`)

Spravuje zvukový výstup a inicializaci.

| Metoda                | Vstup                          | Výstup                              | Účel                                              |
|-----------------------|--------------------------------|-------------------------------------|--------------------------------------------------|
| Konstruktor           | `juce::File`, `bool`           | Instance                            | Inicializuje `Sampler` a `MidiProcessor`.        |
| `prepareToPlay`       | `int`, `double`                | Žádný                               | Připraví `Sampler` na renderování.               |
| `getNextAudioBlock`   | `juce::AudioSourceChannelInfo&` | Žádný                              | Renderuje zvuk přes `Sampler`.                   |
| `releaseResources`    | Žádný                          | Žádný                               | Uvolní zdroje (placeholder).                     |
| Destruktor            | Žádný                          | Žádný                               | Zastaví MIDI a zvuk.                             |

## Poznámky k implementaci

- **JUCE moduly**: `juce_core`, `juce_audio_basics`, `juce_audio_formats`, `juce_audio_devices`, `juce_audio_utils`.
- **Inspirace C++ kódem**:
  - `Performer.cpp`: Alokace hlasů a přebírání (mixle_queue) pro `Sampler`.
  - `DeviceVoice.cpp`: Správa not a velocity pro `SamplerVoice`.
  - `midi.cpp`: Robustní parsování MIDI pro `MidiProcessor`.
- **Formát vzorků**: Názvy ve formátu `mNNN-NOTA-DbLvl-X.wav`, kde dB je negativní (DbLvl-0 = plná hlasitost).
- **JUCE fórum**: Inspirace např. z [Simple Sampler Plugin](https://forum.juce.com/t/simple-sampler-plugin/23456).

---

# Meta-kód pro IthacaPlayer

## Přehled

Níže je meta-kód pro metody tříd projektu `IthacaPlayer`, samplovacího přehrávače v JUCE, inspirovaného `sample-player.py` a C++ třídami `Performer`, `Midi`, `DeviceVoice`. Meta-kód popisuje algoritmické kroky pro každou metodu uvedenou v `README.md`, zohledňuje negativní dB úrovně vzorků (např. `DbLvl-20`, 0 = plná hlasitost) a principy robustního MIDI zpracování, alokace hlasů a správy vzorků. Je určen jako zadání pro vývojáře C++.

## Meta-kód podle tříd

### Config (`Config.h`)

- **velocityLevels**
  ```
  VRAŤ konstantu 8 // Počet vrstev velocity
  ```

- **midiVelocityMax**
  ```
  VRAŤ konstantu 127 // Max MIDI velocity
  ```

- **maxPitchShift**
  ```
  VRAŤ konstantu 12 // Max posun výšky (±12 půltónů)
  ```

- **midiNoteRange**
  ```
  VRAŤ juce::Range<int>{21, 109} // Rozsah MIDI not (A0–C8)
  ```

- **tempDirName**
  ```
  VRAŤ juce::String "samples_tmp" // Název dočasné složky
  ```

### AudioFile (`AudioFile.h`, `AudioFile.cpp`)

- **Konstruktor**
  ```
  NASTAV file = vstupní soubor
  NASTAV midiNote = vstupní MIDI nota
  NASTAV noteName = vstupní název noty
  NASTAV dbLevel = vstupní dB úroveň (negativní nebo 0)
  ```

- **fromFile**
  ```
  ZÍSKEJ název souboru z file.getFileName()
  POKUD název odpovídá vzoru "m(\\d{3})-([A-G]#?_\\d)-DbLvl([-]?\\d+)\\.wav"
    EXTRAKUJ midiNote jako číslo z první skupiny
    EXTRAKUJ noteName jako řetězec z druhé skupiny
    EXTRAKUJ dbLevel jako číslo z třetí skupiny (ověř negativní nebo 0)
    VRAŤ nový AudioFile(file, midiNote, noteName, dbLevel)
  JINAK
    VRAŤ nullptr
  ```

### VelocityMapper (`VelocityMapper.h`, `VelocityMapper.cpp`)

- **Konstruktor**
  ```
  INICIALIZUJ velocityMap jako prázdnou std::map<std::pair<int, int>, juce::File>
  INICIALIZUJ availableNotes jako prázdnou std::set<int>
  ```

- **buildVelocityMap**
  ```
  LOGUJ "Prohledávám složku: " + inputDir.getFullPathName()
  INICIALIZUJ noteDbMap jako std::map<int, std::vector<std::pair<int, juce::File>>>
  PRO každý soubor v DirectoryIterator(inputDir, "*.wav")
    POKUD AudioFile::fromFile(soubor) vrátí platný audioFile
      PŘIDEJ (audioFile.dbLevel, audioFile.file) do noteDbMap[audioFile.midiNote]
      PŘIDEJ audioFile.midiNote do availableNotes
    KONEC
  PRO každý (midiNote, dbFiles) v noteDbMap
    SEŘAĎ dbFiles podle dbLevel (vzestupně, negativní hodnoty)
    ZÍSKEJ velocityRanges z getVelocityRanges(dbFiles.size())
    PRO každý ((dbLevel, file), (vStart, vEnd)) v zip(dbFiles, velocityRanges)
      PRO velocity od vStart do vEnd
        NASTAV velocityMap[{midiNote, velocity}] = file
      KONEC
    KONEC
  LOGUJ "Mapa velocity vytvořena pro " + availableNotes.size() + " not"
  ```

- **getSampleForVelocity**
  ```
  NAJDI (midiNote, velocity) v velocityMap
  POKUD nalezeno
    VRAŤ odpovídající juce::File
  JINAK
    VRAŤ prázdný juce::File
  ```

- **addGeneratedSample**
  ```
  PRO velocity od velocityRange.first do velocityRange.second
    NASTAV velocityMap[{midiNote, velocity}] = file
  KONEC
  PŘIDEJ midiNote do availableNotes
  ```

- **getVelocityRanges**
  ```
  INICIALIZUJ ranges jako std::vector<std::pair<int, int>>
  NASTAV step = Config::midiVelocityMax / levelCount
  PRO i od 0 do levelCount-1
    NASTAV start = zaokrouhli(i * step)
    NASTAV end = (i == levelCount-1) ? Config::midiVelocityMax-1 : zaokrouhli((i+1) * step)-1
    PŘIDEJ (start, end) do ranges
  KONEC
  VRAŤ ranges
  ```

### SampleGenerator (`SampleGenerator.h`, `SampleGenerator.cpp`)

- **Konstruktor**
  ```
  NASTAV velocityMapper = vstupní reference
  ```

- **generateMissingNotes**
  ```
  PRO note v Config::midiNoteRange
    POKUD velocityMapper.getSampleForVelocity(note, 64) existuje
      POKRAČUJ
    KONEC
    NASTAV nearestNote = findNearestAvailableNote(note)
    POKUD nearestNote == -1
      POKRAČUJ
    KONEC
    NASTAV baseFile = velocityMapper.getSampleForVelocity(nearestNote, 64)
    POKUD baseFile neexistuje
      POKRAČUJ
    KONEC
    NASTAV semitoneShift = note - nearestNote
    POKUD abs(semitoneShift) > Config::maxPitchShift
      POKRAČUJ
    KONEC
    NASTAV newFile = tempDir.getChildFile("m" + formátuj(note, "03d") + "-generated-DbLvl0.wav")
    VOLEJ pitchShiftSample(baseFile, newFile, semitoneShift)
    VOLEJ velocityMapper.addGeneratedSample(note, {0, Config::midiVelocityMax-1}, newFile)
  KONEC
  ```

- **pitchShiftSample**
  ```
  INICIALIZUJ formatManager jako AudioFormatManager
  REGISTRUJ základní formáty (WAV)
  VYTVOŘ reader = formatManager.createReaderFor(input)
  POKUD reader neexistuje
    VRAŤ
  KONEC
  VYTVOŘ buffer s reader.numChannels, reader.lengthInSamples
  ČTI reader do buffer
  NASTAV ratio = pow(2.0, semitones / 12.0)
  VYTVOŘ resampled buffer s reader.numChannels, buffer.numSamples / ratio
  PRO každý kanál v buffer
    ZPRACUJ interpolator s ratio, buffer[kanál], resampled[kanál]
  KONEC
  VYTVOŘ writer = WavAudioFormat.createWriterFor(output, reader.sampleRate, reader.numChannels, 16)
  POKUD writer existuje
    ZAPIŠ resampled do writer
  KONEC
  ```

- **findNearestAvailableNote**
  ```
  NASTAV minDistance = Config::maxPitchShift + 1
  NASTAV nearestNote = -1
  PRO note v velocityMapper.availableNotes
    POKUD abs(note - targetNote) <= Config::maxPitchShift A abs(note - targetNote) < minDistance
      NASTAV minDistance = abs(note - targetNote)
      NASTAV nearestNote = note
    KONEC
  KONEC
  VRAŤ nearestNote
  ```

### MidiProcessor (`MidiProcessor.h`, `MidiProcessor.cpp`)

- **Konstruktor**
  ```
  NASTAV synthesiser = vstupní reference
  ```

- **handleIncomingMidiMessage**
  ```
  POKUD message.isNoteOn()
    VOLEJ synthesiser.noteOn(message.getChannel(), message.getNoteNumber(), message.getVelocity())
    LOGUJ "Note On: " + message.getNoteNumber() + ", Velocity: " + message.getVelocity()
  NEBO POKUD message.isNoteOff()
    VOLEJ synthesiser.noteOff(message.getChannel(), message.getNoteNumber(), message.getVelocity(), true)
    LOGUJ "Note Off: " + message.getNoteNumber()
  NEBO POKUD message.isPitchWheel()
    NASTAV pitchWheelValue = message.getPitchWheelValue() - 8192
    VOLEJ synthesiser.handlePitchWheel(message.getChannel(), pitchWheelValue)
  NEBO POKUD message.isController()
    LOGUJ "Control Change: " + message.getControllerNumber() + ", Value: " + message.getControllerValue()
    // Mapuj na SysEx podle potřeby (inspirováno MidiParser::ControlChange)
  NEBO POKUD message.isSysEx()
    // Parsuj SysEx data (inspirováno MidiParser::SystemExclusive)
    LOGUJ "SysEx přijato"
  KONEC
  ```

- **start**
  ```
  NASTAV midiInput = MidiInput::openDevice(deviceName, this)
  POKUD midiInput existuje
    VOLEJ midiInput->start()
  KONEC
  ```

- **stop**
  ```
  POKUD midiInput existuje
    VOLEJ midiInput->stop()
    NASTAV midiInput = nullptr
  KONEC
  ```

### SamplerVoice (`SamplerVoice.h`, `SamplerVoice.cpp`)

- **Konstruktor**
  ```
  NASTAV velocityMapper = vstupní reference
  NASTAV isPlaying = false
  NASTAV currentSample = 0
  ```

- **canPlaySound**
  ```
  VRAŤ dynamic_cast<SamplerSound*>(sound) != nullptr
  ```

- **startNote**
  ```
  NASTAV samplerSound = dynamic_cast<SamplerSound*>(sound)
  POKUD samplerSound neexistuje
    VRAŤ
  KONEC
  NASTAV file = velocityMapper.getSampleForVelocity(midiNoteNumber, velocity * Config::midiVelocityMax)
  POKUD file neexistuje
    VRAŤ
  KONEC
  INICIALIZUJ formatManager jako AudioFormatManager
  REGISTRUJ základní formáty (WAV)
  NASTAV reader = formatManager.createReaderFor(file)
  POKUD reader existuje
    NASTAV velikost buffer na reader.numChannels, reader.lengthInSamples
    ČTI reader do buffer
    NASTAV currentSample = 0
    NASTAV isPlaying = true
  KONEC
  ```

- **stopNote**
  ```
  POKUD allowTailOff
    // Implementuj obálku pokud potřeba
  JINAK
    VOLEJ clearCurrentNote()
    NASTAV isPlaying = false
  KONEC
  ```

- **renderNextBlock**
  ```
  POKUD NOT isPlaying NEBO NOT reader
    VRAŤ
  KONEC
  PRO i od 0 do numSamples A currentSample < buffer.numSamples
    PRO každý kanál v outputBuffer
      PŘIDEJ buffer[kanál % buffer.numChannels, currentSample] do outputBuffer[kanál, startSample + i]
    KONEC
    INKREMENTUJ currentSample
  KONEC
  POKUD currentSample >= buffer.numSamples
    VOLEJ clearCurrentNote()
    NASTAV isPlaying = false
  KONEC
  ```

- **pitchWheelMoved**
  ```
  NASTAV pitchWheelValue = newPitchWheelValue - 8192
  // Aplikuj na přehrávání pokud implementováno
  ```

- **controllerMoved**
  ```
  LOGUJ "Controller: " + controllerNumber + ", Value: " + newControllerValue
  // Aktualizuj parametry přehrávání pokud potřeba
  ```

### Sampler (`Sampler.h`, `Sampler.cpp`)

- **Konstruktor**
  ```
  VOLEJ velocityMapper.buildVelocityMap(inputDir)
  NASTAV tempDir = inputDir.getSiblingFile(Config::tempDirName)
  VYTVOŘ generator jako SampleGenerator(velocityMapper)
  VOLEJ generator.generateMissingNotes(tempDir)
  PŘIDEJ nový SamplerSound("default", Config::midiNoteRange.start, Config::midiNoteRange.end)
  PRO i od 0 do 15
    PŘIDEJ nový SamplerVoice(velocityMapper)
  KONEC
  ```

- **addSound**
  ```
  VOLEJ Synthesiser::addSound(sound) // Zděděno z JUCE
  ```

- **addVoice**
  ```
  VOLEJ Synthesiser::addVoice(voice) // Zděděno z JUCE
  ```

- **noteOn**
  ```
  VOLEJ Synthesiser::noteOn(midiChannel, midiNoteNumber, velocity) // Zděděno, spustí SamplerVoice::startNote
  ```

- **noteOff**
  ```
  VOLEJ Synthesiser::noteOff(midiChannel, midiNoteNumber, velocity, allowTailOff) // Zděděno, spustí SamplerVoice::stopNote
  ```

- **handlePitchWheel**
  ```
  VOLEJ Synthesiser::handlePitchWheel(midiChannel, wheelValue) // Zděděno, spustí SamplerVoice::pitchWheelMoved
  ```

### SamplerSound (`Sampler.h`)

- **Konstruktor**
  ```
  NASTAV name = vstupní název
  NASTAV noteRange = {minNote, maxNote}
  ```

- **appliesToNote**
  ```
  VRAŤ noteRange.contains(midiNoteNumber)
  ```

- **appliesToChannel**
  ```
  VRAŤ true
  ```

### MainAudioComponent (`MainAudioComponent.h`, `MainAudioComponent.cpp`)

- **Konstruktor**
  ```
  VYTVOŘ sampler s inputDir, cleanTemp
  VYTVOŘ midiProcessor s sampler
  VOLEJ setAudioChannels(0, 2) // Stereo výstup
  VOLEJ midiProcessor.start(první název MIDI zařízení)
  ```

- **prepareToPlay**
  ```
  VOLEJ sampler.prepareToPlay(samplesPerBlockExpected, sampleRate)
  ```

- **getNextAudioBlock**
  ```
  VOLEJ sampler.renderNextBlock(bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples)
  ```

- **releaseResources**
  ```
  // Bez operací nebo uvolni zdroje pokud potřeba
  ```

- **Destruktor**
  ```
  VOLEJ midiProcessor.stop()
  VOLEJ shutdownAudio()
  ```

## Poznámky k implementaci

- **JUCE moduly**: Použij `juce_core`, `juce_audio_basics`, `juce_audio_formats`, `juce_audio_devices`, `juce_audio_utils`.
- **Inspirace C++ kódem**:
  - `Performer.cpp`: Implementuj přebírání hlasů v `Sampler` podle `mixle_queue`.
  - `DeviceVoice.cpp`: Správa not a velocity v `SamplerVoice` podle `play` a `velocity`.
  - `midi.cpp`: Robustní parsování MIDI v `MidiProcessor` podle `MidiParser::Parse`.
- **Vzorky**: Formát `mNNN-NOTA-DbLvl-X.wav`, dB negativní (0 = plná hlasitost).
- **JUCE fórum**: Inspirace např. z https://forum.juce.com/t/pitch-shifting-samples/12345 pro `pitchShiftSample`.
