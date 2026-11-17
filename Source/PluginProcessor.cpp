#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace openwav
{

OpenWavAudioProcessor::OpenWavAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

OpenWavAudioProcessor::~OpenWavAudioProcessor()
{
    audioEngine.stopAllVoices();
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
        noteRoundRobinCounters.clear();
        lastRandomZoneIndex.clear();
    }
}

PluginFullState OpenWavAudioProcessor::getFullPluginState() const
{
    const juce::ScopedLock sl(stateLock);
    return fullPluginState;
}

void OpenWavAudioProcessor::setFullPluginState(const PluginFullState& s, bool preloadSamples)
{
    {
        const juce::ScopedLock sl(stateLock);
        fullPluginState = s;
        editState = s.edit;
        sampleMapState = s.sampleMap;
        noteRoundRobinCounters.clear();
        lastRandomZoneIndex.clear();
    }

    if (preloadSamples)
    {
        std::vector<juce::File> filesToPreload;
        for (const auto& z : s.sampleMap.zones)
        {
            if (z.filePath.isNotEmpty())
                filesToPreload.push_back(juce::File(z.filePath));
        }
        if (!filesToPreload.empty())
            audioEngine.preloadSampleFiles(filesToPreload);
    }
}

void OpenWavAudioProcessor::handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity)
{
    if (midiNoteNumber < 0 || midiNoteNumber > 127)
        return;

    int velInt = juce::jlimit(0, 127, static_cast<int>(std::round(velocity * 127.0f)));

    SampleMapState currentSampleMap;
    {
        const juce::ScopedLock sl(stateLock);
        currentSampleMap = sampleMapState;
    }

    int mapChannel = (currentSampleMap.midiChannel > 0) ? currentSampleMap.midiChannel : (midiChannel > 0 ? midiChannel : 1);
    bool hasMappedZones = !currentSampleMap.zones.empty();

    if (hasMappedZones)
    {
        lastNoteOnVelocities[static_cast<size_t>(midiNoteNumber)] = velocity;
        bool zoneTriggered = false;

        // 1. Check Sample Map Zones for matching Key and Velocity (Note-On zones only)
        std::vector<const SampleMapZoneState*> matchingZones;
        for (const auto& z : currentSampleMap.zones)
        {
            if (!z.isReleaseTrigger && midiNoteNumber >= z.keyLow && midiNoteNumber <= z.keyHigh
                && velInt >= z.velLow && velInt <= z.velHigh && z.filePath.isNotEmpty())
            {
                matchingZones.push_back(&z);
            }
        }

        if (!matchingZones.empty())
        {
            std::vector<int> distinctRrGroups;
            for (const auto* z : matchingZones)
            {
                if (std::find(distinctRrGroups.begin(), distinctRrGroups.end(), z->roundRobinIndex) == distinctRrGroups.end())
                    distinctRrGroups.push_back(z->roundRobinIndex);
            }
            std::sort(distinctRrGroups.begin(), distinctRrGroups.end());

            int chosenRrGroup = distinctRrGroups[0];
            if (distinctRrGroups.size() > 1 && currentSampleMap.roundRobinMode != 2)
            {
                if (currentSampleMap.roundRobinMode == 1) // Random RR
                {
                    int lastGroup = lastRandomZoneIndex[midiNoteNumber];
                    int pick = juce::Random::getSystemRandom().nextInt(static_cast<int>(distinctRrGroups.size()));
                    if (distinctRrGroups[static_cast<size_t>(pick)] == lastGroup)
                        pick = (pick + 1) % static_cast<int>(distinctRrGroups.size());
                    chosenRrGroup = distinctRrGroups[static_cast<size_t>(pick)];
                    lastRandomZoneIndex[midiNoteNumber] = chosenRrGroup;
                }
                else // Sequential / Cycle RR
                {
                    int count = noteRoundRobinCounters[midiNoteNumber]++;
                    size_t pick = static_cast<size_t>(count) % distinctRrGroups.size();
                    chosenRrGroup = distinctRrGroups[pick];
                }
            }

            for (const auto* chosenZone : matchingZones)
            {
                if (chosenZone != nullptr && chosenZone->roundRobinIndex == chosenRrGroup)
                {
                    juce::File fileToLoad(chosenZone->filePath);
                    float effectiveVelocity = currentSampleMap.computeVelocityResponse(velocity, chosenZone->velocitySensitivity);
                    audioEngine.playZoneVoice(fileToLoad, midiNoteNumber, chosenZone->rootNote, chosenZone->fineTuneCents, chosenZone->gainDb, effectiveVelocity,
                                              chosenZone->attackMs / 1000.0f, chosenZone->decayMs / 1000.0f, chosenZone->sustainLevel, chosenZone->releaseMs / 1000.0f,
                                              audioEngine.isOneShotEnabled(), audioEngine.isLooping());
                    zoneTriggered = true;
                }
            }
        }

        audioEngine.getKeyboardState().noteOn(mapChannel, midiNoteNumber, velocity);
    }
    else
    {
        // Channel 1 or non-sample-map channels: trigger single master sample preview / playback
        audioEngine.triggerNoteOn(midiNoteNumber, velocity);
        audioEngine.getKeyboardState().noteOn(midiChannel > 0 ? midiChannel : 1, midiNoteNumber, velocity);
    }
}

void OpenWavAudioProcessor::handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float /*velocity*/)
{
    if (midiNoteNumber < 0 || midiNoteNumber > 127)
        return;

    SampleMapState currentSampleMap;
    {
        const juce::ScopedLock sl(stateLock);
        currentSampleMap = sampleMapState;
    }

    int mapChannel = (currentSampleMap.midiChannel > 0) ? currentSampleMap.midiChannel : (midiChannel > 0 ? midiChannel : 1);
    bool hasMappedZones = !currentSampleMap.zones.empty();

    if (hasMappedZones)
    {
        audioEngine.stopZoneVoice(midiNoteNumber);

        // Trigger any Release / Note-Off zones matching this key
        const float relVel = juce::jlimit(0.05f, 1.0f, lastNoteOnVelocities[static_cast<size_t>(midiNoteNumber)] > 0.0f
                                                            ? lastNoteOnVelocities[static_cast<size_t>(midiNoteNumber)]
                                                            : 0.8f);
        const int velInt = juce::jlimit(1, 127, static_cast<int>(std::round(relVel * 127.0f)));
        for (const auto& z : currentSampleMap.zones)
        {
            if (z.isReleaseTrigger && midiNoteNumber >= z.keyLow && midiNoteNumber <= z.keyHigh
                && velInt >= z.velLow && velInt <= z.velHigh && z.filePath.isNotEmpty())
            {
                juce::File fileToLoad(z.filePath);
                float effectiveVelocity = currentSampleMap.computeVelocityResponse(relVel, z.velocitySensitivity);
                audioEngine.playZoneVoice(fileToLoad, midiNoteNumber, z.rootNote, z.fineTuneCents, z.gainDb, effectiveVelocity,
                                          z.attackMs / 1000.0f, z.decayMs / 1000.0f, z.sustainLevel, z.releaseMs / 1000.0f,
                                          true, false);
            }
        }

        audioEngine.getKeyboardState().noteOff(mapChannel, midiNoteNumber, 0.0f);
    }
    else
    {
        audioEngine.triggerNoteOff(midiNoteNumber);
        audioEngine.getKeyboardState().noteOff(midiChannel > 0 ? midiChannel : 1, midiNoteNumber, 0.0f);
    }
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

    // Process all incoming MIDI events cleanly (Note On/Off, Sustain CC 64, All Notes/Sound Off)
    for (const auto metadata : midiMessages)
    {
        processIncomingMidiMessage(metadata.getMessage());
    }

    audioEngine.processNextAudioBlock(buffer, midiMessages);
}

void OpenWavAudioProcessor::processIncomingMidiMessage(const juce::MidiMessage& message)
{
    int configuredChannel = 0;
    {
        const juce::ScopedLock sl(stateLock);
        configuredChannel = sampleMapState.midiChannel;
    }

    const int msgCh = message.getChannel();
    if (configuredChannel > 0 && msgCh > 0 && msgCh != configuredChannel)
        return;

    if (message.isNoteOn())
    {
        const int note = message.getNoteNumber();
        if (note >= 0 && note < 128)
        {
            const size_t idx = static_cast<size_t>(note);
            // If re-triggering a note held by sustain pedal, release the old sustained voice first
            if (sustainedNotes[idx] && !physicalNotesHeld[idx])
            {
                sustainedNotes[idx] = false;
                handleNoteOff(nullptr, msgCh, note, 0.0f);
            }
            physicalNotesHeld[idx] = true;
            sustainedNotes[idx] = false;
            noteChannels[idx] = msgCh > 0 ? msgCh : 1;
            handleNoteOn(nullptr, msgCh, note, message.getFloatVelocity());
        }
    }
    else if (message.isNoteOff())
    {
        const int note = message.getNoteNumber();
        if (note >= 0 && note < 128)
        {
            const size_t idx = static_cast<size_t>(note);
            physicalNotesHeld[idx] = false;
            if (sustainPedalDown)
            {
                sustainedNotes[idx] = true;
            }
            else
            {
                sustainedNotes[idx] = false;
                handleNoteOff(nullptr, msgCh, note, message.getFloatVelocity());
            }
        }
    }
    else if (message.isController())
    {
        const int cc = message.getControllerNumber();
        const int val = message.getControllerValue();
        if (cc == 64) // Sustain / Damper Pedal
        {
            const bool nowDown = (val >= 64);
            if (sustainPedalDown && !nowDown)
            {
                sustainPedalDown = false;
                for (int n = 0; n < 128; ++n)
                {
                    const size_t idx = static_cast<size_t>(n);
                    if (sustainedNotes[idx] && !physicalNotesHeld[idx])
                    {
                        sustainedNotes[idx] = false;
                        handleNoteOff(nullptr, noteChannels[idx] > 0 ? noteChannels[idx] : 1, n, 0.0f);
                    }
                }
            }
            else
            {
                sustainPedalDown = nowDown;
            }
        }
        else if (cc == 120 || cc == 123) // All Sound Off / All Notes Off
        {
            sustainPedalDown = false;
            physicalNotesHeld.fill(false);
            sustainedNotes.fill(false);
            audioEngine.stopAllVoices();
            audioEngine.getKeyboardState().allNotesOff(0);
        }
    }
    else if (message.isPitchWheel())
    {
        const float norm = (static_cast<float>(message.getPitchWheelValue()) - 8192.0f) / 8192.0f;
        audioEngine.setPitchBend(juce::jlimit(-2.0f, 2.0f, norm * 2.0f));
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        sustainPedalDown = false;
        physicalNotesHeld.fill(false);
        sustainedNotes.fill(false);
        audioEngine.stopAllVoices();
        audioEngine.getKeyboardState().allNotesOff(0);
    }
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
            setFullPluginState(fullState, true);

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
