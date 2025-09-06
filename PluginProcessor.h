#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <array>
#include <chrono>
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
 * @brief Hlavní audio procesor pluginu s optimalizovaným handlingem a triple buffering.
 * 
 * Spravuje inicializaci, audio/MIDI a zdroje. Refaktorováno pro nižší latenci pomocí
 * triple buffering a samostatného render threadu s lock-free komunikací.
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

    // Triple buffering pro nižší latenci
    static constexpr int BUFFER_COUNT = 3;
    std::array<std::vector<float>, BUFFER_COUNT> audioBuffers_;
    std::atomic<int> currentReadBuffer_{0};
    std::atomic<int> currentWriteBuffer_{0};
    std::atomic<bool> isBufferReady_{false};
    
    // Render thread a synchronizace
    std::thread renderThread_;
    std::atomic<bool> shouldStop_{false};
    std::condition_variable renderSignal_;
    std::mutex renderMutex_;
    
    // Lock-free MIDI komunikace
    struct MidiEvent {
        juce::MidiMessage message;
        int samplePosition;
    };
    
    static constexpr int MIDI_QUEUE_SIZE = 512;
    std::array<MidiEvent, MIDI_QUEUE_SIZE> midiQueue_;
    std::atomic<int> midiReadIndex_{0};
    std::atomic<int> midiWriteIndex_{0};
    std::mutex midiMutex_;

    /**
     * @brief Inicializuje syntetizér s kontrolou paměti.
     */
    void initializeSynth();
    void handleSynthError(const juce::String& errorMessage);
    void startRenderThread();
    void stopRenderThread();
    void renderThreadFunction();

    // Lock-free MIDI operace
    bool pushMidiEvent(const juce::MidiMessage& message, int samplePosition);
    bool popMidiEvent(MidiEvent& event);
    void clearMidiQueue();

    // Pomocné metody pro efektivnější zpracování
    void processMidiInRealTime(const juce::MidiBuffer& midiMessages);
    void copyAudioBuffer(juce::AudioBuffer<float>& dest, const std::vector<float>& src);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();