#include "SampleLoader.h"
#include <cmath>
#include <fstream>

// Definice static array pro MSVC kompatibilitu
const float SampleLoader::DYNAMIC_AMPLITUDES[8] = {
    0.05f,  // vel0 - pppp (velocity 1-16)
    0.12f,  // vel1 - ppp  (velocity 17-32)
    0.22f,  // vel2 - pp   (velocity 33-48)
    0.35f,  // vel3 - p    (velocity 49-64)
    0.50f,  // vel4 - mp   (velocity 65-80)
    0.68f,  // vel5 - mf   (velocity 81-96)
    0.85f,  // vel6 - f    (velocity 97-112)
    1.00f   // vel7 - ff   (velocity 113-127)
};

/**
 * @brief Konstruktor SampleLoader
 */
SampleLoader::SampleLoader(double sampleRate)
    : sampleRate_(sampleRate), logger_(Logger::getInstance())
{
    // Registrace audio formátů (WAV, AIFF, atd.)
    formatManager_.registerBasicFormats();
    
    logger_.log("SampleLoader/constructor", "info", 
               "SampleLoader inicializován se sample rate " + juce::String(sampleRate_));
}

/**
 * @brief Načte kompletní instrument - všechny noty × všechny dynamic levels
 */
std::vector<LoadedSample> SampleLoader::loadInstrument(
    const juce::File& instrumentDirectory,
    ProgressCallback progressCallback)
{
    auto startTime = juce::Time::getMillisecondCounterHiRes();
    std::vector<LoadedSample> loadedSamples;
    
    // Reset statistik
    loadingStats_ = LoadingStats();
    loadingStats_.totalExpected = (MAX_NOTE - MIN_NOTE + 1) * NUM_DYNAMIC_LEVELS; // 88 × 8 = 704
    
    logger_.log("SampleLoader/loadInstrument", "info", 
               "Začátek načítání instrumentu z directory: " + instrumentDirectory.getFullPathName());
    logger_.log("SampleLoader/loadInstrument", "info", 
               "Očekáváno " + juce::String(loadingStats_.totalExpected) + " souborů");
    
    // Zajištění existence directory
    if (!instrumentDirectory.exists()) {
        if (!instrumentDirectory.createDirectory()) {
            logger_.log("SampleLoader/loadInstrument", "error", 
                       "Nelze vytvořit directory: " + instrumentDirectory.getFullPathName());
            return loadedSamples;
        }
        logger_.log("SampleLoader/loadInstrument", "info", 
                   "Vytvořen instrument directory");
    }
    
    int processed = 0;
    
    // Načtení všech kombinací nota × dynamic level
    for (uint8_t note = MIN_NOTE; note <= MAX_NOTE; ++note) {
        for (uint8_t level = 0; level < NUM_DYNAMIC_LEVELS; ++level) {
            try {
                // Progress callback
                if (progressCallback) {
                    juce::String status = "Načítám notu " + juce::String((int)note) + 
                                        " úroveň " + juce::String((int)level);
                    progressCallback(processed, loadingStats_.totalExpected, status);
                }
                
                LoadedSample sample = loadSingleSample(instrumentDirectory, note, level);
                loadedSamples.push_back(std::move(sample));
                
                ++processed;
                
            } catch (const std::exception& e) {
                logger_.log("SampleLoader/loadInstrument", "error",
                           "Chyba při načítání noty " + juce::String((int)note) + 
                           " level " + juce::String((int)level) + ": " + juce::String(e.what()));
                ++processed; // Pokračujeme i při chybě
            }
        }
    }
    
    // Finální statistiky
    loadingStats_.loadingTimeSeconds = (juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0;
    
    logger_.log("SampleLoader/loadInstrument", "info",
               "Loading dokončen: " + juce::String(loadingStats_.filesLoaded) + " načteno, " +
               juce::String(loadingStats_.filesGenerated) + " vygenerováno, " +
               juce::String(loadingStats_.filesSaved) + " uloženo, " +
               juce::String(loadingStats_.totalMemoryUsed / (1024*1024)) + "MB, " +
               juce::String(loadingStats_.loadingTimeSeconds, 2) + "s");
    
    return loadedSamples;
}

/**
 * @brief Načte jeden konkrétní sample
 */
LoadedSample SampleLoader::loadSingleSample(
    const juce::File& instrumentDirectory,
    uint8_t midiNote, 
    uint8_t dynamicLevel)
{
    juce::String filename = generateFilename(midiNote, dynamicLevel);
    juce::File sampleFile = instrumentDirectory.getChildFile(filename);
    
    logger_.log("SampleLoader/loadSingleSample", "debug",
               "Pokus o načtení: " + sampleFile.getFullPathName());
    
    // Pokus o načtení existujícího souboru
    if (sampleFile.exists()) {
        try {
            LoadedSample sample = loadWavFile(sampleFile, midiNote, dynamicLevel);
            loadingStats_.filesLoaded++;
            loadingStats_.totalMemoryUsed += sample.getDataSize();
            
            logger_.log("SampleLoader/loadSingleSample", "debug",
                       "Úspěšně načten soubor: " + filename + 
                       " (" + juce::String(sample.isStereo() ? "stereo" : "mono") + ")");
            return sample;
            
        } catch (const std::exception& e) {
            logger_.log("SampleLoader/loadSingleSample", "warn",
                       "Chyba při načítání " + filename + ": " + juce::String(e.what()) + 
                       " - generuji náhradu");
        }
    } else {
        logger_.log("SampleLoader/loadSingleSample", "warn",
                   "Soubor neexistuje, generuji placeholder: " + filename);
    }
    
    // Fallback: generování sine wave
    try {
        LoadedSample sample = generateSineWave(midiNote, dynamicLevel);
        loadingStats_.filesGenerated++;
        loadingStats_.totalMemoryUsed += sample.getDataSize();
        
        // Pokus o uložení vygenerovaného sample
        if (saveGeneratedSample(sample, sampleFile)) {
            loadingStats_.filesSaved++;
            logger_.log("SampleLoader/loadSingleSample", "info",
                       "Generovaný sample uložen: " + filename);
        }
        
        return sample;
        
    } catch (const std::exception& e) {
        logger_.log("SampleLoader/loadSingleSample", "error",
                   "Fatální chyba při generování sample pro " + filename + 
                   ": " + juce::String(e.what()));
        throw;
    }
}

/**
 * @brief Načte WAV soubor s optional resampling
 */
LoadedSample SampleLoader::loadWavFile(const juce::File& file, uint8_t midiNote, uint8_t dynamicLevel)
{
    // 1. Analýza souboru
    FileAnalysis analysis = analyzeWavFile(file);
    if (!analysis.isValid) {
        throw std::runtime_error("Invalid WAV file: " + analysis.errorMessage.toStdString());
    }
    
    // 2. Vytvoření LoadedSample
    LoadedSample result;
    result.midiNote = midiNote;
    result.dynamicLevel = dynamicLevel;
    result.isGenerated = false;
    result.sourcePath = file.getFullPathName();
    result.originalSampleRate = analysis.originalSampleRate;
    result.lengthSamples = analysis.targetLengthSamples;
    
    // 3. Načtení dat pomocí správného JUCE API
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager_.createReaderFor(file));
    if (!reader) {
        throw std::runtime_error("Cannot create reader for: " + file.getFullPathName().toStdString());
    }
    
    // Určení počtu kanálů (zachováme stereo pokud existuje)
    result.numChannels = static_cast<uint8_t>(std::min(2, static_cast<int>(reader->numChannels))); // Max 2 kanály
    
    // 4. Alokace paměti pro výsledek (interleaved pro stereo)
    size_t totalSamples = static_cast<size_t>(analysis.targetLengthSamples) * result.numChannels;
    result.audioData = std::make_unique<float[]>(totalSamples);
    
    if (analysis.needsResampling) {
        // Resampling potřebný
        logger_.log("SampleLoader/loadWavFile", "debug",
                   "Resampling " + juce::String(analysis.originalSampleRate) + 
                   "Hz -> " + juce::String(sampleRate_) + "Hz, channels: " + juce::String(result.numChannels));
        
        // Načtení original dat pomocí správného JUCE API
        juce::AudioBuffer<float> tempBuffer(static_cast<int>(reader->numChannels), 
                                           static_cast<int>(analysis.originalLengthSamples));
        
        // Správné volání JUCE read metody
        if (!reader->read(tempBuffer.getArrayOfWritePointers(), 
                         static_cast<int>(reader->numChannels), 
                         0, 
                         static_cast<int>(analysis.originalLengthSamples))) {
            throw std::runtime_error("Chyba při načítání audio dat pro resampling");
        }
        
        // Resampling a interleaving pro každý kanál
        for (int ch = 0; ch < result.numChannels; ++ch) {
            // Použij dostupný kanál (mono soubor → duplicate, stereo → use channel)
            int sourceChannel = std::min(ch, static_cast<int>(reader->numChannels) - 1);
            const float* sourceData = tempBuffer.getReadPointer(sourceChannel);
            
            uint32_t outputLength;
            auto resampledChannel = resampleIfNeeded(
                sourceData, 
                analysis.originalLengthSamples, 
                analysis.originalSampleRate, 
                outputLength
            );
            
            // Interleave do result.audioData
            for (uint32_t i = 0; i < outputLength; ++i) {
                result.audioData[i * result.numChannels + ch] = resampledChannel[i];
            }
        }
        
    } else {
        // Přímé načtení bez resampling
        juce::AudioBuffer<float> tempBuffer(static_cast<int>(reader->numChannels), 
                                           static_cast<int>(analysis.originalLengthSamples));
        
        // Správné volání JUCE read metody
        if (!reader->read(tempBuffer.getArrayOfWritePointers(), 
                         static_cast<int>(reader->numChannels), 
                         0, 
                         static_cast<int>(analysis.originalLengthSamples))) {
            throw std::runtime_error("Chyba při načítání audio dat");
        }
        
        // Kopírování/interleaving do result
        for (uint32_t i = 0; i < analysis.originalLengthSamples; ++i) {
            for (int ch = 0; ch < result.numChannels; ++ch) {
                int sourceChannel = std::min(ch, static_cast<int>(reader->numChannels) - 1);
                result.audioData[i * result.numChannels + ch] = tempBuffer.getSample(sourceChannel, static_cast<int>(i));
            }
        }
    }
    
    return result;
}

/**
 * @brief Vygeneruje sine wave pro danou notu a dynamic level
 */
LoadedSample SampleLoader::generateSineWave(uint8_t midiNote, uint8_t dynamicLevel)
{
    logger_.log("SampleLoader/generateSineWave", "info",
               "Generování sine vlny pro notu " + juce::String((int)midiNote) + 
               ", vrstva " + juce::String((int)dynamicLevel));
    
    LoadedSample result;
    result.midiNote = midiNote;
    result.dynamicLevel = dynamicLevel;
    result.isGenerated = true;
    result.originalSampleRate = sampleRate_;
    result.lengthSamples = static_cast<uint32_t>(sampleRate_ * SAMPLE_SECONDS);
    result.numChannels = 1; // Generované samples jsou mono
    result.sourcePath = "Generated sine wave";
    
    // Alokace paměti pro mono
    result.audioData = std::make_unique<float[]>(result.lengthSamples);
    
    // Generování sine wave s správnou amplitudou pro dynamic level
    double frequency = getFrequencyForNote(midiNote);
    float amplitude = getDynamicAmplitude(dynamicLevel);
    
    const double twoPi = 2.0 * juce::MathConstants<double>::pi;
    const double phaseInc = twoPi * frequency / sampleRate_;
    
    for (uint32_t i = 0; i < result.lengthSamples; ++i) {
        double phase = phaseInc * static_cast<double>(i);
        result.audioData[i] = amplitude * static_cast<float>(std::sin(phase));
    }
    
    logger_.log("SampleLoader/generateSineWave", "info",
               "Sine vlna vygenerována, délka=" + juce::String(result.lengthSamples) + 
               " (mono)");
    
    return result;
}

/**
 * @brief Uloží vygenerovaný sample do .lau souboru
 */
bool SampleLoader::saveGeneratedSample(const LoadedSample& sample, const juce::File& targetFile)
{
    logger_.log("SampleLoader/saveGeneratedSample", "info",
               "Začátek ukládání generovaného souboru: " + targetFile.getFullPathName());
    
    try {
        // Vytvoření WAV formátu
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::FileOutputStream> outputStream(targetFile.createOutputStream());
        
        if (!outputStream) {
            logger_.log("SampleLoader/saveGeneratedSample", "error",
                       "Nelze vytvořit output stream pro: " + targetFile.getFullPathName());
            return false;
        }
        
        // Vytvoření writer
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(
                outputStream.get(),
                sampleRate_,
                sample.numChannels,   // Správný počet kanálů
                16,                   // 16-bit
                {},                   // metadata
                0                     // quality hint
            )
        );
        
        if (!writer) {
            logger_.log("SampleLoader/saveGeneratedSample", "error",
                       "Nelze vytvořit WAV writer");
            return false;
        }
        
        outputStream.release(); // writer převzal vlastnictví
        
        // Příprava dat pro zápis - JUCE očekává planar format
        if (sample.numChannels == 1) {
            // Mono - direct write
            const float* channelData = sample.audioData.get();
            writer->writeFromFloatArrays(&channelData, 1, sample.lengthSamples);
        } else {
            // Stereo - deinterleave pro JUCE
            auto leftChannel = std::make_unique<float[]>(sample.lengthSamples);
            auto rightChannel = std::make_unique<float[]>(sample.lengthSamples);
            
            for (uint32_t i = 0; i < sample.lengthSamples; ++i) {
                leftChannel[i] = sample.audioData[i * 2];
                rightChannel[i] = sample.audioData[i * 2 + 1];
            }
            
            const float* channels[] = { leftChannel.get(), rightChannel.get() };
            writer->writeFromFloatArrays(channels, 2, sample.lengthSamples);
        }
        
        writer->flush();
        
        logger_.log("SampleLoader/saveGeneratedSample", "info",
                   "Soubor úspěšně uložen: " + targetFile.getFullPathName() +
                   " (" + juce::String(sample.lengthSamples) + " samples, " +
                   juce::String(sample.numChannels) + " channels)");
        
        return true;
        
    } catch (const std::exception& e) {
        logger_.log("SampleLoader/saveGeneratedSample", "error",
                   "Výjimka při ukládání: " + juce::String(e.what()));
        return false;
    } catch (...) {
        logger_.log("SampleLoader/saveGeneratedSample", "error",
                   "Neznámá výjimka při ukládání");
        return false;
    }
}

/**
 * @brief Analyzuje WAV soubor bez načtení dat
 */
FileAnalysis SampleLoader::analyzeWavFile(const juce::File& file)
{
    FileAnalysis analysis;
    
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager_.createReaderFor(file));
    if (!reader) {
        analysis.errorMessage = "Cannot create reader";
        return analysis;
    }
    
    analysis.originalLengthSamples = static_cast<uint32_t>(reader->lengthInSamples);
    analysis.originalSampleRate = reader->sampleRate;
    analysis.needsResampling = (std::abs(analysis.originalSampleRate - sampleRate_) > 1.0);
    
    if (analysis.needsResampling) {
        analysis.targetLengthSamples = static_cast<uint32_t>(
            analysis.originalLengthSamples * (sampleRate_ / analysis.originalSampleRate)
        );
    } else {
        analysis.targetLengthSamples = analysis.originalLengthSamples;
    }
    
    // Memory pro stereo
    int channels = std::min(2, static_cast<int>(reader->numChannels));
    analysis.memoryRequired = analysis.targetLengthSamples * channels * sizeof(float);
    analysis.isValid = validateFileAnalysis(analysis);
    
    return analysis;
}

/**
 * @brief Validuje výsledky file analýzy
 */
bool SampleLoader::validateFileAnalysis(const FileAnalysis& analysis)
{
    if (analysis.originalLengthSamples == 0) {
        return false;
    }
    
    if (analysis.originalSampleRate <= 0.0 || analysis.originalSampleRate > 192000.0) {
        return false;
    }
    
    if (analysis.memoryRequired > 1024 * 1024 * 1024) { // Max 1GB per sample
        return false;
    }
    
    return true;
}

/**
 * @brief Provede resampling pokud je potřeba
 */
std::unique_ptr<float[]> SampleLoader::resampleIfNeeded(
    const float* sourceData, 
    uint32_t sourceLength, 
    double sourceSampleRate,
    uint32_t& outputLength)
{
    // Jednoduchý lineární resampling
    double ratio = sampleRate_ / sourceSampleRate;
    outputLength = static_cast<uint32_t>(sourceLength * ratio);
    
    auto outputData = std::make_unique<float[]>(outputLength);
    
    for (uint32_t i = 0; i < outputLength; ++i) {
        double sourceIndex = static_cast<double>(i) / ratio;
        uint32_t index1 = static_cast<uint32_t>(sourceIndex);
        uint32_t index2 = std::min(index1 + 1, sourceLength - 1);
        
        double fraction = sourceIndex - static_cast<double>(index1);
        outputData[i] = static_cast<float>(
            sourceData[index1] * (1.0 - fraction) + sourceData[index2] * fraction
        );
    }
    
    return outputData;
}

// === Static Utility Methods ===

juce::File SampleLoader::getDefaultInstrumentDirectory()
{
    juce::File appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    juce::File ithacaDir = appDataDir.getChildFile("IthacaPlayer");
    juce::File instrumentDir = ithacaDir.getChildFile("instrument");
    
    Logger::getInstance().log("SampleLoader/getDefaultInstrumentDirectory", "debug",
                             "Vrácen default directory: " + instrumentDir.getFullPathName());
    
    return instrumentDir;
}

juce::String SampleLoader::generateFilename(uint8_t midiNote, uint8_t dynamicLevel)
{
    juce::String filename = "m" + juce::String(midiNote).paddedLeft('0', 3) + 
                           "-vel" + juce::String(dynamicLevel) + ".lau";
    
    Logger::getInstance().log("SampleLoader/generateFilename", "debug",
                             "Vygenerován název souboru: " + filename + 
                             " pro notu " + juce::String((int)midiNote) + 
                             ", vrstva " + juce::String((int)dynamicLevel));
    
    return filename;
}

bool SampleLoader::parseFilename(const juce::String& filename, uint8_t& midiNote, uint8_t& dynamicLevel)
{
    // Parse pattern: mXXX-velY.lau
    if (!filename.startsWith("m") || !filename.endsWith(".lau")) {
        return false;
    }
    
    int dashPos = filename.indexOf("-vel");
    if (dashPos == -1) {
        return false;
    }
    
    juce::String noteStr = filename.substring(1, dashPos);
    juce::String levelStr = filename.substring(dashPos + 4, filename.length() - 4);
    
    int note = noteStr.getIntValue();
    int level = levelStr.getIntValue();
    
    if (note < MIN_NOTE || note > MAX_NOTE || level < 0 || level >= NUM_DYNAMIC_LEVELS) {
        return false;
    }
    
    midiNote = static_cast<uint8_t>(note);
    dynamicLevel = static_cast<uint8_t>(level);
    
    return true;
}

uint8_t SampleLoader::velocityToDynamicLevel(uint8_t velocity)
{
    // Map velocity 0-127 to dynamic level 0-7
    if (velocity == 0) return 0;
    return std::min(static_cast<uint8_t>(7), static_cast<uint8_t>((velocity - 1) / 16));
}

float SampleLoader::getDynamicAmplitude(uint8_t dynamicLevel)
{
    if (dynamicLevel >= NUM_DYNAMIC_LEVELS) {
        return DYNAMIC_AMPLITUDES[NUM_DYNAMIC_LEVELS - 1];
    }
    return DYNAMIC_AMPLITUDES[dynamicLevel];
}

double SampleLoader::getFrequencyForNote(uint8_t midiNote) const
{
    // Standardní formule A4=440Hz (MIDI 69)
    return 440.0 * std::pow(2.0, (static_cast<int>(midiNote) - 69) / 12.0);
}