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
    Logger::getInstance().log("PluginEditor/constructor", "info", "Vytvářeni komponenty editoru");
    
    // Inicializace log display (multiline, read-only, se scrollbar)
    logDisplay = std::make_unique<juce::TextEditor>();
    logDisplay->setMultiLine(true);
    logDisplay->setReadOnly(true);
    logDisplay->setScrollbarsShown(true);
    
    // Oprava deprecated Font konstruktoru
    juce::Font monoFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    logDisplay->setFont(monoFont);
    
    // Styling pro lepší čitelnost
    logDisplay->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1e1e1e)); // Tmavě šedé pozadí
    logDisplay->setColour(juce::TextEditor::textColourId, juce::Colour(0xff00ff00));        // Zelený text (matrix style)
    logDisplay->setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff404040));     // Tmavý okraj
    
    addAndMakeVisible(logDisplay.get());
    
    Logger::getInstance().log("PluginEditor/constructor", "info", "Log display inicializovan s matrix theme");

    // Inicializace toggle tlačítka
    toggleLogging = std::make_unique<juce::ToggleButton>("Zapnout/Vypnout logovani");
    toggleLogging->setToggleState(Logger::loggingEnabled, juce::dontSendNotification);
    toggleLogging->onClick = [this] {
        bool newState = toggleLogging->getToggleState();
        Logger::loggingEnabled = newState;
        Logger::getInstance().log("PluginEditor/toggleButton", "info", 
            "Logovani " + juce::String(newState ? "ZAPNUTO" : "VYPNUTO"));
        if (!Logger::loggingEnabled) {
            logDisplay->clear();  // Vyčištění display při vypnutí
        }
    };
    addAndMakeVisible(toggleLogging.get());
    
    Logger::getInstance().log("PluginEditor/constructor", "info", "Toggle button inicializovan");

    // Přidání tlačítka pro vyčištění logů
    clearLogsButton = std::make_unique<juce::TextButton>("Vycistit logy");
    clearLogsButton->onClick = [this] {
        logDisplay->clear();
        Logger::getInstance().log("PluginEditor/clearButton", "info", "=== LOGY VYCISTENY UZIVATELEM ===");
    };
    addAndMakeVisible(clearLogsButton.get());
    
    Logger::getInstance().log("PluginEditor/constructor", "info", "Clear button inicializovan");

    // Nastavení reference na tento editor v Loggeru
    Logger::getInstance().setEditor(this);
    Logger::getInstance().log("PluginEditor/constructor", "info", "Reference na editor nastavena v Loggeru");

    // Rozšířená velikost okna na 800x500
    setSize (1024, 600);
    Logger::getInstance().log("PluginEditor/constructor", "info", "Velikost okna nastavena: 800x500");
    Logger::getInstance().log("PluginEditor/constructor", "info", "=== GUI INICIALIZACE DOKONČENA ===");
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    // Logování před destrukcí
    Logger::getInstance().log("PluginEditor/destructor", "info", "=== UZAVIRANI GUI ===");
    Logger::getInstance().log("PluginEditor/destructor", "info", "Zahajeni destrukce editoru");
    
    // Odstranění reference při destrukci
    Logger::getInstance().setEditor(nullptr);
    Logger::getInstance().log("PluginEditor/destructor", "info", "Reference na editor odstranena");
    Logger::getInstance().log("PluginEditor/destructor", "info", "=== GUI UZAVRENO ===");
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Logování pouze při prvním vykreslení
    static bool firstPaint = true;
    if (firstPaint)
    {
        Logger::getInstance().log("PluginEditor/paint", "info", "=== PRVNI VYKRESLENI GUI ===");
        Logger::getInstance().log("PluginEditor/paint", "info", "Rozmery canvas: " + 
            juce::String(getWidth()) + "x" + juce::String(getHeight()));
        firstPaint = false;
    }
    
    // Gradient pozadí
    juce::ColourGradient gradient(juce::Colour(0xff2a2a2a), 0, 0,
                                  juce::Colour(0xff1a1a1a), 0, (float)getHeight(), false);
    g.setGradientFill(gradient);
    g.fillAll();

    // Horní sekce - nadpis a info
    g.setColour (juce::Colours::lightblue);
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawFittedText("IthacaPlayer Debug Interface", 10, 10, getWidth() - 20, 40, juce::Justification::centred, 1);
    
    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::FontOptions(12.0f));
    g.drawFittedText("Real-time logging a debugging audio pluginu", 10, 50, getWidth() - 20, 20, juce::Justification::centred, 1);
    
    // Oddělovací čára
    g.setColour(juce::Colour(0xff404040));
    g.fillRect(10, 80, getWidth() - 20, 1);
}

void AudioPluginAudioProcessorEditor::resized()
{
    // Logování změny velikosti
    Logger::getInstance().log("PluginEditor/resized", "debug", "Zmena velikosti GUI: " + 
        juce::String(getWidth()) + "x" + juce::String(getHeight()));
    
    // Layout - rozložení komponent
    int margin = 10;
    int buttonHeight = 30;
    int headerHeight = 90;  // Prostor pro nadpis
    
    // Log display zabírá většinu místa
    int logDisplayHeight = getHeight() - headerHeight - buttonHeight * 2 - margin * 4;
    logDisplay->setBounds(margin, headerHeight, getWidth() - 2 * margin, logDisplayHeight);

    // Tlačítka ve spodní části
    int buttonY = headerHeight + logDisplayHeight + margin;
    int buttonWidth = (getWidth() - 3 * margin) / 2;
    
    toggleLogging->setBounds(margin, buttonY, buttonWidth, buttonHeight);
    clearLogsButton->setBounds(margin * 2 + buttonWidth, buttonY, buttonWidth, buttonHeight);
    
    Logger::getInstance().log("PluginEditor/resized", "debug", "Layout komponent aktualizovan - log area: " + 
        juce::String(logDisplay->getWidth()) + "x" + juce::String(logDisplay->getHeight()));
}

/**
 * Aktualizace log display s auto-scroll na konec.
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

    // Auto-scroll na konec pro sledování nových událostí
    logDisplay->moveCaretToEnd();
    
    // Jednoduchý scroll na konec
    logDisplay->scrollEditorToPositionCaret(0, logDisplay->getHeight() - 20);
}