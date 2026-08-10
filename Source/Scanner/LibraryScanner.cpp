#include "LibraryScanner.h"
#include <future>
#include <thread>
#include <vector>
#include <algorithm>

namespace openwav
{

LibraryScanner::LibraryScanner(TagDatabaseManager& dbManager)
    : juce::Thread("OpenWavLibraryScannerThread"),
      db(dbManager)
{
    formatManager.registerBasicFormats();
}

LibraryScanner::~LibraryScanner()
{
    cancelScan();
    stopThread(100);
}

void LibraryScanner::addListener(ScannerListener* listener)
{
    listeners.add(listener);
}

void LibraryScanner::removeListener(ScannerListener* listener)
{
    listeners.remove(listener);
}

void LibraryScanner::startScan(const std::vector<juce::String>& folderPaths)
{
    if (isThreadRunning())
    {
        cancelScan();
        stopThread(3000);
    }

    targetFolders = folderPaths;
    cancelRequested = false;
    startThread(juce::Thread::Priority::low);
}

void LibraryScanner::cancelScan()
{
    cancelRequested = true;
    signalThreadShouldExit();
}

void LibraryScanner::notifyScanStarted()
{
    const juce::ScopedLock sl(listenerLock);
    listeners.call([](ScannerListener& l) { l.scanStarted(); });
}

void LibraryScanner::notifyScanProgress(int filesProcessed, int totalFiles, const juce::String& currentFile)
{
    const juce::ScopedLock sl(listenerLock);
    listeners.call([filesProcessed, totalFiles, currentFile](ScannerListener& l) {
        l.scanProgress(filesProcessed, totalFiles, currentFile);
    });
}

void LibraryScanner::notifyScanFinished(int totalFilesDiscovered)
{
    const juce::ScopedLock sl(listenerLock);
    listeners.call([totalFilesDiscovered](ScannerListener& l) {
        l.scanFinished(totalFilesDiscovered);
    });
}

void LibraryScanner::run()
{
    notifyScanStarted();

    std::vector<juce::File> newFilesToScan;

    for (const auto& folderPath : targetFolders)
    {
        if (threadShouldExit() || cancelRequested)
            break;

        juce::File rootDir(folderPath);
        if (!rootDir.exists() || !rootDir.isDirectory())
            continue;

        juce::DirectoryIterator iter(rootDir, true, "*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff", juce::File::findFiles);

        while (iter.next())
        {
            if (threadShouldExit() || cancelRequested)
                break;

            const auto file = iter.getFile();

            if (file.getFileName().startsWith(".") || file.getFileName().startsWithIgnoreCase("._") || file.isHidden())
                continue;

            auto absPath = file.getFullPathName();
            juce::String id = juce::String::toHexString(absPath.hashCode64());

            MediaItem existingItem;
            if (db.getItemById(id, existingItem))
            {
                if (existingItem.fileSizeBytes == file.getSize() &&
                    existingItem.dateAddedMs == file.getLastModificationTime().toMilliseconds())
                {
                    // File already scanned and unmodified -> skip!
                    continue;
                }
            }

            newFilesToScan.push_back(file);
        }
    }

    std::atomic<int> processedCount { 0 };

    if (!newFilesToScan.empty() && !threadShouldExit() && !cancelRequested)
    {
        unsigned int hardwareCores = std::thread::hardware_concurrency();
        // Dynamically scale worker threads to utilize available hardware CPU cores (up to 16 threads)
        unsigned int numThreads = std::max(2u, std::min(16u, (hardwareCores > 1) ? (hardwareCores - 1) : 2u));
        size_t total = newFilesToScan.size();
        size_t chunkSize = (total + numThreads - 1) / numThreads;

        notifyScanProgress(0, static_cast<int>(total), "Starting high-speed analysis of new files...");

        std::vector<std::future<void>> futures;

        for (unsigned int t = 0; t < numThreads; ++t)
        {
            size_t startIdx = t * chunkSize;
            size_t endIdx = std::min(total, startIdx + chunkSize);

            if (startIdx >= endIdx) break;

            futures.push_back(std::async(std::launch::async, [this, &newFilesToScan, startIdx, endIdx, total, &processedCount]() {
                juce::AudioFormatManager localFormatManager;
                localFormatManager.registerBasicFormats();

                std::vector<MediaItem> localItems;
                localItems.reserve(64);

                for (size_t i = startIdx; i < endIdx; ++i)
                {
                    if (threadShouldExit() || cancelRequested) break;
                    auto item = processAudioFile(newFilesToScan[i], localFormatManager);
                    if (item.filePath.isNotEmpty())
                    {
                        localItems.push_back(item);
                    }

                    int count = ++processedCount;
                    juce::String fileName = newFilesToScan[i].getFileName();

                    if (localItems.size() >= 64)
                    {
                        // Add items to in-memory map without triggering heavy disk serialization or UI rebuilds
                        db.addItems(localItems, false, false);
                        localItems.clear();
                    }

                    notifyScanProgress(count, static_cast<int>(total), fileName);

                    std::this_thread::yield();
                }

                if (!localItems.empty() && !cancelRequested)
                {
                    db.addItems(localItems, false, false);
                }
            }));
        }

        for (auto& fut : futures)
        {
            fut.get();
        }

        // Notify UI and save entire updated database to JSON file once at end of scan
        if (!cancelRequested)
        {
            db.notifyIndexUpdated();
            db.notifyTagsUpdated();
            db.saveToFile();
        }
    }

    notifyScanFinished(processedCount.load());
}

static bool parseWavHeaderFast(const juce::File& file, double& sampleRate, int& numChannels, int& bitDepth, double& durationSeconds)
{
    juce::FileInputStream stream(file);
    if (!stream.openedOk() || stream.getTotalLength() < 44)
        return false;

    char header[44];
    if (stream.read(header, 44) < 44)
        return false;

    if (std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0)
        return false;

    if (std::memcmp(header + 12, "fmt ", 4) != 0)
        return false;

    uint16_t channels = 0;
    uint32_t rate = 0;
    uint16_t bits = 0;

    std::memcpy(&channels, header + 22, 2);
    std::memcpy(&rate, header + 24, 4);
    std::memcpy(&bits, header + 34, 2);

    if (rate == 0 || channels == 0 || bits == 0)
        return false;

    uint32_t fmtChunkSize = 0;
    std::memcpy(&fmtChunkSize, header + 16, 4);
    if (fmtChunkSize < 16 || fmtChunkSize > 10000)
        return false;

    int64_t dataChunkPos = 20 + static_cast<int64_t>(fmtChunkSize);
    uint32_t dataSize = 0;

    int iterations = 0;
    int64_t streamLength = stream.getTotalLength();

    while (dataChunkPos >= 0 && dataChunkPos < streamLength - 8 && iterations++ < 50)
    {
        if (!stream.setPosition(dataChunkPos))
            break;

        char chunkId[4];
        uint32_t chunkSize = 0;
        if (stream.read(chunkId, 4) < 4 || stream.read(&chunkSize, 4) < 4)
            break;

        if (std::memcmp(chunkId, "data", 4) == 0)
        {
            dataSize = chunkSize;
            break;
        }

        int64_t nextPos = dataChunkPos + 8 + static_cast<int64_t>(chunkSize);
        if (nextPos <= dataChunkPos || nextPos >= streamLength)
            break;

        dataChunkPos = nextPos;
    }

    if (dataSize == 0)
        return false;

    int bytesPerSample = (bits / 8) * channels;
    if (bytesPerSample <= 0)
        return false;

    uint64_t totalSamples = dataSize / bytesPerSample;

    sampleRate = static_cast<double>(rate);
    numChannels = static_cast<int>(channels);
    bitDepth = static_cast<int>(bits);
    durationSeconds = static_cast<double>(totalSamples) / sampleRate;

    if (durationSeconds <= 0.0 || durationSeconds > 86400.0)
        return false;

    return true;
}

static void analyzeAudioData(juce::AudioFormatReader* reader, MediaItem& item)
{
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0 || reader->numChannels <= 0)
        return;

    juce::int64 lengthInSamples = reader->lengthInSamples;
    if (item.fileExtension == ".mp3" && reader->sampleRate < 32000.0)
    {
        lengthInSamples /= 2;
    }

    // Read up to 1.5 seconds of mono audio (channel 0) for high-speed analysis
    int maxSamplesToAnalyze = static_cast<int>(std::min(lengthInSamples, static_cast<juce::int64>(reader->sampleRate * 1.5)));
    if (maxSamplesToAnalyze <= 128)
        return;

    juce::AudioBuffer<float> buffer(1, maxSamplesToAnalyze);
    reader->read(&buffer, 0, maxSamplesToAnalyze, 0, true, false);
    const float* samples = buffer.getReadPointer(0);

    // 1. Calculate Peak, RMS, and Decay
    float maxVal = 0.0f;
    double sumSq = 0.0;
    for (int i = 0; i < maxSamplesToAnalyze; ++i)
    {
        float val = std::abs(samples[i]);
        if (val > maxVal) maxVal = val;
        sumSq += val * val;
    }

    if (maxVal < 0.001f) // Silent or near-silent
    {
        item.tags.insert("#Silent");
        return;
    }

    float rms = static_cast<float>(std::sqrt(sumSq / maxSamplesToAnalyze));

    // Calculate RMS in 4 sequential blocks to estimate decay
    int blockSize = maxSamplesToAnalyze / 4;
    double blockRMS[4] = {0.0, 0.0, 0.0, 0.0};
    for (int b = 0; b < 4; ++b)
    {
        double bSumSq = 0.0;
        int start = b * blockSize;
        int end = std::min(maxSamplesToAnalyze, (b + 1) * blockSize);
        int count = end - start;
        if (count > 0)
        {
            for (int i = start; i < end; ++i)
            {
                float val = samples[i];
                bSumSq += val * val;
            }
            blockRMS[b] = std::sqrt(bSumSq / count);
        }
    }

    double decayRatio = (blockRMS[0] > 0.001) ? (blockRMS[3] / blockRMS[0]) : 0.0;

    // Determine One-Shot vs Loop
    bool isLoop = false;
    double duration = item.durationSeconds;
    if (duration >= 1.5)
    {
        if (decayRatio >= 0.28)
        {
            isLoop = true;
            item.tags.insert("#Loop");
        }
        else
        {
            item.tags.insert("#OneShot");
        }
    }
    else
    {
        item.tags.insert("#OneShot");
    }

    // 2. Calculate Zero-Crossing Rate (ZCR) and High-Frequency Energy Ratio
    int zeroCrossings = 0;
    double energyX = 0.0;
    double energyD = 0.0;
    for (int i = 1; i < maxSamplesToAnalyze; ++i)
    {
        float prev = samples[i - 1];
        float curr = samples[i];
        if ((prev < 0.0f && curr >= 0.0f) || (prev > 0.0f && curr <= 0.0f))
            zeroCrossings++;

        energyX += curr * curr;
        float diff = curr - prev;
        energyD += diff * diff;
    }

    double zcr = static_cast<double>(zeroCrossings) / maxSamplesToAnalyze;
    double highFreqRatio = energyD / (energyX + 1e-9);
    float crestFactor = maxVal / (rms + 1e-6f);

    // Save DSP similarity features
    item.zcr = zcr;
    item.highFreqRatio = highFreqRatio;
    item.decayRatio = decayRatio;
    item.crestFactor = static_cast<double>(crestFactor);

    // 3. Audio Classification (Shallow Decision Tree / Rule-based)
    if (!isLoop)
    {
        // One-Shot classifications
        if (highFreqRatio < 0.22 && zcr < 0.06)
        {
            item.tags.insert("#Kick");
        }
        else if (highFreqRatio >= 1.1 || zcr >= 0.22)
        {
            item.tags.insert("#HiHat");
        }
        else if (highFreqRatio >= 0.55 && highFreqRatio < 1.1 && zcr >= 0.10)
        {
            if (decayRatio > 0.12)
                item.tags.insert("#Snare");
            else
                item.tags.insert("#Clap");
        }
        else
        {
            item.tags.insert("#Percussion");
        }
    }
    else
    {
        // Loop classifications
        if (highFreqRatio < 0.12 && zcr < 0.04)
        {
            item.tags.insert("#Bass");
        }
        else if (crestFactor > 3.6f && highFreqRatio > 0.35)
        {
            item.tags.insert("#DrumLoop");
        }
        else
        {
            item.tags.insert("#Synth");
        }

        // 4. BPM Estimation via Autocorrelation of Envelope Onsets
        const int envBlockSize = 256;
        const int numEnvBlocks = maxSamplesToAnalyze / envBlockSize;
        if (numEnvBlocks > 16)
        {
            std::vector<float> env(numEnvBlocks, 0.0f);
            for (int b = 0; b < numEnvBlocks; ++b)
            {
                float bMax = 0.0f;
                int start = b * envBlockSize;
                int end = std::min(maxSamplesToAnalyze, (b + 1) * envBlockSize);
                for (int i = start; i < end; ++i)
                {
                    float val = std::abs(samples[i]);
                    if (val > bMax) bMax = val;
                }
                env[b] = bMax;
            }

            // Onsets = first-difference of envelope
            std::vector<float> onsets(numEnvBlocks, 0.0f);
            for (int i = 1; i < numEnvBlocks; ++i)
            {
                onsets[i] = std::max(0.0f, env[i] - env[i - 1]);
            }

            double fsEnv = reader->sampleRate / envBlockSize;
            int minLag = static_cast<int>(60.0 * fsEnv / 180.0);
            int maxLag = static_cast<int>(60.0 * fsEnv / 60.0);

            double maxR = -1.0;
            int bestLag = 0;

            for (int lag = minLag; lag <= maxLag; ++lag)
            {
                double sum = 0.0;
                int count = 0;
                for (int i = lag; i < numEnvBlocks; ++i)
                {
                    sum += onsets[i] * onsets[i - lag];
                    count++;
                }
                if (count > 0)
                {
                    double r = sum / count;
                    if (r > maxR)
                    {
                        maxR = r;
                        bestLag = lag;
                    }
                }
            }

            if (bestLag > 0)
            {
                double estimatedBpm = 60.0 * fsEnv / bestLag;
                estimatedBpm = std::round(estimatedBpm * 10.0) / 10.0;
                if (estimatedBpm >= 50.0 && estimatedBpm <= 200.0)
                {
                    item.bpm = estimatedBpm;
                    item.tags.insert("#" + juce::String(juce::roundToInt(estimatedBpm)) + "BPM");
                }
            }
        }
    }

    // 5. Acoustic DSP Character Tagging
    if (crestFactor > 4.2f)
        item.tags.insert("#Punchy");
    if (highFreqRatio >= 0.70)
        item.tags.insert("#Bright");
    else if (highFreqRatio < 0.15 && zcr < 0.05)
        item.tags.insert("#Warm");
}

MediaItem LibraryScanner::processAudioFile(const juce::File& file, juce::AudioFormatManager& localFormatManager)
{
    MediaItem item;
    if (!file.existsAsFile() || file.getFileName().startsWith(".") || file.getFileName().startsWithIgnoreCase("._") || file.isHidden())
        return item;

    auto absPath = file.getFullPathName();
    item.filePath = absPath;
    item.fileName = file.getFileName();
    item.fileExtension = file.getFileExtension().toLowerCase();
    item.fileSizeBytes = file.getSize();
    item.dateAddedMs = file.getLastModificationTime().toMilliseconds();

    // Generate stable unique ID based on path string hash
    item.id = juce::String::toHexString(absPath.hashCode64());

    // Check if the item already exists in the database and is unmodified
    MediaItem existingItem;
    if (db.getItemById(item.id, existingItem))
    {
        if (existingItem.fileSizeBytes == item.fileSizeBytes &&
            existingItem.dateAddedMs == item.dateAddedMs)
        {
            return existingItem;
        }
    }

    bool headerParsed = false;
    // Try ultra-fast binary header parsing first for WAV files
    if (item.fileExtension == ".wav" && parseWavHeaderFast(file, item.sampleRate, item.numChannels, item.bitDepth, item.durationSeconds))
    {
        headerParsed = true;
    }

    // Read audio header metadata using JUCE format reader as fallback
    std::unique_ptr<juce::AudioFormatReader> reader(localFormatManager.createReaderFor(file));
    if (reader != nullptr)
    {
        if (!headerParsed)
        {
            item.sampleRate = reader->sampleRate;
            item.numChannels = static_cast<int>(reader->numChannels);
            item.bitDepth = static_cast<int>(reader->bitsPerSample);
            if (reader->sampleRate > 0.0)
            {
                juce::int64 len = reader->lengthInSamples;
                if (item.fileExtension == ".mp3" && reader->sampleRate < 32000.0)
                {
                    len /= 2;
                }
                item.durationSeconds = static_cast<double>(len) / reader->sampleRate;
            }
        }

        // Run local DSP-based shallow ML classifier and BPM estimation
        analyzeAudioData(reader.get(), item);
    }

    // Auto-infer tags from path, file type, channels, and duration
    auto pathTags = TagDatabaseManager::inferTagsFromPath(absPath, item.durationSeconds, item.numChannels);
    for (const auto& t : pathTags)
    {
        item.tags.insert(t);
    }

    return item;
}

} // namespace openwav
