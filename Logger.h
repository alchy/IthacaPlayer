#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_events/juce_events.h>
#include <atomic>

// Definice maximálního počtu logovacích záznamů (sliding window)
#define MAX_LOG_ENTRIES 100

// Forward declaration pro AudioPluginAudioProcessorEditor
class AudioPluginAudioProcessorEditor;

/**
 * Třída Logger - Thread-safe Singleton pro logování událostí v pluginu.
 * 
 * Poskytuje metodu pro logování s timestampem, komponentou, severity a zprávou.
 * Ukládá logy do bufferu s omezenou velikostí (sliding window).
 * Logování lze globálně zapnout/vypnout.
 * Thread-safe aktualizace GUI pomocí MessageManager.
 */
class Logger
{
public:
    // Získání instance singletonu
    static Logger& getInstance();

    // Thread-safe metoda pro logování
    void log(const juce::String& component, const juce::String& severity, const juce::String& message);

    // Globální přepínač logování
    static bool loggingEnabled;

    // Thread-safe nastavení reference na editor
    void setEditor(AudioPluginAudioProcessorEditor* ed);

    // Getter pro přístup k bufferu logů (const reference pro bezpečný přístup)
    const juce::StringArray& getLogBuffer() const { return logBuffer; }

private:
    // Privátní konstruktor pro singleton
    Logger() = default;

    // Zabránění kopírování
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Buffer pro logy
    juce::StringArray logBuffer;

    // Reference na editor pro update GUI
    std::atomic<AudioPluginAudioProcessorEditor*> editor{nullptr};
};