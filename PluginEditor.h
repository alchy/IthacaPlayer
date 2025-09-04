#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    // Metoda pro aktualizaci log display
    void updateLogDisplay();

private:
    // Reference na procesor
    AudioPluginAudioProcessor& processorRef;

    // Komponenty pro logování
    std::unique_ptr<juce::TextEditor> logDisplay;
    std::unique_ptr<juce::ToggleButton> toggleLogging;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};