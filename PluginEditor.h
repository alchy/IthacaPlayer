#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

/**
 * @class AudioPluginAudioProcessorEditor
 * @brief Editor pluginu s jednoduchým GUI.
 * 
 * Obsahuje checkbox pro souborové logování a embedovaný obrázek.
 * Logování do GUI bylo odstraněno, MIDI indikátor vynechán pro jednoduchost.
 */
class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    AudioPluginAudioProcessor& audioProcessor;  // Reference na procesor

    juce::ToggleButton loggingToggle{"Create file for debugging"};  // Checkbox pro logování do souboru
    juce::Label logFilePathLabel;  // Label pro zobrazení relativní cesty k log souboru

    juce::ImageComponent imageComponent;  // Komponenta pro embedovaný obrázek

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};