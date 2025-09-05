#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "SampleLibrary.h"
#include "MidiStateManager.h"
#include "Logger.h"

// #define DEBUG_VOICE_MANAGER 1

/**
 * @class SynthVoice
 * @brief Rozšířený voice renderer s podporou dynamic levels
 * 
 * Nové vlastnosti:
 * - Dynamic level selection based on velocity
 * - Variable sample lengths per dynamic level
 * - No real-time gain calculation (pre-computed in samples)
 * - Improved voice lifecycle management
 */
class SynthVoice
{
public:
    SynthVoice();

    /**
     * @brief Spustí hlas s automatickým výběrem dynamic levelu
     * @param midiNote MIDI nota
     * @param velocity Velocity (automaticky mapováno na dynamic level)
     * @param library Reference na SampleLibrary
     */
    void start(uint8_t midiNote, uint8_t velocity, const SampleLibrary& library);

    /**
     * @brief Zastaví hlas (deaktivuje ho)
     */
    void stop();

    /**
     * @brief Resetuje hlas do výchozího stavu
     */
    void reset();

    /**
     * @brief Renderuje audio bez real-time gain (pre-computed v samples)
     * @param outputBuffer Ukazatel na buffer
     * @param numSamples Počet samplů k renderování
     * @param isStereo Zda je output buffer stereo (pro stereo rendering)
     */
    void render(float* outputBuffer, int numSamples, bool isStereo = false);

    // === Gettery ===
    
    bool isActive() const { return isActive_; }
    uint8_t getNote() const { return midiNote_; }
    uint8_t getVelocity() const { return velocity_; }
    uint8_t getDynamicLevel() const { return currentDynamicLevel_; }
    uint8_t getQueue() const { return queue_; }
    uint32_t getPosition() const { return position_; }
    uint32_t getSampleLength() const { return currentSampleLength_; }
    
    void setQueue(uint8_t queue) { queue_ = queue; }

    /**
     * @brief Vrátí progress jako procenta (0.0-1.0)
     */
    float getProgress() const {
        return (currentSampleLength_ > 0) ? 
               static_cast<float>(position_) / static_cast<float>(currentSampleLength_) : 0.0f;
    }

private:
    Logger& logger_;                    // Reference na logger

    // === Voice State ===
    uint8_t midiNote_{0};              // Aktuální MIDI nota
    uint8_t velocity_{0};              // Velocity
    uint8_t currentDynamicLevel_{0};   // Aktuální dynamic level (0-7)
    bool isActive_{false};             // Stav aktivity

    // === Sample Data ===
    const float* sampleData_{nullptr}; // Ukazatel na data vzorku
    uint32_t currentSampleLength_{0};  // Délka aktuálního sample (variable per dynamic level)
    uint32_t position_{0};             // Aktuální pozice v vzorku
    bool currentSampleIsStereo_{false}; // Zda je aktuální sample stereo

    // === Voice Management ===
    uint8_t queue_{0};                 // Prioritní queue pro stealing (0 = dno, vyšší = top)
    
    /**
     * @brief Najde nejlepší dostupný dynamic level pro danou notu
     * @param library Reference na SampleLibrary
     * @param midiNote MIDI nota
     * @param preferredLevel Preferovaný level
     * @return Nejlepší dostupný level nebo 255 pokud žádný
     */
    uint8_t findBestAvailableLevel(const SampleLibrary& library, uint8_t midiNote, uint8_t preferredLevel);
};

/**
 * @class VoiceManager
 * @brief Rozšířený voice manager s podporou dynamic levels a lepší diagnostikou
 * 
 * Nové vlastnosti:
 * - Automatic dynamic level selection
 * - Enhanced voice stealing algorithm
 * - Real-time voice statistics
 * - Fallback mechanism pro missing dynamic levels
 */
class VoiceManager
{
public:
    /**
     * @brief Konstruktor s referencí na SampleLibrary
     * @param library Reference na SampleLibrary
     * @param numVoices Počet hlasů (výchozí 16)
     */
    VoiceManager(const SampleLibrary& library, int numVoices = 16);

    ~VoiceManager() = default;

    /**
     * @brief Zpracuje MIDI události z MidiStateManager (note-on/off)
     * @param midiState Reference na MidiStateManager
     */
    void processMidiEvents(MidiStateManager& midiState);

    /**
     * @brief Generuje audio mixem všech aktivních hlasů
     * @param buffer Ukazatel na audio buffer
     * @param numSamples Počet samplů
     */
    void generateAudio(float* buffer, int numSamples);

    /**
     * @brief Housekeeping a statistiky
     */
    void refresh();

    // === Statistics & Diagnostics ===
    
    /**
     * @brief Vrátí počet aktivních hlasů
     */
    int getActiveVoiceCount() const;
    
    /**
     * @brief Vrátí počet hlasů podle dynamic levelů
     */
    std::array<int, 8> getVoiceCountByDynamicLevel() const;
    
    /**
     * @brief Vrátí statistiky voice usage
     */
    struct VoiceStats {
        int totalVoices;
        int activeVoices;
        int inactiveVoices;
        std::array<int, 8> dynamicLevelCount;
        float averageProgress;
        int voicesStolenThisRefresh;
        
        VoiceStats() : totalVoices(0), activeVoices(0), inactiveVoices(0), 
                       dynamicLevelCount{}, averageProgress(0.0f), voicesStolenThisRefresh(0) {}
    };
    
    VoiceStats getVoiceStats() const;

private:
    Logger& logger_;                                        // Reference na logger
    const SampleLibrary& sampleLibrary_;                   // Povinná reference na vzorky
    std::vector<std::unique_ptr<SynthVoice>> voices_;      // Kolekce hlasů
    
    // === Statistics ===
    mutable VoiceStats lastStats_;                         // Cache posledních statistik
    int voicesStolenSinceLastRefresh_{0};                  // Počítadlo voice stealing
    
    /**
     * @brief Spustí hlas pro danou notu s automatic dynamic level selection
     * @param midiNote MIDI nota
     * @param velocity Velocity
     */
    void startVoice(uint8_t midiNote, uint8_t velocity);

    /**
     * @brief Zastaví hlas pro danou notu
     * @param midiNote MIDI nota
     */
    void stopVoice(uint8_t midiNote);

    /**
     * @brief Přeuspořádá queue priorit (Enhanced version)
     * @param queueNumber Číslo queue k mixlování
     */
    void mixleQueue(uint8_t queueNumber);
    
    /**
     * @brief Najde nejlepší kandidát pro voice stealing
     * @return Ukazatel na voice nebo nullptr
     */
    SynthVoice* findVoiceStealingCandidate();
    
    /**
     * @brief Aktualizuje statistiky (volané z refresh())
     */
    void updateStatistics() const;
    
    /**
     * @brief Loguje periodic voice status (každých N refresh cyklů)
     */
    void logPeriodicStatus();
    
    // === Constants ===
    static constexpr int PERIODIC_LOG_INTERVAL = 1000;    // Log každých 1000 refresh cyklů
    mutable int refreshCounter_{0};                        // Počítadlo refresh cyklů
};