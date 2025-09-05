#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <mutex>
#include <array>
#include <memory>

#define MAX_LOG_ENTRIES 100

/**
 * @class Logger
 * @brief Singleton logger pro souborové logování (bez GUI závislosti).
 * 
 * Podporuje logování s časovým razítkem, závažností a komponentou. Refaktorováno: Odstraněna GUI aktualizace, zůstalo jen souborové logování pro debugging.
 */
class Logger
{
public:
    static Logger& getInstance();

    void log(const juce::String& component, const juce::String& severity, const juce::String& message);
    static std::atomic<bool> loggingEnabled;

    void clearLogs();

private:
    Logger();
    ~Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    struct LogQueue {
        std::array<juce::String, 256> logs;
        std::atomic<uint8_t> writeIndex{0};
        std::atomic<uint8_t> count{0};
        uint8_t readIndex{0};
    };

    LogQueue logQueue_;

    mutable std::mutex logMutex_;  // Mutex pro queue

    std::unique_ptr<juce::FileLogger> fileLogger_;  // File logger pro souborový výstup

    void pushToLogQueue(const juce::String& logEntry);
};