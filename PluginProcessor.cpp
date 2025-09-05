#include "PluginProcessor.h"
#include "PluginEditor.h"

/**
 * @brief Konstruktor procesoru.
 * Inicializuje komponenty.
 */
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , logger_(Logger::getInstance())
    , voiceManager_(sampleLibrary_)
{
    logger_.log("PluginProcessor/constructor", "info", "Procesor inicializován.");
    sampleRate_ = 44100.0;
    synthState_.store(SynthState::Uninitialized);
    processingEnabled_.store(false);
}

/**
 * @brief Destruktor - uvolní vzorky.
 */
AudioPluginAudioProcessor::~AudioPluginAudioProcessor() 
{
    logger_.log("PluginProcessor/destructor", "info", "Procesor uvolněn.");
    processingEnabled_.store(false);
    synthState_.store(SynthState::Uninitialized);
    sampleLibrary_.clear();
}

/**
 * @brief Připraví plugin na přehrávání.
 * Validuje parametry a inicializuje syntetizér.
 * @param sampleRate Sample rate.
 * @param samplesPerBlock Velikost bloku.
 */
void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    logger_.log("PluginProcessor/prepareToPlay", "info", "Příprava zahájena.");
    
    if (sampleRate <= 0.0 || sampleRate > 192000.0 || samplesPerBlock <= 0 || samplesPerBlock > 8192) {
        handleSynthError("Neplatné parametry.");
        return;
    }
    
    sampleRate_ = sampleRate;
    samplesPerBlock_ = samplesPerBlock;
    synthState_.store(SynthState::Initializing);
    processingEnabled_.store(false);
    
    initializeSynth();
    
    if (synthState_.load() == SynthState::Ready) {
        processingEnabled_.store(true);
    }
}

/**
 * @brief Uvolní zdroje, zachová vzorky v paměti.
 */
void AudioPluginAudioProcessor::releaseResources()
{
    logger_.log("PluginProcessor/releaseResources", "info", "Uvolňování zdrojů.");
    processingEnabled_.store(false);
    synthState_.store(SynthState::Uninitialized);
}

/**
 * @brief Inicializuje syntetizér.
 * Kontroluje paměť a načte vzorky.
 */
void AudioPluginAudioProcessor::initializeSynth()
{
    if (synthState_.load() != SynthState::Initializing) return;
    
    logger_.log("PluginProcessor/initializeSynth", "info", "Inicializace zahájena.");
    
    try {
        if (sampleRate_ <= 0.0) throw std::runtime_error("Neplatný sample rate.");
        
        // Kontrola existujících vzorků
        bool hasSamples = false;
        for (uint8_t note = SampleLibrary::MIN_NOTE; note <= SampleLibrary::MAX_NOTE; ++note) {
            for (uint8_t level = 0; level < 8; ++level) {
                if (sampleLibrary_.isNoteAvailable(note, level)) {
                    hasSamples = true;
                    break;
                }
            }
            if (hasSamples) break;
        }
        
        if (hasSamples) {
            logger_.log("PluginProcessor/initializeSynth", "info", "Vzorky v paměti - rychlá inicializace.");
        } else {
            logger_.log("PluginProcessor/initializeSynth", "info", "Načítání vzorků.");
            auto progressCallback = [this](int current, int total, const juce::String&) {
                if (current % 100 == 0 || current == total) {
                    logger_.log("PluginProcessor/initializeSynth", "debug", 
                                "Průběh: " + juce::String(current) + "/" + juce::String(total));
                }
            };
            sampleLibrary_.initialize(sampleRate_, progressCallback);
        }
        
        // Kontrola dostupnosti
        if (!hasSamples) {
            for (uint8_t note = SampleLibrary::MIN_NOTE; note <= SampleLibrary::MAX_NOTE; ++note) {
                for (uint8_t level = 0; level < 8; ++level) {
                    if (sampleLibrary_.isNoteAvailable(note, level)) {
                        hasSamples = true;
                        break;
                    }
                }
                if (hasSamples) break;
            }
            if (!hasSamples) throw std::runtime_error("Žádné vzorky.");
        }
        
        synthState_.store(SynthState::Ready);
        
    } catch (const std::exception& e) {
        handleSynthError("Chyba: " + juce::String(e.what()));
        synthState_.store(SynthState::Error);
    }
}

/**
 * @brief Handling chyb.
 * @param errorMessage Zpráva.
 */
void AudioPluginAudioProcessor::handleSynthError(const juce::String& errorMessage)
{
    logger_.log("PluginProcessor/handleSynthError", "error", errorMessage);
    processingEnabled_.store(false);
}

/**
 * @brief Vrátí popis stavu.
 * @return Popis.
 */
juce::String AudioPluginAudioProcessor::getStateDescription() const
{
    juce::String base;
    switch (synthState_.load()) {
        case SynthState::Uninitialized: base = "Uninitialized"; break;
        case SynthState::Initializing: base = "Initializing"; break;
        case SynthState::Ready: base = "Ready"; break;
        case SynthState::Error: base = "Error"; break;
        default: base = "Unknown";
    }
    base += " (Processing: " + juce::String(processingEnabled_.load() ? "ON" : "OFF") + ")";
    return base;
}

/**
 * @brief Zpracuje audio blok.
 * @param buffer Buffer.
 * @param midiMessages MIDI.
 */
void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (!isReadyForProcessing()) {
        buffer.clear();
        return;
    }
    
    buffer.clear();
    
    int midiCount = 0;
    for (const auto metadata : midiMessages) {
        auto msg = metadata.getMessage();
        midiCount++;
        if (msg.isNoteOn()) {
            midiState_.pushNoteOn(static_cast<uint8_t>(msg.getChannel() - 1), 
                                  static_cast<uint8_t>(msg.getNoteNumber()), 
                                  static_cast<uint8_t>(msg.getVelocity()));
        } else if (msg.isNoteOff()) {
            midiState_.pushNoteOff(static_cast<uint8_t>(msg.getChannel() - 1), 
                                   static_cast<uint8_t>(msg.getNoteNumber()));
        } else if (msg.isController()) {
            midiState_.setControllerValue(static_cast<uint8_t>(msg.getChannel() - 1), 
                                          static_cast<uint8_t>(msg.getControllerNumber()), 
                                          static_cast<uint8_t>(msg.getControllerValue()));
        }
    }
    
    if (midiCount > 0) {
        logger_.log("PluginProcessor/processBlock", "debug", "Zpracováno MIDI: " + juce::String(midiCount));
    }
    
    voiceManager_.processMidiEvents(midiState_);
    
    float* channelData = buffer.getWritePointer(0);
    if (channelData) {
        voiceManager_.generateAudio(channelData, buffer.getNumSamples());
    }
    
    if (buffer.getNumChannels() >= 2) {
        buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
    }
    
    voiceManager_.refresh();
}

/**
 * @brief Vytvoří editor.
 * @return Editor.
 */
juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    logger_.log("PluginProcessor/createEditor", "info", "Vytváření editoru.");
    return new AudioPluginAudioProcessorEditor(*this);
}

/**
 * @brief Factory pro plugin.
 * @return Procesor.
 */
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}