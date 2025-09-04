#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "SampleLibrary.h"
#include "MidiStateManager.h"
#include "Logger.h"

/*
 * SynthVoice: jednoduchý, real-time-friendly renderer,
 * drží pointer na readonly data (ne vlastní) a pozici.
 */
class SynthVoice
{
public:
    SynthVoice();

    // start: nastavení noty/velocity a ukazatele na data
    void start(uint8_t midiNote, uint8_t velocity, const SampleLibrary& library);

    // stop: okamžitě přestane být aktivní (může být změněno na release-phase)
    void stop();

    // reset: kompletní návrat do default stavu
    void reset();

    // render: zapíše numSamples do bufferu (přičítá)
    void render(float* outputBuffer, int numSamples);

    bool isActive() const { return isActive_; }
    uint8_t getNote() const { return midiNote_; }

private:
    Logger& logger_;

    uint8_t midiNote_{0};
    uint8_t velocity_{0};
    bool isActive_{false};

    const float* sampleData_{nullptr};
    uint32_t sampleLength_{0};
    uint32_t position_{0};
};

/*
 * VoiceManager: spravuje vector hlasů, nutně je konstruován s referencí na SampleLibrary.
 * Tím garantujeme, že při konstrukci existuje sample library a nelze zde mít nullptr.
 */
class VoiceManager
{
public:
    // Konstruktor vyžaduje referenci na existující SampleLibrary (nelze být nullptr).
    VoiceManager(const SampleLibrary& library, int numVoices = 16);

    ~VoiceManager() = default;

    // processMidiEvents čte queue z MidiStateManager a spouští/staví hlasy
    void processMidiEvents(MidiStateManager& midiState);

    // generateAudio mixuje audio z jednotlivých hlasů
    void generateAudio(float* buffer, int numSamples);

    // refresh: housekeeping (může implementovat voice stealing, atd.)
    void refresh();

private:
    Logger& logger_;
    const SampleLibrary& sampleLibrary_; // povinná reference
    std::vector<std::unique_ptr<SynthVoice>> voices_;

    void startVoice(uint8_t midiNote, uint8_t velocity);
    void stopVoice(uint8_t midiNote);
};
