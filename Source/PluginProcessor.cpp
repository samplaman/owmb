#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

namespace openwav
{

namespace
{
SampleEditSettings makeZoneEditSettings(const SampleMapZoneState& z)
{
    SampleEditSettings es;
    es.sampleStartRatio = z.sampleStartRatio;
    es.sampleEndRatio = z.sampleEndRatio;
    es.loopInRatio = z.loopInRatio;
    es.loopOutRatio = z.loopOutRatio;
    es.loopCrossfadeMs = z.loopCrossfadeMs;
    es.fadeInMs = z.fadeInMs;
    es.fadeOutMs = z.fadeOutMs;
    es.fadeInCurveType = z.fadeInCurveType;
    es.fadeOutCurveType = z.fadeOutCurveType;
    es.loopMarkersSet = z.loopMarkersSet;
    es.isLooping = z.isLooping;
    return es;
}
} // namespace

OpenWavAudioProcessor::OpenWavAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Out 1-2 (Main)", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Out 3-4", juce::AudioChannelSet::stereo(), false)
                     .withOutput("Out 5-6", juce::AudioChannelSet::stereo(), false)
                     .withOutput("Out 7-8", juce::AudioChannelSet::stereo(), false)
                     .withOutput("Out 9-10", juce::AudioChannelSet::stereo(), false)
                     .withOutput("Out 11-12", juce::AudioChannelSet::stereo(), false)
                     .withOutput("Out 13-14", juce::AudioChannelSet::stereo(), false)
                     .withOutput("Out 15-16", juce::AudioChannelSet::stereo(), false))
{
    audioEngine.onStopped = [this] { stopMidiFilePlayback(false); };
    audioEngine.onMidiFileAudition = [this](const juce::File& f) { loadAndPlayMidiFile(f); };
    audioEngine.isMidiFileCurrentlyPlaying = [this](const juce::File& f) {
        if (f == juce::File())
            return isMidiFilePlaying();
        return isMidiFilePlaying() && getLoadedMidiFilePath() == f.getFullPathName();
    };
    audioEngine.getMidiAuditionProgressRatio = [this]() {
        const double dur = getMidiFileDurationSeconds();
        if (!isMidiFilePlaying() || dur <= 0.0)
            return 0.0;
        return juce::jlimit(0.0, 1.0, getMidiPlaybackTimeSeconds() / dur);
    };
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
        const int activeIdx = fullPluginState.samplerRack.activeEditSlotIndex;
        if (activeIdx >= 0 && activeIdx < static_cast<int>(fullPluginState.samplerRack.slots.size()))
        {
            auto& activeSlot = fullPluginState.samplerRack.slots[static_cast<size_t>(activeIdx)];
            activeSlot.sampleMap = s;
            activeSlot.midiChannel = juce::jlimit(0, 16, s.midiChannel);
            activeSlot.outputBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, s.outputBus);
            activeSlot.triggerOnNoteOff = s.triggerOnNoteOff;
            activeSlot.sampleMap.midiChannel = activeSlot.midiChannel;
            activeSlot.sampleMap.outputBus = activeSlot.outputBus;
            activeSlot.sampleMap.triggerOnNoteOff = activeSlot.triggerOnNoteOff;
        }
        noteRoundRobinCounters.clear();
        lastRandomZoneIndex.clear();
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

SamplerRackState OpenWavAudioProcessor::getSamplerRackState() const
{
    const juce::ScopedLock sl(stateLock);
    return fullPluginState.samplerRack;
}

void OpenWavAudioProcessor::setSamplerRackState(const SamplerRackState& r, bool preloadSamples)
{
    std::vector<juce::File> filesToPreload;
    {
        const juce::ScopedLock sl(stateLock);
        fullPluginState.samplerRack = r;
        const int activeIdx = r.activeEditSlotIndex;
        if (activeIdx >= 0 && activeIdx < static_cast<int>(r.slots.size()))
        {
            sampleMapState = r.slots[static_cast<size_t>(activeIdx)].sampleMap;
            sampleMapState.midiChannel = juce::jlimit(0, 16, r.slots[static_cast<size_t>(activeIdx)].midiChannel);
            sampleMapState.outputBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, r.slots[static_cast<size_t>(activeIdx)].outputBus);
            sampleMapState.triggerOnNoteOff = r.slots[static_cast<size_t>(activeIdx)].triggerOnNoteOff;
            fullPluginState.sampleMap = sampleMapState;
        }
        rackNoteRoundRobinCounters.clear();
        rackLastRandomZoneIndex.clear();

        if (preloadSamples)
        {
            for (const auto& slItem : r.slots)
            {
                for (const auto& z : slItem.sampleMap.zones)
                {
                    if (z.filePath.isNotEmpty())
                        filesToPreload.push_back(juce::File(z.filePath));
                }
            }
        }
    }

    if (!filesToPreload.empty())
        audioEngine.preloadSampleFiles(filesToPreload);
}

void OpenWavAudioProcessor::clearAllHeldMidiState()
{
    sustainPedalDown.fill(false);
    for (size_t c = 0; c < 16; ++c)
    {
        physicalNotesHeld[c].fill(false);
        sustainedNotes[c].fill(false);
    }
}

MultiOutMixerState OpenWavAudioProcessor::getMixerState() const
{
    const juce::ScopedLock sl(stateLock);
    return fullPluginState.mixer;
}

void OpenWavAudioProcessor::setMixerState(const MultiOutMixerState& m)
{
    {
        const juce::ScopedLock sl(stateLock);
        fullPluginState.mixer = m;
    }
    audioEngine.setDefaultSamplerOutputBus(m.defaultSamplerOutputBus);
    audioEngine.setFoldInactiveBusesToMain(m.foldInactiveBusesToMain);
    audioEngine.setMasterMixerGainDb(m.masterGainDb);
    audioEngine.setMasterMixerPan(m.masterPan);
    audioEngine.setMasterMixerMute(m.masterMute);
    for (int i = 0; i < AudioEngine::MaxOutputBuses; ++i)
    {
        const auto& st = m.strips[static_cast<size_t>(i)];
        audioEngine.setBusGainDb(i, st.gainDb);
        audioEngine.setBusPan(i, st.pan);
        audioEngine.setBusMute(i, st.mute);
        audioEngine.setBusSolo(i, st.solo);
        audioEngine.setBusSendToFxRack(i, st.sendToFxRack);
    }
}

bool OpenWavAudioProcessor::isPluginWrapper() const
{
    return wrapperType == wrapperType_VST3
        || wrapperType == wrapperType_AudioUnit
        || wrapperType == wrapperType_AudioUnitv3
        || wrapperType == wrapperType_VST
        || wrapperType == wrapperType_AAX
        || wrapperType == wrapperType_LV2;
}

juce::String OpenWavAudioProcessor::getWrapperTypeDescription() const
{
    switch (wrapperType)
    {
        case wrapperType_VST3:        return "VST3 Plugin";
        case wrapperType_AudioUnit:   return "AU Plugin";
        case wrapperType_AudioUnitv3: return "AUv3 Plugin";
        case wrapperType_VST:         return "VST2 Plugin";
        case wrapperType_AAX:         return "AAX Plugin";
        case wrapperType_LV2:         return "LV2 Plugin";
        case wrapperType_Standalone:  return "Standalone App";
        default:                      return "Plugin";
    }
}

int OpenWavAudioProcessor::getActiveOutputBusCount() const
{
    int active = 0;
    const int total = getBusCount(false);
    for (int i = 0; i < total; ++i)
    {
        if (const auto* bus = getBus(false, i))
        {
            if (bus->isEnabled() && bus->getNumberOfChannels() > 0)
                ++active;
        }
    }
    return std::max(1, active);
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
        rackNoteRoundRobinCounters.clear();
        rackLastRandomZoneIndex.clear();
    }

    setMixerState(s.mixer);

    if (preloadSamples)
    {
        std::vector<juce::File> filesToPreload;
        for (const auto& z : s.sampleMap.zones)
        {
            if (z.filePath.isNotEmpty())
                filesToPreload.push_back(juce::File(z.filePath));
        }
        for (const auto& slItem : s.samplerRack.slots)
        {
            for (const auto& z : slItem.sampleMap.zones)
            {
                if (z.filePath.isNotEmpty())
                    filesToPreload.push_back(juce::File(z.filePath));
            }
        }
        if (!filesToPreload.empty())
            audioEngine.preloadSampleFiles(filesToPreload);
    }
}

void OpenWavAudioProcessor::handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity)
{
    triggerNoteOnInternal(midiChannel, midiNoteNumber, velocity, false);
}

bool OpenWavAudioProcessor::triggerSingleRackSlotNoteOn(int slotIndex, const SamplerRackSlotState& slot, int midiNoteNumber, float velocity, bool isInternalMidiFile)
{
    const auto& map = slot.sampleMap;
    if (map.zones.empty())
        return false;

    const int mappedNote = juce::jlimit(0, 127, midiNoteNumber + slot.transposeSemitones);
    const int velInt = juce::jlimit(1, 127, static_cast<int>(std::round(velocity * 127.0f)));

    std::vector<const SampleMapZoneState*> matchingZones;
    for (const auto& z : map.zones)
    {
        if (!z.isReleaseTrigger && mappedNote >= z.keyLow && mappedNote <= z.keyHigh
            && velInt >= z.velLow && velInt <= z.velHigh && z.filePath.isNotEmpty())
        {
            matchingZones.push_back(&z);
        }
    }

    if (matchingZones.empty() && isInternalMidiFile && map.zones.size() == 1
        && map.pitchTrackingEnabled && !map.zones[0].isReleaseTrigger
        && map.zones[0].filePath.isNotEmpty())
    {
        matchingZones.push_back(&map.zones[0]);
    }

    if (matchingZones.empty())
        return false;

    std::vector<int> distinctRrGroups;
    for (const auto* z : matchingZones)
    {
        if (std::find(distinctRrGroups.begin(), distinctRrGroups.end(), z->roundRobinIndex) == distinctRrGroups.end())
            distinctRrGroups.push_back(z->roundRobinIndex);
    }
    std::sort(distinctRrGroups.begin(), distinctRrGroups.end());

    int chosenRrGroup = distinctRrGroups[0];
    if (distinctRrGroups.size() > 1 && map.roundRobinMode != 2)
    {
        const auto rrKey = std::make_pair(slotIndex, mappedNote);
        if (map.roundRobinMode == 1)
        {
            int lastGroup = 0;
            {
                const juce::ScopedLock sl(stateLock);
                lastGroup = rackLastRandomZoneIndex[rrKey];
            }
            int pick = juce::Random::getSystemRandom().nextInt(static_cast<int>(distinctRrGroups.size()));
            if (distinctRrGroups[static_cast<size_t>(pick)] == lastGroup)
                pick = (pick + 1) % static_cast<int>(distinctRrGroups.size());
            chosenRrGroup = distinctRrGroups[static_cast<size_t>(pick)];
            {
                const juce::ScopedLock sl(stateLock);
                rackLastRandomZoneIndex[rrKey] = chosenRrGroup;
            }
        }
        else
        {
            int count = 0;
            {
                const juce::ScopedLock sl(stateLock);
                count = rackNoteRoundRobinCounters[rrKey]++;
            }
            chosenRrGroup = distinctRrGroups[static_cast<size_t>(count) % distinctRrGroups.size()];
        }
    }

    bool triggered = false;
    for (const auto* chosenZone : matchingZones)
    {
        if (chosenZone != nullptr && chosenZone->roundRobinIndex == chosenRrGroup)
        {
            juce::File fileToLoad(chosenZone->filePath);
            const float effectiveVelocity = map.computeVelocityResponse(velocity, chosenZone->velocitySensitivity);
            const int targetBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, slot.outputBus);
            const float combinedGainDb = chosenZone->gainDb + slot.gainDb;
            const int adjustedRoot = chosenZone->rootNote - (mappedNote - midiNoteNumber);

            const SampleEditSettings zoneEdit = makeZoneEditSettings(*chosenZone);
            audioEngine.playZoneVoice(fileToLoad, midiNoteNumber, adjustedRoot, chosenZone->fineTuneCents,
                                      combinedGainDb, effectiveVelocity,
                                      chosenZone->attackMs / 1000.0f, chosenZone->decayMs / 1000.0f,
                                      chosenZone->sustainLevel, chosenZone->releaseMs / 1000.0f,
                                      isInternalMidiFile ? false : map.oneShotEnabled,
                                      chosenZone->isLooping || (!isInternalMidiFile && map.loopingEnabled),
                                      false, targetBus, slotIndex, slot.pan,
                                      &zoneEdit,
                                      map.pitchTrackingEnabled ? 1 : 0,
                                      map.randomLoopPosition ? 1 : 0,
                                      true);
            triggered = true;
        }
    }
    return triggered;
}

void OpenWavAudioProcessor::triggerSingleRackSlotNoteOff(int slotIndex, const SamplerRackSlotState& slot, int midiNoteNumber, float relVel)
{
    audioEngine.stopZoneVoice(midiNoteNumber, slotIndex);

    const auto& map = slot.sampleMap;
    if (map.zones.empty())
        return;

    const int mappedNote = juce::jlimit(0, 127, midiNoteNumber + slot.transposeSemitones);
    const int velInt = juce::jlimit(1, 127, static_cast<int>(std::round(relVel * 127.0f)));

    for (const auto& z : map.zones)
    {
        if (z.isReleaseTrigger && mappedNote >= z.keyLow && mappedNote <= z.keyHigh
            && velInt >= z.velLow && velInt <= z.velHigh && z.filePath.isNotEmpty())
        {
            juce::File fileToLoad(z.filePath);
            const float effectiveVelocity = map.computeVelocityResponse(relVel, z.velocitySensitivity);
            const int targetBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, slot.outputBus);
            const float combinedGainDb = z.gainDb + slot.gainDb;
            const int adjustedRoot = z.rootNote - (mappedNote - midiNoteNumber);

            const SampleEditSettings relEdit = makeZoneEditSettings(z);
            audioEngine.playZoneVoice(fileToLoad, midiNoteNumber, adjustedRoot, z.fineTuneCents,
                                      combinedGainDb, effectiveVelocity,
                                      z.attackMs / 1000.0f, z.decayMs / 1000.0f,
                                      z.sustainLevel, z.releaseMs / 1000.0f,
                                      true, false, false, targetBus, slotIndex, slot.pan,
                                      &relEdit,
                                      map.pitchTrackingEnabled ? 1 : 0,
                                      0,
                                      true);
        }
    }

    // Optional per-sampler "Trigger on Note-Off" mode (disabled by default):
    // also trigger this sampler's regular zones as one-shot hits on key release.
    if (slot.triggerOnNoteOff || map.triggerOnNoteOff)
    {
        std::vector<const SampleMapZoneState*> noteOffZones;
        for (const auto& z : map.zones)
        {
            if (!z.isReleaseTrigger && mappedNote >= z.keyLow && mappedNote <= z.keyHigh
                && velInt >= z.velLow && velInt <= z.velHigh && z.filePath.isNotEmpty())
            {
                noteOffZones.push_back(&z);
            }
        }

        if (!noteOffZones.empty())
        {
            std::vector<int> distinctRrGroups;
            for (const auto* z : noteOffZones)
            {
                if (std::find(distinctRrGroups.begin(), distinctRrGroups.end(), z->roundRobinIndex) == distinctRrGroups.end())
                    distinctRrGroups.push_back(z->roundRobinIndex);
            }
            std::sort(distinctRrGroups.begin(), distinctRrGroups.end());

            int chosenRrGroup = distinctRrGroups[0];
            if (distinctRrGroups.size() > 1 && map.roundRobinMode != 2)
            {
                const auto rrKey = std::make_pair(slotIndex, mappedNote);
                if (map.roundRobinMode == 1)
                {
                    int lastGroup = 0;
                    {
                        const juce::ScopedLock sl(stateLock);
                        lastGroup = rackLastRandomZoneIndex[rrKey];
                    }
                    int pick = juce::Random::getSystemRandom().nextInt(static_cast<int>(distinctRrGroups.size()));
                    if (distinctRrGroups[static_cast<size_t>(pick)] == lastGroup)
                        pick = (pick + 1) % static_cast<int>(distinctRrGroups.size());
                    chosenRrGroup = distinctRrGroups[static_cast<size_t>(pick)];
                    {
                        const juce::ScopedLock sl(stateLock);
                        rackLastRandomZoneIndex[rrKey] = chosenRrGroup;
                    }
                }
                else
                {
                    int count = 0;
                    {
                        const juce::ScopedLock sl(stateLock);
                        count = rackNoteRoundRobinCounters[rrKey]++;
                    }
                    chosenRrGroup = distinctRrGroups[static_cast<size_t>(count) % distinctRrGroups.size()];
                }
            }

            for (const auto* chosenZone : noteOffZones)
            {
                if (chosenZone != nullptr && chosenZone->roundRobinIndex == chosenRrGroup)
                {
                    juce::File fileToLoad(chosenZone->filePath);
                    const float effectiveVelocity = map.computeVelocityResponse(relVel, chosenZone->velocitySensitivity);
                    const int targetBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, slot.outputBus);
                    const float combinedGainDb = chosenZone->gainDb + slot.gainDb;
                    const int adjustedRoot = chosenZone->rootNote - (mappedNote - midiNoteNumber);
                    const SampleEditSettings zoneEdit = makeZoneEditSettings(*chosenZone);

                    audioEngine.playZoneVoice(fileToLoad, midiNoteNumber, adjustedRoot, chosenZone->fineTuneCents,
                                              combinedGainDb, effectiveVelocity,
                                              chosenZone->attackMs / 1000.0f, chosenZone->decayMs / 1000.0f,
                                              chosenZone->sustainLevel, chosenZone->releaseMs / 1000.0f,
                                              true, false, false, targetBus, slotIndex, slot.pan,
                                              &zoneEdit,
                                              map.pitchTrackingEnabled ? 1 : 0,
                                              0,
                                              true);
                }
            }
        }
    }
}

void OpenWavAudioProcessor::triggerRackSlotNoteOn(int slotIndex, int midiNoteNumber, float velocity, bool includeSameMidiChannelLayers)
{
    if (midiNoteNumber < 0 || midiNoteNumber > 127)
        return;

    SamplerRackState rackSnapshot;
    {
        const juce::ScopedLock sl(stateLock);
        rackSnapshot = fullPluginState.samplerRack;
    }

    if (slotIndex < 0 || slotIndex >= static_cast<int>(rackSnapshot.slots.size()))
        return;

    const auto& clickedSlot = rackSnapshot.slots[static_cast<size_t>(slotIndex)];
    if (!includeSameMidiChannelLayers)
    {
        triggerSingleRackSlotNoteOn(slotIndex, clickedSlot, midiNoteNumber, velocity, false);
        return;
    }

    const int effCh = (clickedSlot.midiChannel >= 1 && clickedSlot.midiChannel <= 16)
                          ? clickedSlot.midiChannel
                          : 1;
    bool anySolo = false;
    for (const auto& slItem : rackSnapshot.slots)
    {
        if (slItem.soloed && slItem.enabled)
            anySolo = true;
    }

    for (size_t i = 0; i < rackSnapshot.slots.size(); ++i)
    {
        const int idx = static_cast<int>(i);
        const auto& slItem = rackSnapshot.slots[i];
        if (slItem.sampleMap.zones.empty())
            continue;

        if (idx == slotIndex)
        {
            triggerSingleRackSlotNoteOn(idx, slItem, midiNoteNumber, velocity, false);
            continue;
        }

        if (!slItem.enabled || slItem.muted || (anySolo && !slItem.soloed))
            continue;

        const bool chMatch = (slItem.midiChannel == 0 || slItem.midiChannel == effCh);
        if (chMatch)
            triggerSingleRackSlotNoteOn(idx, slItem, midiNoteNumber, velocity, false);
    }
}

void OpenWavAudioProcessor::triggerRackSlotNoteOff(int slotIndex, int midiNoteNumber, bool includeSameMidiChannelLayers)
{
    if (midiNoteNumber < 0 || midiNoteNumber > 127)
        return;

    SamplerRackState rackSnapshot;
    {
        const juce::ScopedLock sl(stateLock);
        rackSnapshot = fullPluginState.samplerRack;
    }

    if (slotIndex < 0 || slotIndex >= static_cast<int>(rackSnapshot.slots.size()))
        return;

    const auto& clickedSlot = rackSnapshot.slots[static_cast<size_t>(slotIndex)];
    if (!includeSameMidiChannelLayers)
    {
        triggerSingleRackSlotNoteOff(slotIndex, clickedSlot, midiNoteNumber, 0.8f);
        return;
    }

    const int effCh = (clickedSlot.midiChannel >= 1 && clickedSlot.midiChannel <= 16)
                          ? clickedSlot.midiChannel
                          : 1;
    bool anySolo = false;
    for (const auto& slItem : rackSnapshot.slots)
    {
        if (slItem.soloed && slItem.enabled)
            anySolo = true;
    }

    for (size_t i = 0; i < rackSnapshot.slots.size(); ++i)
    {
        const int idx = static_cast<int>(i);
        const auto& slItem = rackSnapshot.slots[i];
        if (slItem.sampleMap.zones.empty())
            continue;

        const bool chMatch = (idx == slotIndex) || (slItem.midiChannel == 0 || slItem.midiChannel == effCh);
        if (!chMatch)
            continue;

        if (idx != slotIndex && (!slItem.enabled || slItem.muted || (anySolo && !slItem.soloed)))
            audioEngine.stopZoneVoice(midiNoteNumber, idx);
        else
            triggerSingleRackSlotNoteOff(idx, slItem, midiNoteNumber, 0.8f);
    }
}

bool OpenWavAudioProcessor::triggerLayeredRackNoteOn(int midiChannel, int midiNoteNumber, float velocity, int excludeSlotIndex)
{
    if (midiNoteNumber < 0 || midiNoteNumber > 127)
        return false;

    const int effCh = (midiChannel >= 1 && midiChannel <= 16) ? midiChannel : 1;

    SamplerRackState rackSnapshot;
    {
        const juce::ScopedLock sl(stateLock);
        rackSnapshot = fullPluginState.samplerRack;
    }

    if (rackSnapshot.slots.empty())
        return false;

    bool anySolo = false;
    for (const auto& slItem : rackSnapshot.slots)
    {
        if (slItem.soloed && slItem.enabled)
            anySolo = true;
    }

    bool anyTriggered = false;
    for (size_t i = 0; i < rackSnapshot.slots.size(); ++i)
    {
        const int idx = static_cast<int>(i);
        if (idx == excludeSlotIndex)
            continue;

        const auto& slItem = rackSnapshot.slots[i];
        if (!slItem.enabled || slItem.muted || (anySolo && !slItem.soloed) || slItem.sampleMap.zones.empty())
            continue;

        const bool chMatch = (slItem.midiChannel == 0 || slItem.midiChannel == effCh);
        if (chMatch && triggerSingleRackSlotNoteOn(idx, slItem, midiNoteNumber, velocity, false))
            anyTriggered = true;
    }

    return anyTriggered;
}

void OpenWavAudioProcessor::triggerLayeredRackNoteOff(int midiChannel, int midiNoteNumber, int excludeSlotIndex)
{
    if (midiNoteNumber < 0 || midiNoteNumber > 127)
        return;

    const int effCh = (midiChannel >= 1 && midiChannel <= 16) ? midiChannel : 1;

    SamplerRackState rackSnapshot;
    {
        const juce::ScopedLock sl(stateLock);
        rackSnapshot = fullPluginState.samplerRack;
    }

    if (rackSnapshot.slots.empty())
        return;

    bool anySolo = false;
    for (const auto& slItem : rackSnapshot.slots)
    {
        if (slItem.soloed && slItem.enabled)
            anySolo = true;
    }

    for (size_t i = 0; i < rackSnapshot.slots.size(); ++i)
    {
        const int idx = static_cast<int>(i);
        if (idx == excludeSlotIndex)
            continue;

        const auto& slItem = rackSnapshot.slots[i];
        if (slItem.sampleMap.zones.empty())
            continue;

        const bool chMatch = (slItem.midiChannel == 0 || slItem.midiChannel == effCh);
        if (!chMatch)
            continue;

        if (!slItem.enabled || slItem.muted || (anySolo && !slItem.soloed))
            audioEngine.stopZoneVoice(midiNoteNumber, idx);
        else
            triggerSingleRackSlotNoteOff(idx, slItem, midiNoteNumber, 0.8f);
    }
}

void OpenWavAudioProcessor::triggerNoteOnInternal(int midiChannel, int midiNoteNumber, float velocity, bool isInternalMidiFile)
{
    if (midiNoteNumber < 0 || midiNoteNumber > 127)
        return;

    const int effCh = (midiChannel >= 1 && midiChannel <= 16) ? midiChannel : 1;
    const size_t chIdx = static_cast<size_t>(effCh - 1);
    lastNoteOnVelocities[chIdx][static_cast<size_t>(midiNoteNumber)] = velocity;

    SamplerRackState rackSnapshot;
    SampleMapState currentSampleMap;
    {
        const juce::ScopedLock sl(stateLock);
        rackSnapshot = fullPluginState.samplerRack;
        currentSampleMap = sampleMapState;
    }

    // Multi-SampleMap Sampler Rack Path (Multi-Timbral MIDI Channel + Multi-Output Routing)
    if (!rackSnapshot.slots.empty())
    {
        bool anySolo = false;
        bool anyHasZones = false;
        for (const auto& slItem : rackSnapshot.slots)
        {
            if (slItem.soloed && slItem.enabled)
                anySolo = true;
            if (!slItem.sampleMap.zones.empty())
                anyHasZones = true;
        }

        if (anyHasZones)
        {
            bool anyTriggered = false;
            bool channelMatchedSlot = false;

            for (size_t i = 0; i < rackSnapshot.slots.size(); ++i)
            {
                const auto& slItem = rackSnapshot.slots[i];
                if (!slItem.enabled || slItem.muted || (anySolo && !slItem.soloed) || slItem.sampleMap.zones.empty())
                    continue;

                const bool chMatch = (slItem.midiChannel == 0 || slItem.midiChannel == effCh);
                if (!chMatch)
                    continue;

                channelMatchedSlot = true;
                if (triggerSingleRackSlotNoteOn(static_cast<int>(i), slItem, midiNoteNumber, velocity, isInternalMidiFile))
                    anyTriggered = true;
            }

            // During internal .mid file preview, if no slot matched the track's MIDI channel,
            // fall back to the first active slot's channel and layer ALL slots sharing that channel.
            if (!channelMatchedSlot && isInternalMidiFile)
            {
                int fallbackCh = -1;
                for (size_t i = 0; i < rackSnapshot.slots.size(); ++i)
                {
                    const auto& slItem = rackSnapshot.slots[i];
                    if (!slItem.enabled || slItem.muted || (anySolo && !slItem.soloed) || slItem.sampleMap.zones.empty())
                        continue;

                    if (fallbackCh < 0)
                        fallbackCh = slItem.midiChannel;

                    if (slItem.midiChannel == 0 || slItem.midiChannel == fallbackCh)
                    {
                        if (triggerSingleRackSlotNoteOn(static_cast<int>(i), slItem, midiNoteNumber, velocity, true))
                            anyTriggered = true;
                    }
                }
            }

            if (!anyTriggered && isInternalMidiFile)
                audioEngine.triggerMidiAuditionNoteOn(midiNoteNumber, velocity);

            audioEngine.getKeyboardState().noteOn(effCh, midiNoteNumber, velocity);
            return;
        }
    }

    int velInt = juce::jlimit(1, 127, static_cast<int>(std::round(velocity * 127.0f)));
    int mapChannel = (currentSampleMap.midiChannel > 0) ? currentSampleMap.midiChannel : effCh;
    bool hasMappedZones = !currentSampleMap.zones.empty();

    if (hasMappedZones)
    {
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

        // If auditioning a .mid file and only a single pitched sample is in the Sample Map (default keyLow == keyHigh),
        // allow it to play chromatically across the MIDI file range; otherwise fall back to master/synth so .mid never plays silent!
        if (matchingZones.empty() && isInternalMidiFile && currentSampleMap.zones.size() == 1
            && currentSampleMap.pitchTrackingEnabled && !currentSampleMap.zones[0].isReleaseTrigger
            && currentSampleMap.zones[0].filePath.isNotEmpty())
        {
            matchingZones.push_back(&currentSampleMap.zones[0]);
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
                    const int instrumentBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, currentSampleMap.outputBus);
                    const SampleEditSettings zoneEdit = makeZoneEditSettings(*chosenZone);
                    audioEngine.playZoneVoice(fileToLoad, midiNoteNumber, chosenZone->rootNote, chosenZone->fineTuneCents, chosenZone->gainDb, effectiveVelocity,
                                              chosenZone->attackMs / 1000.0f, chosenZone->decayMs / 1000.0f, chosenZone->sustainLevel, chosenZone->releaseMs / 1000.0f,
                                              isInternalMidiFile ? false : currentSampleMap.oneShotEnabled,
                                              chosenZone->isLooping || (!isInternalMidiFile && currentSampleMap.loopingEnabled),
                                              true, instrumentBus, -1, 0.0f,
                                              &zoneEdit,
                                              currentSampleMap.pitchTrackingEnabled ? 1 : 0,
                                              currentSampleMap.randomLoopPosition ? 1 : 0,
                                              true);
                    zoneTriggered = true;
                }
            }
        }

        if (!zoneTriggered && isInternalMidiFile)
        {
            audioEngine.triggerMidiAuditionNoteOn(midiNoteNumber, velocity);
        }

        audioEngine.getKeyboardState().noteOn(mapChannel, midiNoteNumber, velocity);
    }
    else
    {
        if (isInternalMidiFile)
            audioEngine.triggerMidiAuditionNoteOn(midiNoteNumber, velocity);
        else
            audioEngine.triggerNoteOn(midiNoteNumber, velocity);
        audioEngine.getKeyboardState().noteOn(midiChannel > 0 ? midiChannel : 1, midiNoteNumber, velocity);
    }
}

void OpenWavAudioProcessor::handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity)
{
    triggerNoteOffInternal(midiChannel, midiNoteNumber, velocity, false);
}

void OpenWavAudioProcessor::triggerNoteOffInternal(int midiChannel, int midiNoteNumber, float /*velocity*/, bool isInternalMidiFile)
{
    if (midiNoteNumber < 0 || midiNoteNumber > 127)
        return;

    const int effCh = (midiChannel >= 1 && midiChannel <= 16) ? midiChannel : 1;
    const size_t chIdx = static_cast<size_t>(effCh - 1);
    const float relVel = juce::jlimit(0.05f, 1.0f,
        lastNoteOnVelocities[chIdx][static_cast<size_t>(midiNoteNumber)] > 0.0f
            ? lastNoteOnVelocities[chIdx][static_cast<size_t>(midiNoteNumber)]
            : 0.8f);

    SamplerRackState rackSnapshot;
    SampleMapState currentSampleMap;
    {
        const juce::ScopedLock sl(stateLock);
        rackSnapshot = fullPluginState.samplerRack;
        currentSampleMap = sampleMapState;
    }

    if (!rackSnapshot.slots.empty())
    {
        bool anySolo = false;
        bool anyHasZones = false;
        for (const auto& slItem : rackSnapshot.slots)
        {
            if (slItem.soloed && slItem.enabled)
                anySolo = true;
            if (!slItem.sampleMap.zones.empty())
                anyHasZones = true;
        }

        if (anyHasZones)
        {
            bool channelMatchedSlot = false;
            for (size_t i = 0; i < rackSnapshot.slots.size(); ++i)
            {
                const auto& slItem = rackSnapshot.slots[i];
                if (slItem.sampleMap.zones.empty())
                    continue;

                const bool chMatch = (slItem.midiChannel == 0 || slItem.midiChannel == effCh);
                if (!chMatch)
                    continue;

                channelMatchedSlot = true;
                if (!slItem.enabled || slItem.muted || (anySolo && !slItem.soloed))
                    audioEngine.stopZoneVoice(midiNoteNumber, static_cast<int>(i));
                else
                    triggerSingleRackSlotNoteOff(static_cast<int>(i), slItem, midiNoteNumber, relVel);
            }

            if (!channelMatchedSlot && isInternalMidiFile)
            {
                for (size_t i = 0; i < rackSnapshot.slots.size(); ++i)
                    audioEngine.stopZoneVoice(midiNoteNumber, static_cast<int>(i));
            }

            audioEngine.getKeyboardState().noteOff(effCh, midiNoteNumber, 0.0f);
            return;
        }
    }

    int mapChannel = (currentSampleMap.midiChannel > 0) ? currentSampleMap.midiChannel : effCh;
    bool hasMappedZones = !currentSampleMap.zones.empty();

    if (hasMappedZones)
    {
        audioEngine.stopZoneVoice(midiNoteNumber);

        // Trigger any Release / Note-Off zones matching this key
        const int velInt = juce::jlimit(1, 127, static_cast<int>(std::round(relVel * 127.0f)));
        for (const auto& z : currentSampleMap.zones)
        {
            if (z.isReleaseTrigger && midiNoteNumber >= z.keyLow && midiNoteNumber <= z.keyHigh
                && velInt >= z.velLow && velInt <= z.velHigh && z.filePath.isNotEmpty())
            {
                juce::File fileToLoad(z.filePath);
                float effectiveVelocity = currentSampleMap.computeVelocityResponse(relVel, z.velocitySensitivity);
                const int instrumentBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, currentSampleMap.outputBus);
                const SampleEditSettings relEdit = makeZoneEditSettings(z);
                audioEngine.playZoneVoice(fileToLoad, midiNoteNumber, z.rootNote, z.fineTuneCents, z.gainDb, effectiveVelocity,
                                          z.attackMs / 1000.0f, z.decayMs / 1000.0f, z.sustainLevel, z.releaseMs / 1000.0f,
                                          true, false, true, instrumentBus, -1, 0.0f,
                                          &relEdit,
                                          currentSampleMap.pitchTrackingEnabled ? 1 : 0,
                                          0,
                                          true);
            }
        }

        if (currentSampleMap.triggerOnNoteOff)
        {
            for (const auto& z : currentSampleMap.zones)
            {
                if (!z.isReleaseTrigger && midiNoteNumber >= z.keyLow && midiNoteNumber <= z.keyHigh
                    && velInt >= z.velLow && velInt <= z.velHigh && z.filePath.isNotEmpty())
                {
                    juce::File fileToLoad(z.filePath);
                    float effectiveVelocity = currentSampleMap.computeVelocityResponse(relVel, z.velocitySensitivity);
                    const int instrumentBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, currentSampleMap.outputBus);
                    const SampleEditSettings zoneEdit = makeZoneEditSettings(z);
                    audioEngine.playZoneVoice(fileToLoad, midiNoteNumber, z.rootNote, z.fineTuneCents, z.gainDb, effectiveVelocity,
                                              z.attackMs / 1000.0f, z.decayMs / 1000.0f, z.sustainLevel, z.releaseMs / 1000.0f,
                                              true, false, true, instrumentBus, -1, 0.0f,
                                              &zoneEdit,
                                              currentSampleMap.pitchTrackingEnabled ? 1 : 0,
                                              0,
                                              true);
                }
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

    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        const auto& busSet = layouts.outputBuses.getReference(i);
        if (!busSet.isDisabled() && busSet != juce::AudioChannelSet::stereo())
            return false;
    }

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

    // Drain any pending UI Stop / Load commands BEFORE advancing the internal MIDI sequencer
    // so a Stop command queued right before loadAndPlayMidiFile never kills t = 0.0 Note-On events!
    audioEngine.drainPendingCommandsNow();

    // Process all incoming MIDI events cleanly (Note On/Off, Sustain CC 64, All Notes/Sound Off)
    for (const auto metadata : midiMessages)
    {
        processIncomingMidiMessage(metadata.getMessage(), false);
    }

    // Advance internal MIDI File Player if active
    if (midiPlaybackActive.load(std::memory_order_relaxed))
    {
        const juce::ScopedLock sl(midiSeqLock);
        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
        const double blockDur = static_cast<double>(buffer.getNumSamples()) / sr;
        const double prevTime = midiPlayTimeSec;
        const double nextTime = prevTime + blockDur;
        const double maxDur = midiFileDurationAtomic.load(std::memory_order_relaxed);
        midiPlayTimeSec = nextTime;
        midiPlayTimeAtomic.store(nextTime, std::memory_order_relaxed);

        while (midiPlayEventIndex < midiPlaySequence.getNumEvents())
        {
            const auto* holder = midiPlaySequence.getEventPointer(midiPlayEventIndex);
            if (holder == nullptr) { ++midiPlayEventIndex; continue; }
            const double t = holder->message.getTimeStamp();
            if (t >= nextTime)
                break;

            if (holder->message.isNoteOnOrOff() || holder->message.isController() || holder->message.isPitchWheel())
            {
                const int sampleOffset = juce::jlimit(0, std::max(0, buffer.getNumSamples() - 1),
                                                      static_cast<int>((t - prevTime) * sr));
                midiMessages.addEvent(holder->message, sampleOffset);
                processIncomingMidiMessage(holder->message, true);
            }
            ++midiPlayEventIndex;
        }

        if (midiPlayEventIndex >= midiPlaySequence.getNumEvents() || (maxDur > 0.0 && nextTime >= maxDur))
        {
            clearAllHeldMidiState();
            audioEngine.stopAllZoneVoices(false);
            audioEngine.getKeyboardState().allNotesOff(0);

            if (audioEngine.isLooping() && midiPlaySequence.getNumEvents() > 0 && maxDur > 0.05)
            {
                midiPlayEventIndex = 0;
                midiPlayTimeSec = 0.0;
                midiPlayTimeAtomic.store(0.0, std::memory_order_relaxed);
            }
            else
            {
                midiPlaybackActive.store(false, std::memory_order_relaxed);
                midiPlayEventIndex = 0;
                midiPlayTimeSec = 0.0;
                midiPlayTimeAtomic.store(0.0, std::memory_order_relaxed);
            }
        }
    }

    auto mainBusBuffer = getBusBuffer(buffer, false, 0);
    audioEngine.setHostBusActive(0, mainBusBuffer.getNumChannels() > 0);

    std::array<juce::AudioBuffer<float>, AudioEngine::MaxOutputBuses - 1> auxBusBuffers;
    const int numOutBuses = std::min<int>(AudioEngine::MaxOutputBuses, getBusCount(false));
    for (int b = 1; b < AudioEngine::MaxOutputBuses; ++b)
    {
        bool busEnabled = false;
        if (b < numOutBuses)
        {
            if (const auto* busObj = getBus(false, b))
            {
                if (busObj->isEnabled() && busObj->getNumberOfChannels() > 0)
                {
                    auxBusBuffers[static_cast<size_t>(b - 1)] = getBusBuffer(buffer, false, b);
                    auxBusBuffers[static_cast<size_t>(b - 1)].clear();
                    busEnabled = (auxBusBuffers[static_cast<size_t>(b - 1)].getNumChannels() > 0);
                }
            }
        }
        audioEngine.setHostBusActive(b, busEnabled);
    }

    audioEngine.processNextAudioBlock(mainBusBuffer,
                                      auxBusBuffers.data(),
                                      AudioEngine::MaxOutputBuses - 1,
                                      midiMessages);
}

void OpenWavAudioProcessor::processIncomingMidiMessage(const juce::MidiMessage& message, bool isInternalMidiFile)
{
    {
        const juce::ScopedLock sl(midiOutLock);
        if (activeMidiOutput != nullptr && (message.isNoteOnOrOff() || message.isController() || message.isPitchWheel()))
            activeMidiOutput->sendMessageNow(message);
    }

    if (!isInternalMidiFile)
    {
        bool hasRackSlots = false;
        int configuredChannel = 0;
        {
            const juce::ScopedLock sl(stateLock);
            hasRackSlots = !fullPluginState.samplerRack.slots.empty();
            configuredChannel = sampleMapState.midiChannel;
        }

        const int msgCh = message.getChannel();
        if (!hasRackSlots && configuredChannel > 0 && msgCh > 0 && msgCh != configuredChannel)
            return;
    }

    const int msgCh = message.getChannel();
    const int effCh = (msgCh >= 1 && msgCh <= 16) ? msgCh : 1;
    const size_t chIdx = static_cast<size_t>(effCh - 1);

    if (message.isNoteOn())
    {
        const int note = message.getNoteNumber();
        if (note >= 0 && note < 128)
        {
            const size_t idx = static_cast<size_t>(note);
            // If re-triggering a note held by sustain pedal on this channel, release the old sustained voice first
            if (sustainedNotes[chIdx][idx] && !physicalNotesHeld[chIdx][idx])
            {
                sustainedNotes[chIdx][idx] = false;
                triggerNoteOffInternal(effCh, note, 0.0f, isInternalMidiFile);
            }
            physicalNotesHeld[chIdx][idx] = true;
            sustainedNotes[chIdx][idx] = false;
            triggerNoteOnInternal(effCh, note, message.getFloatVelocity(), isInternalMidiFile);
        }
    }
    else if (message.isNoteOff())
    {
        const int note = message.getNoteNumber();
        if (note >= 0 && note < 128)
        {
            const size_t idx = static_cast<size_t>(note);
            physicalNotesHeld[chIdx][idx] = false;
            if (sustainPedalDown[chIdx])
            {
                sustainedNotes[chIdx][idx] = true;
            }
            else
            {
                sustainedNotes[chIdx][idx] = false;
                triggerNoteOffInternal(effCh, note, message.getFloatVelocity(), isInternalMidiFile);
            }
        }
    }
    else if (message.isController())
    {
        const int cc = message.getControllerNumber();
        const int val = message.getControllerValue();

        {
            const juce::ScopedLock sl(midiCcLock);
            if (midiLearnPending && cc != 64 && cc != 120 && cc != 123)
            {
                midiLearnPending = false;
                pendingLearnTarget.ccNumber = cc;

                // Replace existing binding for this same effectIndex + paramId
                midiCcBindings.erase(
                    std::remove_if(midiCcBindings.begin(), midiCcBindings.end(),
                        [&](const MidiCcBinding& b) {
                            return b.effectIndex == pendingLearnTarget.effectIndex && b.paramId == pendingLearnTarget.paramId;
                        }),
                    midiCcBindings.end());
                midiCcBindings.push_back(pendingLearnTarget);

                if (onMidiLearned)
                {
                    auto cb = onMidiLearned;
                    auto target = pendingLearnTarget;
                    juce::MessageManager::callAsync([cb, cc, target]() {
                        cb(cc, target.effectIndex, target.paramId);
                    });
                }
            }

            for (const auto& b : midiCcBindings)
            {
                if (b.ccNumber == cc)
                {
                    if (auto fx = audioEngine.getPerformanceRack().getEffect(b.effectIndex))
                    {
                        const float norm = static_cast<float>(val) / 127.0f;
                        fx->setParameter(b.paramId, b.minVal + norm * (b.maxVal - b.minVal));
                    }
                }
            }
        }

        if (cc == 64) // Sustain / Damper Pedal (per MIDI channel)
        {
            const bool nowDown = (val >= 64);
            if (sustainPedalDown[chIdx] && !nowDown)
            {
                sustainPedalDown[chIdx] = false;
                for (int n = 0; n < 128; ++n)
                {
                    const size_t idx = static_cast<size_t>(n);
                    if (sustainedNotes[chIdx][idx] && !physicalNotesHeld[chIdx][idx])
                    {
                        sustainedNotes[chIdx][idx] = false;
                        triggerNoteOffInternal(effCh, n, 0.0f, isInternalMidiFile);
                    }
                }
            }
            else
            {
                sustainPedalDown[chIdx] = nowDown;
            }
        }
        else if (cc == 120 || cc == 123) // All Sound Off / All Notes Off
        {
            clearAllHeldMidiState();
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
        clearAllHeldMidiState();
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

bool OpenWavAudioProcessor::loadAndPlayMidiFile(const juce::File& midiFile)
{
    if (!midiFile.existsAsFile())
        return false;

    juce::FileInputStream fis(midiFile);
    if (!fis.openedOk())
        return false;

    juce::MidiFile mf;
    if (!mf.readFrom(fis) || mf.getNumTracks() <= 0)
        return false;

    // If the MIDI file contains no embedded tempo meta-event, apply current effective transport BPM
    bool hasTempoEvent = false;
    for (int t = 0; t < mf.getNumTracks() && !hasTempoEvent; ++t)
    {
        if (const auto* trk = mf.getTrack(t))
        {
            for (int e = 0; e < trk->getNumEvents(); ++e)
            {
                if (trk->getEventPointer(e)->message.isTempoMetaEvent())
                {
                    hasTempoEvent = true;
                    break;
                }
            }
        }
    }

    if (!hasTempoEvent && mf.getTimeFormat() > 0)
    {
        const double bpm = juce::jlimit(40.0, 240.0, audioEngine.getEffectiveBpm() > 0.0 ? audioEngine.getEffectiveBpm() : 120.0);
        juce::MidiMessageSequence tempoSeq;
        auto tempoMsg = juce::MidiMessage::tempoMetaEvent(juce::roundToInt(60000000.0 / bpm));
        tempoMsg.setTimeStamp(0.0);
        tempoSeq.addEvent(tempoMsg);
        mf.addTrack(tempoSeq);
    }

    mf.convertTimestampTicksToSeconds();

    juce::MidiMessageSequence merged;
    for (int t = 0; t < mf.getNumTracks(); ++t)
    {
        if (const auto* track = mf.getTrack(t))
            merged.addSequence(*track, 0.0);
    }
    merged.updateMatchedPairs();
    merged.sort();

    // Find earliest Note-On so clips exported from bar 2/5/9 play immediately on click without dead silence
    double firstNoteStart = std::numeric_limits<double>::max();
    bool hasAnyNoteOn = false;
    for (int i = 0; i < merged.getNumEvents(); ++i)
    {
        const auto* evt = merged.getEventPointer(i);
        if (evt != nullptr && evt->message.isNoteOn())
        {
            hasAnyNoteOn = true;
            firstNoteStart = std::min(firstNoteStart, evt->message.getTimeStamp());
        }
    }

    if (!hasAnyNoteOn)
        return false;

    const double trimOffset = (firstNoteStart > 0.01) ? firstNoteStart : 0.0;
    if (trimOffset > 0.0)
    {
        for (int i = 0; i < merged.getNumEvents(); ++i)
        {
            if (auto* evt = merged.getEventPointer(i))
                evt->message.setTimeStamp(std::max(0.0, evt->message.getTimeStamp() - trimOffset));
        }
        merged.updateMatchedPairs();
        merged.sort();
    }

    stopMidiFilePlayback(false);
    audioEngine.stopAllZoneVoices(true);

    std::vector<MidiDisplayNote> notes;
    double maxNoteEnd = 0.0;
    for (int i = 0; i < merged.getNumEvents(); ++i)
    {
        const auto* evt = merged.getEventPointer(i);
        if (evt != nullptr && evt->message.isNoteOn())
        {
            double start = evt->message.getTimeStamp();
            double end = merged.getTimeOfMatchingKeyUp(i);
            if (end <= start)
                end = start + 0.18;
            if (end > maxNoteEnd)
                maxNoteEnd = end;

            MidiDisplayNote n;
            n.noteNumber = evt->message.getNoteNumber();
            n.startSec = start;
            n.endSec = end;
            n.velocity = evt->message.getFloatVelocity();
            notes.push_back(n);
        }
    }
    const double effectiveDuration = std::max(0.25, maxNoteEnd + 0.05);

    const juce::ScopedLock sl(midiSeqLock);
    clearAllHeldMidiState();
    midiPlaySequence = std::move(merged);
    midiDisplayNotes = std::move(notes);
    loadedMidiFileName = midiFile.getFileName();
    loadedMidiFilePath = midiFile.getFullPathName();
    midiPlayEventIndex = 0;
    midiPlayTimeSec = 0.0;
    midiPlayTimeAtomic.store(0.0, std::memory_order_relaxed);
    midiFileDurationAtomic.store(effectiveDuration, std::memory_order_relaxed);
    midiSelectedForTransport.store(true, std::memory_order_relaxed);
    midiPlaybackActive.store(midiPlaySequence.getNumEvents() > 0, std::memory_order_relaxed);
    return midiPlaybackActive.load(std::memory_order_relaxed);
}

void OpenWavAudioProcessor::stopMidiFilePlayback(bool stopAudioVoices)
{
    const bool wasActive = midiPlaybackActive.exchange(false, std::memory_order_relaxed);
    {
        const juce::ScopedLock sl(midiSeqLock);
        clearAllHeldMidiState();
        midiPlayEventIndex = 0;
        midiPlayTimeSec = 0.0;
        midiPlayTimeAtomic.store(0.0, std::memory_order_relaxed);
    }
    if (wasActive)
    {
        if (stopAudioVoices)
            audioEngine.stopAllVoices();
        else
            audioEngine.stopAllZoneVoices(false);
        audioEngine.getKeyboardState().allNotesOff(0);
    }
}

void OpenWavAudioProcessor::pauseMidiFilePlayback()
{
    const bool wasActive = midiPlaybackActive.exchange(false, std::memory_order_relaxed);
    if (wasActive)
    {
        const juce::ScopedLock sl(midiSeqLock);
        clearAllHeldMidiState();
        audioEngine.stopAllZoneVoices(false);
        audioEngine.getKeyboardState().allNotesOff(0);
    }
}

void OpenWavAudioProcessor::toggleMidiFilePlayback()
{
    if (!midiSelectedForTransport.load(std::memory_order_relaxed))
        return;

    if (midiPlaybackActive.load(std::memory_order_relaxed))
    {
        pauseMidiFilePlayback();
        return;
    }

    const juce::ScopedLock sl(midiSeqLock);
    if (midiPlaySequence.getNumEvents() <= 0)
        return;

    const double dur = midiFileDurationAtomic.load(std::memory_order_relaxed);
    if (midiPlayTimeSec >= dur - 0.02 || midiPlayEventIndex >= midiPlaySequence.getNumEvents())
    {
        midiPlayEventIndex = 0;
        midiPlayTimeSec = 0.0;
        midiPlayTimeAtomic.store(0.0, std::memory_order_relaxed);
    }
    midiPlaybackActive.store(true, std::memory_order_relaxed);
}

void OpenWavAudioProcessor::seekMidiFilePlayback(double normalizedRatio)
{
    if (!midiSelectedForTransport.load(std::memory_order_relaxed))
        return;

    const juce::ScopedLock sl(midiSeqLock);
    const double dur = midiFileDurationAtomic.load(std::memory_order_relaxed);
    if (dur <= 0.0 || midiPlaySequence.getNumEvents() <= 0)
        return;

    const double targetSec = juce::jlimit(0.0, dur, normalizedRatio * dur);
    clearAllHeldMidiState();
    audioEngine.stopAllZoneVoices(false);
    audioEngine.getKeyboardState().allNotesOff(0);

    int newIdx = 0;
    while (newIdx < midiPlaySequence.getNumEvents())
    {
        const auto* evt = midiPlaySequence.getEventPointer(newIdx);
        if (evt != nullptr && evt->message.getTimeStamp() >= targetSec)
            break;
        ++newIdx;
    }

    midiPlayEventIndex = newIdx;
    midiPlayTimeSec = targetSec;
    midiPlayTimeAtomic.store(targetSec, std::memory_order_relaxed);
}

void OpenWavAudioProcessor::clearLoadedMidiFile()
{
    stopMidiFilePlayback(false);
    midiSelectedForTransport.store(false, std::memory_order_relaxed);
}

juce::String OpenWavAudioProcessor::getLoadedMidiFileName() const
{
    const juce::ScopedLock sl(midiSeqLock);
    return loadedMidiFileName;
}

juce::String OpenWavAudioProcessor::getLoadedMidiFilePath() const
{
    const juce::ScopedLock sl(midiSeqLock);
    return loadedMidiFilePath;
}

std::vector<OpenWavAudioProcessor::MidiDisplayNote> OpenWavAudioProcessor::getMidiDisplayNotes() const
{
    const juce::ScopedLock sl(midiSeqLock);
    return midiDisplayNotes;
}

void OpenWavAudioProcessor::startMidiLearn(int effectIndex, const juce::String& paramId, float minVal, float maxVal)
{
    const juce::ScopedLock sl(midiCcLock);
    pendingLearnTarget = { -1, effectIndex, paramId, minVal, maxVal };
    midiLearnPending = true;
}

void OpenWavAudioProcessor::clearMidiLearnForParam(int effectIndex, const juce::String& paramId)
{
    const juce::ScopedLock sl(midiCcLock);
    midiCcBindings.erase(
        std::remove_if(midiCcBindings.begin(), midiCcBindings.end(),
            [&](const MidiCcBinding& b) {
                return b.effectIndex == effectIndex && b.paramId == paramId;
            }),
        midiCcBindings.end());
}

int OpenWavAudioProcessor::getLearnedCcForParam(int effectIndex, const juce::String& paramId) const
{
    const juce::ScopedLock sl(midiCcLock);
    for (const auto& b : midiCcBindings)
    {
        if (b.effectIndex == effectIndex && b.paramId == paramId)
            return b.ccNumber;
    }
    return -1;
}

void OpenWavAudioProcessor::setMidiOutputDevice(const juce::String& deviceId)
{
    const juce::ScopedLock sl(midiOutLock);
    midiOutputDeviceId = deviceId;
    activeMidiOutput.reset();
    if (deviceId.isNotEmpty())
        activeMidiOutput = juce::MidiOutput::openDevice(deviceId);
}

} // namespace openwav

// JUCE Plugin Entrypoint Factory
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new openwav::OpenWavAudioProcessor();
}
