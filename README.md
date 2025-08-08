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

Struktura projektu IthacaPlayer
Přehled
IthacaPlayer je MIDI samplovací přehrávač v JUCE frameworku, kombinující správu .wav vzorků (s negativními dB úrovněmi) a robustní zpracování MIDI z Python skriptu sample-player.py a C++ kódu (Performer, Midi, DeviceVoice). Podporuje polyfonní přehrávání, mapování velocity, generování chybějících vzorků a zpracování MIDI událostí (Note On/Off, Pitch Wheel, Control Change, SysEx).
Struktura projektu

===

Třída Config (Config.h)

velocityLevels: static constexpr int
Vstup: Žádný
Výstup: Počet vrstev velocity (8)
Řeší: Definice počtu vrstev pro mapování velocity


midiVelocityMax: static constexpr int
Vstup: Žádný
Výstup: Maximální MIDI velocity (127)
Řeší: Stanovení rozsahu MIDI velocity


maxPitchShift: static constexpr int
Vstup: Žádný
Výstup: Maximální posun výšky (±12 půltónů)
Řeší: Omezení posunu výšky pro generování vzorků


midiNoteRange: static constexpr juce::Range<int>
Vstup: Žádný
Výstup: Rozsah MIDI not (21–108)
Řeší: Definice rozsahu not pro přehrávání a generování


tempDirName: static const juce::String
Vstup: Žádný
Výstup: Název dočasné složky (samples_tmp)
Řeší: Určení složky pro generované vzorky

===

Třída AudioFile (AudioFile.h, AudioFile.cpp)

Konstruktor: AudioFile(juce::File, int, juce::String, int)
Vstup: Soubor, MIDI nota, název noty, dB úroveň (negativní nebo 0)
Výstup: Instance AudioFile
Řeší: Inicializace objektu vzorku s metadaty


fromFile: static std::unique_ptr<AudioFile>(juce::File)
Vstup: juce::File
Výstup: Ukazatel na AudioFile nebo nullptr
Řeší: Parsování názvu souboru (např. m060-C_4-DbLvl-20.wav)

===

Třída VelocityMapper (VelocityMapper.h, VelocityMapper.cpp)

Konstruktor: VelocityMapper()
Vstup: Žádný
Výstup: Prázdná instance
Řeší: Inicializace datových struktur pro mapování


buildVelocityMap: void(juce::File)
Vstup: Vstupní složka se vzorky
Výstup: Žádný
Řeší: Skenování složky, parsování vzorků, vytvoření mapy (nota, velocity) -> soubor


getSampleForVelocity: juce::File(int, int)
Vstup: MIDI nota, velocity
Výstup: juce::File nebo prázdný soubor
Řeší: Vyhledání vzorku pro danou notu a velocity


addGeneratedSample: void(int, std::pair<int, int>, juce::File)
Vstup: MIDI nota, rozsah velocity, soubor
Výstup: Žádný
Řeší: Přidání vygenerovaného vzorku do mapy


getVelocityRanges: std::vector<std::pair<int, int>>(size_t)
Vstup: Počet dB úrovní
Výstup: Rozsahy velocity
Řeší: Rozdělení MIDI velocity (0–127) na vrstvy

===

Třída SampleGenerator (SampleGenerator.h, SampleGenerator.cpp)

Konstruktor: SampleGenerator(VelocityMapper&)
Vstup: Reference na VelocityMapper
Výstup: Instance
Řeší: Inicializace s přístupem k mapování vzorků


generateMissingNotes: void(juce::File)
Vstup: Dočasná složka
Výstup: Žádný
Řeší: Generování chybějících vzorků posunem výšky tónu


pitchShiftSample: void(juce::File, juce::File, int)
Vstup: Vstupní soubor, výstupní soubor, půltóny
Výstup: Žádný
Řeší: Posun výšky tónu a uložení nového vzorku


findNearestAvailableNote: int(int)
Vstup: Cílová MIDI nota
Výstup: Nejbližší dostupná nota nebo -1
Řeší: Vyhledání nejbližšího vzorku v rozsahu maxPitchShift

===

Třída MidiProcessor (MidiProcessor.h, MidiProcessor.cpp)

Konstruktor: MidiProcessor(juce::Synthesiser&)
Vstup: Reference na Sampler
Výstup: Instance
Řeší: Inicializace s přístupem k sampleru


handleIncomingMidiMessage: void(juce::MidiInput*, juce::MidiMessage&)
Vstup: MIDI zdroj, zpráva
Výstup: Žádný
Řeší: Zpracování Note On/Off, Pitch Wheel, CC, SysEx (inspirováno MidiParser)


start: void(juce::String)
Vstup: Název MIDI zařízení
Výstup: Žádný
Řeší: Otevření a spuštění MIDI vstupu


stop: void()
Vstup: Žádný
Výstup: Žádný
Řeší: Zastavení a uzavření MIDI vstupu

===

Třída SamplerVoice (SamplerVoice.h, SamplerVoice.cpp)

Konstruktor: SamplerVoice(VelocityMapper&)
Vstup: Reference na VelocityMapper
Výstup: Instance
Řeší: Inicializace hlasu pro přehrávání vzorku


canPlaySound: bool(juce::SynthesiserSound*)
Vstup: Ukazatel na zvuk
Výstup: Pravdivostní hodnota
Řeší: Ověření, zda zvuk je SamplerSound


startNote: void(int, float, juce::SynthesiserSound*, int)
Vstup: MIDI nota, velocity, zvuk, pozice pitch wheelu
Výstup: Žádný
Řeší: Načtení a příprava vzorku pro přehrávání (inspirováno DeviceVoice::play)


stopNote: void(float, bool)
Vstup: Velocity, povolení dozvuku
Výstup: Žádný
Řeší: Zastavení přehrávání vzorku


renderNextBlock: void(juce::AudioBuffer<float>&, int, int)
Vstup: Výstupní buffer, počáteční vzorek, počet vzorků
Výstup: Žádný
Řeší: Renderování vzorků do výstupního bufferu


pitchWheelMoved: void(int)
Vstup: Hodnota pitch wheelu
Výstup: Žádný
Řeší: Aplikace modulace pitch wheelu (volitelné)


controllerMoved: void(int, int)
Vstup: Číslo kontroléru, hodnota
Výstup: Žádný
Řeší: Zpracování Control Change (volitelné)

===

Třída Sampler (Sampler.h, Sampler.cpp)

Konstruktor: Sampler(juce::File, bool)
Vstup: Vstupní složka, příznak čištění dočasné složky
Výstup: Instance
Řeší: Inicializace mapy velocity, generování vzorků, přidání hlasů a zvuku


addSound: void(juce::SynthesiserSound*)
Vstup: Ukazatel na zvuk
Výstup: Žádný
Řeší: Přidání SamplerSound (zděděno z JUCE)


addVoice: void(juce::SynthesiserVoice*)
Vstup: Ukazatel na hlas
Výstup: Žádný
Řeší: Přidání SamplerVoice (zděděno z JUCE)


noteOn: void(int, int, float)
Vstup: MIDI kanál, nota, velocity
Výstup: Žádný
Řeší: Spuštění noty na volném hlasu (inspirováno Performer::play)


noteOff: void(int, int, float, bool)
Vstup: MIDI kanál, nota, velocity, povolení dozvuku
Výstup: Žádný
Řeší: Zastavení noty


handlePitchWheel: void(int, int)
Vstup: MIDI kanál, hodnota pitch wheelu
Výstup: Žádný
Řeší: Aplikace pitch wheelu na všechny hlasy

===

Třída SamplerSound (Sampler.h)

Konstruktor: SamplerSound(juce::String, int, int)
Vstup: Název, minimální a maximální nota
Výstup: Instance
Řeší: Definice rozsahu not pro sampler


appliesToNote: bool(int)
Vstup: MIDI nota
Výstup: Pravdivostní hodnota
Řeší: Ověření, zda nota patří do rozsahu


appliesToChannel: bool(int)
Vstup: MIDI kanál
Výstup: Pravdivostní hodnota
Řeší: Potvrzení applicability pro všechny kanály

===

Třída MainAudioComponent (MainAudioComponent.h, MainAudioComponent.cpp)

Konstruktor: MainAudioComponent(juce::File, bool)
Vstup: Vstupní složka, příznak čištění dočasné složky
Výstup: Instance
Řeší: Inicializace Sampler a MidiProcessor, nastavení zvukového výstupu


prepareToPlay: void(int, double)
Vstup: Počet vzorků na blok, vzorkovací frekvence
Výstup: Žádný
Řeší: Příprava Sampler na renderování


getNextAudioBlock: void(juce::AudioSourceChannelInfo&)
Vstup: Informace o výstupním bufferu
Výstup: Žádný
Řeší: Renderování zvuku přes Sampler


releaseResources: void()
Vstup: Žádný
Výstup: Žádný
Řeší: Uvolnění zdrojů (placeholder)


Destruktor: ~MainAudioComponent()
Vstup: Žádný
Výstup: Žádný
Řeší: Zastavení MIDI a zvuku

===

Poznámky k implementaci

JUCE moduly: juce_core, juce_audio_basics, juce_audio_formats, juce_audio_devices, juce_audio_utils.
Inspirace C++ kódem:
Performer.cpp: Alokace hlasů a přebírání (mixle_queue) pro Sampler.
DeviceVoice.cpp: Správa not a velocity pro SamplerVoice.
midi.cpp: Robustní parsování MIDI pro MidiProcessor.


Vzorky: Názvy ve formátu mNNN-NOTA-DbLvl-X.wav, kde dB je negativní (0 = plná hlasitost).
Fórum JUCE: Viz např. https://forum.juce.com/t/simple-sampler-plugin/23456 pro inspiraci.
