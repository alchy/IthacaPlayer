#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Logger.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);
    
    // Inicializace log display (multiline, read-only, se scrollbar)
    logDisplay = std::make_unique<juce::TextEditor>();
    logDisplay->setMultiLine(true);
    logDisplay->setReadOnly(true);
    logDisplay->setScrollbarsShown(true);
    addAndMakeVisible(logDisplay.get());

    // Inicializace toggle tlačítka
    toggleLogging = std::make_unique<juce::ToggleButton>("Zapnout/Vypnout logování");
    toggleLogging->setToggleState(Logger::loggingEnabled, juce::dontSendNotification);
    toggleLogging->onClick = [this] {
        Logger::loggingEnabled = toggleLogging->getToggleState();
        if (!Logger::loggingEnabled) {
            logDisplay->clear();  // Vyčištění display při vypnutí
        }
    };
    addAndMakeVisible(toggleLogging.get());

    // Nastavení reference na tento editor v Loggeru
    Logger::getInstance().setEditor(this);

    // Nastavení velikosti okna (zvětšeno pro logy)
    setSize (400, 500);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    // Odstranění reference při destrukci
    Logger::getInstance().setEditor(nullptr);
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Vyplnění pozadí
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // Text v horní polovině
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Hello World!", 0, 0, getWidth(), getHeight() / 2, juce::Justification::centred, 1);
}

void AudioPluginAudioProcessorEditor::resized()
{
    // Horní polovina pro stávající obsah
    // Dolní polovina pro log display (výška např. 200px)
    logDisplay->setBounds(0, getHeight() / 2, getWidth(), 200);

    // Tlačítko pod log display
    toggleLogging->setBounds(0, getHeight() / 2 + 200, getWidth(), 30);
}

/**
 * Aktualizace log display.
 * 
 * Převede buffer z Loggeru na text a nastaví ho do textového okna,
 * poté scrollne na konec.
 */
void AudioPluginAudioProcessorEditor::updateLogDisplay()
{
    // Získání bufferu z Loggeru přes getter
    const juce::StringArray& buffer = Logger::getInstance().getLogBuffer();

    // Sestavení textu
    juce::String logText;
    for (const auto& entry : buffer)
    {
        logText += entry + "\n";
    }

    // Nastavení textu
    logDisplay->setText(logText);

    // Scroll na konec
    logDisplay->moveCaretToEnd();
    logDisplay->scrollEditorToPositionCaret(0, getHeight() - 20);
}