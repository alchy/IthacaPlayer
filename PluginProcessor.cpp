#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <pthread.h>
#include <sched.h>
#endif

/**
 * @brief Konstruktor procesoru.
 * Inicializuje komponenty a připravuje triple buffering.
 */
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , logger_(Logger::getInstance())
    , voiceManager_(sampleLibrary_)
{
    logger_.log("PluginProcessor/constructor", "info", "Procesor inicializován.");
    sampleRate_ = 44100.0;
    synthState_.store(SynthState::Uninitialized);
    processingEnabled_.store(false);

    // Inicializace triple buffering
    for (auto& buffer : audioBuffers_) {
        buffer.resize(8192 * 2, 0.0f); // Max velikost pro stereo
    }
    currentReadBuffer_.store(0);
    currentWriteBuffer_.store(0);
    isBufferReady_.store(false);

    // Inicializace MIDI queue
    midiReadIndex_.store(0);
    midiWriteIndex_.store(0);
}

/**
 * @brief Destruktor - uvolní zdroje a ukončí render thread.
 */
AudioPluginAudioProcessor::~AudioPluginAudioProcessor() 
{
    logger_.log("PluginProcessor/destructor", "info", "Procesor uvolněn.");
    stopRenderThread();
    processingEnabled_.store(false);
    synthState_.store(SynthState::Uninitialized);
    sampleLibrary_.clear();
}

/**
 * @brief Připraví plugin na přehrávání.
 */
void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    logger_.log("PluginProcessor/prepareToPlay", "info", "Příprava zahájena.");
    
    if (sampleRate <= 0.0 || sampleRate > 192000.0 || samplesPerBlock <= 0 || samplesPerBlock > 8192) {
        handleSynthError("Neplatné parametry.");
        return;
    }
    
    sampleRate_ = sampleRate;
    samplesPerBlock_ = samplesPerBlock;
    synthState_.store(SynthState::Initializing);
    processingEnabled_.store(false);
    
    // Inicializace bufferů podle samplesPerBlock
    for (auto& buffer : audioBuffers_) {
        buffer.resize(samplesPerBlock * 2, 0.0f); // Stereo: 2 kanály
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    }
    isBufferReady_.store(false);
    
    // Reset MIDI queue
    clearMidiQueue();
    
    initializeSynth();
    
    if (synthState_.load() == SynthState::Ready) {
        processingEnabled_.store(true);
        startRenderThread();
    }
}

/**
 * @brief Uvolní zdroje.
 */
void AudioPluginAudioProcessor::releaseResources()
{
    logger_.log("PluginProcessor/releaseResources", "info", "Uvolňování zdrojů.");
    stopRenderThread();
    processingEnabled_.store(false);
    synthState_.store(SynthState::Uninitialized);
}

/**
 * @brief Inicializuje syntetizér.
 */
void AudioPluginAudioProcessor::initializeSynth()
{
    if (synthState_.load() != SynthState::Initializing) return;
    
    logger_.log("PluginProcessor/initializeSynth", "info", "Inicializace zahájena.");
    
    try {
        if (sampleRate_ <= 0.0) throw std::runtime_error("Neplatný sample rate.");
        
        // Kontrola existujících vzorků
        bool hasSamples = false;
        for (uint8_t note = SampleLibrary::MIN_NOTE; note <= SampleLibrary::MAX_NOTE; ++note) {
            for (uint8_t level = 0; level < 8; ++level) {
                if (sampleLibrary_.isNoteAvailable(note, level)) {
                    hasSamples = true;
                    break;
                }
            }
            if (hasSamples) break;
        }
        
        if (hasSamples) {
            logger_.log("PluginProcessor/initializeSynth", "info", "Vzorky v paměti - rychlá inicializace.");
        } else {
            logger_.log("PluginProcessor/initializeSynth", "info", "Načítání vzorků.");
            auto progressCallback = [this](int current, int total, const juce::String&) {
                if (current % 100 == 0 || current == total) {
                    logger_.log("PluginProcessor/initializeSynth", "debug", 
                                "Průběh: " + juce::String(current) + "/" + juce::String(total));
                }
            };
            sampleLibrary_.initialize(sampleRate_, progressCallback);
        }
        
        // Kontrola dostupnosti
        if (!hasSamples) {
            for (uint8_t note = SampleLibrary::MIN_NOTE; note <= SampleLibrary::MAX_NOTE; ++note) {
                for (uint8_t level = 0; level < 8; ++level) {
                    if (sampleLibrary_.isNoteAvailable(note, level)) {
                        hasSamples = true;
                        break;
                    }
                }
                if (hasSamples) break;
            }
            if (!hasSamples) throw std::runtime_error("Žádné vzorky.");
        }
        
        synthState_.store(SynthState::Ready);
        
    } catch (const std::exception& e) {
        handleSynthError("Chyba: " + juce::String(e.what()));
        synthState_.store(SynthState::Error);
    }
}

void AudioPluginAudioProcessor::handleSynthError(const juce::String& errorMessage)
{
    logger_.log("PluginProcessor/handleSynthError", "error", errorMessage);
    processingEnabled_.store(false);
}

juce::String AudioPluginAudioProcessor::getStateDescription() const
{
    juce::String base;
    switch (synthState_.load()) {
        case SynthState::Uninitialized: base = "Uninitialized"; break;
        case SynthState::Initializing: base = "Initializing"; break;
        case SynthState::Ready: base = "Ready"; break;
        case SynthState::Error: base = "Error"; break;
        default: base = "Unknown";
    }
    base += " (Processing: " + juce::String(processingEnabled_.load() ? "ON" : "OFF") + ")";
    return base;
}

void AudioPluginAudioProcessor::startRenderThread()
{
    if (!renderThread_.joinable()) {
        shouldStop_.store(false);
        renderThread_ = std::thread(&AudioPluginAudioProcessor::renderThreadFunction, this);
        logger_.log("PluginProcessor/startRenderThread", "info", "Render thread spuštěn.");
    }
}

void AudioPluginAudioProcessor::stopRenderThread()
{
    if (renderThread_.joinable()) {
        shouldStop_.store(true);
        renderSignal_.notify_one();
        renderThread_.join();
        logger_.log("PluginProcessor/stopRenderThread", "info", "Render thread ukončen.");
    }
}

/**
 * @brief Lock-free přidání MIDI eventu do queue.
 */
bool AudioPluginAudioProcessor::pushMidiEvent(const juce::MidiMessage& message, int samplePosition)
{
    const int currentWrite = midiWriteIndex_.load(std::memory_order_relaxed);
    const int nextWrite = (currentWrite + 1) % MIDI_QUEUE_SIZE;
    
    if (nextWrite == midiReadIndex_.load(std::memory_order_acquire)) {
        return false; // Queue je plná
    }
    
    midiQueue_[currentWrite] = {message, samplePosition};
    midiWriteIndex_.store(nextWrite, std::memory_order_release);
    return true;
}

/**
 * @brief Lock-free čtení MIDI eventu z queue.
 */
bool AudioPluginAudioProcessor::popMidiEvent(MidiEvent& event)
{
    const int currentRead = midiReadIndex_.load(std::memory_order_relaxed);
    
    if (currentRead == midiWriteIndex_.load(std::memory_order_acquire)) {
        return false; // Queue je prázdná
    }
    
    event = midiQueue_[currentRead];
    midiReadIndex_.store((currentRead + 1) % MIDI_QUEUE_SIZE, std::memory_order_release);
    return true;
}

void AudioPluginAudioProcessor::clearMidiQueue()
{
    midiReadIndex_.store(0);
    midiWriteIndex_.store(0);
}

/**
 * @brief Zpracování MIDI v real-time threadu pro minimální latenci.
 */
void AudioPluginAudioProcessor::processMidiInRealTime(const juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages) {
        auto msg = metadata.getMessage();
        pushMidiEvent(msg, metadata.samplePosition);
    }
    renderSignal_.notify_one(); // Okamžitě probudit render thread
}

/**
 * @brief Kopírování audio bufferu s optimalizací.
 */
void AudioPluginAudioProcessor::copyAudioBuffer(juce::AudioBuffer<float>& dest, const std::vector<float>& src)
{
    const int numSamples = dest.getNumSamples();
    
    if (src.size() >= numSamples * 2) {
        // Použití memcpy pro rychlé kopírování
        std::memcpy(dest.getWritePointer(0), src.data(), numSamples * sizeof(float));
        
        if (dest.getNumChannels() >= 2) {
            std::memcpy(dest.getWritePointer(1), src.data() + numSamples, 
                       numSamples * sizeof(float));
        }
    }
}

/**
 * @brief Hlavní render funkce threadu s optimalizovaným time managementem
 */
void AudioPluginAudioProcessor::renderThreadFunction()
{
    juce::ScopedJuceInitialiser_GUI scopedJuce;
    
    // Nastavení vysoké priority pro real-time audio thread
#ifdef _WIN32
    HANDLE currentThread = GetCurrentThread();
    SetThreadPriority(currentThread, THREAD_PRIORITY_TIME_CRITICAL);
#elif defined(__APPLE__)
    pthread_t thread = pthread_self();
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(thread, SCHED_FIFO, &param);
#endif
    
    // Proměnné pro time management
    double rollingAverage = 2.0; // Počáteční odhad 2ms
    constexpr double targetCycleTime = 2.0; // Cíl 2ms cyklus
    constexpr double alpha = 0.1; // EMA faktor
    
    while (!shouldStop_.load())
    {
        auto cycleStartTime = juce::Time::getMillisecondCounterHiRes();
        
        // PROFILING: Začátek cyklu
        auto profileStart = juce::Time::getMillisecondCounterHiRes();
        
        // === ZPRACOVÁNÍ MIDI ===
        MidiEvent midiEvent;
        int midiEventsProcessed = 0;
        bool hasProcessedMidi = false;
        
        // Limit MIDI processing pro zachování responsiveness
        while (midiEventsProcessed < 100 && popMidiEvent(midiEvent))
        {
            auto msg = midiEvent.message;
            
            if (msg.isNoteOn()) {
                midiState_.pushNoteOn(static_cast<uint8_t>(msg.getChannel() - 1), 
                                    static_cast<uint8_t>(msg.getNoteNumber()), 
                                    static_cast<uint8_t>(msg.getVelocity()));
            } else if (msg.isNoteOff()) {
                midiState_.pushNoteOff(static_cast<uint8_t>(msg.getChannel() - 1), 
                                     static_cast<uint8_t>(msg.getNoteNumber()));
            } else if (msg.isController()) {
                midiState_.setControllerValue(static_cast<uint8_t>(msg.getChannel() - 1), 
                                            static_cast<uint8_t>(msg.getControllerNumber()), 
                                            static_cast<uint8_t>(msg.getControllerValue()));
            }
            
            hasProcessedMidi = true;
            midiEventsProcessed++;
        }
        
        auto midiTime = juce::Time::getMillisecondCounterHiRes() - profileStart;
        
        // === VOICE PROCESSING ===
        if (hasProcessedMidi) {
            voiceManager_.processMidiEvents(midiState_);
        }
        
        auto voiceProcessTime = juce::Time::getMillisecondCounterHiRes() - profileStart - midiTime;
        
        // === AUDIO RENDERING ===
        bool shouldRender = hasProcessedMidi || voiceManager_.getActiveVoiceCount() > 0;
        
        if (shouldRender) {
            const int nextWriteBuffer = (currentWriteBuffer_.load() + 1) % BUFFER_COUNT;
            
            // Vyčistit buffer před renderováním
            std::fill(audioBuffers_[nextWriteBuffer].begin(), 
                     audioBuffers_[nextWriteBuffer].end(), 0.0f);
            
            // Renderovat audio
            voiceManager_.generateAudio(audioBuffers_[nextWriteBuffer].data(), samplesPerBlock_);
            
            // Aktualizovat write buffer
            currentWriteBuffer_.store(nextWriteBuffer);
            isBufferReady_.store(true);
        }
        
        auto audioRenderTime = juce::Time::getMillisecondCounterHiRes() - profileStart - midiTime - voiceProcessTime;
        
        // === HOUSEKEEPING ===
        voiceManager_.refresh();
        
        auto refreshTime = juce::Time::getMillisecondCounterHiRes() - profileStart - midiTime - voiceProcessTime - audioRenderTime;
        
        // PROFILING: Konec cyklu
        auto totalCycleTime = juce::Time::getMillisecondCounterHiRes() - profileStart;
        
        // Update rolling average
        rollingAverage = alpha * totalCycleTime + (1.0 - alpha) * rollingAverage;
        
        // Logování pouze při překročení thresholdu nebo občasně
        static int logCounter = 0;
        if (totalCycleTime > 3.0 || ++logCounter % 100 == 0) {
            logger_.log("PluginProcessor/renderThread", "debug", 
                       juce::String::formatted("Cycle: %.1fms (MIDI: %.1fms, Voice: %.1fms, Audio: %.1fms, Refresh: %.1fms), Avg: %.1fms",
                                              totalCycleTime, midiTime, voiceProcessTime, 
                                              audioRenderTime, refreshTime, rollingAverage));
        }
        
        if (totalCycleTime > 5.0) {
            logger_.log("PluginProcessor/renderThread", "warn", 
                       "Render cycle too long: " + juce::String(totalCycleTime, 1) + "ms");
        }
        
        // === DYNAMIC SLEEP MANAGEMENT ===
        auto cycleEndTime = juce::Time::getMillisecondCounterHiRes();
        auto cycleDuration = cycleEndTime - cycleStartTime;
        
        // Vypočítat optimální dobu spánku
        double sleepTime = (std::max)(0.1, targetCycleTime - cycleDuration);
        
        // Použít přesnější sleep
        if (sleepTime > 0.1) {
            std::this_thread::sleep_for(std::chrono::microseconds(
                static_cast<int>(sleepTime * 1000)));
        }
        // Yield pokud zbývá velmi málo času
        else if (sleepTime > 0.01) {
            std::this_thread::yield();
        }
    }
}

/**
 * @brief Zpracování audio bloku s minimální latencí.
 */
void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals; // Důležité pro výkon!
    
    if (!isReadyForProcessing()) {
        buffer.clear();
        return;
    }

    // 1. Okamžité zpracování MIDI pro minimální latenci
    processMidiInRealTime(midiMessages);

    // 2. Kopírování nejčerstvějšího bufferu
    if (isBufferReady_.load()) {
        const int readBuffer = currentReadBuffer_.load();
        copyAudioBuffer(buffer, audioBuffers_[readBuffer]);
        
        // Přepnout na další buffer pro příští iteraci
        currentReadBuffer_.store((readBuffer + 1) % BUFFER_COUNT);
    } else {
        buffer.clear();
    }

    // 3. Pokud není co renderovat, vypnout signalizaci
    if (midiMessages.isEmpty() && voiceManager_.getActiveVoiceCount() == 0) {
        isBufferReady_.store(false);
    }
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    logger_.log("PluginProcessor/createEditor", "info", "Vytváření editoru.");
    return new AudioPluginAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}