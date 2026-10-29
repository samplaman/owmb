#include "LibraryScanner.h"
#include <future>
#include <thread>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>

#if JUCE_MAC
#include <unistd.h>
#endif

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
        stopThread(100);
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

        juce::DirectoryIterator iter(rootDir, true, "*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff;*.aifc;*.WAV;*.MP3;*.FLAC;*.OGG;*.AIF;*.AIFF;*.AIFC", juce::File::findFiles);

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
    }

    // Notify UI and save updated database once at end of scan
    if (!cancelRequested)
    {
        db.notifyIndexUpdated();
        db.notifyTagsUpdated();
        db.saveToFile();
    }

    notifyScanFinished(processedCount.load());
}

static bool parseWavHeaderFast(const juce::File& file, double& sampleRate, int& numChannels, int& bitDepth, double& durationSeconds)
{
    juce::FileInputStream stream(file);
    const int64_t streamLength = stream.getTotalLength();
    if (!stream.openedOk() || streamLength < 44)
        return false;

    char riffHeader[12];
    if (stream.read(riffHeader, 12) < 12)
        return false;

    if (std::memcmp(riffHeader, "RIFF", 4) != 0 || std::memcmp(riffHeader + 8, "WAVE", 4) != 0)
        return false;

    uint16_t channels = 0;
    uint32_t rate = 0;
    uint16_t bits = 0;
    uint32_t dataSize = 0;
    bool foundFmt = false;
    bool foundData = false;

    int64_t chunkPos = 12;
    int iterations = 0;

    while (chunkPos >= 12 && chunkPos <= streamLength - 8 && iterations++ < 64)
    {
        if (!stream.setPosition(chunkPos))
            break;

        char chunkId[4];
        uint8_t sizeBytes[4];
        if (stream.read(chunkId, 4) < 4 || stream.read(sizeBytes, 4) < 4)
            break;

        uint32_t chunkSize = static_cast<uint32_t>(sizeBytes[0]) |
                             (static_cast<uint32_t>(sizeBytes[1]) << 8) |
                             (static_cast<uint32_t>(sizeBytes[2]) << 16) |
                             (static_cast<uint32_t>(sizeBytes[3]) << 24);

        if (std::memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16)
        {
            uint8_t fmtBuf[16];
            if (stream.read(fmtBuf, 16) < 16)
                break;

            channels = static_cast<uint16_t>(fmtBuf[2]) | (static_cast<uint16_t>(fmtBuf[3]) << 8);
            rate = static_cast<uint32_t>(fmtBuf[4]) |
                   (static_cast<uint32_t>(fmtBuf[5]) << 8) |
                   (static_cast<uint32_t>(fmtBuf[6]) << 16) |
                   (static_cast<uint32_t>(fmtBuf[7]) << 24);
            bits = static_cast<uint16_t>(fmtBuf[14]) | (static_cast<uint16_t>(fmtBuf[15]) << 8);

            if (channels > 0 && rate >= 4000 && rate <= 384000 && bits >= 8 && bits <= 64)
                foundFmt = true;
        }
        else if (std::memcmp(chunkId, "data", 4) == 0 && chunkSize > 0)
        {
            dataSize = chunkSize;
            foundData = true;
        }

        if (foundFmt && foundData)
            break;

        int64_t paddedSize = static_cast<int64_t>(chunkSize) + (chunkSize & 1);
        int64_t nextPos = chunkPos + 8 + paddedSize;
        if (nextPos <= chunkPos || nextPos > streamLength)
            break;

        chunkPos = nextPos;
    }

    if (!foundFmt || !foundData || dataSize == 0)
        return false;

    int bytesPerSample = ((bits + 7) / 8) * channels;
    if (bytesPerSample <= 0)
        return false;

    uint64_t totalSamples = dataSize / static_cast<uint64_t>(bytesPerSample);

    sampleRate = static_cast<double>(rate);
    numChannels = static_cast<int>(channels);
    bitDepth = static_cast<int>(bits);
    durationSeconds = static_cast<double>(totalSamples) / sampleRate;

    if (durationSeconds <= 0.0 || durationSeconds > 86400.0)
        return false;

    return true;
}

static double decodeIEEEExtended(const unsigned char* bytes)
{
    uint16_t expon = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
    uint64_t hiMant = (static_cast<uint64_t>(bytes[2]) << 24) |
                      (static_cast<uint64_t>(bytes[3]) << 16) |
                      (static_cast<uint64_t>(bytes[4]) << 8)  |
                      bytes[5];
    uint64_t loMant = (static_cast<uint64_t>(bytes[6]) << 24) |
                      (static_cast<uint64_t>(bytes[7]) << 16) |
                      (static_cast<uint64_t>(bytes[8]) << 8)  |
                      bytes[9];
    uint64_t mantissa = (hiMant << 32) | loMant;

    bool sign = (expon & 0x8000) != 0;
    expon &= 0x7FFF;

    if (expon == 0 && mantissa == 0)
        return 0.0;

    if (expon == 0x7FFF)
        return 0.0;

    double val = std::ldexp(static_cast<double>(mantissa), static_cast<int>(expon) - 16383 - 63);
    return sign ? -val : val;
}

static bool parseAiffHeaderFast(const juce::File& file, double& sampleRate, int& numChannels, int& bitDepth, double& durationSeconds)
{
    juce::FileInputStream stream(file);
    if (!stream.openedOk() || stream.getTotalLength() < 44)
        return false;

    char formHeader[12];
    if (stream.read(formHeader, 12) < 12)
        return false;

    if (std::memcmp(formHeader, "FORM", 4) != 0)
        return false;

    if (std::memcmp(formHeader + 8, "AIFF", 4) != 0 && std::memcmp(formHeader + 8, "AIFC", 4) != 0)
        return false;

    int64_t streamLength = stream.getTotalLength();
    int64_t chunkPos = 12;
    int iterations = 0;
    bool foundComm = false;

    while (chunkPos >= 12 && chunkPos < streamLength - 8 && iterations++ < 60)
    {
        if (!stream.setPosition(chunkPos))
            break;

        char chunkId[4];
        if (stream.read(chunkId, 4) < 4)
            break;

        uint8_t sizeBytes[4];
        if (stream.read(sizeBytes, 4) < 4)
            break;

        uint32_t chunkSize = (static_cast<uint32_t>(sizeBytes[0]) << 24) |
                             (static_cast<uint32_t>(sizeBytes[1]) << 16) |
                             (static_cast<uint32_t>(sizeBytes[2]) << 8)  |
                             static_cast<uint32_t>(sizeBytes[3]);

        if (std::memcmp(chunkId, "COMM", 4) == 0 && chunkSize >= 18)
        {
            uint8_t commData[18];
            if (stream.read(commData, 18) < 18)
                break;

            uint16_t ch = (static_cast<uint16_t>(commData[0]) << 8) | commData[1];
            uint32_t frames = (static_cast<uint32_t>(commData[2]) << 24) |
                              (static_cast<uint32_t>(commData[3]) << 16) |
                              (static_cast<uint32_t>(commData[4]) << 8)  |
                              static_cast<uint32_t>(commData[5]);
            uint16_t bits = (static_cast<uint16_t>(commData[6]) << 8) | commData[7];
            double rate = decodeIEEEExtended(commData + 8);

            if (ch > 0 && bits > 0 && rate > 100.0 && rate <= 384000.0 && frames > 0)
            {
                numChannels = static_cast<int>(ch);
                bitDepth = static_cast<int>(bits);
                sampleRate = rate;
                durationSeconds = static_cast<double>(frames) / rate;
                foundComm = true;
                break;
            }
        }

        int64_t paddedSize = static_cast<int64_t>(chunkSize) + (chunkSize & 1);
        int64_t nextPos = chunkPos + 8 + paddedSize;
        if (nextPos <= chunkPos || nextPos > streamLength)
            break;

        chunkPos = nextPos;
    }

    return foundComm && durationSeconds > 0.0 && durationSeconds <= 86400.0;
}

static void computeQuarterRms(const float* data, int len, double outRms[4])
{
    outRms[0] = outRms[1] = outRms[2] = outRms[3] = 0.0;
    if (len < 4)
        return;

    int blockSize = len / 4;
    for (int b = 0; b < 4; ++b)
    {
        int start = b * blockSize;
        int end = (b == 3) ? len : std::min(len, (b + 1) * blockSize);
        int count = end - start;
        if (count > 0)
        {
            double sumSq = 0.0;
            for (int i = start; i < end; ++i)
            {
                double v = data[i];
                sumSq += v * v;
            }
            outRms[b] = std::sqrt(sumSq / count);
        }
    }
}

static void analyzeAudioData(juce::AudioFormatReader* reader, MediaItem& item)
{
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0 || reader->numChannels <= 0)
        return;

    const double sampleRate = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
    juce::int64 lengthInSamples = reader->lengthInSamples;
    if (item.fileExtension == ".mp3" && sampleRate < 32000.0)
    {
        lengthInSamples /= 2;
    }

    // Read up to 4.5 seconds (gives full multi-beat window down to 60 BPM for loops, zero overhead on short one-shots)
    const int maxSamplesToRead = static_cast<int>(std::min(lengthInSamples, static_cast<juce::int64>(sampleRate * 4.5)));
    if (maxSamplesToRead <= 128)
        return;

    const int numReadCh = std::min<int>(2, static_cast<int>(reader->numChannels));
    juce::AudioBuffer<float> buffer(numReadCh, maxSamplesToRead);
    reader->read(&buffer, 0, maxSamplesToRead, 0, true, numReadCh >= 2);

    // Phase-safe Stereo-to-Mono downmix (captures hard-panned right-channel sounds and phase-inverted wide FX)
    std::vector<float> mono(static_cast<size_t>(maxSamplesToRead), 0.0f);
    if (numReadCh == 1)
    {
        std::memcpy(mono.data(), buffer.getReadPointer(0), static_cast<size_t>(maxSamplesToRead) * sizeof(float));
    }
    else
    {
        const float* chL = buffer.getReadPointer(0);
        const float* chR = buffer.getReadPointer(1);
        double sumAbsL = 0.0, sumAbsR = 0.0, sumAbsMid = 0.0;
        for (int i = 0; i < maxSamplesToRead; ++i)
        {
            float l = chL[i];
            float r = chR[i];
            sumAbsL += std::abs(l);
            sumAbsR += std::abs(r);
            sumAbsMid += std::abs(0.5f * (l + r));
        }

        const bool phaseCancelled = (sumAbsMid < 0.20 * std::max(sumAbsL, sumAbsR));
        for (int i = 0; i < maxSamplesToRead; ++i)
        {
            mono[static_cast<size_t>(i)] = phaseCancelled ? (0.5f * (chL[i] - chR[i]))
                                                          : (0.5f * (chL[i] + chR[i]));
        }
    }

    // 1. Peak scan across buffer
    float maxVal = 0.0f;
    for (int i = 0; i < maxSamplesToRead; ++i)
    {
        float v = std::abs(mono[static_cast<size_t>(i)]);
        if (v > maxVal)
            maxVal = v;
    }

    if (maxVal < 0.001f)
    {
        item.tags.insert("#Silent");
        return;
    }

    // 2. Trim leading onset silence and trailing zero-padding for accurate attack, RMS, decay, and spectral features
    const float onsetThresh = maxVal * 0.02f; // -34 dB relative to peak
    const float tailThresh  = maxVal * 0.005f; // -46 dB relative to peak

    int firstActive = 0;
    while (firstActive < maxSamplesToRead && std::abs(mono[static_cast<size_t>(firstActive)]) < onsetThresh)
        ++firstActive;

    int lastActive = maxSamplesToRead - 1;
    while (lastActive > firstActive && std::abs(mono[static_cast<size_t>(lastActive)]) < tailThresh)
        --lastActive;

    int activeStart = std::max(0, firstActive - static_cast<int>(sampleRate * 0.001));
    int activeEnd   = std::min(maxSamplesToRead, lastActive + static_cast<int>(sampleRate * 0.005) + 1);
    if (activeEnd - activeStart < 128)
    {
        activeStart = 0;
        activeEnd = maxSamplesToRead;
    }

    const int activeLen = activeEnd - activeStart;
    const double activeDuration = static_cast<double>(activeLen) / sampleRate;
    const float* activeSamples = mono.data() + activeStart;

    int localPeakIndex = 0;
    float localPeakVal = 0.0f;
    for (int i = 0; i < activeLen; ++i)
    {
        float v = std::abs(activeSamples[i]);
        if (v > localPeakVal)
        {
            localPeakVal = v;
            localPeakIndex = i;
        }
    }

    const double attackTimeMs = std::max(0.0, (static_cast<double>(localPeakIndex) / sampleRate) * 1000.0);

    // 3. Envelope sustainability & One-Shot vs Loop detection
    double fullBlockRMS[4];
    double activeBlockRMS[4];
    computeQuarterRms(mono.data(), maxSamplesToRead, fullBlockRMS);
    computeQuarterRms(activeSamples, activeLen, activeBlockRMS);

    double maxFullQ = std::max({ fullBlockRMS[0], fullBlockRMS[1], fullBlockRMS[2], fullBlockRMS[3], 1e-6 });
    int sustainedQuarters = 0;
    for (int q = 0; q < 4; ++q)
    {
        if (fullBlockRMS[q] >= 0.22 * maxFullQ)
            ++sustainedQuarters;
    }
    const bool isNonMonotonic = (fullBlockRMS[2] > fullBlockRMS[1] * 1.08) ||
                                (fullBlockRMS[3] > fullBlockRMS[2] * 1.08);

    double denomFull = std::max({ fullBlockRMS[0], fullBlockRMS[1] * 0.5, 0.005 });
    double denomAct  = std::max({ activeBlockRMS[0], activeBlockRMS[1] * 0.5, 0.005 });
    double fullDecayRatio   = juce::jlimit(0.0, 1.25, fullBlockRMS[3] / denomFull);
    double activeDecayRatio = juce::jlimit(0.0, 1.25, activeBlockRMS[3] / denomAct);

    const double duration = item.durationSeconds > 0.0 ? item.durationSeconds
                                                       : (static_cast<double>(lengthInSamples) / sampleRate);

    bool hasExplicitLoopTag = (item.tags.find("#Loop") != item.tags.end() ||
                               item.tags.find("#DrumLoop") != item.tags.end() ||
                               item.tags.find("#TopLoop") != item.tags.end() ||
                               item.tags.find("#PercLoop") != item.tags.end() ||
                               item.tags.find("#BassLoop") != item.tags.end() ||
                               item.tags.find("#MelodicLoop") != item.tags.end() ||
                               item.tags.find("#VocalLoop") != item.tags.end());
    bool hasExplicitOneShotTag = (item.tags.find("#OneShot") != item.tags.end());

    bool isLoop = false;
    if (hasExplicitLoopTag && duration >= 0.65)
    {
        isLoop = true;
    }
    else if (!hasExplicitOneShotTag && duration >= 1.35 &&
             (fullDecayRatio >= 0.26 || (sustainedQuarters >= 3 && isNonMonotonic)))
    {
        isLoop = true;
    }

    if (isLoop)
    {
        item.tags.insert("#Loop");
        item.tags.erase("#OneShot");
    }
    else
    {
        item.tags.insert("#OneShot");
        if (!hasExplicitLoopTag)
            item.tags.erase("#Loop");
    }

    const double decayRatio = isLoop ? fullDecayRatio : activeDecayRatio;

    // 4. Sample-Rate-Invariant Spectral Features (Hz Crossovers + 44.1 kHz Reference ZCR & HF Ratio)
    const int specLen = std::min(activeLen, std::max(128, static_cast<int>(sampleRate * 1.5)));
    double sumSqActive = 0.0;

    auto calcAlpha = [sampleRate](double fc) -> double {
        double clampedFc = juce::jlimit(20.0, sampleRate * 0.45, fc);
        return 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * clampedFc / sampleRate);
    };

    const double aSub  = calcAlpha(140.0);  // Sub / Kick / Bass fundamental (< 140 Hz)
    const double aLow  = calcAlpha(800.0);  // Low-Mid body / Tom / Snare fundamental (140 - 800 Hz)
    const double aMid  = calcAlpha(4000.0); // Mid presence / Clap / Vocal / Synth (800 - 4000 Hz)
    const double a20k  = calcAlpha(19500.0);

    double s1Sub = 0.0, s2Sub = 0.0;
    double s1Low = 0.0, s2Low = 0.0;
    double s1Mid = 0.0, s2Mid = 0.0;

    double eSub = 0.0, eLowMid = 0.0, eMid = 0.0, eHigh = 0.0;
    const bool needAntiAlias = (sampleRate > 50000.0);
    std::vector<float> bandLimited;
    if (needAntiAlias)
        bandLimited.resize(static_cast<size_t>(specLen), 0.0f);
    double aaState = 0.0;

    for (int i = 0; i < specLen; ++i)
    {
        double x = static_cast<double>(activeSamples[i]);
        sumSqActive += x * x;

        // 2-pole cascaded crossovers (12 dB/oct, exact Hz cutoffs across 22.05..192 kHz)
        s1Sub += aSub * (x - s1Sub);
        s2Sub += aSub * (s1Sub - s2Sub);

        s1Low += aLow * (x - s1Low);
        s2Low += aLow * (s1Low - s2Low);

        s1Mid += aMid * (x - s1Mid);
        s2Mid += aMid * (s1Mid - s2Mid);

        double bSub    = s2Sub;
        double bLowMid = s2Low - s2Sub;
        double bMid    = s2Mid - s2Low;
        double bHigh   = x - s2Mid;

        eSub    += bSub * bSub;
        eLowMid += bLowMid * bLowMid;
        eMid    += bMid * bMid;
        eHigh   += bHigh * bHigh;

        if (needAntiAlias)
        {
            aaState += a20k * (x - aaState);
            bandLimited[static_cast<size_t>(i)] = static_cast<float>(aaState);
        }
    }

    const float rms = static_cast<float>(std::sqrt(sumSqActive / specLen));
    const float crestFactor = localPeakVal / (rms + 1e-6f);

    const double totalBandEnergy = eSub + eLowMid + eMid + eHigh + 1e-12;
    const double subShare    = eSub / totalBandEnergy;
    const double lowMidShare = eLowMid / totalBandEnergy;
    const double midShare    = eMid / totalBandEnergy;
    const double highShare   = eHigh / totalBandEnergy;

    // Evaluate ZCR & first-difference HF Ratio on an exact 44.1 kHz virtual grid for 100% sample-rate invariance
    const float* zcrSrc = needAntiAlias ? bandLimited.data() : activeSamples;
    const double step44k = sampleRate / 44100.0;
    const int numVirtualSamples = std::max(2, static_cast<int>((specLen - 1) / step44k));

    int zeroCrossings = 0;
    double energyX = 0.0;
    double energyD = 0.0;
    float prevSample = zcrSrc[0];

    for (int k = 1; k < numVirtualSamples; ++k)
    {
        double srcPos = k * step44k;
        int idx = static_cast<int>(srcPos);
        float frac = static_cast<float>(srcPos - idx);
        float s0 = zcrSrc[idx];
        float s1 = zcrSrc[std::min(specLen - 1, idx + 1)];
        float currSample = s0 + frac * (s1 - s0);

        if ((prevSample < 0.0f && currSample >= 0.0f) || (prevSample > 0.0f && currSample <= 0.0f))
            ++zeroCrossings;

        energyX += static_cast<double>(currSample) * currSample;
        float diff = currSample - prevSample;
        energyD += static_cast<double>(diff) * diff;
        prevSample = currSample;
    }

    const double zcr = static_cast<double>(zeroCrossings) / static_cast<double>(numVirtualSamples);
    const double highFreqRatio = juce::jlimit(0.0, 4.0, energyD / (energyX + 1e-9));

    // Persist sample-rate-invariant DSP similarity descriptors
    item.zcr = zcr;
    item.highFreqRatio = highFreqRatio;
    item.decayRatio = decayRatio;
    item.crestFactor = static_cast<double>(crestFactor);

    // 5. Multi-Band Physical Audio Classification (runs whenever no semantic category tag exists yet;
    //    note that genre tags like #Trap/#Techno or #Key_* do NOT block DSP instrument classification!)
    if (!TagDatabaseManager::hasSemanticCategoryTag(item.tags))
    {
        if (!isLoop)
        {
            if (activeDuration <= 1.15 && attackTimeMs <= 30.0 && decayRatio < 0.30 && crestFactor > 2.2f &&
                (subShare >= 0.50 || (subShare >= 0.32 && (subShare + lowMidShare) >= 0.82 && highShare < 0.06)))
            {
                item.tags.insert("#Kick");
            }
            else if ((subShare + lowMidShare) >= 0.74 && highShare < 0.08 &&
                     (decayRatio >= 0.20 || activeDuration > 0.50 || attackTimeMs > 25.0))
            {
                item.tags.insert("#Bass");
            }
            else if (lowMidShare >= 0.42 && subShare < 0.50 && highShare < 0.16 &&
                     attackTimeMs <= 28.0 && decayRatio < 0.32 && activeDuration <= 1.20)
            {
                item.tags.insert("#Tom");
            }
            else if (highShare >= 0.46 || highFreqRatio >= 1.05 || zcr >= 0.20)
            {
                if (attackTimeMs <= 30.0)
                {
                    if (activeDuration <= 0.28 && decayRatio < 0.20)
                        item.tags.insert("#ClosedHat");
                    else if (activeDuration <= 1.10)
                        item.tags.insert("#OpenHat");
                    else if (crestFactor > 3.4f)
                        item.tags.insert("#Crash");
                    else
                        item.tags.insert("#Cymbal");
                }
                else
                {
                    item.tags.insert("#Riser");
                }
            }
            else if (attackTimeMs <= 34.0 && (midShare + highShare) >= 0.32 && subShare < 0.42 && crestFactor > 2.6f)
            {
                if ((midShare >= 0.36 && subShare < 0.14 && decayRatio < 0.22) ||
                    (decayRatio < 0.14 && activeDuration < 0.50 && highFreqRatio >= 0.45))
                {
                    item.tags.insert("#Clap");
                }
                else
                {
                    item.tags.insert("#Snare");
                }
            }
            else if (attackTimeMs <= 25.0 && crestFactor > 3.4f && activeDuration <= 0.90)
            {
                item.tags.insert("#Percussion");
            }
            else if (attackTimeMs > 110.0 && activeDuration >= 0.80)
            {
                if (highShare >= 0.24 || highFreqRatio >= 0.55)
                    item.tags.insert("#Riser");
                else
                    item.tags.insert("#Pad");
            }
            else if (attackTimeMs <= 55.0 && decayRatio < 0.24)
            {
                item.tags.insert("#Pluck");
            }
            else
            {
                item.tags.insert("#Synth");
            }
        }
        else
        {
            if ((subShare + lowMidShare) >= 0.78 && highShare < 0.06)
            {
                item.tags.insert("#BassLoop");
                item.tags.insert("#Bass");
            }
            else if (subShare < 0.08 && crestFactor > 3.5f && highShare >= 0.25)
            {
                item.tags.insert("#TopLoop");
                item.tags.insert("#Percussion");
            }
            else if (crestFactor > 3.3f && (highShare >= 0.12 || highFreqRatio > 0.30))
            {
                item.tags.insert("#DrumLoop");
            }
            else
            {
                item.tags.insert("#MelodicLoop");
                item.tags.insert("#Synth");
            }
        }
    }

    // 6. Sample-Rate-Invariant Loop BPM Autocorrelation (4.5s window captures 60..200 BPM reliably)
    if (item.bpm <= 0.0 && isLoop && duration >= 1.0)
    {
        double estimatedBpm = 0.0;

        // Constant ~5.8 ms envelope frame size across all sample rates (256 @ 44.1k, 279 @ 48k, 557 @ 96k)
        const int envBlockSize = std::max(64, juce::roundToInt(sampleRate / 172.265625));
        const int numEnvBlocks = maxSamplesToRead / envBlockSize;
        if (numEnvBlocks > 36)
        {
            std::vector<float> env(static_cast<size_t>(numEnvBlocks), 0.0f);
            for (int b = 0; b < numEnvBlocks; ++b)
            {
                float bMax = 0.0f;
                int start = b * envBlockSize;
                int end = std::min(maxSamplesToRead, (b + 1) * envBlockSize);
                for (int i = start; i < end; ++i)
                {
                    float val = std::abs(mono[static_cast<size_t>(i)]);
                    if (val > bMax) bMax = val;
                }
                env[static_cast<size_t>(b)] = bMax / maxVal; // Peak-normalized envelope for gain invariance
            }

            std::vector<float> onsets(static_cast<size_t>(numEnvBlocks), 0.0f);
            int onsetCount = 0;
            for (int i = 1; i < numEnvBlocks; ++i)
            {
                float diff = std::max(0.0f, env[static_cast<size_t>(i)] - env[static_cast<size_t>(i - 1)]);
                onsets[static_cast<size_t>(i)] = diff;
                if (diff > 0.075f) ++onsetCount;
            }

            if (onsetCount >= 3)
            {
                const double fsEnv = sampleRate / static_cast<double>(envBlockSize);
                int minLag = std::max(2, static_cast<int>(60.0 * fsEnv / 200.0));
                int maxLag = std::min(numEnvBlocks - 2, static_cast<int>(60.0 * fsEnv / 60.0));

                if (maxLag > minLag + 2)
                {
                    std::vector<double> rValues(static_cast<size_t>(maxLag + 2), 0.0);
                    double maxR = -1.0;
                    int bestLag = 0;

                    for (int lag = minLag; lag <= maxLag; ++lag)
                    {
                        double sumXY = 0.0, sumX2 = 0.0, sumY2 = 0.0;
                        int count = 0;
                        for (int i = lag; i < numEnvBlocks; ++i)
                        {
                            float x = onsets[static_cast<size_t>(i)];
                            float y = onsets[static_cast<size_t>(i - lag)];
                            sumXY += static_cast<double>(x) * y;
                            sumX2 += static_cast<double>(x) * x;
                            sumY2 += static_cast<double>(y) * y;
                            ++count;
                        }

                        if (count >= 16 && sumX2 > 1e-9 && sumY2 > 1e-9)
                        {
                            double r = sumXY / std::sqrt(sumX2 * sumY2);
                            rValues[static_cast<size_t>(lag)] = r;
                            if (r > maxR)
                            {
                                maxR = r;
                                bestLag = lag;
                            }
                        }
                    }

                    if (bestLag > minLag && bestLag < maxLag && maxR >= 0.36)
                    {
                        double y1 = rValues[static_cast<size_t>(bestLag - 1)];
                        double y2 = rValues[static_cast<size_t>(bestLag)];
                        double y3 = rValues[static_cast<size_t>(bestLag + 1)];
                        double denom = (y1 - 2.0 * y2 + y3);
                        double delta = 0.0;
                        if (std::abs(denom) > 1e-7)
                        {
                            delta = juce::jlimit(-0.5, 0.5, (y1 - y3) / (2.0 * denom));
                        }

                        double refinedLag = static_cast<double>(bestLag) + delta;
                        double dspBpm = (60.0 * fsEnv) / refinedLag;

                        if (dspBpm < 75.0)
                        {
                            int doubleLag = juce::roundToInt(bestLag / 2.0);
                            if (doubleLag >= minLag && rValues[static_cast<size_t>(doubleLag)] > maxR * 0.68)
                                dspBpm *= 2.0;
                        }
                        else if (dspBpm > 175.0)
                        {
                            int halfLag = juce::roundToInt(bestLag * 2.0);
                            if (halfLag <= maxLag && rValues[static_cast<size_t>(halfLag)] > maxR * 0.68)
                                dspBpm /= 2.0;
                        }

                        double nearestInt = std::round(dspBpm);
                        if (std::abs(dspBpm - nearestInt) < 0.35)
                            dspBpm = nearestInt;
                        else
                            dspBpm = std::round(dspBpm * 10.0) / 10.0;

                        double bestBarBpm = 0.0;
                        const int candidateBeats[] = { 4, 8, 16, 32, 64 };
                        for (int b : candidateBeats)
                        {
                            double testBpm = (60.0 * b) / duration;
                            if (std::abs(testBpm - dspBpm) < 2.5)
                            {
                                double roundedBar = std::round(testBpm);
                                if (std::abs(testBpm - roundedBar) < 0.25)
                                {
                                    bestBarBpm = roundedBar;
                                    break;
                                }
                            }
                        }

                        estimatedBpm = (bestBarBpm > 0.0) ? bestBarBpm : dspBpm;
                    }
                }
            }
        }

        if (estimatedBpm >= 60.0 && estimatedBpm <= 200.0)
        {
            item.bpm = estimatedBpm;
            item.tags.insert("#" + juce::String(juce::roundToInt(estimatedBpm)) + "BPM");
        }
    }

    // 7. Acoustic DSP Character Tagging
    if (crestFactor > 3.8f && attackTimeMs <= 20.0)
        item.tags.insert("#Punchy");
    if (highFreqRatio >= 0.65 || highShare >= 0.35)
        item.tags.insert("#Bright");
    else if (highFreqRatio < 0.16 && (subShare + lowMidShare) >= 0.78)
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

    // 1. Parse audio header metadata FIRST so durationSeconds & numChannels are valid for all formats (MP3/FLAC/OGG/WAV/AIFF)
    bool headerParsed = false;
    if (item.fileExtension == ".wav")
    {
        if (parseWavHeaderFast(file, item.sampleRate, item.numChannels, item.bitDepth, item.durationSeconds))
            headerParsed = true;
    }
    else if (item.fileExtension == ".aif" || item.fileExtension == ".aiff" || item.fileExtension == ".aifc")
    {
        if (parseAiffHeaderFast(file, item.sampleRate, item.numChannels, item.bitDepth, item.durationSeconds))
            headerParsed = true;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(localFormatManager.createReaderFor(file));
    if (reader != nullptr && !headerParsed)
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

    // 2. Extract BPM from filename or parent folder path as authoritative ground truth
    double fnBpm = TagDatabaseManager::extractBpmFromFilename(file.getFileNameWithoutExtension());
    if (fnBpm <= 0.0)
    {
        fnBpm = TagDatabaseManager::extractBpmFromFilename(file.getParentDirectory().getFileName());
    }

    if (fnBpm >= 40.0 && fnBpm <= 260.0)
    {
        item.bpm = fnBpm;
        item.tags.insert("#" + juce::String(juce::roundToInt(fnBpm)) + "BPM");
    }

    // 3. Auto-infer tags from path, file type, accurate channel count, and accurate duration
    auto pathTags = TagDatabaseManager::inferTagsFromPath(absPath, item.durationSeconds, item.numChannels);
    for (const auto& t : pathTags)
    {
        item.tags.insert(t);
    }

    // 4. Run sample-rate-invariant multi-band DSP classifier and loop BPM estimation
    if (reader != nullptr)
    {
        analyzeAudioData(reader.get(), item);
    }

    // 5. Final conflict sanitization & BPM synchronization
    TagDatabaseManager::sanitizeItem(item);
    item.precomputeCachedStrings();

    return item;
}

} // namespace openwav
