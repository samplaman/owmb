#include "PerformanceRackDSP.h"

namespace openwav
{

// ─────────────────────────────────────────────────────────────────────────────
//  1. Studio Reverb Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackReverb::RackReverb()
{
    loadPreset(0);
}

void RackReverb::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    reverb.setSampleRate(sampleRate);
    dryCopy.setSize(2, maxBlockSize);

    int maxPreDelaySamples = static_cast<int>(sampleRate * 0.25); // up to 250ms
    preDelayLineL.assign(maxPreDelaySamples, 0.0f);
    preDelayLineR.assign(maxPreDelaySamples, 0.0f);
    preDelayWritePos = 0;
}

void RackReverb::reset()
{
    reverb.reset();
    std::fill(preDelayLineL.begin(), preDelayLineL.end(), 0.0f);
    std::fill(preDelayLineR.begin(), preDelayLineR.end(), 0.0f);
    preDelayWritePos = 0;
}

std::vector<EffectParamInfo> RackReverb::getParameterInfos() const
{
    return {
        { "size", "Size", 0.0f, 1.0f, 0.6f, "" },
        { "damp", "Damping", 0.0f, 1.0f, 0.4f, "" },
        { "width", "Width", 0.0f, 1.0f, 1.0f, "" },
        { "predelay", "Pre-Delay", 0.0f, 100.0f, 10.0f, "ms" },
        { "mix", "Mix", 0.0f, 1.0f, 0.35f, "%" }
    };
}

void RackReverb::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "size") roomSize.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
    else if (paramId == "damp") damping.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
    else if (paramId == "width") width.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
    else if (paramId == "predelay") preDelayMs.store(juce::jlimit(0.0f, 100.0f, value), std::memory_order_relaxed);
    else if (paramId == "mix") mix.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
}

float RackReverb::getParameter(const juce::String& paramId) const
{
    if (paramId == "size") return roomSize.load(std::memory_order_relaxed);
    if (paramId == "damp") return damping.load(std::memory_order_relaxed);
    if (paramId == "width") return width.load(std::memory_order_relaxed);
    if (paramId == "predelay") return preDelayMs.load(std::memory_order_relaxed);
    if (paramId == "mix") return mix.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackReverb::getPresetNames() const
{
    return { "Large Hall", "Drum Plate", "Tight Room", "Ambient Space", "Cathedral" };
}

void RackReverb::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0: // Large Hall
            setParameter("size", 0.75f); setParameter("damp", 0.35f); setParameter("width", 1.0f); setParameter("predelay", 20.0f); setParameter("mix", 0.35f);
            break;
        case 1: // Drum Plate
            setParameter("size", 0.45f); setParameter("damp", 0.6f); setParameter("width", 0.9f); setParameter("predelay", 5.0f); setParameter("mix", 0.28f);
            break;
        case 2: // Tight Room
            setParameter("size", 0.25f); setParameter("damp", 0.7f); setParameter("width", 0.7f); setParameter("predelay", 0.0f); setParameter("mix", 0.22f);
            break;
        case 3: // Ambient Space
            setParameter("size", 0.92f); setParameter("damp", 0.2f); setParameter("width", 1.0f); setParameter("predelay", 40.0f); setParameter("mix", 0.55f);
            break;
        case 4: // Cathedral
            setParameter("size", 0.98f); setParameter("damp", 0.15f); setParameter("width", 1.0f); setParameter("predelay", 50.0f); setParameter("mix", 0.45f);
            break;
        default: break;
    }
}

void RackReverb::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0)
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    float curMix = mix.load(std::memory_order_relaxed);

    if (curMix <= 0.0001f)
        return;

    dryCopy.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryCopy.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // Apply pre-delay
    float pdMs = preDelayMs.load(std::memory_order_relaxed);
    int delaySamples = juce::jlimit(0, static_cast<int>(preDelayLineL.size() - 1), static_cast<int>(pdMs * 0.001 * currentSampleRate));

    if (delaySamples > 0 && !preDelayLineL.empty())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            size_t readPos = (preDelayWritePos + preDelayLineL.size() - delaySamples) % preDelayLineL.size();
            float inL = buffer.getSample(0, i);
            float inR = (numChannels > 1) ? buffer.getSample(1, i) : inL;

            preDelayLineL[preDelayWritePos] = inL;
            preDelayLineR[preDelayWritePos] = inR;

            buffer.setSample(0, i, preDelayLineL[readPos]);
            if (numChannels > 1) buffer.setSample(1, i, preDelayLineR[readPos]);

            preDelayWritePos = (preDelayWritePos + 1) % preDelayLineL.size();
        }
    }

    params.roomSize = roomSize.load(std::memory_order_relaxed);
    params.damping = damping.load(std::memory_order_relaxed);
    params.wetLevel = 1.0f;
    params.dryLevel = 0.0f;
    params.width = width.load(std::memory_order_relaxed);
    reverb.setParameters(params);

    if (numChannels >= 2)
        reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
    else
        reverb.processMono(buffer.getWritePointer(0), numSamples);

    // Wet/dry crossfade
    float dryGain = 1.0f - curMix;
    float wetGain = curMix;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* writePtr = buffer.getWritePointer(ch);
        const auto* dryPtr = dryCopy.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            writePtr[i] = dryPtr[i] * dryGain + writePtr[i] * wetGain;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Stereo Tape / Ping-Pong Delay Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackDelay::RackDelay()
{
    loadPreset(0);
}

void RackDelay::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    int maxDelaySamples = static_cast<int>(sampleRate * 2.5); // up to 2.5s
    delayBufferL.assign(maxDelaySamples, 0.0f);
    delayBufferR.assign(maxDelaySamples, 0.0f);
    writePos = 0;
    dampFilterL = 0.0f;
    dampFilterR = 0.0f;
    dryCopy.setSize(2, maxBlockSize);
}

void RackDelay::reset()
{
    std::fill(delayBufferL.begin(), delayBufferL.end(), 0.0f);
    std::fill(delayBufferR.begin(), delayBufferR.end(), 0.0f);
    writePos = 0;
    dampFilterL = 0.0f;
    dampFilterR = 0.0f;
}

std::vector<EffectParamInfo> RackDelay::getParameterInfos() const
{
    return {
        { "time", "Time", 20.0f, 1500.0f, 350.0f, "ms" },
        { "feedback", "Feedback", 0.0f, 0.92f, 0.45f, "%" },
        { "damp", "Damp Cutoff", 500.0f, 16000.0f, 4500.0f, "Hz" },
        { "pingpong", "Ping-Pong", 0.0f, 1.0f, 0.5f, "%" },
        { "mix", "Mix", 0.0f, 1.0f, 0.35f, "%" }
    };
}

void RackDelay::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "time") timeMs.store(juce::jlimit(20.0f, 1500.0f, value), std::memory_order_relaxed);
    else if (paramId == "feedback") feedback.store(juce::jlimit(0.0f, 0.95f, value), std::memory_order_relaxed);
    else if (paramId == "damp") dampFreqHz.store(juce::jlimit(500.0f, 16000.0f, value), std::memory_order_relaxed);
    else if (paramId == "pingpong") pingPong.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
    else if (paramId == "mix") mix.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
}

float RackDelay::getParameter(const juce::String& paramId) const
{
    if (paramId == "time") return timeMs.load(std::memory_order_relaxed);
    if (paramId == "feedback") return feedback.load(std::memory_order_relaxed);
    if (paramId == "damp") return dampFreqHz.load(std::memory_order_relaxed);
    if (paramId == "pingpong") return pingPong.load(std::memory_order_relaxed);
    if (paramId == "mix") return mix.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackDelay::getPresetNames() const
{
    return { "Slapback Echo", "Ping-Pong Stereo", "Dotted Eighth", "Space Dub Echo", "Warm Tape Roll" };
}

void RackDelay::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0: // Slapback Echo
            setParameter("time", 90.0f); setParameter("feedback", 0.15f); setParameter("damp", 5000.0f); setParameter("pingpong", 0.0f); setParameter("mix", 0.35f);
            break;
        case 1: // Ping-Pong Stereo
            setParameter("time", 320.0f); setParameter("feedback", 0.55f); setParameter("damp", 4500.0f); setParameter("pingpong", 1.0f); setParameter("mix", 0.40f);
            break;
        case 2: // Dotted Eighth
            setParameter("time", 250.0f); setParameter("feedback", 0.50f); setParameter("damp", 6000.0f); setParameter("pingpong", 0.6f); setParameter("mix", 0.35f);
            break;
        case 3: // Space Dub Echo
            setParameter("time", 520.0f); setParameter("feedback", 0.78f); setParameter("damp", 2500.0f); setParameter("pingpong", 0.75f); setParameter("mix", 0.45f);
            break;
        case 4: // Warm Tape Roll
            setParameter("time", 400.0f); setParameter("feedback", 0.40f); setParameter("damp", 2000.0f); setParameter("pingpong", 0.2f); setParameter("mix", 0.30f);
            break;
        default: break;
    }
}

void RackDelay::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0 || delayBufferL.empty())
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    float curMix = mix.load(std::memory_order_relaxed);

    if (curMix <= 0.0001f)
        return;

    dryCopy.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryCopy.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    float tMs = timeMs.load(std::memory_order_relaxed);
    float fb = feedback.load(std::memory_order_relaxed);
    float pp = pingPong.load(std::memory_order_relaxed);
    float dFreq = dampFreqHz.load(std::memory_order_relaxed);

    // Single-pole lowpass damping coefficient
    float alpha = std::min(0.95f, 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * dFreq / static_cast<float>(currentSampleRate)));

    size_t bufSize = delayBufferL.size();
    float delaySamplesFloat = juce::jlimit(1.0f, static_cast<float>(bufSize - 2), static_cast<float>(tMs * 0.001 * currentSampleRate));
    size_t delaySamples = static_cast<size_t>(delaySamplesFloat);

    for (int i = 0; i < numSamples; ++i)
    {
        size_t readL = (writePos + bufSize - delaySamples) % bufSize;
        // Ping pong delay introduces offset on right channel
        size_t offsetR = delaySamples + static_cast<size_t>(delaySamples * 0.5f * pp);
        size_t readR = (writePos + bufSize - (offsetR % bufSize)) % bufSize;

        float delayedL = delayBufferL[readL];
        float delayedR = delayBufferR[readR];

        // Damping filter
        dampFilterL += alpha * (delayedL - dampFilterL);
        dampFilterR += alpha * (delayedR - dampFilterR);

        float inL = buffer.getSample(0, i);
        float inR = (numChannels > 1) ? buffer.getSample(1, i) : inL;

        // Feedback with cross-feed if ping-pong is active
        float nextL = inL + (1.0f - pp * 0.5f) * dampFilterL * fb + (pp * 0.5f) * dampFilterR * fb;
        float nextR = inR + (1.0f - pp * 0.5f) * dampFilterR * fb + (pp * 0.5f) * dampFilterL * fb;

        // Soft saturation in feedback path
        nextL = std::tanh(nextL);
        nextR = std::tanh(nextR);

        delayBufferL[writePos] = nextL;
        delayBufferR[writePos] = nextR;

        writePos = (writePos + 1) % bufSize;

        float wetL = delayedL;
        float wetR = delayedR;

        buffer.setSample(0, i, inL * (1.0f - curMix) + wetL * curMix);
        if (numChannels > 1)
            buffer.setSample(1, i, inR * (1.0f - curMix) + wetR * curMix);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Multi-Mode SVF Filter Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackFilter::RackFilter()
{
    loadPreset(0);
}

void RackFilter::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    s1_L = s2_L = s1_R = s2_R = 0.0f;
    dryCopy.setSize(2, maxBlockSize);
}

void RackFilter::reset()
{
    s1_L = s2_L = s1_R = s2_R = 0.0f;
}

std::vector<EffectParamInfo> RackFilter::getParameterInfos() const
{
    return {
        { "cutoff", "Cutoff", 20.0f, 20000.0f, 1800.0f, "Hz" },
        { "res", "Resonance", 0.5f, 9.5f, 1.5f, "" },
        { "mode", "Filter Mode", 0.0f, 3.0f, 0.0f, "", true, { "LowPass", "HighPass", "BandPass", "Notch" } },
        { "drive", "Drive", 1.0f, 4.0f, 1.0f, "x" },
        { "mix", "Mix", 0.0f, 1.0f, 1.0f, "%" }
    };
}

void RackFilter::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "cutoff") cutoffHz.store(juce::jlimit(20.0f, 20000.0f, value), std::memory_order_relaxed);
    else if (paramId == "res") resonance.store(juce::jlimit(0.5f, 10.0f, value), std::memory_order_relaxed);
    else if (paramId == "mode") mode.store(std::round(juce::jlimit(0.0f, 3.0f, value)), std::memory_order_relaxed);
    else if (paramId == "drive") drive.store(juce::jlimit(1.0f, 5.0f, value), std::memory_order_relaxed);
    else if (paramId == "mix") mix.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
}

float RackFilter::getParameter(const juce::String& paramId) const
{
    if (paramId == "cutoff") return cutoffHz.load(std::memory_order_relaxed);
    if (paramId == "res") return resonance.load(std::memory_order_relaxed);
    if (paramId == "mode") return mode.load(std::memory_order_relaxed);
    if (paramId == "drive") return drive.load(std::memory_order_relaxed);
    if (paramId == "mix") return mix.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackFilter::getPresetNames() const
{
    return { "Warm 12dB Lo-Pass", "Acid Resonance Sweep", "High-Pass Clear", "Radio Telephone Bandpass", "Deep Notch Shifter" };
}

void RackFilter::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0: // Warm Lo-Pass
            setParameter("cutoff", 1400.0f); setParameter("res", 1.2f); setParameter("mode", 0.0f); setParameter("drive", 1.1f); setParameter("mix", 1.0f);
            break;
        case 1: // Acid Resonance Sweep
            setParameter("cutoff", 850.0f); setParameter("res", 7.5f); setParameter("mode", 0.0f); setParameter("drive", 2.2f); setParameter("mix", 1.0f);
            break;
        case 2: // High-Pass Clear
            setParameter("cutoff", 350.0f); setParameter("res", 1.0f); setParameter("mode", 1.0f); setParameter("drive", 1.0f); setParameter("mix", 1.0f);
            break;
        case 3: // Radio Telephone Bandpass
            setParameter("cutoff", 1600.0f); setParameter("res", 4.0f); setParameter("mode", 2.0f); setParameter("drive", 1.5f); setParameter("mix", 1.0f);
            break;
        case 4: // Deep Notch Shifter
            setParameter("cutoff", 2400.0f); setParameter("res", 5.0f); setParameter("mode", 3.0f); setParameter("drive", 1.0f); setParameter("mix", 0.85f);
            break;
        default: break;
    }
}

void RackFilter::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0)
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    float curMix = mix.load(std::memory_order_relaxed);

    dryCopy.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryCopy.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    float fCut = cutoffHz.load(std::memory_order_relaxed);
    float q = resonance.load(std::memory_order_relaxed);
    int filterMode = static_cast<int>(mode.load(std::memory_order_relaxed));
    float drv = drive.load(std::memory_order_relaxed);

    // Andrew Simper SVF coefficients
    float g = std::tan(juce::MathConstants<float>::pi * fCut / static_cast<float>(currentSampleRate));
    float k = 1.0f / q;
    float a1 = 1.0f / (1.0f + g * (g + k));
    float a2 = g * a1;
    float a3 = g * a2;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float& s1 = (ch == 0) ? s1_L : s1_R;
            float& s2 = (ch == 0) ? s2_L : s2_R;

            float x = buffer.getSample(ch, i) * drv;
            // Nonlinear drive saturation inside filter loop
            x = std::tanh(x);

            float v3 = x - s2;
            float v1 = a1 * s1 + a2 * v3;
            float v2 = s2 + a2 * s1 + a3 * v3;
            s1 = 2.0f * v1 - s1;
            s2 = 2.0f * v2 - s2;

            float low = v2;
            float band = v1;
            float high = x - k * v1 - v2;
            float notch = low + high;

            float out = low;
            if (filterMode == 1) out = high;
            else if (filterMode == 2) out = band;
            else if (filterMode == 3) out = notch;

            float dry = dryCopy.getSample(ch, i);
            buffer.setSample(ch, i, dry * (1.0f - curMix) + out * curMix);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Analog Saturation & Overdrive Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackDistortion::RackDistortion()
{
    loadPreset(0);
}

void RackDistortion::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    toneFilterL = toneFilterR = 0.0f;
    dryCopy.setSize(2, maxBlockSize);
}

void RackDistortion::reset()
{
    toneFilterL = toneFilterR = 0.0f;
}

std::vector<EffectParamInfo> RackDistortion::getParameterInfos() const
{
    return {
        { "drive", "Drive", 0.0f, 36.0f, 12.0f, "dB" },
        { "tone", "Tone", 0.0f, 1.0f, 0.5f, "" },
        { "type", "Type", 0.0f, 4.0f, 0.0f, "", true, { "Tape", "Tube", "Overdrive", "Fuzz", "Hard Clip" } },
        { "outgain", "Output", -18.0f, 12.0f, 0.0f, "dB" },
        { "mix", "Mix", 0.0f, 1.0f, 0.8f, "%" }
    };
}

void RackDistortion::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "drive") driveDb.store(juce::jlimit(0.0f, 36.0f, value), std::memory_order_relaxed);
    else if (paramId == "tone") tone.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
    else if (paramId == "type") type.store(std::round(juce::jlimit(0.0f, 4.0f, value)), std::memory_order_relaxed);
    else if (paramId == "outgain") outGainDb.store(juce::jlimit(-18.0f, 12.0f, value), std::memory_order_relaxed);
    else if (paramId == "mix") mix.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
}

float RackDistortion::getParameter(const juce::String& paramId) const
{
    if (paramId == "drive") return driveDb.load(std::memory_order_relaxed);
    if (paramId == "tone") return tone.load(std::memory_order_relaxed);
    if (paramId == "type") return type.load(std::memory_order_relaxed);
    if (paramId == "outgain") return outGainDb.load(std::memory_order_relaxed);
    if (paramId == "mix") return mix.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackDistortion::getPresetNames() const
{
    return { "Tape Saturation", "Warm Tube Amp", "Punchy Overdrive", "Heavy Fuzz", "Digital Hard Clip" };
}

void RackDistortion::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0: // Tape Saturation
            setParameter("drive", 8.0f); setParameter("tone", 0.45f); setParameter("type", 0.0f); setParameter("outgain", -1.0f); setParameter("mix", 0.8f);
            break;
        case 1: // Warm Tube Amp
            setParameter("drive", 14.0f); setParameter("tone", 0.55f); setParameter("type", 1.0f); setParameter("outgain", -2.5f); setParameter("mix", 0.85f);
            break;
        case 2: // Punchy Overdrive
            setParameter("drive", 20.0f); setParameter("tone", 0.65f); setParameter("type", 2.0f); setParameter("outgain", -4.0f); setParameter("mix", 0.9f);
            break;
        case 3: // Heavy Fuzz
            setParameter("drive", 28.0f); setParameter("tone", 0.4f); setParameter("type", 3.0f); setParameter("outgain", -6.0f); setParameter("mix", 0.75f);
            break;
        case 4: // Digital Hard Clip
            setParameter("drive", 16.0f); setParameter("tone", 0.7f); setParameter("type", 4.0f); setParameter("outgain", -3.0f); setParameter("mix", 0.65f);
            break;
        default: break;
    }
}

void RackDistortion::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0)
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    float curMix = mix.load(std::memory_order_relaxed);

    dryCopy.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryCopy.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    float drvGain = std::pow(10.0f, driveDb.load(std::memory_order_relaxed) / 20.0f);
    float outGain = std::pow(10.0f, outGainDb.load(std::memory_order_relaxed) / 20.0f);
    int distType = static_cast<int>(type.load(std::memory_order_relaxed));
    float tParam = tone.load(std::memory_order_relaxed);

    // Tone lowpass filter coefficient
    float toneFreq = 1200.0f + tParam * 14000.0f;
    float alpha = std::min(0.95f, 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * toneFreq / static_cast<float>(currentSampleRate)));

    float peakIn = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float& toneFilter = (ch == 0) ? toneFilterL : toneFilterR;
        auto* writePtr = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float in = writePtr[i] * drvGain;
            peakIn = std::max(peakIn, std::abs(in));

            float saturated = in;
            switch (distType)
            {
                case 0: // Tape (smooth hyperbolic tangent with second harmonic bias)
                    saturated = std::tanh(in + 0.15f * in * in) - 0.15f;
                    break;
                case 1: // Tube (asymmetric soft clipping)
                    if (in > 0.0f)
                        saturated = 1.0f - std::exp(-in);
                    else
                        saturated = - (1.0f - std::exp(in * 0.8f));
                    break;
                case 2: // Overdrive (cubic curve)
                    saturated = juce::jlimit(-1.0f, 1.0f, in - (in * in * in) / 3.0f);
                    break;
                case 3: // Fuzz (extreme wavefolding)
                    saturated = std::sin(in * 1.5f);
                    break;
                case 4: // Hard Clip
                default:
                    saturated = juce::jlimit(-0.95f, 0.95f, in);
                    break;
            }

            // Apply tone lowpass filter
            toneFilter += alpha * (saturated - toneFilter);
            float wet = toneFilter * outGain;

            float dry = dryCopy.getSample(ch, i);
            writePtr[i] = dry * (1.0f - curMix) + wet * curMix;
        }
    }

    currentDriveMeter.store(juce::jlimit(0.0f, 1.0f, peakIn / 4.0f), std::memory_order_relaxed);
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Stereo Chorus & Ensemble Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackChorus::RackChorus()
{
    loadPreset(0);
}

void RackChorus::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    int maxDelay = static_cast<int>(sampleRate * 0.06); // 60ms delay line
    delayL.assign(maxDelay, 0.0f);
    delayR.assign(maxDelay, 0.0f);
    writePos = 0;
    lfoPhaseL = 0.0;
    lfoPhaseR = juce::MathConstants<double>::halfPi;
    dryCopy.setSize(2, maxBlockSize);
}

void RackChorus::reset()
{
    std::fill(delayL.begin(), delayL.end(), 0.0f);
    std::fill(delayR.begin(), delayR.end(), 0.0f);
    writePos = 0;
}

std::vector<EffectParamInfo> RackChorus::getParameterInfos() const
{
    return {
        { "rate", "Rate", 0.1f, 8.0f, 1.2f, "Hz" },
        { "depth", "Depth", 0.5f, 10.0f, 4.0f, "ms" },
        { "feedback", "Feedback", 0.0f, 0.7f, 0.25f, "%" },
        { "width", "Stereo Width", 0.0f, 1.0f, 0.85f, "%" },
        { "mix", "Mix", 0.0f, 1.0f, 0.5f, "%" }
    };
}

void RackChorus::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "rate") rateHz.store(juce::jlimit(0.1f, 8.0f, value), std::memory_order_relaxed);
    else if (paramId == "depth") depthMs.store(juce::jlimit(0.5f, 10.0f, value), std::memory_order_relaxed);
    else if (paramId == "feedback") feedback.store(juce::jlimit(0.0f, 0.7f, value), std::memory_order_relaxed);
    else if (paramId == "width") stereoWidth.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
    else if (paramId == "mix") mix.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
}

float RackChorus::getParameter(const juce::String& paramId) const
{
    if (paramId == "rate") return rateHz.load(std::memory_order_relaxed);
    if (paramId == "depth") return depthMs.load(std::memory_order_relaxed);
    if (paramId == "feedback") return feedback.load(std::memory_order_relaxed);
    if (paramId == "width") return stereoWidth.load(std::memory_order_relaxed);
    if (paramId == "mix") return mix.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackChorus::getPresetNames() const
{
    return { "Subtle Shimmer", "Classic 80s Chorus", "Slow Dimension", "Fast Rotary Vibrato", "Deep Flanger" };
}

void RackChorus::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0: // Subtle Shimmer
            setParameter("rate", 0.8f); setParameter("depth", 2.5f); setParameter("feedback", 0.15f); setParameter("width", 0.75f); setParameter("mix", 0.35f);
            break;
        case 1: // Classic 80s Chorus
            setParameter("rate", 1.4f); setParameter("depth", 4.5f); setParameter("feedback", 0.30f); setParameter("width", 1.0f); setParameter("mix", 0.5f);
            break;
        case 2: // Slow Dimension
            setParameter("rate", 0.35f); setParameter("depth", 6.0f); setParameter("feedback", 0.20f); setParameter("width", 1.0f); setParameter("mix", 0.55f);
            break;
        case 3: // Fast Rotary Vibrato
            setParameter("rate", 5.5f); setParameter("depth", 3.0f); setParameter("feedback", 0.05f); setParameter("width", 0.9f); setParameter("mix", 0.65f);
            break;
        case 4: // Deep Flanger
            setParameter("rate", 0.4f); setParameter("depth", 1.8f); setParameter("feedback", 0.65f); setParameter("width", 0.8f); setParameter("mix", 0.5f);
            break;
        default: break;
    }
}

void RackChorus::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0 || delayL.empty())
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    float curMix = mix.load(std::memory_order_relaxed);

    if (curMix <= 0.0001f)
        return;

    dryCopy.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryCopy.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    float rate = rateHz.load(std::memory_order_relaxed);
    float depth = depthMs.load(std::memory_order_relaxed);
    float fb = feedback.load(std::memory_order_relaxed);
    float widthFactor = stereoWidth.load(std::memory_order_relaxed);

    double phaseInc = 2.0 * juce::MathConstants<double>::pi * rate / currentSampleRate;
    size_t bufSize = delayL.size();
    float baseDelaySamples = 0.015f * static_cast<float>(currentSampleRate); // 15ms base delay
    float modDepthSamples = (depth * 0.001f) * static_cast<float>(currentSampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        float modL = static_cast<float>(std::sin(lfoPhaseL));
        float modR = static_cast<float>(std::sin(lfoPhaseL + widthFactor * juce::MathConstants<double>::halfPi));

        float delayLFloat = baseDelaySamples + modDepthSamples * modL;
        float delayRFloat = baseDelaySamples + modDepthSamples * modR;

        // Linear interpolation read
        auto readFromDelay = [&](const std::vector<float>& buf, float dSamples) -> float {
            float readIdx = static_cast<float>(writePos) - dSamples;
            while (readIdx < 0.0f) readIdx += bufSize;
            size_t idx0 = static_cast<size_t>(readIdx) % bufSize;
            size_t idx1 = (idx0 + 1) % bufSize;
            float frac = readIdx - std::floor(readIdx);
            return buf[idx0] + frac * (buf[idx1] - buf[idx0]);
        };

        float wetL = readFromDelay(delayL, delayLFloat);
        float wetR = readFromDelay(delayR, delayRFloat);

        float inL = buffer.getSample(0, i);
        float inR = (numChannels > 1) ? buffer.getSample(1, i) : inL;

        delayL[writePos] = inL + wetL * fb;
        delayR[writePos] = inR + wetR * fb;

        writePos = (writePos + 1) % bufSize;

        lfoPhaseL = std::fmod(lfoPhaseL + phaseInc, 2.0 * juce::MathConstants<double>::pi);

        buffer.setSample(0, i, inL * (1.0f - curMix) + wetL * curMix);
        if (numChannels > 1)
            buffer.setSample(1, i, inR * (1.0f - curMix) + wetR * curMix);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. VCA Studio Compressor Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackCompressor::RackCompressor()
{
    loadPreset(0);
}

void RackCompressor::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    envDb = -96.0f;
    currentGainReductionDb.store(0.0f, std::memory_order_relaxed);
    dryCopy.setSize(2, maxBlockSize);
}

void RackCompressor::reset()
{
    envDb = -96.0f;
    currentGainReductionDb.store(0.0f, std::memory_order_relaxed);
}

std::vector<EffectParamInfo> RackCompressor::getParameterInfos() const
{
    return {
        { "thresh", "Threshold", -40.0f, 0.0f, -14.0f, "dB" },
        { "ratio", "Ratio", 1.0f, 20.0f, 4.0f, ":1" },
        { "attack", "Attack", 0.5f, 80.0f, 12.0f, "ms" },
        { "release", "Release", 10.0f, 800.0f, 120.0f, "ms" },
        { "makeup", "Makeup", 0.0f, 24.0f, 4.0f, "dB" },
        { "mix", "Mix", 0.0f, 1.0f, 1.0f, "%" }
    };
}

void RackCompressor::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "thresh") thresholdDb.store(juce::jlimit(-40.0f, 0.0f, value), std::memory_order_relaxed);
    else if (paramId == "ratio") ratio.store(juce::jlimit(1.0f, 20.0f, value), std::memory_order_relaxed);
    else if (paramId == "attack") attackMs.store(juce::jlimit(0.5f, 100.0f, value), std::memory_order_relaxed);
    else if (paramId == "release") releaseMs.store(juce::jlimit(10.0f, 1000.0f, value), std::memory_order_relaxed);
    else if (paramId == "makeup") makeupDb.store(juce::jlimit(0.0f, 24.0f, value), std::memory_order_relaxed);
    else if (paramId == "mix") mix.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
}

float RackCompressor::getParameter(const juce::String& paramId) const
{
    if (paramId == "thresh") return thresholdDb.load(std::memory_order_relaxed);
    if (paramId == "ratio") return ratio.load(std::memory_order_relaxed);
    if (paramId == "attack") return attackMs.load(std::memory_order_relaxed);
    if (paramId == "release") return releaseMs.load(std::memory_order_relaxed);
    if (paramId == "makeup") return makeupDb.load(std::memory_order_relaxed);
    if (paramId == "mix") return mix.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackCompressor::getPresetNames() const
{
    return { "Punchy Drums", "Smooth Vocal Leveler", "Bus Glue", "Fast Brickwall Limiter", "Heavy Smash & Crush" };
}

void RackCompressor::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0: // Punchy Drums
            setParameter("thresh", -16.0f); setParameter("ratio", 4.0f); setParameter("attack", 25.0f); setParameter("release", 80.0f); setParameter("makeup", 4.5f); setParameter("mix", 1.0f);
            break;
        case 1: // Smooth Vocal Leveler
            setParameter("thresh", -20.0f); setParameter("ratio", 2.5f); setParameter("attack", 15.0f); setParameter("release", 180.0f); setParameter("makeup", 5.0f); setParameter("mix", 1.0f);
            break;
        case 2: // Bus Glue
            setParameter("thresh", -12.0f); setParameter("ratio", 2.0f); setParameter("attack", 30.0f); setParameter("release", 100.0f); setParameter("makeup", 2.0f); setParameter("mix", 1.0f);
            break;
        case 3: // Fast Limiter
            setParameter("thresh", -4.0f); setParameter("ratio", 16.0f); setParameter("attack", 0.5f); setParameter("release", 40.0f); setParameter("makeup", 2.5f); setParameter("mix", 1.0f);
            break;
        case 4: // Heavy Smash & Crush
            setParameter("thresh", -26.0f); setParameter("ratio", 12.0f); setParameter("attack", 3.0f); setParameter("release", 60.0f); setParameter("makeup", 10.0f); setParameter("mix", 0.8f);
            break;
        default: break;
    }
}

void RackCompressor::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0)
    {
        currentGainReductionDb.store(0.0f, std::memory_order_relaxed);
        return;
    }

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    float curMix = mix.load(std::memory_order_relaxed);

    dryCopy.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryCopy.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    float thresh = thresholdDb.load(std::memory_order_relaxed);
    float rat = ratio.load(std::memory_order_relaxed);
    float attMs = attackMs.load(std::memory_order_relaxed);
    float relMs = releaseMs.load(std::memory_order_relaxed);
    float makeGain = std::pow(10.0f, makeupDb.load(std::memory_order_relaxed) / 20.0f);

    float alphaAttack = std::exp(-1.0f / (attMs * 0.001f * static_cast<float>(currentSampleRate)));
    float alphaRelease = std::exp(-1.0f / (relMs * 0.001f * static_cast<float>(currentSampleRate)));

    float maxGr = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float levelL = std::abs(buffer.getSample(0, i));
        float levelR = (numChannels > 1) ? std::abs(buffer.getSample(1, i)) : levelL;
        float peak = std::max(levelL, levelR);

        float peakDb = (peak > 1e-5f) ? (20.0f * std::log10(peak)) : -100.0f;

        // Envelope detection
        if (peakDb > envDb)
            envDb = alphaAttack * envDb + (1.0f - alphaAttack) * peakDb;
        else
            envDb = alphaRelease * envDb + (1.0f - alphaRelease) * peakDb;

        // Gain computer (soft knee)
        float grDb = 0.0f;
        if (envDb > thresh)
        {
            grDb = (thresh - envDb) * (1.0f - 1.0f / rat);
        }

        maxGr = std::min(maxGr, grDb);
        float compGain = std::pow(10.0f, grDb / 20.0f) * makeGain;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float dry = dryCopy.getSample(ch, i);
            float wet = dry * compGain;
            buffer.setSample(ch, i, dry * (1.0f - curMix) + wet * curMix);
        }
    }

    currentGainReductionDb.store(-maxGr, std::memory_order_relaxed);
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. Vintage Bitcrusher & Lo-Fi Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackBitcrusher::RackBitcrusher()
{
    loadPreset(0);
}

void RackBitcrusher::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    holdL = holdR = 0.0f;
    sampleCounter = 0.0f;
    postFilterStateL = postFilterStateR = 0.0f;
    dryCopy.setSize(2, maxBlockSize);
}

void RackBitcrusher::reset()
{
    holdL = holdR = 0.0f;
    sampleCounter = 0.0f;
    postFilterStateL = postFilterStateR = 0.0f;
}

std::vector<EffectParamInfo> RackBitcrusher::getParameterInfos() const
{
    return {
        { "bits", "Bit Depth", 3.0f, 16.0f, 10.0f, "bit", true },
        { "downsample", "Downsample", 1.0f, 24.0f, 3.0f, "x", true },
        { "noise", "Vinyl Noise", 0.0f, 0.3f, 0.05f, "%" },
        { "filter", "Post Filter", 500.0f, 18000.0f, 6000.0f, "Hz" },
        { "mix", "Mix", 0.0f, 1.0f, 0.75f, "%" }
    };
}

void RackBitcrusher::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "bits") bitDepth.store(juce::jlimit(2.0f, 16.0f, value), std::memory_order_relaxed);
    else if (paramId == "downsample") downsample.store(juce::jlimit(1.0f, 32.0f, value), std::memory_order_relaxed);
    else if (paramId == "noise") noiseAmount.store(juce::jlimit(0.0f, 0.5f, value), std::memory_order_relaxed);
    else if (paramId == "filter") postFilterHz.store(juce::jlimit(400.0f, 20000.0f, value), std::memory_order_relaxed);
    else if (paramId == "mix") mix.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
}

float RackBitcrusher::getParameter(const juce::String& paramId) const
{
    if (paramId == "bits") return bitDepth.load(std::memory_order_relaxed);
    if (paramId == "downsample") return downsample.load(std::memory_order_relaxed);
    if (paramId == "noise") return noiseAmount.load(std::memory_order_relaxed);
    if (paramId == "filter") return postFilterHz.load(std::memory_order_relaxed);
    if (paramId == "mix") return mix.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackBitcrusher::getPresetNames() const
{
    return { "12-Bit Classic Sampler", "8-Bit Chiptune Arcade", "Downsampled Lo-Fi Chill", "Digital Crunch", "Vinyl Hiss & Grit" };
}

void RackBitcrusher::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0: // 12-Bit Classic Sampler
            setParameter("bits", 12.0f); setParameter("downsample", 2.0f); setParameter("noise", 0.02f); setParameter("filter", 9000.0f); setParameter("mix", 0.85f);
            break;
        case 1: // 8-Bit Chiptune Arcade
            setParameter("bits", 8.0f); setParameter("downsample", 6.0f); setParameter("noise", 0.04f); setParameter("filter", 4500.0f); setParameter("mix", 1.0f);
            break;
        case 2: // Downsampled Lo-Fi Chill
            setParameter("bits", 10.0f); setParameter("downsample", 4.0f); setParameter("noise", 0.08f); setParameter("filter", 3800.0f); setParameter("mix", 0.7f);
            break;
        case 3: // Digital Crunch
            setParameter("bits", 5.0f); setParameter("downsample", 8.0f); setParameter("noise", 0.12f); setParameter("filter", 7500.0f); setParameter("mix", 0.65f);
            break;
        case 4: // Vinyl Hiss & Grit
            setParameter("bits", 14.0f); setParameter("downsample", 1.0f); setParameter("noise", 0.18f); setParameter("filter", 5500.0f); setParameter("mix", 0.6f);
            break;
        default: break;
    }
}

void RackBitcrusher::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0)
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    float curMix = mix.load(std::memory_order_relaxed);

    dryCopy.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryCopy.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    float bits = bitDepth.load(std::memory_order_relaxed);
    float ds = downsample.load(std::memory_order_relaxed);
    float noise = noiseAmount.load(std::memory_order_relaxed);
    float filterHz = postFilterHz.load(std::memory_order_relaxed);

    float numLevels = std::pow(2.0f, bits);
    float invLevels = 1.0f / numLevels;
    float alpha = std::min(0.95f, 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * filterHz / static_cast<float>(currentSampleRate)));

    for (int i = 0; i < numSamples; ++i)
    {
        sampleCounter += 1.0f;
        if (sampleCounter >= ds)
        {
            sampleCounter = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float in = buffer.getSample(ch, i);
                // Quantize to bit depth
                float quantized = std::floor(in * numLevels + 0.5f) * invLevels;
                if (ch == 0) holdL = quantized;
                else holdR = quantized;
            }
        }

        // Add subtle noise
        rngState = rngState * 1664525u + 1013904223u;
        float randomNoise = (static_cast<float>(rngState & 0xFFFF) / 32768.0f - 1.0f) * noise * 0.2f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float crushed = ((ch == 0) ? holdL : holdR) + randomNoise;
            float& fState = (ch == 0) ? postFilterStateL : postFilterStateR;
            fState += alpha * (crushed - fState);

            float dry = dryCopy.getSample(ch, i);
            buffer.setSample(ch, i, dry * (1.0f - curMix) + fState * curMix);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  8. 3-Band Parametric EQ Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackParametricEQ::RackParametricEQ()
{
    loadPreset(0);
}

void RackParametricEQ::prepare(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate;
    reset();
}

void RackParametricEQ::reset()
{
    lowStateL = lowStateR = { 0, 0, 0, 0 };
    midStateL = midStateR = { 0, 0, 0, 0 };
    highStateL = highStateR = { 0, 0, 0, 0 };
}

std::vector<EffectParamInfo> RackParametricEQ::getParameterInfos() const
{
    return {
        { "lowgain", "Low Gain", -18.0f, 18.0f, 0.0f, "dB" },
        { "lowfreq", "Low Freq", 40.0f, 400.0f, 120.0f, "Hz" },
        { "midgain", "Mid Gain", -18.0f, 18.0f, 0.0f, "dB" },
        { "midfreq", "Mid Freq", 200.0f, 8000.0f, 1200.0f, "Hz" },
        { "midq", "Mid Q", 0.5f, 5.0f, 1.2f, "" },
        { "highgain", "High Gain", -18.0f, 18.0f, 0.0f, "dB" },
        { "highfreq", "High Freq", 2000.0f, 16000.0f, 6500.0f, "Hz" }
    };
}

void RackParametricEQ::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "lowgain") lowGainDb.store(juce::jlimit(-18.0f, 18.0f, value), std::memory_order_relaxed);
    else if (paramId == "lowfreq") lowFreqHz.store(juce::jlimit(40.0f, 400.0f, value), std::memory_order_relaxed);
    else if (paramId == "midgain") midGainDb.store(juce::jlimit(-18.0f, 18.0f, value), std::memory_order_relaxed);
    else if (paramId == "midfreq") midFreqHz.store(juce::jlimit(200.0f, 8000.0f, value), std::memory_order_relaxed);
    else if (paramId == "midq") midQ.store(juce::jlimit(0.5f, 5.0f, value), std::memory_order_relaxed);
    else if (paramId == "highgain") highGainDb.store(juce::jlimit(-18.0f, 18.0f, value), std::memory_order_relaxed);
    else if (paramId == "highfreq") highFreqHz.store(juce::jlimit(2000.0f, 16000.0f, value), std::memory_order_relaxed);
}

float RackParametricEQ::getParameter(const juce::String& paramId) const
{
    if (paramId == "lowgain") return lowGainDb.load(std::memory_order_relaxed);
    if (paramId == "lowfreq") return lowFreqHz.load(std::memory_order_relaxed);
    if (paramId == "midgain") return midGainDb.load(std::memory_order_relaxed);
    if (paramId == "midfreq") return midFreqHz.load(std::memory_order_relaxed);
    if (paramId == "midq") return midQ.load(std::memory_order_relaxed);
    if (paramId == "highgain") return highGainDb.load(std::memory_order_relaxed);
    if (paramId == "highfreq") return highFreqHz.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackParametricEQ::getPresetNames() const
{
    return { "Flat", "Loudness Smile Curve", "Mid Scoop & Punch", "Vocal Air & Presence", "Deep Bass Booster" };
}

void RackParametricEQ::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0: // Flat
            setParameter("lowgain", 0.0f); setParameter("midgain", 0.0f); setParameter("highgain", 0.0f);
            break;
        case 1: // Loudness Smile Curve
            setParameter("lowgain", 4.5f); setParameter("lowfreq", 100.0f); setParameter("midgain", -2.5f); setParameter("midfreq", 1000.0f); setParameter("midq", 1.0f); setParameter("highgain", 5.0f); setParameter("highfreq", 8000.0f);
            break;
        case 2: // Mid Scoop & Punch
            setParameter("lowgain", 3.0f); setParameter("lowfreq", 80.0f); setParameter("midgain", -5.0f); setParameter("midfreq", 500.0f); setParameter("midq", 1.8f); setParameter("highgain", 2.0f); setParameter("highfreq", 7000.0f);
            break;
        case 3: // Vocal Air & Presence
            setParameter("lowgain", -2.0f); setParameter("lowfreq", 120.0f); setParameter("midgain", 3.5f); setParameter("midfreq", 3000.0f); setParameter("midq", 1.4f); setParameter("highgain", 6.0f); setParameter("highfreq", 11000.0f);
            break;
        case 4: // Deep Bass Booster
            setParameter("lowgain", 7.0f); setParameter("lowfreq", 75.0f); setParameter("midgain", -1.0f); setParameter("midfreq", 800.0f); setParameter("midq", 1.0f); setParameter("highgain", 0.0f); setParameter("highfreq", 6500.0f);
            break;
        default: break;
    }
}

void RackParametricEQ::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0)
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    float lGain = lowGainDb.load(std::memory_order_relaxed);
    float lFreq = lowFreqHz.load(std::memory_order_relaxed);
    float mGain = midGainDb.load(std::memory_order_relaxed);
    float mFreq = midFreqHz.load(std::memory_order_relaxed);
    float mQ = midQ.load(std::memory_order_relaxed);
    float hGain = highGainDb.load(std::memory_order_relaxed);
    float hFreq = highFreqHz.load(std::memory_order_relaxed);

    if (std::abs(lGain) < 0.01f && std::abs(mGain) < 0.01f && std::abs(hGain) < 0.01f)
        return;

    struct Coeffs { float b0, b1, b2, a1, a2; };

    auto calcLowShelf = [fs = currentSampleRate](float f0, float gainDb) -> Coeffs {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = 2.0f * juce::MathConstants<float>::pi * f0 / static_cast<float>(fs);
        float cosW = std::cos(w0), sinW = std::sin(w0);
        float beta = std::sqrt(A + A);
        float a0 = (A + 1.0f) + (A - 1.0f) * cosW + beta * sinW;
        return {
            (A * ((A + 1.0f) - (A - 1.0f) * cosW + beta * sinW)) / a0,
            (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW)) / a0,
            (A * ((A + 1.0f) - (A - 1.0f) * cosW - beta * sinW)) / a0,
            (-2.0f * ((A - 1.0f) + (A + 1.0f) * cosW)) / a0,
            ((A + 1.0f) + (A - 1.0f) * cosW - beta * sinW) / a0
        };
    };

    auto calcPeaking = [fs = currentSampleRate](float f0, float gainDb, float Q) -> Coeffs {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = 2.0f * juce::MathConstants<float>::pi * f0 / static_cast<float>(fs);
        float cosW = std::cos(w0), sinW = std::sin(w0);
        float alpha = sinW / (2.0f * Q);
        float a0 = 1.0f + alpha / A;
        return {
            (1.0f + alpha * A) / a0,
            (-2.0f * cosW) / a0,
            (1.0f - alpha * A) / a0,
            (-2.0f * cosW) / a0,
            (1.0f - alpha / A) / a0
        };
    };

    auto calcHighShelf = [fs = currentSampleRate](float f0, float gainDb) -> Coeffs {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = 2.0f * juce::MathConstants<float>::pi * f0 / static_cast<float>(fs);
        float cosW = std::cos(w0), sinW = std::sin(w0);
        float beta = std::sqrt(A + A);
        float a0 = (A + 1.0f) - (A - 1.0f) * cosW + beta * sinW;
        return {
            (A * ((A + 1.0f) + (A - 1.0f) * cosW + beta * sinW)) / a0,
            (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW)) / a0,
            (A * ((A + 1.0f) + (A - 1.0f) * cosW - beta * sinW)) / a0,
            (2.0f * ((A - 1.0f) - (A + 1.0f) * cosW)) / a0,
            ((A + 1.0f) - (A - 1.0f) * cosW - beta * sinW) / a0
        };
    };

    Coeffs cLow = calcLowShelf(lFreq, lGain);
    Coeffs cMid = calcPeaking(mFreq, mGain, mQ);
    Coeffs cHigh = calcHighShelf(hFreq, hGain);

    auto applyFilter = [](float in, FilterState& s, const Coeffs& c) -> float {
        float out = c.b0 * in + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
        s.x2 = s.x1; s.x1 = in;
        s.y2 = s.y1; s.y1 = out;
        return out;
    };

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* writePtr = buffer.getWritePointer(ch);
        FilterState& sL = (ch == 0) ? lowStateL : lowStateR;
        FilterState& sM = (ch == 0) ? midStateL : midStateR;
        FilterState& sH = (ch == 0) ? highStateL : highStateR;

        for (int i = 0; i < numSamples; ++i)
        {
            float s = writePtr[i];
            if (std::abs(lGain) >= 0.01f) s = applyFilter(s, sL, cLow);
            if (std::abs(mGain) >= 0.01f) s = applyFilter(s, sM, cMid);
            if (std::abs(hGain) >= 0.01f) s = applyFilter(s, sH, cHigh);
            writePtr[i] = s;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  9. Pitch Shifter & Harmonizer Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackPitchShifter::RackPitchShifter()
{
    loadPreset(0);
}

void RackPitchShifter::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    grainSizeSamples = static_cast<float>(sampleRate * 0.045); // 45ms grain
    int maxDelay = static_cast<int>(sampleRate * 0.25);
    delayBufferL.assign(maxDelay, 0.0f);
    delayBufferR.assign(maxDelay, 0.0f);
    writePos = 0;
    readHead1 = 0.0;
    readHead2 = grainSizeSamples * 0.5;
    dryCopy.setSize(2, maxBlockSize);
}

void RackPitchShifter::reset()
{
    std::fill(delayBufferL.begin(), delayBufferL.end(), 0.0f);
    std::fill(delayBufferR.begin(), delayBufferR.end(), 0.0f);
    writePos = 0;
    readHead1 = 0.0;
    readHead2 = grainSizeSamples * 0.5;
}

std::vector<EffectParamInfo> RackPitchShifter::getParameterInfos() const
{
    return {
        { "pitch", "Pitch Shift", -12.0f, 12.0f, 0.0f, "st", true },
        { "fine", "Fine Tune", -50.0f, 50.0f, 0.0f, "ct" },
        { "mix", "Mix", 0.0f, 1.0f, 0.5f, "%" }
    };
}

void RackPitchShifter::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "pitch") semitones.store(juce::jlimit(-12.0f, 12.0f, value), std::memory_order_relaxed);
    else if (paramId == "fine") fineTuneCents.store(juce::jlimit(-50.0f, 50.0f, value), std::memory_order_relaxed);
    else if (paramId == "mix") mix.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
}

float RackPitchShifter::getParameter(const juce::String& paramId) const
{
    if (paramId == "pitch") return semitones.load(std::memory_order_relaxed);
    if (paramId == "fine") return fineTuneCents.load(std::memory_order_relaxed);
    if (paramId == "mix") return mix.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackPitchShifter::getPresetNames() const
{
    return { "Sub Octave (-12st)", "Upper Octave (+12st)", "Fifth Up (+7st)", "Detuned Doubler (+9ct)", "Fifth Down (-5st)" };
}

void RackPitchShifter::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0: // Sub Octave
            setParameter("pitch", -12.0f); setParameter("fine", 0.0f); setParameter("mix", 0.5f);
            break;
        case 1: // Upper Octave
            setParameter("pitch", 12.0f); setParameter("fine", 0.0f); setParameter("mix", 0.45f);
            break;
        case 2: // Fifth Up
            setParameter("pitch", 7.0f); setParameter("fine", 0.0f); setParameter("mix", 0.40f);
            break;
        case 3: // Detuned Doubler
            setParameter("pitch", 0.0f); setParameter("fine", 9.0f); setParameter("mix", 0.5f);
            break;
        case 4: // Fifth Down
            setParameter("pitch", -5.0f); setParameter("fine", 0.0f); setParameter("mix", 0.40f);
            break;
        default: break;
    }
}

void RackPitchShifter::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0 || delayBufferL.empty())
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    float curMix = mix.load(std::memory_order_relaxed);

    dryCopy.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryCopy.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    float semis = semitones.load(std::memory_order_relaxed);
    float fine = fineTuneCents.load(std::memory_order_relaxed);

    if (std::abs(semis) < 0.01f && std::abs(fine) < 0.01f)
        return;

    float pitchRatio = std::pow(2.0f, (semis + fine / 100.0f) / 12.0f);
    double rate = 1.0 - pitchRatio; // read head advance relative to write pos

    size_t bufSize = delayBufferL.size();
    float gSize = grainSizeSamples;

    for (int i = 0; i < numSamples; ++i)
    {
        delayBufferL[writePos] = buffer.getSample(0, i);
        if (numChannels > 1) delayBufferR[writePos] = buffer.getSample(1, i);

        readHead1 += rate;
        readHead2 += rate;

        while (readHead1 >= gSize) readHead1 -= gSize;
        while (readHead1 < 0.0) readHead1 += gSize;
        while (readHead2 >= gSize) readHead2 -= gSize;
        while (readHead2 < 0.0) readHead2 += gSize;

        // Triangular crossfade windows
        float win1 = 1.0f - std::abs((2.0f * static_cast<float>(readHead1) / gSize) - 1.0f);
        float win2 = 1.0f - std::abs((2.0f * static_cast<float>(readHead2) / gSize) - 1.0f);

        auto readGrain = [&](const std::vector<float>& buf, double head) -> float {
            float rPos = static_cast<float>(writePos) - static_cast<float>(head);
            while (rPos < 0.0f) rPos += bufSize;
            size_t idx0 = static_cast<size_t>(rPos) % bufSize;
            size_t idx1 = (idx0 + 1) % bufSize;
            float frac = rPos - std::floor(rPos);
            return buf[idx0] + frac * (buf[idx1] - buf[idx0]);
        };

        float wetL = readGrain(delayBufferL, readHead1) * win1 + readGrain(delayBufferL, readHead2) * win2;
        float wetR = readGrain(delayBufferR, readHead1) * win1 + readGrain(delayBufferR, readHead2) * win2;

        writePos = (writePos + 1) % bufSize;

        float dryL = dryCopy.getSample(0, i);
        buffer.setSample(0, i, dryL * (1.0f - curMix) + wetL * curMix);
        if (numChannels > 1)
        {
            float dryR = dryCopy.getSample(1, i);
            buffer.setSample(1, i, dryR * (1.0f - curMix) + wetR * curMix);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  10. Tremolo & Auto-Panner Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackTremolo::RackTremolo()
{
    loadPreset(0);
}

void RackTremolo::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    phase = 0.0;
    dryCopy.setSize(2, maxBlockSize);
}

void RackTremolo::reset()
{
    phase = 0.0;
}

std::vector<EffectParamInfo> RackTremolo::getParameterInfos() const
{
    return {
        { "rate", "Rate", 0.2f, 20.0f, 4.0f, "Hz" },
        { "depth", "Depth", 0.0f, 1.0f, 0.65f, "%" },
        { "shape", "LFO Waveform", 0.0f, 2.0f, 0.0f, "", true, { "Sine", "Triangle", "Square" } },
        { "phase", "Stereo Phase", 0.0f, 180.0f, 90.0f, "deg" },
        { "mix", "Mix", 0.0f, 1.0f, 1.0f, "%" }
    };
}

void RackTremolo::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "rate") rateHz.store(juce::jlimit(0.2f, 20.0f, value), std::memory_order_relaxed);
    else if (paramId == "depth") depth.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
    else if (paramId == "shape") shape.store(std::round(juce::jlimit(0.0f, 2.0f, value)), std::memory_order_relaxed);
    else if (paramId == "phase") stereoPhaseDeg.store(juce::jlimit(0.0f, 180.0f, value), std::memory_order_relaxed);
    else if (paramId == "mix") mix.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
}

float RackTremolo::getParameter(const juce::String& paramId) const
{
    if (paramId == "rate") return rateHz.load(std::memory_order_relaxed);
    if (paramId == "depth") return depth.load(std::memory_order_relaxed);
    if (paramId == "shape") return shape.load(std::memory_order_relaxed);
    if (paramId == "phase") return stereoPhaseDeg.load(std::memory_order_relaxed);
    if (paramId == "mix") return mix.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackTremolo::getPresetNames() const
{
    return { "Vintage Tube Tremolo", "Stereo Auto-Pan", "Helicopter Square Chop", "Slow Ambient Drift", "Fast Rotary Flutter" };
}

void RackTremolo::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0: // Vintage Tube Tremolo
            setParameter("rate", 4.2f); setParameter("depth", 0.65f); setParameter("shape", 0.0f); setParameter("phase", 0.0f); setParameter("mix", 1.0f);
            break;
        case 1: // Stereo Auto-Pan
            setParameter("rate", 1.8f); setParameter("depth", 0.85f); setParameter("shape", 0.0f); setParameter("phase", 180.0f); setParameter("mix", 1.0f);
            break;
        case 2: // Helicopter Square Chop
            setParameter("rate", 8.0f); setParameter("depth", 0.95f); setParameter("shape", 2.0f); setParameter("phase", 0.0f); setParameter("mix", 1.0f);
            break;
        case 3: // Slow Ambient Drift
            setParameter("rate", 0.45f); setParameter("depth", 0.50f); setParameter("shape", 1.0f); setParameter("phase", 120.0f); setParameter("mix", 0.9f);
            break;
        case 4: // Fast Rotary Flutter
            setParameter("rate", 12.0f); setParameter("depth", 0.40f); setParameter("shape", 0.0f); setParameter("phase", 90.0f); setParameter("mix", 0.75f);
            break;
        default: break;
    }
}

void RackTremolo::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0)
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    float curMix = mix.load(std::memory_order_relaxed);

    dryCopy.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryCopy.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    float rHz = rateHz.load(std::memory_order_relaxed);
    float dep = depth.load(std::memory_order_relaxed);
    int waveShape = static_cast<int>(shape.load(std::memory_order_relaxed));
    float sPhase = stereoPhaseDeg.load(std::memory_order_relaxed) * (juce::MathConstants<float>::pi / 180.0f);

    double phaseInc = 2.0 * juce::MathConstants<double>::pi * rHz / currentSampleRate;

    auto evalLfo = [&](double p) -> float {
        float normP = static_cast<float>(std::fmod(p / (2.0 * juce::MathConstants<double>::pi), 1.0));
        if (normP < 0.0f) normP += 1.0f;

        if (waveShape == 1) // Triangle
            return (normP < 0.5f) ? (4.0f * normP - 1.0f) : (3.0f - 4.0f * normP);
        if (waveShape == 2) // Square
            return (normP < 0.5f) ? 1.0f : -1.0f;
        return static_cast<float>(std::sin(p)); // Sine
    };

    for (int i = 0; i < numSamples; ++i)
    {
        float lfoL = evalLfo(phase);
        float lfoR = evalLfo(phase + sPhase);

        float gainL = 1.0f - dep * 0.5f * (1.0f - lfoL);
        float gainR = 1.0f - dep * 0.5f * (1.0f - lfoR);

        phase = std::fmod(phase + phaseInc, 2.0 * juce::MathConstants<double>::pi);

        float dryL = dryCopy.getSample(0, i);
        buffer.setSample(0, i, dryL * (1.0f - curMix) + dryL * gainL * curMix);

        if (numChannels > 1)
        {
            float dryR = dryCopy.getSample(1, i);
            buffer.setSample(1, i, dryR * (1.0f - curMix) + dryR * gainR * curMix);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  11. Stereo Analog Phaser Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackPhaser::RackPhaser()
{
    reset();
}

void RackPhaser::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    dryCopy.setSize(2, maxBlockSize, false, true, false);
    reset();
}

void RackPhaser::reset()
{
    for (auto& ch : allpassStates)
        ch.fill(0.0f);
    feedbackBuffer.fill(0.0f);
    lfoPhase = 0.0;
}

std::vector<EffectParamInfo> RackPhaser::getParameterInfos() const
{
    return {
        { "rate", "Rate", 0.05f, 8.0f, 0.5f, "Hz", false, {} },
        { "depth", "Depth", 0.0f, 1.0f, 0.75f, "%", false, {} },
        { "feedback", "Feedback", -0.95f, 0.95f, 0.5f, "%", false, {} },
        { "poles", "Stages", 0.0f, 2.0f, 1.0f, "", true, { "4-Stage", "8-Stage", "12-Stage" } },
        { "spread", "Stereo", 0.0f, 180.0f, 90.0f, "°", false, {} },
        { "mix", "Mix", 0.0f, 1.0f, 0.5f, "%", false, {} }
    };
}

void RackPhaser::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "rate") rateHz.store(value, std::memory_order_relaxed);
    else if (paramId == "depth") depth.store(value, std::memory_order_relaxed);
    else if (paramId == "feedback") feedback.store(value, std::memory_order_relaxed);
    else if (paramId == "poles") poles.store(value, std::memory_order_relaxed);
    else if (paramId == "spread") stereoPhaseDeg.store(value, std::memory_order_relaxed);
    else if (paramId == "mix") mix.store(value, std::memory_order_relaxed);
}

float RackPhaser::getParameter(const juce::String& paramId) const
{
    if (paramId == "rate") return rateHz.load(std::memory_order_relaxed);
    if (paramId == "depth") return depth.load(std::memory_order_relaxed);
    if (paramId == "feedback") return feedback.load(std::memory_order_relaxed);
    if (paramId == "poles") return poles.load(std::memory_order_relaxed);
    if (paramId == "spread") return stereoPhaseDeg.load(std::memory_order_relaxed);
    if (paramId == "mix") return mix.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackPhaser::getPresetNames() const
{
    return {
        "Subtle 4-Stage Drift",
        "Deep 8-Stage Sweep",
        "Vocal Liquid Jet",
        "Barber-Pole Whirlwind",
        "Vintage 70s Stomp",
        "Slow Cosmic Phase"
    };
}

void RackPhaser::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0:
            setParameter("rate", 0.3f); setParameter("depth", 0.5f); setParameter("feedback", 0.3f); setParameter("poles", 0.0f); setParameter("spread", 45.0f); setParameter("mix", 0.5f);
            break;
        case 1:
            setParameter("rate", 0.6f); setParameter("depth", 0.85f); setParameter("feedback", 0.7f); setParameter("poles", 1.0f); setParameter("spread", 90.0f); setParameter("mix", 0.5f);
            break;
        case 2:
            setParameter("rate", 1.2f); setParameter("depth", 0.9f); setParameter("feedback", -0.75f); setParameter("poles", 2.0f); setParameter("spread", 120.0f); setParameter("mix", 0.6f);
            break;
        case 3:
            setParameter("rate", 3.5f); setParameter("depth", 0.8f); setParameter("feedback", 0.8f); setParameter("poles", 1.0f); setParameter("spread", 180.0f); setParameter("mix", 0.5f);
            break;
        case 4:
            setParameter("rate", 0.4f); setParameter("depth", 0.7f); setParameter("feedback", 0.45f); setParameter("poles", 0.0f); setParameter("spread", 0.0f); setParameter("mix", 0.5f);
            break;
        case 5:
            setParameter("rate", 0.12f); setParameter("depth", 0.95f); setParameter("feedback", 0.6f); setParameter("poles", 2.0f); setParameter("spread", 90.0f); setParameter("mix", 0.5f);
            break;
        default: break;
    }
}

void RackPhaser::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0)
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    float curMix = mix.load(std::memory_order_relaxed);
    float fb = feedback.load(std::memory_order_relaxed);
    float dep = depth.load(std::memory_order_relaxed);
    float rHz = rateHz.load(std::memory_order_relaxed);
    int numPoles = (static_cast<int>(poles.load(std::memory_order_relaxed)) + 1) * 4;
    float spreadRad = stereoPhaseDeg.load(std::memory_order_relaxed) * juce::MathConstants<float>::pi / 180.0f;

    dryCopy.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryCopy.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    double phaseInc = (2.0 * juce::MathConstants<double>::pi * rHz) / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            double chPhase = lfoPhase + (ch == 1 ? spreadRad : 0.0);
            float lfo = 0.5f * (1.0f + static_cast<float>(std::sin(chPhase)));
            float fMin = 200.0f;
            float fMax = 4500.0f;
            float fc = fMin + (fMax - fMin) * (lfo * dep + (1.0f - dep) * 0.5f);
            float w = static_cast<float>(std::tan(juce::MathConstants<double>::pi * fc / currentSampleRate));
            float alpha = (w - 1.0f) / (w + 1.0f);

            float input = dryCopy.getSample(ch, i) + feedbackBuffer[ch] * fb;
            float x = input;

            for (int p = 0; p < numPoles; ++p)
            {
                float y = alpha * x + allpassStates[ch][p];
                allpassStates[ch][p] = x - alpha * y;
                x = y;
            }

            feedbackBuffer[ch] = x;
            float dry = dryCopy.getSample(ch, i);
            buffer.setSample(ch, i, dry * (1.0f - curMix) + x * curMix);
        }

        lfoPhase = std::fmod(lfoPhase + phaseInc, 2.0 * juce::MathConstants<double>::pi);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  12. Tape & BBD Stereo Flanger Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackFlanger::RackFlanger()
{
    prepare(44100.0, 512);
}

void RackFlanger::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    size_t lineLen = static_cast<size_t>(sampleRate * 0.05) + 64;
    delayLineL.assign(lineLen, 0.0f);
    delayLineR.assign(lineLen, 0.0f);
    writePos = 0;
    dryCopy.setSize(2, maxBlockSize, false, true, false);
    reset();
}

void RackFlanger::reset()
{
    std::fill(delayLineL.begin(), delayLineL.end(), 0.0f);
    std::fill(delayLineR.begin(), delayLineR.end(), 0.0f);
    writePos = 0;
    lfoPhase = 0.0;
}

std::vector<EffectParamInfo> RackFlanger::getParameterInfos() const
{
    return {
        { "rate", "Rate", 0.02f, 5.0f, 0.25f, "Hz", false, {} },
        { "depth", "Depth", 0.0f, 1.0f, 0.8f, "%", false, {} },
        { "delay", "Manual", 0.2f, 12.0f, 2.5f, "ms", false, {} },
        { "feedback", "Feedback", -0.98f, 0.98f, 0.65f, "%", false, {} },
        { "spread", "Stereo", 0.0f, 1.0f, 0.7f, "%", false, {} },
        { "mix", "Mix", 0.0f, 1.0f, 0.5f, "%", false, {} }
    };
}

void RackFlanger::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "rate") rateHz.store(value, std::memory_order_relaxed);
    else if (paramId == "depth") depth.store(value, std::memory_order_relaxed);
    else if (paramId == "delay") delayMs.store(value, std::memory_order_relaxed);
    else if (paramId == "feedback") feedback.store(value, std::memory_order_relaxed);
    else if (paramId == "spread") stereoSpread.store(value, std::memory_order_relaxed);
    else if (paramId == "mix") mix.store(value, std::memory_order_relaxed);
}

float RackFlanger::getParameter(const juce::String& paramId) const
{
    if (paramId == "rate") return rateHz.load(std::memory_order_relaxed);
    if (paramId == "depth") return depth.load(std::memory_order_relaxed);
    if (paramId == "delay") return delayMs.load(std::memory_order_relaxed);
    if (paramId == "feedback") return feedback.load(std::memory_order_relaxed);
    if (paramId == "spread") return stereoSpread.load(std::memory_order_relaxed);
    if (paramId == "mix") return mix.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackFlanger::getPresetNames() const
{
    return {
        "Jet Engine Sweep",
        "Through-Zero Tape Flange",
        "Subtle Metallic Edge",
        "Sci-Fi Resonator",
        "Thick 80s Dimension",
        "Deep Underwater Flange"
    };
}

void RackFlanger::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0:
            setParameter("rate", 0.2f); setParameter("depth", 0.85f); setParameter("delay", 2.0f); setParameter("feedback", 0.8f); setParameter("spread", 0.8f); setParameter("mix", 0.5f);
            break;
        case 1:
            setParameter("rate", 0.1f); setParameter("depth", 0.95f); setParameter("delay", 0.8f); setParameter("feedback", -0.7f); setParameter("spread", 0.5f); setParameter("mix", 0.5f);
            break;
        case 2:
            setParameter("rate", 0.5f); setParameter("depth", 0.4f); setParameter("delay", 1.2f); setParameter("feedback", 0.4f); setParameter("spread", 0.6f); setParameter("mix", 0.35f);
            break;
        case 3:
            setParameter("rate", 1.8f); setParameter("depth", 0.9f); setParameter("delay", 4.5f); setParameter("feedback", 0.92f); setParameter("spread", 1.0f); setParameter("mix", 0.6f);
            break;
        case 4:
            setParameter("rate", 0.35f); setParameter("depth", 0.7f); setParameter("delay", 3.0f); setParameter("feedback", -0.55f); setParameter("spread", 0.9f); setParameter("mix", 0.5f);
            break;
        case 5:
            setParameter("rate", 0.08f); setParameter("depth", 1.0f); setParameter("delay", 6.0f); setParameter("feedback", 0.75f); setParameter("spread", 0.75f); setParameter("mix", 0.55f);
            break;
        default: break;
    }
}

void RackFlanger::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0)
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    float curMix = mix.load(std::memory_order_relaxed);
    float fb = feedback.load(std::memory_order_relaxed);
    float dep = depth.load(std::memory_order_relaxed);
    float manDelay = delayMs.load(std::memory_order_relaxed) * 0.001f * static_cast<float>(currentSampleRate);
    float rHz = rateHz.load(std::memory_order_relaxed);
    float spreadPhase = stereoSpread.load(std::memory_order_relaxed) * juce::MathConstants<float>::pi;

    dryCopy.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryCopy.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    size_t lineLen = delayLineL.size();
    if (lineLen < 128) return;

    double phaseInc = (2.0 * juce::MathConstants<double>::pi * rHz) / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& line = (ch == 0) ? delayLineL : delayLineR;
            double chPhase = lfoPhase + (ch == 1 ? spreadPhase : 0.0);
            float mod = 0.5f * (1.0f + static_cast<float>(std::sin(chPhase)));
            float delaySamps = juce::jlimit(1.0f, static_cast<float>(lineLen - 4), manDelay * (1.0f + dep * (mod * 2.0f - 1.0f)));

            double readPos = static_cast<double>(writePos) - delaySamps;
            if (readPos < 0.0) readPos += lineLen;

            size_t idx0 = static_cast<size_t>(readPos);
            size_t idx1 = (idx0 + 1) % lineLen;
            float frac = static_cast<float>(readPos - std::floor(readPos));
            float flanged = line[idx0] + frac * (line[idx1] - line[idx0]);

            float dry = dryCopy.getSample(ch, i);
            line[writePos] = dry + flanged * fb;

            buffer.setSample(ch, i, dry * (1.0f - curMix) + flanged * curMix);
        }

        writePos = (writePos + 1) % lineLen;
        lfoPhase = std::fmod(lfoPhase + phaseInc, 2.0 * juce::MathConstants<double>::pi);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  13. Stereo Width & Spatial Imager Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackStereoImager::RackStereoImager()
{
    reset();
}

void RackStereoImager::prepare(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate;
    reset();
}

void RackStereoImager::reset()
{
    sideHpState = 0.0f;
}

std::vector<EffectParamInfo> RackStereoImager::getParameterInfos() const
{
    return {
        { "width", "Width", 0.0f, 2.0f, 1.35f, "%", false, {} },
        { "monobass", "Mono Bass", 20.0f, 500.0f, 120.0f, "Hz", false, {} },
        { "balance", "Balance", -1.0f, 1.0f, 0.0f, "%", false, {} },
        { "sidegain", "Side Gain", -12.0f, 12.0f, 0.0f, "dB", false, {} },
        { "midgain", "Mid Gain", -12.0f, 12.0f, 0.0f, "dB", false, {} }
    };
}

void RackStereoImager::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "width") width.store(value, std::memory_order_relaxed);
    else if (paramId == "monobass") monoBassCutoffHz.store(value, std::memory_order_relaxed);
    else if (paramId == "balance") balance.store(value, std::memory_order_relaxed);
    else if (paramId == "sidegain") sideGainDb.store(value, std::memory_order_relaxed);
    else if (paramId == "midgain") midGainDb.store(value, std::memory_order_relaxed);
}

float RackStereoImager::getParameter(const juce::String& paramId) const
{
    if (paramId == "width") return width.load(std::memory_order_relaxed);
    if (paramId == "monobass") return monoBassCutoffHz.load(std::memory_order_relaxed);
    if (paramId == "balance") return balance.load(std::memory_order_relaxed);
    if (paramId == "sidegain") return sideGainDb.load(std::memory_order_relaxed);
    if (paramId == "midgain") return midGainDb.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackStereoImager::getPresetNames() const
{
    return {
        "Master Width & Mono Bass",
        "Ultra-Wide Ambience (200%)",
        "Mono Compatibility Check",
        "Side High-Shimmer Boost",
        "Tight Vocal Centering",
        "Sub Bass Anchor (250 Hz)"
    };
}

void RackStereoImager::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0:
            setParameter("width", 1.35f); setParameter("monobass", 120.0f); setParameter("balance", 0.0f); setParameter("sidegain", 0.0f); setParameter("midgain", 0.0f);
            break;
        case 1:
            setParameter("width", 1.85f); setParameter("monobass", 150.0f); setParameter("balance", 0.0f); setParameter("sidegain", 2.5f); setParameter("midgain", -1.0f);
            break;
        case 2:
            setParameter("width", 0.0f); setParameter("monobass", 20.0f); setParameter("balance", 0.0f); setParameter("sidegain", 0.0f); setParameter("midgain", 0.0f);
            break;
        case 3:
            setParameter("width", 1.5f); setParameter("monobass", 200.0f); setParameter("balance", 0.0f); setParameter("sidegain", 3.0f); setParameter("midgain", 0.0f);
            break;
        case 4:
            setParameter("width", 0.85f); setParameter("monobass", 100.0f); setParameter("balance", 0.0f); setParameter("sidegain", -2.0f); setParameter("midgain", 1.5f);
            break;
        case 5:
            setParameter("width", 1.25f); setParameter("monobass", 250.0f); setParameter("balance", 0.0f); setParameter("sidegain", 0.5f); setParameter("midgain", 0.0f);
            break;
        default: break;
    }
}

void RackStereoImager::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0 || buffer.getNumChannels() < 2)
        return;

    int numSamples = buffer.getNumSamples();
    float w = width.load(std::memory_order_relaxed);
    float mbFreq = monoBassCutoffHz.load(std::memory_order_relaxed);
    float bal = balance.load(std::memory_order_relaxed);
    float sGain = std::pow(10.0f, sideGainDb.load(std::memory_order_relaxed) / 20.0f);
    float mGain = std::pow(10.0f, midGainDb.load(std::memory_order_relaxed) / 20.0f);

    float alpha = static_cast<float>(std::exp(-2.0 * juce::MathConstants<double>::pi * mbFreq / currentSampleRate));
    float panL = (bal < 0.0f) ? 1.0f : (1.0f - bal);
    float panR = (bal > 0.0f) ? 1.0f : (1.0f + bal);

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        float l = left[i];
        float r = right[i];

        float mid = (l + r) * 0.5f * mGain;
        float rawSide = (l - r) * 0.5f;

        sideHpState = rawSide - (rawSide - sideHpState) * alpha;
        float side = (rawSide - sideHpState) * w * sGain;

        left[i] = (mid + side) * panL;
        right[i] = (mid - side) * panR;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  14. Dynamic Auto-Wah & Envelope Filter Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackAutoWah::RackAutoWah()
{
    reset();
}

void RackAutoWah::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    dryCopy.setSize(2, maxBlockSize, false, true, false);
    reset();
}

void RackAutoWah::reset()
{
    envFollower = 0.0f;
    s1[0] = s1[1] = 0.0f;
    s2[0] = s2[1] = 0.0f;
}

std::vector<EffectParamInfo> RackAutoWah::getParameterInfos() const
{
    return {
        { "sens", "Sensitivity", 0.0f, 1.0f, 0.6f, "%", false, {} },
        { "depth", "Depth", 0.0f, 1.0f, 0.75f, "%", false, {} },
        { "q", "Resonance", 0.5f, 10.0f, 4.5f, "", false, {} },
        { "base", "Base Freq", 40.0f, 3000.0f, 350.0f, "Hz", false, {} },
        { "attack", "Attack", 1.0f, 100.0f, 12.0f, "ms", false, {} },
        { "release", "Release", 20.0f, 500.0f, 120.0f, "ms", false, {} },
        { "mode", "Type", 0.0f, 2.0f, 0.0f, "", true, { "Lowpass", "Bandpass", "Highpass" } },
        { "mix", "Mix", 0.0f, 1.0f, 0.85f, "%", false, {} }
    };
}

void RackAutoWah::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "sens") sensitivity.store(value, std::memory_order_relaxed);
    else if (paramId == "depth") depth.store(value, std::memory_order_relaxed);
    else if (paramId == "q") resonance.store(value, std::memory_order_relaxed);
    else if (paramId == "base") baseCutoffHz.store(value, std::memory_order_relaxed);
    else if (paramId == "attack") attackMs.store(value, std::memory_order_relaxed);
    else if (paramId == "release") releaseMs.store(value, std::memory_order_relaxed);
    else if (paramId == "mode") mode.store(value, std::memory_order_relaxed);
    else if (paramId == "mix") mix.store(value, std::memory_order_relaxed);
}

float RackAutoWah::getParameter(const juce::String& paramId) const
{
    if (paramId == "sens") return sensitivity.load(std::memory_order_relaxed);
    if (paramId == "depth") return depth.load(std::memory_order_relaxed);
    if (paramId == "q") return resonance.load(std::memory_order_relaxed);
    if (paramId == "base") return baseCutoffHz.load(std::memory_order_relaxed);
    if (paramId == "attack") return attackMs.load(std::memory_order_relaxed);
    if (paramId == "release") return releaseMs.load(std::memory_order_relaxed);
    if (paramId == "mode") return mode.load(std::memory_order_relaxed);
    if (paramId == "mix") return mix.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackAutoWah::getPresetNames() const
{
    return {
        "Classic Funk Wah",
        "Subtle Dynamic Pluck",
        "Acid Squawk Bandpass",
        "Down-Sweep Duck",
        "Talkbox Vowel",
        "Punchy Slap Bass"
    };
}

void RackAutoWah::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0:
            setParameter("sens", 0.65f); setParameter("depth", 0.8f); setParameter("q", 5.0f); setParameter("base", 300.0f); setParameter("attack", 10.0f); setParameter("release", 110.0f); setParameter("mode", 0.0f); setParameter("mix", 0.9f);
            break;
        case 1:
            setParameter("sens", 0.45f); setParameter("depth", 0.5f); setParameter("q", 2.5f); setParameter("base", 500.0f); setParameter("attack", 5.0f); setParameter("release", 75.0f); setParameter("mode", 0.0f); setParameter("mix", 0.65f);
            break;
        case 2:
            setParameter("sens", 0.8f); setParameter("depth", 0.9f); setParameter("q", 8.0f); setParameter("base", 200.0f); setParameter("attack", 8.0f); setParameter("release", 140.0f); setParameter("mode", 1.0f); setParameter("mix", 0.85f);
            break;
        case 3:
            setParameter("sens", 0.55f); setParameter("depth", 0.7f); setParameter("q", 3.8f); setParameter("base", 1800.0f); setParameter("attack", 15.0f); setParameter("release", 220.0f); setParameter("mode", 0.0f); setParameter("mix", 0.8f);
            break;
        case 4:
            setParameter("sens", 0.75f); setParameter("depth", 0.85f); setParameter("q", 6.5f); setParameter("base", 450.0f); setParameter("attack", 18.0f); setParameter("release", 160.0f); setParameter("mode", 1.0f); setParameter("mix", 0.95f);
            break;
        case 5:
            setParameter("sens", 0.7f); setParameter("depth", 0.7f); setParameter("q", 4.0f); setParameter("base", 120.0f); setParameter("attack", 6.0f); setParameter("release", 90.0f); setParameter("mode", 0.0f); setParameter("mix", 0.8f);
            break;
        default: break;
    }
}

void RackAutoWah::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0)
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    float curMix = mix.load(std::memory_order_relaxed);
    float sens = sensitivity.load(std::memory_order_relaxed);
    float dep = depth.load(std::memory_order_relaxed);
    float q = resonance.load(std::memory_order_relaxed);
    float baseF = baseCutoffHz.load(std::memory_order_relaxed);
    float attCoeff = static_cast<float>(std::exp(-1.0 / ((attackMs.load(std::memory_order_relaxed) * 0.001f) * currentSampleRate)));
    float relCoeff = static_cast<float>(std::exp(-1.0 / ((releaseMs.load(std::memory_order_relaxed) * 0.001f) * currentSampleRate)));
    int fMode = static_cast<int>(mode.load(std::memory_order_relaxed));

    dryCopy.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryCopy.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        float inputMag = std::abs(dryCopy.getSample(0, i));
        if (numChannels > 1)
            inputMag = std::max(inputMag, std::abs(dryCopy.getSample(1, i)));

        if (inputMag > envFollower)
            envFollower = inputMag + attCoeff * (envFollower - inputMag);
        else
            envFollower = inputMag + relCoeff * (envFollower - inputMag);

        float modFc = baseF * std::pow(2.0f, envFollower * sens * dep * 4.0f);
        modFc = juce::jlimit(20.0f, static_cast<float>(currentSampleRate * 0.48), modFc);

        float g = static_cast<float>(std::tan(juce::MathConstants<double>::pi * modFc / currentSampleRate));
        float k = 1.0f / q;
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float in = dryCopy.getSample(ch, i);
            float v3 = in - s2[ch];
            float v1 = a1 * s1[ch] + a2 * v3;
            float v2 = s2[ch] + a2 * s1[ch] + a3 * v3;
            s1[ch] = 2.0f * v1 - s1[ch];
            s2[ch] = 2.0f * v2 - s2[ch];

            float lp = v2;
            float bp = v1;
            float hp = in - k * v1 - v2;

            float out = (fMode == 1) ? bp : ((fMode == 2) ? hp : lp);
            buffer.setSample(ch, i, in * (1.0f - curMix) + out * curMix);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  15. Metallic Ring Modulator Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackRingModulator::RackRingModulator()
{
    reset();
}

void RackRingModulator::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    dryCopy.setSize(2, maxBlockSize, false, true, false);
    reset();
}

void RackRingModulator::reset()
{
    carrierPhase = 0.0;
    lfoPhase = 0.0;
}

std::vector<EffectParamInfo> RackRingModulator::getParameterInfos() const
{
    return {
        { "carrier", "Carrier Freq", 1.0f, 4000.0f, 440.0f, "Hz", false, {} },
        { "shape", "Waveform", 0.0f, 2.0f, 0.0f, "", true, { "Sine", "Triangle", "Square" } },
        { "lforate", "LFO Rate", 0.0f, 20.0f, 0.0f, "Hz", false, {} },
        { "lfodepth", "LFO Depth", 0.0f, 1.0f, 0.0f, "%", false, {} },
        { "mix", "Mix", 0.0f, 1.0f, 0.6f, "%", false, {} }
    };
}

void RackRingModulator::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "carrier") carrierFreqHz.store(value, std::memory_order_relaxed);
    else if (paramId == "shape") waveShape.store(value, std::memory_order_relaxed);
    else if (paramId == "lforate") lfoRateHz.store(value, std::memory_order_relaxed);
    else if (paramId == "lfodepth") lfoDepth.store(value, std::memory_order_relaxed);
    else if (paramId == "mix") mix.store(value, std::memory_order_relaxed);
}

float RackRingModulator::getParameter(const juce::String& paramId) const
{
    if (paramId == "carrier") return carrierFreqHz.load(std::memory_order_relaxed);
    if (paramId == "shape") return waveShape.load(std::memory_order_relaxed);
    if (paramId == "lforate") return lfoRateHz.load(std::memory_order_relaxed);
    if (paramId == "lfodepth") return lfoDepth.load(std::memory_order_relaxed);
    if (paramId == "mix") return mix.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackRingModulator::getPresetNames() const
{
    return {
        "Sci-Fi Dalek Robot",
        "Tubular Bells",
        "Subtle Harmonic Clang",
        "Tremolo Ring Stutter",
        "Alien Transmission",
        "Sub Harmonic Drone (60 Hz)"
    };
}

void RackRingModulator::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0:
            setParameter("carrier", 280.0f); setParameter("shape", 0.0f); setParameter("lforate", 0.0f); setParameter("lfodepth", 0.0f); setParameter("mix", 0.75f);
            break;
        case 1:
            setParameter("carrier", 1120.0f); setParameter("shape", 1.0f); setParameter("lforate", 0.0f); setParameter("lfodepth", 0.0f); setParameter("mix", 0.5f);
            break;
        case 2:
            setParameter("carrier", 640.0f); setParameter("shape", 0.0f); setParameter("lforate", 2.0f); setParameter("lfodepth", 0.25f); setParameter("mix", 0.35f);
            break;
        case 3:
            setParameter("carrier", 18.0f); setParameter("shape", 2.0f); setParameter("lforate", 0.0f); setParameter("lfodepth", 0.0f); setParameter("mix", 0.8f);
            break;
        case 4:
            setParameter("carrier", 880.0f); setParameter("shape", 2.0f); setParameter("lforate", 6.5f); setParameter("lfodepth", 0.8f); setParameter("mix", 0.65f);
            break;
        case 5:
            setParameter("carrier", 60.0f); setParameter("shape", 0.0f); setParameter("lforate", 0.5f); setParameter("lfodepth", 0.15f); setParameter("mix", 0.5f);
            break;
        default: break;
    }
}

void RackRingModulator::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0)
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    float curMix = mix.load(std::memory_order_relaxed);
    float baseCarrier = carrierFreqHz.load(std::memory_order_relaxed);
    int shape = static_cast<int>(waveShape.load(std::memory_order_relaxed));
    float lfoR = lfoRateHz.load(std::memory_order_relaxed);
    float lfoD = lfoDepth.load(std::memory_order_relaxed);

    dryCopy.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryCopy.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    double lfoInc = (2.0 * juce::MathConstants<double>::pi * lfoR) / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        float lfoVal = (lfoR > 0.001f) ? static_cast<float>(std::sin(lfoPhase)) : 0.0f;
        float actualCarrier = baseCarrier * (1.0f + lfoVal * lfoD);
        double carrierInc = (2.0 * juce::MathConstants<double>::pi * actualCarrier) / currentSampleRate;

        float carrierVal = 0.0f;
        double normP = carrierPhase / (2.0 * juce::MathConstants<double>::pi);
        normP = normP - std::floor(normP);

        if (shape == 1)
            carrierVal = (normP < 0.5) ? static_cast<float>(4.0 * normP - 1.0) : static_cast<float>(3.0 - 4.0 * normP);
        else if (shape == 2)
            carrierVal = (normP < 0.5) ? 1.0f : -1.0f;
        else
            carrierVal = static_cast<float>(std::sin(carrierPhase));

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float dry = dryCopy.getSample(ch, i);
            float wet = dry * carrierVal;
            buffer.setSample(ch, i, dry * (1.0f - curMix) + wet * curMix);
        }

        carrierPhase = std::fmod(carrierPhase + carrierInc, 2.0 * juce::MathConstants<double>::pi);
        lfoPhase = std::fmod(lfoPhase + lfoInc, 2.0 * juce::MathConstants<double>::pi);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  16. Vintage Amp & Cabinet Simulator Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackAmpCabinet::RackAmpCabinet()
{
    reset();
}

void RackAmpCabinet::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    dryCopy.setSize(2, maxBlockSize, false, true, false);
    reset();
}

void RackAmpCabinet::reset()
{
    sBass[0] = sBass[1] = 0.0f;
    sTreble[0] = sTreble[1] = 0.0f;
    sCab[0] = sCab[1] = 0.0f;
}

std::vector<EffectParamInfo> RackAmpCabinet::getParameterInfos() const
{
    return {
        { "drive", "Drive", 0.0f, 1.0f, 0.45f, "%", false, {} },
        { "bass", "Bass", 0.0f, 1.0f, 0.5f, "%", false, {} },
        { "mid", "Middle", 0.0f, 1.0f, 0.5f, "%", false, {} },
        { "treble", "Treble", 0.0f, 1.0f, 0.5f, "%", false, {} },
        { "presence", "Presence", 0.0f, 1.0f, 0.5f, "%", false, {} },
        { "cab", "Cabinet", 0.0f, 4.0f, 0.0f, "", true, { "British 4x12", "Tweed 1x12", "Boutique 2x12", "Ampeg 8x10", "Radio Horn" } },
        { "output", "Master", -18.0f, 6.0f, -2.0f, "dB", false, {} },
        { "mix", "Mix", 0.0f, 1.0f, 1.0f, "%", false, {} }
    };
}

void RackAmpCabinet::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "drive") drive.store(value, std::memory_order_relaxed);
    else if (paramId == "bass") bass.store(value, std::memory_order_relaxed);
    else if (paramId == "mid") mid.store(value, std::memory_order_relaxed);
    else if (paramId == "treble") treble.store(value, std::memory_order_relaxed);
    else if (paramId == "presence") presence.store(value, std::memory_order_relaxed);
    else if (paramId == "cab") cabinetType.store(value, std::memory_order_relaxed);
    else if (paramId == "output") outputGainDb.store(value, std::memory_order_relaxed);
    else if (paramId == "mix") mix.store(value, std::memory_order_relaxed);
}

float RackAmpCabinet::getParameter(const juce::String& paramId) const
{
    if (paramId == "drive") return drive.load(std::memory_order_relaxed);
    if (paramId == "bass") return bass.load(std::memory_order_relaxed);
    if (paramId == "mid") return mid.load(std::memory_order_relaxed);
    if (paramId == "treble") return treble.load(std::memory_order_relaxed);
    if (paramId == "presence") return presence.load(std::memory_order_relaxed);
    if (paramId == "cab") return cabinetType.load(std::memory_order_relaxed);
    if (paramId == "output") return outputGainDb.load(std::memory_order_relaxed);
    if (paramId == "mix") return mix.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackAmpCabinet::getPresetNames() const
{
    return {
        "British 4x12 Crunch",
        "Warm Tweed Clean",
        "Ampeg Bass Punch",
        "High-Gain Solo Lead",
        "Lo-Fi Phone Speaker",
        "Boutique Chime"
    };
}

void RackAmpCabinet::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0:
            setParameter("drive", 0.6f); setParameter("bass", 0.55f); setParameter("mid", 0.65f); setParameter("treble", 0.6f); setParameter("presence", 0.55f); setParameter("cab", 0.0f); setParameter("output", -3.0f); setParameter("mix", 1.0f);
            break;
        case 1:
            setParameter("drive", 0.22f); setParameter("bass", 0.6f); setParameter("mid", 0.5f); setParameter("treble", 0.45f); setParameter("presence", 0.4f); setParameter("cab", 1.0f); setParameter("output", 0.0f); setParameter("mix", 1.0f);
            break;
        case 2:
            setParameter("drive", 0.38f); setParameter("bass", 0.8f); setParameter("mid", 0.35f); setParameter("treble", 0.4f); setParameter("presence", 0.3f); setParameter("cab", 3.0f); setParameter("output", -2.0f); setParameter("mix", 1.0f);
            break;
        case 3:
            setParameter("drive", 0.88f); setParameter("bass", 0.5f); setParameter("mid", 0.75f); setParameter("treble", 0.7f); setParameter("presence", 0.7f); setParameter("cab", 0.0f); setParameter("output", -5.0f); setParameter("mix", 1.0f);
            break;
        case 4:
            setParameter("drive", 0.5f); setParameter("bass", 0.1f); setParameter("mid", 0.9f); setParameter("treble", 0.2f); setParameter("presence", 0.1f); setParameter("cab", 4.0f); setParameter("output", -1.0f); setParameter("mix", 1.0f);
            break;
        case 5:
            setParameter("drive", 0.4f); setParameter("bass", 0.45f); setParameter("mid", 0.6f); setParameter("treble", 0.75f); setParameter("presence", 0.65f); setParameter("cab", 2.0f); setParameter("output", -2.5f); setParameter("mix", 1.0f);
            break;
        default: break;
    }
}

void RackAmpCabinet::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0)
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    float curMix = mix.load(std::memory_order_relaxed);
    float drv = 1.0f + drive.load(std::memory_order_relaxed) * 12.0f;
    float bGain = 0.5f + bass.load(std::memory_order_relaxed) * 1.0f;
    float mGain = 0.5f + mid.load(std::memory_order_relaxed) * 1.0f;
    float tGain = 0.5f + treble.load(std::memory_order_relaxed) * 1.0f;
    float pGain = 0.5f + presence.load(std::memory_order_relaxed) * 1.0f;
    int cab = static_cast<int>(cabinetType.load(std::memory_order_relaxed));
    float outVol = std::pow(10.0f, outputGainDb.load(std::memory_order_relaxed) / 20.0f);

    dryCopy.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryCopy.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    float cabCutoff = 4800.0f;
    if (cab == 1) cabCutoff = 4200.0f;
    else if (cab == 2) cabCutoff = 5400.0f;
    else if (cab == 3) cabCutoff = 3500.0f;
    else if (cab == 4) cabCutoff = 2400.0f;
    float cabCoeff = static_cast<float>(std::exp(-2.0 * juce::MathConstants<double>::pi * cabCutoff / currentSampleRate));

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float in = dryCopy.getSample(ch, i);

            float x = in * drv;
            float sat = std::tanh(x + 0.15f * x * x);

            sBass[ch] += 0.05f * (sat - sBass[ch]);
            float bassComponent = sBass[ch] * bGain;

            sTreble[ch] += 0.25f * (sat - sTreble[ch]);
            float trebleComponent = (sat - sTreble[ch]) * tGain;

            float midComponent = (sat - bassComponent - trebleComponent) * mGain;
            float shaped = (bassComponent + midComponent + trebleComponent + sat * pGain * 0.3f);

            sCab[ch] = shaped + cabCoeff * (sCab[ch] - shaped);
            float wet = sCab[ch] * outVol;

            buffer.setSample(ch, i, in * (1.0f - curMix) + wet * curMix);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  17. Global ADSR Gain Envelope Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackADSREnvelope::RackADSREnvelope()
{
    loadPreset(0);
}

void RackADSREnvelope::prepare(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
}

void RackADSREnvelope::reset()
{
    fastEnv[0] = fastEnv[1] = 0.0f;
    slowEnv[0] = slowEnv[1] = 0.0f;
    currentTelemetryMeter.store(0.0f, std::memory_order_relaxed);
}

std::vector<EffectParamInfo> RackADSREnvelope::getParameterInfos() const
{
    return {
        { "attack",  "Attack",  0.1f, 5000.0f,  5.0f,   "ms", false, {} },
        { "decay",   "Decay",   1.0f, 5000.0f,  200.0f, "ms", false, {} },
        { "sustain", "Sustain", 0.0f, 100.0f,   100.0f, "%",  false, {} },
        { "release", "Release", 1.0f, 10000.0f, 300.0f, "ms", false, {} },
        { "punch",   "Punch",   0.0f, 100.0f,   0.0f,   "%",  false, {} },
        { "gain",    "Gain",   -24.0f, 12.0f,   0.0f,   "dB", false, {} }
    };
}

void RackADSREnvelope::setParameter(const juce::String& paramId, float value)
{
    if (paramId == "attack")       attackMs.store(juce::jlimit(0.1f, 5000.0f, value), std::memory_order_relaxed);
    else if (paramId == "decay")   decayMs.store(juce::jlimit(1.0f, 5000.0f, value), std::memory_order_relaxed);
    else if (paramId == "sustain") sustainPercent.store(juce::jlimit(0.0f, 100.0f, value), std::memory_order_relaxed);
    else if (paramId == "release") releaseMs.store(juce::jlimit(1.0f, 10000.0f, value), std::memory_order_relaxed);
    else if (paramId == "punch")   punchPercent.store(juce::jlimit(0.0f, 100.0f, value), std::memory_order_relaxed);
    else if (paramId == "gain")    gainDb.store(juce::jlimit(-24.0f, 12.0f, value), std::memory_order_relaxed);
}

float RackADSREnvelope::getParameter(const juce::String& paramId) const
{
    if (paramId == "attack")       return attackMs.load(std::memory_order_relaxed);
    if (paramId == "decay")        return decayMs.load(std::memory_order_relaxed);
    if (paramId == "sustain")      return sustainPercent.load(std::memory_order_relaxed);
    if (paramId == "release")      return releaseMs.load(std::memory_order_relaxed);
    if (paramId == "punch")        return punchPercent.load(std::memory_order_relaxed);
    if (paramId == "gain")         return gainDb.load(std::memory_order_relaxed);
    return 0.0f;
}

juce::StringArray RackADSREnvelope::getPresetNames() const
{
    return {
        "Default Linear",
        "Fast Percussive / Staccato",
        "Pluck & Tight Key",
        "Smooth Pad & Swell",
        "Slow Ambient Drone",
        "Organ Gate (Zero Rel)",
        "Punchy Snare & Clap",
        "Long Lush Reverb Tail"
    };
}

void RackADSREnvelope::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
        case 0: // Default Linear
            setParameter("attack", 5.0f); setParameter("decay", 200.0f); setParameter("sustain", 100.0f); setParameter("release", 300.0f); setParameter("punch", 0.0f); setParameter("gain", 0.0f);
            break;
        case 1: // Fast Percussive / Staccato
            setParameter("attack", 0.5f); setParameter("decay", 120.0f); setParameter("sustain", 0.0f); setParameter("release", 80.0f); setParameter("punch", 35.0f); setParameter("gain", 0.0f);
            break;
        case 2: // Pluck & Tight Key
            setParameter("attack", 1.0f); setParameter("decay", 350.0f); setParameter("sustain", 25.0f); setParameter("release", 180.0f); setParameter("punch", 20.0f); setParameter("gain", 0.0f);
            break;
        case 3: // Smooth Pad & Swell
            setParameter("attack", 650.0f); setParameter("decay", 800.0f); setParameter("sustain", 85.0f); setParameter("release", 1200.0f); setParameter("punch", 0.0f); setParameter("gain", 1.0f);
            break;
        case 4: // Slow Ambient Drone
            setParameter("attack", 1800.0f); setParameter("decay", 1500.0f); setParameter("sustain", 90.0f); setParameter("release", 3500.0f); setParameter("punch", 0.0f); setParameter("gain", 2.0f);
            break;
        case 5: // Organ Gate (Zero Rel)
            setParameter("attack", 2.0f); setParameter("decay", 50.0f); setParameter("sustain", 100.0f); setParameter("release", 10.0f); setParameter("punch", 0.0f); setParameter("gain", 0.0f);
            break;
        case 6: // Punchy Snare & Clap
            setParameter("attack", 0.1f); setParameter("decay", 240.0f); setParameter("sustain", 0.0f); setParameter("release", 150.0f); setParameter("punch", 60.0f); setParameter("gain", 1.5f);
            break;
        case 7: // Long Lush Reverb Tail
            setParameter("attack", 10.0f); setParameter("decay", 500.0f); setParameter("sustain", 70.0f); setParameter("release", 4500.0f); setParameter("punch", 10.0f); setParameter("gain", 0.0f);
            break;
        default: break;
    }
}

void RackADSREnvelope::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0)
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    float punch = punchPercent.load(std::memory_order_relaxed) * 0.01f;
    float gainLinear = std::pow(10.0f, gainDb.load(std::memory_order_relaxed) / 20.0f);

    float fastCoeff = std::exp(-1.0f / (0.005f * static_cast<float>(currentSampleRate)));
    float slowCoeff = std::exp(-1.0f / (0.040f * static_cast<float>(currentSampleRate)));

    float sumSq = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer(ch);
        int stateIdx = (ch == 0) ? 0 : 1;
        float fEnv = fastEnv[stateIdx];
        float sEnv = slowEnv[stateIdx];

        for (int i = 0; i < numSamples; ++i)
        {
            float s = channelData[i];
            float absS = std::abs(s);

            fEnv = absS + fastCoeff * (fEnv - absS);
            sEnv = absS + slowCoeff * (sEnv - absS);

            if (punch > 0.001f)
            {
                float diff = fEnv - sEnv;
                if (diff > 0.0f)
                {
                    s += s * (diff * punch * 2.5f);
                }
            }

            s *= gainLinear;
            channelData[i] = s;
            sumSq += s * s;
        }

        fastEnv[stateIdx] = fEnv;
        slowEnv[stateIdx] = sEnv;
    }

    float rms = std::sqrt(sumSq / static_cast<float>(std::max(1, numChannels * numSamples)));
    float curMeter = currentTelemetryMeter.load(std::memory_order_relaxed);
    currentTelemetryMeter.store(curMeter * 0.75f + rms * 0.25f, std::memory_order_relaxed);
}

// ─────────────────────────────────────────────────────────────────────────────
//  PerformanceRackDSP Implementation
// ─────────────────────────────────────────────────────────────────────────────
PerformanceRackDSP::PerformanceRackDSP()
{
}

void PerformanceRackDSP::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize = maxBlockSize;

    const juce::SpinLock::ScopedLockType sl(chainLock);
    for (auto& fx : activeChain)
    {
        if (fx)
            fx->prepare(sampleRate, maxBlockSize);
    }
}

void PerformanceRackDSP::reset()
{
    const juce::SpinLock::ScopedLockType sl(chainLock);
    for (auto& fx : activeChain)
    {
        if (fx)
            fx->reset();
    }
}

void PerformanceRackDSP::process(juce::AudioBuffer<float>& buffer)
{
    if (masterBypassed.load(std::memory_order_relaxed) || buffer.getNumSamples() == 0)
        return;

    // Fast lock-free read of active chain
    if (chainLock.tryEnter())
    {
        for (auto& fx : activeChain)
        {
            if (fx && !fx->getBypassed())
                fx->process(buffer);
        }
        chainLock.exit();
    }

    // Apply master gain and soft ceiling limiter to avoid harsh digital clipping
    float mg = masterGain.load(std::memory_order_relaxed);
    if (std::abs(mg - 1.0f) > 0.001f)
    {
        buffer.applyGain(mg);
    }

    // Soft-knee safety ceiling at +/-0.99
    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* ptr = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float val = ptr[i];
            if (val > 0.95f) ptr[i] = 0.95f + 0.04f * std::tanh((val - 0.95f) / 0.04f);
            else if (val < -0.95f) ptr[i] = -0.95f + 0.04f * std::tanh((val + 0.95f) / 0.04f);
        }
    }
}

std::shared_ptr<RackEffectBase> PerformanceRackDSP::createEffect(PerformanceEffectType type)
{
    switch (type)
    {
        case PerformanceEffectType::Reverb:       return std::make_shared<RackReverb>();
        case PerformanceEffectType::Delay:        return std::make_shared<RackDelay>();
        case PerformanceEffectType::Filter:       return std::make_shared<RackFilter>();
        case PerformanceEffectType::Distortion:   return std::make_shared<RackDistortion>();
        case PerformanceEffectType::Chorus:       return std::make_shared<RackChorus>();
        case PerformanceEffectType::Compressor:   return std::make_shared<RackCompressor>();
        case PerformanceEffectType::Bitcrusher:   return std::make_shared<RackBitcrusher>();
        case PerformanceEffectType::ParametricEQ: return std::make_shared<RackParametricEQ>();
        case PerformanceEffectType::PitchShifter: return std::make_shared<RackPitchShifter>();
        case PerformanceEffectType::Tremolo:      return std::make_shared<RackTremolo>();
        case PerformanceEffectType::Phaser:       return std::make_shared<RackPhaser>();
        case PerformanceEffectType::Flanger:      return std::make_shared<RackFlanger>();
        case PerformanceEffectType::StereoImager: return std::make_shared<RackStereoImager>();
        case PerformanceEffectType::AutoWah:      return std::make_shared<RackAutoWah>();
        case PerformanceEffectType::RingModulator:return std::make_shared<RackRingModulator>();
        case PerformanceEffectType::AmpCabinet:   return std::make_shared<RackAmpCabinet>();
        case PerformanceEffectType::ADSREnvelope: return std::make_shared<RackADSREnvelope>();
        default: break;
    }
    return nullptr;
}

std::shared_ptr<RackEffectBase> PerformanceRackDSP::addEffect(PerformanceEffectType type)
{
    auto newFx = createEffect(type);
    if (!newFx) return nullptr;

    newFx->prepare(currentSampleRate, currentBlockSize);

    const juce::SpinLock::ScopedLockType sl(chainLock);
    activeChain.push_back(newFx);
    return newFx;
}

void PerformanceRackDSP::removeEffect(int index)
{
    const juce::SpinLock::ScopedLockType sl(chainLock);
    if (index >= 0 && index < static_cast<int>(activeChain.size()))
    {
        activeChain.erase(activeChain.begin() + index);
    }
}

void PerformanceRackDSP::moveEffect(int fromIndex, int toIndex)
{
    const juce::SpinLock::ScopedLockType sl(chainLock);
    int count = static_cast<int>(activeChain.size());
    if (fromIndex >= 0 && fromIndex < count && toIndex >= 0 && toIndex < count && fromIndex != toIndex)
    {
        auto item = activeChain[fromIndex];
        activeChain.erase(activeChain.begin() + fromIndex);
        activeChain.insert(activeChain.begin() + toIndex, item);
    }
}

void PerformanceRackDSP::clearEffects()
{
    const juce::SpinLock::ScopedLockType sl(chainLock);
    activeChain.clear();
}

int PerformanceRackDSP::getNumEffects() const
{
    const juce::SpinLock::ScopedLockType sl(chainLock);
    return static_cast<int>(activeChain.size());
}

std::shared_ptr<RackEffectBase> PerformanceRackDSP::getEffect(int index) const
{
    const juce::SpinLock::ScopedLockType sl(chainLock);
    if (index >= 0 && index < static_cast<int>(activeChain.size()))
        return activeChain[index];
    return nullptr;
}

std::vector<std::shared_ptr<RackEffectBase>> PerformanceRackDSP::getEffectsSnapshot() const
{
    const juce::SpinLock::ScopedLockType sl(chainLock);
    return activeChain;
}

bool PerformanceRackDSP::getActiveADSREnvelope(float& attackSec, float& decaySec, float& sustainLevel, float& releaseSec) const
{
    if (masterBypassed.load(std::memory_order_relaxed))
        return false;

    const juce::SpinLock::ScopedLockType sl(chainLock);
    for (const auto& fx : activeChain)
    {
        if (fx && fx->getType() == PerformanceEffectType::ADSREnvelope && !fx->getBypassed())
        {
            attackSec = fx->getParameter("attack") / 1000.0f;
            decaySec = fx->getParameter("decay") / 1000.0f;
            sustainLevel = fx->getParameter("sustain") / 100.0f;
            releaseSec = fx->getParameter("release") / 1000.0f;
            return true;
        }
    }
    return false;
}

juce::StringArray PerformanceRackDSP::getRackTemplateNames() const
{
    return {
        "Empty Rack",
        "Lo-Fi Vintage Vinyl",
        "Ambient Space Station",
        "Slammed Drum Bus",
        "Psychedelic Dub Echo",
        "Clean Master Bus",
        "Neo-Soul & Rhodes Keys",
        "Guitar Hero Rig",
        "Funkadelic Synth Bass",
        "80s Dream Pop",
        "Sci-Fi Sound Design",
        "Stereo Width & Polish"
    };
}

void PerformanceRackDSP::loadRackTemplate(int templateIndex)
{
    clearEffects();
    switch (templateIndex)
    {
        case 1: // Lo-Fi Vintage Vinyl
        {
            auto bc = addEffect(PerformanceEffectType::Bitcrusher);
            if (bc) bc->loadPreset(0);
            auto dist = addEffect(PerformanceEffectType::Distortion);
            if (dist) dist->loadPreset(0);
            auto filt = addEffect(PerformanceEffectType::Filter);
            if (filt) filt->loadPreset(0);
            break;
        }
        case 2: // Ambient Space Station
        {
            auto ch = addEffect(PerformanceEffectType::Chorus);
            if (ch) ch->loadPreset(0);
            auto del = addEffect(PerformanceEffectType::Delay);
            if (del) del->loadPreset(1);
            auto rev = addEffect(PerformanceEffectType::Reverb);
            if (rev) rev->loadPreset(3);
            break;
        }
        case 3: // Slammed Drum Bus
        {
            auto comp = addEffect(PerformanceEffectType::Compressor);
            if (comp) comp->loadPreset(0);
            auto dist = addEffect(PerformanceEffectType::Distortion);
            if (dist) dist->loadPreset(1);
            auto eq = addEffect(PerformanceEffectType::ParametricEQ);
            if (eq) eq->loadPreset(1);
            break;
        }
        case 4: // Psychedelic Dub Echo
        {
            auto trem = addEffect(PerformanceEffectType::Tremolo);
            if (trem) trem->loadPreset(1);
            auto del = addEffect(PerformanceEffectType::Delay);
            if (del) del->loadPreset(3);
            auto rev = addEffect(PerformanceEffectType::Reverb);
            if (rev) rev->loadPreset(0);
            break;
        }
        case 5: // Clean Master Bus
        {
            auto eq = addEffect(PerformanceEffectType::ParametricEQ);
            if (eq) eq->loadPreset(3);
            auto comp = addEffect(PerformanceEffectType::Compressor);
            if (comp) comp->loadPreset(2);
            break;
        }
        case 6: // Neo-Soul & Rhodes Keys
        {
            auto ph = addEffect(PerformanceEffectType::Phaser);
            if (ph) ph->loadPreset(0);
            auto ch = addEffect(PerformanceEffectType::Chorus);
            if (ch) ch->loadPreset(1);
            auto trem = addEffect(PerformanceEffectType::Tremolo);
            if (trem) trem->loadPreset(0);
            auto amp = addEffect(PerformanceEffectType::AmpCabinet);
            if (amp) amp->loadPreset(1); // Tweed Clean
            break;
        }
        case 7: // Guitar Hero Rig
        {
            auto comp = addEffect(PerformanceEffectType::Compressor);
            if (comp) comp->loadPreset(0);
            auto amp = addEffect(PerformanceEffectType::AmpCabinet);
            if (amp) amp->loadPreset(0); // British Crunch
            auto del = addEffect(PerformanceEffectType::Delay);
            if (del) del->loadPreset(0);
            auto rev = addEffect(PerformanceEffectType::Reverb);
            if (rev) rev->loadPreset(0);
            break;
        }
        case 8: // Funkadelic Synth Bass
        {
            auto wah = addEffect(PerformanceEffectType::AutoWah);
            if (wah) wah->loadPreset(0); // Funk Wah
            auto dist = addEffect(PerformanceEffectType::Distortion);
            if (dist) dist->loadPreset(0);
            auto comp = addEffect(PerformanceEffectType::Compressor);
            if (comp) comp->loadPreset(1);
            break;
        }
        case 9: // 80s Dream Pop
        {
            auto fl = addEffect(PerformanceEffectType::Flanger);
            if (fl) fl->loadPreset(4); // 80s Dimension
            auto ch = addEffect(PerformanceEffectType::Chorus);
            if (ch) ch->loadPreset(0);
            auto del = addEffect(PerformanceEffectType::Delay);
            if (del) del->loadPreset(2);
            auto rev = addEffect(PerformanceEffectType::Reverb);
            if (rev) rev->loadPreset(3);
            break;
        }
        case 10: // Sci-Fi Sound Design
        {
            auto rm = addEffect(PerformanceEffectType::RingModulator);
            if (rm) rm->loadPreset(0);
            auto ps = addEffect(PerformanceEffectType::PitchShifter);
            if (ps) ps->loadPreset(0);
            auto filt = addEffect(PerformanceEffectType::Filter);
            if (filt) filt->loadPreset(1);
            auto del = addEffect(PerformanceEffectType::Delay);
            if (del) del->loadPreset(3);
            break;
        }
        case 11: // Stereo Width & Polish
        {
            auto imager = addEffect(PerformanceEffectType::StereoImager);
            if (imager) imager->loadPreset(0);
            auto eq = addEffect(PerformanceEffectType::ParametricEQ);
            if (eq) eq->loadPreset(0);
            auto rev = addEffect(PerformanceEffectType::Reverb);
            if (rev) rev->loadPreset(1);
            break;
        }
        default:
            break;
    }
}

} // namespace openwav

