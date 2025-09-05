#include "SampleLibrary.h"
#include <cmath>

/**
 * @brief SampleLibrary constructor
 */
SampleLibrary::SampleLibrary()
    : logger_(Logger::getInstance())
{
    logger_.log("SampleLibrary/constructor", "info", "SampleLibrary initialized with dynamic levels");
}

/**
 * @brief Initializes sample library with hybrid loading system
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
                "Starting initialization with sample rate=" + juce::String(sampleRate_) + 
                " using dynamic levels");

    // Reset statistics
    loadingStats_ = SampleLibraryStats();

    try {
        // Create SampleLoader
        SampleLoader loader(sampleRate);
        juce::File instrumentDir = SampleLoader::getDefaultInstrumentDirectory();
        
        // Ensure directory exists
        if (!instrumentDir.exists()) {
            if (!instrumentDir.createDirectory()) {
                throw std::runtime_error("Cannot create instrument directory: " + 
                                       instrumentDir.getFullPathName().toStdString());
            }
            logger_.log("SampleLibrary/initialize", "info", 
                       "Created instrument directory: " + instrumentDir.getFullPathName());
        }

        // Progress callback wrapper s redukovaným loggingem
        auto progressWrapper = [this, progressCallback](int current, int total, const juce::String& status) {
            if (progressCallback) {
                progressCallback(current, total, status);
            }
            // Log pouze každý 100. sample pro snížení noise
            if (current % 100 == 0 || current == total) {
                logger_.log("SampleLibrary/initialize", "debug", 
                           "Progress: " + juce::String(current) + "/" + juce::String(total) + 
                           " - " + status);
            }
        };

        // Load all samples
        std::vector<LoadedSample> loadedSamples = loader.loadInstrument(instrumentDir, progressWrapper);
        
        // Store samples in internal structure
        for (const auto& sample : loadedSamples) {
            try {
                storeSampleRam(sample);
                loadingStats_.totalSamples++;
                
                if (sample.isGenerated) {
                    loadingStats_.generatedSines++;
                } else {
                    loadingStats_.loadedFromFiles++;
                }
                
            } catch (const std::exception& e) {
                logger_.log("SampleLibrary/initialize", "error",
                           "Error storing sample for note " + juce::String((int)sample.midiNote) + 
                           " level " + juce::String((int)sample.dynamicLevel) + 
                           ": " + juce::String(e.what()));
            }
        }
        
        // Adopt statistics from SampleLoader
        const auto& loaderStats = loader.getLoadingStats();
        loadingStats_.savedToFiles = loaderStats.filesSaved;
        loadingStats_.totalMemoryUsed = getTotalMemoryUsage();
        loadingStats_.loadingTimeSeconds = (juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0;

        logger_.log("SampleLibrary/initialize", "info",
                   "Initialization completed: " + loadingStats_.getDescription());
        
        // Check if we have any samples
        if (loadingStats_.totalSamples == 0) {
            throw std::runtime_error("No samples were loaded!");
        }
        
        // Check availability of basic notes for debugging
        AvailabilityInfo availInfo = getAvailabilityInfo();
        logger_.log("SampleLibrary/initialize", "info",
                   "Available notes: " + juce::String(availInfo.notesWithAnyLevel) + "/" + 
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
                   "Fatal error during initialization: " + juce::String(e.what()));
        throw;
    } catch (...) {
        logger_.log("SampleLibrary/initialize", "error",
                   "Unknown fatal error during initialization");
        throw std::runtime_error("Unknown error during initialization");
    }
}

/**
 * @brief Clears all samples from memory
 */
void SampleLibrary::clear()
{
    std::lock_guard<std::mutex> lock(accessMutex_);
    
    logger_.log("SampleLibrary/clear", "info", "Starting SampleLibrary clear operation");
    
    size_t totalFreed = 0;
    int segmentsCleared = 0;
    
    for (auto& segment : sampleSegments_) {
        if (segment.midiNote != 0) { // Segment has data
            totalFreed += segment.getMemoryUsage();
            segment.reset();
            segmentsCleared++;
        }
    }

    // Reset statistics
    loadingStats_ = SampleLibraryStats();

    logger_.log("SampleLibrary/clear", "info", 
               "SampleLibrary cleared - freed " + juce::String(segmentsCleared) + 
               " segments, " + juce::String(totalFreed / (1024*1024)) + "MB");
}

/**
 * @brief Returns sample data for specific note and dynamic level
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
 * @brief Returns sample length for specific note and dynamic level
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
 * @brief Checks availability of specific dynamic level
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
 * @brief Checks if sample is stereo
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
 * @brief Stores loaded sample into RAM-based internal structure s redukovaným loggingem
 */
void SampleLibrary::storeSampleRam(const LoadedSample& sample)
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
    
    // Copy data (necessary due to unique_ptr)
    size_t totalSamples = static_cast<size_t>(sample.lengthSamples) * sample.numChannels;
    auto dataCopy = std::make_unique<float[]>(totalSamples);
    std::copy(sample.audioData.get(), 
              sample.audioData.get() + totalSamples, 
              dataCopy.get());
    
    bool isStereo = (sample.numChannels == 2);
    segment.storeLayer(sample.dynamicLevel, std::move(dataCopy), sample.lengthSamples, isStereo);
    
    // OPRAVA: Drasticky redukované logging - pouze každých 200 samples
    static int storeCounter = 0;
    if (++storeCounter % 200 == 0) {
        logger_.log("SampleLibrary/storeSampleRam", "debug",
                   "Batch stored " + juce::String(storeCounter) + " samples in RAM " +
                   "(latest: note " + juce::String((int)sample.midiNote) + 
                   " level " + juce::String((int)sample.dynamicLevel) + 
                   ", " + juce::String(sample.lengthSamples) + " samples, " +
                   juce::String(isStereo ? "stereo" : "mono") + ")");
    }
}

/**
 * @brief Maps MIDI velocity to dynamic level
 */
uint8_t SampleLibrary::velocityToDynamicLevel(uint8_t velocity)
{
    return SampleLoader::velocityToDynamicLevel(velocity);
}

/**
 * @brief Returns total memory usage in bytes
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
 * @brief Returns count of available notes
 */
int SampleLibrary::getAvailableNoteCount() const
{
    std::lock_guard<std::mutex> lock(accessMutex_);
    
    int count = 0;
    for (uint8_t note = MIN_NOTE; note <= MAX_NOTE; ++note) {
        // Count note as available if it has at least one dynamic level
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
 * @brief Returns detailed information about available dynamic levels
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