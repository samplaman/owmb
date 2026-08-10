#include "OpenWavLookAndFeel.h"

namespace openwav {

juce::Colour OpenWavLookAndFeel::bgDark =
    juce::Colour::fromRGB(240, 240, 240);
juce::Colour OpenWavLookAndFeel::bgHeader =
    juce::Colour::fromRGB(255, 255, 255);
juce::Colour OpenWavLookAndFeel::bgCard =
    juce::Colour::fromRGB(255, 255, 255);
juce::Colour OpenWavLookAndFeel::bgHover =
    juce::Colour::fromRGB(225, 225, 225);
juce::Colour OpenWavLookAndFeel::accentCyan =
    juce::Colour::fromRGB(60, 60, 60);
juce::Colour OpenWavLookAndFeel::accentBlue =
    juce::Colour::fromRGB(110, 110, 110);
juce::Colour OpenWavLookAndFeel::textPrimary =
    juce::Colour::fromRGB(30, 30, 30);
juce::Colour OpenWavLookAndFeel::textSecondary =
    juce::Colour::fromRGB(100, 100, 100);
juce::Colour OpenWavLookAndFeel::borderColour =
    juce::Colour::fromRGB(215, 215, 215);
juce::Colour OpenWavLookAndFeel::favoriteRed =
    juce::Colour::fromRGB(120, 120, 120);

juce::Colour OpenWavLookAndFeel::customPrimaryColour = juce::Colour::fromRGB(0, 200, 220);
bool OpenWavLookAndFeel::hasCustomPrimaryColour = false;

void OpenWavLookAndFeel::setPrimaryColour(juce::Colour colour)
{
    hasCustomPrimaryColour = true;
    customPrimaryColour = colour;
    accentCyan = colour;
    accentBlue = colour.withMultipliedBrightness(0.85f);
}

void OpenWavLookAndFeel::resetPrimaryColour()
{
    hasCustomPrimaryColour = false;
    if (bgDark.getBrightness() < 0.5f)
    {
        accentCyan = juce::Colour::fromRGB(0, 200, 220);
        accentBlue = juce::Colour::fromRGB(0, 140, 255);
    }
    else
    {
        accentCyan = juce::Colour::fromRGB(60, 60, 60);
        accentBlue = juce::Colour::fromRGB(110, 110, 110);
    }
}

void OpenWavLookAndFeel::setDarkTheme(bool useDark)
{
    if (useDark)
    {
        bgDark = juce::Colour::fromRGB(18, 18, 18);
        bgHeader = juce::Colour::fromRGB(30, 30, 30);
        bgCard = juce::Colour::fromRGB(32, 32, 32);
        bgHover = juce::Colour::fromRGB(48, 48, 48);
        accentCyan = hasCustomPrimaryColour ? customPrimaryColour : juce::Colour::fromRGB(0, 200, 220);
        accentBlue = hasCustomPrimaryColour ? customPrimaryColour.withMultipliedBrightness(0.85f) : juce::Colour::fromRGB(0, 140, 255);
        textPrimary = juce::Colour::fromRGB(240, 240, 240);
        textSecondary = juce::Colour::fromRGB(160, 160, 160);
        borderColour = juce::Colour::fromRGB(50, 50, 50);
        favoriteRed = juce::Colour::fromRGB(230, 70, 70);
    }
    else
    {
        bgDark = juce::Colour::fromRGB(240, 240, 240);
        bgHeader = juce::Colour::fromRGB(255, 255, 255);
        bgCard = juce::Colour::fromRGB(255, 255, 255);
        bgHover = juce::Colour::fromRGB(225, 225, 225);
        accentCyan = hasCustomPrimaryColour ? customPrimaryColour : juce::Colour::fromRGB(60, 60, 60);
        accentBlue = hasCustomPrimaryColour ? customPrimaryColour.withMultipliedBrightness(0.85f) : juce::Colour::fromRGB(110, 110, 110);
        textPrimary = juce::Colour::fromRGB(30, 30, 30);
        textSecondary = juce::Colour::fromRGB(100, 100, 100);
        borderColour = juce::Colour::fromRGB(215, 215, 215);
        favoriteRed = juce::Colour::fromRGB(120, 120, 120);
    }
}

OpenWavLookAndFeel::OpenWavLookAndFeel() {
  updateColors();
}

void OpenWavLookAndFeel::updateColors() {
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

  // ListBox & TableHeader (used in Audio/MIDI output channels and MIDI inputs listboxes)
  setColour(juce::ListBox::backgroundColourId, bgCard);
  setColour(juce::ListBox::textColourId, textPrimary);
  setColour(juce::ListBox::outlineColourId, borderColour);
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
  
  // Scrollbars
  setColour(juce::ScrollBar::thumbColourId, accentCyan);

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

  auto text = button.getButtonText().toLowerCase();
  if (text.contains("mute")) {
      if (button.getToggleState()) {
          fillColour = favoriteRed.withAlpha(0.25f);
      } else if (shouldDrawButtonAsHighlighted) {
          fillColour = bgHover;
      } else {
          fillColour = bgCard;
      }
  } else if (button.getToggleState()) {
    fillColour = accentCyan.withAlpha(0.18f);
  } else if (shouldDrawButtonAsDown) {
    fillColour = accentBlue.withAlpha(0.35f);
  } else if (shouldDrawButtonAsHighlighted) {
    fillColour = bgHover;
  }

  g.setColour(fillColour);
  g.fillRoundedRectangle(bounds, cornerRadius);

  juce::Colour border = text.contains("mute") ? (button.getToggleState() ? favoriteRed : borderColour) : (button.getToggleState() ? accentCyan : borderColour);
  float stroke = button.getToggleState() ? 1.4f : 1.0f;
  g.setColour(border);
  g.drawRoundedRectangle(bounds, cornerRadius, stroke);
}

void OpenWavLookAndFeel::drawButtonText(juce::Graphics &g,
                                        juce::TextButton &button,
                                        bool /*shouldDrawButtonAsHighlighted*/,
                                        bool /*shouldDrawButtonAsDown*/) {
  auto text = button.getButtonText();
  auto bounds = button.getLocalBounds().toFloat();
  juce::String svgString;

  if (text == "+ Add Folder" || text == "Add Folder") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M4 20h16a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.93a2 2 0 0 1-1.66-.9l-.82-1.2A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2z\"/><line x1=\"12\" y1=\"10\" x2=\"12\" y2=\"16\"/><line x1=\"9\" y1=\"13\" x2=\"15\" y2=\"13\"/></svg>";
      text = "Add Folder";
  } else if (text == "Rescan") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M21.5 2v6h-6M21.34 15.57a10 10 0 1 1-.57-8.38l5.67-5.67\"/></svg>";
  } else if (text == "Settings") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"12\" cy=\"12\" r=\"3\"/><path d=\"M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z\"/></svg>";
  } else if (text == "List") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"8\" y1=\"6\" x2=\"21\" y2=\"6\"/><line x1=\"8\" y1=\"12\" x2=\"21\" y2=\"12\"/><line x1=\"8\" y1=\"18\" x2=\"21\" y2=\"18\"/><line x1=\"3\" y1=\"6\" x2=\"3.01\" y2=\"6\"/><line x1=\"3\" y1=\"12\" x2=\"3.01\" y2=\"12\"/><line x1=\"3\" y1=\"18\" x2=\"3.01\" y2=\"18\"/></svg>";
  } else if (text == "Cloud") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M18 10h-1.26A8 8 0 1 0 9 20h9a5 5 0 0 0 0-10z\"/></svg>";
  } else if (text == "Library" || text == "Librarys" || text == "Libraries") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M2 3h6a4 4 0 0 1 4 4v14a3 3 0 0 0-3-3H2z\"/><path d=\"M22 3h-6a4 4 0 0 0-4 4v14a3 3 0 0 1 3-3h7z\"/></svg>";
      text = "Library";
  } else if (text == "Record" || text == "RECORD") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3z\"/><path d=\"M19 10v2a7 7 0 0 1-14 0v-2\"/><line x1=\"12\" y1=\"19\" x2=\"12\" y2=\"22\"/></svg>";
  } else if (text == "All") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M4 6h16M4 12h16M4 18h7\"/></svg>";
  } else if (text == ".wav") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M3 10v4M6 6v12M9 10v4M12 2v20M15 10v4M18 6v12M21 10v4\"/></svg>";
  } else if (text == ".mp3") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M9 18V5l12-2v13\"/><circle cx=\"6\" cy=\"18\" r=\"3\"/><circle cx=\"18\" cy=\"16\" r=\"3\"/></svg>";
  } else if (text == ".flac") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5\"/></svg>";
  } else if (text == ".ogg") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"12\" cy=\"12\" r=\"10\"/><circle cx=\"12\" cy=\"12\" r=\"3\"/></svg>";
  } else if (text == ".aiff") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M9 18V5l12-2v13\"/><circle cx=\"6\" cy=\"18\" r=\"3\"/><circle cx=\"18\" cy=\"16\" r=\"3\"/></svg>";
  } else if (text == "Play") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"5 3 19 12 5 21 5 3\"/></svg>";
  } else if (text == "Pause") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"6\" y=\"4\" width=\"4\" height=\"16\"/><rect x=\"14\" y=\"4\" width=\"4\" height=\"16\"/></svg>";
  } else if (text == "Stop") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"4\" y=\"4\" width=\"16\" height=\"16\" rx=\"2\" ry=\"2\"/></svg>";
  } else if (text == "Loop") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M17 2.1l4 4-4 4\"/><path d=\"M3 12a9 9 0 0 1 15-6.7L21 8\"/><path d=\"M7 21.9l-4-4 4-4\"/><path d=\"M21 12a9 9 0 0 1-15 6.7l-3-2.7\"/></svg>";
  } else if (text == "Slice") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"6\" cy=\"6\" r=\"3\"/><circle cx=\"6\" cy=\"18\" r=\"3\"/><line x1=\"9.8\" y1=\"8.2\" x2=\"21\" y2=\"19.4\"/><line x1=\"9.8\" y1=\"15.8\" x2=\"21\" y2=\"4.6\"/></svg>";
  } else if (text.containsIgnoreCase("mute")) {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M11 5L6 9H2v6h4l5 4V5z\"/><line x1=\"23\" y1=\"9\" x2=\"17\" y2=\"15\"/><line x1=\"17\" y1=\"9\" x2=\"23\" y2=\"15\"/></svg>";
  }

  g.setFont(juce::Font(11.5f).boldened());
  juce::Colour textColour = button.getButtonText().toLowerCase().contains("mute") ? (button.getToggleState() ? favoriteRed : textPrimary) : (button.getToggleState() ? accentCyan : textPrimary);
  g.setColour(textColour);

  if (svgString.isNotEmpty()) {
      // Dynamic color injection into the SVG stroke/fill
      juce::String hexColour = textColour.toDisplayString(false);
      svgString = svgString.replace("currentColor", "#" + hexColour);

      auto xml = juce::XmlDocument::parse(svgString);
      if (xml != nullptr) {
          auto drawable = juce::Drawable::createFromSVG(*xml);
          if (drawable != nullptr) {
              // Draw the icon on the left, text on the right
              auto iconArea = bounds.removeFromLeft(28.0f).reduced(6.0f);
              drawable->drawWithin(g, iconArea, juce::RectanglePlacement::centred, 1.0f);
          }
      }
  }

  g.drawText(text, bounds, juce::Justification::centred, true);
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
        auto tickArea = r.removeFromLeft(16).toFloat();
        g.setColour(accentCyan);
        float dotRadius = 3.0f;
        g.fillEllipse(tickArea.getCentreX() - dotRadius, tickArea.getCentreY() - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
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
        auto arrowArea = r.removeFromRight(10).toFloat();
        juce::Path p;
        p.addTriangle(arrowArea.getX(), arrowArea.getCentreY() - 4.0f,
                      arrowArea.getRight(), arrowArea.getCentreY(),
                      arrowArea.getX(), arrowArea.getCentreY() + 4.0f);
        g.fillPath(p);
    }
}

void OpenWavLookAndFeel::drawTickBox(juce::Graphics& g, juce::Component& /*component*/,
                                     float x, float y, float w, float h,
                                     bool ticked, bool isEnabled,
                                     bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/)
{
    auto box = juce::Rectangle<float>(x, y, w, h).reduced(1.0f);
    g.setColour(bgCard);
    g.fillRoundedRectangle(box, 3.0f);
    g.setColour(ticked ? accentCyan : borderColour);
    g.drawRoundedRectangle(box, 3.0f, 1.0f);

    if (ticked)
    {
        g.setColour(isEnabled ? accentCyan : textSecondary);
        float dotRadius = std::min(box.getWidth(), box.getHeight()) * 0.22f;
        g.fillEllipse(box.getCentreX() - dotRadius, box.getCentreY() - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
    }
}

} // namespace openwav
