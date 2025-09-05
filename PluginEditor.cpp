#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "decorators/BinaryData.h"  // Pro embedovaný obrázek (generováno Projucerem)

/**
 * @brief Konstruktor editoru.
 * Inicializuje komponenty GUI: checkbox pro debugging soubor, label cesty a embedovaný obrázek.
 * @param p Reference na audio procesor.
 */
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Checkbox pro vytváření debug souboru (řídí logování do souboru)
    addAndMakeVisible(loggingToggle);
    loggingToggle.setToggleState(true, juce::dontSendNotification);  // Výchozí stav: zapnuto
    loggingToggle.onClick = [this] {
        Logger::loggingEnabled.store(loggingToggle.getToggleState());  // Řídí zapnutí/vypnutí logování do souboru
    };

    // Label pro zobrazení relativní cesty k log souboru (cross-platform)
    addAndMakeVisible(logFilePathLabel);
    juce::File logDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("IthacaPlayer");
    juce::File logFile = logDir.getChildFile("IthacaPlayer.log");
    // Oprava cesty: Použijeme getParentDirectory() pro relativní cestu
    logFilePathLabel.setText(logFile.getParentDirectory().getFileName() + "/" + logFile.getFileName(), juce::dontSendNotification);
    logFilePathLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    // Embedovaný obrázek z BinaryData (načtení a nastavení)
    juce::Image image = juce::ImageCache::getFromMemory(BinaryData::ithacaplayer1_jpg, BinaryData::ithacaplayer1_jpgSize);
    imageComponent.setImage(image);
    addAndMakeVisible(imageComponent);

    setSize(259, 443);  // Výchozí velikost okna editoru
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
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawFittedText("IthacaPlayer Editor", getLocalBounds(), juce::Justification::centredTop, 1);
}

/**
 * @brief Resized - nastaví pozice komponent v GUI.
 */
void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    loggingToggle.setBounds(10, 10, 200, 24);  // Pozice checkboxu nahoře
    logFilePathLabel.setBounds(10, 40, 300, 24);  // Pozice labelu pod checkboxem
    
    imageComponent.setBounds(10, 70, 380, 200);  // Obrázek dole (upravte velikost podle obrázku)
}