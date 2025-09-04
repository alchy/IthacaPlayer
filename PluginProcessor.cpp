#include "PluginProcessor.h"
#include "PluginEditor.h"

// Podmíněné logování pro tento soubor (defaultně vypnuto kvůli frekvenci volání)
#define LOGGING_ENABLED false

// Pro debug na konzoli (stdout)
#include <iostream>
#ifdef _WIN32
#include <windows.h>  // Pro AllocConsole na Windows
#include <cstdio>     // Pro freopen_s
#endif

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), consoleAllocated(false)
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Inicializace procesoru");
    #endif

    // Debug výstup pro Standalone režim - otevření konzole a výpis (bezpečnější verze pro Windows)
    #ifdef _WIN32
    if (AllocConsole()) {
        consoleAllocated = true;
        FILE* stream = nullptr;
        errno_t err = freopen_s(&stream, "CONOUT$", "w", stdout);
        if (err == 0) {
            std::cout << "Standalone režim spuštěn - plugin inicializován." << std::endl;
        }
    }
    #else
    std::cout << "Standalone režim spuštěn - plugin inicializován." << std::endl;
    #endif
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/destructor", "info", "Destrukce procesoru");
    #endif

    // DŮLEŽITÉ: Nejprve odstraníme referenci na editor v Loggeru
    Logger::getInstance().setEditor(nullptr);

    // Výpis o ukončení programu (bez blokujícího čekání)
    std::cout << "Program ukončen korektně (destruktor procesoru volán)." << std::endl;

    // ODEBRANÉ: Blokující čekání na input - to způsobovalo pády!
    // std::cout << "Stiskněte Enter pro ukončení..." << std::endl;
    // std::cin.get();

    // Bezpečné uvolnění konzole na Windows
    #ifdef _WIN32
    if (consoleAllocated) {
        fclose(stdout);
        FreeConsole();
    }
    #endif
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/getName", "debug", "Volání getName");
    #endif
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/acceptsMidi", "debug", "Přijímá MIDI: true");
    #endif
    return true;
   #else
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/acceptsMidi", "debug", "Přijímá MIDI: false");
    #endif
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/producesMidi", "debug", "Produkuje MIDI: true");
    #endif
    return true;
   #else
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/producesMidi", "debug", "Produkuje MIDI: false");
    #endif
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/isMidiEffect", "debug", "Je MIDI efekt: true");
    #endif
    return true;
   #else
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/isMidiEffect", "debug", "Je MIDI efekt: false");
    #endif
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/getTailLengthSeconds", "debug", "Délka tail: 0.0");
    #endif
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/getNumPrograms", "debug", "Počet programů: 1");
    #endif
    return 1;
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/getCurrentProgram", "debug", "Aktuální program: 0");
    #endif
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index)
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/setCurrentProgram", "info", "Nastavení programu: " + juce::String(index));
    #endif
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/getProgramName", "debug", "Název programu pro index: " + juce::String(index));
    #endif
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/changeProgramName", "info", "Změna názvu programu: " + newName);
    #endif
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "Příprava na přehrávání: sampleRate=" + juce::String(sampleRate) + ", samplesPerBlock=" + juce::String(samplesPerBlock));
    #endif
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void AudioPluginAudioProcessor::releaseResources()
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/releaseResources", "info", "Uvolňování zdrojů");
    #endif
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/isBusesLayoutSupported", "debug", "Podpora layoutu pro MIDI efekt: true");
    #endif
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    {
        #if LOGGING_ENABLED
        Logger::getInstance().log("AudioPluginAudioProcessor/isBusesLayoutSupported", "warn", "Nepodporovaný layout");
        #endif
        return false;
    }

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    {
        #if LOGGING_ENABLED
        Logger::getInstance().log("AudioPluginAudioProcessor/isBusesLayoutSupported", "warn", "Input a output layout se neshodují");
        #endif
        return false;
    }
   #endif

    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/isBusesLayoutSupported", "debug", "Podpora layoutu: true");
    #endif
    return true;
  #endif
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/processBlock", "debug", "Zpracování bloku: samples=" + juce::String(buffer.getNumSamples()));
    #endif
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        juce::ignoreUnused (channelData);
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/hasEditor", "debug", "Má editor: true");
    #endif
    return true;
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/createEditor", "info", "Vytváření editoru");
    #endif
    return new AudioPluginAudioProcessorEditor (*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/getStateInformation", "info", "Získávání stavu");
    #endif
    juce::ignoreUnused (destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    #if LOGGING_ENABLED
    Logger::getInstance().log("AudioPluginAudioProcessor/setStateInformation", "info", "Nastavování stavu: velikost=" + juce::String(sizeInBytes));
    #endif
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}