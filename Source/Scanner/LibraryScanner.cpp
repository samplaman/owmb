#include "LibraryScanner.h"

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
    stopThread(4000);
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

void LibraryScanner::run()
{
    listeners.call([](ScannerListener& l) { l.scanStarted(); });

    int totalProcessed = 0;
    std::vector<MediaItem> batch;
    batch.reserve(50);

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

            MediaItem item = processAudioFile(file);

            if (item.filePath.isNotEmpty())
            {
                batch.push_back(item);
                totalProcessed++;

                const auto curPath = item.fileName;
                listeners.call([totalProcessed, curPath](ScannerListener& l) {
                    l.scanProgress(totalProcessed, curPath);
                });

                if (batch.size() >= 25)
                {
                    db.addItems(batch);
                    batch.clear();
                }
            }
        }
    }

    if (!batch.empty() && !threadShouldExit())
    {
        db.addItems(batch);
        batch.clear();
    }

    listeners.call([totalProcessed](ScannerListener& l) {
        l.scanFinished(totalProcessed);
    });
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

    int64_t dataChunkPos = 20 + fmtChunkSize;
    uint32_t dataSize = 0;

    while (dataChunkPos < stream.getTotalLength() - 8)
    {
        stream.setPosition(dataChunkPos);
        char chunkId[4];
        uint32_t chunkSize = 0;
        if (stream.read(chunkId, 4) < 4 || stream.read(&chunkSize, 4) < 4)
            break;

        if (std::memcmp(chunkId, "data", 4) == 0)
        {
            dataSize = chunkSize;
            break;
        }
        dataChunkPos += 8 + chunkSize;
    }

    if (dataSize == 0)
        return false;

    int bytesPerSample = (bits / 8) * channels;
    if (bytesPerSample == 0)
        return false;

    uint64_t totalSamples = dataSize / bytesPerSample;

    sampleRate = static_cast<double>(rate);
    numChannels = static_cast<int>(channels);
    bitDepth = static_cast<int>(bits);
    durationSeconds = static_cast<double>(totalSamples) / sampleRate;

    return true;
}

MediaItem LibraryScanner::processAudioFile(const juce::File& file)
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

    // Auto-infer tags from path and file type
    item.tags = TagDatabaseManager::inferTagsFromPath(absPath);

    // Try ultra-fast binary header parsing first for WAV files
    if (item.fileExtension == ".wav" && parseWavHeaderFast(file, item.sampleRate, item.numChannels, item.bitDepth, item.durationSeconds))
    {
        return item;
    }

    // Read audio header metadata using JUCE format reader as fallback
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader != nullptr)
    {
        item.sampleRate = reader->sampleRate;
        item.numChannels = static_cast<int>(reader->numChannels);
        item.bitDepth = static_cast<int>(reader->bitsPerSample);
        if (reader->sampleRate > 0.0)
        {
            item.durationSeconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
        }
    }

    return item;
}

} // namespace openwav
