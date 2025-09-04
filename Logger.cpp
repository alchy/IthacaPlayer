#include "Logger.h"
#include "PluginEditor.h"
#include <sstream>

std::atomic<bool> Logger::loggingEnabled{true};

/**
 * @brief Konstruktor Logger.
 * Inicializuje singleton a přidává file logger.
 * Oprava: Použit unique_ptr místo deprecated ScopedPointer.
 */
Logger::Logger() {
    // Oprava: Inicializace file loggeru (umístění v default app log složce)
    fileLogger_ = std::unique_ptr<juce::FileLogger>(
        juce::FileLogger::createDefaultAppLogger("IthacaPlayer", "IthacaPlayer.log", "Start IthacaPlayer logu", 0)
    );
    DBG("Logger initialized.");  // Přidaný debug pro konzoli
}

/**
 * @brief Vrátí singleton instanci Logger.
 * @return Reference na instanci
 */
Logger& Logger::getInstance()
{
    static Logger instance;
    return instance;
}

/**
 * @brief Loguje zprávu s časovým razítkem, komponentou a závažností.
 * @param component Komponenta (např. třída/metoda)
 * @param severity Závažnost (info, debug, error, warn)
 * @param message Zpráva
 * Oprava: Přidán zápis do fileLogger, pokud existuje.
 */
void Logger::log(const juce::String& component, const juce::String& severity, const juce::String& message)
{
    if (!loggingEnabled.load(std::memory_order_relaxed))
        return;

    try {
        auto now = juce::Time::getCurrentTime();
        juce::String timestamp = now.formatted("%Y-%m-%d %H:%M:%S");
        juce::String logEntry = "[" + timestamp + "] [" + component + "] [" + severity + "]: " + message;

        pushToLogQueue(logEntry);
        scheduleGUIUpdate();

        // Oprava: Zápis do souboru, pokud fileLogger existuje
        if (fileLogger_ != nullptr) {
            fileLogger_->logMessage(logEntry);
        }
    } catch (...) {
        // Bezpečný fallback při chybě
        DBG("Logger error in log method.");  // Přidaný debug pro chyby
    }
}

void Logger::pushToLogQueue(const juce::String& logEntry)
{
    std::lock_guard<std::mutex> lock(logMutex_);

    uint8_t writeIndex = logQueue_.writeIndex.load();
    uint8_t currentCount = logQueue_.count.load();

    logQueue_.logs[writeIndex] = logEntry;
    logQueue_.writeIndex.store(static_cast<uint8_t>(writeIndex + 1));

    if (currentCount < 256) {
        logQueue_.count.store(currentCount + 1);
    } else {
        logQueue_.readIndex = static_cast<uint8_t>(logQueue_.readIndex + 1);
    }

    if (logQueue_.count.load() > MAX_LOG_ENTRIES) {
        uint8_t excess = logQueue_.count.load() - MAX_LOG_ENTRIES;
        logQueue_.readIndex = static_cast<uint8_t>(logQueue_.readIndex + excess);
        logQueue_.count.store(MAX_LOG_ENTRIES);
    }
}

void Logger::setEditor(AudioPluginAudioProcessorEditor* ed)
{
    std::lock_guard<std::mutex> lock(editorMutex_);
    editorPtr_ = ed;
    DBG("Editor set in Logger.");  // Přidaný debug pro nastavení editoru
}

void Logger::scheduleGUIUpdate()
{
    juce::MessageManager::callAsync([this]() {
        std::lock_guard<std::mutex> lock(editorMutex_);
        if (editorPtr_ != nullptr) {
            editorPtr_->updateLogDisplay();
        }
    });
}

juce::StringArray Logger::getLogBuffer() const
{
    return getCurrentLogs();
}

juce::StringArray Logger::getCurrentLogs() const
{
    std::lock_guard<std::mutex> lock(logMutex_);

    juce::StringArray result;
    uint8_t currentCount = logQueue_.count.load();
    uint8_t readIndex = logQueue_.readIndex;

    for (uint8_t i = 0; i < currentCount; ++i) {
        uint8_t index = static_cast<uint8_t>(readIndex + i);
        result.add(logQueue_.logs[index]);
    }
    return result;
}

void Logger::clearLogs()
{
    std::lock_guard<std::mutex> lock(logMutex_);
    logQueue_.writeIndex.store(0);
    logQueue_.count.store(0);
    logQueue_.readIndex = 0;
    for (auto& log : logQueue_.logs) {
        log = juce::String();
    }
    DBG("Logs cleared.");  // Přidaný debug pro čištění logů
}

size_t Logger::getLogCount() const
{
    return logQueue_.count.load(std::memory_order_relaxed);
}