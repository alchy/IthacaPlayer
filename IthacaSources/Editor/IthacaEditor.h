#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Logger/DebugLogger.h"  // Pro logování.

//==============================================================================
/**
 * Custom editor pro Ithaca synth – skeleton s prázdnými GUI metodami.
 * - Slouží k kreslení UI prvků (např. velocity mapy).
 * - Každá metoda loguje při volání, pokud debug=true.
 */
class IthacaEditor
{
public:
    // Konstruktor: Inicializuje GUI komponenty.
    IthacaEditor();

    // Destruktor: Uvolní resources.
    ~IthacaEditor();

    // Metoda: Kreslí velocity mapu v UI.
    // - Bude vizualizovat velocity vrstvy.
    // @param g Graphics kontext.
    // @param debug Zapne logování (default false).
    void drawVelocityMap(juce::Graphics& g, bool debug = false);

    // Metoda: Aktualizuje UI z processoru.
    // - Bude synchronizovat data (např. aktuální vzorky).
    // @param debug Zapne logování (default false).
    void updateUIFromProcessor(bool debug = false);

    // Metoda: Zpracovává resize okna.
    // - Bude layoutovat subkomponenty.
    // @param debug Zapne logování (default false).
    void resized(bool debug = false);

    // Metoda: Kreslí hlavní UI.
    // - Bude integrovat custom prvky jako Hello World s Ithaca tématem.
    // @param g Graphics kontext.
    // @param debug Zapne logování (default false).
    void paint(juce::Graphics& g, bool debug = false);

private:
    // Sub-funkce: Loguje volání metody.
    // Zdůvodnění: Odděleno pro přehlednost.
    void logMethodCall(const juce::String& methodName, bool debug);

    // Sub-funkce: Inicializuje interní UI struktury.
    // Zdůvodnění: Voláno v konstruktoru.
    void initialize();
};