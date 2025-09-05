#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "SampleLibrary.h"
#include "VoiceManager.h"
#include "MidiStateManager.h"
#include "Logger.h"

// Enum pro stavy syntetizéru
enum class SynthState {
    Uninitialized,  // Počáteční stav
    Initializing,   // Probíhá inicializace
    Ready,          // Připraveno k zpracování
    Error           // Kritická chyba
};

/**
 * @class AudioPluginAudioProcessor
 * @brief Hlavní audio procesor pluginu s optimalizovaným handlingem.
 * 
 * Spravuje inicializaci, audio/MIDI a zdroje. Refaktorováno pro nižší latenci a čitelnost.
 */
class AudioPluginAudioProcessor : public juce::AudioProcessor
{
public:
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    juce::String getStateDescription() const;
    bool isReadyForProcessing() const { 
        return synthState_.load() == SynthState::Ready && processingEnabled_.load(); 
    }
    double getCurrentSampleRate() const { return sampleRate_; }
    int getCurrentBufferSize() const { return samplesPerBlock_; }

private:
    Logger& logger_;
    SampleLibrary sampleLibrary_;
    VoiceManager voiceManager_;
    MidiStateManager midiState_;

    std::atomic<SynthState> synthState_{SynthState::Uninitialized};
    std::atomic<bool> processingEnabled_{false};
    
    double sampleRate_{0.0};
    int samplesPerBlock_{0};

    /**
     * @brief Inicializuje syntetizér s kontrolou paměti.
     * Dynamicky rozhoduje o plné nebo rychlé inicializaci.
     */
    void initializeSynth();

    /**
     * @brief Handling chyb.
     * @param errorMessage Zpráva o chybě.
     */
    void handleSynthError(const juce::String& errorMessage);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();