#pragma once

#include <juce_core/juce_core.h>
#include "MidiStateManager.h"
#include "SampleLibrary.h"
#include "Logger.h"

/**
 * SynthVoice - reprezentuje jeden hlas syntezátoru
 * Inspirováno vaší DeviceVoice třídou
 */
class SynthVoice 
{
public:
    SynthVoice(int voiceNumber);
    
    // Voice control methods (podobné vašemu DeviceVoice interface)
    void play(bool gate, uint8_t note);
    void setVelocity(uint8_t velocity);
    void setPitchWheel(int16_t pitchWheel);
    
    // Playback state (podobné vaším get/set metodám)
    uint8_t getNote() const { return currentNote_; }
    bool getGate() const { return isPlaying_; }
    uint8_t getQueue() const { return queuePosition_; }
    void setQueue(uint8_t position) { queuePosition_ = position; }
    
    // Audio generation - hlavní metoda pro generování audio
    void generateAudio(float* outputBuffer, int numSamples, const SampleLibrary& sampleLibrary);
    
    // Reset methods
    void reset();
    
private:
    int voiceNumber_;           // Číslo hlasu (podobné vaším voice indexům)
    uint8_t currentNote_;       // Aktuálně hraná nota
    uint8_t velocity_;          // Velocity noty
    bool isPlaying_;           // Gate stav (podobné vašemu gate flag)
    uint8_t queuePosition_;    // Pozice v queue pro voice stealing
    
    // Playback state pro sample library
    uint32_t samplePosition_;   // Aktuální pozice v sample
    int16_t pitchWheel_;       // Pitch wheel value
    
    Logger& logger_;
};

/**
 * VoiceManager - správce hlasů syntezátoru
 * Přímá inspirace vaší Performer třídou s podobnými metodami
 */
class VoiceManager 
{
public:
    VoiceManager(int maxVoices = 16);
    ~VoiceManager();
    
    // Hlavní metody inspirované vaším Performer interface
    void play(bool gate, uint8_t note, uint8_t velocity);
    void setPitchWheel(int16_t pitchWheel);
    
    // Audio generation - kombinuje všechny hlasy
    void generateAudio(float* outputBuffer, int numSamples, const SampleLibrary& sampleLibrary);
    
    // Voice management methods (podobné vašim Performer metodám)
    void refresh();  // Podobné vašemu refresh()
    int getVoiceCount() const { return voiceCount_; }
    
    // Integration s MidiStateManager
    void processMidiEvents(MidiStateManager& midiState);
    
private:
    // Voice allocation methods (přesně podle vaší logiky)
    int getFreeVoice(uint8_t note);  // Váš algoritmus
    void mixleQueue(int queueNumber); // Váš mixle_queue algoritmus
    
    // Member variables podobné vašim voice_ arrays
    static const int MAX_VOICES = 16;
    SynthVoice* voices_[MAX_VOICES];
    int voiceCount_;
    
    // Pitch wheel state pro všechny hlasy
    int16_t globalPitchWheel_;
    
    Logger& logger_;
    
    // Helper methods
    void setVoiceCount(int count) { voiceCount_ = count; }
};