#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

namespace openwav
{

OpenWavAudioProcessor::OpenWavAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
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
    triggerNoteOnInternal(midiChannel, midiNoteNumber, velocity, false);
}

void OpenWavAudioProcessor::triggerNoteOnInternal(int midiChannel, int midiNoteNumber, float velocity, bool isInternalMidiFile)
{
    if (midiNoteNumber < 0 || midiNoteNumber > 127)
        return;

    int velInt = juce::jlimit(1, 127, static_cast<int>(std::round(velocity * 127.0f)));

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
                    audioEngine.playZoneVoice(fileToLoad, midiNoteNumber, chosenZone->rootNote, chosenZone->fineTuneCents, chosenZone->gainDb, effectiveVelocity,
                                              chosenZone->attackMs / 1000.0f, chosenZone->decayMs / 1000.0f, chosenZone->sustainLevel, chosenZone->releaseMs / 1000.0f,
                                              isInternalMidiFile ? false : audioEngine.isOneShotEnabled(),
                                              chosenZone->isLooping || (!isInternalMidiFile && audioEngine.isLooping()),
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
                                          true, false, true);
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
            sustainPedalDown = false;
            physicalNotesHeld.fill(false);
            sustainedNotes.fill(false);
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

    audioEngine.processNextAudioBlock(buffer, midiMessages);
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
        int configuredChannel = 0;
        {
            const juce::ScopedLock sl(stateLock);
            configuredChannel = sampleMapState.midiChannel;
        }

        const int msgCh = message.getChannel();
        if (configuredChannel > 0 && msgCh > 0 && msgCh != configuredChannel)
            return;
    }

    const int msgCh = message.getChannel();

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
                triggerNoteOffInternal(msgCh, note, 0.0f, isInternalMidiFile);
            }
            physicalNotesHeld[idx] = true;
            sustainedNotes[idx] = false;
            noteChannels[idx] = msgCh > 0 ? msgCh : 1;
            triggerNoteOnInternal(msgCh, note, message.getFloatVelocity(), isInternalMidiFile);
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
                triggerNoteOffInternal(msgCh, note, message.getFloatVelocity(), isInternalMidiFile);
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
    sustainPedalDown = false;
    physicalNotesHeld.fill(false);
    sustainedNotes.fill(false);
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
        sustainPedalDown = false;
        physicalNotesHeld.fill(false);
        sustainedNotes.fill(false);
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
        sustainPedalDown = false;
        physicalNotesHeld.fill(false);
        sustainedNotes.fill(false);
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
    sustainPedalDown = false;
    physicalNotesHeld.fill(false);
    sustainedNotes.fill(false);
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
