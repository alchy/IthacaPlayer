#include "SampleLoader.h"
#include <cmath>  // Pro std::sin, std::pow atd.

// Definice konstant
const float SampleLoader::DYNAMIC_AMPLITUDES[8] = {0.05f, 0.1f, 0.2f, 0.3f, 0.4f, 0.6f, 0.8f, 1.0f};

/**
 * @brief Konstruktor SampleLoader.
 * Inicializuje format manager a logger.
 */
SampleLoader::SampleLoader(double sampleRate)
    : sampleRate_(sampleRate), logger_(Logger::getInstance())
{
    formatManager_.registerBasicFormats();  // Registrace WAV atd.
}

/**
 * @brief Načte kompletní instrument s optimalizovaným loading procesem.
 * Iteruje přes noty a levels s batch processing pro rychlost.
 */
std::vector<LoadedSample> SampleLoader::loadInstrument(
    const juce::File& instrumentDirectory,
    ProgressCallback progressCallback
) {
    std::vector<LoadedSample> loadedSamples;
    loadingStats_ = LoadingStats{};  // Reset statistik
    
    double startTime = juce::Time::getMillisecondCounterHiRes();
    loadingStats_.totalExpected = (MAX_NOTE - MIN_NOTE + 1) * NUM_DYNAMIC_LEVELS;
    
    // OPTIMALIZACE: Předalokace vektoru pro eliminaci realokací
    loadedSamples.reserve(loadingStats_.totalExpected);
    
    // Vytvoření directory pokud neexistuje
    if (!instrumentDirectory.exists()) {
        if (!instrumentDirectory.createDirectory()) {
            logger_.log("SampleLoader/loadInstrument", "error", 
                       "Cannot create instrument directory: " + instrumentDirectory.getFullPathName());
            return loadedSamples;
        }
        logger_.log("SampleLoader/loadInstrument", "info", 
                   "Created instrument directory");
    }
    
    int processed = 0;
    int batchLogCounter = 0;
    
    // Načtení všech kombinací nota × dynamic level s optimalizovaným reportingem
    for (uint8_t note = MIN_NOTE; note <= MAX_NOTE; ++note) {
        for (uint8_t level = 0; level < NUM_DYNAMIC_LEVELS; ++level) {
            try {
                // Progress callback optimalizace - pouze každých 50 samples nebo první/poslední
                if (progressCallback && (processed % 50 == 0 || processed < 10 || processed == loadingStats_.totalExpected - 1)) {
                    juce::String status = "Loading note " + juce::String((int)note) + 
                                        " level " + juce::String((int)level) + 
                                        " (" + juce::String(processed + 1) + "/" + juce::String(loadingStats_.totalExpected) + ")";
                    progressCallback(processed, loadingStats_.totalExpected, status);
                }
                
                LoadedSample sample = loadSingleSample(instrumentDirectory, note, level);
                loadedSamples.emplace_back(std::move(sample));  // Použij move pro přesun
                
                // Update statistik
                loadingStats_.totalMemoryUsed += loadedSamples.back().getDataSize();
                if (loadedSamples.back().isGenerated) {
                    loadingStats_.filesGenerated++;
                } else {
                    loadingStats_.filesLoaded++;
                }
                
                ++processed;
                
                // Batch logging každých 100 samples pro snížení noise
                if (++batchLogCounter >= 100) {
                    logger_.log("SampleLoader/loadInstrument", "debug", 
                               "Batch progress: " + juce::String(processed) + "/" + juce::String(loadingStats_.totalExpected) + 
                               " samples processed");
                    batchLogCounter = 0;
                }
                
            } catch (const std::exception& e) {
                logger_.log("SampleLoader/loadInstrument", "error",
                           "Error loading note " + juce::String((int)note) + 
                           " level " + juce::String((int)level) + ": " + juce::String(e.what()));
                ++processed; // Pokračujeme i při chybě
            }
        }
    }
    
    // Finální statistiky
    loadingStats_.loadingTimeSeconds = (juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0;
    double samplesPerSecond = (loadingStats_.loadingTimeSeconds > 0) ? (processed / loadingStats_.loadingTimeSeconds) : 0;
    
    logger_.log("SampleLoader/loadInstrument", "info",
               "Loading completed: " + juce::String(loadingStats_.filesLoaded) + " loaded, " +
               juce::String(loadingStats_.filesGenerated) + " generated, " +
               juce::String(loadingStats_.filesSaved) + " saved, " +
               juce::String(loadingStats_.totalMemoryUsed / (1024*1024)) + "MB, " +
               juce::String(loadingStats_.loadingTimeSeconds, 2) + "s" +
               " (avg: " + juce::String(samplesPerSecond, 1) + " samples/sec)");
    
    return loadedSamples;
}

/**
 * @brief Načte jeden konkrétní sample s optimalizovanou fallback logikou.
 * Pokusí se načíst pro target SR, fallback na base SR s resamplingem a ukládáním.
 */
LoadedSample SampleLoader::loadSingleSample(
    const juce::File& instrumentDirectory,
    uint8_t midiNote, 
    uint8_t dynamicLevel
) {
    double baseSR = 44100.0;
    double otherSR = (std::abs(sampleRate_ - 44100.0) < 1.0) ? 48000.0 : 44100.0;
    
    // 1. Zkus target SR
    juce::String targetFilename = generateFilename(midiNote, dynamicLevel, sampleRate_);
    juce::File targetFile = instrumentDirectory.getChildFile(targetFilename);
    if (targetFile.exists()) {
        LoadedSample sample = loadWavFile(targetFile, midiNote, dynamicLevel);
        
        // Redukované logging - pouze každý 200. úspěšný load
        static int successCounter = 0;
        if (++successCounter % 200 == 0) {
            logger_.log("SampleLoader/loadSingleSample", "debug",
                       "Batch: loaded " + juce::String(successCounter) + " files from disk");
        }
        
        return sample;
    }
    
    // 2. Zkus other SR a resampluj
    juce::String otherFilename = generateFilename(midiNote, dynamicLevel, otherSR);
    juce::File otherFile = instrumentDirectory.getChildFile(otherFilename);
    if (otherFile.exists()) {
        LoadedSample otherSample = loadWavFile(otherFile, midiNote, dynamicLevel);  // Načti a resampluj interně
        saveGeneratedSample(otherSample, targetFile);  // Ulož resamplovanou verzi pro target
        loadingStats_.filesSaved++;
        
        logger_.log("SampleLoader/loadSingleSample", "info",
                   "Fallback resampling: " + otherFilename + " -> " + targetFilename);
        return otherSample;
    }
    
    // 3. Generuj pro base (44100)
    LoadedSample baseSample = generateSineWave(midiNote, dynamicLevel);  // Generuj pro baseSR
    baseSample.originalSampleRate = baseSR;
    juce::String baseFilename = generateFilename(midiNote, dynamicLevel, baseSR);
    juce::File baseFile = instrumentDirectory.getChildFile(baseFilename);
    saveGeneratedSample(baseSample, baseFile);  // Ulož base
    loadingStats_.filesSaved++;
    
    // 4. Resampluj na 48000 a ulož
    uint32_t resampledLength;
    auto resampledData = resampleIfNeeded(baseSample.audioData.get(), baseSample.lengthSamples, baseSR, resampledLength);
    LoadedSample resampledSample(std::move(baseSample));  // Použij move konstruktor
    resampledSample.audioData = std::move(resampledData);
    resampledSample.lengthSamples = resampledLength;
    resampledSample.originalSampleRate = 48000.0;
    juce::String resampledFilename = generateFilename(midiNote, dynamicLevel, 48000.0);
    juce::File resampledFile = instrumentDirectory.getChildFile(resampledFilename);
    saveGeneratedSample(resampledSample, resampledFile);
    loadingStats_.filesSaved++;
    
    // Logging pouze pro generované samples
    static int generateCounter = 0;
    if (++generateCounter % 50 == 0) {
        logger_.log("SampleLoader/loadSingleSample", "info",
                   "Generated " + juce::String(generateCounter) + " sine wave samples");
    }
    
    // Vrátíme verzi pro target SR
    return (std::abs(sampleRate_ - baseSR) < 1.0) ? std::move(baseSample) : std::move(resampledSample);
}

/**
 * @brief Načte WAV soubor s optimalizovaným resampling procesem.
 */
LoadedSample SampleLoader::loadWavFile(const juce::File& file, uint8_t midiNote, uint8_t dynamicLevel) {
    FileAnalysis analysis = analyzeWavFile(file);
    if (!analysis.isValid) {
        throw std::runtime_error("Invalid WAV file: " + analysis.errorMessage.toStdString());
    }
    
    LoadedSample result;
    result.midiNote = midiNote;
    result.dynamicLevel = dynamicLevel;
    result.isGenerated = false;
    result.sourcePath = file.getFullPathName();
    result.originalSampleRate = analysis.originalSampleRate;
    result.lengthSamples = analysis.targetLengthSamples;
    
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager_.createReaderFor(file));
    if (!reader) {
        throw std::runtime_error("Cannot create reader for: " + file.getFullPathName().toStdString());
    }
    
    result.numChannels = static_cast<uint8_t>(std::min(2, static_cast<int>(reader->numChannels)));
    
    // OPTIMALIZACE: Jednorázová alokace pro celý výsledný buffer
    size_t totalSamples = static_cast<size_t>(analysis.targetLengthSamples) * result.numChannels;
    result.audioData = std::make_unique<float[]>(totalSamples);
    
    if (analysis.needsResampling) {
        // Resampling path s optimalizovaným temp bufferem
        juce::AudioBuffer<float> tempBuffer(static_cast<int>(reader->numChannels), 
                                           static_cast<int>(analysis.originalLengthSamples));
        if (!reader->read(tempBuffer.getArrayOfWritePointers(), 
                         static_cast<int>(reader->numChannels), 
                         0, 
                         static_cast<int>(analysis.originalLengthSamples))) {
            throw std::runtime_error("Error reading audio data for resampling");
        }
        
        // OPTIMALIZACE: Přímý resampling bez extra alokací
        for (int ch = 0; ch < result.numChannels; ++ch) {
            int sourceChannel = std::min(ch, static_cast<int>(reader->numChannels) - 1);
            const float* sourceData = tempBuffer.getReadPointer(sourceChannel);
            
            uint32_t outputLength;
            auto resampledChannel = resampleIfNeeded(
                sourceData, 
                analysis.originalLengthSamples, 
                analysis.originalSampleRate, 
                outputLength
            );
            
            // Kopírování do interleaved bufferu
            for (uint32_t i = 0; i < outputLength; ++i) {
                result.audioData[i * result.numChannels + ch] = resampledChannel[i];
            }
        }
    } else {
        // Direct loading path bez resamplingu
        juce::AudioBuffer<float> tempBuffer(static_cast<int>(reader->numChannels), 
                                           static_cast<int>(analysis.originalLengthSamples));
        if (!reader->read(tempBuffer.getArrayOfWritePointers(), 
                         static_cast<int>(reader->numChannels), 
                         0, 
                         static_cast<int>(analysis.originalLengthSamples))) {
            throw std::runtime_error("Error reading audio data");
        }
        
        // OPTIMALIZACE: Přímé kopírování do interleaved bufferu
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
 * @brief Vygeneruje sine wave pro danou notu a dynamic level (pro base SR 44100).
 */
LoadedSample SampleLoader::generateSineWave(uint8_t midiNote, uint8_t dynamicLevel) {
    LoadedSample result;
    result.midiNote = midiNote;
    result.dynamicLevel = dynamicLevel;
    result.isGenerated = true;
    result.originalSampleRate = 44100.0;  // Vždy pro base SR
    result.lengthSamples = static_cast<uint32_t>(result.originalSampleRate * SAMPLE_SECONDS);
    result.numChannels = 1; // Generované samples jsou mono
    result.sourcePath = "Generated sine wave";
    
    result.audioData = std::make_unique<float[]>(result.lengthSamples);
    
    double frequency = getFrequencyForNote(midiNote);
    float amplitude = getDynamicAmplitude(dynamicLevel);
    
    const double twoPi = 2.0 * juce::MathConstants<double>::pi;
    const double phaseInc = twoPi * frequency / result.originalSampleRate;
    
    // OPTIMALIZACE: Rychlé generování bez logging per sample
    for (uint32_t i = 0; i < result.lengthSamples; ++i) {
        double phase = phaseInc * static_cast<double>(i);
        result.audioData[i] = amplitude * static_cast<float>(std::sin(phase));
    }
    
    return result;
}

/**
 * @brief Uloží vygenerovaný sample do .wav souboru s optimalizovaným error handlingem.
 */
bool SampleLoader::saveGeneratedSample(const LoadedSample& sample, const juce::File& targetFile) {
    try {
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::FileOutputStream> outputStream(targetFile.createOutputStream());
        
        if (!outputStream) {
            // Redukované error logging - pouze když se opakuje
            static int saveErrorCounter = 0;
            if (++saveErrorCounter % 10 == 1) {  // Log každou 10. chybu
                logger_.log("SampleLoader/saveGeneratedSample", "error",
                           "Cannot create output stream (error #" + juce::String(saveErrorCounter) + ")");
            }
            return false;
        }
        
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(
                outputStream.get(),
                sample.originalSampleRate,
                sample.numChannels,   
                16,                   
                {},                   
                0                     
            )
        );
        
        if (!writer) {
            logger_.log("SampleLoader/saveGeneratedSample", "error", "Cannot create WAV writer");
            return false;
        }
        
        outputStream.release(); 
        
        // OPTIMALIZACE: Streamlined channel handling
        if (sample.numChannels == 1) {
            const float* channelData = sample.audioData.get();
            writer->writeFromFloatArrays(&channelData, 1, sample.lengthSamples);
        } else {
            // Rychlé de-interleaving pro stereo
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
        
        // Redukované success logging
        static int saveSuccessCounter = 0;
        if (++saveSuccessCounter % 100 == 0) {
            logger_.log("SampleLoader/saveGeneratedSample", "debug",
                       "Batch: saved " + juce::String(saveSuccessCounter) + " generated files");
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger_.log("SampleLoader/saveGeneratedSample", "error",
                   "Exception while saving: " + juce::String(e.what()));
        return false;
    } catch (...) {
        logger_.log("SampleLoader/saveGeneratedSample", "error", "Unknown exception while saving");
        return false;
    }
}

/**
 * @brief Analyzuje WAV soubor bez načtení dat (pro memory planning).
 */
FileAnalysis SampleLoader::analyzeWavFile(const juce::File& file) {
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
    
    int channels = std::min(2, static_cast<int>(reader->numChannels));
    analysis.memoryRequired = analysis.targetLengthSamples * channels * sizeof(float);
    analysis.isValid = validateFileAnalysis(analysis);
    
    return analysis;
}

/**
 * @brief Validuje výsledky file analýzy.
 */
bool SampleLoader::validateFileAnalysis(const FileAnalysis& analysis) {
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
 * @brief Provede resampling pokud je potřeba s optimalizovanou interpolací.
 */
std::unique_ptr<float[]> SampleLoader::resampleIfNeeded(
    const float* sourceData, 
    uint32_t sourceLength, 
    double sourceSampleRate,
    uint32_t& outputLength
) {
    double ratio = sampleRate_ / sourceSampleRate;
    outputLength = static_cast<uint32_t>(sourceLength * ratio);
    
    auto outputData = std::make_unique<float[]>(outputLength);
    
    // OPTIMALIZACE: Rychlá lineární interpolace
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

/**
 * @brief Vrátí default instrument directory.
 */
juce::File SampleLoader::getDefaultInstrumentDirectory() {
    juce::File appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    juce::File ithacaDir = appDataDir.getChildFile("IthacaPlayer");
    juce::File instrumentDir = ithacaDir.getChildFile("instrument");
    
    return instrumentDir;
}

/**
 * @brief Vygeneruje název souboru podle naming convention.
 */
juce::String SampleLoader::generateFilename(uint8_t midiNote, uint8_t dynamicLevel, double sr) {
    juce::String srSuffix = (std::abs(sr - 44100.0) < 1.0) ? "44" : "48";
    juce::String filename = "m" + juce::String(midiNote).paddedLeft('0', 3) + 
                           "-vel" + juce::String(dynamicLevel) + 
                           "-" + srSuffix + ".wav";
    return filename;
}

/**
 * @brief Parsuje název souboru a extrahuje parametry.
 */
bool SampleLoader::parseFilename(const juce::String& filename, uint8_t& midiNote, uint8_t& dynamicLevel, double& sr) {
    if (!filename.startsWith("m") || !filename.endsWith(".wav")) {
        return false;
    }
    
    juce::StringArray parts = juce::StringArray::fromTokens(filename.upToLastOccurrenceOf(".wav", false, false), "-", "");
    if (parts.size() != 3) {
        return false;
    }
    
    juce::String noteStr = parts[0].substring(1);
    juce::String levelStr = parts[1].substring(3);
    juce::String srStr = parts[2];
    
    int note = noteStr.getIntValue();
    int level = levelStr.getIntValue();
    sr = (srStr == "44") ? 44100.0 : (srStr == "48" ? 48000.0 : 0.0);
    
    if (note < MIN_NOTE || note > MAX_NOTE || level < 0 || level >= NUM_DYNAMIC_LEVELS || sr == 0.0) {
        return false;
    }
    
    midiNote = static_cast<uint8_t>(note);
    dynamicLevel = static_cast<uint8_t>(level);
    
    return true;
}

/**
 * @brief Mapuje velocity na dynamic level.
 */
uint8_t SampleLoader::velocityToDynamicLevel(uint8_t velocity) {
    if (velocity == 0) return 0;
    return std::min(static_cast<uint8_t>(7), static_cast<uint8_t>((velocity - 1) / 16));
}

/**
 * @brief Vrátí amplitude pro dynamic level.
 */
float SampleLoader::getDynamicAmplitude(uint8_t dynamicLevel) {
    if (dynamicLevel >= NUM_DYNAMIC_LEVELS) {
        return DYNAMIC_AMPLITUDES[NUM_DYNAMIC_LEVELS - 1];
    }
    return DYNAMIC_AMPLITUDES[dynamicLevel];
}

/**
 * @brief Vrátí frekvenci pro MIDI notu.
 */
double SampleLoader::getFrequencyForNote(uint8_t midiNote) const {
    return 440.0 * std::pow(2.0, (static_cast<int>(midiNote) - 69) / 12.0);
}