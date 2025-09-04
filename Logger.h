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
 * Logger - třída pro thread-safe logování událostí v pluginu (singleton).
 * Ukládá logy do circular bufferu s omezenou velikostí.
 * Logování lze globálně zapnout/vypnout.
 * Používá MessageManager pro bezpečnou aktualizaci GUI.
 */
class Logger
{
public:
    // Metoda pro získání instance singletonu.
    static Logger& getInstance();

    // Metoda pro thread-safe logování s timestampem, komponentou, severity a zprávou.
    void log(const juce::String& component, const juce::String& severity, const juce::String& message);

    // Globální přepínač pro zapnutí/vypnutí logování.
    static bool loggingEnabled;

    // Metoda pro thread-safe nastavení reference na editor (pro GUI update).
    void setEditor(AudioPluginAudioProcessorEditor* ed);

    // Metoda pro získání aktuálních logů jako StringArray.
    juce::StringArray getLogBuffer() const;

private:
    // Privátní konstruktor pro singleton pattern.
    Logger() = default;

    // Zabránění kopírování instance.
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Struktura pro circular buffer logů (velikost 256 pro uint8 wrap-around).
    struct LogQueue {
        juce::String logs[256];     // Fixed array pro logovací zprávy
        uint8_t readIndex;          // Index pro čtení (uint8 pro automatický wrap-around)
        uint8_t writeIndex;         // Index pro zápis (uint8 pro automatický wrap-around)
        uint8_t count;              // Počet položek v bufferu
        
        LogQueue() : readIndex(0), writeIndex(0), count(0) {}
    };
    
    LogQueue logQueue;  // Circular buffer pro logy

    // Reference na editor pro thread-safe GUI update.
    std::atomic<AudioPluginAudioProcessorEditor*> editor{nullptr};
    
    // Pomocná metoda pro přidání logu do circular bufferu s omezením velikosti.
    void pushToLogQueue(const juce::String& logEntry);
    
    // Pomocná metoda pro získání aktuálních logů jako StringArray (pro GUI).
    juce::StringArray getCurrentLogs() const;
};