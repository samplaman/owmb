#include "OpenWavLookAndFeel.h"

namespace openwav
{

const juce::Colour OpenWavLookAndFeel::bgDark        = juce::Colour::fromRGB(244, 246, 250);
const juce::Colour OpenWavLookAndFeel::bgHeader      = juce::Colour::fromRGB(232, 236, 244);
const juce::Colour OpenWavLookAndFeel::bgCard        = juce::Colour::fromRGB(255, 255, 255);
const juce::Colour OpenWavLookAndFeel::bgHover       = juce::Colour::fromRGB(224, 230, 242);
const juce::Colour OpenWavLookAndFeel::accentCyan    = juce::Colour::fromRGB(0, 140, 210);
const juce::Colour OpenWavLookAndFeel::accentBlue    = juce::Colour::fromRGB(30, 90, 220);
const juce::Colour OpenWavLookAndFeel::textPrimary   = juce::Colour::fromRGB(20, 24, 33);
const juce::Colour OpenWavLookAndFeel::textSecondary = juce::Colour::fromRGB(95, 110, 130);
const juce::Colour OpenWavLookAndFeel::borderColour  = juce::Colour::fromRGB(210, 218, 230);
const juce::Colour OpenWavLookAndFeel::favoriteRed   = juce::Colour::fromRGB(235, 45, 85);

OpenWavLookAndFeel::OpenWavLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, bgDark);

    // Text Editor
    setColour(juce::TextEditor::backgroundColourId, bgCard);
    setColour(juce::TextEditor::textColourId, textPrimary);
    setColour(juce::TextEditor::highlightColourId, accentCyan.withAlpha(0.25f));
    setColour(juce::TextEditor::outlineColourId, borderColour);
    setColour(juce::TextEditor::focusedOutlineColourId, accentCyan);

    // TextButton - Explicit Light Mode Defaults
    setColour(juce::TextButton::buttonColourId, bgCard);
    setColour(juce::TextButton::buttonOnColourId, accentCyan.withAlpha(0.18f));
    setColour(juce::TextButton::textColourOffId, textPrimary);
    setColour(juce::TextButton::textColourOnId, accentCyan);

    // ToggleButton
    setColour(juce::ToggleButton::textColourId, textPrimary);
    setColour(juce::ToggleButton::tickColourId, accentCyan);
    setColour(juce::ToggleButton::tickDisabledColourId, textSecondary);

    // ListBox & TableHeader
    setColour(juce::ListBox::backgroundColourId, bgDark);
    setColour(juce::TableHeaderComponent::backgroundColourId, bgHeader);
    setColour(juce::TableHeaderComponent::textColourId, textSecondary);

    // PopupMenu
    setColour(juce::PopupMenu::backgroundColourId, bgCard);
    setColour(juce::PopupMenu::textColourId, textPrimary);
    setColour(juce::PopupMenu::headerTextColourId, accentCyan);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, bgHover);
    setColour(juce::PopupMenu::highlightedTextColourId, textPrimary);

    // Slider
    setColour(juce::Slider::thumbColourId, textPrimary);
    setColour(juce::Slider::trackColourId, accentCyan);
    setColour(juce::Slider::backgroundColourId, borderColour);
}

void OpenWavLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                               const juce::Colour& backgroundColour,
                                               bool shouldDrawButtonAsHighlighted,
                                               bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto cornerRadius = 5.0f;

    // Light mode fill determination
    juce::Colour fillColour = bgCard;

    if (backgroundColour != juce::Colour(0xff444444) && !backgroundColour.isTransparent() && backgroundColour != bgDark)
    {
        fillColour = backgroundColour;
    }

    if (button.getToggleState())
    {
        fillColour = accentCyan.withAlpha(0.18f);
    }
    else if (shouldDrawButtonAsDown)
    {
        fillColour = accentBlue.withAlpha(0.35f);
    }
    else if (shouldDrawButtonAsHighlighted)
    {
        fillColour = bgHover;
    }

    g.setColour(fillColour);
    g.fillRoundedRectangle(bounds, cornerRadius);

    juce::Colour border = button.getToggleState() ? accentCyan : borderColour;
    float stroke = button.getToggleState() ? 1.4f : 1.0f;
    g.setColour(border);
    g.drawRoundedRectangle(bounds, cornerRadius, stroke);
}

void OpenWavLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                          bool /*shouldDrawButtonAsHighlighted*/,
                                          bool /*shouldDrawButtonAsDown*/)
{
    g.setFont(juce::Font(12.0f).boldened());
    g.setColour(button.getToggleState() ? accentCyan : textPrimary);
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, true);
}

void OpenWavLookAndFeel::drawTableHeaderBackground(juce::Graphics& g, juce::TableHeaderComponent& header)
{
    g.fillAll(bgHeader);
    g.setColour(borderColour);
    g.drawRect(header.getLocalBounds().removeFromBottom(1));
}

void OpenWavLookAndFeel::drawTableHeaderColumn(juce::Graphics& g, juce::TableHeaderComponent& /*header*/,
                                               const juce::String& columnName, int /*columnId*/,
                                               int width, int height, bool isMouseOver,
                                               bool /*isMouseDown*/, int /*columnFlags*/)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(8, 0);

    if (isMouseOver)
    {
        g.setColour(bgHover);
        g.fillRect(0, 0, width, height);
    }

    g.setFont(juce::Font(11.0f).boldened());
    g.setColour(isMouseOver ? textPrimary : textSecondary);
    g.drawText(columnName.toUpperCase(), bounds, juce::Justification::centredLeft, true);

    g.setColour(borderColour);
    g.drawLine(static_cast<float>(width - 1), 0.0f, static_cast<float>(width - 1), static_cast<float>(height));
}

void OpenWavLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& textEditor)
{
    auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
    g.setColour(textEditor.hasKeyboardFocus(true) ? accentCyan : borderColour);
    g.drawRoundedRectangle(bounds, 5.0f, textEditor.hasKeyboardFocus(true) ? 1.4f : 1.0f);
}

void OpenWavLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                          const juce::Slider::SliderStyle /*style*/, juce::Slider& slider)
{
    auto isHorizontal = slider.isHorizontal();
    float trackThickness = 4.0f;

    if (isHorizontal)
    {
        float centerY = y + height * 0.5f;

        // Background track
        g.setColour(borderColour);
        g.fillRoundedRectangle(static_cast<float>(x), centerY - trackThickness * 0.5f, static_cast<float>(width), trackThickness, 2.0f);

        // Filled track
        g.setColour(accentCyan);
        g.fillRoundedRectangle(static_cast<float>(x), centerY - trackThickness * 0.5f, sliderPos - x, trackThickness, 2.0f);

        // Thumb indicator circle
        g.setColour(textPrimary);
        g.fillEllipse(sliderPos - 5.0f, centerY - 5.0f, 10.0f, 10.0f);
    }
}

} // namespace openwav
