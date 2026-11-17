#include "AutoSamplerEngine.h"
#include <algorithm>
#include <cmath>

namespace openwav
{

namespace
{
constexpr int kBlockSize = 512;
constexpr float kNormalizeTargetPeak = 0.8912509f; // -1.0 dBFS
constexpr float kTailSilenceThreshold = 0.0003162f; // -70 dBFS
constexpr float kLeadingSilenceThreshold = 0.0001f; // -80 dBFS
} // namespace

AutoSamplerEngine::AutoSamplerEngine()
{
    formatManager.addDefaultFormats();
    loadKnownPlugins();
}

AutoSamplerEngine::~AutoSamplerEngine()
{
    aliveToken->store(false, std::memory_order_release);
    stopTimer();
    activeScanner.reset();
    scanning.store(false, std::memory_order_release);
    cancelSampling();

    if (sampleThread && sampleThread->joinable())
        sampleThread->join();
}

juce::File AutoSamplerEngine::getKnownPluginsFile() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("OpenWav");
    if (!dir.exists())
        dir.createDirectory();
    return dir.getChildFile("known_plugins.xml");
}

juce::File AutoSamplerEngine::getDeadMansPedalFile() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("OpenWav");
    if (!dir.exists())
        dir.createDirectory();
    return dir.getChildFile("known_plugins_deadmans.txt");
}

juce::File AutoSamplerEngine::getDefaultOutputBaseDirectory()
{
    auto docs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto base = docs.getChildFile("OWMB_AutoSampled");
    if (!base.exists())
        base.createDirectory();
    return base;
}

juce::String AutoSamplerEngine::midiNoteToName(int midiNote)
{
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int n = juce::jlimit(0, 127, midiNote);
    int octave = (n / 12) - 1;
    return juce::String(names[n % 12]) + juce::String(octave);
}

void AutoSamplerEngine::loadKnownPlugins()
{
    auto file = getKnownPluginsFile();
    if (!file.existsAsFile())
        return;

    auto xml = juce::XmlDocument::parse(file);
    if (xml != nullptr)
    {
        const juce::ScopedLock sl(pluginListLock);
        knownPluginList.recreateFromXml(*xml);
    }
}

void AutoSamplerEngine::saveKnownPlugins() const
{
    std::unique_ptr<juce::XmlElement> xml;
    {
        const juce::ScopedLock sl(pluginListLock);
        xml = knownPluginList.createXml();
    }
    if (xml != nullptr)
        xml->writeTo(getKnownPluginsFile());
}

std::vector<juce::PluginDescription> AutoSamplerEngine::getInstrumentPlugins() const
{
    const juce::ScopedLock sl(pluginListLock);
    std::vector<juce::PluginDescription> result;
    for (const auto& desc : knownPluginList.getTypes())
    {
        if (desc.isInstrument)
            result.push_back(desc);
    }
    std::sort(result.begin(), result.end(), [](const juce::PluginDescription& a, const juce::PluginDescription& b) {
        return a.name.compareIgnoreCase(b.name) < 0;
    });
    return result;
}

void AutoSamplerEngine::cancelScan()
{
    if (!scanning.load(std::memory_order_acquire))
        return;

    finishScan(true);
}

void AutoSamplerEngine::advanceScanFormat()
{
    activeScanner.reset();
    auto deadMansFile = getDeadMansPedalFile();
    int numFormats = formatManager.getNumFormats();

    while (scanFormatIndex < numFormats)
    {
        auto* format = formatManager.getFormat(scanFormatIndex++);
        if (format == nullptr)
            continue;

        auto paths = format->getDefaultLocationsToSearch();
        activeScanner = std::make_unique<juce::PluginDirectoryScanner>(
            knownPluginList, *format, paths, true, deadMansFile, false);
        break;
    }
}

void AutoSamplerEngine::finishScan(bool cancelled)
{
    stopTimer();
    activeScanner.reset();
    scanFormatIndex = 0;

    if (aliveToken->load(std::memory_order_acquire))
        saveKnownPlugins();

    int foundCount = static_cast<int>(getInstrumentPlugins().size());
    scanning.store(false, std::memory_order_release);

    auto cb = std::move(scanCompletionCb);
    scanProgressCb = nullptr;
    scanCompletionCb = nullptr;

    if (cb && !cancelled && aliveToken->load(std::memory_order_acquire))
        cb(foundCount);
}

void AutoSamplerEngine::timerCallback()
{
    if (!scanning.load(std::memory_order_acquire) || !aliveToken->load(std::memory_order_acquire))
    {
        stopTimer();
        return;
    }

    if (activeScanner == nullptr)
    {
        finishScan(false);
        return;
    }

    juce::String pluginName;
    bool hasMore = false;
    {
        const juce::ScopedLock sl(pluginListLock);
        hasMore = activeScanner->scanNextFile(true, pluginName);
    }

    int numFormats = std::max(1, formatManager.getNumFormats());
    int currentFmtIdx = juce::jlimit(0, numFormats - 1, scanFormatIndex - 1);
    float fmtProgress = activeScanner->getProgress();
    float overallProgress = (static_cast<float>(currentFmtIdx) + fmtProgress) / static_cast<float>(numFormats);

    if (scanProgressCb && pluginName.isNotEmpty())
        scanProgressCb(overallProgress, pluginName);

    if (!hasMore)
    {
        advanceScanFormat();
        if (activeScanner == nullptr)
            finishScan(false);
    }
}

void AutoSamplerEngine::scanPluginsAsync(
    std::function<void(float progress, const juce::String& pluginName)> progressCb,
    std::function<void(int numInstrumentsFound)> completionCb)
{
    if (scanning.load(std::memory_order_acquire))
        return;

    scanning.store(true, std::memory_order_release);
    scanProgressCb = std::move(progressCb);
    scanCompletionCb = std::move(completionCb);
    scanFormatIndex = 0;

    advanceScanFormat();
    if (activeScanner != nullptr)
        startTimer(1);
    else
        finishScan(false);
}

std::unique_ptr<juce::AudioPluginInstance> AutoSamplerEngine::createPluginInstance(
    const juce::PluginDescription& desc, double sampleRate, int blockSize, juce::String& outError)
{
    return formatManager.createPluginInstance(desc, sampleRate, blockSize, outError);
}

void AutoSamplerEngine::cancelSampling()
{
    cancelSampleRequested.store(true, std::memory_order_release);
}

void AutoSamplerEngine::startAutoSample(
    juce::AudioPluginInstance* pluginInstance,
    const AutoSamplerConfig& config,
    std::function<void(float progress, const juce::String& status)> progressCb,
    std::function<void(const SampleMapState& resultState, bool success, const juce::String& error)> completionCb)
{
    if (pluginInstance == nullptr || sampling.load(std::memory_order_acquire))
    {
        if (completionCb)
            completionCb({}, false, pluginInstance == nullptr ? "No plugin instrument loaded." : "Auto-sampling is already running.");
        return;
    }

    if (sampleThread && sampleThread->joinable())
        sampleThread->join();

    cancelSampleRequested.store(false, std::memory_order_release);
    sampling.store(true, std::memory_order_release);

    auto token = aliveToken;
    sampleThread = std::make_unique<std::thread>([this, token, pluginInstance, config, progressCb, completionCb]() {
        SampleMapState resultState;
        bool success = false;
        juce::String errorMsg;

        try
        {
            const int minN = juce::jlimit(0, 127, std::min(config.minNote, config.maxNote));
            const int maxN = juce::jlimit(minN, 127, std::max(config.minNote, config.maxNote));
            const int stride = std::max(1, config.semitoneStride);
            const int velLayers = juce::jlimit(1, 8, config.velocityLayers);
            const int rrCount = juce::jlimit(1, 8, config.roundRobinCount);
            const double sr = (config.sampleRate > 0.0) ? config.sampleRate : 44100.0;
            const int midiCh = juce::jlimit(1, 16, config.midiChannel);

            std::vector<int> notes;
            for (int n = minN; n <= maxN; n += stride)
                notes.push_back(n);
            if (notes.empty())
                notes.push_back(minN);

            juce::String cleanInstName = juce::File::createLegalFileName(
                config.instrumentName.trim().isEmpty() ? "AutoSampled" : config.instrumentName.trim());
            juce::File outDir = config.outputDirectory.getFullPathName().isNotEmpty()
                                    ? config.outputDirectory
                                    : getDefaultOutputBaseDirectory().getChildFile(cleanInstName);
            outDir.createDirectory();

            const int numOutCh = std::max(2, pluginInstance->getTotalNumOutputChannels());
            const int numInCh = pluginInstance->getTotalNumInputChannels();
            const int bufCh = std::max(numInCh, numOutCh);

            pluginInstance->setNonRealtime(true);
            pluginInstance->prepareToPlay(sr, kBlockSize);
            pluginInstance->reset();

            const int sustainSamples = std::max(kBlockSize, static_cast<int>(config.sustainSeconds * sr));
            const int maxTailSamples = std::max(kBlockSize, static_cast<int>(config.maxTailSeconds * sr));
            const int silenceHoldSamples = static_cast<int>(0.20 * sr); // 200ms below threshold

            struct CapturedTake
            {
                juce::AudioBuffer<float> buffer;
                juce::File file;
                SampleMapZoneState zone;
                float peak { 0.0f };
            };
            std::vector<CapturedTake> takes;
            float globalMaxPeak = 0.0f;

            const int totalJobs = static_cast<int>(notes.size()) * velLayers * rrCount;
            int completedJobs = 0;

            juce::AudioBuffer<float> blockBuf(bufCh, kBlockSize);
            juce::MidiBuffer midiBuf;

            for (size_t nIdx = 0; nIdx < notes.size(); ++nIdx)
            {
                if (cancelSampleRequested.load(std::memory_order_acquire) || !token->load(std::memory_order_acquire))
                    break;

                const int rootNote = notes[nIdx];
                const int keyLow = (nIdx == 0) ? minN : (rootNote - (stride / 2));
                const int keyHigh = (nIdx + 1 == notes.size()) ? maxN : (rootNote + ((stride - 1) / 2));

                for (int vIdx = 0; vIdx < velLayers; ++vIdx)
                {
                    if (cancelSampleRequested.load(std::memory_order_acquire) || !token->load(std::memory_order_acquire))
                        break;

                    const int velLow = (vIdx * 128) / velLayers;
                    const int velHigh = ((vIdx + 1) * 128) / velLayers - 1;
                    const int triggerVel = juce::jlimit(1, 127, (vIdx == velLayers - 1) ? 127 : ((velLow + velHigh + 1) / 2));

                    for (int rrIdx = 0; rrIdx < rrCount; ++rrIdx)
                    {
                        if (cancelSampleRequested.load(std::memory_order_acquire) || !token->load(std::memory_order_acquire))
                            break;

                        if (progressCb)
                        {
                            float prog = static_cast<float>(completedJobs) / static_cast<float>(std::max(1, totalJobs));
                            juce::String status = midiNoteToName(rootNote) + "  |  Vel " + juce::String(triggerVel)
                                                + "  |  RR " + juce::String(rrIdx + 1) + "/" + juce::String(rrCount);
                            juce::MessageManager::callAsync([token, progressCb, prog, status]() {
                                if (token->load(std::memory_order_acquire))
                                    progressCb(prog, status);
                            });
                        }

                        // Flush short silence before note-on without calling reset() between RR takes,
                        // so instruments with internal round-robin or free-running drift advance naturally.
                        midiBuf.clear();
                        if (rrIdx == 0)
                            midiBuf.addEvent(juce::MidiMessage::allNotesOff(midiCh), 0);
                        for (int flush = 0; flush < 4; ++flush)
                        {
                            blockBuf.clear();
                            pluginInstance->processBlock(blockBuf, midiBuf);
                            midiBuf.clear();
                        }

                        const int maxTotalSamples = sustainSamples + maxTailSamples;
                        juce::AudioBuffer<float> captureBuf(2, maxTotalSamples);
                        captureBuf.clear();

                        int writtenSamples = 0;
                        bool noteOffSent = false;
                        int silentRun = 0;

                        while (writtenSamples < maxTotalSamples)
                        {
                            if (cancelSampleRequested.load(std::memory_order_acquire) || !token->load(std::memory_order_acquire))
                                break;

                            midiBuf.clear();
                            blockBuf.clear();

                            if (writtenSamples == 0)
                            {
                                midiBuf.addEvent(juce::MidiMessage::noteOn(midiCh, rootNote, static_cast<juce::uint8>(triggerVel)), 0);
                            }
                            if (!noteOffSent && writtenSamples >= sustainSamples)
                            {
                                midiBuf.addEvent(juce::MidiMessage::noteOff(midiCh, rootNote), 0);
                                noteOffSent = true;
                            }

                            pluginInstance->processBlock(blockBuf, midiBuf);

                            int toCopy = std::min(kBlockSize, maxTotalSamples - writtenSamples);
                            for (int ch = 0; ch < 2; ++ch)
                            {
                                int srcCh = std::min(ch, numOutCh - 1);
                                captureBuf.copyFrom(ch, writtenSamples, blockBuf, srcCh, 0, toCopy);
                            }

                            float blockMag = std::max(blockBuf.getMagnitude(0, 0, toCopy),
                                                      numOutCh > 1 ? blockBuf.getMagnitude(1, 0, toCopy) : 0.0f);

                            writtenSamples += toCopy;

                            if (noteOffSent && config.autoTailDetect)
                            {
                                if (blockMag < kTailSilenceThreshold)
                                {
                                    silentRun += toCopy;
                                    if (silentRun >= silenceHoldSamples)
                                        break;
                                }
                                else
                                {
                                    silentRun = 0;
                                }
                            }
                        }

                        if (cancelSampleRequested.load(std::memory_order_acquire) || !token->load(std::memory_order_acquire))
                            break;

                        // Trim leading silence if enabled
                        int trimStart = 0;
                        if (config.trimLeadingSilence && writtenSamples > 0)
                        {
                            while (trimStart < writtenSamples - 1)
                            {
                                float sL = std::abs(captureBuf.getSample(0, trimStart));
                                float sR = std::abs(captureBuf.getSample(1, trimStart));
                                if (std::max(sL, sR) >= kLeadingSilenceThreshold)
                                    break;
                                ++trimStart;
                            }
                            trimStart = std::max(0, trimStart - 16); // Keep 16 samples of pre-roll
                        }

                        int finalLen = std::max(1, writtenSamples - trimStart);
                        juce::AudioBuffer<float> finalBuf(2, finalLen);
                        for (int ch = 0; ch < 2; ++ch)
                            finalBuf.copyFrom(ch, 0, captureBuf, ch, trimStart, finalLen);

                        float peak = finalBuf.getMagnitude(0, 0, finalLen);
                        globalMaxPeak = std::max(globalMaxPeak, peak);

                        juce::String noteNameClean = midiNoteToName(rootNote).replace("#", "s");
                        juce::String fileName = cleanInstName + "_" + noteNameClean
                                              + "_v" + juce::String(triggerVel)
                                              + "_rr" + juce::String(rrIdx + 1) + ".wav";
                        juce::File wavFile = outDir.getChildFile(fileName);

                        SampleMapZoneState zone;
                        zone.filePath = wavFile.getFullPathName();
                        zone.sampleName = fileName;
                        zone.rootNote = rootNote;
                        zone.keyLow = juce::jlimit(0, 127, keyLow);
                        zone.keyHigh = juce::jlimit(zone.keyLow, 127, keyHigh);
                        zone.velLow = juce::jlimit(0, 127, velLow);
                        zone.velHigh = juce::jlimit(zone.velLow, 127, velHigh);
                        zone.roundRobinIndex = rrIdx + 1;

                        takes.push_back({ std::move(finalBuf), wavFile, std::move(zone), peak });

                        // Capture dedicated Note-Off / Release sample from the post-noteOff segment
                        if (config.captureReleaseTriggers && writtenSamples > sustainSamples + 32)
                        {
                            const int relStart = sustainSamples;
                            const int relLen = writtenSamples - relStart;
                            juce::AudioBuffer<float> relBuf(2, relLen);
                            for (int ch = 0; ch < 2; ++ch)
                                relBuf.copyFrom(ch, 0, captureBuf, ch, relStart, relLen);

                            const float relPeak = relBuf.getMagnitude(0, 0, relLen);
                            if (relPeak >= 0.0003f) // > ~-70 dBFS
                            {
                                const int fadeSamples = std::min(64, relLen / 4);
                                if (fadeSamples > 0)
                                    relBuf.applyGainRamp(0, 0, fadeSamples, 0.0f, 1.0f);

                                globalMaxPeak = std::max(globalMaxPeak, relPeak);
                                juce::String relFileName = cleanInstName + "_" + noteNameClean
                                                         + "_v" + juce::String(triggerVel)
                                                         + "_rr" + juce::String(rrIdx + 1) + "_rel.wav";
                                juce::File relWavFile = outDir.getChildFile(relFileName);

                                SampleMapZoneState relZone;
                                relZone.filePath = relWavFile.getFullPathName();
                                relZone.sampleName = relFileName;
                                relZone.rootNote = rootNote;
                                relZone.keyLow = juce::jlimit(0, 127, keyLow);
                                relZone.keyHigh = juce::jlimit(relZone.keyLow, 127, keyHigh);
                                relZone.velLow = juce::jlimit(0, 127, velLow);
                                relZone.velHigh = juce::jlimit(relZone.velLow, 127, velHigh);
                                relZone.roundRobinIndex = rrIdx + 1;
                                relZone.attackMs = 2.0f;
                                relZone.releaseMs = 250.0f;
                                relZone.isReleaseTrigger = true;

                                takes.push_back({ std::move(relBuf), relWavFile, std::move(relZone), relPeak });
                            }
                        }

                        ++completedJobs;
                    }
                }
            }

            pluginInstance->setNonRealtime(false);

            if (cancelSampleRequested.load(std::memory_order_acquire) || !token->load(std::memory_order_acquire))
            {
                errorMsg = "Auto-sampling cancelled.";
                success = false;
            }
            else if (takes.empty())
            {
                errorMsg = "No samples were captured.";
                success = false;
            }
            else
            {
                juce::WavAudioFormat wavFormat;
                juce::String sfzText;
                sfzText << "// Generated by OWMB Auto-Sampler\n";
                sfzText << "<control>\ndefault_path=\n\n";
                sfzText << "<global>\nloop_mode=no_loop\n\n";

                for (auto& take : takes)
                {
                    if (config.normalizeMode == 1 && globalMaxPeak > 0.00001f)
                        take.buffer.applyGain(kNormalizeTargetPeak / globalMaxPeak);
                    else if (config.normalizeMode == 2 && take.peak > 0.00001f)
                        take.buffer.applyGain(kNormalizeTargetPeak / take.peak);

                    if (take.file.existsAsFile())
                        take.file.deleteFile();

                    std::unique_ptr<juce::FileOutputStream> fos(take.file.createOutputStream());
                    if (fos != nullptr && fos->openedOk())
                    {
                        std::unique_ptr<juce::AudioFormatWriter> writer(
                            wavFormat.createWriterFor(fos.get(), sr, 2, 24, {}, 0));
                        if (writer != nullptr)
                        {
                            fos.release();
                            writer->writeFromAudioSampleBuffer(take.buffer, 0, take.buffer.getNumSamples());
                        }
                    }

                    const auto& z = take.zone;
                    sfzText << "<region> sample=" << take.file.getFileName()
                            << " lokey=" << z.keyLow
                            << " hikey=" << z.keyHigh
                            << " pitch_keycenter=" << z.rootNote
                            << " lovel=" << z.velLow
                            << " hivel=" << z.velHigh;
                    if (rrCount > 1)
                        sfzText << " seq_length=" << rrCount << " seq_position=" << z.roundRobinIndex;
                    if (z.isReleaseTrigger)
                        sfzText << " trigger=release";
                    sfzText << "\n";

                    resultState.zones.push_back(z);
                }

                if (config.exportSfz)
                {
                    auto sfzFile = outDir.getChildFile(cleanInstName + ".sfz");
                    sfzFile.replaceWithText(sfzText);
                }

                resultState.roundRobinMode = 0;
                resultState.pitchTrackingEnabled = true;
                resultState.instrument.instrumentName = config.instrumentName.trim().isEmpty()
                                                            ? "AutoSampled Instrument"
                                                            : config.instrumentName.trim();

                if (progressCb)
                {
                    juce::MessageManager::callAsync([token, progressCb]() {
                        if (token->load(std::memory_order_acquire))
                            progressCb(1.0f, "Auto-sampling complete!");
                    });
                }
                success = true;
            }
        }
        catch (const std::exception& e)
        {
            pluginInstance->setNonRealtime(false);
            errorMsg = e.what();
            success = false;
        }
        catch (...)
        {
            pluginInstance->setNonRealtime(false);
            errorMsg = "Unknown error during auto-sampling.";
            success = false;
        }

        sampling.store(false, std::memory_order_release);

        if (completionCb && token->load(std::memory_order_acquire))
        {
            juce::MessageManager::callAsync([token, completionCb, resultState, success, errorMsg]() {
                if (token->load(std::memory_order_acquire))
                    completionCb(resultState, success, errorMsg);
            });
        }
    });
}

} // namespace openwav
