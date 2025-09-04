#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Logger.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);
    
    // Logování vytváření editoru
    Logger::getInstance().log("PluginEditor/constructor", "info", "=== INICIALIZACE GUI ===");
    Logger::getInstance().log("PluginEditor/constructor", "info", "Vytváření komponenty editoru");
    
    // Inicializace log display (multiline, read-only, se scrollbar)
    logDisplay = std::make_unique<juce::TextEditor>();
    logDisplay->setMultiLine(true);
    logDisplay->setReadOnly(true);
    logDisplay->setScrollbarsShown(true);
    juce::Font monoFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    logDisplay->setFont(monoFont);
    addAndMakeVisible(logDisplay.get());
    
    Logger::getInstance().log("PluginEditor/constructor", "info", "Log display inicializován");

    // Inicializace toggle tlačítka
    toggleLogging = std::make_unique<juce::ToggleButton>("Zapnout/Vypnout logování");
    toggleLogging->setToggleState(Logger::loggingEnabled, juce::dontSendNotification);
    toggleLogging->onClick = [this] {
        bool newState = toggleLogging->getToggleState();
        Logger::loggingEnabled = newState;
        Logger::getInstance().log("PluginEditor/toggleButton", "info", 
            "Logování " + juce::String(newState ? "ZAPNUTO" : "VYPNUTO"));
        if (!Logger::loggingEnabled) {
            logDisplay->clear();  // Vyčištění display při vypnutí
        }
    };
    addAndMakeVisible(toggleLogging.get());
    
    Logger::getInstance().log("PluginEditor/constructor", "info", "Toggle button inicializován");

    // Nastavení reference na tento editor v Loggeru
    Logger::getInstance().setEditor(this);
    Logger::getInstance().log("PluginEditor/constructor", "info", "Reference na editor nastavena v Loggeru");

    // Nastavení velikosti okna (zvětšeno pro logy)
    setSize (400, 500);
    Logger::getInstance().log("PluginEditor/constructor", "info", "Velikost okna nastavena: 400x500");
    Logger::getInstance().log("PluginEditor/constructor", "info", "=== GUI INICIALIZACE DOKONČENA ===");
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    // Logování před destrukcí
    Logger::getInstance().log("PluginEditor/destructor", "info", "=== UZAVÍRÁNÍ GUI ===");
    Logger::getInstance().log("PluginEditor/destructor", "info", "Zahájení destrukce editoru");
    
    // Odstranění reference při destrukci
    Logger::getInstance().setEditor(nullptr);
    Logger::getInstance().log("PluginEditor/destructor", "info", "Reference na editor odstraněna");
    Logger::getInstance().log("PluginEditor/destructor", "info", "=== GUI UZAVŘENO ===");
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Logování pouze při prvním vykreslení
    static bool firstPaint = true;
    if (firstPaint)
    {
        Logger::getInstance().log("PluginEditor/paint", "info", "=== PRVNÍ VYKRESLENÍ GUI ===");
        Logger::getInstance().log("PluginEditor/paint", "info", "Rozměry canvas: " + 
            juce::String(getWidth()) + "x" + juce::String(getHeight()));
        firstPaint = false;
    }
    
    // Vyplnění pozadí
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // Text v horní polovině
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("IthacaPlayer Debug Interface", 0, 0, getWidth(), getHeight() / 2, juce::Justification::centred, 1);
}

void AudioPluginAudioProcessorEditor::resized()
{
    // Logování změny velikosti
    Logger::getInstance().log("PluginEditor/resized", "debug", "Změna velikosti GUI: " + 
        juce::String(getWidth()) + "x" + juce::String(getHeight()));
    
    // Horní polovina pro stávající obsah
    // Dolní polovina pro log display (výška např. 200px)
    logDisplay->setBounds(0, getHeight() / 2, getWidth(), 200);

    // Tlačítko pod log display
    toggleLogging->setBounds(0, getHeight() / 2 + 200, getWidth(), 30);
    
    Logger::getInstance().log("PluginEditor/resized", "debug", "Layout komponent aktualizován");
}

/**
 * Aktualizace log display.
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