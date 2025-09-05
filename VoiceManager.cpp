#include "VoiceManager.h"
#include "Logger.h"

/**
 * @brief Konstruktor SynthVoice
 */
SynthVoice::SynthVoice()
    : logger_(Logger::getInstance())
{
    reset();
}

/**
 * @brief Spustí hlas s automatickým výběrem dynamic levelu
 */
void SynthVoice::start(uint8_t midiNote, uint8_t velocity, const SampleLibrary& library)
{
    reset();

    midiNote_ = midiNote;
    velocity_ = velocity;

    // Mapování velocity na dynamic level
    uint8_t preferredLevel = library.velocityToDynamicLevel(velocity);
    
    // Najdeme nejlepší dostupný level
    uint8_t bestLevel = findBestAvailableLevel(library, midiNote, preferredLevel);
    
    if (bestLevel == 255) {
        logger_.log("SynthVoice/start", "error", 
                   "Žádný dostupný dynamic level pro notu " + juce::String((int)midiNote));
        isActive_ = false;
        return;
    }

    currentDynamicLevel_ = bestLevel;
    sampleData_ = library.getSampleData(midiNote, currentDynamicLevel_);
    currentSampleLength_ = library.getSampleLength(midiNote, currentDynamicLevel_);
    
    // Zjistíme zda je sample stereo
    currentSampleIsStereo_ = library.isSampleStereo(midiNote, currentDynamicLevel_);

    if (!sampleData_ || currentSampleLength_ == 0) {
        logger_.log("SynthVoice/start", "error", 
                   "Neplatný sample pro notu " + juce::String((int)midiNote) + 
                   " level " + juce::String((int)currentDynamicLevel_));
        isActive_ = false;
        return;
    }

    position_ = 0;
    isActive_ = true;

    logger_.log("SynthVoice/start", "debug", 
               "Spuštěna nota " + juce::String((int)midiNote) + 
               " velocity=" + juce::String((int)velocity) +
               " level=" + juce::String((int)currentDynamicLevel_) +
               " délka=" + juce::String(currentSampleLength_) +
               " stereo=" + juce::String(currentSampleIsStereo_ ? "ano" : "ne"));
}

void SynthVoice::stop()
{
    isActive_ = false;
    logger_.log("SynthVoice/stop", "debug", 
               "Zastaven voice nota " + juce::String((int)midiNote_));
}

void SynthVoice::reset()
{
    midiNote_ = 0;
    velocity_ = 0;
    currentDynamicLevel_ = 0;
    isActive_ = false;
    sampleData_ = nullptr;
    currentSampleLength_ = 0;
    position_ = 0;
    queue_ = 0;
    currentSampleIsStereo_ = false;
}

/**
 * @brief Renderuje audio bez real-time gain (pre-computed v samples)
 */
void SynthVoice::render(float* outputBuffer, int numSamples, bool isStereo)
{
    if (!isActive_ || sampleData_ == nullptr || currentSampleLength_ == 0)
        return;

    if (currentSampleIsStereo_ && isStereo) {
        // Stereo sample → stereo output
        for (int i = 0; i < numSamples; ++i) {
            if (position_ >= currentSampleLength_) {
                stop();
                break;
            }
            
            // Interleaved stereo data
            outputBuffer[i * 2] += sampleData_[position_ * 2];     // Left
            outputBuffer[i * 2 + 1] += sampleData_[position_ * 2 + 1]; // Right
            ++position_;
        }
    } else if (!currentSampleIsStereo_ && isStereo) {
        // Mono sample → stereo output (duplicate na oba kanály)
        for (int i = 0; i < numSamples; ++i) {
            if (position_ >= currentSampleLength_) {
                stop();
                break;
            }
            
            float sample = sampleData_[position_];
            outputBuffer[i * 2] += sample;     // Left
            outputBuffer[i * 2 + 1] += sample; // Right
            ++position_;
        }
    } else {
        // Mono sample → mono output NEBO stereo→mono (mix)
        for (int i = 0; i < numSamples; ++i) {
            if (position_ >= currentSampleLength_) {
                stop();
                break;
            }
            
            if (currentSampleIsStereo_) {
                // Stereo sample → mono output (mix L+R)
                float left = sampleData_[position_ * 2];
                float right = sampleData_[position_ * 2 + 1];
                outputBuffer[i] += (left + right) * 0.5f;
            } else {
                // Mono sample → mono output
                outputBuffer[i] += sampleData_[position_];
            }
            ++position_;
        }
    }
}

/**
 * @brief Najde nejlepší dostupný dynamic level
 */
uint8_t SynthVoice::findBestAvailableLevel(const SampleLibrary& library, uint8_t midiNote, uint8_t preferredLevel)
{
    // 1. Zkus preferovaný level
    if (library.isNoteAvailable(midiNote, preferredLevel)) {
        return preferredLevel;
    }

    // 2. Zkus blízké levely (směrem dolů i nahoru)
    for (int offset = 1; offset < 8; ++offset) {
        // Zkus nižší level
        if (preferredLevel >= offset) {
            // Explicitní cast pro odstranění warning C4244
            uint8_t lowerLevel = static_cast<uint8_t>(preferredLevel - offset);
            if (library.isNoteAvailable(midiNote, lowerLevel)) {
                logger_.log("SynthVoice/findBestAvailableLevel", "debug",
                           "Fallback na nižší level " + juce::String((int)lowerLevel) + 
                           " místo " + juce::String((int)preferredLevel));
                return lowerLevel;
            }
        }
        
        // Zkus vyšší level
        // Explicitní cast pro odstranění warning C4244
        uint8_t higherLevel = static_cast<uint8_t>(preferredLevel + offset);
        if (higherLevel < 8 && library.isNoteAvailable(midiNote, higherLevel)) {
            logger_.log("SynthVoice/findBestAvailableLevel", "debug",
                       "Fallback na vyšší level " + juce::String((int)higherLevel) + 
                       " místo " + juce::String((int)preferredLevel));
            return higherLevel;
        }
    }

    // 3. Žádný level není dostupný
    return 255;
}

// ======================== VoiceManager =========================

/**
 * @brief Konstruktor VoiceManager
 */
VoiceManager::VoiceManager(const SampleLibrary& library, int numVoices)
    : logger_(Logger::getInstance()), sampleLibrary_(library)
{
    voices_.reserve(numVoices);
    for (int i = 0; i < numVoices; ++i) {
        voices_.emplace_back(std::make_unique<SynthVoice>());
        voices_.back()->setQueue(0);
    }

    logger_.log("VoiceManager/constructor", "info", 
               "VoiceManager vytvořen s " + juce::String(numVoices) + " hlasy pro dynamic levels");
}

/**
 * @brief Zpracuje MIDI události z MidiStateManager
 */
void VoiceManager::processMidiEvents(MidiStateManager& midiState)
{
    // Zpracování NOTE ON
    for (int ch = 0; ch < 16; ++ch) {
        while (true) {
            uint8_t raw = midiState.popNoteOn(static_cast<uint8_t>(ch));
            if (raw == 255) break;
            
            uint8_t note = raw;
            uint8_t vel = midiState.getVelocity(static_cast<uint8_t>(ch), note);
            startVoice(note, vel);
        }
    }

    // Zpracování NOTE OFF
    for (int ch = 0; ch < 16; ++ch) {
        while (true) {
            uint8_t raw = midiState.popNoteOff(static_cast<uint8_t>(ch));
            if (raw == 255) break;
            
            uint8_t note = raw;
            stopVoice(note);
        }
    }
}

/**
 * @brief Generuje audio mixem hlasů s stereo support
 */
void VoiceManager::generateAudio(float* buffer, int numSamples)
{
    if (buffer == nullptr || numSamples <= 0) return;

    // Detekujeme zda buffer vypadá jako stereo (heuristika)
    // V reálné implementaci by to měl být parametr
    bool isStereoOutput = true; // Předpokládáme stereo output
    
    // Mix všech aktivních hlasů
    for (auto& v : voices_) {
        if (v->isActive()) {
            v->render(buffer, numSamples, isStereoOutput);
        }
    }
}

/**
 * @brief Housekeeping a statistiky
 */
void VoiceManager::refresh()
{
    #ifdef DEBUG_VOICE_MANAGER

    ++refreshCounter_;
    
    // Reset voice stealing counter
    voicesStolenSinceLastRefresh_ = 0;
    
    // Aktualizace statistik
    updateStatistics();
    
    // Periodické logování
    if (refreshCounter_ % PERIODIC_LOG_INTERVAL == 0) {
        logPeriodicStatus();
    }

    #endif
}

/**
 * @brief Spustí hlas s enhanced voice stealing
 */
void VoiceManager::startVoice(uint8_t midiNote, uint8_t velocity)
{
    // Nejprve hledej existující voice pro tuto notu
    for (auto& v : voices_) {
        if (v->isActive() && v->getNote() == midiNote) {
            v->start(midiNote, velocity, sampleLibrary_);
            mixleQueue(v->getQueue());
            v->setQueue(static_cast<uint8_t>(voices_.size() - 1));
            return;
        }
    }

    // Hledej volnou voice s nejvyšším queue
    SynthVoice* candidate = nullptr;
    uint8_t maxQueue = 0;
    for (auto& v : voices_) {
        if (!v->isActive() && v->getQueue() >= maxQueue) {
            candidate = v.get();
            maxQueue = v->getQueue();
        }
    }

    // Pokud není volná, použij voice stealing
    if (!candidate) {
        candidate = findVoiceStealingCandidate();
        if (candidate) {
            voicesStolenSinceLastRefresh_++;
            logger_.log("VoiceManager/startVoice", "debug", 
                       "Voice stealing pro notu " + juce::String((int)midiNote) + 
                       " (ukradena nota " + juce::String((int)candidate->getNote()) + ")");
        }
    }

    if (candidate) {
        mixleQueue(candidate->getQueue());
        candidate->start(midiNote, velocity, sampleLibrary_);
        candidate->setQueue(static_cast<uint8_t>(voices_.size() - 1));
    } else {
        logger_.log("VoiceManager/startVoice", "warn", 
                   "Nelze najít voice pro notu " + juce::String((int)midiNote));
    }
}

/**
 * @brief Zastaví hlas pro danou notu
 */
void VoiceManager::stopVoice(uint8_t midiNote)
{
    for (auto& v : voices_) {
        if (v->isActive() && v->getNote() == midiNote) {
            v->stop();
            mixleQueue(v->getQueue());
            v->setQueue(0);
            return;
        }
    }
}

/**
 * @brief Enhanced voice stealing algorithm
 */
SynthVoice* VoiceManager::findVoiceStealingCandidate()
{
    SynthVoice* candidate = nullptr;
    uint8_t maxQueue = 0;
    float maxProgress = 0.0f;
    
    // Najdi voice s nejvyšším queue (nejstarší) a nejvyšším progress
    for (auto& v : voices_) {
        if (v->isActive()) {
            if (v->getQueue() > maxQueue || 
                (v->getQueue() == maxQueue && v->getProgress() > maxProgress)) {
                candidate = v.get();
                maxQueue = v->getQueue();
                maxProgress = v->getProgress();
            }
        }
    }
    
    return candidate;
}

/**
 * @brief Přeuspořádá queue priorit
 */
void VoiceManager::mixleQueue(uint8_t queueNumber) 
{
    for (auto& v : voices_) {
        if (v->getQueue() == queueNumber) {
            v->setQueue(0);  // Posun na dno
        } else if (v->getQueue() > queueNumber) {
            v->setQueue(v->getQueue() - 1);  // Posun dolů
        } else {
            v->setQueue(v->getQueue() + 1);  // Posun nahoru
        }
    }
}

/**
 * @brief Vrátí počet aktivních hlasů
 */
int VoiceManager::getActiveVoiceCount() const
{
    int count = 0;
    for (const auto& voice : voices_) {
        if (voice->isActive()) ++count;
    }
    return count;
}

/**
 * @brief Vrátí počet hlasů podle dynamic levelů
 */
std::array<int, 8> VoiceManager::getVoiceCountByDynamicLevel() const
{
    std::array<int, 8> counts{};
    
    for (const auto& voice : voices_) {
        if (voice->isActive()) {
            uint8_t level = voice->getDynamicLevel();
            if (level < 8) {
                counts[level]++;
            }
        }
    }
    
    return counts;
}

/**
 * @brief Vrátí detailní statistiky
 */
VoiceManager::VoiceStats VoiceManager::getVoiceStats() const
{
    return lastStats_;
}

/**
 * @brief Aktualizuje statistiky
 */
void VoiceManager::updateStatistics() const
{
    lastStats_.totalVoices = static_cast<int>(voices_.size());
    lastStats_.activeVoices = getActiveVoiceCount();
    lastStats_.inactiveVoices = lastStats_.totalVoices - lastStats_.activeVoices;
    lastStats_.dynamicLevelCount = getVoiceCountByDynamicLevel();
    lastStats_.voicesStolenThisRefresh = voicesStolenSinceLastRefresh_;
    
    // Průměrný progress
    float totalProgress = 0.0f;
    int activeCount = 0;
    for (const auto& voice : voices_) {
        if (voice->isActive()) {
            totalProgress += voice->getProgress();
            activeCount++;
        }
    }
    lastStats_.averageProgress = (activeCount > 0) ? (totalProgress / activeCount) : 0.0f;
}

/**
 * @brief Loguje periodický status
 */
void VoiceManager::logPeriodicStatus()
{
    const auto& stats = lastStats_;
    
    juce::String dynamicLevelInfo;
    for (int i = 0; i < 8; ++i) {
        if (stats.dynamicLevelCount[i] > 0) {
            dynamicLevelInfo += "L" + juce::String(i) + ":" + juce::String(stats.dynamicLevelCount[i]) + " ";
        }
    }
    
    logger_.log("VoiceManager/periodicStatus", "info",
               "Voices: " + juce::String(stats.activeVoices) + "/" + juce::String(stats.totalVoices) + 
               " active, avg progress: " + juce::String(stats.averageProgress * 100.0f, 1) + "%" +
               " dynamic levels: " + dynamicLevelInfo);
}