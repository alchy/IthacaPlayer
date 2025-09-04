#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Logger.h"
#include "SampleLibrary.h"
#include "MidiStateManager.h"
#include "VoiceManager.h"

//==============================================================================
/**
 * AudioPluginAudioProcessor - hlavní třída audio pluginu
 * OPRAVA: Přidání explicitních error states pro lepší debugging
 */
class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    // OPRAVA: Enum pro explicitní stavy synth komponent
    enum class SynthState { 
        Uninitialized, 
        Initializing, 
        Ready, 
        Error 
    };

    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // OPRAVA: Getter pro aktuální stav (pro debugging)
    SynthState getCurrentState() const { return synthState_.load(); }
    juce::String getStateDescription() const;

private:
    //==============================================================================
    // OPRAVA: Exception-safe metody pro inicializaci
    bool initializeSynth(double sampleRate);
    void cleanupSynth();
    
    // OPRAVA: Error recovery metoda
    void handleSynthError(const std::string& errorMessage);

    // Synth komponenty - vytvářejí se až při prepareToPlay
    std::unique_ptr<SampleLibrary> sampleLibrary_;
    std::unique_ptr<MidiStateManager> midiStateManager_;
    std::unique_ptr<VoiceManager> voiceManager_;
    
    // OPRAVA: Atomic state management pro thread safety
    std::atomic<SynthState> synthState_{SynthState::Uninitialized};
    double currentSampleRate_;
    
    // Debug counters pro optimalizaci logování
    int processBlockCount_;
    int totalMidiEvents_;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};