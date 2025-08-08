# IthacaPlayer - MIDI Samplovací Přehrávač v JUCE

## Přehled

**IthacaPlayer** je softwarový samplovací přehrávač vytvořený pomocí frameworku [JUCE](https://github.com/juce-framework/JUCE/), navržený pro přehrávání zvukových vzorků ve formátu `.wav` spouštěných MIDI vstupem. Kombinuje správu vzorků a mapování velocity z Python skriptu (`sample-player.py`) s robustním zpracováním MIDI a správou hlasů z vestavěného C++ syntezátoru (`Performer`, `Midi`, `DeviceVoice`). Projekt podporuje polyfonní přehrávání, dynamické mapování velocity, generování chybějících vzorků posunem výšky tónu a zpracování MIDI událostí (Note On/Off, Pitch Wheel, Control Change, System Exclusive).

Implementace využívá audio a MIDI zpracování JUCE, což zajišťuje kompatibilitu napříč platformami a efektivní výkon v reálném čase. Inspirací jsou příklady z JUCE (např. `SynthesiserDemo` v `juce_audio_basics`) a diskuse na [JUCE fóru](https://forum.juce.com/).

## Funkce

- **Správa vzorků**: Načítá `.wav` soubory s metadaty (MIDI nota, název noty, úroveň dB) podle pojmenovací konvence (např. `m060-C_4-DbLvl-20.wav`).
- **Mapování velocity**: Mapuje MIDI velocity na vzorky podle úrovně dB (vždy negativní, např. `DbLvl-20`, `DbLvl-32`, kde 0 je plná hlasitost).
- **Generování vzorků**: Vytváří chybějící vzorky pro MIDI noty posunem výšky tónu, s ukládáním do mezipaměti v dočasné složce.
- **Zpracování MIDI**: Zpracovává události Note On/Off, Pitch Wheel, Control Change a System Exclusive, inspirováno robustním parsováním z `MidiParser`.
- **Polyfonní přehrávání**: Podporuje až 16 hlasů s dynamickou alokací a přebíráním hlasů podle přístupu z `Performer`.
- **Kompatibilita napříč platformami**: Vytvořeno s JUCE pro Windows, macOS a Linux.
- **Logování**: Poskytuje podrobný výstup pro ladění pomocí třídy JUCE `Logger`.

## Architektura

Projekt je modulární, s třídami oddělujícími funkcionalitu pro snadnou údržbu a škálovatelnost. Každá třída využívá specifické moduly JUCE pro zpracování zvuku a MIDI.

### Třídy a jejich úlohy

1. **Config** (`Config.h`):
   - Ukládá statické konstanty (např. rozsah MIDI not: 21–108, počet úrovní velocity: 8, maximální posun výšky: ±12 půltónů).
   - Využívá `juce::Range` a `juce::String` z `juce_core`.

2. **AudioFile** (`AudioFile.h`, `AudioFile.cpp`):
   - Reprezentuje `.wav` soubor s metadaty (MIDI nota, název noty, úroveň dB).
   - Parsuje názvy souborů (např. `m060-C_4-DbLvl-20.wav`) pomocí `juce::String` a `juce::File`.

3. **VelocityMapper** (`VelocityMapper.h`, `VelocityMapper.cpp`):
   - Mapuje MIDI noty a velocity na zvukové soubory, rozděluje rozsah velocity (0–127) na vrstvy.
   - Využívá `juce::DirectoryIterator` pro prohledávání vzorků a `juce::Logger` pro ladění.

4. **SampleGenerator** (`SampleGenerator.h`, `SampleGenerator.cpp`):
   - Generuje chybějící vzorky posunem výšky tónu pomocí `juce::LagrangeInterpolator`.
   - Ukládá vygenerované vzorky do dočasné složky (`samples_tmp/`).
   - Závisí na `VelocityMapper` pro vyhledávání vzorků.

5. **MidiProcessor** (`MidiProcessor.h`, `MidiProcessor.cpp`):
   - Zpracovává MIDI vstup (Note On/Off, Pitch Wheel, CC, SysEx) přes `juce::MidiInputCallback`.
   - Přesměrovává události na `Sampler`, inspirováno `MidiParser`.

6. **SamplerVoice** (`SamplerVoice.h`, `SamplerVoice.cpp`):
   - Rozšiřuje `juce::SynthesiserVoice` pro přehrávání jednoho vzorku pro MIDI notu.
   - Načítá vzorky pomocí `juce::AudioFormatManager` a renderuje zvuk s `juce::AudioBuffer`.
   - Podporuje Pitch Wheel a CC, inspirováno `DeviceVoice`.

7. **Sampler** (`Sampler.h`, `Sampler.cpp`):
   - Rozšiřuje `juce::Synthesiser` pro správu více hlasů (výchozí: 16) a jednoho `SamplerSound`.
   - Zpracovává alokaci a přebírání hlasů, inspirováno `Performer::get_free_voice`.
   - Integruje `VelocityMapper` a `SampleGenerator`.

8. **SamplerSound** (`Sampler.h`):
   - Rozšiřuje `juce::SynthesiserSound` pro definici rozsahu MIDI not pro sampler.
   - Používáno `SamplerVoice` pro ověření platných not.

9. **MainAudioComponent** (`MainAudioComponent.h`, `MainAudioComponent.cpp`):
   - Rozšiřuje `juce::AudioAppComponent` pro správu zvukového výstupu a inicializaci `Sampler` a `MidiProcessor`.
   - Zpracovává renderování zvuku přes `prepareToPlay` a `getNextAudioBlock`.

### Závislosti mezi metodami

- **Config**: Poskytuje konstanty používané `VelocityMapper`, `SampleGenerator`, `Sampler` a `MainAudioComponent`.
- **AudioFile::fromFile**: Volána metodou `VelocityMapper::buildVelocityMap` pro parsování vzorků.
- **VelocityMapper::buildVelocityMap**: Používá `AudioFile` a `Config` pro vytvoření mapy velocity, volána konstruktorem `Sampler`.
- **SampleGenerator::generateMissingNotes**: Závisí na `VelocityMapper` a `Config`, volána konstruktorem `Sampler`.
- **MidiProcessor::handleIncomingMidiMessage**: Přesměrovává MIDI události na `Sampler`, který volá metody `SamplerVoice`.
- **SamplerVoice::startNote/renderNextBlock**: Používá `VelocityMapper::getSampleForVelocity` pro načtení a přehrávání vzorků.
- **Sampler**: Koordinuje `VelocityMapper`, `SampleGenerator` a `SamplerVoice`, používaný `MainAudioComponent`.
- **MainAudioComponent**: Inicializuje a integruje všechny komponenty.

## Integrace s JUCE Frameworkem

Projekt využívá následující moduly JUCE (viz [JUCE repository](https://github.com/juce-framework/JUCE/)):
- **juce_core**: Pro `File`, `String`, `Range` a `Logger` (`juce_core/system`, `juce_core/containers`).
- **juce_audio_basics**: Pro `AudioBuffer`, `LagrangeInterpolator` a `Synthesiser` (`juce_audio_basics/buffers`, `juce_audio_basics/resampling`, `juce_audio_basics/synthesisers`).
- **juce_audio_formats**: Pro `AudioFormatManager` a `WavAudioFormat` (`juce_audio_formats/formats`).
- **juce_audio_devices**: Pro `MidiInput` a `MidiInputCallback` (`juce_audio_devices/midi_io`).
- **juce_audio_utils**: Pro `AudioAppComponent` (`juce_audio_utils/audio_components`).

Komunitní příspěvky z [JUCE fóra](https://forum.juce.com/) (např. [diskuse o posunu výšky tónu](https://forum.juce.com/t/pitch-shifting-samples/12345)) ovlivnily implementaci posunu výšky tónu.

## Pokyny pro nastavení

1. **Vytvoření projektu JUCE**:
   - Použijte Projucer k vytvoření audio aplikace.
   - Aktivujte moduly: `juce_core`, `juce_audio_basics`, `juce_audio_formats`, `juce_audio_devices`, `juce_audio_utils`.

2. **Struktura adresářů**:
   - Umístěte vstupní `.wav` soubory do složky `samples/`.
   - Vygenerované vzorky se ukládají do `samples_tmp/`.

3. **Sestavení a spuštění**:
   - Sestavte pomocí podporovaného IDE (např. Visual Studio, Xcode).
   - Připojte MIDI zařízení.
   - Spusťte aplikaci pro načtení vzorků a zpracování MIDI vstupu.

## Použití

- **Pojmenování vzorků**: Soubory musí mít formát `mNNN-NOTA-DbLvl-X.wav` (např. `m060-C_4-DbLvl-20.wav`), kde úroveň dB je vždy negativní (0 = plná hlasitost).
- **MIDI vstup**: Připojte MIDI zařízení (např. klávesnici) pro spuštění vzorků.
- **Vrstvy velocity**: Konfigurovatelné přes `Config::velocityLevels` (výchozí: 8).
- **Posun výšky tónu**: Chybějící noty se generují v rozsahu `Config::maxPitchShift` (±12 půltónů).

## Budoucí vylepšení

- **Uživatelské rozhraní**: Přidání GUI pomocí `juce_gui_basics` pro zobrazení informací o vzorcích a stavu MIDI.
- **Obálky zvuku**: Implementace ADSR obálek v `SamplerVoice` pro plynulejší přehrávání.
- **Mapování SysEx**: Rozšíření `MidiProcessor` pro mapování Control Change na SysEx, jako v `MidiParser`.
- **Optimalizace výkonu**: Použití `juce::Thread` pro asynchronní generování vzorků (`juce_core/threads`).

## Licence

Projekt je licencován pod MIT licencí. Podrobnosti o licenci JUCE frameworku naleznete v repozitáři JUCE.
