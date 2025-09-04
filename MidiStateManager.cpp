#include "MidiStateManager.h"
#include <algorithm>
#include <cassert>

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4244) // Conversion warnings pro MIDI values
#endif

//==============================================================================
MidiStateManager::MidiStateManager()
    : logger_(Logger::getInstance())
{
    logger_.log("MidiStateManager/constructor", "info", "=== INICIALIZACE MIDI STATE MANAGER ===");
    
    // Inicializace aktivních not
    for (auto& note : activeNotes_) {
        note.reset();
    }
    
    // Inicializace MIDI controller hodnot podle standardu
    for (int channel = 0; channel < 16; channel++) {
        for (int controller = 0; controller < 128; controller++) {
            controllerValues_[channel][controller] = 0;
        }
        
        // Výchozí hodnoty pro standardní controllery
        controllerValues_[channel][7] = 100;   // Volume (0-127, default high)
        controllerValues_[channel][10] = 64;   // Pan (0-127, default center)
        controllerValues_[channel][11] = 127;  // Expression (0-127, default max)
        controllerValues_[channel][64] = 0;    // Sustain pedal (0-127, default off)
        controllerValues_[channel][91] = 0;    // Reverb (0-127, default off)
        controllerValues_[channel][93] = 0;    // Chorus (0-127, default off)
    }
    
    // Reset všech queue
    for (auto& queue : noteOnQueues_) {
        queue.reset();
    }
    for (auto& queue : noteOffQueues_) {
        queue.reset();
    }
    
    logger_.log("MidiStateManager/constructor", "info", 
                "Max aktivnich not: " + juce::String(MAX_ACTIVE_NOTES));
    logger_.log("MidiStateManager/constructor", "info", 
                "MIDI channels: 16, Controllers: 128 per channel");
    logger_.log("MidiStateManager/constructor", "info", 
                "=== MIDI STATE MANAGER INICIALIZOVAN ===");
}

//==============================================================================
void MidiStateManager::processMidiBuffer(const juce::MidiBuffer& midiBuffer)
{
    // Počítadlo pro optimalizaci logování
    static uint32_t processedMessages = 0;
    int messagesInBuffer = 0;
    
    for (const auto& midiMetadata : midiBuffer) {
        auto message = midiMetadata.getMessage();
        processedMessages++;
        messagesInBuffer++;
        totalMidiMessages_.fetch_add(1, std::memory_order_relaxed);
        
        // Zpracování různých typů MIDI zpráv
        if (message.isNoteOn()) {
            // MIDI Note On s velocity 0 se považuje za Note Off podle standardu
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
            // Převod z JUCE rozsahu (0-16383) na signed int16 (-8192 až +8191)
            int pitchWheelValue = message.getPitchWheelValue() - 8192;
            setPitchWheel(static_cast<int16_t>(pitchWheelValue));
        }
        else if (message.isController()) {
            setControllerValue(message.getChannel() - 1, 
                             message.getControllerNumber(), 
                             message.getControllerValue());
        }
        else if (message.isProgramChange()) {
            logger_.log("MidiStateManager/processMidiBuffer", "info",
                       "Program Change: " + juce::String(message.getProgramChangeNumber()) + 
                       " na kanalu " + juce::String(message.getChannel()));
        }
        else if (message.isChannelPressure()) {
            logger_.log("MidiStateManager/processMidiBuffer", "debug",
                       "Channel Pressure: " + juce::String(message.getChannelPressureValue()) + 
                       " na kanalu " + juce::String(message.getChannel()));
        }
        else if (message.isAftertouch()) {
            logger_.log("MidiStateManager/processMidiBuffer", "debug",
                       "Aftertouch: nota " + juce::String(message.getNoteNumber()) + 
                       " pressure " + juce::String(message.getAfterTouchValue()) + 
                       " na kanalu " + juce::String(message.getChannel()));
        }
        
        // Detailní logování pouze pro první zprávy nebo při debug režimu
        if (processedMessages <= 10) {
            juce::String midiInfo = "MIDI #" + juce::String(processedMessages) + 
                                   " @ sample " + juce::String(midiMetadata.samplePosition);
            
            if (message.isNoteOn() && message.getVelocity() > 0) {
                midiInfo += ": NOTE ON - " + 
                           message.getMidiNoteName(message.getNoteNumber(), true, true, 4) + 
                           " vel:" + juce::String(message.getVelocity()) + 
                           " ch:" + juce::String(message.getChannel());
            } else if (message.isNoteOff() || (message.isNoteOn() && message.getVelocity() == 0)) {
                midiInfo += ": NOTE OFF - " + 
                           message.getMidiNoteName(message.getNoteNumber(), true, true, 4) + 
                           " ch:" + juce::String(message.getChannel());
            } else if (message.isPitchWheel()) {
                midiInfo += ": PITCH WHEEL - " + juce::String(message.getPitchWheelValue()) + 
                           " ch:" + juce::String(message.getChannel());
            } else if (message.isController()) {
                midiInfo += ": CC" + juce::String(message.getControllerNumber()) + 
                           " = " + juce::String(message.getControllerValue()) + 
                           " ch:" + juce::String(message.getChannel());
            } else {
                midiInfo += ": " + message.getDescription();
            }
            
            logger_.log("MidiStateManager/processMidiBuffer", "info", midiInfo);
        }
    }
    
    // Logování souhrnu pro buffer s více zprávami
    if (messagesInBuffer > 1) {
        logger_.log("MidiStateManager/processMidiBuffer", "info",
                   "Buffer zpracovan: " + juce::String(messagesInBuffer) + 
                   " zprav (celkem: " + juce::String(totalMidiMessages_.load()) + ")");
    }
}

//==============================================================================
void MidiStateManager::putNoteOn(uint8_t channel, uint8_t key, uint8_t velocity)
{
    if (!isValidChannel(channel) || !isValidKey(key)) {
        logger_.log("MidiStateManager/putNoteOn", "warn", 
                   "Neplatny channel nebo key: ch=" + juce::String(channel) + 
                   " key=" + juce::String(key));
        return;
    }
    
    logger_.log("MidiStateManager/putNoteOn", "info", 
               "Note ON - Ch:" + juce::String(channel) + 
               " Key:" + juce::String(key) + 
               " Vel:" + juce::String(velocity));
    
    // OPRAVA: Unified locking strategy
    std::lock_guard<std::mutex> lock(midiMutex_);
    
    // Hledání existujícího slotu nebo volného slotu
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
        
        // Přidání do queue pro VoiceManager
        pushToQueue(noteOnQueues_[channel], key);
    } else {
        logger_.log("MidiStateManager/putNoteOn", "warn", 
                   "Zadny volny slot pro notu - zvyste MAX_ACTIVE_NOTES");
    }
}

void MidiStateManager::putNoteOff(uint8_t channel, uint8_t key)
{
    if (!isValidChannel(channel) || !isValidKey(key)) {
        logger_.log("MidiStateManager/putNoteOff", "warn", 
                   "Neplatny channel nebo key: ch=" + juce::String(channel) + 
                   " key=" + juce::String(key));
        return;
    }
    
    logger_.log("MidiStateManager/putNoteOff", "info", 
               "Note OFF - Ch:" + juce::String(channel) + 
               " Key:" + juce::String(key));
    
    // OPRAVA: Unified locking strategy
    std::lock_guard<std::mutex> lock(midiMutex_);
    
    // Hledání a deaktivace noty
    int slot = findNoteSlot(channel, key);
    if (slot != -1) {
        activeNotes_[slot].isActive = false;
        
        // Přidání do note-off queue
        pushToQueue(noteOffQueues_[channel], key);
    } else {
        logger_.log("MidiStateManager/putNoteOff", "debug", 
                   "Note OFF pro neaktivni notu: ch=" + juce::String(channel) + 
                   " key=" + juce::String(key));
        
        // I neaktivní nota může potřebovat note-off (pro voice cleanup)
        pushToQueue(noteOffQueues_[channel], key);
    }
}

//==============================================================================
uint8_t MidiStateManager::popNoteOn(uint8_t channel)
{
    if (!isValidChannel(channel)) {
        return 0xff;
    }
    
    // OPRAVA: Unified locking
    std::lock_guard<std::mutex> lock(midiMutex_);
    return popFromQueue(noteOnQueues_[channel]);
}

uint8_t MidiStateManager::popNoteOff(uint8_t channel)
{
    if (!isValidChannel(channel)) {
        return 0xff;
    }
    
    // OPRAVA: Unified locking
    std::lock_guard<std::mutex> lock(midiMutex_);
    return popFromQueue(noteOffQueues_[channel]);
}

//==============================================================================
uint8_t MidiStateManager::getVelocity(uint8_t channel, uint8_t key) const
{
    if (!isValidChannel(channel) || !isValidKey(key)) {
        return 0;
    }
    
    // OPRAVA: Unified locking
    std::lock_guard<std::mutex> lock(midiMutex_);
    
    int slot = findNoteSlot(channel, key);
    if (slot != -1 && activeNotes_[slot].isActive) {
        return activeNotes_[slot].velocity;
    }
    
    return 0; // Výchozí velocity pro neaktivní notu
}

bool MidiStateManager::isNoteActive(uint8_t channel, uint8_t key) const
{
    if (!isValidChannel(channel) || !isValidKey(key)) {
        return false;
    }
    
    // OPRAVA: Unified locking
    std::lock_guard<std::mutex> lock(midiMutex_);
    
    int slot = findNoteSlot(channel, key);
    return (slot != -1 && activeNotes_[slot].isActive);
}

//==============================================================================
void MidiStateManager::setPitchWheel(int16_t pitchWheelValue)
{
    pitchWheel_.store(pitchWheelValue, std::memory_order_relaxed);
    
    logger_.log("MidiStateManager/setPitchWheel", "debug", 
               "Pitch wheel: " + juce::String(pitchWheelValue));
}

//==============================================================================
void MidiStateManager::setControllerValue(uint8_t channel, uint8_t controller, uint8_t value)
{
    if (!isValidChannel(channel) || !isValidController(controller)) {
        logger_.log("MidiStateManager/setControllerValue", "warn", 
                   "Neplatny channel nebo controller: ch=" + juce::String(channel) + 
                   " cc=" + juce::String(controller));
        return;
    }
    
    // OPRAVA: Unified locking
    std::lock_guard<std::mutex> lock(midiMutex_);
    controllerValues_[channel][controller] = value;
    
    // Logování pouze pro důležité controllery
    if (controller == 1 || controller == 7 || controller == 10 || controller == 64 || 
        controller == 91 || controller == 93) {
        
        juce::String ccName = "CC" + juce::String(controller);
        switch (controller) {
            case 1: ccName = "Modulation"; break;
            case 7: ccName = "Volume"; break;
            case 10: ccName = "Pan"; break;
            case 64: ccName = "Sustain"; break;
            case 91: ccName = "Reverb"; break;
            case 93: ccName = "Chorus"; break;
        }
        
        logger_.log("MidiStateManager/setControllerValue", "info", 
                   ccName + " Ch:" + juce::String(channel) + 
                   " Val:" + juce::String(value));
    }
}

uint8_t MidiStateManager::getControllerValue(uint8_t channel, uint8_t controller) const
{
    if (!isValidChannel(channel) || !isValidController(controller)) {
        return 0;
    }
    
    // OPRAVA: Unified locking
    std::lock_guard<std::mutex> lock(midiMutex_);
    return controllerValues_[channel][controller];
}

//==============================================================================
void MidiStateManager::logActiveNotes() const
{
    // OPRAVA: Unified locking
    std::lock_guard<std::mutex> lock(midiMutex_);
    
    int activeCount = 0;
    juce::String noteList;
    
    for (const auto& note : activeNotes_) {
        if (note.isActive) {
            activeCount++;
            if (noteList.isNotEmpty()) noteList += ", ";
            noteList += juce::String(note.key) + "(ch" + juce::String(note.channel) + ")";
        }
    }
    
    logger_.log("MidiStateManager/logActiveNotes", "debug", 
               "Aktivnich not: " + juce::String(activeCount) + " [" + noteList + "]");
}

int MidiStateManager::getActiveNoteCount() const
{
    // OPRAVA: Unified locking
    std::lock_guard<std::mutex> lock(midiMutex_);
    
    int count = 0;
    for (const auto& note : activeNotes_) {
        if (note.isActive) {
            count++;
        }
    }
    return count;
}

void MidiStateManager::resetAllNotes()
{
    logger_.log("MidiStateManager/resetAllNotes", "info", "=== RESET VSECH NOT ===");
    
    // OPRAVA: Unified locking
    std::lock_guard<std::mutex> lock(midiMutex_);
    
    // Reset všech aktivních not
    for (auto& note : activeNotes_) {
        note.reset();
    }
    
    // Reset všech queue
    for (auto& queue : noteOnQueues_) {
        queue.reset();
    }
    for (auto& queue : noteOffQueues_) {
        queue.reset();
    }
    
    // Reset pitch wheel
    pitchWheel_.store(0);
    
    logger_.log("MidiStateManager/resetAllNotes", "info", "Vse resetovano");
}

//==============================================================================
// Private helper methods
//==============================================================================

int MidiStateManager::findNoteSlot(uint8_t channel, uint8_t key) const
{
    // Metoda se volá již v rámci mutex lock
    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        if (activeNotes_[i].channel == channel && activeNotes_[i].key == key) {
            return i;
        }
    }
    return -1; // Slot nenalezen
}

int MidiStateManager::findFreeSlot() const
{
    // Metoda se volá již v rámci mutex lock
    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        if (!activeNotes_[i].isActive) {
            return i;
        }
    }
    return -1; // Žádný volný slot
}

void MidiStateManager::pushToQueue(NoteQueue& queue, uint8_t note)
{
    // OPRAVA: Už jsme v unified mutex lock, nepotřebujeme další lock
    uint8_t currentCount = queue.count.load();
    if (currentCount < 255) {  // Ponecháme místo pro wrap-around detection
        uint8_t writeIndex = queue.writeIndex.load();
        queue.notes[writeIndex] = note;
        queue.writeIndex.store(static_cast<uint8_t>(writeIndex + 1));  // uint8 auto wrap
        queue.count.store(currentCount + 1);
    } else {
        // Queue overflow - starší zprávy se ztratí (sliding window)
        logger_.log("MidiStateManager/pushToQueue", "warn", 
                   "Queue overflow - zprava ztracena");
    }
}

uint8_t MidiStateManager::popFromQueue(NoteQueue& queue)
{
    // OPRAVA: Už jsme v unified mutex lock, nepotřebujeme další lock
    uint8_t currentCount = queue.count.load();
    if (currentCount > 0) {
        uint8_t note = queue.notes[queue.readIndex];
        queue.readIndex = static_cast<uint8_t>(queue.readIndex + 1);  // uint8 auto wrap
        queue.count.store(currentCount - 1);
        return note;
    }
    
    return 0xff; // Fronta je prázdná
}

#ifdef _WIN32
#pragma warning(pop)
#endif