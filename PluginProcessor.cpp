#include "PluginProcessor.h"
#include "PluginEditor.h"

/**
 * @brief Konstruktor AudioPluginAudioProcessor.
 * Inicializuje komponenty a stav.
 */
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , logger_(Logger::getInstance())
    , voiceManager_(sampleLibrary_)  // VoiceManager vyžaduje SampleLibrary
{
    logger_.log("PluginProcessor/constructor", "info", "=== STARTING PROCESSOR INITIALIZATION ===");
    
    try {
        // Inicializace s bezpečnými výchozími hodnotami
        sampleRate_ = 44100.0;
        synthState_.store(SynthState::Uninitialized);
        processingEnabled_.store(false);
        
        logger_.log("PluginProcessor/constructor", "info", "Basic components initialized");
        logger_.log("PluginProcessor/constructor", "info", "Default sample rate: " + juce::String(sampleRate_));
        
    } catch (const std::exception& e) {
        logger_.log("PluginProcessor/constructor", "error", "Error in constructor: " + juce::String(e.what()));
        synthState_.store(SynthState::Error);
    } catch (...) {
        logger_.log("PluginProcessor/constructor", "error", "Unknown error in constructor");
        synthState_.store(SynthState::Error);
    }
    
    logger_.log("PluginProcessor/constructor", "info", "=== PROCESSOR CONSTRUCTOR COMPLETED ===");
    DBG("Processor constructor completed with state: " + getStateDescription());
}

/**
 * @brief Destruktor - NYU opravdu vymažeme samples
 */
AudioPluginAudioProcessor::~AudioPluginAudioProcessor() 
{
    logger_.log("PluginProcessor/destructor", "info", "=== STARTING PROCESSOR DESTRUCTION ===");
    
    try {
        // Okamžité zastavení zpracování
        processingEnabled_.store(false);
        synthState_.store(SynthState::Uninitialized);
        
        logger_.log("PluginProcessor/destructor", "info", "Audio processing stopped");
        
        // NYU opravdu uvolníme vzorky při destruktoru
        sampleLibrary_.clear();
        logger_.log("PluginProcessor/destructor", "info", "Sample library cleared");
        
        // Krátké čekání pro dokončení případných audio vláken
        juce::Thread::sleep(10);
        
    } catch (const std::exception& e) {
        logger_.log("PluginProcessor/destructor", "error", "Error in destructor: " + juce::String(e.what()));
    } catch (...) {
        logger_.log("PluginProcessor/destructor", "error", "Unknown error in destructor");
    }
    
    logger_.log("PluginProcessor/destructor", "info", "=== PROCESSOR DESTRUCTION COMPLETED ===");
    DBG("Processor destructor completed");
}

void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    logger_.log("PluginProcessor/prepareToPlay", "info", "=== PREPARE TO PLAY START ===");
    logger_.log("PluginProcessor/prepareToPlay", "info", 
                "Parameters - SampleRate: " + juce::String(sampleRate) + 
                ", BufferSize: " + juce::String(samplesPerBlock));
    
    try {
        // Validace vstupních parametrů
        if (sampleRate <= 0.0 || sampleRate > 192000.0) {
            logger_.log("PluginProcessor/prepareToPlay", "error", "Invalid sample rate: " + juce::String(sampleRate));
            throw std::invalid_argument(("Invalid sample rate: " + juce::String(sampleRate)).toStdString());
        }
        
        if (samplesPerBlock <= 0 || samplesPerBlock > 8192) {
            logger_.log("PluginProcessor/prepareToPlay", "error", "Invalid buffer size: " + juce::String(samplesPerBlock));
            throw std::invalid_argument(("Invalid buffer size: " + juce::String(samplesPerBlock)).toStdString());
        }
        
        // Uložení starých hodnot pro porovnání
        double oldSampleRate = sampleRate_;
        int oldBufferSize = samplesPerBlock_;
        SynthState oldState = synthState_.load();
        
        // Uložení nových parametrů
        sampleRate_ = sampleRate;
        samplesPerBlock_ = samplesPerBlock;
        
        // OPRAVA: Detekce změn (po uložení nových hodnot) - odstranění unused isFirstInit
        bool sampleRateChanged = (std::abs(sampleRate - oldSampleRate) > 1.0);
        bool bufferSizeChanged = (samplesPerBlock != oldBufferSize);
        bool hasError = (oldState == SynthState::Error);
        
        // KLÍČOVÁ OPRAVA: Rozlišit "první init po startu" vs "reinit po releaseResources"
        bool isInitAfterRelease = (oldState == SynthState::Uninitialized && oldSampleRate > 0);
        bool isTrueFirstInit = (oldState == SynthState::Uninitialized && oldSampleRate == 0);
        
        logger_.log("PluginProcessor/prepareToPlay", "info", 
                   "Changes - SampleRate: " + juce::String(sampleRateChanged ? "YES" : "NO") + 
                   " (" + juce::String(oldSampleRate) + " -> " + juce::String(sampleRate) + ")" +
                   ", BufferSize: " + juce::String(bufferSizeChanged ? "YES" : "NO") + 
                   " (" + juce::String(oldBufferSize) + " -> " + juce::String(samplesPerBlock) + ")" +
                   ", TrueFirstInit: " + juce::String(isTrueFirstInit ? "YES" : "NO") + 
                   ", InitAfterRelease: " + juce::String(isInitAfterRelease ? "YES" : "NO") +
                   ", HasError: " + juce::String(hasError ? "YES" : "NO"));
        
        // KRITICKÁ OPRAVA: Reinicializace pouze když je skutečně potřeba
        bool needsFullReinit = isTrueFirstInit || hasError || sampleRateChanged;
        
        if (needsFullReinit) {
            logger_.log("PluginProcessor/prepareToPlay", "info", 
                       "Performing FULL reinitialization - reason: " + 
                       juce::String(isTrueFirstInit ? "true first init" : 
                                   hasError ? "error state" : 
                                   sampleRateChanged ? "sample rate change" : "unknown"));
            
            // Dočasné zastavení zpracování
            processingEnabled_.store(false);
            synthState_.store(SynthState::Initializing);
            
            // Plná reinicializace (vzorky, voice manager, atd.)
            initializeSynth();
            
        } else if (isInitAfterRelease && !sampleRateChanged) {
            logger_.log("PluginProcessor/prepareToPlay", "info", 
                       "OPTIMIZED reinitialization after releaseResources - same sample rate " + 
                       juce::String(sampleRate) + ", only restoring state");
            
            // OPTIMALIZACE: Po releaseResources se stejným sample rate nemusíme regenerovat samples!
            // Samples jsou již na disku, jen obnovíme stav
            synthState_.store(SynthState::Initializing);
            processingEnabled_.store(false);
            
            // Rychlá reinicializace bez generování samples
            initializeSynthFast();
            
        } else if (bufferSizeChanged) {
            logger_.log("PluginProcessor/prepareToPlay", "info", 
                       "Buffer size change ONLY from " + juce::String(oldBufferSize) + 
                       " to " + juce::String(samplesPerBlock) + 
                       " - NO sample reinitialization needed");
            
            // OPRAVA: Pro změnu buffer size nepotřebujeme reinicializovat samples!
            // Audio engine JUCE si sám spravuje buffery
            
        } else {
            logger_.log("PluginProcessor/prepareToPlay", "info", 
                       "No changes require action - state remains: " + getStateDescription());
        }
        
        // Povolení zpracování pouze pokud je vše v pořádku
        if (synthState_.load() == SynthState::Ready) {
            processingEnabled_.store(true);
            logger_.log("PluginProcessor/prepareToPlay", "info", "Audio processing enabled");
        } else if (!needsFullReinit && !isInitAfterRelease) {
            logger_.log("PluginProcessor/prepareToPlay", "warn", 
                       "Unexpected state after buffer change: " + getStateDescription());
        }
        
    } catch (const std::exception& e) {
        logger_.log("PluginProcessor/prepareToPlay", "error", "Error in prepareToPlay: " + juce::String(e.what()));
        handleSynthError("Error in prepareToPlay: " + juce::String(e.what()));
        synthState_.store(SynthState::Error);
        processingEnabled_.store(false);
    } catch (...) {
        logger_.log("PluginProcessor/prepareToPlay", "error", "Unknown error in prepareToPlay");
        handleSynthError("Unknown error in prepareToPlay");
        synthState_.store(SynthState::Error);
        processingEnabled_.store(false);
    }
    
    logger_.log("PluginProcessor/prepareToPlay", "info", 
               "=== PREPARE TO PLAY COMPLETED - State: " + getStateDescription() + 
               ", SampleRate: " + juce::String(sampleRate_) + 
               ", BufferSize: " + juce::String(samplesPerBlock_) + " ===");
    DBG("prepareToPlay completed with state: " + getStateDescription());
}

/**
 * @brief Uvolní zdroje - OPTIMALIZACE: nemazat samples z paměti
 */
void AudioPluginAudioProcessor::releaseResources()
{
    logger_.log("PluginProcessor/releaseResources", "info", "=== STARTING RELEASE RESOURCES ===");
    
    try {
        // Okamžité zastavení zpracování
        processingEnabled_.store(false);
        
        // OPTIMALIZACE: NEMAZAT samples z paměti
        // Samples zůstávají v RAM pro rychlou reinicializaci
        // sampleLibrary_.clear();  // <-- ZAKOMENTOVÁNO
        
        logger_.log("PluginProcessor/releaseResources", "info", "Sample library kept in memory for fast restart");
        
        // Reset pouze stavu, ne dat
        synthState_.store(SynthState::Uninitialized);
        
        logger_.log("PluginProcessor/releaseResources", "info", "All resources released (samples preserved)");
        
    } catch (const std::exception& e) {
        logger_.log("PluginProcessor/releaseResources", "error", "Error during release: " + juce::String(e.what()));
    } catch (...) {
        logger_.log("PluginProcessor/releaseResources", "error", "Unknown error during release");
    }
    
    logger_.log("PluginProcessor/releaseResources", "info", "=== RELEASE RESOURCES COMPLETED ===");
    DBG("Resources released (samples preserved in memory)");
}

/**
 * @brief Inicializuje syntetizér s robustním error handlingem.
 */
void AudioPluginAudioProcessor::initializeSynth()
{
    if (synthState_.load() != SynthState::Initializing) {
        logger_.log("PluginProcessor/initializeSynth", "warn", "Initialization skipped - wrong state: " + getStateDescription());
        return;
    }

    logger_.log("PluginProcessor/initializeSynth", "info", "Starting synth initialization");
    
    try {
        if (sampleRate_ <= 0.0) {
            throw std::runtime_error("Sample rate not set");
        }
        
        logger_.log("PluginProcessor/initializeSynth", "info", "Initializing sample library...");
        sampleLibrary_.initialize(sampleRate_);
        
        // Kontrola, zda byla inicializace úspěšná
        bool hasValidSamples = false;
        for (uint8_t note = SampleLibrary::MIN_NOTE; note <= SampleLibrary::MAX_NOTE; ++note) {
            for (uint8_t level = 0; level < 8; ++level) {
                if (sampleLibrary_.isNoteAvailable(note, level)) {
                    hasValidSamples = true;
                    break;
                }
            }
            if (hasValidSamples) break;
        }
        
        if (!hasValidSamples) {
            throw std::runtime_error("No samples were generated");
        }
        
        synthState_.store(SynthState::Ready);
        logger_.log("PluginProcessor/initializeSynth", "info", "Synth successfully initialized");
        
    } catch (const std::exception& e) {
        handleSynthError("Initialization failed: " + juce::String(e.what()));
        synthState_.store(SynthState::Error);
    } catch (...) {
        handleSynthError("Unknown error during initialization");
        synthState_.store(SynthState::Error);
    }
}

/**
 * @brief Skutečně rychlá reinicializace - samples už jsou v paměti
 */
void AudioPluginAudioProcessor::initializeSynthFast()
{
    if (synthState_.load() != SynthState::Initializing) {
        logger_.log("PluginProcessor/initializeSynthFast", "warn", "Fast init skipped - wrong state: " + getStateDescription());
        return;
    }

    logger_.log("PluginProcessor/initializeSynthFast", "info", "Starting FAST synth reinitialization");
    
    try {
        if (sampleRate_ <= 0.0) {
            throw std::runtime_error("Sample rate not set");
        }
        
        // Kontrola, zda máme už samples v paměti
        bool hasValidSamples = false;
        for (uint8_t note = SampleLibrary::MIN_NOTE; note <= SampleLibrary::MAX_NOTE; ++note) {
            for (uint8_t level = 0; level < 8; ++level) {
                if (sampleLibrary_.isNoteAvailable(note, level)) {
                    hasValidSamples = true;
                    break;
                }
            }
            if (hasValidSamples) break;
        }
        
        if (hasValidSamples) {
            // INSTANT: Samples už jsou v paměti!
            logger_.log("PluginProcessor/initializeSynthFast", "info", 
                       "Samples already in memory - INSTANT reinitialization (0ms)");
            
            synthState_.store(SynthState::Ready);
            
        } else {
            // Fallback: Musíme načíst z disku
            logger_.log("PluginProcessor/initializeSynthFast", "info", 
                       "No samples in memory - loading from disk...");
            
            auto progressCallback = [this](int current, int total, const juce::String& /*status*/) {
                if (current % 100 == 0 || current == total) {
                    logger_.log("PluginProcessor/initializeSynthFast", "debug", 
                               "Loading progress: " + juce::String(current) + "/" + juce::String(total));
                }
            };
            
            sampleLibrary_.initialize(sampleRate_, progressCallback);
            
            // Kontrola úspěšnosti
            hasValidSamples = false;
            for (uint8_t note = SampleLibrary::MIN_NOTE; note <= SampleLibrary::MAX_NOTE; ++note) {
                for (uint8_t level = 0; level < 8; ++level) {
                    if (sampleLibrary_.isNoteAvailable(note, level)) {
                        hasValidSamples = true;
                        break;
                    }
                }
                if (hasValidSamples) break;
            }
            
            if (!hasValidSamples) {
                throw std::runtime_error("No samples available after loading");
            }
            
            synthState_.store(SynthState::Ready);
        }
        
        logger_.log("PluginProcessor/initializeSynthFast", "info", "Fast synth reinitialization completed successfully");
        
    } catch (const std::exception& e) {
        handleSynthError("Fast initialization failed: " + juce::String(e.what()));
        synthState_.store(SynthState::Error);
    } catch (...) {
        handleSynthError("Unknown error during fast initialization");
        synthState_.store(SynthState::Error);
    }
}

/**
 * @brief Centrální handling chyb s detailním logováním.
 * @param errorMessage Zpráva o chybě
 */
void AudioPluginAudioProcessor::handleSynthError(const juce::String& errorMessage)
{
    logger_.log("PluginProcessor/handleSynthError", "error", errorMessage);
    
    // Zastavení zpracování při chybě
    processingEnabled_.store(false);
    
    // Možné rozšíření o recovery mechanismy
    DBG("Synth error: " + errorMessage);
}

/**
 * @brief Vrátí popis stavu s dodatečnými informacemi.
 * @return String popis
 */
juce::String AudioPluginAudioProcessor::getStateDescription() const
{
    juce::String base;
    switch (synthState_.load()) {
        case SynthState::Uninitialized: base = "Uninitialized"; break;
        case SynthState::Initializing: base = "Initializing"; break;
        case SynthState::Ready: base = "Ready"; break;
        case SynthState::Error: base = "Error"; break;
        default: base = "Unknown state"; break;
    }
    
    base += " (Processing: " + juce::String(processingEnabled_.load() ? "ON" : "OFF") + ")";
    return base;
}

/**
 * @brief Zpracuje audio blok s kompletním error handlingem a validací.
 * @param buffer Audio buffer
 * @param midiMessages MIDI buffer
 */
void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    static int blockCounter = 0;
    blockCounter++;
    
    // Periodické logging každých 1000 bloků pro snížení zátěže
    bool shouldLog = (blockCounter % 1000 == 1) || (blockCounter <= 10);
    
    try {
        if (shouldLog) {
            logger_.log("PluginProcessor/processBlock", "debug", 
                       "Block #" + juce::String(blockCounter) + " - State: " + getStateDescription() + 
                       ", Size: " + juce::String(buffer.getNumSamples()) + 
                       ", Channels: " + juce::String(buffer.getNumChannels()));
        }
        
        // Základní validace
        if (!processingEnabled_.load() || synthState_.load() != SynthState::Ready) {
            buffer.clear();
            if (shouldLog) {
                logger_.log("PluginProcessor/processBlock", "debug", "Block skipped - processing disabled or wrong state");
            }
            return;
        }
        
        // Validace bufferu
        if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0) {
            logger_.log("PluginProcessor/processBlock", "error", "Invalid buffer - samples: " + 
                       juce::String(buffer.getNumSamples()) + ", channels: " + juce::String(buffer.getNumChannels()));
            return;
        }
        
        // Zpracování MIDI zpráv s error handlingem
        int midiEventCount = 0;
        try {
            for (const auto metadata : midiMessages) {
                auto msg = metadata.getMessage();
                midiEventCount++;
                
                if (msg.isNoteOn()) {
                    // OPRAVA: Explicitní cast pro odstranění warning C4244
                    midiState_.pushNoteOn(static_cast<uint8_t>(msg.getChannel() - 1), 
                                         static_cast<uint8_t>(msg.getNoteNumber()), 
                                         static_cast<uint8_t>(msg.getVelocity()));
                    if (shouldLog) {
                        logger_.log("PluginProcessor/processBlock", "debug", 
                                   "NoteOn: note " + juce::String(msg.getNoteNumber()) + 
                                   ", velocity " + juce::String(msg.getVelocity()) + 
                                   ", channel " + juce::String(msg.getChannel()));
                    }
                } else if (msg.isNoteOff()) {
                    // OPRAVA: Explicitní cast pro odstranění warning C4244
                    midiState_.pushNoteOff(static_cast<uint8_t>(msg.getChannel() - 1), 
                                          static_cast<uint8_t>(msg.getNoteNumber()));
                    if (shouldLog) {
                        logger_.log("PluginProcessor/processBlock", "debug", 
                                   "NoteOff: note " + juce::String(msg.getNoteNumber()) + 
                                   ", channel " + juce::String(msg.getChannel()));
                    }
                } else if (msg.isController()) {
                    // OPRAVA: Explicitní cast pro odstranění warning C4244
                    midiState_.setControllerValue(static_cast<uint8_t>(msg.getChannel() - 1), 
                                                 static_cast<uint8_t>(msg.getControllerNumber()), 
                                                 static_cast<uint8_t>(msg.getControllerValue()));
                    if (shouldLog) {
                        logger_.log("PluginProcessor/processBlock", "debug", 
                                   "Controller: #" + juce::String(msg.getControllerNumber()) + 
                                   " = " + juce::String(msg.getControllerValue()));
                    }
                }
            }
        } catch (const std::exception& e) {
            logger_.log("PluginProcessor/processBlock", "error", "Error processing MIDI: " + juce::String(e.what()));
            // Pokračujeme bez MIDI dat
        }
        
        if (shouldLog && midiEventCount > 0) {
            logger_.log("PluginProcessor/processBlock", "debug", "MIDI messages processed: " + juce::String(midiEventCount));
        }

        // Zpracování hlasů s error handlingem
        try {
            voiceManager_.processMidiEvents(midiState_);
        } catch (const std::exception& e) {
            logger_.log("PluginProcessor/processBlock", "error", "Error processing voices: " + juce::String(e.what()));
            buffer.clear();
            return;
        }

        // Generace audio s bezpečnostními kontrolami
        buffer.clear();
        
        // OPRAVA: Získání float* pointeru z AudioBuffer
        float* channelData = buffer.getWritePointer(0);
        if (channelData == nullptr) {
            logger_.log("PluginProcessor/processBlock", "error", "Null pointer for audio buffer channel 0");
            return;
        }
        
        try {
            // OPRAVA: Předání float* místo AudioBuffer
            voiceManager_.generateAudio(channelData, buffer.getNumSamples());
        } catch (const std::exception& e) {
            logger_.log("PluginProcessor/processBlock", "error", "Error generating audio: " + juce::String(e.what()));
            buffer.clear();
            return;
        }

        // Bezpečná konverze na stereo
        if (buffer.getNumChannels() >= 2) {
            try {
                buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
                if (shouldLog) {
                    logger_.log("PluginProcessor/processBlock", "debug", "Stereo conversion completed");
                }
            } catch (const std::exception& e) {
                logger_.log("PluginProcessor/processBlock", "error", "Error in stereo conversion: " + juce::String(e.what()));
                // Necháme mono, není to kritická chyba
            }
        }

        // Refresh voice manageru
        voiceManager_.refresh();

        if (shouldLog) {
            int activeVoices = voiceManager_.getActiveVoiceCount();
            logger_.log("PluginProcessor/processBlock", "debug", 
                       "Block completed - Active voices: " + juce::String(activeVoices));
        }
        
    } catch (const std::exception& e) {
        logger_.log("PluginProcessor/processBlock", "error", "Exception in processBlock: " + juce::String(e.what()));
        buffer.clear(); // Zajistíme tichý výstup při chybě
        processingEnabled_.store(false); // Zastavíme zpracování při kritické chybě
    } catch (...) {
        logger_.log("PluginProcessor/processBlock", "error", "Unknown exception in processBlock");
        buffer.clear();
        processingEnabled_.store(false);
    }
}

/**
 * @brief Vytvoří editor s error handlingem.
 */
juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    logger_.log("PluginProcessor/createEditor", "info", "=== CREATING EDITOR ===");
    
    try {
        auto* editor = new AudioPluginAudioProcessorEditor(*this);
        logger_.log("PluginProcessor/createEditor", "info", "Editor successfully created");
        return editor;
    } catch (const std::exception& e) {
        logger_.log("PluginProcessor/createEditor", "error", "Error creating editor: " + juce::String(e.what()));
        return nullptr;
    } catch (...) {
        logger_.log("PluginProcessor/createEditor", "error", "Unknown error creating editor");
        return nullptr;
    }
}

/**
 * @brief Exportovaná funkce pro JUCE plugin s error handlingem.
 */
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    try {
        return new AudioPluginAudioProcessor();
    } catch (const std::exception& e) {
        DBG("Error creating plugin: " + juce::String(e.what()));
        return nullptr;
    } catch (...) {
        DBG("Unknown error creating plugin");
        return nullptr;
    }
}