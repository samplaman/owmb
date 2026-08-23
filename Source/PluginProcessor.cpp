#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace openwav
{

OpenWavAudioProcessor::OpenWavAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), false)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

OpenWavAudioProcessor::~OpenWavAudioProcessor()
{
}

EditComponentState OpenWavAudioProcessor::getEditState() const
{
    const juce::ScopedLock sl(stateLock);
    return editState;
}

void OpenWavAudioProcessor::setEditState(const EditComponentState& s)
{
    const juce::ScopedLock sl(stateLock);
    editState = s;
}

SampleMapState OpenWavAudioProcessor::getSampleMapState() const
{
    const juce::ScopedLock sl(stateLock);
    return sampleMapState;
}

void OpenWavAudioProcessor::setSampleMapState(const SampleMapState& s)
{
    {
        const juce::ScopedLock sl(stateLock);
        sampleMapState = s;
        fullPluginState.sampleMap = s;
    }
    std::vector<juce::File> filesToPreload;
    for (const auto& z : s.zones)
    {
        if (z.filePath.isNotEmpty())
            filesToPreload.push_back(juce::File(z.filePath));
    }
    if (!filesToPreload.empty())
        audioEngine.preloadSampleFiles(filesToPreload);
}

PluginFullState OpenWavAudioProcessor::getFullPluginState() const
{
    const juce::ScopedLock sl(stateLock);
    return fullPluginState;
}

void OpenWavAudioProcessor::setFullPluginState(const PluginFullState& s)
{
    {
        const juce::ScopedLock sl(stateLock);
        fullPluginState = s;
        editState = s.edit;
        sampleMapState = s.sampleMap;
    }

    std::vector<juce::File> filesToPreload;
    for (const auto& z : s.sampleMap.zones)
    {
        if (z.filePath.isNotEmpty())
            filesToPreload.push_back(juce::File(z.filePath));
    }
    if (!filesToPreload.empty())
        audioEngine.preloadSampleFiles(filesToPreload);
}

void OpenWavAudioProcessor::handleNoteOn(juce::MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float velocity)
{
    if (midiNoteNumber < 0 || midiNoteNumber > 127)
        return;

    int velInt = juce::jlimit(0, 127, static_cast<int>(velocity * 127.0f));

    SampleMapState currentSampleMap;
    {
        const juce::ScopedLock sl(stateLock);
        currentSampleMap = sampleMapState;
    }

    bool hasMappedZones = !currentSampleMap.zones.empty();
    bool zoneTriggered = false;

    // 1. Check Sample Map Zones
    for (const auto& z : currentSampleMap.zones)
    {
        if (midiNoteNumber >= z.keyLow && midiNoteNumber <= z.keyHigh && velInt >= z.velLow && velInt <= z.velHigh)
        {
            if (z.filePath.isNotEmpty())
            {
                juce::File fileToLoad(z.filePath);
                audioEngine.playZoneVoice(fileToLoad, midiNoteNumber, z.rootNote, z.fineTuneCents, z.gainDb, velocity,
                                          z.attackMs / 1000.0f, z.decayMs / 1000.0f, z.sustainLevel, z.releaseMs / 1000.0f,
                                          audioEngine.isOneShotEnabled(), audioEngine.isLooping());
                zoneTriggered = true;
                break;
            }
        }
    }

    // 2. Fallback: Trigger single master sample only if NO zones are mapped and none triggered
    if (!hasMappedZones && !zoneTriggered)
    {
        audioEngine.triggerNoteOn(midiNoteNumber, velocity);
    }
}

void OpenWavAudioProcessor::handleNoteOff(juce::MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float /*velocity*/)
{
    if (midiNoteNumber < 0 || midiNoteNumber > 127)
        return;

    audioEngine.stopZoneVoice(midiNoteNumber);
    audioEngine.triggerNoteOff(midiNoteNumber);
}

void OpenWavAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    audioEngine.prepareToPlay(sampleRate, samplesPerBlock);
}

void OpenWavAudioProcessor::releaseResources()
{
    audioEngine.releaseResources();
}

bool OpenWavAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled()
        && layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void OpenWavAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    if (!audioEngine.isMidiInputEnabled())
    {
        midiMessages.clear();
    }

    bool hostPlaying = false;
    double hostBpm = 120.0;
    double hostPosSec = 0.0;
    double hostPpq = 0.0;

    if (auto* playHead = getPlayHead())
    {
        if (auto posInfo = playHead->getPosition())
        {
            hostPlaying = posInfo->getIsPlaying();
            if (auto bpmOpt = posInfo->getBpm())
                hostBpm = *bpmOpt;
            if (auto timeOpt = posInfo->getTimeInSeconds())
                hostPosSec = *timeOpt;
            if (auto ppqOpt = posInfo->getPpqPosition())
                hostPpq = *ppqOpt;
        }
    }
    audioEngine.setHostTransportState(hostPlaying, hostBpm, hostPosSec, hostPpq);

    // Process all incoming DAW MIDI note events directly
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        if (message.isNoteOn())
        {
            handleNoteOn(nullptr, message.getChannel(), message.getNoteNumber(), message.getFloatVelocity());
        }
        else if (message.isNoteOff())
        {
            handleNoteOff(nullptr, message.getChannel(), message.getNoteNumber(), message.getFloatVelocity());
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            audioEngine.stopAllVoices();
        }
    }

    audioEngine.processNextAudioBlock(buffer, midiMessages);
}

juce::AudioProcessorEditor* OpenWavAudioProcessor::createEditor()
{
    return new OpenWavAudioProcessorEditor(*this);
}

void OpenWavAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto* ed = dynamic_cast<OpenWavAudioProcessorEditor*>(getActiveEditor()))
    {
        ed->saveStateToProcessor();
    }

    PluginFullState fullState;
    {
        const juce::ScopedLock sl(stateLock);
        fullState = fullPluginState;
    }

    juce::var stateVar = fullState.toVar();
    juce::String jsonString = juce::JSON::toString(stateVar, false);
    destData.replaceAll(jsonString.toRawUTF8(), jsonString.getNumBytesAsUTF8());

    dbManager.saveToFile();
}

void OpenWavAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data != nullptr && sizeInBytes > 0)
    {
        juce::String jsonString = juce::String::createStringFromData(data, sizeInBytes);
        juce::var stateVar = juce::JSON::parse(jsonString);
        if (stateVar.isObject())
        {
            auto fullState = PluginFullState::fromVar(stateVar);
            setFullPluginState(fullState);

            if (auto* ed = dynamic_cast<OpenWavAudioProcessorEditor*>(getActiveEditor()))
            {
                ed->restoreStateFromProcessor();
            }
        }
    }

    dbManager.loadFromFile();
}

} // namespace openwav

// JUCE Plugin Entrypoint Factory
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new openwav::OpenWavAudioProcessor();
}
