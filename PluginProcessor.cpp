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
                       ),
       synthState_(SynthState::Uninitialized),
       currentSampleRate_(0.0),
       processBlockCount_(0),
       totalMidiEvents_(0)
{
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "=== APLIKACE SPUSTENA ===");
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Inicializace procesoru");
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Plugin nazev: " + getName());
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Je synthesizer: " + juce::String(JucePlugin_IsSynth ? "ANO" : "NE"));
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Prijima MIDI: " + juce::String(acceptsMidi() ? "ANO" : "NE"));
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", "Produkuje MIDI: " + juce::String(producesMidi() ? "ANO" : "NE"));
    
    // Komponenty se vytvoří až v prepareToPlay kdy známe sample rate
    Logger::getInstance().log("AudioPluginAudioProcessor/constructor", "info", 
                              "Stav: " + getStateDescription());
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    Logger::getInstance().log("AudioPluginAudioProcessor/destructor", "info", "=== APLIKACE SE UKONCUJE ===");
    Logger::getInstance().log("AudioPluginAudioProcessor/destructor", "info", "Zahajeni destrukce procesoru");
    
    cleanupSynth();
    Logger::getInstance().setEditor(nullptr);
    
    Logger::getInstance().log("AudioPluginAudioProcessor/destructor", "info", "=== DESTRUKCE DOKONCENA ===");
}

//==============================================================================
// Inicializuje synth komponenty s danou sample rate.
// Vrátí true při úspěchu, jinak false.
bool AudioPluginAudioProcessor::initializeSynth(double sampleRate)
{
    Logger::getInstance().log("AudioPluginAudioProcessor/initializeSynth", "info", "=== INICIALIZACE SYNTH KOMPONENT ===");
    
    // Nastavení stavu na Initializing
    synthState_.store(SynthState::Initializing);
    
    try {
        // Vytvoření komponent v správném pořadí
        sampleLibrary_ = std::make_unique<SampleLibrary>();
        sampleLibrary_->initialize(sampleRate);
        Logger::getInstance().log("AudioPluginAudioProcessor/initializeSynth", "info", "SampleLibrary inicializovana");
        
        midiStateManager_ = std::make_unique<MidiStateManager>();
        Logger::getInstance().log("AudioPluginAudioProcessor/initializeSynth", "info", "MidiStateManager inicializovan");
        
        // VoiceManager nyní vyžaduje referenci na SampleLibrary při konstrukci
        voiceManager_ = std::make_unique<VoiceManager>(*sampleLibrary_, 16);
        Logger::getInstance().log("AudioPluginAudioProcessor/initializeSynth", "info", "VoiceManager inicializovan (16 hlasu)");
        
        currentSampleRate_ = sampleRate;
        
        // Atomické nastavení stavu na Ready
        synthState_.store(SynthState::Ready);
        
        Logger::getInstance().log("AudioPluginAudioProcessor/initializeSynth", "info", 
                                  "=== SYNTH INICIALIZACE DOKONCENA === Stav: " + getStateDescription());
        return true;
        
    } catch (const std::exception& e) {
        handleSynthError("Chyba pri inicializaci: " + std::string(e.what()));
        return false;
    } catch (...) {
        handleSynthError("Neznama chyba pri inicializaci synth komponent");
        return false;
    }
}

// Uvolní synth komponenty v opačném pořadí než byly vytvořeny.
void AudioPluginAudioProcessor::cleanupSynth()
{
    Logger::getInstance().log("AudioPluginAudioProcessor/cleanupSynth", "info", "Uvolnovani synth komponent");
    
    // Uvolnění v opačném pořadí než vytvoření
    voiceManager_.reset();
    midiStateManager_.reset();
    sampleLibrary_.reset();
    
    synthState_.store(SynthState::Uninitialized);
    Logger::getInstance().log("AudioPluginAudioProcessor/cleanupSynth", "info", "Synth komponenty uvolneny");
}

// Zpracuje chybu při inicializaci synth komponent.
void AudioPluginAudioProcessor::handleSynthError(const std::string& errorMessage)
{
    Logger::getInstance().log("AudioPluginAudioProcessor/handleSynthError", "error", 
                              "SYNTH ERROR: " + juce::String(errorMessage));
    
    // Cleanup při chybě
    cleanupSynth();
    synthState_.store(SynthState::Error);
    
    Logger::getInstance().log("AudioPluginAudioProcessor/handleSynthError", "error", 
                              "Stav nastaven na ERROR, komponenty uvolneny");
}

// Vrátí textový popis aktuálního stavu synth.
juce::String AudioPluginAudioProcessor::getStateDescription() const
{
    switch (synthState_.load()) {
        case SynthState::Uninitialized: return "Neinicializovano";
        case SynthState::Initializing: return "Inicializuje se";
        case SynthState::Ready: return "Pripraveno";
        case SynthState::Error: return "Chyba";
        default: return "Neznamy stav";
    }
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
    
    // OPRAVA: Exception-safe inicializace nebo reinicializace synth komponent
    if (synthState_.load() != SynthState::Ready || currentSampleRate_ != sampleRate) {
        if (synthState_.load() != SynthState::Uninitialized) {
            Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", "Reinicializace kvuli zmene sample rate");
            cleanupSynth();
        }
        
        if (!initializeSynth(sampleRate)) {
            Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "error", 
                                      "KRITICKA CHYBA: Inicializace synth selhala!");
            return; // Plugin zůstane v error stavu
        }
    }
    
    Logger::getInstance().log("AudioPluginAudioProcessor/prepareToPlay", "info", 
                              "=== AUDIO PROCESSING PRIPRAVEN === Stav: " + getStateDescription());
}

void AudioPluginAudioProcessor::releaseResources()
{
    Logger::getInstance().log("AudioPluginAudioProcessor/releaseResources", "info", "=== UVOLNOVANI AUDIO ZDROJU ===");
    
    // Resetování čítačů
    processBlockCount_ = 0;
    totalMidiEvents_ = 0;
    
    Logger::getInstance().log("AudioPluginAudioProcessor/releaseResources", "info", "Audio processing zastaven");
}

//==============================================================================
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

//==============================================================================
void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    
    // Explicitní kontrola stavu s informativním logováním
    auto currentState = synthState_.load();
    if (currentState != SynthState::Ready) {
        // Vymažeme buffer a logujeme důvod
        buffer.clear();
        
        static int errorLogCount = 0;
        if (errorLogCount < 5) { // Omezíme spam v logu
            Logger::getInstance().log("AudioPluginAudioProcessor/processBlock", "warn", 
                                      "Audio block preskocen - stav: " + getStateDescription());
            errorLogCount++;
        }
        return;
    }
    
    // Počítadlo pro optimalizaci logování
    processBlockCount_++;
    
    // Detailní logování prvních bloků
    if (processBlockCount_ <= 3) {
        Logger::getInstance().log("AudioPluginAudioProcessor/processBlock", "info", 
            "Audio blok #" + juce::String(processBlockCount_) + " - velikost: " + juce::String(buffer.getNumSamples()) + 
            " samples, kanaly: " + juce::String(buffer.getNumChannels()));
    } else if (processBlockCount_ % 1000 == 0) {
        Logger::getInstance().log("AudioPluginAudioProcessor/processBlock", "debug", 
            "Zpracovano " + juce::String(processBlockCount_) + " audio bloku, celkem MIDI: " + juce::String(totalMidiEvents_));
    }
    
    // MIDI EVENT PROCESSING
    if (!midiMessages.isEmpty()) {
        int midiEventsInBlock = 0;
        
        // Počítání MIDI událostí pro logování
        for (const auto& midiMetadata : midiMessages) {
            midiEventsInBlock++;
            totalMidiEvents_++;
        }
        
        if (midiEventsInBlock > 0) {
            Logger::getInstance().log("AudioPluginAudioProcessor/processBlock", "info", 
                "MIDI udalosti v bloku: " + juce::String(midiEventsInBlock) + 
                " (celkem: " + juce::String(totalMidiEvents_) + ")");
        }
        
        // Zpracování MIDI událostí přes MidiStateManager
        midiStateManager_->processMidiBuffer(midiMessages);
    }
    
    // VOICE MANAGEMENT - zpracování MIDI událostí do hlasů
    voiceManager_->processMidiEvents(*midiStateManager_);
    
    // AUDIO GENERATION
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    // Vyčištění přebytečných výstupních kanálů
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }
    
    // Generování audio pro každý výstupní kanál
    for (int channel = 0; channel < totalNumOutputChannels; ++channel) {
        auto* channelData = buffer.getWritePointer(channel);
        
        // Vymazání bufferu před generováním
        std::fill(channelData, channelData + buffer.getNumSamples(), 0.0f);
        
        // Generování audio ze všech hlasů
        // Odstraněn třetí parametr (*sampleLibrary_), protože VoiceManager drží interní referenci
        voiceManager_->generateAudio(channelData, buffer.getNumSamples());
    }
    
    // Voice management refresh (cleanup neaktivních hlasů)
    voiceManager_->refresh();
    
    // Analýza výstupní amplitudy pro první bloky
    if (processBlockCount_ <= 3 && buffer.getNumChannels() > 0) {
        float maxAmplitude = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
            auto* channelData = buffer.getReadPointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
                maxAmplitude = juce::jmax(maxAmplitude, std::abs(channelData[sample]));
            }
        }
        Logger::getInstance().log("AudioPluginAudioProcessor/processBlock", "info", 
            "Maximalni vystupni amplituda: " + juce::String(maxAmplitude, 6));
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
    Logger::getInstance().log("AudioPluginAudioProcessor/createEditor", "info", "Inicializace uzivatelskeho rozhrani");
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
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}