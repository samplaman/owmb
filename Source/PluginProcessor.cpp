#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace openwav
{

OpenWavAudioProcessor::OpenWavAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), false)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    audioEngine.getKeyboardState().addListener(this);
}

OpenWavAudioProcessor::~OpenWavAudioProcessor()
{
    audioEngine.getKeyboardState().removeListener(this);
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

    if (s.irFilePath.isNotEmpty())
    {
        audioEngine.loadImpulseResponseFile(juce::File(s.irFilePath));
        audioEngine.setSamplerIrReverbAmount(s.irReverbWetLevel);
        audioEngine.setSamplerIrReverbDryLevel(s.irReverbDryLevel);
    }
    audioEngine.setSamplerDelay(s.delayTimeMs, s.delayFeedback, s.delayWetLevel);
    audioEngine.setSamplerChorus(s.chorusRateHz, s.chorusDepth, s.chorusWetLevel);
    audioEngine.setSamplerReverbAmount(s.samplerReverbAmount);
    audioEngine.setSamplerLowpassCutoff(s.masterFilterCutoffHz);
    audioEngine.setSamplerHighpassCutoff(s.masterHighpassHz);

    for (const auto& m : s.modulators)
    {
        if (m.scope.equalsIgnoreCase("global") || m.scope.isEmpty())
        {
            audioEngine.setLfoFrequency(m.frequency);
            audioEngine.setLfoAmount(m.modAmount);
            audioEngine.setLfoShapeByName(m.shape);
            audioEngine.setLfoTargetByName(m.target);
            audioEngine.setLfoTargetName(m.target);
            break;
        }
    }

    audioEngine.resetAllGroups();
    for (const auto& g : s.groups)
    {
        audioEngine.setGroupVolumeDb(g.index, g.volumeDb);
        audioEngine.setGroupPan(g.index, g.pan);
        audioEngine.setGroupTuningCents(g.index, g.fineTuneCents);
        audioEngine.setGroupMuted(g.index, g.muted || !g.enabled);
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

    if (s.sampleMap.irFilePath.isNotEmpty())
    {
        audioEngine.loadImpulseResponseFile(juce::File(s.sampleMap.irFilePath));
        audioEngine.setSamplerIrReverbAmount(s.sampleMap.irReverbWetLevel);
        audioEngine.setSamplerIrReverbDryLevel(s.sampleMap.irReverbDryLevel);
    }
    audioEngine.setSamplerDelay(s.sampleMap.delayTimeMs, s.sampleMap.delayFeedback, s.sampleMap.delayWetLevel);
    audioEngine.setSamplerChorus(s.sampleMap.chorusRateHz, s.sampleMap.chorusDepth, s.sampleMap.chorusWetLevel);
    audioEngine.setSamplerReverbAmount(s.sampleMap.samplerReverbAmount);
    audioEngine.setSamplerLowpassCutoff(s.sampleMap.masterFilterCutoffHz);
    audioEngine.setSamplerHighpassCutoff(s.sampleMap.masterHighpassHz);

    audioEngine.resetAllGroups();
    for (const auto& g : s.sampleMap.groups)
    {
        audioEngine.setGroupVolumeDb(g.index, g.volumeDb);
        audioEngine.setGroupPan(g.index, g.pan);
        audioEngine.setGroupTuningCents(g.index, g.fineTuneCents);
        audioEngine.setGroupMuted(g.index, g.muted || !g.enabled);
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

    int velInt = juce::jlimit(1, 127, static_cast<int>(std::round(velocity * 127.0f)));

    SampleMapState currentSampleMap;
    {
        const juce::ScopedLock sl(stateLock);
        currentSampleMap = sampleMapState;
    }

    bool hasMappedZones = !currentSampleMap.zones.empty();
    bool zoneTriggered = false;

    // 1. Check Sample Map Zones for matching Key and Velocity (excluding release triggers)
    std::vector<const SampleMapZoneState*> matchingZones;
    for (const auto& z : currentSampleMap.zones)
    {
        int vLow = z.velLow;
        int vHigh = (z.velHigh > 0) ? z.velHigh : 127;
        if (midiNoteNumber >= z.keyLow && midiNoteNumber <= z.keyHigh &&
            velInt >= vLow && velInt <= vHigh &&
            z.filePath.isNotEmpty() &&
            !z.trigger.equalsIgnoreCase("release"))
        {
            matchingZones.push_back(&z);
        }
    }

    // Velocity fallback: If no zones match exact velocity range, match key range
    if (matchingZones.empty())
    {
        for (const auto& z : currentSampleMap.zones)
        {
            if (midiNoteNumber >= z.keyLow && midiNoteNumber <= z.keyHigh &&
                z.filePath.isNotEmpty() &&
                !z.trigger.equalsIgnoreCase("release"))
            {
                matchingZones.push_back(&z);
            }
        }
    }

    if (!matchingZones.empty())
    {
        // 1. Identify distinct Round Robin sequence positions among matching zones
        std::vector<int> distinctRRIndices;
        for (const auto* z : matchingZones)
        {
            int rr = std::max(1, z->roundRobinIndex);
            if (std::find(distinctRRIndices.begin(), distinctRRIndices.end(), rr) == distinctRRIndices.end())
                distinctRRIndices.push_back(rr);
        }
        std::sort(distinctRRIndices.begin(), distinctRRIndices.end());

        int targetRRIndex = 1;
        if (distinctRRIndices.size() <= 1 || currentSampleMap.roundRobinMode == 2)
        {
            targetRRIndex = !distinctRRIndices.empty() ? distinctRRIndices[0] : 1;
        }
        else if (currentSampleMap.roundRobinMode == 1) // Random RR
        {
            int lastIdx = lastRandomZoneIndex[midiNoteNumber];
            int pick = juce::Random::getSystemRandom().nextInt(static_cast<int>(distinctRRIndices.size()));
            if (distinctRRIndices.size() > 1 && pick == lastIdx)
                pick = (pick + 1) % static_cast<int>(distinctRRIndices.size());
            lastRandomZoneIndex[midiNoteNumber] = pick;
            targetRRIndex = distinctRRIndices[static_cast<size_t>(pick)];
        }
        else // Sequential / Cycle RR
        {
            int count = noteRoundRobinCounters[midiNoteNumber]++;
            size_t pick = static_cast<size_t>(count) % distinctRRIndices.size();
            targetRRIndex = distinctRRIndices[pick];
        }

        // 2. Filter matching zones to the chosen Round Robin variation
        std::vector<const SampleMapZoneState*> rrMatchingZones;
        for (const auto* z : matchingZones)
        {
            int rr = std::max(1, z->roundRobinIndex);
            if (rr == targetRRIndex || z->roundRobinIndex <= 0)
                rrMatchingZones.push_back(z);
        }

        // 3. Trigger multi-group layers (e.g. multi-mic positions or group layers for this RR)
        std::map<int, std::vector<const SampleMapZoneState*>> groupZoneMap;
        for (const auto* z : rrMatchingZones)
        {
            groupZoneMap[z->groupIndex].push_back(z);
        }

        for (auto& pair : groupZoneMap)
        {
            auto& grpZones = pair.second;
            if (grpZones.empty()) continue;

            const SampleMapZoneState* chosenZone = grpZones[0];
            if (chosenZone != nullptr)
            {
                juce::File fileToLoad(chosenZone->filePath);
                float att = (chosenZone->attackMs > 0.0f ? chosenZone->attackMs : currentSampleMap.globalAttackMs) / 1000.0f;
                float dec = (chosenZone->decayMs > 0.0f ? chosenZone->decayMs : currentSampleMap.globalDecayMs) / 1000.0f;
                float sus = chosenZone->sustainLevel;
                float rel = (chosenZone->releaseMs > 0.0f ? chosenZone->releaseMs : currentSampleMap.globalReleaseMs) / 1000.0f;

                audioEngine.playZoneVoice(fileToLoad, midiNoteNumber, chosenZone->rootNote, chosenZone->fineTuneCents + currentSampleMap.masterFineTuneCents, chosenZone->gainDb, velocity,
                                          att, dec, sus, rel,
                                          audioEngine.isOneShotEnabled(), chosenZone->loopEnabled, chosenZone->groupIndex, chosenZone->pan,
                                          chosenZone->sampleStart, chosenZone->sampleEnd, chosenZone->loopStart, chosenZone->loopEnd);
                zoneTriggered = true;
            }
        }
    }

    // 2. Fallback: Trigger single master sample only if NO zones are mapped and none triggered
    if (!hasMappedZones && !zoneTriggered)
    {
        audioEngine.triggerNoteOn(midiNoteNumber, velocity);
    }
}

void OpenWavAudioProcessor::handleNoteOff(juce::MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float velocity)
{
    if (midiNoteNumber < 0 || midiNoteNumber > 127)
        return;

    audioEngine.stopZoneVoice(midiNoteNumber);
    audioEngine.triggerNoteOff(midiNoteNumber);

    SampleMapState currentSampleMap;
    {
        const juce::ScopedLock sl(stateLock);
        currentSampleMap = sampleMapState;
    }

    int velInt = juce::jlimit(1, 127, static_cast<int>(velocity * 127.0f));
    for (const auto& z : currentSampleMap.zones)
    {
        if (z.trigger.equalsIgnoreCase("release") &&
            midiNoteNumber >= z.keyLow && midiNoteNumber <= z.keyHigh &&
            velInt >= z.velLow && velInt <= z.velHigh &&
            z.filePath.isNotEmpty())
        {
            juce::File fileToLoad(z.filePath);
            float att = (z.attackMs > 0.0f ? z.attackMs : currentSampleMap.globalAttackMs) / 1000.0f;
            float dec = (z.decayMs > 0.0f ? z.decayMs : currentSampleMap.globalDecayMs) / 1000.0f;
            float sus = z.sustainLevel;
            float rel = (z.releaseMs > 0.0f ? z.releaseMs : currentSampleMap.globalReleaseMs) / 1000.0f;
            audioEngine.playZoneVoice(fileToLoad, midiNoteNumber, z.rootNote, z.fineTuneCents + currentSampleMap.masterFineTuneCents, z.gainDb, std::max(0.2f, velocity),
                                      att, dec, sus, rel,
                                      true, false, z.groupIndex, z.pan,
                                      z.sampleStart, z.sampleEnd, z.loopStart, z.loopEnd);
        }
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

    // Process all incoming DAW MIDI events (Notes, Mod Wheel CC 1, CC mappings, Pitch Wheel)
    SampleMapState currentMap;
    {
        const juce::ScopedLock sl(stateLock);
        currentMap = sampleMapState;
    }

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
        else if (message.isController())
        {
            int ccNum = message.getControllerNumber();
            int ccVal = message.getControllerValue();
            float normVal = ccVal / 127.0f;

            // 1. Process custom Decent Sampler MIDI CC mappings
            for (const auto& ccMap : currentMap.midiCcMappings)
            {
                if (ccMap.ccNumber == ccNum)
                {
                    for (const auto& b : ccMap.bindings)
                    {
                        if (b.type.equalsIgnoreCase("modulator") || b.parameter.containsIgnoreCase("lfo") || b.parameter.containsIgnoreCase("MOD_AMOUNT") || b.parameter.containsIgnoreCase("depth"))
                        {
                            audioEngine.setLfoAmount(normVal);
                        }
                        else if (b.parameter.containsIgnoreCase("MOD_FREQUENCY") || b.parameter.containsIgnoreCase("rate") || b.parameter.containsIgnoreCase("speed"))
                        {
                            audioEngine.setLfoFrequency(normVal * 20.0f);
                        }
                        else if (b.level.equalsIgnoreCase("group"))
                        {
                            int pos = b.position;
                            if (b.parameter.containsIgnoreCase("volume") || b.parameter.containsIgnoreCase("gain") || b.parameter.containsIgnoreCase("AMP_VOLUME"))
                            {
                                float volDb = (normVal > 0.0001f) ? (20.0f * std::log10(normVal)) : -96.0f;
                                audioEngine.setGroupVolumeDb(pos, volDb);
                            }
                            else if (b.parameter.containsIgnoreCase("pan"))
                            {
                                audioEngine.setGroupPan(pos, normVal * 2.0f - 1.0f);
                            }
                            else if (b.parameter.containsIgnoreCase("mute"))
                            {
                                audioEngine.setGroupMuted(pos, normVal > 0.5f);
                            }
                        }
                        else if (b.parameter.containsIgnoreCase("reverb") || b.parameter.containsIgnoreCase("ir"))
                        {
                            audioEngine.setSamplerIrReverbAmount(normVal);
                            audioEngine.setSamplerReverbAmount(normVal);
                        }
                        else if (b.parameter.containsIgnoreCase("cutoff") || b.parameter.containsIgnoreCase("filter"))
                        {
                            float cutoff = 100.0f * std::pow(220.0f, normVal);
                            audioEngine.setSamplerLowpassCutoff(cutoff);
                        }
                    }
                }
            }

            // 2. Standard MIDI CC handling
            if (ccNum == 1) // Modulation Wheel (CC 1 -> LFO Vibrato / Tremolo)
            {
                audioEngine.setMidiModWheel(normVal);
            }
            else if (ccNum == 11) // Expression (CC 11)
            {
                audioEngine.setMidiExpression(normVal);
            }
            else if (ccNum == 7) // Volume (CC 7)
            {
                audioEngine.setGain(normVal);
            }
        }
        else if (message.isPitchWheel())
        {
            int pitchVal = message.getPitchWheelValue(); // 0 - 16383, 8192 center
            float bendNorm = (pitchVal - 8192) / 8192.0f; // -1.0 to +1.0
            audioEngine.setPitchBendSemis(bendNorm * 2.0f);
        }
        else if (message.isChannelPressure())
        {
            float pressureNorm = message.getChannelPressureValue() / 127.0f;
            if (audioEngine.getMidiModWheel() < 0.01f && pressureNorm > 0.01f)
            {
                audioEngine.setMidiModWheel(pressureNorm);
            }
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
