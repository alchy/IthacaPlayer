#pragma once

#include "PluginProcessor.h"
#include "IthacaSources/Editor/IthacaEditor.h"  // Přidáno: Include pro náš custom editor (zdůvodnění: Delegování GUI metod).

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AudioPluginAudioProcessor& processorRef;
    std::unique_ptr<IthacaEditor> ithacaEditor;  // Slouží k delegování custom GUI logiky Ithaca (zdůvodnění: Smart pointer pro správu paměti).

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};