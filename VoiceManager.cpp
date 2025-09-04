#include "VoiceManager.h"
#include "Logger.h"

SynthVoice::SynthVoice()
    : logger_(Logger::getInstance())
{
    reset();
}

void SynthVoice::start(uint8_t midiNote, uint8_t velocity, const SampleLibrary& library)
{
    reset();

    midiNote_ = midiNote;
    velocity_ = velocity;

    if (!library.isNoteAvailable(midiNote)) {
        logger_.log("SynthVoice/start", "error", "Requested note not available: " + juce::String((int)midiNote));
        isActive_ = false;
        return;
    }

    sampleData_ = library.getSampleData(midiNote);
    sampleLength_ = library.getSampleLength(midiNote);

    if (!sampleData_ || sampleLength_ == 0) {
        logger_.log("SynthVoice/start", "error", "Invalid sample for note " + juce::String((int)midiNote));
        isActive_ = false;
        return;
    }

    position_ = 0;
    isActive_ = true;

    logger_.log("SynthVoice/start", "debug", "Started note " + juce::String((int)midiNote) +
                                             " len=" + juce::String(sampleLength_));
}

void SynthVoice::stop()
{
    isActive_ = false;
}

void SynthVoice::reset()
{
    midiNote_ = 0;
    velocity_ = 0;
    isActive_ = false;
    sampleData_ = nullptr;
    sampleLength_ = 0;
    position_ = 0;
}

void SynthVoice::render(float* outputBuffer, int numSamples)
{
    if (!isActive_ || sampleData_ == nullptr || sampleLength_ == 0)
        return;

    // jednoduchý lineární gain podle velocity
    const float gain = static_cast<float>(velocity_) / 127.0f;

    for (int i = 0; i < numSamples; ++i) {
        if (position_ >= sampleLength_) {
            // dohráno -> disable
            stop();
            break;
        }
        outputBuffer[i] += sampleData_[position_] * gain;
        ++position_;
    }
}

// ======================== VoiceManager =========================

VoiceManager::VoiceManager(const SampleLibrary& library, int numVoices)
    : logger_(Logger::getInstance()), sampleLibrary_(library)
{
    voices_.reserve(numVoices);
    for (int i = 0; i < numVoices; ++i)
        voices_.emplace_back(std::make_unique<SynthVoice>());

    logger_.log("VoiceManager/constructor", "info", "VoiceManager created with " + juce::String(numVoices) + " voices");
}

void VoiceManager::processMidiEvents(MidiStateManager& midiState)
{
    // NOTE ON
    for (int ch = 0; ch < 16; ++ch) {
        while (true) {
            int raw = midiState.popNoteOn(ch);
            if (raw == 0xff) break;
            uint8_t note = static_cast<uint8_t>(raw);
            uint8_t vel = static_cast<uint8_t>(midiState.getVelocity(ch, note));
            startVoice(note, vel);
        }
    }

    // NOTE OFF
    for (int ch = 0; ch < 16; ++ch) {
        while (true) {
            int raw = midiState.popNoteOff(ch);
            if (raw == 0xff) break;
            uint8_t note = static_cast<uint8_t>(raw);
            stopVoice(note);
        }
    }
}

void VoiceManager::generateAudio(float* buffer, int numSamples)
{
    if (buffer == nullptr || numSamples <= 0) return;

    // mix všech aktivních hlasů
    for (auto& v : voices_) {
        if (v->isActive()) v->render(buffer, numSamples);
    }
}

void VoiceManager::refresh()
{
    // může být rozšířeno o statistiky / voice stealing atd.
}

void VoiceManager::startVoice(uint8_t midiNote, uint8_t velocity)
{
    // hledejte volný hlas
    for (auto& v : voices_) {
        if (!v->isActive()) {
            v->start(midiNote, velocity, sampleLibrary_);
            return;
        }
    }

    // voice stealing: recyklovat nejstarší (zjednodušeno: index 0)
    if (!voices_.empty()) {
        voices_[0]->start(midiNote, velocity, sampleLibrary_);
        logger_.log("VoiceManager/startVoice", "warn", "Voice stealing: recycled voice 0 for note " + juce::String((int)midiNote));
    }
}

void VoiceManager::stopVoice(uint8_t midiNote)
{
    for (auto& v : voices_) {
        if (v->isActive() && v->getNote() == midiNote) {
            v->stop();
            return;
        }
    }
}
