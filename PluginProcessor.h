#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "SampleLibrary.h"
#include "VoiceManager.h"
#include "MidiStateManager.h"
#include "Logger.h"

// Enum pro stavy syntetizéru s rozšířenou funkcionalitou
enum class SynthState {
    Uninitialized,  // Počáteční stav - nic není inicializováno
    Initializing,   // Probíhá inicializace
    Ready,          // Připraveno k audio zpracování
    Error           // Kritická chyba - vyžaduje restart
};

/**
 * @class AudioPluginAudioProcessor
 * @brief Hlavní audio procesor pluginu (JUCE-based) s robustním error handlingem.
 * 
 * Spravuje inicializaci, zpracování audio/MIDI, stav syntetizéru a error recovery.
 * Nově přidány atomic safety mechanismy a detailní logging pro debugging.
 * 
 * Thread Safety:
 * - synthState_ a processingEnabled_ jsou atomic pro bezpečný přístup z více vláken
 * - Všechny kritické operace jsou chráněny try-catch bloky
 * - Error handling s automatic recovery kde je to možné
 * 
 * Memory Management:
 * - Automatické cleanup v destruktoru
 * - Safe resource deallocation při chybách
 * - Kontrola validity pointerů před použitím
 */
class AudioPluginAudioProcessor : public juce::AudioProcessor
{
public:
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    // === JUCE Audio Processor Interface ===
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // === Editor Management ===
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // === Plugin Metadata ===
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // === Program Management (Basic Implementation) ===
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // === State Persistence (Placeholder) ===
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    // === Public Status Methods ===
    /**
     * @brief Vrátí lidsky čitelný popis aktuálního stavu.
     * @return String s popisem stavu včetně processing status
     */
    juce::String getStateDescription() const;

    /**
     * @brief Kontroluje, zda je procesor připraven k audio zpracování.
     * @return true pokud je ready a processing enabled
     */
    bool isReadyForProcessing() const { 
        return synthState_.load() == SynthState::Ready && processingEnabled_.load(); 
    }

    /**
     * @brief Vrátí aktuální sample rate.
     * @return Sample rate nebo 0 pokud není nastaven
     */
    double getCurrentSampleRate() const { return sampleRate_; }

    /**
     * @brief Vrátí velikost audio bufferu.
     * @return Velikost bufferu nebo 0 pokud není nastaven
     */
    int getCurrentBufferSize() const { return samplesPerBlock_; }

private:
    // === Core Components ===
    Logger& logger_;  // Reference na singleton logger
    SampleLibrary sampleLibrary_;  // Knihovna audio vzorků
    VoiceManager voiceManager_;  // Manager polyphonic hlasů
    MidiStateManager midiState_;  // Manager MIDI stavu a událostí

    // === State Management (Thread-Safe) ===
    std::atomic<SynthState> synthState_{SynthState::Uninitialized};  // Stav syntetizéru
    std::atomic<bool> processingEnabled_{false};  // Povolení audio zpracování
    
    // === Audio Configuration ===
    double sampleRate_{44100.0};  // Aktuální sample rate
    int samplesPerBlock_{512};     // Velikost audio bufferu

    // === Private Methods ===

    /**
     * @brief Inicializuje syntetizér (vzorky, voices atd.) s error handlingem.
     * Volá se z prepareToPlay po validaci parametrů.
     * 
     * Process:
     * 1. Kontrola přechodů stavů
     * 2. Inicializace SampleLibrary
     * 3. Validace vygenerovaných vzorků
     * 4. Nastavení Ready stavu
     * 
     * @throws std::runtime_error při kritických chybách
     */
    void initializeSynth();

    /**
     * @brief Rychlá reinicializace syntetizéru bez regenerování samples
     * Volá se po releaseResources() se stejným sample rate pro optimalizaci.
     * 
     * Process:
     * 1. Rychlé načtení existujících samples z disku
     * 2. Validace dostupnosti vzorků
     * 3. Nastavení Ready stavu
     * 
     * @throws std::runtime_error při kritických chybách
     */
    void initializeSynthFast();

    /**
     * @brief Centrální metoda pro handling chyb s automatickým recovery.
     * 
     * Akce při chybě:
     * - Logování s detailním popisem
     * - Zastavení audio zpracování
     * - Možné future recovery mechanismy
     * 
     * @param errorMessage Zpráva o chybě pro logging
     */
    void handleSynthError(const juce::String& errorMessage);

    // === JUCE Macro for Memory Leak Detection ===
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};

// === Global Plugin Factory Function ===
/**
 * @brief Factory funkce pro vytváření plugin instance.
 * Vyžadována JUCE frameworkem pro VST3, AU a další formáty.
 * 
 * @return Novou instanci AudioPluginAudioProcessor nebo nullptr při chybě
 */
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();