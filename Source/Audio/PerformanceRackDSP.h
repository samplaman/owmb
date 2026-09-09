#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_core/juce_core.h>
 #include <juce_graphics/juce_graphics.h>
 #include <juce_audio_basics/juce_audio_basics.h>
 #include <juce_dsp/juce_dsp.h>
#endif

#include <vector>
#include <memory>
#include <atomic>
#include <map>
#include <array>
#include <cmath>
#include <algorithm>

namespace openwav
{

enum class PerformanceEffectType : int
{
    Reverb = 0,
    Delay,
    Filter,
    Distortion,
    Chorus,
    Compressor,
    Bitcrusher,
    ParametricEQ,
    PitchShifter,
    Tremolo,
    Phaser,
    Flanger,
    StereoImager,
    AutoWah,
    RingModulator,
    AmpCabinet
};

struct EffectParamInfo
{
    juce::String id;
    juce::String name;
    float minValue { 0.0f };
    float maxValue { 1.0f };
    float defaultValue { 0.5f };
    juce::String suffix;
    bool isInt { false };
    juce::StringArray choiceLabels;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Base Rack Effect
// ─────────────────────────────────────────────────────────────────────────────
class RackEffectBase
{
public:
    virtual ~RackEffectBase() = default;

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;
    virtual void process(juce::AudioBuffer<float>& buffer) = 0;

    virtual PerformanceEffectType getType() const = 0;
    virtual juce::String getName() const = 0;
    virtual juce::Colour getAccentColour() const = 0;

    virtual std::vector<EffectParamInfo> getParameterInfos() const = 0;
    virtual void setParameter(const juce::String& paramId, float value) = 0;
    virtual float getParameter(const juce::String& paramId) const = 0;

    virtual juce::StringArray getPresetNames() const = 0;
    virtual void loadPreset(int presetIndex) = 0;

    void setBypassed(bool b) { isBypassed.store(b, std::memory_order_relaxed); }
    bool getBypassed() const { return isBypassed.load(std::memory_order_relaxed); }

    juce::String getId() const { return instanceId; }
    void setId(const juce::String& id) { instanceId = id; }

    virtual float getTelemetryMeter() const { return 0.0f; }

protected:
    std::atomic<bool> isBypassed { false };
    juce::String instanceId { juce::Uuid().toString() };
    double currentSampleRate { 44100.0 };
};

// ─────────────────────────────────────────────────────────────────────────────
//  1. Studio Reverb
// ─────────────────────────────────────────────────────────────────────────────
class RackReverb : public RackEffectBase
{
public:
    RackReverb();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::Reverb; }
    juce::String getName() const override { return "Studio Reverb"; }
    juce::Colour getAccentColour() const override { return juce::Colour(155, 89, 182); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

private:
    juce::Reverb reverb;
    juce::Reverb::Parameters params;
    std::atomic<float> roomSize { 0.6f };
    std::atomic<float> damping { 0.4f };
    std::atomic<float> width { 1.0f };
    std::atomic<float> mix { 0.35f };
    std::atomic<float> preDelayMs { 10.0f };

    juce::AudioBuffer<float> dryCopy;
    std::vector<float> preDelayLineL, preDelayLineR;
    size_t preDelayWritePos { 0 };
};

// ─────────────────────────────────────────────────────────────────────────────
//  2. Stereo Tape / Ping-Pong Delay
// ─────────────────────────────────────────────────────────────────────────────
class RackDelay : public RackEffectBase
{
public:
    RackDelay();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::Delay; }
    juce::String getName() const override { return "Stereo Delay"; }
    juce::Colour getAccentColour() const override { return juce::Colour(0, 210, 255); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

private:
    std::atomic<float> timeMs { 350.0f };
    std::atomic<float> feedback { 0.45f };
    std::atomic<float> dampFreqHz { 4500.0f };
    std::atomic<float> pingPong { 0.5f };
    std::atomic<float> mix { 0.35f };

    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    size_t writePos { 0 };
    float dampFilterL { 0.0f };
    float dampFilterR { 0.0f };
    juce::AudioBuffer<float> dryCopy;
};

// ─────────────────────────────────────────────────────────────────────────────
//  3. Multi-Mode SVF Filter
// ─────────────────────────────────────────────────────────────────────────────
class RackFilter : public RackEffectBase
{
public:
    RackFilter();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::Filter; }
    juce::String getName() const override { return "Multi-Mode Filter"; }
    juce::Colour getAccentColour() const override { return juce::Colour(46, 204, 113); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

private:
    std::atomic<float> cutoffHz { 1800.0f };
    std::atomic<float> resonance { 1.5f };
    std::atomic<float> mode { 0.0f }; // 0: LowPass, 1: HighPass, 2: BandPass, 3: Notch
    std::atomic<float> drive { 1.0f };
    std::atomic<float> mix { 1.0f };

    // State-Variable Filter states for stereo
    float s1_L { 0.0f }, s2_L { 0.0f };
    float s1_R { 0.0f }, s2_R { 0.0f };
    juce::AudioBuffer<float> dryCopy;
};

// ─────────────────────────────────────────────────────────────────────────────
//  4. Analog Saturation & Overdrive
// ─────────────────────────────────────────────────────────────────────────────
class RackDistortion : public RackEffectBase
{
public:
    RackDistortion();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::Distortion; }
    juce::String getName() const override { return "Analog Saturation"; }
    juce::Colour getAccentColour() const override { return juce::Colour(243, 156, 18); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

    float getTelemetryMeter() const override { return currentDriveMeter.load(std::memory_order_relaxed); }

private:
    std::atomic<float> driveDb { 12.0f };
    std::atomic<float> tone { 0.5f };
    std::atomic<float> type { 0.0f }; // 0: Tape, 1: Tube, 2: Overdrive, 3: Fuzz, 4: Hard Clip
    std::atomic<float> outGainDb { 0.0f };
    std::atomic<float> mix { 0.8f };

    mutable std::atomic<float> currentDriveMeter { 0.0f };
    float toneFilterL { 0.0f };
    float toneFilterR { 0.0f };
    juce::AudioBuffer<float> dryCopy;
};

// ─────────────────────────────────────────────────────────────────────────────
//  5. Stereo Chorus & Ensemble
// ─────────────────────────────────────────────────────────────────────────────
class RackChorus : public RackEffectBase
{
public:
    RackChorus();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::Chorus; }
    juce::String getName() const override { return "Stereo Chorus"; }
    juce::Colour getAccentColour() const override { return juce::Colour(233, 30, 99); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

private:
    std::atomic<float> rateHz { 1.2f };
    std::atomic<float> depthMs { 4.0f };
    std::atomic<float> feedback { 0.25f };
    std::atomic<float> stereoWidth { 0.85f };
    std::atomic<float> mix { 0.5f };

    std::vector<float> delayL, delayR;
    size_t writePos { 0 };
    double lfoPhaseL { 0.0 }, lfoPhaseR { juce::MathConstants<double>::halfPi };
    juce::AudioBuffer<float> dryCopy;
};

// ─────────────────────────────────────────────────────────────────────────────
//  6. VCA Studio Compressor
// ─────────────────────────────────────────────────────────────────────────────
class RackCompressor : public RackEffectBase
{
public:
    RackCompressor();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::Compressor; }
    juce::String getName() const override { return "Studio Compressor"; }
    juce::Colour getAccentColour() const override { return juce::Colour(231, 76, 60); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

    float getTelemetryMeter() const override { return currentGainReductionDb.load(std::memory_order_relaxed); }

private:
    std::atomic<float> thresholdDb { -14.0f };
    std::atomic<float> ratio { 4.0f };
    std::atomic<float> attackMs { 12.0f };
    std::atomic<float> releaseMs { 120.0f };
    std::atomic<float> makeupDb { 4.0f };
    std::atomic<float> mix { 1.0f };

    float envDb { -96.0f };
    mutable std::atomic<float> currentGainReductionDb { 0.0f };
    juce::AudioBuffer<float> dryCopy;
};

// ─────────────────────────────────────────────────────────────────────────────
//  7. Vintage Bitcrusher & Lo-Fi
// ─────────────────────────────────────────────────────────────────────────────
class RackBitcrusher : public RackEffectBase
{
public:
    RackBitcrusher();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::Bitcrusher; }
    juce::String getName() const override { return "Vintage Bitcrusher"; }
    juce::Colour getAccentColour() const override { return juce::Colour(241, 196, 15); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

private:
    std::atomic<float> bitDepth { 10.0f };
    std::atomic<float> downsample { 3.0f };
    std::atomic<float> noiseAmount { 0.05f };
    std::atomic<float> postFilterHz { 6000.0f };
    std::atomic<float> mix { 0.7f };

    float holdL { 0.0f }, holdR { 0.0f };
    float sampleCounter { 0.0f };
    float postFilterStateL { 0.0f }, postFilterStateR { 0.0f };
    uint32_t rngState { 123456789 };
    juce::AudioBuffer<float> dryCopy;
};

// ─────────────────────────────────────────────────────────────────────────────
//  8. 3-Band Parametric EQ
// ─────────────────────────────────────────────────────────────────────────────
class RackParametricEQ : public RackEffectBase
{
public:
    RackParametricEQ();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::ParametricEQ; }
    juce::String getName() const override { return "3-Band EQ"; }
    juce::Colour getAccentColour() const override { return juce::Colour(52, 152, 219); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

private:
    std::atomic<float> lowGainDb { 0.0f };
    std::atomic<float> lowFreqHz { 120.0f };
    std::atomic<float> midGainDb { 0.0f };
    std::atomic<float> midFreqHz { 1200.0f };
    std::atomic<float> midQ { 1.2f };
    std::atomic<float> highGainDb { 0.0f };
    std::atomic<float> highFreqHz { 6500.0f };

    struct FilterState { float x1 { 0 }, x2 { 0 }, y1 { 0 }, y2 { 0 }; };
    FilterState lowStateL, lowStateR;
    FilterState midStateL, midStateR;
    FilterState highStateL, highStateR;
};

// ─────────────────────────────────────────────────────────────────────────────
//  9. Pitch Shifter & Harmonizer
// ─────────────────────────────────────────────────────────────────────────────
class RackPitchShifter : public RackEffectBase
{
public:
    RackPitchShifter();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::PitchShifter; }
    juce::String getName() const override { return "Pitch Harmonizer"; }
    juce::Colour getAccentColour() const override { return juce::Colour(142, 68, 173); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

private:
    std::atomic<float> semitones { 0.0f };
    std::atomic<float> fineTuneCents { 0.0f };
    std::atomic<float> mix { 0.5f };

    std::vector<float> delayBufferL, delayBufferR;
    size_t writePos { 0 };
    double readHead1 { 0.0 }, readHead2 { 0.0 };
    float grainSizeSamples { 2048.0f };
    juce::AudioBuffer<float> dryCopy;
};

// ─────────────────────────────────────────────────────────────────────────────
//  10. Tremolo & Auto-Panner
// ─────────────────────────────────────────────────────────────────────────────
class RackTremolo : public RackEffectBase
{
public:
    RackTremolo();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::Tremolo; }
    juce::String getName() const override { return "Tremolo & Panner"; }
    juce::Colour getAccentColour() const override { return juce::Colour(26, 188, 156); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

private:
    std::atomic<float> rateHz { 4.0f };
    std::atomic<float> depth { 0.65f };
    std::atomic<float> shape { 0.0f }; // 0: Sine, 1: Triangle, 2: Square
    std::atomic<float> stereoPhaseDeg { 90.0f }; // 0 = Tremolo, 180 = Auto-Pan
    std::atomic<float> mix { 1.0f };

    double phase { 0.0 };
    juce::AudioBuffer<float> dryCopy;
};

// ─────────────────────────────────────────────────────────────────────────────
//  11. Stereo Analog Phaser
// ─────────────────────────────────────────────────────────────────────────────
class RackPhaser : public RackEffectBase
{
public:
    RackPhaser();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::Phaser; }
    juce::String getName() const override { return "Analog Phaser"; }
    juce::Colour getAccentColour() const override { return juce::Colour(230, 126, 34); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

private:
    std::atomic<float> rateHz { 0.5f };
    std::atomic<float> depth { 0.75f };
    std::atomic<float> feedback { 0.5f };
    std::atomic<float> poles { 1.0f }; // 0: 4-stage, 1: 8-stage, 2: 12-stage
    std::atomic<float> stereoPhaseDeg { 90.0f };
    std::atomic<float> mix { 0.5f };

    double lfoPhase { 0.0 };
    std::array<std::array<float, 12>, 2> allpassStates;
    std::array<float, 2> feedbackBuffer { 0.0f, 0.0f };
    juce::AudioBuffer<float> dryCopy;
};

// ─────────────────────────────────────────────────────────────────────────────
//  12. Tape & BBD Stereo Flanger
// ─────────────────────────────────────────────────────────────────────────────
class RackFlanger : public RackEffectBase
{
public:
    RackFlanger();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::Flanger; }
    juce::String getName() const override { return "Tape Flanger"; }
    juce::Colour getAccentColour() const override { return juce::Colour(52, 152, 219); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

private:
    std::atomic<float> rateHz { 0.25f };
    std::atomic<float> depth { 0.8f };
    std::atomic<float> delayMs { 2.5f };
    std::atomic<float> feedback { 0.65f };
    std::atomic<float> stereoSpread { 0.7f };
    std::atomic<float> mix { 0.5f };

    double lfoPhase { 0.0 };
    std::vector<float> delayLineL, delayLineR;
    size_t writePos { 0 };
    juce::AudioBuffer<float> dryCopy;
};

// ─────────────────────────────────────────────────────────────────────────────
//  13. Stereo Width & Spatial Imager
// ─────────────────────────────────────────────────────────────────────────────
class RackStereoImager : public RackEffectBase
{
public:
    RackStereoImager();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::StereoImager; }
    juce::String getName() const override { return "Stereo Imager"; }
    juce::Colour getAccentColour() const override { return juce::Colour(46, 204, 113); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

private:
    std::atomic<float> width { 1.35f }; // 0 = Mono, 1.0 = Normal, 2.0 = Ultra-Wide
    std::atomic<float> monoBassCutoffHz { 120.0f };
    std::atomic<float> balance { 0.0f }; // -1.0 to +1.0
    std::atomic<float> sideGainDb { 0.0f };
    std::atomic<float> midGainDb { 0.0f };

    // 1-pole high-pass filter state for side channel bass collapse
    float sideHpState { 0.0f };
};

// ─────────────────────────────────────────────────────────────────────────────
//  14. Dynamic Auto-Wah & Envelope Filter
// ─────────────────────────────────────────────────────────────────────────────
class RackAutoWah : public RackEffectBase
{
public:
    RackAutoWah();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::AutoWah; }
    juce::String getName() const override { return "Dynamic Auto-Wah"; }
    juce::Colour getAccentColour() const override { return juce::Colour(241, 196, 15); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

private:
    std::atomic<float> sensitivity { 0.6f };
    std::atomic<float> depth { 0.75f };
    std::atomic<float> resonance { 4.5f };
    std::atomic<float> baseCutoffHz { 350.0f };
    std::atomic<float> attackMs { 12.0f };
    std::atomic<float> releaseMs { 120.0f };
    std::atomic<float> mode { 0.0f }; // 0: Lowpass, 1: Bandpass, 2: Highpass
    std::atomic<float> mix { 0.85f };

    float envFollower { 0.0f };
    float s1[2] { 0.0f, 0.0f };
    float s2[2] { 0.0f, 0.0f };
    juce::AudioBuffer<float> dryCopy;
};

// ─────────────────────────────────────────────────────────────────────────────
//  15. Metallic Ring Modulator
// ─────────────────────────────────────────────────────────────────────────────
class RackRingModulator : public RackEffectBase
{
public:
    RackRingModulator();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::RingModulator; }
    juce::String getName() const override { return "Ring Modulator"; }
    juce::Colour getAccentColour() const override { return juce::Colour(155, 89, 182); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

private:
    std::atomic<float> carrierFreqHz { 440.0f };
    std::atomic<float> waveShape { 0.0f }; // 0: Sine, 1: Triangle, 2: Square
    std::atomic<float> lfoRateHz { 0.0f };
    std::atomic<float> lfoDepth { 0.0f };
    std::atomic<float> mix { 0.6f };

    double carrierPhase { 0.0 };
    double lfoPhase { 0.0 };
    juce::AudioBuffer<float> dryCopy;
};

// ─────────────────────────────────────────────────────────────────────────────
//  16. Vintage Amp & Cabinet Simulator
// ─────────────────────────────────────────────────────────────────────────────
class RackAmpCabinet : public RackEffectBase
{
public:
    RackAmpCabinet();
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

    PerformanceEffectType getType() const override { return PerformanceEffectType::AmpCabinet; }
    juce::String getName() const override { return "Amp & Cabinet"; }
    juce::Colour getAccentColour() const override { return juce::Colour(192, 57, 43); }

    std::vector<EffectParamInfo> getParameterInfos() const override;
    void setParameter(const juce::String& paramId, float value) override;
    float getParameter(const juce::String& paramId) const override;

    juce::StringArray getPresetNames() const override;
    void loadPreset(int presetIndex) override;

private:
    std::atomic<float> drive { 0.45f };
    std::atomic<float> bass { 0.5f };
    std::atomic<float> mid { 0.5f };
    std::atomic<float> treble { 0.5f };
    std::atomic<float> presence { 0.5f };
    std::atomic<float> cabinetType { 0.0f }; // 0: British 4x12, 1: Tweed 1x12, 2: Boutique 2x12, 3: Ampeg Bass 8x10, 4: Radio Box
    std::atomic<float> outputGainDb { -2.0f };
    std::atomic<float> mix { 1.0f };

    float sBass[2] { 0.0f, 0.0f };
    float sTreble[2] { 0.0f, 0.0f };
    float sCab[2] { 0.0f, 0.0f };
    juce::AudioBuffer<float> dryCopy;
};

// ─────────────────────────────────────────────────────────────────────────────
//  PerformanceRackDSP: Lock-Free Multi-Effect Signal Chain Manager
// ─────────────────────────────────────────────────────────────────────────────
class PerformanceRackDSP
{
public:
    PerformanceRackDSP();
    ~PerformanceRackDSP() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    // Rack Chain Operations (Called from UI thread, synchronized for audio thread)
    std::shared_ptr<RackEffectBase> addEffect(PerformanceEffectType type);
    void removeEffect(int index);
    void moveEffect(int fromIndex, int toIndex);
    void clearEffects();

    void setMasterBypassed(bool b) { masterBypassed.store(b, std::memory_order_relaxed); }
    bool isMasterBypassed() const { return masterBypassed.load(std::memory_order_relaxed); }

    void setMasterGain(float gain) { masterGain.store(juce::jlimit(0.0f, 2.0f, gain), std::memory_order_relaxed); }
    float getMasterGain() const { return masterGain.load(std::memory_order_relaxed); }

    void loadRackTemplate(int templateIndex);
    juce::StringArray getRackTemplateNames() const;

    int getNumEffects() const;
    std::shared_ptr<RackEffectBase> getEffect(int index) const;
    std::vector<std::shared_ptr<RackEffectBase>> getEffectsSnapshot() const;

    static std::shared_ptr<RackEffectBase> createEffect(PerformanceEffectType type);

private:
    void publishNewChain(const std::vector<std::shared_ptr<RackEffectBase>>& newChain);

    double currentSampleRate { 44100.0 };
    int currentBlockSize { 512 };

    std::atomic<bool> masterBypassed { false };
    std::atomic<float> masterGain { 1.0f };

    // Double-buffered lock-free effect chain for audio thread
    mutable juce::SpinLock chainLock;
    std::vector<std::shared_ptr<RackEffectBase>> activeChain;
};

} // namespace openwav
