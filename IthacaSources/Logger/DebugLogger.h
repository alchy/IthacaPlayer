#pragma once

#include <juce_core/juce_core.h>  // Pro juce::Logger, juce::File atd.

//==============================================================================
/**
 * Třída pro debug logging s podporou souboru nebo stdout.
 * - Automaticky přidává timestamp (DD-MM-YYYY hh:mm:ss) k každému logu.
 * - Podporuje parametr debug (default false) pro zapnutí/vypnutí.
 * - Výstup: buď do souboru (pokud useFile=true), nebo do stdout/debug konzole.
 * - Inicializuje výstup v konstruktoru pro bezpečnost (zdůvodnění: zabraňuje chybám při prvním volání logu).
 * - Členění: Metody rozděleny do sub-funkcí pro lepší přehlednost (např. initializeOutput, handleLogging).
 */
class DebugLogger : public juce::Logger
{
public:
    // Konstruktor: Inicializuje logger s cestou k souboru, debug módem a volbou výstupu.
    // @param logFilePath Cesta k souboru pro logování (pokud useFile=true).
    // @param debug Zapne/vypne logování (default false).
    // @param useFile Pokud true, loguje do souboru; jinak do stdout (default true).
    explicit DebugLogger(const juce::String& logFilePath, bool debug = false, bool useFile = true)
        : debugMode(debug), useFileOutput(useFile), filePath(logFilePath)
    {
        initializeOutput();  // Inicializace výstupu hned v konstruktoru (zdůvodnění: zajišťuje, že objekty jsou připraveny před prvním použitím).
    }

    // Destruktor: Uvolní resources (např. zavře soubor).
    ~DebugLogger() override
    {
        cleanupOutput();  // Sub-funkce pro uklizení (zdůvodnění: oddělení logiky pro přehlednost).
    }

    // Metoda pro nastavení debug módu dynamicky.
    // - Slouží k změně debug během runtime (např. při testování).
    // @param newDebug Nová hodnota debug módu.
    void setDebugMode(bool newDebug, bool debug = false)
    {
        if (debug) handleLogging("Nastavuji debug mode na: " + juce::String(newDebug ? "true" : "false"));
        debugMode = newDebug;  // Přiřazení nové hodnoty (zdůvodnění: umožňuje flexibilní změnu bez nové instance).
    }

    // Hlavní metoda pro logování: Volá se jako logger.log("zpráva").
    // - Přidá timestamp interně přes JUCE.
    // - Podmíněné na debugMode a per-volání debug.
    // @param message Zpráva k zalogování.
    // @param debug Podpora per-volání debug (default false).
    void log(const juce::String& message, bool debug = false)
    {
        if (debug && debugMode)  // Podpora per-volání debug (zdůvodnění: flexibilita, pokud chceš logovat jen specifické zprávy).
            handleLogging(message);  // Sub-funkce pro zpracování logu.
    }

    // Povinná metoda z juce::Logger: Zde se přidává timestamp a zapisuje.
    void logMessage(const juce::String& message) override
    {
        writeLogWithTimestamp(message);  // Sub-funkce pro přidání timestampu a zápis (zdůvodnění: členění pro přehlednost).
    }

private:
    bool debugMode;         // Slouží k zapnutí/vypnutí logování (true = loguje se).
    bool useFileOutput;     // Slouží k výběru výstupu: true = soubor, false = stdout.
    juce::String filePath;  // Slouží k uložení cesty k log souboru (pokud useFileOutput=true).
    std::unique_ptr<juce::FileOutputStream> fileStream;  // Slouží k zápisu do souboru (inicializováno v initializeOutput).

    // Sub-funkce: Inicializuje výstup (soubor nebo fallback na stdout).
    // Zdůvodnění: Odděleno pro přehlednost, voláno v konstruktoru.
    void initializeOutput()
    {
        if (useFileOutput && !filePath.isEmpty())
        {
            juce::File logFile(filePath);
            logFile.createDirectory();  // Vytvoří složku, pokud neexistuje (zdůvodnění: robustnost proti chybám).
            fileStream = logFile.createOutputStream();
            if (!fileStream)
            {
                juce::Logger::outputDebugString("Chyba: Nelze otevřít log soubor: " + filePath);  // Fallback na stdout při chybě.
            }
        }
        // Jinak použij standardní stdout (bez inicializace, JUCE to zpracuje přes outputDebugString).
    }

    // Sub-funkce: Uklidí výstup (zavře soubor).
    // Zdůvodnění: Odděleno pro přehlednost, voláno v destruktoru.
    void cleanupOutput()
    {
        fileStream.reset();  // Automaticky zavře stream.
    }

    // Sub-funkce: Formátuje timestamp pro log.
    // Zdůvodnění: Odděleno pro přehlednost, aby se logika timestampu neduplikovala.
    juce::String formatTimestamp()
    {
        return juce::Time::getCurrentTime().formatted("%d-%m-%Y %H:%M:%S");
    }

    // Sub-funkce: Zpracuje logování jen pokud debugMode=true.
    // Zdůvodnění: Kondicionální logika oddělena pro přehlednost.
    void handleLogging(const juce::String& message)
    {
        writeToLog(message);  // Volá standardní JUCE metodu, která přidá timestamp.
    }

    // Sub-funkce: Zapisuje log s timestampem do vybraného výstupu.
    // Zdůvodnění: Členění pro přehlednost – tady se rozhoduje mezi souborem a stdout.
    void writeLogWithTimestamp(const juce::String& message)
    {
        juce::String timestampedMessage = "[" + formatTimestamp() + "] " + message;
        if (useFileOutput && fileStream)
        {
            *fileStream << timestampedMessage << juce::newLine;  // Zápis do souboru.
            fileStream->flush();  // Zajistí okamžitý zápis (zdůvodnění: pro real-time debugging).
        }
        else
        {
            juce::Logger::outputDebugString(timestampedMessage);  // Fallback na stdout/debug konzoli.
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DebugLogger)
};