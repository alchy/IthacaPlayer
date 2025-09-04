#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <mutex>
#include "Logger.h"

// Konstanty pro MIDI rozsahy
constexpr uint8_t MIDI_NOTES = 128;  // Standardní počet MIDI not (0-127)
constexpr uint8_t MIDI_CHANNELS = 16;  // Standardní počet MIDI kanálů

/**
 * @class MidiStateManager
 * @brief Spravuje stav MIDI zpráv, včetně aktivních not, velocity, controllerů a queue pro note-on/off.
 * 
 * Tato třída je thread-safe díky mutexům a atomic proměnným. Poskytuje metody pro push/pop MIDI událostí,
 * sledování aktivních not a controller hodnot. Inicializuje výchozí hodnoty controllerů podle MIDI standardu.
 */
class MidiStateManager
{
public:
    MidiStateManager();

    // Metody pro note-on/off
    void pushNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);  // Přidá note-on do queue a aktualizuje stav
    void pushNoteOff(uint8_t channel, uint8_t note);  // Přidá note-off do queue a aktualizuje stav
    uint8_t popNoteOn(uint8_t channel);  // 🔧 Změna: Změněno na uint8_t, vrací 255 pokud prázdná (pro odstranění warningu C4244)
    uint8_t popNoteOff(uint8_t channel);  // 🔧 Změna: Změněno na uint8_t, vrací 255 pokud prázdná

    // Metody pro aktivní noty
    bool isNoteActive(uint8_t channel, uint8_t note) const;  // Zkontroluje, zda je nota aktivní
    uint8_t getVelocity(uint8_t channel, uint8_t note) const;  // Vrátí velocity aktivní noty

    // Metody pro MIDI controllery
    void setControllerValue(uint8_t channel, uint8_t controller, uint8_t value);  // Nastaví hodnotu controlleru
    uint8_t getControllerValue(uint8_t channel, uint8_t controller) const;  // Vrátí hodnotu controlleru

private:
    Logger& logger_;  // Reference na logger pro logování událostí

    // Struktura pro circular buffer queue (zachována pro efektivitu a thread-safety)
    struct NoteQueue {
        std::array<uint8_t, 256> notes;  // Circular buffer o velikosti 256 (magické číslo zachováno)
        std::atomic<uint8_t> writeIndex{0};  // Atomic index pro zápis (thread-safe)
        std::atomic<uint8_t> count{0};  // Počet prvků v queue (atomic pro bezpečný přístup)
        uint8_t readIndex{0};  // Index pro čtení (chráněn mutexem)
        mutable std::mutex mutex;  // Mutex pro synchronizaci přístupu

        void reset();  // Resetuje queue na výchozí stav
    };

    // Queue pro note-on a note-off pro každý kanál
    std::array<NoteQueue, MIDI_CHANNELS> noteOnQueues_;
    std::array<NoteQueue, MIDI_CHANNELS> noteOffQueues_;

    // Pole pro aktivní noty a velocity
    std::array<std::atomic<bool>, MIDI_NOTES> activeNotes_;  // Atomic pro thread-safety
    std::array<std::array<uint8_t, MIDI_NOTES>, MIDI_CHANNELS> velocities_;  // Velocity pro každý kanál a notu
    std::array<std::array<uint8_t, 128>, MIDI_CHANNELS> controllerValues_;  // Controller hodnoty

    // Interní helper metody
    void pushToQueue(NoteQueue& queue, uint8_t note);  // Přidá do queue s automatickým přetečením
    uint8_t popFromQueue(NoteQueue& queue);  // Vytáhne z queue, vrací 255 při prázdné
};