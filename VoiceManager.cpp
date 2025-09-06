#include "VoiceManager.h"
#include "Logger.h"

// ======================== SynthVoice Implementation =========================

/**
 * @brief Konstruktor SynthVoice
 * Inicializuje všechny member variables na bezpečné výchozí hodnoty.
 */
SynthVoice::SynthVoice()
    : logger_(Logger::getInstance())
{
    reset();
}

/**
 * @brief Spustí voice s automatickým výběrem dynamic levelu
 * Přechod state: Inactive/Release → Playing
 * @param midiNote MIDI nota (21-108)
 * @param velocity MIDI velocity (1-127)
 * @param library Reference na SampleLibrary pro přístup k audio datům
 */
void SynthVoice::start(uint8_t midiNote, uint8_t velocity, const SampleLibrary& library)
{
    // OPTIMALIZACE: Rychlý restart stejné noty bez full resetu
    if (voiceState_ == VoiceState::Playing && midiNote_ == midiNote) 
    {
        // Pouze aktualizuj velocity a resetuj pozici
        velocity_ = velocity;
        currentDynamicLevel_ = library.velocityToDynamicLevel(velocity);
        position_ = 0;
        releaseCounter_ = 0;
        return;
    }

    // Plný reset při změně noty
    reset();

    midiNote_ = midiNote;
    velocity_ = velocity;
    voiceState_ = VoiceState::Playing;

    // Mapování velocity na dynamic level (0-7)
    uint8_t preferredLevel = library.velocityToDynamicLevel(velocity);
    
    // Najdeme nejlepší dostupný level (s fallback logikou)
    uint8_t bestLevel = findBestAvailableLevel(library, midiNote, preferredLevel);
    
    if (bestLevel == 255) {
        logger_.log("SynthVoice/start", "error", 
                   "No available dynamic level for note " + juce::String((int)midiNote));
        voiceState_ = VoiceState::Inactive;
        return;
    }

    // Načtení sample dat z knihovny
    currentDynamicLevel_ = bestLevel;
    sampleData_ = library.getSampleData(midiNote, currentDynamicLevel_);
    currentSampleLength_ = library.getSampleLength(midiNote, currentDynamicLevel_);
    
    // Zjištění stereo/mono formátu
    currentSampleIsStereo_ = library.isSampleStereo(midiNote, currentDynamicLevel_);

    // Validace načtených dat
    if (!sampleData_ || currentSampleLength_ == 0) {
        logger_.log("SynthVoice/start", "error", 
                   "Invalid sample data for note " + juce::String((int)midiNote) + 
                   " level " + juce::String((int)currentDynamicLevel_));
        voiceState_ = VoiceState::Inactive;
        return;
    }

    // Inicializace playback stavu
    position_ = 0;
    releaseCounter_ = 0;

    // REDUKOVANÉ LOGOVÁNÍ - pouze každých 50 startů
    static int startCounter = 0;
    if (++startCounter % 50 == 0) {
        logger_.log("SynthVoice/start", "debug", 
                   "Voice started: note=" + juce::String((int)midiNote) + 
                   " vel=" + juce::String((int)velocity) +
                   " level=" + juce::String((int)currentDynamicLevel_));
    }
}

/**
 * @brief Spustí release fázi s časovým counterem (minimalistická varianta)
 * Přechod state: Playing → Release
 * Voice bude automaticky ukončena po RELEASE_DURATION_SAMPLES bez fade-out
 */
void SynthVoice::startRelease()
{
    if (voiceState_ == VoiceState::Playing) {
        voiceState_ = VoiceState::Release;
        releaseCounter_ = 0;
        
        // REDUKOVANÉ LOGOVÁNÍ - pouze každých 100 release
        static int releaseCounter = 0;
        if (++releaseCounter % 100 == 0) {
            double releaseTimeMs = (static_cast<double>(RELEASE_DURATION_SAMPLES) / ASSUMED_SAMPLE_RATE) * 1000.0;
            logger_.log("SynthVoice/startRelease", "debug", 
                       "Voice release: note=" + juce::String((int)midiNote_) +
                       " (" + juce::String(releaseTimeMs, 1) + "ms)");
        }
    }
}

/**
 * @brief Okamžitě zastaví voice
 * Přechod state: Playing/Release → Inactive
 */
void SynthVoice::stop()
{
    voiceState_ = VoiceState::Inactive;
}

/**
 * @brief Resetuje voice do výchozího stavu
 * Přechod state: Any → Inactive
 */
void SynthVoice::reset()
{
    voiceState_ = VoiceState::Inactive;
    midiNote_ = 0;
    velocity_ = 0;
    currentDynamicLevel_ = 0;
    sampleData_ = nullptr;
    currentSampleLength_ = 0;
    position_ = 0;
    queue_ = 0;
    currentSampleIsStereo_ = false;
    releaseCounter_ = 0;
}

/**
 * @brief Renderuje audio s release counter management
 * Automaticky počítá release counter a ukončuje voice po vypršení.
 * Minimalistická varianta: žádný fade-out, jen časový limit.
 * @param outputBuffer Ukazatel na output buffer (mono nebo interleaved stereo)
 * @param numSamples Počet samples k renderování (per channel)
 * @param isStereo True pokud output buffer je stereo interleaved
 */
void SynthVoice::render(float* outputBuffer, int numSamples, bool isStereo)
{
    // RYCHLÁ EXIT CONDITION - žádné aktivní voice nebo data
    if (!isActive() || sampleData_ == nullptr || currentSampleLength_ == 0 || !outputBuffer)
        return;

    // OPTIMALIZACE: Předpočítané hodnoty pro rychlejší přístup
    const bool sampleIsStereo = currentSampleIsStereo_;
    const uint32_t maxPosition = currentSampleLength_;
    const float* samplePtr = sampleData_;
    const uint32_t currentPos = position_;
    
    // RYCHLÁ KONTROLA KONCE SAMPLU
    if (currentPos >= maxPosition) {
        voiceState_ = VoiceState::Inactive;
        return;
    }
    
    // RYCHLÁ KONTROLA RELEASE COUNTERU
    if (voiceState_ == VoiceState::Release) {
        if (releaseCounter_ >= RELEASE_DURATION_SAMPLES) {
            voiceState_ = VoiceState::Inactive;
            return;
        }
    }
    
    // VYPOČET SKUTEČNÉHO POČTU SAMPLES K RENDEROVÁNÍ
    const uint32_t samplesToRender = (numSamples < (maxPosition - currentPos)) ? 
                                    numSamples : (maxPosition - currentPos);
    
    // OPTIMALIZACE: SPECIFICKÉ RENDERING CESTY PRO RŮZNÉ PŘÍPADY
    if (isStereo) {
        if (sampleIsStereo) {
            // OPTIMALIZACE: Stereo sample → Stereo output (přímé kopírování)
            for (uint32_t i = 0; i < samplesToRender; ++i) {
                const uint32_t sampleIndex = (currentPos + i) * 2;
                outputBuffer[i * 2] += samplePtr[sampleIndex];
                outputBuffer[i * 2 + 1] += samplePtr[sampleIndex + 1];
            }
        } else {
            // OPTIMALIZACE: Mono sample → Stereo output (duplikace)
            for (uint32_t i = 0; i < samplesToRender; ++i) {
                const float sample = samplePtr[currentPos + i];
                outputBuffer[i * 2] += sample;
                outputBuffer[i * 2 + 1] += sample;
            }
        }
    } else {
        // OPTIMALIZACE: Mono output
        if (sampleIsStereo) {
            for (uint32_t i = 0; i < samplesToRender; ++i) {
                const uint32_t sampleIndex = (currentPos + i) * 2;
                outputBuffer[i] += (samplePtr[sampleIndex] + samplePtr[sampleIndex + 1]) * 0.5f;
            }
        } else {
            for (uint32_t i = 0; i < samplesToRender; ++i) {
                outputBuffer[i] += samplePtr[currentPos + i];
            }
        }
    }
    
    // AKTUALIZACE POZICE
    position_ += samplesToRender;
    
    // AKTUALIZACE RELEASE COUNTERU
    if (voiceState_ == VoiceState::Release) {
        releaseCounter_ += samplesToRender;
        if (releaseCounter_ >= RELEASE_DURATION_SAMPLES) {
            voiceState_ = VoiceState::Inactive;
        }
    }
    
    // KONEČNÁ KONTROLA KONCE SAMPLU
    if (position_ >= maxPosition) {
        voiceState_ = VoiceState::Inactive;
    }
}

/**
 * @brief Najde nejlepší dostupný dynamic level s fallback logikou
 * Implementuje spiral search pattern - hledá nejblíže preferovanému levelu.
 * @param library Reference na SampleLibrary
 * @param midiNote MIDI nota k vyhledání
 * @param preferredLevel Preferovaný dynamic level (z velocity mapping)
 * @return Nejlepší dostupný level nebo 255 pokud žádný není dostupný
 */
uint8_t SynthVoice::findBestAvailableLevel(const SampleLibrary& library, uint8_t midiNote, uint8_t preferredLevel)
{
    // 1. Zkus preferovaný level
    if (library.isNoteAvailable(midiNote, preferredLevel)) {
        return preferredLevel;
    }

    // 2. Spiral search - hledej nejbližší dostupný level
    for (int offset = 1; offset < 8; ++offset) {
        // Zkus nižší level (měkčí dynamika)
        if (preferredLevel >= offset) {
            uint8_t lowerLevel = static_cast<uint8_t>(preferredLevel - offset);
            if (library.isNoteAvailable(midiNote, lowerLevel)) {
                return lowerLevel;
            }
        }
        
        // Zkus vyšší level (tvrdší dynamika)
        uint8_t higherLevel = static_cast<uint8_t>(preferredLevel + offset);
        if (higherLevel < 8 && library.isNoteAvailable(midiNote, higherLevel)) {
            return higherLevel;
        }
    }

    // 3. Žádný level není dostupný
    return 255;
}

// ======================== VoiceManager Implementation =========================

/**
 * @brief Konstruktor VoiceManager
 * Inicializuje pool voices a nastavuje výchozí statistiky.
 * @param library Reference na SampleLibrary (musí být platná po celou dobu života VoiceManager)
 * @param numVoices Počet voices v poolu (výchozí 16)
 */
VoiceManager::VoiceManager(const SampleLibrary& library, int numVoices)
    : logger_(Logger::getInstance()), sampleLibrary_(library)
{
    // Validace vstupních parametrů
    if (numVoices <= 0 || numVoices > 64) {
        logger_.log("VoiceManager/constructor", "warn", 
                   "Invalid voice count " + juce::String(numVoices) + ", using 16");
        numVoices = 16;
    }

    // Inicializace voice poolu
    voices_.reserve(numVoices);
    for (int i = 0; i < numVoices; ++i) {
        voices_.emplace_back(std::make_unique<SynthVoice>());
        voices_.back()->setQueue(0);
    }

    // Reset statistik
    lastStats_ = VoiceStats();
    voicesStolenSinceLastRefresh_ = 0;
    releaseVoicesStolen_ = 0;
    playingVoicesStolen_ = 0;
    refreshCounter_ = 0;

    logger_.log("VoiceManager/constructor", "info", 
               "VoiceManager created with " + juce::String(numVoices) + " voices");
}

/**
 * @brief Zpracuje MIDI události z MidiStateManager
 * Hlavní vstupní bod pro MIDI data - zpracovává note-on a note-off události.
 * @param midiState Reference na MidiStateManager s MIDI frontami
 */
void VoiceManager::processMidiEvents(MidiStateManager& midiState)
{
    int totalNotesProcessed = 0;
    auto processStart = juce::Time::getMillisecondCounterHiRes();

    // === ZPRACOVÁNÍ NOTE ON UDÁLOSTÍ ===
    for (int ch = 0; ch < 16; ++ch) {
        while (true) {
            uint8_t rawNote = midiState.popNoteOn(static_cast<uint8_t>(ch));
            if (rawNote == 255) break;
            
            uint8_t note = rawNote;
            uint8_t velocity = midiState.getVelocity(static_cast<uint8_t>(ch), note);
            
            if (velocity == 0) {
                stopVoice(note);
            } else {
                startVoice(note, velocity);
                totalNotesProcessed++;
            }
            
            // Safety limit pro zachování responsiveness
            if (totalNotesProcessed >= 30) break;
        }
        if (totalNotesProcessed >= 30) break;
    }

    // === ZPRACOVÁNÍ NOTE OFF UDÁLOSTÍ ===
    for (int ch = 0; ch < 16; ++ch) {
        while (true) {
            uint8_t rawNote = midiState.popNoteOff(static_cast<uint8_t>(ch));
            if (rawNote == 255) break;
            
            stopVoice(rawNote);
            totalNotesProcessed++;
            
            // Safety limit
            if (totalNotesProcessed >= 50) break;
        }
        if (totalNotesProcessed >= 50) break;
    }

    // LOGOVÁNÍ POUZE PŘI VYSOKÉM ZATÍŽENÍ
    auto processTime = juce::Time::getMillisecondCounterHiRes() - processStart;
    if (processTime > 1.0 || totalNotesProcessed > 10) {
        logger_.log("VoiceManager/processMidiEvents", "debug", 
                   "Processed " + juce::String(totalNotesProcessed) + " events in " + 
                   juce::String(processTime, 1) + "ms");
    }
}

/**
 * @brief Generuje audio mixem všech aktivních voices
 * Renderuje Playing i Release voices (obě jsou "aktivní").
 * @param buffer Ukazatel na audio buffer (stereo interleaved)
 * @param numSamples Počet samples k vygenerování (per channel)
 */
void VoiceManager::generateAudio(float* buffer, int numSamples)
{
    // ZÁKLADNÍ VALIDACE
    if (buffer == nullptr || numSamples <= 0) {
        return;
    }

    // OPTIMALIZACE: RYCHLÝ VÝSTUP POKUD ŽÁDNÉ AKTIVNÍ VOICES
    bool hasActiveVoices = false;
    for (auto& voice : voices_) {
        if (voice->isActive()) {
            hasActiveVoices = true;
            break;
        }
    }
    if (!hasActiveVoices) return;

    // PŘEDPOKLÁDÁME STEREO OUTPUT
    constexpr bool isStereoOutput = true;
    int activeVoiceCount = 0;
    int playingCount = 0;
    int releaseCount = 0;
    
    // MIX VŠECH AKTIVNÍCH VOICES
    for (auto& voice : voices_) {
        if (voice->isActive()) {
            voice->render(buffer, numSamples, isStereoOutput);
            activeVoiceCount++;
            
            if (voice->isPlaying()) {
                playingCount++;
            } else if (voice->isInRelease()) {
                releaseCount++;
            }
        }
    }

    // REDUKOVANÉ LOGOVÁNÍ - pouze při změně stavu
    static int lastActiveCount = 0;
    static int lastPlayingCount = 0;
    static int lastReleaseCount = 0;
    
    if (activeVoiceCount != lastActiveCount || 
        playingCount != lastPlayingCount || 
        releaseCount != lastReleaseCount) {
        
        lastActiveCount = activeVoiceCount;
        lastPlayingCount = playingCount;
        lastReleaseCount = releaseCount;
    }
}

/**
 * @brief Housekeeping a statistiky
 * Pravidelně volaná metoda pro údržbu voice manageru a aktualizaci statistik.
 */
void VoiceManager::refresh()
{
    ++refreshCounter_;
    
    // Reset voice stealing counters pro tento cyklus
    voicesStolenSinceLastRefresh_ = 0;
    releaseVoicesStolen_ = 0;
    playingVoicesStolen_ = 0;
    
    // Aktualizace statistik
    updateStatistics();
    
    // Periodické logování (každých 1000 cyklů)
    if (refreshCounter_ % PERIODIC_LOG_INTERVAL == 0) {
        logPeriodicStatus();
    }
}

/**
 * @brief VOICE ALLOCATION ALGORITMUS s release counter podporou
 * 
 * Implementuje intelligent voice management s následující prioritou:
 * 1. NOTE RESTART DETECTION - stejná nota na stejném voice
 * 2. FREE VOICE ALLOCATION - najdi neaktivní voice
 * 3. RELEASE VOICE STEALING - ukradni voice v release fázi (preferované)
 * 4. PLAYING VOICE STEALING - ukradni aktivně hrající voice (last resort)
 * 
 * @param midiNote MIDI nota (0-127)
 * @param velocity MIDI velocity (1-127)
 */
void VoiceManager::startVoice(uint8_t midiNote, uint8_t velocity)
{
    // REDUKOVANÉ LOGOVÁNÍ - pouze každých 25 volání
    static int callCounter = 0;
    bool shouldLog = (++callCounter % 25 == 0);
    
    if (shouldLog) {
        logger_.log("VoiceManager/startVoice", "debug", 
                   "Allocation: note=" + juce::String((int)midiNote) + 
                   " vel=" + juce::String((int)velocity));
    }

    // === KROK 1: NOTE RESTART DETECTION ===
    SynthVoice* existingVoice = findVoicePlayingNote(midiNote);
    if (existingVoice) {
        existingVoice->start(midiNote, velocity, sampleLibrary_);
        return;
    }

    // === KROK 2: FREE VOICE ALLOCATION ===
    SynthVoice* freeVoice = findBestFreeVoice();
    if (freeVoice) {
        freeVoice->start(midiNote, velocity, sampleLibrary_);
        assignTopPriority(freeVoice);
        return;
    }

    // === KROK 3: RELEASE VOICE STEALING ===
    SynthVoice* releaseVoice = findBestReleaseCandidate();
    if (releaseVoice) {
        uint8_t stolenNote = releaseVoice->getNote();
        releaseVoice->start(midiNote, velocity, sampleLibrary_);
        assignTopPriority(releaseVoice);
        
        voicesStolenSinceLastRefresh_++;
        releaseVoicesStolen_++;
        return;
    }

    // === KROK 4: PLAYING VOICE STEALING (LAST RESORT) ===
    SynthVoice* playingVoice = findBestPlayingCandidate();
    if (playingVoice) {
        uint8_t stolenNote = playingVoice->getNote();
        playingVoice->start(midiNote, velocity, sampleLibrary_);
        assignTopPriority(playingVoice);
        
        voicesStolenSinceLastRefresh_++;
        playingVoicesStolen_++;
        return;
    }

    // === KROK 5: ALLOCATION FAILURE ===
    if (shouldLog) {
        logger_.log("VoiceManager/startVoice", "error", 
                   "Allocation failed for note " + juce::String((int)midiNote));
    }
}

/**
 * @brief Spustí release counter pro danou notu
 * Přechod: Playing → Release (spustí release counter)
 * @param midiNote MIDI nota k release
 */
void VoiceManager::stopVoice(uint8_t midiNote)
{
    // Najdi voice hrající tuto notu (pouze Playing state)
    for (auto& voice : voices_) {
        if (voice->isPlaying() && voice->getNote() == midiNote) {
            voice->startRelease();
            return;
        }
    }
}

// === VOICE ALLOCATION HELPER METHODS ===

/**
 * @brief Najde voice již hrající specifickou notu
 * Používá se pro note restart detection (KROK 1)
 * @param midiNote Nota k vyhledání
 * @return Voice hrající tuto notu nebo nullptr
 */
SynthVoice* VoiceManager::findVoicePlayingNote(uint8_t midiNote)
{
    for (auto& voice : voices_) {
        if (voice->isPlaying() && voice->getNote() == midiNote) {
            return voice.get();
        }
    }
    return nullptr;
}

/**
 * @brief Najde nejlepší neaktivní voice
 * Používá se pro free voice allocation (KROK 2)
 * Preferuje voice s nejnižší prioritou (nejdéle nepoužitá)
 * @return Nejlepší free voice nebo nullptr
 */
SynthVoice* VoiceManager::findBestFreeVoice()
{
    SynthVoice* bestCandidate = nullptr;
    uint8_t lowestQueue = 255;
    
    for (auto& voice : voices_) {
        if (voice->isInactive()) {
            uint8_t currentQueue = voice->getQueue();
            if (currentQueue <= lowestQueue) {
                bestCandidate = voice.get();
                lowestQueue = currentQueue;
            }
        }
    }
    
    return bestCandidate;
}

/**
 * @brief Najde nejlepší release voice pro stealing
 * Používá se pro release voice stealing (KROK 3)
 * Preferuje nejstarší release voice (nejnižší queue priority)
 * @return Nejlepší release candidate nebo nullptr
 */
SynthVoice* VoiceManager::findBestReleaseCandidate()
{
    SynthVoice* bestCandidate = nullptr;
    uint8_t lowestQueue = 255;
    uint32_t highestCounter = 0;
    
    for (auto& voice : voices_) {
        if (voice->isInRelease()) {
            uint8_t currentQueue = voice->getQueue();
            uint32_t releaseRemaining = voice->getReleaseCounterRemaining();
            uint32_t releaseElapsed = SynthVoice::RELEASE_DURATION_SAMPLES - releaseRemaining;
            
            if (currentQueue < lowestQueue || 
                (currentQueue == lowestQueue && releaseElapsed > highestCounter)) {
                bestCandidate = voice.get();
                lowestQueue = currentQueue;
                highestCounter = releaseElapsed;
            }
        }
    }
    
    return bestCandidate;
}

/**
 * @brief Najde nejlepší playing voice pro stealing (last resort)
 * Používá se pro playing voice stealing (KROK 4)
 * Preferuje nejstarší + nejvyšší progress (minimální audio disruption)
 * @return Nejlepší playing candidate nebo nullptr
 */
SynthVoice* VoiceManager::findBestPlayingCandidate()
{
    SynthVoice* bestCandidate = nullptr;
    uint8_t lowestQueue = 255;
    float highestProgress = 0.0f;
    
    for (auto& voice : voices_) {
        if (voice->isPlaying()) {
            uint8_t currentQueue = voice->getQueue();
            float currentProgress = voice->getProgress();
            
            if (currentQueue < lowestQueue || 
                (currentQueue == lowestQueue && currentProgress > highestProgress)) {
                bestCandidate = voice.get();
                lowestQueue = currentQueue;
                highestProgress = currentProgress;
            }
        }
    }
    
    return bestCandidate;
}

// === PRIORITY MANAGEMENT ===

/**
 * @brief Přiřadí voice top prioritu (pro nově spuštěné voices)
 * Nově alokované voices dostávají nejvyšší prioritu
 * @param voice Voice k povýšení
 */
void VoiceManager::assignTopPriority(SynthVoice* voice)
{
    if (!voice) return;
    
    // Najdi aktuálně nejvyšší queue v systému
    uint8_t maxQueue = 0;
    for (const auto& v : voices_) {
        if (v.get() != voice && v->getQueue() > maxQueue) {
            maxQueue = v->getQueue();
        }
    }
    
    // Přiřaď o 1 vyšší prioritu (pokud možno)
    uint8_t newQueue = (maxQueue < 254) ? (maxQueue + 1) : 254;
    voice->setQueue(newQueue);
}

/**
 * @brief Sníží prioritu voice (pro inactive voices)
 * Neaktivní voices dostávají nejnižší prioritu
 * @param voice Voice k degradaci
 */
void VoiceManager::demotePriority(SynthVoice* voice)
{
    if (!voice) return;
    voice->setQueue(0);
}

// === STATISTICS AND DIAGNOSTICS ===

/**
 * @brief Vrátí počet aktivních voices (Playing + Release)
 * @return Počet aktuálně aktivních voices
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
 * @brief Vrátí počet voices v každém stavu
 * @return Struct s počty pro každý VoiceState
 */
VoiceManager::VoiceStateCounts VoiceManager::getVoiceStateCounts() const
{
    VoiceStateCounts counts;
    
    for (const auto& voice : voices_) {
        if (voice->isInactive()) {
            counts.inactive++;
        } else if (voice->isPlaying()) {
            counts.playing++;
        } else if (voice->isInRelease()) {
            counts.release++;
        }
    }
    
    return counts;
}

/**
 * @brief Vrátí počet voices podle dynamic levelů
 * @return Array s počty voices pro každý dynamic level (0-7)
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
 * @brief Vrátí detailní statistiky voice manageru
 * @return Kopie posledních statistik
 */
VoiceManager::VoiceStats VoiceManager::getVoiceStats() const
{
    return lastStats_;
}

/**
 * @brief Aktualizuje interní statistiky
 * Volá se z refresh() pro cachování výsledků.
 */
void VoiceManager::updateStatistics() const
{
    VoiceStateCounts stateCounts = getVoiceStateCounts();
    
    lastStats_.totalVoices = static_cast<int>(voices_.size());
    lastStats_.inactiveVoices = stateCounts.inactive;
    lastStats_.playingVoices = stateCounts.playing;
    lastStats_.releaseVoices = stateCounts.release;
    lastStats_.activeVoices = lastStats_.playingVoices + lastStats_.releaseVoices;
    
    lastStats_.dynamicLevelCount = getVoiceCountByDynamicLevel();
    lastStats_.voicesStolenThisRefresh = voicesStolenSinceLastRefresh_;
    lastStats_.releaseVoicesStolen = releaseVoicesStolen_;
    lastStats_.playingVoicesStolen = playingVoicesStolen_;
    
    // Průměrný progress aktivních voices
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
 * @brief Loguje periodický status (každých PERIODIC_LOG_INTERVAL refresh cyklů)
 * Poskytuje přehled o stavu voice manageru pro debugging.
 */
void VoiceManager::logPeriodicStatus()
{
    const auto& stats = lastStats_;
    
    // Sestavení informací o dynamic levelech
    juce::String dynamicLevelInfo;
    for (int i = 0; i < 8; ++i) {
        if (stats.dynamicLevelCount[i] > 0) {
            dynamicLevelInfo += "L" + juce::String(i) + ":" + juce::String(stats.dynamicLevelCount[i]) + " ";
        }
    }
    
    if (dynamicLevelInfo.isEmpty()) {
        dynamicLevelInfo = "none";
    }
    
    // Sestavení stealing statistics
    juce::String stealingInfo;
    if (stats.releaseVoicesStolen > 0 || stats.playingVoicesStolen > 0) {
        stealingInfo = " stealing(release:" + juce::String(stats.releaseVoicesStolen) + 
                      " playing:" + juce::String(stats.playingVoicesStolen) + ")";
    }
    
    logger_.log("VoiceManager/periodicStatus", "info",
               "Voices: " + juce::String(stats.activeVoices) + "/" + juce::String(stats.totalVoices) + 
               " active (playing:" + juce::String(stats.playingVoices) + 
               " release:" + juce::String(stats.releaseVoices) + 
               " inactive:" + juce::String(stats.inactiveVoices) + ")" +
               " avg_progress:" + juce::String(stats.averageProgress * 100.0f, 1) + "%" +
               " levels:" + dynamicLevelInfo + stealingInfo);
}