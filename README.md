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
- **Formát vzorků**: Názvy ve formátu `mNNN-NOTA-DbLvl-X.wav`, kde dB je negativní (0 = plná hlasitost).
- **JUCE fórum**: Inspirace např. z [Simple Sampler Plugin](https://forum.juce.com/t/simple-sampler-plugin/23456).

## Možnost stažení

Markdown soubor s popisem projektu lze stáhnout zde: [IthacaPlayer.md](#).
