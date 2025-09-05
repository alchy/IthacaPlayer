// PluginEditor.cpp - Opravená verze
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "decorators/BinaryData.h"

/**
 * @brief Konstruktor editoru.
 * Inicializuje komponenty GUI: checkbox pro debugging soubor, label cesty a embedovaný obrázek.
 * @param p Reference na audio procesor.
 */
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Embedovaný obrázek z BinaryData (načtení a nastavení)
    juce::Image image = juce::ImageCache::getFromMemory(BinaryData::ithacaplayer1_jpg, BinaryData::ithacaplayer1_jpgSize);
    imageComponent.setImage(image);
    imageComponent.setImagePlacement(juce::RectanglePlacement::stretchToFit);  // ✅ NOVÉ: Roztáhni na celou plochu
    addAndMakeVisible(imageComponent);

    // Checkbox pro vytváření debug souboru (řídí logování do souboru)
    addAndMakeVisible(loggingToggle);
    loggingToggle.setToggleState(true, juce::dontSendNotification);
    loggingToggle.onClick = [this] {
        Logger::loggingEnabled.store(loggingToggle.getToggleState());
    };

    // Label pro zobrazení relativní cesty k log souboru (cross-platform)
    addAndMakeVisible(logFilePathLabel);
    juce::File logDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("IthacaPlayer");
    juce::File logFile = logDir.getChildFile("IthacaPlayer.log");
    logFilePathLabel.setText(logFile.getParentDirectory().getFileName() + "/" + logFile.getFileName(), juce::dontSendNotification);
    logFilePathLabel.setColour(juce::Label::textColourId, juce::Colours::white);  // ✅ ZMĚNA: Bílý text pro lepší čitelnost
    logFilePathLabel.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.7f));  // ✅ NOVÉ: Polotransparentní pozadí

    // ✅ NOVÉ: Nastav průhlednost checkboxu pro lepší vzhled
    loggingToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);

    setSize(400, 600);  // ✅ ZMĚNA: Zvětšená výchozí velikost okna
}

/**
 * @brief Destruktor editoru.
 * Žádné speciální uvolnění zdrojů (komponenty se uvolní automaticky).
 */
AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    // Žádné speciální akce potřebné
}

/**
 * @brief Maluje pozadí GUI.
 * @param g Grafický kontext pro malování.
 */
void AudioPluginAudioProcessorEditor::paint(juce::Graphics& g)
{
    // ✅ ZMĚNA: Žádné pozadí - necháme obrázek jako pozadí
    // g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    
    // ✅ NOVÉ: Volitelně můžeme přidat tlumený overlay pro lepší čitelnost textu
    g.setColour(juce::Colours::black.withAlpha(0.1f));
    g.fillRect(getLocalBounds().removeFromTop(80));  // Jen horní část pro controls
}

/**
 * @brief Resized - nastavuje pozice komponent v GUI.
 */
void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    // ✅ KLÍČOVÁ ZMĚNA: Obrázek zabírá celou plochu
    imageComponent.setBounds(bounds);
    
    // ✅ ZMĚNA: Controls jsou overlay přes obrázek
    auto controlArea = bounds.removeFromTop(80).reduced(10);
    
    loggingToggle.setBounds(controlArea.removeFromTop(24));
    logFilePathLabel.setBounds(controlArea.removeFromTop(24));
}