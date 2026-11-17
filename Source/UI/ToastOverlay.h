#pragma once

#include "OpenWavLookAndFeel.h"

namespace openwav
{

/**
 * Non-blocking auto-fading toast notification overlay.
 * Hosted by PluginEditor; any component can call ToastOverlay::show(...).
 */
class ToastOverlay : public juce::Component,
                     private juce::Timer
{
public:
    ToastOverlay()
    {
        setInterceptsMouseClicks(false, false);
        setVisible(false);
        activeInstance() = this;
    }

    ~ToastOverlay() override
    {
        stopTimer();
        if (activeInstance() == this)
            activeInstance() = nullptr;
    }

    static void show(const juce::String& message, bool isError = false)
    {
        if (message.isEmpty())
            return;

        juce::MessageManager::callAsync([message, isError] {
            if (auto* inst = activeInstance())
                inst->trigger(message, isError);
        });
    }

    void trigger(const juce::String& msg, bool error)
    {
        currentMessage = msg;
        isErrorState = error;
        alpha = 0.0f;
        holdTicks = 72; // ~2.4s at 30 Hz
        fadingOut = false;
        setVisible(true);
        toFront(false);
        startTimerHz(30);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        if (currentMessage.isEmpty() || alpha <= 0.01f)
            return;

        auto font = OpenWavLookAndFeel::getBodyFont(12.0f, true);
        juce::GlyphArrangement ga;
        ga.addLineOfText(font, currentMessage, 0.0f, 0.0f);
        float textW = ga.getBoundingBox(0, -1, false).getWidth();

        float pillW = juce::jlimit(160.0f, 420.0f, textW + 44.0f);
        float pillH = 34.0f;
        auto pill = juce::Rectangle<float>(
            static_cast<float>(getWidth()) - pillW - 18.0f,
            static_cast<float>(getHeight()) - pillH - 18.0f,
            pillW, pillH);

        g.setColour(juce::Colours::black.withAlpha(0.35f * alpha));
        g.fillRoundedRectangle(pill.translated(0.0f, 2.0f), 8.0f);

        g.setColour(OpenWavLookAndFeel::bgHeader.withAlpha(0.96f * alpha));
        g.fillRoundedRectangle(pill, 8.0f);

        auto accent = isErrorState ? OpenWavLookAndFeel::favoriteRed : OpenWavLookAndFeel::accentCyan;
        g.setColour(accent.withAlpha(0.75f * alpha));
        g.drawRoundedRectangle(pill.reduced(0.5f), 8.0f, 1.0f);

        // Status indicator dot
        g.setColour(accent.withAlpha(alpha));
        g.fillEllipse(pill.getX() + 14.0f, pill.getCentreY() - 3.5f, 7.0f, 7.0f);

        g.setColour(OpenWavLookAndFeel::textPrimary.withAlpha(alpha));
        g.setFont(font);
        g.drawFittedText(currentMessage, pill.withTrimmedLeft(28.0f).reduced(8.0f, 2.0f).toNearestInt(),
                         juce::Justification::centredLeft, 1);
    }

private:
    static ToastOverlay*& activeInstance()
    {
        static ToastOverlay* instance = nullptr;
        return instance;
    }

    void timerCallback() override
    {
        if (!fadingOut)
        {
            alpha = std::min(1.0f, alpha + 0.22f);
            if (alpha >= 1.0f)
            {
                if (--holdTicks <= 0)
                    fadingOut = true;
            }
        }
        else
        {
            alpha = std::max(0.0f, alpha - 0.10f);
            if (alpha <= 0.0f)
            {
                stopTimer();
                setVisible(false);
            }
        }
        repaint();
    }

    juce::String currentMessage;
    bool isErrorState { false };
    float alpha { 0.0f };
    int holdTicks { 0 };
    bool fadingOut { false };
};

} // namespace openwav
