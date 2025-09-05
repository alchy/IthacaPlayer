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
    // Reset předchozího stavu (důležité při restart stejné noty)
    reset();

    midiNote_ = midiNote;
    velocity_ = velocity;
    voiceState_ = VoiceState::Playing;  // Přechod do Playing state

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
    releaseCounter_ = 0;  // Reset release counter

    logger_.log("SynthVoice/start", "debug", 
               "Voice started: note=" + juce::String((int)midiNote) + 
               " vel=" + juce::String((int)velocity) +
               " level=" + juce::String((int)currentDynamicLevel_) +
               " length=" + juce::String(currentSampleLength_) +
               " stereo=" + juce::String(currentSampleIsStereo_ ? "yes" : "no"));
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
        releaseCounter_ = 0;  // Reset release counter na začátek
        
        // Výpočet release doby pro informaci
        double releaseTimeMs = (static_cast<double>(RELEASE_DURATION_SAMPLES) / ASSUMED_SAMPLE_RATE) * 1000.0;
        
        logger_.log("SynthVoice/startRelease", "debug", 
                   "Voice entering release phase: note=" + juce::String((int)midiNote_) +
                   " duration=" + juce::String(RELEASE_DURATION_SAMPLES) + " samples" +
                   " (" + juce::String(releaseTimeMs, 1) + "ms)");
    }
}

/**
 * @brief Okamžitě zastaví voice
 * Přechod state: Playing/Release → Inactive
 */
void SynthVoice::stop()
{
    voiceState_ = VoiceState::Inactive;
    logger_.log("SynthVoice/stop", "debug", 
               "Voice stopped immediately: note=" + juce::String((int)midiNote_));
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
    releaseCounter_ = 0;  // Reset release counter
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
    // Renderuj pouze pokud voice je aktivní (Playing nebo Release)
    if (!isActive() || sampleData_ == nullptr || currentSampleLength_ == 0 || !outputBuffer)
        return;

    // Hlavní rendering loop s release counter management
    for (int i = 0; i < numSamples; ++i) {
        // === KONTROLA UKONČENÍ ===
        bool shouldStop = false;
        juce::String stopReason;
        
        // Kontrola konce sample (platí pro Playing i Release)
        if (position_ >= currentSampleLength_) {
            shouldStop = true;
            stopReason = "end of sample";
        }
        
        // === RELEASE COUNTER MANAGEMENT ===
        if (voiceState_ == VoiceState::Release) {
            releaseCounter_++;
            
            // Kontrola vypršení release času
            if (releaseCounter_ >= RELEASE_DURATION_SAMPLES) {
                shouldStop = true;
                stopReason = "release timer expired";
            }
        }
        
        // Ukončení voice pokud je potřeba
        if (shouldStop) {
            voiceState_ = VoiceState::Inactive;
            logger_.log("SynthVoice/render", "debug", 
                       "Voice auto-stopped: note=" + juce::String((int)midiNote_) + 
                       " reason=" + stopReason + 
                       " position=" + juce::String(position_) + "/" + juce::String(currentSampleLength_) +
                       " release_counter=" + juce::String(releaseCounter_));
            break;
        }
        
        // === AUDIO RENDERING (bez fade-out - minimalistická varianta) ===
        // Poznámka: V budoucnu zde bude aplikace ADSR envelope gain
        
        if (isStereo) {
            // === STEREO OUTPUT ===
            if (currentSampleIsStereo_) {
                // Stereo sample → stereo output (přímé kopírování)
                outputBuffer[i * 2] += sampleData_[position_ * 2];         // Left channel
                outputBuffer[i * 2 + 1] += sampleData_[position_ * 2 + 1]; // Right channel
            } else {
                // Mono sample → stereo output (duplikace na oba kanály)
                float sample = sampleData_[position_];
                outputBuffer[i * 2] += sample;     // Left channel
                outputBuffer[i * 2 + 1] += sample; // Right channel
            }
        } else {
            // === MONO OUTPUT ===
            if (currentSampleIsStereo_) {
                // Stereo sample → mono output (mix L+R s -3dB kompenzací)
                float left = sampleData_[position_ * 2];
                float right = sampleData_[position_ * 2 + 1];
                outputBuffer[i] += (left + right) * 0.5f;
            } else {
                // Mono sample → mono output (přímé kopírování)
                outputBuffer[i] += sampleData_[position_];
            }
        }
        
        ++position_;
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
                logger_.log("SynthVoice/findBestAvailableLevel", "debug",
                           "Fallback to lower level " + juce::String((int)lowerLevel) + 
                           " instead of " + juce::String((int)preferredLevel));
                return lowerLevel;
            }
        }
        
        // Zkus vyšší level (tvrdší dynamika)
        uint8_t higherLevel = static_cast<uint8_t>(preferredLevel + offset);
        if (higherLevel < 8 && library.isNoteAvailable(midiNote, higherLevel)) {
            logger_.log("SynthVoice/findBestAvailableLevel", "debug",
                       "Fallback to higher level " + juce::String((int)higherLevel) + 
                       " instead of " + juce::String((int)preferredLevel));
            return higherLevel;
        }
    }

    // 3. Žádný level není dostupný
    logger_.log("SynthVoice/findBestAvailableLevel", "error",
               "No dynamic level available for note " + juce::String((int)midiNote));
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
        voices_.back()->setQueue(0);  // Všechny voices začínají s nejnižší prioritou
    }

    // Reset statistik
    lastStats_ = VoiceStats();
    voicesStolenSinceLastRefresh_ = 0;
    releaseVoicesStolen_ = 0;
    playingVoicesStolen_ = 0;
    refreshCounter_ = 0;

    logger_.log("VoiceManager/constructor", "info", 
               "Minimalist VoiceManager created with " + juce::String(numVoices) + 
               " voices and release counter system");
}

/**
 * @brief Zpracuje MIDI události z MidiStateManager
 * Hlavní vstupní bod pro MIDI data - zpracovává note-on a note-off události.
 * @param midiState Reference na MidiStateManager s MIDI frontami
 */
void VoiceManager::processMidiEvents(MidiStateManager& midiState)
{
    int totalNotesProcessed = 0;

    // === ZPRACOVÁNÍ NOTE ON UDÁLOSTÍ ===
    for (int ch = 0; ch < 16; ++ch) {
        while (true) {
            uint8_t rawNote = midiState.popNoteOn(static_cast<uint8_t>(ch));
            if (rawNote == 255) break;  // Prázdná fronta
            
            uint8_t note = rawNote;
            uint8_t velocity = midiState.getVelocity(static_cast<uint8_t>(ch), note);
            
            // Validace MIDI dat
            if (velocity == 0) {
                // Velocity 0 je note-off
                stopVoice(note);
            } else {
                startVoice(note, velocity);
                totalNotesProcessed++;
            }
        }
    }

    // === ZPRACOVÁNÍ NOTE OFF UDÁLOSTÍ ===
    for (int ch = 0; ch < 16; ++ch) {
        while (true) {
            uint8_t rawNote = midiState.popNoteOff(static_cast<uint8_t>(ch));
            if (rawNote == 255) break;  // Prázdná fronta
            
            uint8_t note = rawNote;
            stopVoice(note);  // Spustí release counter
            totalNotesProcessed++;
        }
    }

    // Logování pouze při zpracování událostí
    if (totalNotesProcessed > 0) {
        logger_.log("VoiceManager/processMidiEvents", "debug", 
                   "Processed " + juce::String(totalNotesProcessed) + " MIDI events");
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
    // Základní validace
    if (buffer == nullptr || numSamples <= 0) {
        logger_.log("VoiceManager/generateAudio", "warn", "Invalid buffer or sample count");
        return;
    }

    // Předpokládáme stereo output (standard pro plugin)
    bool isStereoOutput = true;
    int activeVoiceCount = 0;
    int playingCount = 0;
    int releaseCount = 0;
    
    // Mix všech aktivních voices (Playing + Release)
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

    // Logování pouze při změně stavu
    static int lastActiveCount = 0;
    static int lastPlayingCount = 0;
    static int lastReleaseCount = 0;
    
    if (activeVoiceCount != lastActiveCount || 
        playingCount != lastPlayingCount || 
        releaseCount != lastReleaseCount) {
        
        logger_.log("VoiceManager/generateAudio", "debug", 
                   "Active voices: " + juce::String(activeVoiceCount) + 
                   " (playing: " + juce::String(playingCount) + 
                   ", release: " + juce::String(releaseCount) + ")");
        
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
    logger_.log("VoiceManager/startVoice", "debug", 
               "=== VOICE ALLOCATION START: note=" + juce::String((int)midiNote) + 
               " vel=" + juce::String((int)velocity) + " ===");

    // === KROK 1: NOTE RESTART DETECTION ===
    // Stejná nota musí být vždy na stejném voice (monofonie per nota)
    // Důvod: Přirozené chování syntezátoru + prevent phase issues
    SynthVoice* existingVoice = findVoicePlayingNote(midiNote);
    if (existingVoice) {
        logger_.log("VoiceManager/startVoice", "debug", 
                   "STEP 1: Note restart detected - reusing existing voice");
        
        // RESTART existující voice s novou velocity
        existingVoice->start(midiNote, velocity, sampleLibrary_);
        // ZACHOVÁNÍ queue priority - voice zůstává na stejné pozici
        
        logger_.log("VoiceManager/startVoice", "debug", 
                   "SUCCESS: Restarted existing voice for note " + juce::String((int)midiNote) +
                   " with velocity " + juce::String((int)velocity));
        return;
    }

    // === KROK 2: FREE VOICE ALLOCATION ===
    // Hledáme nejlepší neaktivní voice (Inactive state)
    // Preferujeme voice s nejnižší prioritou (nejdéle nepoužitá)
    SynthVoice* freeVoice = findBestFreeVoice();
    if (freeVoice) {
        logger_.log("VoiceManager/startVoice", "debug", 
                   "STEP 2: Free voice found - allocating");
        
        freeVoice->start(midiNote, velocity, sampleLibrary_);
        assignTopPriority(freeVoice);  // Nová voice dostává top prioritu
        
        logger_.log("VoiceManager/startVoice", "debug", 
                   "SUCCESS: Allocated free voice for note " + juce::String((int)midiNote));
        return;
    }

    // === KROK 3: RELEASE VOICE STEALING ===
    // Preferujeme ukradnout voice v release fázi (dokončuje release counter)
    // Důvod: Minimální audio impact, přirozené chování
    SynthVoice* releaseVoice = findBestReleaseCandidate();
    if (releaseVoice) {
        logger_.log("VoiceManager/startVoice", "debug", 
                   "STEP 3: Release voice stealing - taking voice in release phase");
        
        uint8_t stolenNote = releaseVoice->getNote();
        uint32_t releaseRemaining = releaseVoice->getReleaseCounterRemaining();
        
        releaseVoice->start(midiNote, velocity, sampleLibrary_);
        assignTopPriority(releaseVoice);
        
        // Statistiky
        voicesStolenSinceLastRefresh_++;
        releaseVoicesStolen_++;
        
        logger_.log("VoiceManager/startVoice", "debug", 
                   "SUCCESS: Stole release voice - note " + juce::String((int)midiNote) + 
                   " replaced note " + juce::String((int)stolenNote) + 
                   " (had " + juce::String(releaseRemaining) + " samples remaining)");
        return;
    }

    // === KROK 4: PLAYING VOICE STEALING (LAST RESORT) ===
    // Krajní řešení - ukradneme aktivně hrající voice
    // Preferujeme nejstarší + nejvíce dokončený (minimální audio disruption)
    SynthVoice* playingVoice = findBestPlayingCandidate();
    if (playingVoice) {
        logger_.log("VoiceManager/startVoice", "warn", 
                   "STEP 4: Playing voice stealing - last resort activated");
        
        uint8_t stolenNote = playingVoice->getNote();
        float stolenProgress = playingVoice->getProgress();
        
        playingVoice->start(midiNote, velocity, sampleLibrary_);
        assignTopPriority(playingVoice);
        
        // Statistiky
        voicesStolenSinceLastRefresh_++;
        playingVoicesStolen_++;
        
        logger_.log("VoiceManager/startVoice", "warn", 
                   "SUCCESS: Stole playing voice - note " + juce::String((int)midiNote) + 
                   " replaced note " + juce::String((int)stolenNote) + 
                   " (was " + juce::String(stolenProgress * 100.0f, 1) + "% complete)");
        return;
    }

    // === KROK 5: ALLOCATION FAILURE ===
    // Nemělo by se nikdy stát, ale pro robustnost
    logger_.log("VoiceManager/startVoice", "error", 
               "FAILURE: Cannot allocate voice for note " + juce::String((int)midiNote) + 
               " - all allocation strategies failed!");
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
            // Spustí release counter místo immediate stop
            voice->startRelease();
            // Queue priorita se NEMĚNÍ - voice pokračuje na stejné pozici
            
            logger_.log("VoiceManager/stopVoice", "debug", 
                       "Started release counter for note " + juce::String((int)midiNote));
            return;
        }
    }
    
    // Nota nebyla nalezena - může být normální při rychlém MIDI trafficu
    logger_.log("VoiceManager/stopVoice", "debug", 
               "Note " + juce::String((int)midiNote) + " not found for release (already stopped?)");
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
        // Hledáme v Playing i Release state (obě můžou být "restarted")
        if (voice->isActive() && voice->getNote() == midiNote) {
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
    uint8_t lowestQueue = 255;  // Hledáme nejnižší prioritu (nejdéle nepoužitá)
    
    for (auto& voice : voices_) {
        if (voice->isInactive()) {
            uint8_t currentQueue = voice->getQueue();
            if (currentQueue <= lowestQueue) {
                bestCandidate = voice.get();
                lowestQueue = currentQueue;
            }
        }
    }
    
    if (bestCandidate) {
        logger_.log("VoiceManager/findBestFreeVoice", "debug", 
                   "Found free voice with queue priority " + juce::String((int)lowestQueue));
    } else {
        logger_.log("VoiceManager/findBestFreeVoice", "debug", "No free voices available");
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
    uint8_t lowestQueue = 255;          // Nejstarší = nejnižší queue
    uint32_t highestCounter = 0;        // Nejvíce pokročilý release counter
    
    for (auto& voice : voices_) {
        if (voice->isInRelease()) {
            uint8_t currentQueue = voice->getQueue();
            uint32_t releaseRemaining = voice->getReleaseCounterRemaining();
            uint32_t releaseElapsed = SynthVoice::RELEASE_DURATION_SAMPLES - releaseRemaining;
            
            // Priorita: nejnižší queue (nejstarší), při shodě nejvyšší release progress
            if (currentQueue < lowestQueue || 
                (currentQueue == lowestQueue && releaseElapsed > highestCounter)) {
                bestCandidate = voice.get();
                lowestQueue = currentQueue;
                highestCounter = releaseElapsed;
            }
        }
    }
    
    if (bestCandidate) {
        uint32_t remaining = bestCandidate->getReleaseCounterRemaining();
        logger_.log("VoiceManager/findBestReleaseCandidate", "debug", 
                   "Found release voice: queue=" + juce::String((int)lowestQueue) + 
                   " remaining=" + juce::String(remaining) + " samples");
    } else {
        logger_.log("VoiceManager/findBestReleaseCandidate", "debug", "No release voices available");
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
    uint8_t lowestQueue = 255;      // Nejstarší = nejnižší queue
    float highestProgress = 0.0f;   // Nejvíce dokončený = nejmenší disruption
    
    for (auto& voice : voices_) {
        if (voice->isPlaying()) {
            uint8_t currentQueue = voice->getQueue();
            float currentProgress = voice->getProgress();
            
            // Priorita: nejnižší queue (nejstarší), při shodě nejvyšší progress
            if (currentQueue < lowestQueue || 
                (currentQueue == lowestQueue && currentProgress > highestProgress)) {
                bestCandidate = voice.get();
                lowestQueue = currentQueue;
                highestProgress = currentProgress;
            }
        }
    }
    
    if (bestCandidate) {
        logger_.log("VoiceManager/findBestPlayingCandidate", "warn", 
                   "Found playing voice for stealing: queue=" + juce::String((int)lowestQueue) + 
                   " progress=" + juce::String(highestProgress * 100.0f, 1) + "% (DISRUPTIVE!)");
    } else {
        logger_.log("VoiceManager/findBestPlayingCandidate", "error", "No playing voices found - this should not happen!");
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
    
    logger_.log("VoiceManager/assignTopPriority", "debug", 
               "Assigned top priority: queue=" + juce::String((int)newQueue));
}

/**
 * @brief Sníží prioritu voice (pro inactive voices)
 * Neaktivní voices dostávají nejnižší prioritu
 * @param voice Voice k degradaci
 */
void VoiceManager::demotePriority(SynthVoice* voice)
{
    if (!voice) return;
    
    voice->setQueue(0);  // Nejnižší priorita pro neaktivní voices
    
    logger_.log("VoiceManager/demotePriority", "debug", "Voice demoted to lowest priority");
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