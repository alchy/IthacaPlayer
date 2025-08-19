#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Logger/DebugLogger.h"  // Pro logování (relativní cesta v IthacaSources).
#include "../Common/Config.h"  // Pro sdílené konstanty.

//==============================================================================
/**
 * Custom processor pro Ithaca synth – skeleton s prázdnými metodami.
 * - Slouží k zpracování audio, MIDI, velocity mappingu a generování vzorků.
 * - Každá metoda loguje při volání, pokud debug=true.
 */
class IthacaProcessor
{
public:
    // Konstruktor: Inicializuje hlasy a logger.
    IthacaProcessor();

    // Destruktor: Uvolní resources.
    ~IthacaProcessor();

    // Metoda: Připraví na přehrávání (nastaví sample rate atd.).
    // - Bude inicializovat buffers a resources pro real-time audio.
    // @param sampleRate Sample rate.
    // @param samplesPerBlock Blok velikost.
    // @param debug Zapne logování (default false).
    void prepareToPlay(double sampleRate, int samplesPerBlock, bool debug = false);

    // Metoda: Uvolní resources.
    // - Bude čistit paměť po vzorcích a hlasích.
    // @param debug Zapne logování (default false).
    void releaseResources(bool debug = false);

    // Metoda: Inicializuje hlasy pro polyfonii.
    // - Bude vytvářet až 16 hlasů s voice stealing.
    // @param debug Zapne logování (default false).
    void initVoices(bool debug = false);

    // Metoda: Zpracovává audio blok s MIDI vstupem.
    // - Bude zpracovávat MIDI events, přiřazovat hlasy a mixovat audio.
    // @param buffer Audio buffer k zpracování.
    // @param midiMessages MIDI buffer.
    // @param debug Zapne logování (default false).
    void processBlockWithMidi(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, bool debug = false);

    // Metoda: Načítá audio vzorky ze složky.
    // - Bude parsovat soubory podle formátu mNNN-NOTA-DbLvl-X.wav.
    // @param inputDir Cesta ke složce se vzorky.
    // @param debug Zapne logování (default false).
    void loadSamples(const juce::String& inputDir, bool debug = false);

    // Metoda: Mapuje MIDI velocity na vzorky.
    // - Bude dynamicky vybírat vzorky podle dB úrovní a velocity.
    // @param midiNote MIDI nota.
    // @param velocity MIDI velocity (0-127).
    // @param debug Zapne logování (default false).
    void velocityMapping(int midiNote, int velocity, bool debug = false);

    // Metoda: Generuje chybějící noty pitch-shiftingem.
    // - Bude vytvářet vzorky pro chybějící noty (±12 půltónů).
    // @param tempDir Dočasná složka pro generované vzorky.
    // @param debug Zapne logování (default false).
    void generateMissingNotes(const juce::String& tempDir, bool debug = false);

private:
    struct Voice {};  // Placeholder pro hlas (bude obsahovat sample, gate atd., zdůvodnění: Skeleton pro polyfonii).
    Voice voices[Config::MAX_VOICES];  // Pole hlasů (až 16, slouží k polyfonnímu přehrávání).

    // Sub-funkce: Loguje volání metody.
    // Zdůvodnění: Odděleno pro přehlednost, voláno v každé metodě.
    void logMethodCall(const juce::String& methodName, bool debug);

    // Sub-funkce: Inicializuje interní struktury.
    // Zdůvodnění: Voláno v konstruktoru pro připravenost.
    void initialize();
};