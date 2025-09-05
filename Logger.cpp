#include "Logger.h"
#include <sstream>

std::atomic<bool> Logger::loggingEnabled{true};

/**
 * @brief Konstruktor Logger.
 * Inicializuje singleton a file logger (umístění v app data složce).
 */
Logger::Logger() {
    // Inicializace file loggeru
    fileLogger_ = std::unique_ptr<juce::FileLogger>(
        juce::FileLogger::createDefaultAppLogger("IthacaPlayer", "IthacaPlayer.log", "Start IthacaPlayer logu", 0)
    );
}

/**
 * @brief Vrátí singleton instanci Logger.
 * @return Reference na instanci.
 */
Logger& Logger::getInstance()
{
    static Logger instance;
    return instance;
}

/**
 * @brief Loguje zprávu s časovým razítkem, komponentou a závažností.
 * Zapisuje jen do souboru (bez GUI).
 * @param component Komponenta (např. třída/metoda).
 * @param severity Závažnost (info, debug, error, warn).
 * @param message Zpráva.
 */
void Logger::log(const juce::String& component, const juce::String& severity, const juce::String& message)
{
    if (!loggingEnabled.load(std::memory_order_relaxed))
        return;

    auto now = juce::Time::getCurrentTime();
    juce::String timestamp = now.formatted("%Y-%m-%d %H:%M:%S");
    juce::String logEntry = "[" + timestamp + "] [" + component + "] [" + severity + "]: " + message;

    pushToLogQueue(logEntry);

    // Zápis do souboru
    if (fileLogger_ != nullptr) {
        fileLogger_->logMessage(logEntry);
    }
}

/**
 * @brief Přidá log do queue (pro interní ukládání, pokud potřeba).
 * @param logEntry Logovací záznam.
 */
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

/**
 * @brief Vyčistí logy v queue.
 */
void Logger::clearLogs()
{
    std::lock_guard<std::mutex> lock(logMutex_);
    logQueue_.writeIndex.store(0);
    logQueue_.count.store(0);
    logQueue_.readIndex = 0;
    for (auto& log : logQueue_.logs) {
        log = juce::String();
    }
}