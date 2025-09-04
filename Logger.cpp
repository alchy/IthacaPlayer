#include "Logger.h"
#include "PluginEditor.h"

// Inicializace statické proměnné
bool Logger::loggingEnabled = true;

/**
 * Získání instance singletonu.
 */
Logger& Logger::getInstance()
{
    static Logger instance;
    return instance;
}

/**
 * Metoda pro logování s thread-safety pro GUI aktualizace.
 */
void Logger::log(const juce::String& component, const juce::String& severity, const juce::String& message)
{
    if (!loggingEnabled) return;

    // Vytvoření timestampu
    juce::String timestamp = juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M:%S");

    // Formátovaný log
    juce::String logEntry = "[" + timestamp + "] [" + component + "] [" + severity + "]: " + message;

    // Přidání do bufferu
    logBuffer.add(logEntry);

    // Omezení velikosti (sliding window)
    if (logBuffer.size() > MAX_LOG_ENTRIES)
    {
        logBuffer.removeRange(0, 1);
    }

    // Thread-safe aktualizace GUI
    auto currentEditor = editor.load();
    if (currentEditor != nullptr)
    {
        // Použití MessageManager pro bezpečnou aktualizaci GUI z libovolného vlákna
        juce::MessageManager::callAsync([this, currentEditor]() {
            // Dvojitá kontrola po async volání
            if (editor.load() == currentEditor && currentEditor != nullptr)
            {
                currentEditor->updateLogDisplay();
            }
        });
    }
}

/**
 * Thread-safe nastavení reference na editor.
 */
void Logger::setEditor(AudioPluginAudioProcessorEditor* ed)
{
    editor.store(ed);
}