#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "=== APLIKACE SPUSTENA ===");
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Inicializace procesoru");
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Plugin nazev: " + getName());
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Je synthesizer: " + juce::String(JucePlugin_IsSynth ? "ANO" : "NE"));
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Prijima MIDI: " + juce::String(acceptsMidi() ? "ANO" : "NE"));
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Produkuje MIDI: " + juce::String(producesMidi() ? "ANO" : "NE"));
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    Logger::getInstance().log("AudioPluginAudioProcessor/destructor", "info", "=== APLIKACE SE UKONCUJE ===");
    Logger::getInstance().log("AudioPluginAudioProcessor/destructor", "info", "Zahajeni destrukce procesoru");
    Logger::getInstance().setEditor(nullptr);
    Logger::getInstance().log("AudioPluginAudioProcessor/destructor", "info", "=== DESTRUKCE DOKONCENA ===");
}

const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1;
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index)
{
    Logger::getInstance().log("AudioPluginAudioProcessor/setCurrentProgram", "info", "Zmena programu na index: " + juce::String(index));
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    Logger::getInstance().log("AudioPluginAudioProcessor/changeProgramName", "info", "Zmena nazvu programu [" + juce::String(index) + "]: " + newName);
    juce::ignoreUnused (index, newName);
}

void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "=== PRIPRAVA AUDIO PROCESINGU ===");
    Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "Sample rate: " + juce::String(sampleRate, 1) + " Hz");
    Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "Buffer size: " + juce::String(samplesPerBlock) + " samples");
    Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "Vstupni kanaly: " + juce::String(getTotalNumInputChannels()));
    Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "Vystupni kanaly: " + juce::String(getTotalNumOutputChannels()));
    
    // Vypocet latence
    double latencyMs = (double)samplesPerBlock / sampleRate * 1000.0;
    Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "Odhadovana latence: " + juce::String(latencyMs, 2) + " ms");
    
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void AudioPluginAudioProcessor::releaseResources()
{
    Logger::getInstance().log("AudioPluginAudioProcessor/releaseResources", "info", "=== UVOLNOVANI AUDIO ZDROJU ===");
    Logger::getInstance().log("AudioPluginAudioProcessor/releaseResources", "info", "Audio processing zastaven");
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    auto mainOutput = layouts.getMainOutputChannelSet();
    auto mainInput = layouts.getMainInputChannelSet();
    
    Logger::getInstance().log("AudioPluginAudioProcessor/isBusesLayoutSupported", "debug", 
        "Kontrola layoutu - Input: " + mainInput.getDescription() + 
        ", Output: " + mainOutput.getDescription());
    
    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
    {
        Logger::getInstance().log("AudioPluginAudioProcessor/isBusesLayoutSupported", "warn", 
            "Nepodporovany output layout: " + mainOutput.getDescription());
        return false;
    }

   #if ! JucePlugin_IsSynth
    if (mainOutput != mainInput)
    {
        Logger::getInstance().log("AudioPluginAudioProcessor/isBusesLayoutSupported", "warn", 
            "Input a output layout se neshoduji");
        return false;
    }
   #endif

    return true;
  #endif
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    // Pocitadlo pro omezeni logovani
    static int processCount = 0;
    static int totalMidiEvents = 0;
    processCount++;
    
    // Detailni logování prvnich bloku
    if (processCount <= 5)
    {
        Logger::getInstance().log("AudioPluginAudioProcessor/processBlock", "info", 
            "Audio blok #" + juce::String(processCount) + " - velikost: " + juce::String(buffer.getNumSamples()) + 
            " samples, kanaly: " + juce::String(buffer.getNumChannels()));
            
        // Analyza amplitudy pro prvni bloky
        if (buffer.getNumChannels() > 0)
        {
            float maxAmplitude = 0.0f;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                auto* channelData = buffer.getReadPointer(channel);
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    maxAmplitude = juce::jmax(maxAmplitude, std::abs(channelData[sample]));
                }
            }
            Logger::getInstance().log("AudioPluginAudioProcessor/processBlock", "info", 
                "Maximalni amplituda v bloku: " + juce::String(maxAmplitude, 6));
        }
    }
    else if (processCount % 1000 == 0)
    {
        Logger::getInstance().log("AudioPluginAudioProcessor/processBlock", "debug", 
            "Zpracovano " + juce::String(processCount) + " audio bloku, celkem MIDI: " + juce::String(totalMidiEvents));
    }
    
    // Detailni MIDI logování
    if (!midiMessages.isEmpty())
    {
        int midiEventsInBlock = 0;
        Logger::getInstance().log("AudioPluginAudioProcessor/processBlock", "info", "=== MIDI UDALOSTI ===");
        
        for (const auto& midiMetadata : midiMessages)
        {
            auto message = midiMetadata.getMessage();
            int sampleNumber = midiMetadata.samplePosition;
            midiEventsInBlock++;
            totalMidiEvents++;
            
            juce::String midiInfo = "MIDI #" + juce::String(totalMidiEvents) + 
                                   " @ sample " + juce::String(sampleNumber) + ": ";
            
            if (message.isNoteOn())
            {
                midiInfo += "NOTE ON - Note: " + juce::String(message.getNoteNumber()) + 
                           " (" + message.getMidiNoteName(message.getNoteNumber(), true, true, 4) + ")" +
                           ", Velocity: " + juce::String(message.getVelocity()) +
                           ", Channel: " + juce::String(message.getChannel());
            }
            else if (message.isNoteOff())
            {
                midiInfo += "NOTE OFF - Note: " + juce::String(message.getNoteNumber()) + 
                           " (" + message.getMidiNoteName(message.getNoteNumber(), true, true, 4) + ")" +
                           ", Velocity: " + juce::String(message.getVelocity()) +
                           ", Channel: " + juce::String(message.getChannel());
            }
            else if (message.isController())
            {
                midiInfo += "CC - Controller: " + juce::String(message.getControllerNumber()) +
                           ", Value: " + juce::String(message.getControllerValue()) +
                           ", Channel: " + juce::String(message.getChannel());
            }
            else if (message.isPitchWheel())
            {
                midiInfo += "PITCH BEND - Value: " + juce::String(message.getPitchWheelValue()) +
                           ", Channel: " + juce::String(message.getChannel());
            }
            else if (message.isProgramChange())
            {
                midiInfo += "PROGRAM CHANGE - Program: " + juce::String(message.getProgramChangeNumber()) +
                           ", Channel: " + juce::String(message.getChannel());
            }
            else if (message.isChannelPressure())
            {
                midiInfo += "CHANNEL PRESSURE - Pressure: " + juce::String(message.getChannelPressureValue()) +
                           ", Channel: " + juce::String(message.getChannel());
            }
            else if (message.isAftertouch())
            {
                midiInfo += "AFTERTOUCH - Note: " + juce::String(message.getNoteNumber()) +
                           ", Pressure: " + juce::String(message.getAfterTouchValue()) +
                           ", Channel: " + juce::String(message.getChannel());
            }
            else
            {
                midiInfo += "OTHER - " + message.getDescription();
            }
            
            Logger::getInstance().log("AudioPluginAudioProcessor/processBlock", "info", midiInfo);
        }
        
        Logger::getInstance().log("AudioPluginAudioProcessor/processBlock", "info", 
            "Celkem MIDI udalosti v bloku: " + juce::String(midiEventsInBlock));
    }

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Vyčištění přebytečných výstupních kanálů
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Zde by bylo audio processing - momentálně jen passthrough
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        juce::ignoreUnused (channelData);
        // TODO: Implementovat audio processing
    }
}

bool AudioPluginAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    Logger::getInstance().log("AudioPluginAudioProcessor/createEditor", "info", "=== VYTVARENI GUI EDITORU ===");
    Logger::getInstance().log("AudioPluginAudioProcessor/createEditor", "info", "Inicializace uzivatelského rozhrani");
    return new AudioPluginAudioProcessorEditor (*this);
}

void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    Logger::getInstance().log("AudioPluginAudioProcessor/getStateInformation", "info", "Ukladani stavu pluginu");
    juce::ignoreUnused (destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    Logger::getInstance().log("AudioPluginAudioProcessor/setStateInformation", "info", 
        "Nacitani stavu pluginu (velikost: " + juce::String(sizeInBytes) + " bytu)");
    juce::ignoreUnused (data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}