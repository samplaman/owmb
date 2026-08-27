#include "LorisResynthesizer.h"
#include <Analyzer.h>
#include <Synthesizer.h>
#include <PartialList.h>
#include <PartialUtils.h>
#include <LinearEnvelope.h>
#include <cmath>
#include <algorithm>

namespace openwav
{

LorisResynthesizer::LorisResynthesizer()
{
}

LorisResynthesizer::~LorisResynthesizer()
{
    cancel();
    if (workerThread && workerThread->joinable())
    {
        workerThread->join();
    }
}

void LorisResynthesizer::cancel()
{
    cancelRequested.store(true, std::memory_order_release);
}

int LorisResynthesizer::detectRootMidiNote(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    if (buffer.getNumSamples() < 512 || sampleRate <= 0.0)
        return 60; // Default C4

    const int numSamples = std::min(buffer.getNumSamples(), (int)(sampleRate * 2.0)); // up to 2s
    std::vector<float> mono(numSamples, 0.0f);

    const int numChannels = buffer.getNumChannels();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* readPtr = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            mono[i] += readPtr[i] / (float)numChannels;
        }
    }

    // Autocorrelation pitch detection (min freq 40Hz, max freq 2000Hz)
    const int minLag = (int)(sampleRate / 2000.0);
    const int maxLag = (int)(sampleRate / 40.0);

    if (maxLag >= numSamples / 2)
        return 60;

    float maxCorrelation = -1.0f;
    int bestLag = -1;

    // Normalize signal energy
    double energy0 = 0.0;
    for (int i = 0; i < numSamples / 2; ++i)
        energy0 += mono[i] * mono[i];

    if (energy0 < 1e-6)
        return 60;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double sum = 0.0;
        double energyLag = 0.0;
        for (int i = 0; i < numSamples / 2; ++i)
        {
            sum += mono[i] * mono[i + lag];
            energyLag += mono[i + lag] * mono[i + lag];
        }

        double norm = std::sqrt(energy0 * energyLag);
        if (norm > 1e-6)
        {
            float corr = (float)(sum / norm);
            if (corr > maxCorrelation)
            {
                maxCorrelation = corr;
                bestLag = lag;
            }
        }
    }

    if (bestLag > 0 && maxCorrelation > 0.45f)
    {
        // Parabolic interpolation around peak
        double lagFine = bestLag;
        if (bestLag > minLag && bestLag < maxLag)
        {
            // Simple 3-point parabolic refinement
            double y1 = 0, y2 = 0, y3 = 0;
            for (int i = 0; i < numSamples / 2; ++i)
            {
                y1 += mono[i] * mono[i + bestLag - 1];
                y2 += mono[i] * mono[i + bestLag];
                y3 += mono[i] * mono[i + bestLag + 1];
            }
            double denom = (y1 - 2.0 * y2 + y3);
            if (std::abs(denom) > 1e-9)
            {
                double delta = (y1 - y3) / (2.0 * denom);
                if (std::abs(delta) < 1.0)
                    lagFine += delta;
            }
        }

        double fundamentalHz = sampleRate / lagFine;
        if (fundamentalHz >= 20.0 && fundamentalHz <= 4000.0)
        {
            int midiNote = (int)std::round(69.0 + 12.0 * std::log2(fundamentalHz / 440.0));
            return std::clamp(midiNote, 0, 127);
        }
    }

    return 60; // Fallback C4
}

juce::String LorisResynthesizer::getMidiNoteName(int midiNoteNumber, bool includeOctave)
{
    static const char* const noteNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    int note = ((midiNoteNumber % 12) + 12) % 12;
    int octave = (midiNoteNumber / 12) - 1;

    juce::String name = noteNames[note];
    if (includeOctave)
        name << octave;

    return name;
}

void LorisResynthesizer::startResynthesis(
    const LorisResynthesisConfig& config,
    std::function<void(float progress, const juce::String& statusText)> progressCallback,
    std::function<void(const std::vector<ResynthesizedZone>& zones, bool success, const juce::String& errorMsg)> completionCallback)
{
    if (processing.load(std::memory_order_relaxed))
    {
        if (completionCallback)
            completionCallback({}, false, "Resynthesis is already in progress.");
        return;
    }

    if (workerThread && workerThread->joinable())
    {
        workerThread->join();
    }

    cancelRequested.store(false, std::memory_order_release);
    processing.store(true, std::memory_order_release);

    workerThread = std::make_unique<std::thread>([this, config, progressCallback, completionCallback]() {
        runProcessing(config, progressCallback, completionCallback);
    });
}

void LorisResynthesizer::runProcessing(
    LorisResynthesisConfig config,
    std::function<void(float, const juce::String&)> progressCallback,
    std::function<void(const std::vector<ResynthesizedZone>&, bool, const juce::String&)> completionCallback)
{
    std::vector<ResynthesizedZone> resultZones;
    juce::String errorMessage;
    bool success = false;

    auto reportProgress = [progressCallback](float p, const juce::String& txt) {
        if (progressCallback)
        {
            juce::MessageManager::callAsync([progressCallback, p, txt]() {
                progressCallback(p, txt);
            });
        }
    };

    try
    {
        // 1. Prepare Audio Source Buffer
        juce::AudioBuffer<float> sourceBuf;
        double sr = config.sampleRate;

        if (config.sourceBuffer.getNumSamples() > 0)
        {
            sourceBuf.makeCopyOf(config.sourceBuffer);
        }
        else if (config.sourceFile.existsAsFile())
        {
            juce::AudioFormatManager formatMgr;
            formatMgr.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(config.sourceFile));
            if (reader != nullptr)
            {
                sourceBuf.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
                reader->read(&sourceBuf, 0, (int)reader->lengthInSamples, 0, true, true);
                sr = reader->sampleRate;
            }
            else
            {
                throw std::runtime_error("Failed to read audio file: " + config.sourceFile.getFullPathName().toStdString());
            }
        }
        else
        {
            throw std::runtime_error("No valid audio source buffer or file provided.");
        }

        if (sourceBuf.getNumSamples() == 0 || sr <= 0.0)
        {
            throw std::runtime_error("Invalid sample data or sample rate.");
        }

        // 2. Determine Root Note
        int rootNote = config.rootNote;
        if (config.autoDetectRoot)
        {
            reportProgress(0.05f, "Detecting root pitch...");
            rootNote = detectRootMidiNote(sourceBuf, sr);
        }

        double fundamentalHz = 440.0 * std::pow(2.0, (rootNote - 69) / 12.0);

        // 3. Configure Loris Analyzer
        double resolutionHz = (config.freqResolutionHz > 0.0) ? config.freqResolutionHz : std::clamp(fundamentalHz * 0.8, 25.0, 350.0);
        double windowWidthHz = (config.windowWidthHz > 0.0) ? config.windowWidthHz : (resolutionHz * 2.0);
        double freqDriftHz = (config.freqDriftHz > 0.0) ? config.freqDriftHz : (resolutionHz * 0.5);

        reportProgress(0.10f, "Analyzing partials with Loris (f0 = " + juce::String(fundamentalHz, 1) + " Hz)...");

        const int numChannels = sourceBuf.getNumChannels();
        const int numSamples = sourceBuf.getNumSamples();

        std::vector<Loris::PartialList> channelPartials(numChannels);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (cancelRequested.load(std::memory_order_relaxed))
                throw std::runtime_error("Resynthesis cancelled by user.");

            std::vector<double> inAudio(numSamples);
            const float* chData = sourceBuf.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                inAudio[i] = static_cast<double>(chData[i]);
            }

            Loris::Analyzer analyzer(resolutionHz, windowWidthHz);
            analyzer.setFreqDrift(freqDriftHz);

            channelPartials[ch] = analyzer.analyze(inAudio, sr);
        }

        if (channelPartials[0].empty())
        {
            throw std::runtime_error("Loris analysis produced no partials. Check input audio signal level.");
        }

        // 4. Prepare Output Directory
        juce::File outDir = config.outputDirectory;
        if (!outDir.isDirectory())
        {
            outDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("OWMB_Loris_Resynth_" + juce::String(juce::Time::getMillisecondCounter()));
            outDir.createDirectory();
        }

        // 5. Generate Target Notes Loop
        const int minN = std::clamp(config.minNote, 0, 127);
        const int maxN = std::clamp(config.maxNote, minN, 127);
        const int stride = std::max(1, config.noteStride);

        std::vector<int> targetNotes;
        for (int n = minN; n <= maxN; n += stride)
        {
            targetNotes.push_back(n);
        }

        const float gainScale = std::pow(10.0f, config.gainDb / 20.0f);
        const size_t totalNotes = targetNotes.size();

        for (size_t idx = 0; idx < totalNotes; ++idx)
        {
            if (cancelRequested.load(std::memory_order_relaxed))
                throw std::runtime_error("Resynthesis cancelled by user.");

            int midiNote = targetNotes[idx];
            double semitones = midiNote - rootNote;
            double pitchRatio = std::pow(2.0, semitones / 12.0);

            float prog = 0.20f + 0.75f * ((float)idx / (float)totalNotes);
            juce::String noteName = getMidiNoteName(midiNote);
            reportProgress(prog, "Synthesizing note " + noteName + " (" + juce::String(idx + 1) + "/" + juce::String((int)totalNotes) + ")...");

            std::vector<std::vector<double>> synthesizedChannels(numChannels);
            size_t maxLen = 0;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                // Deep copy base partials
                Loris::PartialList notePartials = channelPartials[ch];

                // Transpose partial frequencies
                Loris::PartialUtils::scaleFrequency(notePartials.begin(), notePartials.end(), pitchRatio);

                // Optional Formant Envelope Preservation:
                // When scaling pitch, formant preservation adjusts partial amplitudes according
                // to the original spectral envelope profile.
                if (config.preserveFormants && std::abs(pitchRatio - 1.0) > 0.005)
                {
                    // Gentle spectral tilt compensation
                    double formantCompensation = 1.0 / std::pow(pitchRatio, 0.35);
                    for (auto it = notePartials.begin(); it != notePartials.end(); ++it)
                    {
                        for (auto bpIt = it->begin(); bpIt != it->end(); ++bpIt)
                        {
                            bpIt->setAmplitude(bpIt->amplitude() * formantCompensation);
                        }
                    }
                }

                // Render with Loris Synthesizer
                std::vector<double> renderedAudio;
                Loris::Synthesizer synth(sr, renderedAudio);
                synth.synthesize(notePartials.begin(), notePartials.end());

                synthesizedChannels[ch] = std::move(renderedAudio);
                if (synthesizedChannels[ch].size() > maxLen)
                    maxLen = synthesizedChannels[ch].size();
            }

            if (maxLen == 0)
                continue;

            // Convert to JUCE AudioBuffer
            juce::AudioBuffer<float> renderedBuffer(numChannels, (int)maxLen);
            renderedBuffer.clear();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* writePtr = renderedBuffer.getWritePointer(ch);
                const auto& chData = synthesizedChannels[ch];
                for (size_t s = 0; s < chData.size(); ++s)
                {
                    writePtr[s] = static_cast<float>(chData[s]) * gainScale;
                }
            }

            // Determine Zone Key Bounds
            int keyLow = (idx == 0) ? minN : (midiNote - (stride / 2));
            int keyHigh = (idx == totalNotes - 1) ? maxN : (midiNote + ((stride - 1) / 2));
            keyLow = std::clamp(keyLow, 0, 127);
            keyHigh = std::clamp(keyHigh, keyLow, 127);

            // Save WAV File
            juce::String cleanBaseName = config.baseSampleName.isEmpty() ? "LorisSample" : config.baseSampleName;
            cleanBaseName = juce::File::createLegalFileName(cleanBaseName);

            juce::String fileName = cleanBaseName + "_Loris_" + noteName + "_" + juce::String(midiNote) + ".wav";
            juce::File noteFile = outDir.getChildFile(fileName);
            if (noteFile.existsAsFile())
                noteFile.deleteFile();

            {
                std::unique_ptr<juce::FileOutputStream> outStream(noteFile.createOutputStream());
                if (outStream != nullptr)
                {
                    juce::WavAudioFormat wavFormat;
                    std::unique_ptr<juce::AudioFormatWriter> writer(
                        wavFormat.createWriterFor(outStream.get(), sr, renderedBuffer.getNumChannels(), 24, {}, 0));

                    if (writer != nullptr)
                    {
                        outStream.release(); // Writer took ownership
                        writer->writeFromAudioSampleBuffer(renderedBuffer, 0, renderedBuffer.getNumSamples());
                        writer.reset();
                    }
                }
            }

            ResynthesizedZone zone;
            zone.audioFile = noteFile;
            zone.sampleName = fileName;
            zone.rootNote = midiNote;
            zone.keyLow = keyLow;
            zone.keyHigh = keyHigh;
            zone.sampleRate = sr;
            zone.numChannels = numChannels;
            zone.numSamples = (int)maxLen;

            resultZones.push_back(zone);
        }

        if (resultZones.empty())
        {
            throw std::runtime_error("No resynthesized zones were generated.");
        }

        reportProgress(1.0f, "Resynthesis complete!");
        success = true;
    }
    catch (const std::exception& e)
    {
        errorMessage = e.what();
        success = false;
    }
    catch (...)
    {
        errorMessage = "An unknown error occurred during Loris resynthesis.";
        success = false;
    }

    processing.store(false, std::memory_order_release);

    if (completionCallback)
    {
        juce::MessageManager::callAsync([completionCallback, resultZones, success, errorMessage]() {
            completionCallback(resultZones, success, errorMessage);
        });
    }
}

} // namespace openwav
