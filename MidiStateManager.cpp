#include "MidiStateManager.h"
#include <algorithm>
#include <cassert>

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4244)  // Varování konverze pro MIDI hodnoty
#endif

/**
 * @brief Konstruktor MidiStateManager.
 * Inicializuje logger, resetuje queue a nastavuje výchozí hodnoty controllerů podle MIDI standardu.
 */
MidiStateManager::MidiStateManager()
    : logger_(Logger::getInstance())
{
    logger_.log("MidiStateManager/constructor", "info", "=== MIDI STATE MANAGER INITIALIZATION ===");
    
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
    
    logger_.log("MidiStateManager/constructor", "info", "Initialization completed successfully");
    DBG("MidiStateManager initialized.");  // Přidaný debug pro konzoli
}

/**
 * @brief Resetuje NoteQueue na výchozí stav.
 * Nastavuje indexy a počet na 0, nemusí čistit array (přepsáno při použití).
 */
void MidiStateManager::NoteQueue::reset() {
    // Thread-safe reset
    std::lock_guard<std::mutex> lock(mutex);
    
    writeIndex.store(0);
    count.store(0);
    readIndex = 0;
    
    // Volitelné vyčištění dat pro debug účely
    #ifdef _DEBUG
    std::fill(notes.begin(), notes.end(), 0);
    #endif
}

/**
 * @brief Přidá note-on do queue a aktualizuje stav aktivní noty a velocity.
 * @param channel MIDI kanál (0-15)
 * @param note MIDI nota (0-127)
 * @param velocity Velocity (0-127)
 */
void MidiStateManager::pushNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    // Přidána rozsáhlá validace
    if (channel >= MIDI_CHANNELS) {
        logger_.log("MidiStateManager/pushNoteOn", "warn", 
                   "Invalid MIDI channel: " + juce::String(channel));
        return;
    }
    
    if (note >= MIDI_NOTES) {
        logger_.log("MidiStateManager/pushNoteOn", "warn", 
                   "Invalid MIDI note: " + juce::String(note));
        return;
    }
    
    if (velocity == 0) {
        // Velocity 0 je vlastně note-off
        pushNoteOff(channel, note);
        return;
    }
    
    if (velocity > 127) {
        logger_.log("MidiStateManager/pushNoteOn", "warn", 
                   "Invalid velocity: " + juce::String(velocity) + ", clamped to 127");
        velocity = 127;
    }
    
    pushToQueue(noteOnQueues_[channel], note);
    activeNotes_[note].store(true);
    velocities_[channel][note] = velocity;
    
    logger_.log("MidiStateManager/pushNoteOn", "debug", 
               "NoteOn ch=" + juce::String(channel) + 
               " note=" + juce::String(note) + 
               " vel=" + juce::String(velocity));
}

/**
 * @brief Přidá note-off do queue a deaktivuje notu.
 * @param channel MIDI kanál (0-15)
 * @param note MIDI nota (0-127)
 */
void MidiStateManager::pushNoteOff(uint8_t channel, uint8_t note) {
    // Přidána validace
    if (channel >= MIDI_CHANNELS) {
        logger_.log("MidiStateManager/pushNoteOff", "warn", 
                   "Invalid MIDI channel: " + juce::String(channel));
        return;
    }
    
    if (note >= MIDI_NOTES) {
        logger_.log("MidiStateManager/pushNoteOff", "warn", 
                   "Invalid MIDI note: " + juce::String(note));
        return;
    }
    
    pushToQueue(noteOffQueues_[channel], note);
    activeNotes_[note].store(false);
    
    // Reset velocity při note-off pro konzistenci
    velocities_[channel][note] = 0;
    
    logger_.log("MidiStateManager/pushNoteOff", "debug", 
               "NoteOff ch=" + juce::String(channel) + 
               " note=" + juce::String(note));
}

/**
 * @brief Vytáhne note-on z queue pro daný kanál.
 * @param channel MIDI kanál
 * @return Nota (0-127) nebo 255 pokud prázdná queue
 */
uint8_t MidiStateManager::popNoteOn(uint8_t channel) {
    if (channel >= MIDI_CHANNELS) {
        logger_.log("MidiStateManager/popNoteOn", "warn", 
                   "Invalid MIDI channel: " + juce::String(channel));
        return 255;
    }
    return popFromQueue(noteOnQueues_[channel]);
}

/**
 * @brief Vytáhne note-off z queue pro daný kanál.
 * @param channel MIDI kanál
 * @return Nota (0-127) nebo 255 pokud prázdná queue
 */
uint8_t MidiStateManager::popNoteOff(uint8_t channel) {
    if (channel >= MIDI_CHANNELS) {
        logger_.log("MidiStateManager/popNoteOff", "warn", 
                   "Invalid MIDI channel: " + juce::String(channel));
        return 255;
    }
    return popFromQueue(noteOffQueues_[channel]);
}

/**
 * @brief Zkontroluje, zda je nota aktivní.
 * @param channel MIDI kanál
 * @param note MIDI nota
 * @return True pokud aktivní
 */
bool MidiStateManager::isNoteActive(uint8_t channel, uint8_t note) const {
    if (channel >= MIDI_CHANNELS || note >= MIDI_NOTES) {
        return false;
    }
    return activeNotes_[note].load();
}

/**
 * @brief Vrátí velocity aktivní noty.
 * @param channel MIDI kanál
 * @param note MIDI nota
 * @return Velocity (0-127)
 */
uint8_t MidiStateManager::getVelocity(uint8_t channel, uint8_t note) const {
    if (channel >= MIDI_CHANNELS || note >= MIDI_NOTES) {
        return 0;
    }
    return velocities_[channel][note];
}

/**
 * @brief Nastaví hodnotu MIDI controlleru.
 * @param channel MIDI kanál
 * @param controller Číslo controlleru (0-127)
 * @param value Hodnota (0-127)
 */
void MidiStateManager::setControllerValue(uint8_t channel, uint8_t controller, uint8_t value) {
    if (channel >= MIDI_CHANNELS) {
        logger_.log("MidiStateManager/setControllerValue", "warn", 
                   "Invalid MIDI channel: " + juce::String(channel));
        return;
    }
    
    if (controller > 127) {
        logger_.log("MidiStateManager/setControllerValue", "warn", 
                   "Invalid controller: " + juce::String(controller));
        return;
    }
    
    if (value > 127) {
        logger_.log("MidiStateManager/setControllerValue", "warn", 
                   "Invalid controller value: " + juce::String(value) + 
                   ", clamped to 127");
        value = 127;
    }
    
    controllerValues_[channel][controller] = value;
    
    // Rozšířené logování pro důležité controllery
    juce::String controllerName;
    switch (controller) {
        case 7: controllerName = "Volume"; break;
        case 10: controllerName = "Pan"; break;
        case 11: controllerName = "Expression"; break;
        case 64: controllerName = "Sustain"; break;
        case 91: controllerName = "Reverb"; break;
        case 93: controllerName = "Chorus"; break;
        default: controllerName = "CC" + juce::String(controller); break;
    }
    
    // Log jen důležité controllery nebo při debug módu
    if (controller == 7 || controller == 10 || controller == 64 || 
        Logger::loggingEnabled.load()) {
        logger_.log("MidiStateManager/setControllerValue", "debug", 
                   "Ch" + juce::String(channel) + 
                   " " + controllerName + "=" + juce::String(value));
    }
}

/**
 * @brief Vrátí hodnotu MIDI controlleru.
 * @param channel MIDI kanál
 * @param controller Číslo controlleru
 * @return Hodnota (0-127)
 */
uint8_t MidiStateManager::getControllerValue(uint8_t channel, uint8_t controller) const {
    if (channel >= MIDI_CHANNELS || controller > 127) {
        return 0;
    }
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

    uint8_t currentCount = queue.count.load();
    uint8_t writeIndex = queue.writeIndex.load();

    // Správné řešení overflow s circular behavior
    if (currentCount >= 256) {
        // Queue je plná - přepíšeme nejstarší záznam (circular behavior)
        queue.readIndex = (queue.readIndex + 1) % 256;
        // Snížíme count o 1, protože přepíšeme starý záznam
        queue.count.store(255);
        currentCount = 255;
        
        logger_.log("MidiStateManager/pushToQueue", "debug", 
                   "Queue overflow - overwriting oldest record");
    }

    // Zápis nového záznamu
    queue.notes[writeIndex] = note;
    queue.writeIndex.store((writeIndex + 1) % 256);
    queue.count.store(currentCount + 1);
}

/**
 * @brief Vytáhne prvek z circular queue.
 * @param queue Reference na queue
 * @return Hodnota nebo 255 při prázdné queue
 */
uint8_t MidiStateManager::popFromQueue(NoteQueue& queue) {
    std::lock_guard<std::mutex> lock(queue.mutex);

    uint8_t currentCount = queue.count.load();
    if (currentCount == 0) {
        return 255;  // Prázdná queue
    }

    // Dodatečná validace před čtením
    uint8_t readIndex = queue.readIndex;
    if (readIndex >= 256) {
        // Nouzový reset při poškození indexu
        queue.readIndex = 0;
        readIndex = 0;
        logger_.log("MidiStateManager/popFromQueue", "warn", 
                   "Emergency readIndex reset");
    }

    uint8_t note = queue.notes[readIndex];
    queue.readIndex = (readIndex + 1) % 256;
    
    // Bezpečné snížení count s kontrolou underflow
    if (currentCount > 0) {
        queue.count.store(currentCount - 1);
    } else {
        // Nemělo by se stát, ale pro jistotu
        queue.count.store(0);
        logger_.log("MidiStateManager/popFromQueue", "warn", 
                   "Count underflow protection activated");
    }
    
    return note;
}

#ifdef _WIN32
#pragma warning(pop)
#endif