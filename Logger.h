#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <mutex>
#include <array>

#define MAX_LOG_ENTRIES 100

class AudioPluginAudioProcessorEditor;

class Logger
{
public:
    static Logger& getInstance();

    void log(const juce::String& component, const juce::String& severity, const juce::String& message);
    static std::atomic<bool> loggingEnabled;

    void setEditor(AudioPluginAudioProcessorEditor* ed);

    juce::StringArray getLogBuffer() const;
    void clearLogs();
    size_t getLogCount() const;

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

    mutable std::mutex logMutex_;
    mutable std::mutex editorMutex_;

    // 🔧 oprava: už jen raw pointer (JUCE spravuje lifecycle editoru)
    AudioPluginAudioProcessorEditor* editorPtr_{nullptr};

    void pushToLogQueue(const juce::String& logEntry);
    juce::StringArray getCurrentLogs() const;
    void scheduleGUIUpdate();
};
