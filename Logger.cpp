#include "Logger.h"
#include "PluginEditor.h"
#include <sstream>

std::atomic<bool> Logger::loggingEnabled{true};

Logger::Logger() {}

Logger& Logger::getInstance()
{
    static Logger instance;
    return instance;
}

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
    } catch (...) {
        // bezpečný fallback
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
}

size_t Logger::getLogCount() const
{
    return logQueue_.count.load(std::memory_order_relaxed);
}
