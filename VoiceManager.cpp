#include "VoiceManager.h"

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4244) // Conversion warnings pro MIDI values
#endif

// SynthVoice implementace
SynthVoice::SynthVoice(int voiceNumber)
    : voiceNumber_(voiceNumber)
    , currentNote_(0)
    , velocity_(0)
    , isPlaying_(false)
    , queuePosition_(0)
    , samplePosition_(0)
    , pitchWheel_(0)
    , logger_(Logger::getInstance())
{
    logger_.log("SynthVoice/constructor", "debug", "Voice " + juce::String(voiceNumber_) + " vytvoren");
}

void SynthVoice::play(bool gate, uint8_t note)
{
    if (gate) {
        // Note ON
        currentNote_ = note;
        isPlaying_ = true;
        samplePosition_ = 0; // Reset sample position na začátek
        
        logger_.log("SynthVoice/play", "info", "Voice " + juce::String(voiceNumber_) + 
                    " Note ON: " + juce::String(note));
    } else {
        // Note OFF - podle vašeho kódu jen vypneme gate
        isPlaying_ = false;
        
        logger_.log("SynthVoice/play", "info", "Voice " + juce::String(voiceNumber_) + 
                    " Note OFF: " + juce::String(currentNote_));
    }
}

void SynthVoice::setVelocity(uint8_t velocity)
{
    velocity_ = velocity;
    logger_.log("SynthVoice/setVelocity", "debug", "Voice " + juce::String(voiceNumber_) + 
                " velocity: " + juce::String(velocity));
}

void SynthVoice::setPitchWheel(int16_t pitchWheel)
{
    pitchWheel_ = pitchWheel;
    // Logování pouze při změně
    static int16_t lastPitchWheel = 0;
    if (pitchWheel != lastPitchWheel) {
        logger_.log("SynthVoice/setPitchWheel", "debug", "Voice " + juce::String(voiceNumber_) + 
                    " pitch wheel: " + juce::String(pitchWheel));
        lastPitchWheel = pitchWheel;
    }
}

void SynthVoice::generateAudio(float* outputBuffer, int numSamples, const SampleLibrary& sampleLibrary)
{
    if (!isPlaying_ || outputBuffer == nullptr) {
        return;
    }
    
    // Pro prototyp používáme pouze notu 60 (middle C) ze sample library
    const float* sampleData = sampleLibrary.getSampleData(SAMPLE_NOTE_FOR_PROTOTYPE);
    uint32_t sampleLength = sampleLibrary.getSampleLength(SAMPLE_NOTE_FOR_PROTOTYPE);
    
    if (sampleData == nullptr || sampleLength == 0) {
        return;
    }
    
    // Generování audio samples
    for (int i = 0; i < numSamples; i++) {
        if (samplePosition_ < sampleLength) {
            // Velocity scaling (0-127 -> 0.0-1.0)
            float velocityScale = velocity_ / 127.0f;
            outputBuffer[i] += sampleData[samplePosition_] * velocityScale;
            samplePosition_++;
        } else {
            // Konec sample - vypni hlas (bez loop podle požadavku)
            isPlaying_ = false;
            break;
        }
    }
}

void SynthVoice::reset()
{
    isPlaying_ = false;
    currentNote_ = 0;
    velocity_ = 0;
    samplePosition_ = 0;
    pitchWheel_ = 0;
    
    logger_.log("SynthVoice/reset", "debug", "Voice " + juce::String(voiceNumber_) + " reset");
}

// VoiceManager implementace
VoiceManager::VoiceManager(int maxVoices)
    : voiceCount_(0)
    , globalPitchWheel_(0)
    , logger_(Logger::getInstance())
{
    // Omezení počtu hlasů
    int actualVoiceCount = juce::jmin(maxVoices, MAX_VOICES);
    
    // Vytvoření hlasů (podobné vašemu build_dumb_voices)
    for (int i = 0; i < actualVoiceCount; i++) {
        voices_[i] = new SynthVoice(i);
        voices_[i]->setQueue(i); // Inicializační queue pozice
    }
    
    setVoiceCount(actualVoiceCount);
    
    logger_.log("VoiceManager/constructor", "info", "VoiceManager vytvoren s " + juce::String(voiceCount_) + " hlasy");
}

VoiceManager::~VoiceManager()
{
    logger_.log("VoiceManager/destructor", "info", "Uvolnovani VoiceManager");
    
    for (int i = 0; i < voiceCount_; i++) {
        delete voices_[i];
        voices_[i] = nullptr;
    }
    
    logger_.log("VoiceManager/destructor", "info", "VoiceManager uvolnen");
}

void VoiceManager::play(bool gate, uint8_t note, uint8_t velocity)
{
    if (!gate) {
        // Note OFF - najdi všechny hlasy s touto notou a vypni je
        for (int voice = 0; voice < voiceCount_; voice++) {
            if (voices_[voice]->getNote() == note) {
                voices_[voice]->play(false, note);
            }
        }
    } else {
        // Note ON - najdi volný hlas nebo ukradni jeden
        int freeVoice = getFreeVoice(note);
        voices_[freeVoice]->play(true, note);
        voices_[freeVoice]->setVelocity(velocity); // Velocity se nastavuje pouze při note on
    }
}

void VoiceManager::setPitchWheel(int16_t pitchWheel)
{
    globalPitchWheel_ = pitchWheel;
    
    // Aplikuj na všechny hlasy
    for (int voice = 0; voice < voiceCount_; voice++) {
        voices_[voice]->setPitchWheel(pitchWheel);
    }
}

void VoiceManager::generateAudio(float* outputBuffer, int numSamples, const SampleLibrary& sampleLibrary)
{
    if (outputBuffer == nullptr || numSamples <= 0) {
        return;
    }
    
    // Vyčištění output bufferu
    for (int i = 0; i < numSamples; i++) {
        outputBuffer[i] = 0.0f;
    }
    
    // Mix všech aktivních hlasů
    for (int voice = 0; voice < voiceCount_; voice++) {
        if (voices_[voice]->getGate()) {
            voices_[voice]->generateAudio(outputBuffer, numSamples, sampleLibrary);
        }
    }
}

void VoiceManager::refresh()
{
    // Podobné vašemu performer.refresh() - zde můžeme implementovat dodatečnou logiku
    // Pro zatím neděláme nic speciálního
}

void VoiceManager::processMidiEvents(MidiStateManager& midiState)
{
    // Zpracuj všechny MIDI kanály (pro prototyp používáme kanál 0)
    uint8_t channel = 0;
    
    // Zpracuj note ON události
    uint8_t key;
    while ((key = midiState.popNoteOn(channel)) != 0xff) {
        uint8_t velocity = midiState.getVelocity(channel, key);
        play(true, key, velocity);
        
        logger_.log("VoiceManager/processMidiEvents", "info", "MIDI Note ON zpracovana: key=" + 
                    juce::String(key) + " vel=" + juce::String(velocity));
    }
    
    // Zpracuj note OFF události
    while ((key = midiState.popNoteOff(channel)) != 0xff) {
        play(false, key, 0);
        
        logger_.log("VoiceManager/processMidiEvents", "info", "MIDI Note OFF zpracovana: key=" + 
                    juce::String(key));
    }
    
    // Zpracuj pitch wheel
    int16_t pitchWheel = midiState.getPitchWheel();
    if (pitchWheel != globalPitchWheel_) {
        setPitchWheel(pitchWheel);
    }
}

// Implementace vašeho voice allocation algoritmu
int VoiceManager::getFreeVoice(uint8_t note)
{
    int voice, voiceCandidate = -1;
    
    // První průchod: hledej hlas který už hraje tuto notu nebo který přestal hrát a nebyl nahrazen
    for (voice = 0; voice < voiceCount_; voice++) {
        if (voices_[voice]->getNote() == note) {
            return voice; // Našel hlas s touto notou
        }
    }
    
    // Druhý průchod: najdi nepoužívaný hlas s nejvyšším queue číslem
    for (voice = 0; voice < voiceCount_; voice++) {
        if (!voices_[voice]->getGate()) { // Hlas není aktivní
            if (voiceCandidate == -1) {
                voiceCandidate = voice; // První kandidát
            } else {
                if (voices_[voice]->getQueue() > voices_[voiceCandidate]->getQueue()) {
                    voiceCandidate = voice; // Tento má vyšší queue pozici
                }
            }
        }
    }
    
    if (voiceCandidate != -1) {
        mixleQueue(voices_[voiceCandidate]->getQueue()); // Přeorganizuj queue
        return voiceCandidate;
    }
    
    // Třetí průchod: musíme ukradnout hlas - vyber ten s nejvyšším queue číslem
    for (voice = 0; voice < voiceCount_; voice++) {
        if (voices_[voice]->getQueue() == voiceCount_ - 1) { // Hlas na vrcholu stacku
            mixleQueue(voices_[voice]->getQueue());
            return voice;
        }
    }
    
    return 0; // Fallback - tohle by se nikdy nemělo stát
}

// Implementace vašeho mixle_queue algoritmu
void VoiceManager::mixleQueue(int queueNumber)
{
    // První průchod: najdi hlas s daným queue číslem a nastav ho na 0
    for (int alpha = 0; alpha < voiceCount_; alpha++) {
        if (voices_[alpha]->getQueue() == queueNumber) {
            voices_[alpha]->setQueue(0);
        } else {
            voices_[alpha]->setQueue(voices_[alpha]->getQueue() + 1);
        }
    }
    
    // Druhý průchod: komprese - snižit všechny queue pozice vyšší než queueNumber
    for (int alpha = 0; alpha < voiceCount_; alpha++) {
        if (voices_[alpha]->getQueue() > queueNumber) {
            voices_[alpha]->setQueue(voices_[alpha]->getQueue() - 1);
        }
    }
    
    logger_.log("VoiceManager/mixleQueue", "debug", "Queue reorganizovana pro pozici " + juce::String(queueNumber));
}

#ifdef _WIN32
#pragma warning(pop)
#endif