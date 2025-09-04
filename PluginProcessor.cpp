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
    logger_.log("PluginProcessor/constructor", "info", "=== ZAHÁJENÍ INICIALIZACE PROCESSOR ===");
    
    try {
        // Inicializace s bezpečnými výchozími hodnotami
        sampleRate_ = 44100.0;
        synthState_.store(SynthState::Uninitialized);
        processingEnabled_.store(false);
        
        logger_.log("PluginProcessor/constructor", "info", "Základní komponenty inicializovány");
        logger_.log("PluginProcessor/constructor", "info", "Výchozí sample rate: " + juce::String(sampleRate_));
        
    } catch (const std::exception& e) {
        logger_.log("PluginProcessor/constructor", "error", "Chyba v konstruktoru: " + juce::String(e.what()));
        synthState_.store(SynthState::Error);
    } catch (...) {
        logger_.log("PluginProcessor/constructor", "error", "Neznámá chyba v konstruktoru");
        synthState_.store(SynthState::Error);
    }
    
    logger_.log("PluginProcessor/constructor", "info", "=== PROCESSOR KONSTRUKTOR DOKONČEN ===");
    DBG("Processor constructor completed with state: " + getStateDescription());
}

/**
 * @brief Destruktor s kompletním cleanup a logováním.
 */
AudioPluginAudioProcessor::~AudioPluginAudioProcessor() 
{
    logger_.log("PluginProcessor/destructor", "info", "=== ZAHÁJENÍ DESTRUKCE PROCESSOR ===");
    
    try {
        // Okamžité zastavení zpracování
        processingEnabled_.store(false);
        synthState_.store(SynthState::Uninitialized);
        
        logger_.log("PluginProcessor/destructor", "info", "Audio zpracování zastaveno");
        
        // Uvolnění zdrojů v správném pořadí
        sampleLibrary_.clear();
        logger_.log("PluginProcessor/destructor", "info", "Sample library vyčištěna");
        
        // Krátké čekání pro dokončení případných audio vláken
        juce::Thread::sleep(10);
        
    } catch (const std::exception& e) {
        logger_.log("PluginProcessor/destructor", "error", "Chyba v destruktoru: " + juce::String(e.what()));
    } catch (...) {
        logger_.log("PluginProcessor/destructor", "error", "Neznámá chyba v destruktoru");
    }
    
    logger_.log("PluginProcessor/destructor", "info", "=== DESTRUKCE PROCESSOR DOKONČENA ===");
    DBG("Processor destructor completed");
}

void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    logger_.log("PluginProcessor/prepareToPlay", "info", "=== ZAHÁJENÍ PREPARE TO PLAY ===");
    logger_.log("PluginProcessor/prepareToPlay", "info", 
                "Parametry - SampleRate: " + juce::String(sampleRate) + 
                ", BufferSize: " + juce::String(samplesPerBlock));
    
    try {
        // Validace vstupních parametrů
        if (sampleRate <= 0.0 || sampleRate > 192000.0) {
            logger_.log("PluginProcessor/prepareToPlay", "error", "Neplatný sample rate: " + juce::String(sampleRate));
            throw std::invalid_argument(("Neplatný sample rate: " + juce::String(sampleRate)).toStdString());
        }
        
        if (samplesPerBlock <= 0 || samplesPerBlock > 8192) {
            logger_.log("PluginProcessor/prepareToPlay", "error", "Neplatná velikost bufferu: " + juce::String(samplesPerBlock));
            throw std::invalid_argument(("Neplatná velikost bufferu: " + juce::String(samplesPerBlock)).toStdString());
        }
        
        // Uložení starých hodnot pro porovnání
        double oldSampleRate = sampleRate_;
        int oldBufferSize = samplesPerBlock_;
        
        // Uložení nových parametrů
        sampleRate_ = sampleRate;
        samplesPerBlock_ = samplesPerBlock;
        
        // TEPRVE TEĎ detekce změn (po uložení nových hodnot)
        bool sampleRateChanged = (std::abs(sampleRate - oldSampleRate) > 1.0);
        bool bufferSizeChanged = (samplesPerBlock != oldBufferSize);
        bool isFirstInit = (synthState_.load() == SynthState::Uninitialized);
        bool hasError = (synthState_.load() == SynthState::Error);
        
        logger_.log("PluginProcessor/prepareToPlay", "info", 
                   "Změny - SampleRate: " + juce::String(sampleRateChanged ? "ANO" : "NE") + 
                   " (" + juce::String(oldSampleRate) + " -> " + juce::String(sampleRate) + ")" +
                   ", BufferSize: " + juce::String(bufferSizeChanged ? "ANO" : "NE") + 
                   " (" + juce::String(oldBufferSize) + " -> " + juce::String(samplesPerBlock) + ")" +
                   ", FirstInit: " + juce::String(isFirstInit ? "ANO" : "NE") + 
                   ", HasError: " + juce::String(hasError ? "ANO" : "NE"));
        
        // OPTIMALIZACE: Reinicializace pouze když je skutečně potřeba
        bool needsFullReinit = isFirstInit || hasError || sampleRateChanged;
        
        if (needsFullReinit) {
            logger_.log("PluginProcessor/prepareToPlay", "info", 
                       "Provádím PLNOU reinicializaci - důvod: " + 
                       juce::String(isFirstInit ? "první inicializace" : 
                                   hasError ? "chybový stav" : 
                                   sampleRateChanged ? "změna sample rate" : "neznámý"));
            
            // Dočasné zastavení zpracování
            processingEnabled_.store(false);
            synthState_.store(SynthState::Initializing);
            
            // Plná reinicializace (vzorky, voice manager, atd.)
            initializeSynth();
            
        } else if (bufferSizeChanged) {
            logger_.log("PluginProcessor/prepareToPlay", "info", 
                       "Změna POUZE velikosti bufferu z " + juce::String(oldBufferSize) + 
                       " na " + juce::String(samplesPerBlock) + 
                       " - žádná reinicializace vzorků není potřeba");
            
            // Pouze logování změny - žádná reinicializace
            // Audio systém už je připraven, jen se změnila velikost bloku
            
        } else {
            logger_.log("PluginProcessor/prepareToPlay", "info", 
                       "Žádné změny nevyžadují akci - stav zůstává: " + getStateDescription());
        }
        
        // Povolení zpracování pouze pokud je vše v pořádku
        if (synthState_.load() == SynthState::Ready) {
            processingEnabled_.store(true);
            logger_.log("PluginProcessor/prepareToPlay", "info", "Audio zpracování povoleno");
        } else if (!needsFullReinit) {
            // Pokud nebyla reinicializace a přesto není Ready, je problém
            logger_.log("PluginProcessor/prepareToPlay", "warn", 
                       "Neočekávaný stav po změně bufferu: " + getStateDescription());
        }
        
    } catch (const std::exception& e) {
        logger_.log("PluginProcessor/prepareToPlay", "error", "Chyba v prepareToPlay: " + juce::String(e.what()));
        handleSynthError("Chyba v prepareToPlay: " + juce::String(e.what()));
        synthState_.store(SynthState::Error);
        processingEnabled_.store(false);
    } catch (...) {
        logger_.log("PluginProcessor/prepareToPlay", "error", "Neznámá chyba v prepareToPlay");
        handleSynthError("Neznámá chyba v prepareToPlay");
        synthState_.store(SynthState::Error);
        processingEnabled_.store(false);
    }
    
    logger_.log("PluginProcessor/prepareToPlay", "info", 
               "=== PREPARE TO PLAY DOKONČEN - Stav: " + getStateDescription() + 
               ", SampleRate: " + juce::String(sampleRate_) + 
               ", BufferSize: " + juce::String(samplesPerBlock_) + " ===");
    DBG("prepareToPlay completed with state: " + getStateDescription());
}

/**
 * @brief Uvolní zdroje s kompletním logováním.
 */
void AudioPluginAudioProcessor::releaseResources()
{
    logger_.log("PluginProcessor/releaseResources", "info", "=== ZAHÁJENÍ RELEASE RESOURCES ===");
    
    try {
        // Okamžité zastavení zpracování
        processingEnabled_.store(false);
        
        // Vyčištění vzorků
        sampleLibrary_.clear();
        logger_.log("PluginProcessor/releaseResources", "info", "Sample library vyčištěna");
        
        // Reset stavu
        synthState_.store(SynthState::Uninitialized);
        
        logger_.log("PluginProcessor/releaseResources", "info", "Všechny zdroje uvolněny");
        
    } catch (const std::exception& e) {
        logger_.log("PluginProcessor/releaseResources", "error", "Chyba při uvolňování: " + juce::String(e.what()));
    } catch (...) {
        logger_.log("PluginProcessor/releaseResources", "error", "Neznámá chyba při uvolňování");
    }
    
    logger_.log("PluginProcessor/releaseResources", "info", "=== RELEASE RESOURCES DOKONČEN ===");
    DBG("Resources released");
}

/**
 * @brief Inicializuje syntetizér s robustním error handlingem.
 */
void AudioPluginAudioProcessor::initializeSynth()
{
    if (synthState_.load() != SynthState::Initializing) {
        logger_.log("PluginProcessor/initializeSynth", "warn", "Inicializace přeskočena - nesprávný stav: " + getStateDescription());
        return;
    }

    logger_.log("PluginProcessor/initializeSynth", "info", "Zahájení inicializace syntezátoru");
    
    try {
        if (sampleRate_ <= 0.0) {
            throw std::runtime_error("Sample rate není nastaven");
        }
        
        logger_.log("PluginProcessor/initializeSynth", "info", "Inicializace sample library...");
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
            throw std::runtime_error("Žádné vzorky nebyly vygenerovány");
        }
        
        synthState_.store(SynthState::Ready);
        logger_.log("PluginProcessor/initializeSynth", "info", "Syntezátor úspěšně inicializován");
        
    } catch (const std::exception& e) {
        handleSynthError("Inicializace selhala: " + juce::String(e.what()));
        synthState_.store(SynthState::Error);
    } catch (...) {
        handleSynthError("Neznámá chyba při inicializaci");
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
        case SynthState::Uninitialized: base = "Neinicializováno"; break;
        case SynthState::Initializing: base = "Inicializace"; break;
        case SynthState::Ready: base = "Připraveno"; break;
        case SynthState::Error: base = "Chyba"; break;
        default: base = "Neznámý stav"; break;
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
                       "Blok #" + juce::String(blockCounter) + " - Stav: " + getStateDescription() + 
                       ", Velikost: " + juce::String(buffer.getNumSamples()) + 
                       ", Kanály: " + juce::String(buffer.getNumChannels()));
        }
        
        // Základní validace
        if (!processingEnabled_.load() || synthState_.load() != SynthState::Ready) {
            buffer.clear();
            if (shouldLog) {
                logger_.log("PluginProcessor/processBlock", "debug", "Blok přeskočen - zpracování vypnuto nebo nesprávný stav");
            }
            return;
        }
        
        // Validace bufferu
        if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0) {
            logger_.log("PluginProcessor/processBlock", "error", "Neplatný buffer - samples: " + 
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
                                   "NoteOn: nota " + juce::String(msg.getNoteNumber()) + 
                                   ", velocity " + juce::String(msg.getVelocity()) + 
                                   ", kanál " + juce::String(msg.getChannel()));
                    }
                } else if (msg.isNoteOff()) {
                    // OPRAVA: Explicitní cast pro odstranění warning C4244
                    midiState_.pushNoteOff(static_cast<uint8_t>(msg.getChannel() - 1), 
                                          static_cast<uint8_t>(msg.getNoteNumber()));
                    if (shouldLog) {
                        logger_.log("PluginProcessor/processBlock", "debug", 
                                   "NoteOff: nota " + juce::String(msg.getNoteNumber()) + 
                                   ", kanál " + juce::String(msg.getChannel()));
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
            logger_.log("PluginProcessor/processBlock", "error", "Chyba při zpracování MIDI: " + juce::String(e.what()));
            // Pokračujeme bez MIDI dat
        }
        
        if (shouldLog && midiEventCount > 0) {
            logger_.log("PluginProcessor/processBlock", "debug", "MIDI zprávy zpracovány: " + juce::String(midiEventCount));
        }

        // Zpracování hlasů s error handlingem
        try {
            voiceManager_.processMidiEvents(midiState_);
        } catch (const std::exception& e) {
            logger_.log("PluginProcessor/processBlock", "error", "Chyba při zpracování hlasů: " + juce::String(e.what()));
            buffer.clear();
            return;
        }

        // Generace audio s bezpečnostními kontrolami
        buffer.clear();
        
        // OPRAVA: Získání float* pointeru z AudioBuffer
        float* channelData = buffer.getWritePointer(0);
        if (channelData == nullptr) {
            logger_.log("PluginProcessor/processBlock", "error", "Null pointer pro audio buffer kanál 0");
            return;
        }
        
        try {
            // OPRAVA: Předání float* místo AudioBuffer
            voiceManager_.generateAudio(channelData, buffer.getNumSamples());
        } catch (const std::exception& e) {
            logger_.log("PluginProcessor/processBlock", "error", "Chyba při generaci audio: " + juce::String(e.what()));
            buffer.clear();
            return;
        }

        // Bezpečná konverze na stereo
        if (buffer.getNumChannels() >= 2) {
            try {
                buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
                if (shouldLog) {
                    logger_.log("PluginProcessor/processBlock", "debug", "Stereo konverze dokončena");
                }
            } catch (const std::exception& e) {
                logger_.log("PluginProcessor/processBlock", "error", "Chyba při stereo konverzi: " + juce::String(e.what()));
                // Necháme mono, není to kritická chyba
            }
        }

        // Refresh voice manageru
        voiceManager_.refresh();

        if (shouldLog) {
            int activeVoices = voiceManager_.getActiveVoiceCount();
            logger_.log("PluginProcessor/processBlock", "debug", 
                       "Blok dokončen - Aktivní hlasy: " + juce::String(activeVoices));
        }
        
    } catch (const std::exception& e) {
        logger_.log("PluginProcessor/processBlock", "error", "Výjimka v processBlock: " + juce::String(e.what()));
        buffer.clear(); // Zajistíme tichý výstup při chybě
        processingEnabled_.store(false); // Zastavíme zpracování při kritické chybě
    } catch (...) {
        logger_.log("PluginProcessor/processBlock", "error", "Neznámá výjimka v processBlock");
        buffer.clear();
        processingEnabled_.store(false);
    }
}

/**
 * @brief Vytvoří editor s error handlingem.
 */
juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    logger_.log("PluginProcessor/createEditor", "info", "=== VYTVOŘENÍ EDITORU ===");
    
    try {
        auto* editor = new AudioPluginAudioProcessorEditor(*this);
        logger_.log("PluginProcessor/createEditor", "info", "Editor úspěšně vytvořen");
        return editor;
    } catch (const std::exception& e) {
        logger_.log("PluginProcessor/createEditor", "error", "Chyba při vytváření editoru: " + juce::String(e.what()));
        return nullptr;
    } catch (...) {
        logger_.log("PluginProcessor/createEditor", "error", "Neznámá chyba při vytváření editoru");
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