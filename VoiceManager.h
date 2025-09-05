#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "SampleLibrary.h"
#include "MidiStateManager.h"
#include "Logger.h"

// #define DEBUG_VOICE_MANAGER 1

/**
 * @enum VoiceState
 * @brief Definuje možné stavy voice pro release counter systém
 * 
 * Minimalistická varianta:
 * - Inactive: Voice není použita, dostupná pro alokaci
 * - Playing: Voice aktivně hraje po note-on
 * - Release: Voice v release fázi s časovým counterem (bez fade-out)
 */
enum class VoiceState {
    Inactive,    // Voice není použita - dostupná pro alokaci
    Playing,     // Aktivně hraje po note-on
    Release      // V release fázi s counterem (bez fade-out)
};

/**
 * @class SynthVoice
 * @brief Minimalistická voice s release counter mechanismem
 * 
 * Klíčové vlastnosti:
 * - Stavový automat (Inactive/Playing/Release)
 * - Release counter pro automatické ukončení po note-off
 * - Žádný fade-out - jen časový limit
 * - Jednoduchá implementace pro rychlé testování
 */
class SynthVoice
{
public:
    SynthVoice();

    /**
     * @brief Spustí voice s automatickým výběrem dynamic levelu
     * Přechod: Inactive/Release → Playing
     * @param midiNote MIDI nota (21-108)
     * @param velocity Velocity (automaticky mapováno na dynamic level)
     * @param library Reference na SampleLibrary
     */
    void start(uint8_t midiNote, uint8_t velocity, const SampleLibrary& library);

    /**
     * @brief Spustí release fázi s časovým counterem
     * Přechod: Playing → Release
     * Voice bude automaticky ukončena po RELEASE_DURATION_SAMPLES
     */
    void startRelease();

    /**
     * @brief Okamžitě zastaví voice
     * Přechod: Playing/Release → Inactive
     */
    void stop();

    /**
     * @brief Resetuje voice do výchozího stavu
     * Přechod: Any → Inactive
     */
    void reset();

    /**
     * @brief Renderuje audio s release counter management
     * Automaticky počítá release counter a ukončuje voice po vypršení
     * @param outputBuffer Ukazatel na buffer
     * @param numSamples Počet samples k renderování
     * @param isStereo Zda je output buffer stereo
     */
    void render(float* outputBuffer, int numSamples, bool isStereo = false);

    // === State Query Methods ===
    
    /**
     * @brief Vrací true pokud voice je nějakým způsobem aktivní
     * @return true pro Playing nebo Release state
     */
    bool isActive() const { return voiceState_ != VoiceState::Inactive; }
    
    /**
     * @brief Vrací true pokud voice aktivně hraje
     * @return true pouze pro Playing state
     */
    bool isPlaying() const { return voiceState_ == VoiceState::Playing; }
    
    /**
     * @brief Vrací true pokud voice je v release fázi
     * @return true pouze pro Release state
     */
    bool isInRelease() const { return voiceState_ == VoiceState::Release; }
    
    /**
     * @brief Vrací true pokud voice je neaktivní
     * @return true pouze pro Inactive state
     */
    bool isInactive() const { return voiceState_ == VoiceState::Inactive; }

    // === Standard Getters ===
    
    uint8_t getNote() const { return midiNote_; }
    uint8_t getVelocity() const { return velocity_; }
    uint8_t getDynamicLevel() const { return currentDynamicLevel_; }
    uint8_t getQueue() const { return queue_; }
    uint32_t getPosition() const { return position_; }
    uint32_t getSampleLength() const { return currentSampleLength_; }
    VoiceState getVoiceState() const { return voiceState_; }
    
    void setQueue(uint8_t queue) { queue_ = queue; }

    /**
     * @brief Vrátí progress jako procenta (0.0-1.0)
     * Používá se pro voice stealing prioritu
     */
    float getProgress() const {
        return (currentSampleLength_ > 0) ? 
               static_cast<float>(position_) / static_cast<float>(currentSampleLength_) : 0.0f;
    }

    /**
     * @brief Vrátí zbývající samples v release counteru
     * Pro debugging účely
     */
    uint32_t getReleaseCounterRemaining() const {
        return (releaseCounter_ < RELEASE_DURATION_SAMPLES) ? 
               (RELEASE_DURATION_SAMPLES - releaseCounter_) : 0;
    }

private:
    Logger& logger_;                    // Reference na logger

    // === Voice State Management ===
    VoiceState voiceState_{VoiceState::Inactive};  // Stavový automat
    uint8_t midiNote_{0};              // Aktuální MIDI nota
    uint8_t velocity_{0};              // Velocity
    uint8_t currentDynamicLevel_{0};   // Aktuální dynamic level (0-7)

    // === Sample Data ===
    const float* sampleData_{nullptr}; // Ukazatel na data vzorku
    uint32_t currentSampleLength_{0};  // Délka aktuálního sample
    uint32_t position_{0};             // Aktuální pozice v vzorku
    bool currentSampleIsStereo_{false}; // Zda je aktuální sample stereo

    // === Voice Management ===
    uint8_t queue_{0};                 // Prioritní queue pro stealing

    // === Release Counter Mechanism ===
    uint32_t releaseCounter_{0};       // Counter pro release fázi
    
public:
    static constexpr uint32_t RELEASE_DURATION_SAMPLES = 4800;  // 100ms @ 48kHz
    
private:
    static constexpr double ASSUMED_SAMPLE_RATE = 48000.0;      // Pro výpočty

    /**
     * @brief Najde nejlepší dostupný dynamic level pro danou notu
     * Implementuje spiral search s fallback logikou
     * @param library Reference na SampleLibrary
     * @param midiNote MIDI nota
     * @param preferredLevel Preferovaný level z velocity mapping
     * @return Nejlepší dostupný level nebo 255 pokud žádný
     */
    uint8_t findBestAvailableLevel(const SampleLibrary& library, uint8_t midiNote, uint8_t preferredLevel);
};

/**
 * @class VoiceManager
 * @brief Voice manager s minimalistickým release counter systémem
 * 
 * Klíčové vlastnosti pro rychlé testování:
 * - Release counter místo ADSR envelope
 * - Intelligent voice stealing (preferuje release voices)
 * - Note restart detection (monofonie per nota)
 * - Dynamic level fallback mechanism
 * - Jednoduché automatické ukončení voices
 */
class VoiceManager
{
public:
    /**
     * @brief Konstruktor s referencí na SampleLibrary
     * @param library Reference na SampleLibrary (musí být platná po celou dobu života)
     * @param numVoices Počet voices (výchozí 16)
     */
    VoiceManager(const SampleLibrary& library, int numVoices = 16);

    ~VoiceManager() = default;

    /**
     * @brief Zpracuje MIDI události z MidiStateManager
     * Hlavní vstupní bod pro MIDI data
     * @param midiState Reference na MidiStateManager
     */
    void processMidiEvents(MidiStateManager& midiState);

    /**
     * @brief Generuje audio mixem všech aktivních voices
     * @param buffer Ukazatel na audio buffer (stereo interleaved)
     * @param numSamples Počet samples per channel
     */
    void generateAudio(float* buffer, int numSamples);

    /**
     * @brief Housekeeping a statistiky
     * Pravidelně volaná údržba
     */
    void refresh();

    // === Statistics & Diagnostics ===
    
    /**
     * @brief Vrátí počet aktivních voices (Playing + Release)
     */
    int getActiveVoiceCount() const;
    
    /**
     * @brief Vrátí počet voices v každém stavu
     */
    struct VoiceStateCounts {
        int inactive;
        int playing; 
        int release;
        
        VoiceStateCounts() : inactive(0), playing(0), release(0) {}
    };
    
    VoiceStateCounts getVoiceStateCounts() const;
    
    /**
     * @brief Vrátí počet voices podle dynamic levelů
     */
    std::array<int, 8> getVoiceCountByDynamicLevel() const;
    
    /**
     * @brief Minimalistické voice statistics
     */
    struct VoiceStats {
        int totalVoices;
        int activeVoices;              // Playing + Release
        int playingVoices;             // Pouze Playing
        int releaseVoices;             // Pouze Release
        int inactiveVoices;            // Inactive
        std::array<int, 8> dynamicLevelCount;
        float averageProgress;
        int voicesStolenThisRefresh;
        int releaseVoicesStolen;       // Kolik release voices bylo ukradeno
        int playingVoicesStolen;       // Kolik playing voices bylo ukradeno
        
        VoiceStats() : totalVoices(0), activeVoices(0), playingVoices(0), 
                       releaseVoices(0), inactiveVoices(0), dynamicLevelCount{}, 
                       averageProgress(0.0f), voicesStolenThisRefresh(0),
                       releaseVoicesStolen(0), playingVoicesStolen(0) {}
    };
    
    VoiceStats getVoiceStats() const;

private:
    Logger& logger_;                                        // Reference na logger
    const SampleLibrary& sampleLibrary_;                   // Reference na vzorky
    std::vector<std::unique_ptr<SynthVoice>> voices_;      // Pool voices
    
    // === Statistics ===
    mutable VoiceStats lastStats_;                         // Cache posledních statistik
    int voicesStolenSinceLastRefresh_{0};                  // Voice stealing counter
    int releaseVoicesStolen_{0};                           // Release voices stolen counter
    int playingVoicesStolen_{0};                           // Playing voices stolen counter
    
    /**
     * @brief Spustí voice s comprehensive allocation strategy
     * 
     * ALGORITMUS ALOKACE (v pořadí priority):
     * 1. Note restart detection - stejná nota na stejném voice
     * 2. Free voice allocation - najdi neaktivní voice
     * 3. Release voice stealing - ukradni nejstarší release voice
     * 4. Playing voice stealing - ukradni nejstarší playing voice (last resort)
     * 
     * @param midiNote MIDI nota
     * @param velocity Velocity
     */
    void startVoice(uint8_t midiNote, uint8_t velocity);

    /**
     * @brief Spustí release counter pro danou notu
     * Přechod Playing → Release (spustí časový counter)
     * @param midiNote MIDI nota k release
     */
    void stopVoice(uint8_t midiNote);

    // === Voice Allocation Helpers ===
    
    /**
     * @brief Najde voice již hrající specifickou notu
     * Pro note restart detection
     * @param midiNote Nota k vyhledání
     * @return Voice hrající tuto notu nebo nullptr
     */
    SynthVoice* findVoicePlayingNote(uint8_t midiNote);
    
    /**
     * @brief Najde nejlepší neaktivní voice
     * @return Nejlepší free voice nebo nullptr
     */
    SynthVoice* findBestFreeVoice();
    
    /**
     * @brief Najde nejlepší release voice pro stealing
     * Preferuje nejstarší release voices
     * @return Nejlepší release candidate nebo nullptr
     */
    SynthVoice* findBestReleaseCandidate();
    
    /**
     * @brief Najde nejlepší playing voice pro stealing (last resort)
     * Preferuje nejstarší + nejvyšší progress
     * @return Nejlepší playing candidate nebo nullptr
     */
    SynthVoice* findBestPlayingCandidate();

    // === Priority Management ===
    
    /**
     * @brief Přiřadí voice top prioritu (pro nově spuštěné)
     * @param voice Voice k povýšení
     */
    void assignTopPriority(SynthVoice* voice);
    
    /**
     * @brief Sníží prioritu voice (pro inactive voices)
     * @param voice Voice k degradaci
     */
    void demotePriority(SynthVoice* voice);
    
    /**
     * @brief Aktualizuje statistiky (volané z refresh())
     */
    void updateStatistics() const;
    
    /**
     * @brief Loguje periodic status
     */
    void logPeriodicStatus();
    
    // === Constants ===
    static constexpr int PERIODIC_LOG_INTERVAL = 1000;    // Log každých 1000 refresh cyklů
    mutable int refreshCounter_{0};                        // Počítadlo refresh cyklů
};