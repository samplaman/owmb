#include "OpenWavLookAndFeel.h"

namespace openwav {

const juce::Colour OpenWavLookAndFeel::bgDark =
    juce::Colour::fromRGB(240, 240, 240);
const juce::Colour OpenWavLookAndFeel::bgHeader =
    juce::Colour::fromRGB(255, 255, 255);
const juce::Colour OpenWavLookAndFeel::bgCard =
    juce::Colour::fromRGB(255, 255, 255);
const juce::Colour OpenWavLookAndFeel::bgHover =
    juce::Colour::fromRGB(225, 225, 225);
const juce::Colour OpenWavLookAndFeel::accentCyan =
    juce::Colour::fromRGB(60, 60, 60);
const juce::Colour OpenWavLookAndFeel::accentBlue =
    juce::Colour::fromRGB(110, 110, 110);
const juce::Colour OpenWavLookAndFeel::textPrimary =
    juce::Colour::fromRGB(30, 30, 30);
const juce::Colour OpenWavLookAndFeel::textSecondary =
    juce::Colour::fromRGB(100, 100, 100);
const juce::Colour OpenWavLookAndFeel::borderColour =
    juce::Colour::fromRGB(215, 215, 215);
const juce::Colour OpenWavLookAndFeel::favoriteRed =
    juce::Colour::fromRGB(120, 120, 120);

OpenWavLookAndFeel::OpenWavLookAndFeel() {
  setColour(juce::ResizableWindow::backgroundColourId, bgDark);
  setColour(juce::DialogWindow::backgroundColourId, bgDark);
  setColour(juce::AlertWindow::backgroundColourId, bgDark);
  setColour(juce::AlertWindow::textColourId, textPrimary);
  setColour(juce::AlertWindow::outlineColourId, borderColour);

  // GroupComponent & ComboBox (used in Audio/MIDI setup dialogs)
  setColour(juce::GroupComponent::outlineColourId, borderColour);
  setColour(juce::GroupComponent::textColourId, textPrimary);

  setColour(juce::ComboBox::backgroundColourId, bgCard);
  setColour(juce::ComboBox::textColourId, textPrimary);
  setColour(juce::ComboBox::outlineColourId, borderColour);
  setColour(juce::ComboBox::arrowColourId, textPrimary);
  setColour(juce::ComboBox::focusedOutlineColourId, accentCyan);

  setColour(juce::Label::textColourId, textPrimary);

  // Text Editor
  setColour(juce::TextEditor::backgroundColourId, bgCard);
  setColour(juce::TextEditor::textColourId, textPrimary);
  setColour(juce::TextEditor::highlightColourId, accentCyan.withAlpha(0.25f));
  setColour(juce::TextEditor::outlineColourId, borderColour);
  setColour(juce::TextEditor::focusedOutlineColourId, accentCyan);

  // TextButton - Explicit Defaults
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

  // PopupMenu (Right-click menus)
  setColour(juce::PopupMenu::backgroundColourId, bgCard);
  setColour(juce::PopupMenu::textColourId, textPrimary);
  setColour(juce::PopupMenu::headerTextColourId, accentCyan);
  setColour(juce::PopupMenu::highlightedBackgroundColourId, bgHover);
  setColour(juce::PopupMenu::highlightedTextColourId, accentCyan);

  // Directory / FileBrowserComponent
  setColour(juce::FileBrowserComponent::currentPathBoxBackgroundColourId, bgCard);
  setColour(juce::FileBrowserComponent::filenameBoxBackgroundColourId, bgCard);

  // Slider
  setColour(juce::Slider::thumbColourId, textPrimary);
  setColour(juce::Slider::trackColourId, accentCyan);
  setColour(juce::Slider::backgroundColourId, borderColour);
}

void OpenWavLookAndFeel::drawButtonBackground(
    juce::Graphics &g, juce::Button &button,
    const juce::Colour &backgroundColour, bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown) {
  auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
  auto cornerRadius = 5.0f;

  // Light mode fill determination
  juce::Colour fillColour = bgCard;

  if (backgroundColour != juce::Colour(0xff444444) &&
      !backgroundColour.isTransparent() && backgroundColour != bgDark) {
    fillColour = backgroundColour;
  }

  if (button.getToggleState()) {
    fillColour = accentCyan.withAlpha(0.18f);
  } else if (shouldDrawButtonAsDown) {
    fillColour = accentBlue.withAlpha(0.35f);
  } else if (shouldDrawButtonAsHighlighted) {
    fillColour = bgHover;
  }

  g.setColour(fillColour);
  g.fillRoundedRectangle(bounds, cornerRadius);

  juce::Colour border = button.getToggleState() ? accentCyan : borderColour;
  float stroke = button.getToggleState() ? 1.4f : 1.0f;
  g.setColour(border);
  g.drawRoundedRectangle(bounds, cornerRadius, stroke);
}

void OpenWavLookAndFeel::drawButtonText(juce::Graphics &g,
                                        juce::TextButton &button,
                                        bool /*shouldDrawButtonAsHighlighted*/,
                                        bool /*shouldDrawButtonAsDown*/) {
  g.setFont(juce::Font(12.0f).boldened());
  g.setColour(button.getToggleState() ? accentCyan : textPrimary);
  g.drawText(button.getButtonText(), button.getLocalBounds(),
             juce::Justification::centred, true);
}

void OpenWavLookAndFeel::drawTableHeaderBackground(
    juce::Graphics &g, juce::TableHeaderComponent &header) {
  g.fillAll(bgHeader);
  g.setColour(borderColour);
  g.drawRect(header.getLocalBounds().removeFromBottom(1));
}

void OpenWavLookAndFeel::drawTableHeaderColumn(
    juce::Graphics &g, juce::TableHeaderComponent & /*header*/,
    const juce::String &columnName, int /*columnId*/, int width, int height,
    bool isMouseOver, bool /*isMouseDown*/, int /*columnFlags*/) {
  auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(8, 0);

  if (isMouseOver) {
    g.setColour(bgHover);
    g.fillRect(0, 0, width, height);
  }

  g.setFont(juce::Font(11.0f).boldened());
  g.setColour(isMouseOver ? textPrimary : textSecondary);
  g.drawText(columnName.toUpperCase(), bounds, juce::Justification::centredLeft,
             true);

  g.setColour(borderColour);
  g.drawLine(static_cast<float>(width - 1), 0.0f, static_cast<float>(width - 1),
             static_cast<float>(height));
}

void OpenWavLookAndFeel::drawTextEditorOutline(juce::Graphics &g, int width,
                                               int height,
                                               juce::TextEditor &textEditor) {
  auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width),
                                       static_cast<float>(height))
                    .reduced(0.5f);
  g.setColour(textEditor.hasKeyboardFocus(true) ? accentCyan : borderColour);
  g.drawRoundedRectangle(bounds, 5.0f,
                         textEditor.hasKeyboardFocus(true) ? 1.4f : 1.0f);
}

void OpenWavLookAndFeel::drawLinearSlider(
    juce::Graphics &g, int x, int y, int width, int height, float sliderPos,
    float /*minSliderPos*/, float /*maxSliderPos*/,
    const juce::Slider::SliderStyle /*style*/, juce::Slider &slider) {
  auto isHorizontal = slider.isHorizontal();
  float trackThickness = 4.0f;

  if (isHorizontal) {
    float centerY = y + height * 0.5f;

    // Background track
    g.setColour(borderColour);
    g.fillRoundedRectangle(static_cast<float>(x),
                           centerY - trackThickness * 0.5f,
                           static_cast<float>(width), trackThickness, 2.0f);

    // Filled track
    g.setColour(accentCyan);
    g.fillRoundedRectangle(static_cast<float>(x),
                           centerY - trackThickness * 0.5f, sliderPos - x,
                           trackThickness, 2.0f);

    // Thumb indicator circle
    g.setColour(textPrimary);
    g.fillEllipse(sliderPos - 5.0f, centerY - 5.0f, 10.0f, 10.0f);
  }
}

void OpenWavLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(bgCard);
    g.setColour(borderColour);
    g.drawRect(0, 0, width, height, 1);
}

void OpenWavLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                            bool isSeparator, bool isActive, bool isHighlighted,
                                            bool isTicked, bool hasSubMenu, const juce::String& text,
                                            const juce::String& shortcutKeyText,
                                            const juce::Drawable* icon, const juce::Colour* textColourToUse)
{
    if (isSeparator)
    {
        auto r = area.reduced(6, 0);
        r.removeFromTop(r.getHeight() / 2);
        g.setColour(borderColour);
        g.fillRect(r.removeFromTop(1));
        return;
    }

    auto textColour = textPrimary;
    if (textColourToUse != nullptr)
        textColour = *textColourToUse;

    if (!isActive)
        textColour = textSecondary;

    if (isHighlighted && isActive)
    {
        g.setColour(bgHover);
        g.fillRect(area.reduced(2, 1));
        textColour = accentCyan;
    }

    auto r = area.reduced(10, 0);

    if (isTicked)
    {
        auto tickArea = r.removeFromLeft(16);
        g.setColour(accentCyan);
        g.drawText("✓", tickArea, juce::Justification::centredLeft, false);
    }

    if (icon != nullptr)
    {
        auto iconArea = r.removeFromLeft(20).toFloat();
        icon->drawWithin(g, iconArea, juce::RectanglePlacement::centred, 1.0f);
    }

    g.setFont(juce::Font(12.0f).boldened());
    g.setColour(textColour);
    g.drawText(text, r, juce::Justification::centredLeft, true);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour(textSecondary);
        g.drawText(shortcutKeyText, r, juce::Justification::centredRight, true);
    }

    if (hasSubMenu)
    {
        g.setColour(isActive ? textPrimary : textSecondary);
        auto arrowArea = r.removeFromRight(10);
        juce::Path p;
        p.addTriangle(static_cast<float>(arrowArea.getX()), static_cast<float>(arrowArea.getY() + 4),
                      static_cast<float>(arrowArea.getRight()), static_cast<float>(arrowArea.getCentreY()),
                      static_cast<float>(arrowArea.getX()), static_cast<float>(arrowArea.getBottom() - 4));
        g.fillPath(p);
    }
}

} // namespace openwav
