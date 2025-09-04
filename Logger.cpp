#include "Logger.h"
#include "PluginEditor.h"

// Inicializace globálního přepínače logování.
bool Logger::loggingEnabled = true;

/**
 * Metoda pro získání instance singletonu.
 */
Logger& Logger::getInstance()
{
    static Logger instance;
    return instance;
}

/**
 * Metoda pro thread-safe logování.
 * Vytváří formátovaný log s timestampem a přidává do bufferu.
 * Aktualizuje GUI přes MessageManager.
 */
void Logger::log(const juce::String& component, const juce::String& severity, const juce::String& message)
{
    if (!loggingEnabled) return;

    // Vytvoření timestampu pro log.
    juce::String timestamp = juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M:%S");

    // Formátování logovacího záznamu.
    juce::String logEntry = "[" + timestamp + "] [" + component + "] [" + severity + "]: " + message;

    // Přidání do circular bufferu.
    pushToLogQueue(logEntry);

    // Thread-safe aktualizace GUI přes MessageManager.
    auto currentEditor = editor.load();
    if (currentEditor != nullptr)
    {
        juce::MessageManager::callAsync([this, currentEditor]() {
            // Dvojitá kontrola pro bezpečnost po async volání.
            if (editor.load() == currentEditor && currentEditor != nullptr)
            {
                currentEditor->updateLogDisplay();
            }
        });
    }
}

/**
 * Metoda pro thread-safe nastavení reference na editor.
 */
void Logger::setEditor(AudioPluginAudioProcessorEditor* ed)
{
    editor.store(ed);
}

/**
 * Pomocná metoda pro přidání logu do circular bufferu.
 * Pokud buffer překročí MAX_LOG_ENTRIES, přepíše nejstarší položku (sliding window).
 */
void Logger::pushToLogQueue(const juce::String& logEntry)
{
    logQueue.logs[logQueue.writeIndex] = logEntry;
    logQueue.writeIndex++;  // uint8 overflow automaticky wrap-around na 0 po 255
    if (logQueue.count < 256) {
        logQueue.count++;
    } else {
        // Sliding window: Pokud plný, posun readIndex (přepis nejstaršího).
        logQueue.readIndex++;
    }
    
    // Omezení na MAX_LOG_ENTRIES (pokud je menší než 256, ručně ořezat).
    if (logQueue.count > MAX_LOG_ENTRIES) {
        logQueue.readIndex = (logQueue.readIndex + (logQueue.count - MAX_LOG_ENTRIES)) % 256;
        logQueue.count = MAX_LOG_ENTRIES;
    }
}

/**
 * Pomocná metoda pro získání aktuálních logů jako StringArray.
 * Používá se pro přístup k bufferu z GUI.
 */
juce::StringArray Logger::getCurrentLogs() const
{
    juce::StringArray result;
    uint8_t index = logQueue.readIndex;
    for (uint8_t i = 0; i < logQueue.count; ++i) {
        result.add(logQueue.logs[index]);
        index++;  // uint8 wrap-around automaticky
    }
    return result;
}

/**
 * Metoda pro získání aktuálních logů jako StringArray.
 */
juce::StringArray Logger::getLogBuffer() const
{
    return getCurrentLogs();
}