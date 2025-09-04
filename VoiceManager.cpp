#include "VoiceManager.h"
#include "Logger.h"

/**
 * @brief Konstruktor SynthVoice.
 * Inicializuje logger a resetuje stav.
 */
SynthVoice::SynthVoice()
    : logger_(Logger::getInstance())
{
    reset();
}

/**
 * @brief Spustí hlas s kontrolou dostupnosti vzorku.
 * @param midiNote MIDI nota
 * @param velocity Velocity
 * @param library SampleLibrary pro data
 */
void SynthVoice::start(uint8_t midiNote, uint8_t velocity, const SampleLibrary& library)
{
    reset();

    midiNote_ = midiNote;
    velocity_ = velocity;

    if (!library.isNoteAvailable(midiNote)) {
        logger_.log("SynthVoice/start", "error", "Požadovaná nota není dostupná: " + juce::String((int)midiNote));
        isActive_ = false;
        return;
    }

    sampleData_ = library.getSampleData(midiNote);
    sampleLength_ = library.getSampleLength(midiNote);

    if (!sampleData_ || sampleLength_ == 0) {
        logger_.log("SynthVoice/start", "error", "Neplatný vzorek pro notu " + juce::String((int)midiNote));
        isActive_ = false;
        return;
    }

    position_ = 0;
    isActive_ = true;

    logger_.log("SynthVoice/start", "debug", "Spuštěna nota " + juce::String((int)midiNote) +
                                             " délka=" + juce::String(sampleLength_));
}

void SynthVoice::stop()
{
    isActive_ = false;  // Okamžité zastavení (může být rozšířeno o release fázi)
}

void SynthVoice::reset()
{
    midiNote_ = 0;
    velocity_ = 0;
    isActive_ = false;
    sampleData_ = nullptr;
    sampleLength_ = 0;
    position_ = 0;
    queue_ = 0;  // Reset queue na dno
}

/**
 * @brief Renderuje audio s gainem podle velocity.
 * @param outputBuffer Buffer pro přičtení
 * @param numSamples Počet samplů
 */
void SynthVoice::render(float* outputBuffer, int numSamples)
{
    if (!isActive_ || sampleData_ == nullptr || sampleLength_ == 0)
        return;

    const float gain = static_cast<float>(velocity_) / 127.0f;  // Lineární gain z velocity

    for (int i = 0; i < numSamples; ++i) {
        if (position_ >= sampleLength_) {
            stop();  // Dohráno -> deaktivace
            break;
        }
        outputBuffer[i] += sampleData_[position_] * gain;
        ++position_;
    }
}

// ======================== VoiceManager =========================

/**
 * @brief Konstruktor VoiceManager.
 * Vytvoří voices s výchozí queue=0.
 * @param library SampleLibrary
 * @param numVoices Počet hlasů
 */
VoiceManager::VoiceManager(const SampleLibrary& library, int numVoices)
    : logger_(Logger::getInstance()), sampleLibrary_(library)
{
    voices_.reserve(numVoices);
    for (int i = 0; i < numVoices; ++i) {
        voices_.emplace_back(std::make_unique<SynthVoice>());
        voices_.back()->setQueue(0);  // Výchozí queue na 0 (dno stacku)
    }

    logger_.log("VoiceManager/constructor", "info", "VoiceManager vytvořen s " + juce::String(numVoices) + " hlasy");
}

/**
 * @brief Zpracuje MIDI události (note-on/off) z queue.
 * @param midiState MidiStateManager
 * Oprava: Upraveno pro uint8_t z popNoteOn/popNoteOff, kontrola if (raw == 255)
 */
void VoiceManager::processMidiEvents(MidiStateManager& midiState)
{
    // Zpracování NOTE ON
    for (int ch = 0; ch < 16; ++ch) {
        while (true) {
            uint8_t raw = midiState.popNoteOn(static_cast<uint8_t>(ch));  // Oprava: Explicit cast
            if (raw == 255) break;
            uint8_t note = raw;
            uint8_t vel = midiState.getVelocity(static_cast<uint8_t>(ch), note);  // Oprava: Explicit cast
            startVoice(note, vel);
        }
    }

    // Zpracování NOTE OFF
    for (int ch = 0; ch < 16; ++ch) {
        while (true) {
            uint8_t raw = midiState.popNoteOff(static_cast<uint8_t>(ch));  // Oprava: Explicit cast
            if (raw == 255) break;
            uint8_t note = raw;
            stopVoice(note);
        }
    }
}

/**
 * @brief Generuje audio mixem hlasů.
 * @param buffer Audio buffer
 * @param numSamples Počet samplů
 */
void VoiceManager::generateAudio(float* buffer, int numSamples)
{
    if (buffer == nullptr || numSamples <= 0) return;

    // Mix všech aktivních hlasů
    for (auto& v : voices_) {
        if (v->isActive()) v->render(buffer, numSamples);
    }
}

void VoiceManager::refresh()
{
    // Může být rozšířeno o statistiky / voice stealing atd. (aktuálně prázdné)
}

/**
 * @brief Spustí hlas s lepším voice stealingem (inspirováno HW syntetizérem).
 * Nejprve hledá existující, pak volnou s max queue, pak ukradne.
 * @param midiNote Nota
 * @param velocity Velocity
 */
void VoiceManager::startVoice(uint8_t midiNote, uint8_t velocity)
{
    // Nejprve hledej existující voice pro tuto notu
    for (auto& v : voices_) {
        if (v->isActive() && v->getNote() == midiNote) {
            v->start(midiNote, velocity, sampleLibrary_);
            mixleQueue(v->getQueue());  // Přeuspořádej queue
            v->setQueue(static_cast<uint8_t>(voices_.size() - 1));  // Nastav na top
            return;
        }
    }

    // Hledej volnou voice s nejvyšším queue (nejstarší na top)
    SynthVoice* candidate = nullptr;
    uint8_t maxQueue = 0;
    for (auto& v : voices_) {
        if (!v->isActive() && v->getQueue() >= maxQueue) {
            candidate = v.get();
            maxQueue = v->getQueue();
        }
    }

    // Pokud není volná, ukradni s nejvyšším queue
    if (!candidate) {
        for (auto& v : voices_) {
            if (v->getQueue() >= maxQueue) {
                candidate = v.get();
                maxQueue = v->getQueue();
            }
        }
        logger_.log("VoiceManager/startVoice", "warn", "Voice stealing: ukraden voice pro notu " + juce::String((int)midiNote));
    }

    if (candidate) {
        mixleQueue(candidate->getQueue());  // Přeuspořádej
        candidate->start(midiNote, velocity, sampleLibrary_);
        candidate->setQueue(static_cast<uint8_t>(voices_.size() - 1));  // Nastav na top
    }
}

/**
 * @brief Zastaví hlas a přeuspořádá queue.
 * @param midiNote Nota
 */
void VoiceManager::stopVoice(uint8_t midiNote)
{
    for (auto& v : voices_) {
        if (v->isActive() && v->getNote() == midiNote) {
            v->stop();
            mixleQueue(v->getQueue());  // Přeuspořádej po uvolnění
            v->setQueue(0);  // Reset na dno
            return;
        }
    }
}

/**
 * @brief Přeuspořádá queue: Posune vybranou na dno, ostatní posune nahoru/dolů.
 * @param queueNumber Číslo queue k mixlování
 */
void VoiceManager::mixleQueue(uint8_t queueNumber) {
    for (auto& v : voices_) {
        if (v->getQueue() == queueNumber) {
            v->setQueue(0);  // Posun na dno
        } else if (v->getQueue() > queueNumber) {
            v->setQueue(v->getQueue() - 1);  // Posun dolů
        } else {
            v->setQueue(v->getQueue() + 1);  // Posun nahoru
        }
    }
}

/**
 * @brief Vrátí počet aktivních hlasů (pro debug a monitoring).
 * @return Počet aktivních hlasů
 */
int VoiceManager::getActiveVoiceCount() const
{
    int count = 0;
    for (const auto& voice : voices_) {
        if (voice->isActive()) ++count;
    }
    return count;
}