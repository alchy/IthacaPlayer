#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);
    setSize (400, 300);
    ithacaEditor = std::make_unique<IthacaEditor>();  // Inicializace custom editoru (zdůvodnění: Připravenost před voláním metod).
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    ithacaEditor->paint(g);  // Delegování (zdůvodnění: Custom kreslení v Ithaca).
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));  // Zachování původního pro fallback.
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void AudioPluginAudioProcessorEditor::resized()
{
    ithacaEditor->resized();  // Delegování (zdůvodnění: Custom layout v Ithaca).
}