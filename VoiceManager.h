#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "SampleLibrary.h"
#include "MidiStateManager.h"
#include "Logger.h"

/**
 * @class SynthVoice
 * @brief Jednoduchý renderer pro jeden hlas (voice), drží data vzorku a pozici.
 * 
 * Renderuje audio z readonly dat SampleLibrary. Podporuje start/stop/reset a rendering do bufferu.
 * Nově přidána queue pro voice stealing (priorita: vyšší = novější).
 */
class SynthVoice
{
public:
    SynthVoice();

    /**
     * @brief Spustí hlas s danou notou a velocity.
     * @param midiNote MIDI nota
     * @param velocity Velocity (0-127)
     * @param library Reference na SampleLibrary
     */
    void start(uint8_t midiNote, uint8_t velocity, const SampleLibrary& library);

    /**
     * @brief Zastaví hlas (deaktivuje ho).
     */
    void stop();

    /**
     * @brief Resetuje hlas do výchozího stavu.
     */
    void reset();

    /**
     * @brief Renderuje audio do bufferu (přičítá k existujícím datům).
     * @param outputBuffer Ukazatel na buffer
     * @param numSamples Počet samplů k renderování
     */
    void render(float* outputBuffer, int numSamples);

    bool isActive() const { return isActive_; }  // Vrátí, zda je hlas aktivní
    uint8_t getNote() const { return midiNote_; }  // Vrátí aktuální notu
    uint8_t getQueue() const { return queue_; }  // Vrátí prioritu queue
    void setQueue(uint8_t queue) { queue_ = queue; }  // Nastaví prioritu queue

private:
    Logger& logger_;  // Reference na logger

    uint8_t midiNote_{0};  // Aktuální MIDI nota
    uint8_t velocity_{0};  // Velocity
    bool isActive_{false};  // Stav aktivity

    const float* sampleData_{nullptr};  // Ukazatel na data vzorku
    uint32_t sampleLength_{0};  // Délka vzorku
    uint32_t position_{0};  // Aktuální pozice v vzorku

    uint8_t queue_{0};  // Prioritní queue pro stealing (0 = dno, vyšší = top)
};

/**
 * @class VoiceManager
 * @brief Spravuje kolekci hlasů (voices), zpracovává MIDI události a generuje audio.
 * 
 * Vyžaduje SampleLibrary. Podporuje voice stealing inspirovaný HW syntetizérem (s queue prioritou).
 * Procesuje MIDI z MidiStateManager a mixuje audio z hlasů.
 */
class VoiceManager
{
public:
    /**
     * @brief Konstruktor s referencí na SampleLibrary.
     * @param library Reference na SampleLibrary
     * @param numVoices Počet hlasů (výchozí 16)
     */
    VoiceManager(const SampleLibrary& library, int numVoices = 16);

    ~VoiceManager() = default;

    /**
     * @brief Zpracuje MIDI události z MidiStateManager (note-on/off).
     * @param midiState Reference na MidiStateManager
     */
    void processMidiEvents(MidiStateManager& midiState);

    /**
     * @brief Generuje audio mixem všech aktivních hlasů.
     * @param buffer Ukazatel na audio buffer
     * @param numSamples Počet samplů
     */
    void generateAudio(float* buffer, int numSamples);

    /**
     * @brief Housekeeping: Může být rozšířeno (aktuálně prázdné).
     */
    void refresh();

    /**
     * @brief Vrátí počet aktivních hlasů (pro debug a monitoring).
     * @return Počet aktivních hlasů
     */
    int getActiveVoiceCount() const;

private:
    Logger& logger_;  // Reference na logger
    const SampleLibrary& sampleLibrary_;  // Povinná reference na vzorky
    std::vector<std::unique_ptr<SynthVoice>> voices_;  // Kolekce hlasů

    /**
     * @brief Spustí hlas pro danou notu (s voice stealingem).
     * @param midiNote MIDI nota
     * @param velocity Velocity
     */
    void startVoice(uint8_t midiNote, uint8_t velocity);

    /**
     * @brief Zastaví hlas pro danou notu.
     * @param midiNote MIDI nota
     */
    void stopVoice(uint8_t midiNote);

    /**
     * @brief Přeuspořádá queue priorit (posune vybranou na dno, ostatní posune).
     * @param queueNumber Číslo queue k mixlování
     */
    void mixleQueue(uint8_t queueNumber);
};