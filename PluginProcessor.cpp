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
                       ), synthInitialized_(false)
{
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "=== ITHACA PLAYER SPUSTEN ===");
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Inicializace procesoru");
    
    // Základní info o pluginu (podobné vašemu printf výpisu v main)
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Plugin nazev: " + getName());
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Je synthesizer: " + juce::String(JucePlugin_IsSynth ? "ANO" : "NE"));
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Prijima MIDI: " + juce::String(acceptsMidi() ? "ANO" : "NE"));
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Produkuje MIDI: " + juce::String(producesMidi() ? "ANO" : "NE"));
    
    // Inicializace synth komponent (podobné vašemu main() - vytvoření objektů)
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "=== VYTVARENI SYNTH KOMPONENT ===");
    
    // Zatím používáme dummy sample rate - bude aktualizován v prepareToPlay (podobné vaší setupMIDI logice)
    sampleLibrary_ = std::make_unique<SampleLibrary>(44100.0);
    midiStateManager_ = std::make_unique<MidiStateManager>();
    voiceManager_ = std::make_unique<VoiceManager>(16); // 16 hlasů jako ve vaší voice_[16]
    
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Sample Library vytvoren");
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "MIDI State Manager vytvoren");
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Voice Manager vytvoren s " + juce::String(voiceManager_->getVoiceCount()) + " hlasy");
    
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "=== KONSTRUKTOR DOKONCEN ===");
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    Logger::getInstance().log("AudioPluginAudioProcessor/destructor", "info", "=== ITHACA PLAYER SE UKONCUJE ===");
    Logger::getInstance().log("AudioPluginAudioProcessor/destructor", "info", "Zahajeni destrukce procesoru");
    
    // Nejprve odstraníme referenci na editor v Loggeru (bezpečnost)
    Logger::getInstance().setEditor(nullptr);
    Logger::getInstance().log("AudioPluginAudioProcessor/destructor", "info", "Logger reference odstranena");
    
    // Uvolnění synth komponent (v opačném pořadí než byly vytvořeny)
    voiceManager_.reset();
    Logger::getInstance().log("AudioPluginAudioProcessor/destructor", "info", "Voice Manager uvolnen");
    
    midiStateManager_.reset();
    Logger::getInstance().log("AudioPluginAudioProcessor/destructor", "info", "MIDI State Manager uvolnen");
    
    sampleLibrary_.reset();
    Logger::getInstance().log("AudioPluginAudioProcessor/destructor", "info", "Sample Library uvolnena");
    
    Logger::getInstance().log("AudioPluginAudioProcessor/destructor", "info", "=== DESTRUKCE DOKONCENA ===");
}

//==============================================================================
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

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "=== PRIPRAVA AUDIO PROCESINGU ===");
    Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "Sample rate: " + juce::String(sampleRate, 1) + " Hz");
    Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "Buffer size: " + juce::String(samplesPerBlock) + " samples");
    Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "Vstupni kanaly: " + juce::String(getTotalNumInputChannels()));
    Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "Vystupni kanaly: " + juce::String(getTotalNumOutputChannels()));
    
    // Výpočet latence
    double latencyMs = (double)samplesPerBlock / sampleRate * 1000.0;
    Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "Odhadovana latence: " + juce::String(latencyMs, 2) + " ms");
    
    // Inicializace synth pouze jednou nebo při změně sample rate (podobné vaší setupMIDI + lfo_ticker.attach logice)
    if (!synthInitialized_ || sampleLibrary_->getSampleRate() != sampleRate) {
        Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "=== INICIALIZACE SAMPLE LIBRARY ===");
        
        // Reinicializace s korektní sample rate (podobné vašemu scan_bus workflow)
        sampleLibrary_ = std::make_unique<SampleLibrary>(sampleRate);
        sampleLibrary_->initializeLibrary();
        
        // Generování prototypu pro střední C (podobné vašemu build_dumb_voices -> assign_dcos_to_voices workflow)
        float middleCFreq = 261.63f; // C4 frequency
        if (sampleLibrary_->generateSineWaveForNote(SAMPLE_NOTE_FOR_PROTOTYPE, middleCFreq)) {
            Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", 
                "Sample pro MIDI notu " + juce::String(SAMPLE_NOTE_FOR_PROTOTYPE) + " (Middle C) vygenerovan");
            Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", 
                "Frekvence: " + juce::String(middleCFreq, 2) + " Hz");
        } else {
            Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "warn", 
                "CHYBA: Nepodal se vygenerovat sample pro MIDI notu " + juce::String(SAMPLE_NOTE_FOR_PROTOTYPE));
        }
        
        synthInitialized_ = true;
        Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "=== SYNTH READY TO PLAY ===");
    }
    
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void AudioPluginAudioProcessor::releaseResources()
{
    Logger::getInstance().log("AudioPluginAudioProcessor/releaseResources", "info", "=== UVOLNOVANI AUDIO ZDROJU ===");
    Logger::getInstance().log("AudioPluginAudioProcessor/releaseResources", "info", "Audio processing zastaven");
    
    // Reset synth state při ukončení audio processingu
    synthInitialized_ = false;
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
    // Počítadlo pro omezení logování (podobné vašemu printf omezení)
    static int processCount = 0;
    processCount++;
    
    // Logování prvních několika bloků pro debugging
    if (processCount <= 5) {
        Logger::getInstance().log("AudioPluginAudioProcessor/processBlock", "info", 
            "Audio blok #" + juce::String(processCount) + " - samples: " + juce::String(buffer.getNumSamples()) + 
            ", kanaly: " + juce::String(buffer.getNumChannels()));
    } else if (processCount % 1000 == 0) {
        Logger::getInstance().log("AudioPluginAudioProcessor/processBlock", "debug", 
            "Zpracovano " + juce::String(processCount) + " audio bloku");
    }
    
    // Pokud synth není inicializován, vygeneruj tichý výstup
    if (!synthInitialized_) {
        buffer.clear();
        return;
    }
    
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Vyčištění přebytečných výstupních kanálů
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // === HLAVNÍ SYNTH PROCESSING LOOP - inspirováno vaší main while(1) loop ===
    
    // 1. MIDI zpracování (podobné vašemu midiParser.Parse())
    if (!midiMessages.isEmpty()) {
        if (processCount <= 10) { // Log pouze prvních 10 bloků s MIDI
            Logger::getInstance().log("AudioPluginAudioProcessor/processBlock", "info", "=== ZPRACOVANI MIDI ZPRAV ===");
        }
        midiStateManager_->processMidiBuffer(midiMessages);
    }
    
    // 2. Voice management - zpracování MIDI událostí (podobné vašemu pop_map_note_on/off pattern)
    voiceManager_->processMidiEvents(*midiStateManager_);
    
    // 3. Audio generování (podobné vašemu performer.refresh())
    for (int channel = 0; channel < totalNumOutputChannels; ++channel) {
        auto* channelData = buffer.getWritePointer(channel);
        
        // Pro prototyp - generuj pouze do prvního kanálu, kopíruj do ostatních
        if (channel == 0) {
            // Vyčisti buffer
            buffer.clear(channel, 0, buffer.getNumSamples());
            
            // Generuj audio ze všech hlasů
            voiceManager_->generateAudio(channelData, buffer.getNumSamples(), *sampleLibrary_);
        } else {
            // Kopíruj z prvního kanálu (mono -> stereo pro prototyp)
            auto* sourceData = buffer.getReadPointer(0);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
                channelData[sample] = sourceData[sample];
            }
        }
    }
    
    // 4. Refresh cycle (podobné vašemu performer.refresh())
    voiceManager_->refresh();
    
    // Periodické logování aktivních hlasů pro debugging
    if (processCount % 5000 == 0) {
        midiStateManager_->logActiveNotes();
    }
}

//==============================================================================
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

//==============================================================================
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

//==============================================================================
// POVINNÁ factory funkce pro JUCE pluginy
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}