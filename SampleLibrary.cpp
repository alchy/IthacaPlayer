#include "SampleLibrary.h"
#include <cmath>

/**
 * @brief Konstruktor SampleLibrary
 */
SampleLibrary::SampleLibrary()
    : logger_(Logger::getInstance())
{
    logger_.log("SampleLibrary/constructor", "info", "SampleLibrary inicializována s dynamic levels");
}

/**
 * @brief Inicializuje sample library s hybridním loading systémem
 */
void SampleLibrary::initialize(double sampleRate, 
                              std::function<void(int, int, const juce::String&)> progressCallback)
{
    if (sampleRate <= 0.0) {
        logger_.log("SampleLibrary/initialize", "error", "Invalid sampleRate: " + juce::String(sampleRate));
        throw std::invalid_argument("Invalid sampleRate");
    }

    auto startTime = juce::Time::getMillisecondCounterHiRes();
    
    sampleRate_ = sampleRate;
    clear();

    logger_.log("SampleLibrary/initialize", "info",
                "Začátek inicializace se sample rate=" + juce::String(sampleRate_) + 
                " s dynamic levels");

    // Reset statistik
    loadingStats_ = SampleLibraryStats();

    try {
        // Vytvoření SampleLoader
        SampleLoader loader(sampleRate);
        juce::File instrumentDir = SampleLoader::getDefaultInstrumentDirectory();
        
        // Ujištění, že directory existuje
        if (!instrumentDir.exists()) {
            if (!instrumentDir.createDirectory()) {
                throw std::runtime_error("Nelze vytvořit instrument directory: " + 
                                       instrumentDir.getFullPathName().toStdString());
            }
            logger_.log("SampleLibrary/initialize", "info", 
                       "Vytvořen instrument directory: " + instrumentDir.getFullPathName());
        }

        // Progress callback wrapper
        auto progressWrapper = [this, progressCallback](int current, int total, const juce::String& status) {
            if (progressCallback) {
                progressCallback(current, total, status);
            }
            // Logování každého 50. sample pro snížení noise
            if (current % 50 == 0 || current == total) {
                logger_.log("SampleLibrary/initialize", "debug", 
                           "Progress: " + juce::String(current) + "/" + juce::String(total) + 
                           " - " + status);
            }
        };

        // Načtení všech samples
        std::vector<LoadedSample> loadedSamples = loader.loadInstrument(instrumentDir, progressWrapper);
        
        // Uložení samples do interní struktury
        for (const auto& sample : loadedSamples) {
            try {
                storeSample(sample);
                loadingStats_.totalSamples++;
                
                if (sample.isGenerated) {
                    loadingStats_.generatedSines++;
                } else {
                    loadingStats_.loadedFromFiles++;
                }
                
            } catch (const std::exception& e) {
                logger_.log("SampleLibrary/initialize", "error",
                           "Chyba při ukládání sample noty " + juce::String((int)sample.midiNote) + 
                           " level " + juce::String((int)sample.dynamicLevel) + 
                           ": " + juce::String(e.what()));
            }
        }
        
        // Převzetí statistik z SampleLoader
        const auto& loaderStats = loader.getLoadingStats();
        loadingStats_.savedToFiles = loaderStats.filesSaved;
        loadingStats_.totalMemoryUsed = getTotalMemoryUsage();
        loadingStats_.loadingTimeSeconds = (juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0;

        logger_.log("SampleLibrary/initialize", "info",
                   "Inicializace dokončena: " + loadingStats_.getDescription());
        
        // Kontrola, zda máme nějaké samples
        if (loadingStats_.totalSamples == 0) {
            throw std::runtime_error("Žádné samples nebyly načteny!");
        }
        
        // Kontrola dostupnosti základních not pro debugging
        AvailabilityInfo availInfo = getAvailabilityInfo();
        logger_.log("SampleLibrary/initialize", "info",
                   "Dostupné noty: " + juce::String(availInfo.notesWithAnyLevel) + "/" + 
                   juce::String(MAX_NOTE - MIN_NOTE + 1) + 
                   " (mono: " + juce::String(availInfo.monoSamples) + 
                   ", stereo: " + juce::String(availInfo.stereoSamples) + ")");
        
        // Log dynamic level distribution
        juce::String levelDistribution = "Dynamic levels: ";
        for (int i = 0; i < NUM_DYNAMIC_LEVELS; ++i) {
            levelDistribution += "L" + juce::String(i) + ":" + juce::String(availInfo.levelCounts[i]) + " ";
        }
        logger_.log("SampleLibrary/initialize", "info", levelDistribution);
        
    } catch (const std::exception& e) {
        logger_.log("SampleLibrary/initialize", "error",
                   "Fatální chyba při inicializaci: " + juce::String(e.what()));
        throw;
    } catch (...) {
        logger_.log("SampleLibrary/initialize", "error",
                   "Neznámá fatální chyba při inicializaci");
        throw std::runtime_error("Unknown error during initialization");
    }
}

/**
 * @brief Vyčistí všechny samples
 */
void SampleLibrary::clear()
{
    std::lock_guard<std::mutex> lock(accessMutex_);
    
    logger_.log("SampleLibrary/clear", "info", "Začátek čištění SampleLibrary");
    
    size_t totalFreed = 0;
    int segmentsCleared = 0;
    
    for (auto& segment : sampleSegments_) {
        if (segment.midiNote != 0) { // Segment má nějaká data
            totalFreed += segment.getMemoryUsage();
            segment.reset();
            segmentsCleared++;
        }
    }

    // Reset statistik
    loadingStats_ = SampleLibraryStats();

    logger_.log("SampleLibrary/clear", "info", 
               "SampleLibrary vyčištěna - uvolněno " + juce::String(segmentsCleared) + 
               " segmentů, " + juce::String(totalFreed / (1024*1024)) + "MB");
}

/**
 * @brief Vrátí sample data pro konkrétní notu a dynamic level
 */
const float* SampleLibrary::getSampleData(uint8_t midiNote, uint8_t dynamicLevel) const
{
    if (!isValidNote(midiNote) || !isValidDynamicLevel(dynamicLevel)) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(accessMutex_);
    return sampleSegments_[midiNote].getLayerData(dynamicLevel);
}

/**
 * @brief Vrátí délku sample pro konkrétní notu a dynamic level
 */
uint32_t SampleLibrary::getSampleLength(uint8_t midiNote, uint8_t dynamicLevel) const
{
    if (!isValidNote(midiNote) || !isValidDynamicLevel(dynamicLevel)) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(accessMutex_);
    return sampleSegments_[midiNote].getLayerLength(dynamicLevel);
}

/**
 * @brief Zkontroluje dostupnost konkrétního dynamic levelu
 */
bool SampleLibrary::isNoteAvailable(uint8_t midiNote, uint8_t dynamicLevel) const
{
    if (!isValidNote(midiNote) || !isValidDynamicLevel(dynamicLevel)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(accessMutex_);
    return sampleSegments_[midiNote].isLayerAvailable(dynamicLevel);
}

/**
 * @brief Zkontroluje zda je sample stereo
 */
bool SampleLibrary::isSampleStereo(uint8_t midiNote, uint8_t dynamicLevel) const
{
    if (!isValidNote(midiNote) || !isValidDynamicLevel(dynamicLevel)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(accessMutex_);
    return sampleSegments_[midiNote].isLayerStereo(dynamicLevel);
}

/**
 * @brief Uloží načtený sample do interní struktury
 */
void SampleLibrary::storeSample(const LoadedSample& sample)
{
    if (!isValidNote(sample.midiNote) || !isValidDynamicLevel(sample.dynamicLevel)) {
        throw std::invalid_argument("Invalid MIDI note or dynamic level");
    }
    
    if (!sample.audioData || sample.lengthSamples == 0) {
        throw std::invalid_argument("Invalid sample data");
    }

    std::lock_guard<std::mutex> lock(accessMutex_);
    
    SampleSegment& segment = sampleSegments_[sample.midiNote];
    segment.midiNote = sample.midiNote;
    
    // Kopírování dat (nutné kvůli unique_ptr)
    size_t totalSamples = static_cast<size_t>(sample.lengthSamples) * sample.numChannels;
    auto dataCopy = std::make_unique<float[]>(totalSamples);
    std::copy(sample.audioData.get(), 
              sample.audioData.get() + totalSamples, 
              dataCopy.get());
    
    bool isStereo = (sample.numChannels == 2);
    segment.storeLayer(sample.dynamicLevel, std::move(dataCopy), sample.lengthSamples, isStereo);
    
    logger_.log("SampleLibrary/storeSample", "debug",
               "Uložen sample nota " + juce::String((int)sample.midiNote) + 
               " level " + juce::String((int)sample.dynamicLevel) + 
               " (" + juce::String(sample.lengthSamples) + " samples, " +
               juce::String(isStereo ? "stereo" : "mono") + ", " +
               juce::String(sample.isGenerated ? "generated" : "loaded") + ")");
}

/**
 * @brief Mapuje MIDI velocity na dynamic level
 */
uint8_t SampleLibrary::velocityToDynamicLevel(uint8_t velocity)
{
    return SampleLoader::velocityToDynamicLevel(velocity);
}

/**
 * @brief Vrátí celkovou spotřebu paměti
 */
size_t SampleLibrary::getTotalMemoryUsage() const
{
    std::lock_guard<std::mutex> lock(accessMutex_);
    
    size_t total = 0;
    for (const auto& segment : sampleSegments_) {
        total += segment.getMemoryUsage();
    }
    
    return total;
}

/**
 * @brief Vrátí počet dostupných not
 */
int SampleLibrary::getAvailableNoteCount() const
{
    std::lock_guard<std::mutex> lock(accessMutex_);
    
    int count = 0;
    for (uint8_t note = MIN_NOTE; note <= MAX_NOTE; ++note) {
        // Počítáme notu jako dostupnou pokud má alespoň jeden dynamic level
        bool hasAnyLevel = false;
        for (uint8_t level = 0; level < NUM_DYNAMIC_LEVELS; ++level) {
            if (sampleSegments_[note].isLayerAvailable(level)) {
                hasAnyLevel = true;
                break;
            }
        }
        if (hasAnyLevel) {
            count++;
        }
    }
    
    return count;
}

/**
 * @brief Vrátí detailní informace o dostupných dynamic levels
 */
SampleLibrary::AvailabilityInfo SampleLibrary::getAvailabilityInfo() const
{
    std::lock_guard<std::mutex> lock(accessMutex_);
    
    AvailabilityInfo info;
    info.totalNotes = MAX_NOTE - MIN_NOTE + 1;
    
    for (uint8_t note = MIN_NOTE; note <= MAX_NOTE; ++note) {
        bool hasAnyLevel = false;
        
        for (uint8_t level = 0; level < NUM_DYNAMIC_LEVELS; ++level) {
            if (sampleSegments_[note].isLayerAvailable(level)) {
                hasAnyLevel = true;
                info.levelCounts[level]++;
                
                if (sampleSegments_[note].isLayerStereo(level)) {
                    info.stereoSamples++;
                } else {
                    info.monoSamples++;
                }
            }
        }
        
        if (hasAnyLevel) {
            info.notesWithAnyLevel++;
        }
    }
    
    return info;
}