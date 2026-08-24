#include "SampleMapComponent.h"
#include "OpenWavLookAndFeel.h"
#include <regex>
// Rebuild trigger

namespace openwav
{

SampleMapComponent::SampleMapComponent(AudioEngine& engine)
    : audioEngine(engine)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
    audioEngine.getKeyboardState().addListener(this);
    audioEngine.addListener(this);

    // ── Action Toolbar Buttons ─────────────────────────
    addAndMakeVisible(addSampleButton);
    addSampleButton.addListener(this);

    addAndMakeVisible(autoMapPitchButton);
    autoMapPitchButton.addListener(this);

    addAndMakeVisible(autoMapChromaticButton);
    autoMapChromaticButton.addListener(this);

    addAndMakeVisible(autoMapVelButton);
    autoMapVelButton.addListener(this);

    addAndMakeVisible(autoMapRRButton);
    autoMapRRButton.addListener(this);

    addAndMakeVisible(clearMapButton);
    clearMapButton.addListener(this);

    roundRobinButton.setClickingTogglesState(false);
    roundRobinButton.setButtonText(roundRobinMode == 0 ? "RR: Cycle" : (roundRobinMode == 1 ? "RR: Random" : "RR: OFF"));
    roundRobinButton.onClick = [this] {
        roundRobinMode = (roundRobinMode + 1) % 3;
        roundRobinButton.setButtonText(roundRobinMode == 0 ? "RR: Cycle" : (roundRobinMode == 1 ? "RR: Random" : "RR: OFF"));
        if (onStateChanged) onStateChanged();
    };
    addAndMakeVisible(roundRobinButton);

    pitchTrackButton.setClickingTogglesState(true);
    bool ptEnabled = audioEngine.isPitchTrackingEnabled();
    pitchTrackButton.setToggleState(ptEnabled, juce::dontSendNotification);
    pitchTrackButton.setButtonText(ptEnabled ? "Pitch Track: ON" : "Pitch Track: OFF");
    pitchTrackButton.onClick = [this] {
        bool enabled = pitchTrackButton.getToggleState();
        audioEngine.setPitchTrackingEnabled(enabled);
        pitchTrackButton.setButtonText(enabled ? "Pitch Track: ON" : "Pitch Track: OFF");
    };
    addAndMakeVisible(pitchTrackButton);

    oneShotButton.setClickingTogglesState(true);
    bool osEnabled = audioEngine.isOneShotEnabled();
    oneShotButton.setToggleState(osEnabled, juce::dontSendNotification);
    oneShotButton.setButtonText(osEnabled ? "One Shot: ON" : "One Shot: OFF");
    oneShotButton.onClick = [this] {
        bool enabled = oneShotButton.getToggleState();
        audioEngine.setOneShotEnabled(enabled);
        oneShotButton.setButtonText(enabled ? "One Shot: ON" : "One Shot: OFF");
    };
    addAndMakeVisible(oneShotButton);

    loopButton.setClickingTogglesState(true);
    bool loopEnabled = audioEngine.isLooping();
    loopButton.setToggleState(loopEnabled, juce::dontSendNotification);
    loopButton.setButtonText(loopEnabled ? "Loop: ON" : "Loop: OFF");
    loopButton.onClick = [this] {
        bool enabled = loopButton.getToggleState();
        audioEngine.setLooping(enabled);
        loopButton.setButtonText(enabled ? "Loop: ON" : "Loop: OFF");
    };
    addAndMakeVisible(loopButton);

    // ── Inspector Labels & Sliders ─────────────────────
    inspectorTitle.setFont(juce::Font(14.0f).boldened());
    inspectorTitle.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    addAndMakeVisible(inspectorTitle);

    sampleNameValue.setFont(juce::Font(12.0f).boldened());
    sampleNameValue.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    addAndMakeVisible(sampleNameValue);

    auto setupSlider = [this](juce::Slider& s, juce::Label& lbl, const juce::String& text, double minV, double maxV, double stepV, double defV) {
        lbl.setFont(juce::Font(11.0f));
        lbl.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
        addAndMakeVisible(lbl);

        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 18);
        s.setRange(minV, maxV, stepV);
        s.setValue(defV, juce::dontSendNotification);
        s.addListener(this);
        addAndMakeVisible(s);
    };

    setupSlider(rootNoteSlider, rootNoteTitle, "Root Note:", 0.0, 127.0, 1.0, 60.0);
    setupSlider(keyLowSlider, keyLowTitle, "Key Low:", 0.0, 127.0, 1.0, 36.0);
    setupSlider(keyHighSlider, keyHighTitle, "Key High:", 0.0, 127.0, 1.0, 84.0);
    setupSlider(velLowSlider, velLowTitle, "Vel Low:", 0.0, 127.0, 1.0, 0.0);
    setupSlider(velHighSlider, velHighTitle, "Vel High:", 0.0, 127.0, 1.0, 127.0);
    setupSlider(rrSlider, rrTitle, "Round Robin:", 1.0, 8.0, 1.0, 1.0);
    setupSlider(tuneSlider, tuneTitle, "Fine Tune:", -100.0, 100.0, 1.0, 0.0);
    setupSlider(gainSlider, gainTitle, "Gain (dB):", -24.0, 12.0, 0.5, 0.0);

    setupSlider(attackSlider, attackTitle, "Attack (ms):", 0.0, 2000.0, 1.0, 5.0);
    setupSlider(decaySlider, decayTitle, "Decay (ms):", 0.0, 2000.0, 1.0, 100.0);
    setupSlider(sustainSlider, sustainTitle, "Sustain (%):", 0.0, 1.0, 0.01, 1.0);
    setupSlider(releaseSlider, releaseTitle, "Release (ms):", 0.0, 5000.0, 1.0, 200.0);
    setupSlider(reverbSlider, reverbTitle, "Reverb (%):", 0.0, 100.0, 1.0, 0.0);

    // ── ADSR Rotary Knobs (Top Bar beside Clear Map) ──
    auto setupKnob = [this](juce::Slider& s, juce::Label& lbl, const juce::String& text, double minV, double maxV, double stepV, double defV) {
        lbl.setFont(juce::Font(11.0f).boldened());
        lbl.setText(text, juce::dontSendNotification);
        lbl.setJustificationType(juce::Justification::centred);
        lbl.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
        addAndMakeVisible(lbl);

        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
        s.setRange(minV, maxV, stepV);
        s.setValue(defV, juce::dontSendNotification);
        s.addListener(this);
        addAndMakeVisible(s);
    };

    setupKnob(attackKnob, attackLabel, "Attack", 0.0, 2000.0, 1.0, 5.0);
    setupKnob(decayKnob, decayLabel, "Decay", 0.0, 2000.0, 1.0, 100.0);
    setupKnob(sustainKnob, sustainLabel, "Sustain", 0.0, 1.0, 0.01, 1.0);
    setupKnob(releaseKnob, releaseLabel, "Release", 0.0, 5000.0, 1.0, 200.0);

    activeNoteVelocities.fill(-1);

    lookAndFeelChanged();
}

void SampleMapComponent::lookAndFeelChanged()
{
    // Sliders & buttons inherit LookAndFeel automatically
}

SampleMapComponent::~SampleMapComponent()
{
    stopTimer();
    audioEngine.getKeyboardState().removeListener(this);
    audioEngine.removeListener(this);
}

void SampleMapComponent::sampleLoaded(const juce::String& /*filePath*/)
{
    juce::MessageManager::callAsync([this] {
        resized();
        repaint();
    });
}

void SampleMapComponent::pitchTrackingStateChanged(bool enabled)
{
    juce::MessageManager::callAsync([this, enabled] {
        pitchTrackButton.setToggleState(enabled, juce::dontSendNotification);
        pitchTrackButton.setButtonText(enabled ? "Pitch Track: ON" : "Pitch Track: OFF");
    });
}

void SampleMapComponent::oneShotStateChanged(bool enabled)
{
    juce::MessageManager::callAsync([this, enabled] {
        oneShotButton.setToggleState(enabled, juce::dontSendNotification);
        oneShotButton.setButtonText(enabled ? "One Shot: ON" : "One Shot: OFF");
    });
}

void SampleMapComponent::loopingStateChanged(bool enabled)
{
    juce::MessageManager::callAsync([this, enabled] {
        loopButton.setToggleState(enabled, juce::dontSendNotification);
        loopButton.setButtonText(enabled ? "Loop: ON" : "Loop: OFF");
    });
}

void SampleMapComponent::handleNoteOn(juce::MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float velocity)
{
    if (midiNoteNumber >= 0 && midiNoteNumber < 128)
    {
        activeMidiNotes[static_cast<size_t>(midiNoteNumber)] = true;
        int velInt = juce::jlimit(0, 127, static_cast<int>(velocity * 127.0f));
        activeNoteVelocities[static_cast<size_t>(midiNoteNumber)] = velInt;

        uint32_t now = juce::Time::getMillisecondCounter();
        recentHitDots.erase(
            std::remove_if(recentHitDots.begin(), recentHitDots.end(), [midiNoteNumber](const HitDot& d) {
                return d.note == midiNoteNumber;
            }),
            recentHitDots.end()
        );
        recentHitDots.push_back({ midiNoteNumber, velInt, now });

        juce::File fileToLoad;
        for (size_t i = 0; i < zones.size(); ++i)
        {
            const auto& z = zones[i];
            if (midiNoteNumber >= z.keyLow && midiNoteNumber <= z.keyHigh && velInt >= z.velLow && velInt <= z.velHigh)
            {
                selectedZoneIndex = static_cast<int>(i);
                selectedZoneIndices.clear();
                selectedZoneIndices.insert(static_cast<int>(i));

                if (z.filePath.isNotEmpty())
                    fileToLoad = juce::File(z.filePath);
                break;
            }
        }

        juce::MessageManager::callAsync([this, fileToLoad] {
            if (!isTimerRunning())
                startTimerHz(30);

            if (fileToLoad.existsAsFile())
            {
                if (audioEngine.getCurrentFile() != fileToLoad)
                {
                    audioEngine.loadFile(fileToLoad, false, true);
                }
            }
            resized();
            repaint();
        });
    }
}

void SampleMapComponent::handleNoteOff(juce::MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float /*velocity*/)
{
    if (midiNoteNumber >= 0 && midiNoteNumber < 128)
    {
        activeMidiNotes[static_cast<size_t>(midiNoteNumber)] = false;
        activeNoteVelocities[static_cast<size_t>(midiNoteNumber)] = -1;
        juce::MessageManager::callAsync([this] {
            if (!isTimerRunning())
                startTimerHz(30);
            repaint();
        });
    }
}

void SampleMapComponent::timerCallback()
{
    uint32_t now = juce::Time::getMillisecondCounter();
    bool hasHeld = false;
    for (int note = 0; note < 128; ++note)
    {
        if (activeNoteVelocities[static_cast<size_t>(note)] >= 0)
        {
            hasHeld = true;
            break;
        }
    }

    recentHitDots.erase(
        std::remove_if(recentHitDots.begin(), recentHitDots.end(), [now, this](const HitDot& dot) {
            bool isHeld = (dot.note >= 0 && dot.note < 128 && activeNoteVelocities[static_cast<size_t>(dot.note)] >= 0);
            return !isHeld && (now - dot.timestampMs > 1200);
        }),
        recentHitDots.end()
    );

    if (recentHitDots.empty() && !hasHeld)
    {
        stopTimer();
    }
    repaint();
}

bool SampleMapComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        juce::File file(f);
        if (file.hasFileExtension("wav") || file.hasFileExtension("mp3") ||
            file.hasFileExtension("flac") || file.hasFileExtension("aiff") ||
            file.hasFileExtension("aif") || file.hasFileExtension("aifc") ||
            file.hasFileExtension("ogg"))
            return true;
    }
    return false;
}

void SampleMapComponent::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    for (const auto& f : files)
    {
        juce::File file(f);
        if (file.existsAsFile())
        {
            addSampleFile(file);
        }
    }
}

bool SampleMapComponent::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& /*dragSourceDetails*/)
{
    return true;
}

void SampleMapComponent::itemDropped(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails)
{
    juce::String description = dragSourceDetails.description.toString();
    if (description.isNotEmpty())
    {
        juce::File file(description);
        if (file.existsAsFile())
        {
            addSampleFile(file);
        }
    }
}

juce::Rectangle<float> SampleMapComponent::getGridBounds() const
{
    auto area = getLocalBounds().reduced(20, 16);
    area.removeFromTop(44);     // Top toolbar gap
    area.removeFromBottom(50);  // Keybed height
    int inspectorW = 240;
    area.removeFromRight(inspectorW + 16); // Inspector sidebar
    return area.toFloat();
}

juce::Rectangle<float> SampleMapComponent::getKeybedBounds() const
{
    auto grid = getGridBounds();
    return juce::Rectangle<float>(grid.getX(), grid.getBottom() + 4.0f, grid.getWidth(), 44.0f);
}

juce::Rectangle<float> SampleMapComponent::getInspectorBounds() const
{
    auto area = getLocalBounds().reduced(20, 16);
    area.removeFromTop(44);
    int inspectorW = 240;
    return area.removeFromRight(inspectorW).toFloat();
}

int SampleMapComponent::noteNumberAtX(float x, juce::Rectangle<float> gridArea) const
{
    float relX = (x - gridArea.getX()) / gridArea.getWidth();
    int note = juce::jlimit(0, 127, static_cast<int>(relX * 128.0f));
    return note;
}

float SampleMapComponent::xForNoteNumber(int noteNum, juce::Rectangle<float> gridArea) const
{
    float norm = static_cast<float>(juce::jlimit(0, 127, noteNum)) / 128.0f;
    return gridArea.getX() + norm * gridArea.getWidth();
}

int SampleMapComponent::velocityAtY(float y, juce::Rectangle<float> gridArea) const
{
    float relY = 1.0f - (y - gridArea.getY()) / gridArea.getHeight();
    return juce::jlimit(0, 127, static_cast<int>(relY * 128.0f));
}

float SampleMapComponent::yForVelocity(int vel, juce::Rectangle<float> gridArea) const
{
    float norm = 1.0f - static_cast<float>(juce::jlimit(0, 127, vel)) / 128.0f;
    return gridArea.getY() + norm * gridArea.getHeight();
}

juce::String SampleMapComponent::midiNoteToName(int noteNum) const
{
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int octave = (noteNum / 12) - 1;
    int nameIdx = noteNum % 12;
    return juce::String(noteNames[nameIdx]) + juce::String(octave);
}

int SampleMapComponent::parseRootNoteFromFilename(const juce::String& filename) const
{
    // Search for note patterns e.g. C3, F#2, Db4, A-1
    std::string name = filename.toStdString();
    std::regex noteRegex("([A-Ga-g])(#|b|s)?(-?[0-9])");
    std::smatch match;

    if (std::regex_search(name, match, noteRegex))
    {
        std::string noteStr = match[1].str();
        std::string accStr = match[2].str();
        std::string octStr = match[3].str();

        char noteChar = std::toupper(noteStr[0]);
        int baseNote = 0;
        switch (noteChar)
        {
            case 'C': baseNote = 0; break;
            case 'D': baseNote = 2; break;
            case 'E': baseNote = 4; break;
            case 'F': baseNote = 5; break;
            case 'G': baseNote = 7; break;
            case 'A': baseNote = 9; break;
            case 'B': baseNote = 11; break;
        }

        if (accStr == "#" || accStr == "s") baseNote += 1;
        else if (accStr == "b") baseNote -= 1;

        int octave = std::stoi(octStr);
        int midiNote = (octave + 1) * 12 + baseNote;
        return juce::jlimit(0, 127, midiNote);
    }

    return 60; // Default C4
}

static int parseRoundRobinFromFilename(const juce::String& filename)
{
    std::string name = filename.toStdString();
    std::regex rrRegex("(?i)(?:_|-|\\s)(?:rr|take|r)([1-9][0-9]?)");
    std::smatch match;
    if (std::regex_search(name, match, rrRegex))
    {
        return std::stoi(match[1].str());
    }
    return 1;
}

// ─────────────────────────────────────────────────────────
//  Zone Actions & Operations
// ─────────────────────────────────────────────────────────
void SampleMapComponent::addSample(const MediaItem& item)
{
    SampleMapZone z;
    z.filePath = item.filePath;
    z.sampleName = item.fileName;
    z.rootNote = parseRootNoteFromFilename(item.fileName);
    z.keyLow = z.rootNote;
    z.keyHigh = z.rootNote;
    z.velLow = 0;
    z.velHigh = 127;
    z.roundRobinIndex = parseRoundRobinFromFilename(item.fileName);

    zones.push_back(z);
    selectedZoneIndex = static_cast<int>(zones.size()) - 1;
    juce::File itemFile(item.filePath);
    if (itemFile.existsAsFile())
    {
        audioEngine.loadFile(itemFile, false, true);
    }
    resized();
    repaint();
    if (onStateChanged) onStateChanged();
}

void SampleMapComponent::addSampleFile(const juce::File& file)
{
    if (!file.existsAsFile()) return;

    SampleMapZone z;
    z.filePath = file.getFullPathName();
    z.sampleName = file.getFileName();
    z.rootNote = parseRootNoteFromFilename(file.getFileName());
    z.keyLow = z.rootNote;
    z.keyHigh = z.rootNote;
    z.velLow = 0;
    z.velHigh = 127;
    z.roundRobinIndex = parseRoundRobinFromFilename(file.getFileName());

    zones.push_back(z);
    selectedZoneIndex = static_cast<int>(zones.size()) - 1;
    audioEngine.loadFile(file, false, true);
    resized();
    repaint();
    if (onStateChanged) onStateChanged();
}

void SampleMapComponent::sliceFileToZones(const juce::File& audioFile, const juce::AudioBuffer<float>& buffer, double sampleRate, const std::vector<double>& sliceRatios)
{
    clearAllZones();

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || numChannels <= 0 || sampleRate <= 0.0) return;

    int numSlices = static_cast<int>(sliceRatios.size());

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("OWMB_Temp");
    tempDir.createDirectory();

    static int sliceCounter = 0;
    auto timestamp = juce::Time::currentTimeMillis();
    juce::String baseName = audioFile.getFileNameWithoutExtension();

    for (int i = 0; i < numSlices; ++i)
    {
        double startR = sliceRatios[i];
        double endR = (i + 1 < numSlices) ? sliceRatios[i + 1] : 1.0;

        int startSample = static_cast<int>(startR * numSamples);
        int endSample = static_cast<int>(endR * numSamples);
        int sliceLen = endSample - startSample;
        if (sliceLen <= 0) continue;

        juce::File sliceFile = tempDir.getChildFile(baseName + "_Slice_" + juce::String(i + 1) + "_" + juce::String(timestamp) + "_" + juce::String(++sliceCounter) + ".wav");
        sliceFile.deleteFile();

        auto* rawStream = sliceFile.createOutputStream().release();
        if (rawStream != nullptr)
        {
            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(rawStream, sampleRate, numChannels, 16, {}, 0));
            if (writer != nullptr)
            {
                writer->writeFromAudioSampleBuffer(buffer, startSample, sliceLen);
            }
        }

        if (sliceFile.existsAsFile() && sliceFile.getSize() > 44)
        {
            juce::AudioBuffer<float> sliceBuf(numChannels, sliceLen);
            for (int ch = 0; ch < numChannels; ++ch)
                sliceBuf.copyFrom(ch, 0, buffer, ch, startSample, sliceLen);

            audioEngine.putSampleInCache(sliceFile.getFullPathName(), sampleRate, sliceBuf);

            SampleMapZone z;
            z.filePath = sliceFile.getFullPathName();
            z.sampleName = sliceFile.getFileName();

            int mappedKey = juce::jmin(127, 36 + i);
            z.rootNote = mappedKey;
            z.keyLow = mappedKey;
            z.keyHigh = mappedKey;
            z.velLow = 0;
            z.velHigh = 127;
            z.fineTuneCents = 0.0f;
            z.gainDb = 0.0f;

            zones.push_back(z);
        }
    }

    if (!zones.empty())
    {
        selectedZoneIndex = 0;
        selectedZoneIndices.insert(0);
        juce::File firstSlice(zones[0].filePath);
        if (firstSlice.existsAsFile())
        {
            audioEngine.loadFile(firstSlice, false, true);
        }
    }

    resized();
    repaint();
    if (onStateChanged)
        onStateChanged();
}

void SampleMapComponent::sliceLoadedSample(const std::vector<double>& sliceRatios)
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    if (!audioEngine.getAudioBufferCopy(buffer, sampleRate))
        return;

    juce::File currentFile = audioEngine.getCurrentFile();
    if (!currentFile.existsAsFile())
    {
        juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("OWMB_Temp");
        tempDir.createDirectory();
        currentFile = tempDir.getChildFile("LoadedSample.wav");
    }

    sliceFileToZones(currentFile, buffer, sampleRate, sliceRatios);
}

void SampleMapComponent::autoSliceToSampler(const MediaItem& item, const std::vector<double>& customSliceRatios)
{
    try
    {
        if (onSliceToSamplerStarted)
            onSliceToSamplerStarted();

        juce::File audioFile(item.filePath);
        if (!audioFile.existsAsFile()) return;

        std::unique_ptr<juce::AudioFormatReader> reader(audioEngine.getFormatManager().createReaderFor(audioFile));
        if (reader == nullptr) return;

        int numChannels = static_cast<int>(reader->numChannels);
        int numSamples = static_cast<int>(reader->lengthInSamples);
        double sampleRate = reader->sampleRate;

        if (numSamples <= 0 || numChannels <= 0 || sampleRate <= 0.0) return;
        if (numSamples > 192000 * 600) return;

        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        reader->read(&buffer, 0, numSamples, 0, true, true);

        if (!customSliceRatios.empty())
        {
            sliceFileToZones(audioFile, buffer, sampleRate, customSliceRatios);
            return;
        }

        int blockSize = 256;
        int numBlocks = numSamples / blockSize;

        std::vector<double> sliceRatios;
        sliceRatios.push_back(0.0);

        if (numBlocks > 4)
        {
            std::vector<float> energy(numBlocks, 0.0f);
            for (int b = 0; b < numBlocks; ++b)
            {
                float sum = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    for (int s = 0; s < blockSize; ++s)
                    {
                        int idx = b * blockSize + s;
                        if (idx < numSamples)
                        {
                            float val = buffer.getSample(ch, idx);
                            sum += val * val;
                        }
                    }
                }
                energy[b] = std::sqrt(sum / static_cast<float>(blockSize * numChannels));
            }

            std::vector<float> onset(numBlocks, 0.0f);
            float sumOnset = 0.0f;
            for (int b = 1; b < numBlocks; ++b)
            {
                float diff = energy[b] - energy[b - 1];
                if (diff > 0.0f)
                {
                    onset[b] = diff;
                    sumOnset += diff;
                }
            }

            float meanOnset = sumOnset / static_cast<float>(numBlocks);
            float sqDiffSum = 0.0f;
            for (int b = 1; b < numBlocks; ++b)
            {
                float diff = onset[b] - meanOnset;
                sqDiffSum += diff * diff;
            }
            float stdOnset = std::sqrt(sqDiffSum / static_cast<float>(numBlocks));

            float threshold = std::max(0.005f, meanOnset + 0.35f * stdOnset);
            int minDistanceBlocks = static_cast<int>(0.05 * sampleRate / static_cast<double>(blockSize));
            if (minDistanceBlocks < 1) minDistanceBlocks = 1;
            int lastOnsetBlock = -minDistanceBlocks;

            struct OnsetCandidate
            {
                int blockIdx { 0 };
                float strength { 0.0f };
            };
            std::vector<OnsetCandidate> candidates;

            for (int b = 1; b < numBlocks - 1; ++b)
            {
                if (onset[b] > threshold && onset[b] >= onset[b - 1] && onset[b] >= onset[b + 1])
                {
                    if (b - lastOnsetBlock >= minDistanceBlocks)
                    {
                        candidates.push_back({ b, onset[b] });
                        lastOnsetBlock = b;
                    }
                }
            }

            const size_t maxOnsets = 39; // 39 onsets + 0.0 start = max 40 slices
            if (candidates.size() > maxOnsets)
            {
                std::sort(candidates.begin(), candidates.end(), [](const OnsetCandidate& a, const OnsetCandidate& b) {
                    return a.strength > b.strength;
                });
                candidates.resize(maxOnsets);
                std::sort(candidates.begin(), candidates.end(), [](const OnsetCandidate& a, const OnsetCandidate& b) {
                    return a.blockIdx < b.blockIdx;
                });
            }

            for (const auto& c : candidates)
            {
                double ratio = static_cast<double>(c.blockIdx * blockSize) / static_cast<double>(numSamples);
                sliceRatios.push_back(ratio);
            }
        }

        if (sliceRatios.size() <= 1)
        {
            sliceRatios.clear();
            for (int i = 0; i < 8; ++i)
            {
                sliceRatios.push_back(static_cast<double>(i) / 8.0);
            }
        }

        std::sort(sliceRatios.begin(), sliceRatios.end());
        sliceRatios.erase(std::unique(sliceRatios.begin(), sliceRatios.end()), sliceRatios.end());

        sliceFileToZones(audioFile, buffer, sampleRate, sliceRatios);
    }
    catch (...)
    {
    }
}

void SampleMapComponent::selectZone(int index, bool addToSelection)
{
    if (!addToSelection)
    {
        selectedZoneIndices.clear();
    }
    if (index >= 0 && index < static_cast<int>(zones.size()))
    {
        selectedZoneIndices.insert(index);
        selectedZoneIndex = index;

        const auto& z = zones[index];
        juce::File f(z.filePath);
        if (f.existsAsFile())
        {
            audioEngine.loadFile(f, false, true);
        }

        attackKnob.setValue(z.attackMs, juce::dontSendNotification);
        decayKnob.setValue(z.decayMs, juce::dontSendNotification);
        sustainKnob.setValue(z.sustainLevel, juce::dontSendNotification);
        releaseKnob.setValue(z.releaseMs, juce::dontSendNotification);
    }
    else
    {
        selectedZoneIndex = -1;
    }
    resized();
    repaint();
}

void SampleMapComponent::deselectAllZones()
{
    selectedZoneIndices.clear();
    selectedZoneIndex = -1;
    resized();
    repaint();
}

void SampleMapComponent::clearAllZones()
{
    zones.clear();
    selectedZoneIndex = -1;
    selectedZoneIndices.clear();
    audioEngine.clearMasterSample();
    resized();
    repaint();
    if (onStateChanged) onStateChanged();
}

void SampleMapComponent::autoMapByPitch()
{
    if (zones.empty()) return;

    // Sort zones by root note
    std::sort(zones.begin(), zones.end(), [](const SampleMapZone& a, const SampleMapZone& b) {
        return a.rootNote < b.rootNote;
    });

    int numZones = static_cast<int>(zones.size());
    for (int i = 0; i < numZones; ++i)
    {
        int prevRoot = (i > 0) ? zones[i - 1].rootNote : 0;
        int nextRoot = (i + 1 < numZones) ? zones[i + 1].rootNote : 127;

        zones[i].keyLow = (i == 0) ? 0 : (zones[i].rootNote + prevRoot) / 2;
        zones[i].keyHigh = (i == numZones - 1) ? 127 : (zones[i].rootNote + nextRoot) / 2 - 1;
        zones[i].keyHigh = juce::jmax(zones[i].keyLow, zones[i].keyHigh);
    }

    repaint();
}

void SampleMapComponent::autoMapChromatic()
{
    if (zones.empty()) return;

    int currentKey = 36; // C2
    for (auto& z : zones)
    {
        z.rootNote = currentKey;
        z.keyLow = currentKey;
        z.keyHigh = currentKey;
        z.velLow = 0;
        z.velHigh = 127;
        currentKey = juce::jmin(127, currentKey + 1);
    }

    repaint();
}

void SampleMapComponent::autoMapVelocityLayers()
{
    if (zones.empty()) return;

    // Group zones that share the same key span or rootNote
    std::map<std::pair<int, int>, std::vector<int>> keyGroups;
    for (size_t i = 0; i < zones.size(); ++i)
    {
        keyGroups[{ zones[i].keyLow, zones[i].keyHigh }].push_back(static_cast<int>(i));
    }

    for (const auto& [keyRange, indices] : keyGroups)
    {
        if (indices.size() <= 1) continue;

        int numLayers = static_cast<int>(indices.size());
        int velStep = 128 / numLayers;
        for (int l = 0; l < numLayers; ++l)
        {
            int idx = indices[static_cast<size_t>(l)];
            zones[idx].velLow = l * velStep;
            zones[idx].velHigh = (l == numLayers - 1) ? 127 : ((l + 1) * velStep - 1);
        }
    }

    resized();
    repaint();
    if (onStateChanged) onStateChanged();
}

void SampleMapComponent::autoMapRoundRobin()
{
    if (zones.empty()) return;

    // Group zones that share the same key span (keyLow, keyHigh)
    std::map<std::pair<int, int>, std::vector<int>> keyGroups;
    for (size_t i = 0; i < zones.size(); ++i)
    {
        keyGroups[{ zones[i].keyLow, zones[i].keyHigh }].push_back(static_cast<int>(i));
    }

    for (const auto& [keyRange, indices] : keyGroups)
    {
        // Further sub-group by velocity tier
        std::map<std::pair<int, int>, std::vector<int>> velGroups;
        for (int idx : indices)
        {
            velGroups[{ zones[idx].velLow, zones[idx].velHigh }].push_back(idx);
        }

        for (const auto& [velRange, rrIndices] : velGroups)
        {
            for (size_t rr = 0; rr < rrIndices.size(); ++rr)
            {
                zones[rrIndices[rr]].roundRobinIndex = static_cast<int>(rr + 1);
            }
        }
    }

    resized();
    repaint();
    if (onStateChanged) onStateChanged();
}

// ─────────────────────────────────────────────────────────
//  Paint
// ─────────────────────────────────────────────────────────
void SampleMapComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    auto gridArea = getGridBounds();
    auto keybedArea = getKeybedBounds();
    auto inspectorArea = getInspectorBounds();

    paintZoneGrid(g, gridArea);
    paintKeybed(g, keybedArea);

    // Inspector card background
    g.setColour(OpenWavLookAndFeel::bgCard);
    g.fillRoundedRectangle(inspectorArea, 8.0f);
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.4f));
    g.drawRoundedRectangle(inspectorArea, 8.0f, 1.0f);
}

void SampleMapComponent::paintZoneGrid(juce::Graphics& g, juce::Rectangle<float> area) const
{
    // Background card
    g.setColour(OpenWavLookAndFeel::bgCard);
    g.fillRoundedRectangle(area, 8.0f);
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.4f));
    g.drawRoundedRectangle(area, 8.0f, 1.0f);

    auto inner = area.reduced(2.0f);

    // Grid octaves (every 12 notes)
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.15f));
    for (int note = 0; note <= 128; note += 12)
    {
        float x = xForNoteNumber(note, inner);
        g.drawVerticalLine(static_cast<int>(x), inner.getY(), inner.getBottom());
        g.setFont(juce::Font(9.0f));
        g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.5f));
        g.drawText("C" + juce::String((note / 12) - 1), x + 2.0f, inner.getY() + 2.0f, 30.0f, 12.0f, juce::Justification::left);
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.15f));
    }

    // Velocity grid lines and labels (0, 32, 64, 96, 127)
    g.setFont(juce::Font(9.0f));
    for (int vel : { 32, 64, 96 })
    {
        float y = yForVelocity(vel, inner);
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.2f));
        g.drawHorizontalLine(static_cast<int>(y), inner.getX(), inner.getRight());
        g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.45f));
        g.drawText("v" + juce::String(vel), inner.getRight() - 24.0f, y - 10.0f, 22.0f, 10.0f, juce::Justification::right);
    }

    // Draw mapped zones
    for (size_t i = 0; i < zones.size(); ++i)
    {
        const auto& z = zones[i];
        float x1 = xForNoteNumber(z.keyLow, inner);
        float x2 = xForNoteNumber(z.keyHigh + 1, inner);
        float y1 = yForVelocity(z.velHigh, inner);
        float y2 = yForVelocity(z.velLow, inner);

        juce::Rectangle<float> zRect(x1, y1, std::max(6.0f, x2 - x1), std::max(6.0f, y2 - y1));

        bool isSelected = isZoneSelected(static_cast<int>(i));
        
        // Base color with slight hue shift for different Round Robin layers
        float hueShift = (z.roundRobinIndex > 1) ? (0.07f * (z.roundRobinIndex - 1)) : 0.0f;
        juce::Colour baseAccent = OpenWavLookAndFeel::accentCyan.withRotatedHue(hueShift);
        juce::Colour zoneCol = isSelected ? baseAccent : baseAccent.withAlpha(0.65f);

        // Zone background fill
        g.setColour(zoneCol.withAlpha(isSelected ? 0.28f : 0.16f));
        g.fillRoundedRectangle(zRect, 4.0f);

        // Zone border
        g.setColour(zoneCol);
        g.drawRoundedRectangle(zRect, 4.0f, isSelected ? 2.0f : 1.0f);

        // Root note indicator line
        float rx = xForNoteNumber(z.rootNote, inner) + (xForNoteNumber(z.rootNote + 1, inner) - xForNoteNumber(z.rootNote, inner)) * 0.5f;
        if (rx >= zRect.getX() && rx <= zRect.getRight())
        {
            g.setColour(OpenWavLookAndFeel::favoriteRed.withAlpha(0.8f));
            g.drawLine(rx, zRect.getY(), rx, zRect.getBottom(), 1.5f);
        }

        // Edge resize handles for selected zone card
        if (isSelected)
        {
            g.setColour(OpenWavLookAndFeel::textPrimary);
            float handleSize = 8.0f;
            g.fillRect(juce::Rectangle<float>(zRect.getX() - 2.0f, zRect.getCentreY() - handleSize * 0.5f, 4.0f, handleSize));
            g.fillRect(juce::Rectangle<float>(zRect.getRight() - 2.0f, zRect.getCentreY() - handleSize * 0.5f, 4.0f, handleSize));
            g.fillRect(juce::Rectangle<float>(zRect.getCentreX() - handleSize * 0.5f, zRect.getY() - 2.0f, handleSize, 4.0f));
            g.fillRect(juce::Rectangle<float>(zRect.getCentreX() - handleSize * 0.5f, zRect.getBottom() - 2.0f, handleSize, 4.0f));
        }

        // Zone text label & badges
        g.setColour(OpenWavLookAndFeel::textPrimary);
        g.setFont(juce::Font(10.0f).boldened());

        juce::String labelText = z.sampleName;
        juce::String tag;
        if (z.roundRobinIndex > 1) tag += "RR" + juce::String(z.roundRobinIndex);
        if (z.velLow > 0 || z.velHigh < 127)
        {
            if (tag.isNotEmpty()) tag += " ";
            tag += "v:" + juce::String(z.velLow) + "-" + juce::String(z.velHigh);
        }
        if (tag.isNotEmpty() && zRect.getHeight() > 24.0f)
            labelText += " [" + tag + "]";

        g.drawText(labelText, zRect.reduced(4.0f, 2.0f), juce::Justification::centred, true);
    }

    // ── Velocity Recorded Hit Dots ──
    uint32_t nowTime = juce::Time::getMillisecondCounter();
    for (const auto& dot : recentHitDots)
    {
        if (dot.note < 0 || dot.note > 127) continue;

        bool isHeld = (activeNoteVelocities[static_cast<size_t>(dot.note)] >= 0);
        float alpha = 1.0f;
        if (!isHeld)
        {
            float ageMs = static_cast<float>(nowTime - dot.timestampMs);
            alpha = juce::jlimit(0.0f, 1.0f, 1.0f - (ageMs / 1200.0f));
        }

        if (alpha <= 0.01f) continue;

        float dotX = xForNoteNumber(dot.note, inner) + (xForNoteNumber(dot.note + 1, inner) - xForNoteNumber(dot.note, inner)) * 0.5f;
        float dotY = yForVelocity(dot.vel, inner);

        // Outer ambient glow
        g.setColour(juce::Colours::white.withAlpha(0.22f * alpha));
        g.fillEllipse(dotX - 9.0f, dotY - 9.0f, 18.0f, 18.0f);

        // Middle halo
        g.setColour(juce::Colours::white.withAlpha(0.55f * alpha));
        g.fillEllipse(dotX - 5.5f, dotY - 5.5f, 11.0f, 11.0f);

        // Core bright white dot
        g.setColour(juce::Colours::white.withAlpha(0.98f * alpha));
        g.fillEllipse(dotX - 3.0f, dotY - 3.0f, 6.0f, 6.0f);

        // Little velocity label badge beside dot
        g.setFont(juce::Font(9.0f).boldened());
        g.setColour(juce::Colours::white.withAlpha(0.9f * alpha));
        g.drawText("v" + juce::String(dot.vel), dotX + 6.0f, dotY - 7.0f, 32.0f, 14.0f, juce::Justification::left);
    }

    // Rubber-band lasso selection box
    if (activeDragTarget == DragTarget::BoxSelect && !lassoRect.isEmpty())
    {
        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.2f));
        g.fillRect(lassoRect);
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.drawRect(lassoRect, 1.5f);
    }
}

void SampleMapComponent::paintKeybed(juce::Graphics& g, juce::Rectangle<float> area) const
{
    g.setColour(OpenWavLookAndFeel::bgHeader);
    g.fillRoundedRectangle(area, 4.0f);

    float noteWidth = area.getWidth() / 128.0f;

    // Draw 128 keys (white & black)
    for (int note = 0; note < 128; ++note)
    {
        float kx = area.getX() + note * noteWidth;
        int noteInOctave = note % 12;
        bool isBlackKey = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10);

        juce::Rectangle<float> keyRect(kx, area.getY(), noteWidth, area.getHeight());

        bool isMidiPressed = activeMidiNotes[static_cast<size_t>(note)];
        if (note == auditionNote || isMidiPressed)
        {
            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.fillRect(keyRect);
        }
        else if (isBlackKey)
        {
            g.setColour(juce::Colours::black.withAlpha(0.8f));
            g.fillRect(keyRect.withHeight(area.getHeight() * 0.6f));
        }
        else
        {
            g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.15f));
            g.fillRect(keyRect);
            g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.3f));
            g.drawRect(keyRect, 0.5f);
        }
    }
}

// ─────────────────────────────────────────────────────────
//  Mouse Interaction
// ─────────────────────────────────────────────────────────
void SampleMapComponent::mouseDown(const juce::MouseEvent& e)
{
    auto gridArea = getGridBounds();
    auto keybedArea = getKeybedBounds();

    if (keybedArea.contains(e.position))
    {
        auditionNote = noteNumberAtX(e.x, keybedArea);
        activeDragTarget = DragTarget::KeybedAudition;

        for (size_t i = 0; i < zones.size(); ++i)
        {
            const auto& z = zones[i];
            if (auditionNote >= z.keyLow && auditionNote <= z.keyHigh)
            {
                selectedZoneIndex = static_cast<int>(i);
                selectedZoneIndices.clear();
                selectedZoneIndices.insert(static_cast<int>(i));

                if (z.filePath.isNotEmpty())
                {
                    juce::File f(z.filePath);
                    if (f.existsAsFile() && audioEngine.getCurrentFile() != f)
                    {
                        audioEngine.loadFile(f, false, true);
                    }
                }
                break;
            }
        }

        audioEngine.getKeyboardState().noteOn(1, auditionNote, 0.8f);
        resized();
        repaint();
        return;
    }

    if (gridArea.contains(e.position))
    {
        auto inner = gridArea.reduced(2.0f);
        float edgeThreshold = 8.0f;
        bool isMulti = e.mods.isCommandDown() || e.mods.isCtrlDown() || e.mods.isShiftDown();

        for (int i = static_cast<int>(zones.size()) - 1; i >= 0; --i)
        {
            const auto& z = zones[i];
            float x1 = xForNoteNumber(z.keyLow, inner);
            float x2 = xForNoteNumber(z.keyHigh + 1, inner);
            float y1 = yForVelocity(z.velHigh, inner);
            float y2 = yForVelocity(z.velLow, inner);

            juce::Rectangle<float> zRect(x1, y1, std::max(8.0f, x2 - x1), std::max(8.0f, y2 - y1));

            if (zRect.expanded(edgeThreshold).contains(e.position))
            {
                if (isMulti)
                {
                    if (isZoneSelected(i))
                        selectedZoneIndices.erase(i);
                    else
                        selectedZoneIndices.insert(i);
                    selectedZoneIndex = i;
                }
                else if (!isZoneSelected(i))
                {
                    selectZone(i, false);
                }

                activeDragZone = i;

                dragStartNote = noteNumberAtX(e.x, gridArea);
                dragStartVel = velocityAtY(e.y, gridArea);
                dragStartZones.clear();
                for (int sIdx : selectedZoneIndices)
                {
                    if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()))
                        dragStartZones[sIdx] = zones[sIdx];
                }

                juce::File fileToLoad(z.filePath);
                if (fileToLoad.existsAsFile())
                {
                    audioEngine.loadFile(fileToLoad, false, true);
                }

                float zoneW = zRect.getWidth();
                float effectiveEdgeThreshold = (zoneW < 24.0f) ? std::min(3.0f, zoneW * 0.25f) : edgeThreshold;

                float dLeft = std::abs(e.x - zRect.getX());
                float dRight = std::abs(e.x - zRect.getRight());
                float dTop = std::abs(e.y - zRect.getY());
                float dBottom = std::abs(e.y - zRect.getBottom());

                if (e.mods.isAltDown())
                {
                    activeDragTarget = DragTarget::MoveZone;
                }
                else if (zoneW < 24.0f && e.x > zRect.getX() + effectiveEdgeThreshold && e.x < zRect.getRight() - effectiveEdgeThreshold)
                {
                    activeDragTarget = DragTarget::MoveZone;
                }
                else if (dLeft <= effectiveEdgeThreshold) activeDragTarget = DragTarget::ResizeKeyLow;
                else if (dRight <= effectiveEdgeThreshold) activeDragTarget = DragTarget::ResizeKeyHigh;
                else if (dTop <= edgeThreshold) activeDragTarget = DragTarget::ResizeVelHigh;
                else if (dBottom <= edgeThreshold) activeDragTarget = DragTarget::ResizeVelLow;
                else activeDragTarget = DragTarget::MoveZone;

                resized();
                repaint();
                return;
            }
        }

        if (!isMulti) deselectAllZones();
        activeDragTarget = DragTarget::BoxSelect;
        boxSelectStartPos = e.position;
        lassoRect = juce::Rectangle<float>(e.x, e.y, 0, 0);
        repaint();
    }
}

void SampleMapComponent::mouseMove(const juce::MouseEvent& e)
{
    auto gridArea = getGridBounds();
    if (!gridArea.contains(e.position))
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }

    auto inner = gridArea.reduced(2.0f);
    float edgeThreshold = 8.0f;

    for (int i = static_cast<int>(zones.size()) - 1; i >= 0; --i)
    {
        const auto& z = zones[i];
        float x1 = xForNoteNumber(z.keyLow, inner);
        float x2 = xForNoteNumber(z.keyHigh + 1, inner);
        float y1 = yForVelocity(z.velHigh, inner);
        float y2 = yForVelocity(z.velLow, inner);

        juce::Rectangle<float> zRect(x1, y1, std::max(8.0f, x2 - x1), std::max(8.0f, y2 - y1));

        if (zRect.expanded(edgeThreshold).contains(e.position))
        {
            float zoneW = zRect.getWidth();
            float effectiveEdgeThreshold = (zoneW < 24.0f) ? std::min(3.0f, zoneW * 0.25f) : edgeThreshold;

            float dLeft = std::abs(e.x - zRect.getX());
            float dRight = std::abs(e.x - zRect.getRight());
            float dTop = std::abs(e.y - zRect.getY());
            float dBottom = std::abs(e.y - zRect.getBottom());

            if (dLeft <= effectiveEdgeThreshold || dRight <= effectiveEdgeThreshold)
            {
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                return;
            }
            if (dTop <= edgeThreshold || dBottom <= edgeThreshold)
            {
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
                return;
            }
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            return;
        }
    }

    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void SampleMapComponent::mouseDrag(const juce::MouseEvent& e)
{
    auto gridArea = getGridBounds();

    if (activeDragTarget == DragTarget::BoxSelect)
    {
        float curX = juce::jlimit(gridArea.getX(), gridArea.getRight(), e.position.x);
        float curY = juce::jlimit(gridArea.getY(), gridArea.getBottom(), e.position.y);
        float startX = juce::jlimit(gridArea.getX(), gridArea.getRight(), boxSelectStartPos.x);
        float startY = juce::jlimit(gridArea.getY(), gridArea.getBottom(), boxSelectStartPos.y);

        float lx = std::min(startX, curX);
        float ly = std::min(startY, curY);
        float lw = std::abs(curX - startX);
        float lh = std::abs(curY - startY);
        lassoRect = juce::Rectangle<float>(lx, ly, lw, lh).getIntersection(gridArea);

        auto inner = gridArea.reduced(2.0f);
        if (!e.mods.isCommandDown() && !e.mods.isCtrlDown() && !e.mods.isShiftDown())
            selectedZoneIndices.clear();

        for (int i = 0; i < static_cast<int>(zones.size()); ++i)
        {
            const auto& z = zones[i];
            float x1 = xForNoteNumber(z.keyLow, inner);
            float x2 = xForNoteNumber(z.keyHigh + 1, inner);
            float y1 = yForVelocity(z.velHigh, inner);
            float y2 = yForVelocity(z.velLow, inner);

            juce::Rectangle<float> zRect(x1, y1, std::max(8.0f, x2 - x1), std::max(8.0f, y2 - y1));
            if (lassoRect.intersects(zRect))
            {
                selectedZoneIndices.insert(i);
                selectedZoneIndex = i;
            }
        }
        resized();
        repaint();
        return;
    }

    if (activeDragZone < 0 || activeDragZone >= static_cast<int>(zones.size())) return;

    int currentNote = noteNumberAtX(e.x, gridArea);
    int currentVel = velocityAtY(e.y, gridArea);
    int noteDelta = currentNote - dragStartNote;
    int velDelta = currentVel - dragStartVel;

    if (activeDragTarget == DragTarget::MoveZone)
    {
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()) && dragStartZones.count(sIdx) > 0)
            {
                const auto& startZ = dragStartZones[sIdx];
                int span = startZ.keyHigh - startZ.keyLow;
                int newLow = juce::jlimit(0, 127 - span, startZ.keyLow + noteDelta);

                auto& z = zones[sIdx];
                z.keyLow = newLow;
                z.keyHigh = newLow + span;
                z.rootNote = juce::jlimit(z.keyLow, z.keyHigh, startZ.rootNote + noteDelta);

                int vSpan = startZ.velHigh - startZ.velLow;
                int newVelLow = juce::jlimit(0, 127 - vSpan, startZ.velLow + velDelta);
                z.velLow = newVelLow;
                z.velHigh = newVelLow + vSpan;
            }
        }
    }
    else if (activeDragTarget == DragTarget::ResizeKeyLow)
    {
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()) && dragStartZones.count(sIdx) > 0)
            {
                const auto& startZ = dragStartZones[sIdx];
                auto& z = zones[sIdx];
                z.keyLow = juce::jmin(z.keyHigh, juce::jlimit(0, 127, startZ.keyLow + noteDelta));
                z.rootNote = juce::jlimit(z.keyLow, z.keyHigh, z.rootNote);
            }
        }
    }
    else if (activeDragTarget == DragTarget::ResizeKeyHigh)
    {
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()) && dragStartZones.count(sIdx) > 0)
            {
                const auto& startZ = dragStartZones[sIdx];
                auto& z = zones[sIdx];
                z.keyHigh = juce::jmax(z.keyLow, juce::jlimit(0, 127, startZ.keyHigh + noteDelta));
                z.rootNote = juce::jlimit(z.keyLow, z.keyHigh, z.rootNote);
            }
        }
    }
    else if (activeDragTarget == DragTarget::ResizeVelLow)
    {
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()) && dragStartZones.count(sIdx) > 0)
            {
                const auto& startZ = dragStartZones[sIdx];
                auto& z = zones[sIdx];
                z.velLow = juce::jmin(z.velHigh, juce::jlimit(0, 127, startZ.velLow + velDelta));
            }
        }
    }
    else if (activeDragTarget == DragTarget::ResizeVelHigh)
    {
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()) && dragStartZones.count(sIdx) > 0)
            {
                const auto& startZ = dragStartZones[sIdx];
                auto& z = zones[sIdx];
                z.velHigh = juce::jmax(z.velLow, juce::jlimit(0, 127, startZ.velHigh + velDelta));
            }
        }
    }

    resized();
    repaint();
}

void SampleMapComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    if (auditionNote >= 0)
    {
        audioEngine.getKeyboardState().noteOff(1, auditionNote, 0.0f);
    }

    bool wasDraggingZone = (activeDragTarget == DragTarget::MoveZone ||
                            activeDragTarget == DragTarget::ResizeKeyLow ||
                            activeDragTarget == DragTarget::ResizeKeyHigh ||
                            activeDragTarget == DragTarget::ResizeVelLow ||
                            activeDragTarget == DragTarget::ResizeVelHigh);

    activeDragTarget = DragTarget::None;
    activeDragZone = -1;
    auditionNote = -1;
    lassoRect = juce::Rectangle<float>();
    dragStartZones.clear();
    repaint();

    if (wasDraggingZone && onStateChanged)
    {
        onStateChanged();
    }
}

// ─────────────────────────────────────────────────────────
//  Layout
// ─────────────────────────────────────────────────────────
void SampleMapComponent::resized()
{
    auto area = getLocalBounds().reduced(20, 16);

    // ── Single Top Toolbar ──
    auto topRow = area.removeFromTop(28);
    int gap = 5;

    addSampleButton.setBounds(topRow.removeFromLeft(82));
    topRow.removeFromLeft(gap);
    autoMapPitchButton.setBounds(topRow.removeFromLeft(78));
    topRow.removeFromLeft(gap);
    autoMapChromaticButton.setBounds(topRow.removeFromLeft(95));
    topRow.removeFromLeft(gap);
    autoMapVelButton.setBounds(topRow.removeFromLeft(88));
    topRow.removeFromLeft(gap);
    autoMapRRButton.setBounds(topRow.removeFromLeft(70));
    topRow.removeFromLeft(gap);
    clearMapButton.setBounds(topRow.removeFromLeft(70));
    topRow.removeFromLeft(gap);
    roundRobinButton.setBounds(topRow.removeFromLeft(88));
    topRow.removeFromLeft(gap);
    pitchTrackButton.setBounds(topRow.removeFromLeft(98));
    topRow.removeFromLeft(gap);
    oneShotButton.setBounds(topRow.removeFromLeft(95));
    topRow.removeFromLeft(gap);
    loopButton.setBounds(topRow.removeFromLeft(75));

    attackKnob.setVisible(false);
    attackLabel.setVisible(false);
    decayKnob.setVisible(false);
    decayLabel.setVisible(false);
    sustainKnob.setVisible(false);
    sustainLabel.setVisible(false);
    releaseKnob.setVisible(false);
    releaseLabel.setVisible(false);

    // ── Inspector Panel Layout ─────────────────────────
    auto inspectorArea = getInspectorBounds().reduced(14, 10);
    int rowH = 20;
    int labelW = 85;
    int rowGap = 4;

    inspectorTitle.setBounds(inspectorArea.removeFromTop(22).toNearestInt());
    inspectorArea.removeFromTop(4);

    if (selectedZoneIndex >= 0 && selectedZoneIndex < static_cast<int>(zones.size()))
    {
        const auto& z = zones[selectedZoneIndex];
        sampleNameValue.setText(z.sampleName, juce::dontSendNotification);
        inspectorArea.removeFromTop(rowH);

        auto setupRow = [&](juce::Label& lbl, juce::Slider& slider, double val) {
            auto r = inspectorArea.removeFromTop(rowH);
            inspectorArea.removeFromTop(rowGap);
            lbl.setBounds(r.removeFromLeft(labelW).toNearestInt());
            r.removeFromLeft(4);
            slider.setBounds(r.toNearestInt());
            slider.setValue(val, juce::dontSendNotification);
        };

        setupRow(rootNoteTitle, rootNoteSlider, z.rootNote);
        setupRow(keyLowTitle, keyLowSlider, z.keyLow);
        setupRow(keyHighTitle, keyHighSlider, z.keyHigh);
        setupRow(velLowTitle, velLowSlider, z.velLow);
        setupRow(velHighTitle, velHighSlider, z.velHigh);
        setupRow(rrTitle, rrSlider, z.roundRobinIndex);
        setupRow(tuneTitle, tuneSlider, z.fineTuneCents);
        setupRow(gainTitle, gainSlider, z.gainDb);

        inspectorArea.removeFromTop(6);
        setupRow(attackTitle, attackSlider, z.attackMs);
        setupRow(decayTitle, decaySlider, z.decayMs);
        setupRow(sustainTitle, sustainSlider, z.sustainLevel);
        setupRow(releaseTitle, releaseSlider, z.releaseMs);
        setupRow(reverbTitle, reverbSlider, audioEngine.getSamplerReverbAmount() * 100.0f);
    }
    else
    {
        sampleNameValue.setText("No Zone Selected", juce::dontSendNotification);
        sampleNameValue.setBounds(inspectorArea.removeFromTop(rowH).toNearestInt());
    }
}

// ─────────────────────────────────────────────────────────
//  Listeners
// ─────────────────────────────────────────────────────────
void SampleMapComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &attackKnob || slider == &attackSlider)
    {
        float val = static_cast<float>(slider->getValue());
        globalAttackMs = val;
        attackKnob.setValue(val, juce::dontSendNotification);
        attackSlider.setValue(val, juce::dontSendNotification);
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()))
                zones[sIdx].attackMs = val;
        }
        repaint();
        return;
    }
    else if (slider == &decayKnob || slider == &decaySlider)
    {
        float val = static_cast<float>(slider->getValue());
        globalDecayMs = val;
        decayKnob.setValue(val, juce::dontSendNotification);
        decaySlider.setValue(val, juce::dontSendNotification);
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()))
                zones[sIdx].decayMs = val;
        }
        repaint();
        return;
    }
    else if (slider == &sustainKnob || slider == &sustainSlider)
    {
        float val = static_cast<float>(slider->getValue());
        globalSustainLevel = val;
        sustainKnob.setValue(val, juce::dontSendNotification);
        sustainSlider.setValue(val, juce::dontSendNotification);
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()))
                zones[sIdx].sustainLevel = val;
        }
        repaint();
        return;
    }
    else if (slider == &releaseKnob || slider == &releaseSlider)
    {
        float val = static_cast<float>(slider->getValue());
        globalReleaseMs = val;
        releaseKnob.setValue(val, juce::dontSendNotification);
        releaseSlider.setValue(val, juce::dontSendNotification);
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()))
                zones[sIdx].releaseMs = val;
        }
        repaint();
        return;
    }

    if (selectedZoneIndex < 0 || selectedZoneIndex >= static_cast<int>(zones.size()))
        return;

    auto& z = zones[selectedZoneIndex];

    if (slider == &rootNoteSlider) z.rootNote = static_cast<int>(rootNoteSlider.getValue());
    else if (slider == &keyLowSlider) z.keyLow = juce::jmin(z.keyHigh, static_cast<int>(keyLowSlider.getValue()));
    else if (slider == &keyHighSlider) z.keyHigh = juce::jmax(z.keyLow, static_cast<int>(keyHighSlider.getValue()));
    else if (slider == &velLowSlider) z.velLow = juce::jmin(z.velHigh, static_cast<int>(velLowSlider.getValue()));
    else if (slider == &velHighSlider) z.velHigh = juce::jmax(z.velLow, static_cast<int>(velHighSlider.getValue()));
    else if (slider == &rrSlider) z.roundRobinIndex = static_cast<int>(rrSlider.getValue());
    else if (slider == &tuneSlider) z.fineTuneCents = static_cast<float>(tuneSlider.getValue());
    else if (slider == &gainSlider) z.gainDb = static_cast<float>(gainSlider.getValue());
    else if (slider == &reverbSlider)
    {
        float val = static_cast<float>(reverbSlider.getValue() / 100.0f);
        audioEngine.setSamplerReverbAmount(val);
        repaint();
        return;
    }

    repaint();
    if (onStateChanged) onStateChanged();
}

void SampleMapComponent::buttonClicked(juce::Button* button)
{
    if (button == &addSampleButton)
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Audio Samples to Map",
            juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
            "*.wav;*.mp3;*.flac;*.aiff;*.aif;*.aifc;*.ogg");

        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectMultipleItems, [this, chooser](const juce::FileChooser& fc) {
            auto results = fc.getResults();
            for (const auto& file : results)
            {
                addSampleFile(file);
            }
        });
    }
    else if (button == &autoMapPitchButton) autoMapByPitch();
    else if (button == &autoMapChromaticButton) autoMapChromatic();
    else if (button == &autoMapVelButton) autoMapVelocityLayers();
    else if (button == &autoMapRRButton) autoMapRoundRobin();
    else if (button == &clearMapButton) clearAllZones();
}

bool SampleMapComponent::keyPressed(const juce::KeyPress& key)
{
    if (selectedZoneIndices.empty())
        return false;

    int deltaNote = 0;
    int deltaVel = 0;

    if (key == juce::KeyPress::leftKey) deltaNote = -1;
    else if (key == juce::KeyPress::rightKey) deltaNote = 1;
    else if (key == juce::KeyPress::upKey) deltaVel = 4;
    else if (key == juce::KeyPress::downKey) deltaVel = -4;

    if (deltaNote != 0 || deltaVel != 0)
    {
        for (int idx : selectedZoneIndices)
        {
            if (idx >= 0 && idx < static_cast<int>(zones.size()))
            {
                if (deltaNote != 0)
                {
                    int span = zones[idx].keyHigh - zones[idx].keyLow;
                    int newLow = juce::jlimit(0, 127 - span, zones[idx].keyLow + deltaNote);
                    zones[idx].keyLow = newLow;
                    zones[idx].keyHigh = newLow + span;
                    zones[idx].rootNote = juce::jlimit(zones[idx].keyLow, zones[idx].keyHigh, zones[idx].rootNote + deltaNote);
                }
                if (deltaVel != 0)
                {
                    int span = zones[idx].velHigh - zones[idx].velLow;
                    int newLow = juce::jlimit(0, 127 - span, zones[idx].velLow + deltaVel);
                    zones[idx].velLow = newLow;
                    zones[idx].velHigh = newLow + span;
                }
            }
        }
        resized();
        repaint();
        if (onStateChanged) onStateChanged();
        return true;
    }

    return false;
}

SampleMapState SampleMapComponent::getState() const
{
    SampleMapState s;
    for (const auto& z : zones)
    {
        SampleMapZoneState zs;
        zs.filePath = z.filePath;
        zs.sampleName = z.sampleName;
        zs.rootNote = z.rootNote;
        zs.keyLow = z.keyLow;
        zs.keyHigh = z.keyHigh;
        zs.velLow = z.velLow;
        zs.velHigh = z.velHigh;
        zs.roundRobinIndex = z.roundRobinIndex;
        zs.fineTuneCents = z.fineTuneCents;
        zs.gainDb = z.gainDb;
        zs.attackMs = z.attackMs;
        zs.decayMs = z.decayMs;
        zs.sustainLevel = z.sustainLevel;
        zs.releaseMs = z.releaseMs;
        s.zones.push_back(zs);
    }
    s.globalAttackMs = globalAttackMs;
    s.globalDecayMs = globalDecayMs;
    s.globalSustainLevel = globalSustainLevel;
    s.globalReleaseMs = globalReleaseMs;
    s.samplerReverbAmount = audioEngine.getSamplerReverbAmount();
    s.pitchTrackingEnabled = audioEngine.isPitchTrackingEnabled();
    s.roundRobinMode = roundRobinMode;
    return s;
}

void SampleMapComponent::setState(const SampleMapState& state)
{
    zones.clear();
    selectedZoneIndex = -1;
    selectedZoneIndices.clear();

    for (const auto& zs : state.zones)
    {
        SampleMapZone z;
        z.filePath = zs.filePath;
        z.sampleName = zs.sampleName;
        z.rootNote = zs.rootNote;
        z.keyLow = zs.keyLow;
        z.keyHigh = zs.keyHigh;
        z.velLow = zs.velLow;
        z.velHigh = zs.velHigh;
        z.roundRobinIndex = zs.roundRobinIndex;
        z.fineTuneCents = zs.fineTuneCents;
        z.gainDb = zs.gainDb;
        z.attackMs = zs.attackMs;
        z.decayMs = zs.decayMs;
        z.sustainLevel = zs.sustainLevel;
        z.releaseMs = zs.releaseMs;
        z.isSelected = false;
        zones.push_back(z);
    }

    globalAttackMs = state.globalAttackMs;
    globalDecayMs = state.globalDecayMs;
    globalSustainLevel = state.globalSustainLevel;
    globalReleaseMs = state.globalReleaseMs;

    attackKnob.setValue(globalAttackMs, juce::dontSendNotification);
    decayKnob.setValue(globalDecayMs, juce::dontSendNotification);
    sustainKnob.setValue(globalSustainLevel, juce::dontSendNotification);
    releaseKnob.setValue(globalReleaseMs, juce::dontSendNotification);

    reverbSlider.setValue(state.samplerReverbAmount, juce::dontSendNotification);
    audioEngine.setSamplerReverbAmount(state.samplerReverbAmount);

    audioEngine.setPitchTrackingEnabled(state.pitchTrackingEnabled);
    pitchTrackButton.setToggleState(state.pitchTrackingEnabled, juce::dontSendNotification);
    pitchTrackButton.setButtonText(state.pitchTrackingEnabled ? "Pitch Track: ON" : "Pitch Track: OFF");

    roundRobinMode = state.roundRobinMode;
    roundRobinButton.setButtonText(roundRobinMode == 0 ? "RR: Cycle" : (roundRobinMode == 1 ? "RR: Random" : "RR: OFF"));

    if (!zones.empty())
    {
        selectedZoneIndex = 0;
        selectedZoneIndices.insert(0);
        juce::File firstSlice(zones[0].filePath);
        if (firstSlice.existsAsFile())
        {
            audioEngine.loadFile(firstSlice, false, true);
        }
    }

    resized();
    repaint();
}

} // namespace openwav

