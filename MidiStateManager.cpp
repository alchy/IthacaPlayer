#include "MidiStateManager.h"

MidiStateManager::MidiStateManager()
    : pitchWheel_(0)
    , logger_(Logger::getInstance())
{
    // Inicializace všech aktivních not
    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        activeNotes_[i] = ActiveNote();
    }
    
    // Inicializace controller values (výchozí hodnoty podle MIDI standardu)
    for (int channel = 0; channel < 16; channel++) {
        for (int controller = 0; controller < 128; controller++) {
            controllerValues_[channel][controller] = 0;
        }
        // Výchozí hodnoty pro některé standardní controllery
        controllerValues_[channel][7] = 100;  // Volume - defaultně 100
        controllerValues_[channel][10] = 64;  // Pan - defaultně center
        controllerValues_[channel][64] = 0;   // Sustain pedal - defaultně off
    }
    
    logger_.log("MidiStateManager/constructor", "info", "MidiStateManager inicializovan");
    logger_.log("MidiStateManager/constructor", "info", "Max aktivnich not: " + juce::String(MAX_ACTIVE_NOTES));
}

void MidiStateManager::putNoteOn(uint8_t channel, uint8_t key, uint8_t velocity)
{
    if (channel >= 16 || key >= 128) {
        logger_.log("MidiStateManager/putNoteOn", "warn", "Neplatny channel nebo key: ch=" + 
                    juce::String(channel) + " key=" + juce::String(key));
        return;
    }
    
    logger_.log("MidiStateManager/putNoteOn", "info", "Note ON - Ch:" + juce::String(channel) + 
                " Key:" + juce::String(key) + " Vel:" + juce::String(velocity));
    
    // Najdi existující slot pro tuto notu nebo vytvoř nový
    int slot = findNoteSlot(channel, key);
    if (slot == -1) {
        slot = findFreeSlot();
    }
    
    if (slot != -1) {
        activeNotes_[slot].channel = channel;
        activeNotes_[slot].key = key;
        activeNotes_[slot].velocity = velocity;
        activeNotes_[slot].isActive = true;
        activeNotes_[slot].triggerTime = juce::Time::getMillisecondCounter();
        
        // Přidej do note-on queue
        pushToQueue(noteOnQueue_[channel], key);
    } else {
        logger_.log("MidiStateManager/putNoteOn", "warn", "Zadny volny slot pro novou notu");
    }
}

void MidiStateManager::putNoteOff(uint8_t channel, uint8_t key)
{
    if (channel >= 16 || key >= 128) {
        logger_.log("MidiStateManager/putNoteOff", "warn", "Neplatny channel nebo key: ch=" + 
                    juce::String(channel) + " key=" + juce::String(key));
        return;
    }
    
    logger_.log("MidiStateManager/putNoteOff", "info", "Note OFF - Ch:" + juce::String(channel) + 
                " Key:" + juce::String(key));
    
    // Najdi slot s touto notou a označ ji jako neaktivní
    int slot = findNoteSlot(channel, key);
    if (slot != -1) {
        activeNotes_[slot].isActive = false;
        
        // Přidej do note-off queue
        pushToQueue(noteOffQueue_[channel], key);
    }
}

uint8_t MidiStateManager::popNoteOn(uint8_t channel)
{
    if (channel >= 16) {
        return 0xff;
    }
    
    return popFromQueue(noteOnQueue_[channel]);
}

uint8_t MidiStateManager::popNoteOff(uint8_t channel)
{
    if (channel >= 16) {
        return 0xff;
    }
    
    return popFromQueue(noteOffQueue_[channel]);
}

uint8_t MidiStateManager::getVelocity(uint8_t channel, uint8_t key) const
{
    int slot = findNoteSlot(channel, key);
    if (slot != -1 && activeNotes_[slot].isActive) {
        return activeNotes_[slot].velocity;
    }
    
    return 0; // Defaultní velocity pokud nota není aktivní
}

void MidiStateManager::setPitchWheel(int16_t pitchWheelValue)
{
    pitchWheel_ = pitchWheelValue;
    logger_.log("MidiStateManager/setPitchWheel", "debug", "Pitch wheel: " + juce::String(pitchWheelValue));
}

void MidiStateManager::setControllerValue(uint8_t channel, uint8_t controller, uint8_t value)
{
    if (channel >= 16 || controller >= 128) {
        logger_.log("MidiStateManager/setControllerValue", "warn", "Neplatny channel nebo controller: ch=" + 
                    juce::String(channel) + " cc=" + juce::String(controller));
        return;
    }
    
    controllerValues_[channel][controller] = value;
    
    // Logování pouze pro důležité controllery
    if (controller == 1 || controller == 7 || controller == 10 || controller == 64) {
        juce::String ccName = "CC" + juce::String(controller);
        if (controller == 1) ccName = "Modulation";
        else if (controller == 7) ccName = "Volume";
        else if (controller == 10) ccName = "Pan";
        else if (controller == 64) ccName = "Sustain";
        
        logger_.log("MidiStateManager/setControllerValue", "info", 
                    ccName + " Ch:" + juce::String(channel) + " Val:" + juce::String(value));
    }
}

uint8_t MidiStateManager::getControllerValue(uint8_t channel, uint8_t controller) const
{
    if (channel >= 16 || controller >= 128) {
        return 0;
    }
    
    return controllerValues_[channel][controller];
}

void MidiStateManager::processMidiBuffer(const juce::MidiBuffer& midiBuffer)
{
    for (const auto& midiMetadata : midiBuffer) {
        auto message = midiMetadata.getMessage();
        
        if (message.isNoteOn()) {
            // MIDI Note On s velocity 0 = Note Off
            if (message.getVelocity() == 0) {
                putNoteOff(message.getChannel() - 1, message.getNoteNumber());
            } else {
                putNoteOn(message.getChannel() - 1, message.getNoteNumber(), message.getVelocity());
            }
        }
        else if (message.isNoteOff()) {
            putNoteOff(message.getChannel() - 1, message.getNoteNumber());
        }
        else if (message.isPitchWheel()) {
            // Převod z JUCE formátu na signed int16 (-8192 až +8191)
            int pitchWheelValue = message.getPitchWheelValue() - 8192;
            setPitchWheel(pitchWheelValue);
        }
        else if (message.isController()) {
            setControllerValue(message.getChannel() - 1, message.getControllerNumber(), message.getControllerValue());
        }
    }
}

void MidiStateManager::logActiveNotes() const
{
    int activeCount = 0;
    juce::String noteList;
    
    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        if (activeNotes_[i].isActive) {
            activeCount++;
            if (noteList.isNotEmpty()) noteList += ", ";
            noteList += juce::String(activeNotes_[i].key);
        }
    }
    
    logger_.log("MidiStateManager/logActiveNotes", "debug", 
                "Aktivnich not: " + juce::String(activeCount) + " [" + noteList + "]");
}

int MidiStateManager::getActiveNoteCount() const
{
    int count = 0;
    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        if (activeNotes_[i].isActive) {
            count++;
        }
    }
    return count;
}

int MidiStateManager::findNoteSlot(uint8_t channel, uint8_t key) const
{
    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        if (activeNotes_[i].channel == channel && activeNotes_[i].key == key) {
            return i;
        }
    }
    return -1; // Nenalezen
}

int MidiStateManager::findFreeSlot() const
{
    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        if (!activeNotes_[i].isActive) {
            return i;
        }
    }
    return -1; // Žádný volný slot
}

void MidiStateManager::pushToQueue(NoteQueue& queue, uint8_t note)
{
    if (queue.count < 128) {
        queue.notes[queue.writeIndex] = note;
        queue.writeIndex = (queue.writeIndex + 1) % 128;
        queue.count++;
    }
}

uint8_t MidiStateManager::popFromQueue(NoteQueue& queue)
{
    if (queue.count > 0) {
        uint8_t note = queue.notes[queue.readIndex];
        queue.readIndex = (queue.readIndex + 1) % 128;
        queue.count--;
        return note;
    }
    return 0xff; // Queue je prázdná
}