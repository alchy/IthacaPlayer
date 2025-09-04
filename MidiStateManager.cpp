#include "MidiStateManager.h"
#include <algorithm>
#include <cassert>

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4244)  // Varování konverze pro MIDI hodnoty
#endif

/**
 * @brief Konstruktor MidiStateManager.
 * Inicializuje logger, resetuje queue a nastaví výchozí hodnoty controllerů podle MIDI standardu.
 */
MidiStateManager::MidiStateManager()
    : logger_(Logger::getInstance())
{
    logger_.log("MidiStateManager/constructor", "info", "=== INICIALIZACE MIDI STATE MANAGER ===");
    
    // Inicializace aktivních not
    for (auto& note : activeNotes_) {
        note.store(false);  // Všechny noty neaktivní
    }
    
    // Inicializace MIDI controller hodnot
    for (int channel = 0; channel < MIDI_CHANNELS; ++channel) {
        for (int controller = 0; controller < 128; ++controller) {
            controllerValues_[channel][controller] = 0;  // Výchozí 0
        }
        
        // Výchozí hodnoty pro standardní controllery
        controllerValues_[channel][7] = 100;   // Volume (výchozí vysoká)
        controllerValues_[channel][10] = 64;   // Pan (střed)
        controllerValues_[channel][11] = 127;  // Expression (max)
        controllerValues_[channel][64] = 0;    // Sustain pedal (vypnutý)
        controllerValues_[channel][91] = 0;    // Reverb (vypnutý)
        controllerValues_[channel][93] = 0;    // Chorus (vypnutý)
    }
    
    // Reset všech queue
    for (auto& queue : noteOnQueues_) {
        queue.reset();
    }
    for (auto& queue : noteOffQueues_) {
        queue.reset();
    }
    
    logger_.log("MidiStateManager/constructor", "info", "Inicializace dokončena.");
    DBG("MidiStateManager initialized.");  // Přidaný debug pro konzoli
}

/**
 * @brief Resetuje NoteQueue na výchozí stav.
 * Nastaví indexy a počet na 0, nemusí čistit array (přepsáno při použití).
 */
void MidiStateManager::NoteQueue::reset() {
    writeIndex.store(0);
    count.store(0);
    readIndex = 0;
}

/**
 * @brief Přidá note-on do queue a aktualizuje stav aktivní noty a velocity.
 * @param channel MIDI kanál (0-15)
 * @param note MIDI nota (0-127)
 * @param velocity Velocity (0-127)
 */
void MidiStateManager::pushNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (channel >= MIDI_CHANNELS || note >= MIDI_NOTES) return;  // Bezpečnostní kontrola
    
    pushToQueue(noteOnQueues_[channel], note);
    activeNotes_[note].store(true);
    velocities_[channel][note] = velocity;
    
    logger_.log("MidiStateManager/pushNoteOn", "debug", "NoteOn kanál " + juce::String(channel) + ", nota " + juce::String(note));
}

/**
 * @brief Přidá note-off do queue a deaktivuje notu.
 * @param channel MIDI kanál (0-15)
 * @param note MIDI nota (0-127)
 */
void MidiStateManager::pushNoteOff(uint8_t channel, uint8_t note) {
    if (channel >= MIDI_CHANNELS || note >= MIDI_NOTES) return;
    
    pushToQueue(noteOffQueues_[channel], note);
    activeNotes_[note].store(false);
    
    logger_.log("MidiStateManager/pushNoteOff", "debug", "NoteOff kanál " + juce::String(channel) + ", nota " + juce::String(note));
}

/**
 * @brief Vytáhne note-on z queue pro daný kanál.
 * @param channel MIDI kanál
 * @return Nota (0-127) nebo 255 pokud prázdná queue (🔧 Změna: Pro odstranění warningu C4244)
 */
uint8_t MidiStateManager::popNoteOn(uint8_t channel) {  // 🔧 Změna: Změněno na uint8_t
    if (channel >= MIDI_CHANNELS) return 255;
    return popFromQueue(noteOnQueues_[channel]);
}

/**
 * @brief Vytáhne note-off z queue pro daný kanál.
 * @param channel MIDI kanál
 * @return Nota (0-127) nebo 255 pokud prázdná queue (🔧 Změna: Pro odstranění warningu C4244)
 */
uint8_t MidiStateManager::popNoteOff(uint8_t channel) {  // 🔧 Změna: Změněno na uint8_t
    if (channel >= MIDI_CHANNELS) return 255;
    return popFromQueue(noteOffQueues_[channel]);
}

/**
 * @brief Zkontroluje, zda je nota aktivní.
 * @param channel MIDI kanál
 * @param note MIDI nota
 * @return True pokud aktivní
 */
bool MidiStateManager::isNoteActive(uint8_t channel, uint8_t note) const {
    if (channel >= MIDI_CHANNELS || note >= MIDI_NOTES) return false;
    return activeNotes_[note].load();
}

/**
 * @brief Vrátí velocity aktivní noty.
 * @param channel MIDI kanál
 * @param note MIDI nota
 * @return Velocity (0-127)
 */
uint8_t MidiStateManager::getVelocity(uint8_t channel, uint8_t note) const {
    if (channel >= MIDI_CHANNELS || note >= MIDI_NOTES) return 0;
    return velocities_[channel][note];
}

/**
 * @brief Nastaví hodnotu MIDI controlleru.
 * @param channel MIDI kanál
 * @param controller Číslo controlleru (0-127)
 * @param value Hodnota (0-127)
 */
void MidiStateManager::setControllerValue(uint8_t channel, uint8_t controller, uint8_t value) {
    if (channel >= MIDI_CHANNELS || controller > 127) return;
    controllerValues_[channel][controller] = value;
}

/**
 * @brief Vrátí hodnotu MIDI controlleru.
 * @param channel MIDI kanál
 * @param controller Číslo controlleru
 * @return Hodnota (0-127)
 */
uint8_t MidiStateManager::getControllerValue(uint8_t channel, uint8_t controller) const {
    if (channel >= MIDI_CHANNELS || controller > 127) return 0;
    return controllerValues_[channel][controller];
}

/**
 * @brief Přidá prvek do circular queue s automatickým přetečením.
 * @param queue Reference na queue
 * @param note Hodnota k přidání
 * Zjednodušeno: Používá modulo pro index, atomic operace pro count.
 */
void MidiStateManager::pushToQueue(NoteQueue& queue, uint8_t note) {
    std::lock_guard<std::mutex> lock(queue.mutex);

    if (queue.count.load() >= 256) {
        logger_.log("MidiStateManager/pushToQueue", "warn", "Queue plná - zpráva ztracena");
        return;  // Zachováno varování při plné queue
    }

    uint8_t index = queue.writeIndex.load();
    queue.notes[index] = note;
    queue.writeIndex.store((index + 1) % 256);  // Automatické přetečení modulo 256
    queue.count.fetch_add(1);  // Atomic inkrement počtu
}

/**
 * @brief Vytáhne prvek z circular queue.
 * @param queue Reference na queue
 * @return Hodnota nebo 255 při prázdné queue (🔧 Změna: Pro konzistenci s uint8_t)
 */
uint8_t MidiStateManager::popFromQueue(NoteQueue& queue) {
    std::lock_guard<std::mutex> lock(queue.mutex);

    if (queue.count.load() == 0) {
        return 255;  // 🔧 Změna: 255 místo 0xff pro uint8_t (prázdná queue)
    }

    uint8_t note = queue.notes[queue.readIndex];
    queue.readIndex = (queue.readIndex + 1) % 256;  // Modulo pro přetečení
    queue.count.fetch_sub(1);  // Atomic dekrement počtu
    return note;
}

#ifdef _WIN32
#pragma warning(pop)
#endif