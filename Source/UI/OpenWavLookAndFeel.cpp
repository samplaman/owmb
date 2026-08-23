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

  bool isMuteBtn = (text == "Mute" || text == "Unmute");
  juce::Colour border = isMuteBtn ? (button.getToggleState() ? favoriteRed : borderColour) : (button.getToggleState() ? accentCyan : borderColour);
  float stroke = button.getToggleState() ? 1.4f : 1.0f;
  g.setColour(border);
  g.drawRoundedRectangle(bounds, cornerRadius, stroke);
}

void OpenWavLookAndFeel::drawButtonText(juce::Graphics &g,
                                        juce::TextButton &button,
                                        bool /*shouldDrawButtonAsHighlighted*/,
                                        bool /*shouldDrawButtonAsDown*/) {
  auto text = button.getButtonText();
  // Small padding inside the button
  auto bounds = button.getLocalBounds().toFloat().reduced(4.0f, 2.0f);
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
  } else if (text == "Sample Map" || text == "SampleMap") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"2\" y=\"4\" width=\"20\" height=\"16\" rx=\"2\"/><line x1=\"6\" y1=\"4\" x2=\"6\" y2=\"13\"/><line x1=\"10\" y1=\"4\" x2=\"10\" y2=\"13\"/><line x1=\"14\" y1=\"4\" x2=\"14\" y2=\"13\"/><line x1=\"18\" y1=\"4\" x2=\"18\" y2=\"13\"/><line x1=\"2\" y1=\"13\" x2=\"22\" y2=\"13\"/></svg>";
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
  } else if (text == "Analysis" || text == "Analyse") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"18\" y1=\"20\" x2=\"18\" y2=\"10\"/><line x1=\"12\" y1=\"20\" x2=\"12\" y2=\"4\"/><line x1=\"6\" y1=\"20\" x2=\"6\" y2=\"14\"/></svg>";
  } else if (text == "Edit") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"6\" cy=\"6\" r=\"3\"/><circle cx=\"6\" cy=\"18\" r=\"3\"/><line x1=\"20\" y1=\"4\" x2=\"8.12\" y2=\"15.88\"/><line x1=\"14.47\" y1=\"14.48\" x2=\"20\" y2=\"20\"/><line x1=\"8.12\" y1=\"8.12\" x2=\"12\" y2=\"12\"/></svg>";
  } else if (text == "Play Sel") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"5 3 15 12 5 21 5 3\"/><line x1=\"19\" y1=\"4\" x2=\"19\" y2=\"20\"/></svg>";
  } else if (text == "Crop") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M6 2v14a2 2 0 0 0 2 2h14\"/><path d=\"M18 22V8a2 2 0 0 0-2-2H2\"/></svg>";
  } else if (text == "Reset") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"1 4 1 10 7 10\"/><path d=\"M3.51 15a9 9 0 1 0 2.13-9.36L1 10\"/></svg>";
  } else if (text == "Snap 0-X" || text == "Snap 0X") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M13 2L3 14h9l-1 8 10-12h-9l1-8z\"/></svg>";
  } else if (text == "Silence") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"1\" y1=\"1\" x2=\"23\" y2=\"23\"/><path d=\"M9 9v3a3 3 0 0 0 5.12 2.12M15 9.34V4a3 3 0 0 0-5.94-.6\"/></svg>";
  } else if (text == "Reverse") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"17 1 21 5 17 9\"/><path d=\"M3 11V9a4 4 0 0 1 4-4h14\"/><polyline points=\"7 23 3 19 7 15\"/><path d=\"M21 13v2a4 4 0 0 1-4 4H3\"/></svg>";
  } else if (text == "Normalize") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"22 12 18 12 15 21 9 3 6 12 2 12\"/></svg>";
  } else if (text == "Deverb") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"12\" y1=\"2\" x2=\"12\" y2=\"22\"/><line x1=\"17\" y1=\"5\" x2=\"17\" y2=\"19\"/><line x1=\"22\" y1=\"8\" x2=\"22\" y2=\"16\"/><line x1=\"7\" y1=\"8\" x2=\"7\" y2=\"16\"/><line x1=\"2\" y1=\"10\" x2=\"2\" y2=\"14\"/></svg>";
  } else if (text == "Bake Fades" || text == "Bake") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"22 6 12 13 2 6\"/><polyline points=\"22 18 12 13 2 18\"/></svg>";
  } else if (text == "Export" || text == "Export Selection") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4\"/><polyline points=\"7 10 12 15 17 10\"/><line x1=\"12\" y1=\"15\" x2=\"12\" y2=\"3\"/></svg>";
  } else if (text == "Select All") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"3\" y=\"3\" width=\"18\" height=\"18\" rx=\"2\"/><path d=\"M9 9h6v6H9z\"/></svg>";
  } else if (text == "Deselect All") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"3\" y=\"3\" width=\"18\" height=\"18\" rx=\"2\"/><line x1=\"9\" y1=\"9\" x2=\"15\" y2=\"15\"/><line x1=\"15\" y1=\"9\" x2=\"9\" y2=\"15\"/></svg>";
  } else if (text == "Auto-Trim") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"4\" y1=\"6\" x2=\"4\" y2=\"18\"/><line x1=\"20\" y1=\"6\" x2=\"20\" y2=\"18\"/><polyline points=\"8 12 12 12 16 12\"/></svg>";
  } else if (text == "Low Cut") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M2 18l6-12 14 0\"/></svg>";
  } else if (text == "Invert Phase") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6\"/></svg>";
  } else if (text == "Spectral" || text == "Spectral Clean") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"2\" y=\"2\" width=\"20\" height=\"20\" rx=\"3\"/><line x1=\"6\" y1=\"18\" x2=\"6\" y2=\"12\"/><line x1=\"10\" y1=\"18\" x2=\"10\" y2=\"6\"/><line x1=\"14\" y1=\"18\" x2=\"14\" y2=\"10\"/><line x1=\"18\" y1=\"18\" x2=\"18\" y2=\"8\"/></svg>";
  } else if (text == "Mute" || text == "Unmute") {
      svgString = "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M11 5L6 9H2v6h4l5 4V5z\"/><line x1=\"23\" y1=\"9\" x2=\"17\" y2=\"15\"/><line x1=\"17\" y1=\"9\" x2=\"23\" y2=\"15\"/></svg>";
  }

  auto font = juce::Font(11.5f).boldened();
  g.setFont(font);
  bool isMuteBtn = (button.getButtonText() == "Mute" || button.getButtonText() == "Unmute");
  juce::Colour textColour = isMuteBtn ? (button.getToggleState() ? favoriteRed : textPrimary) : (button.getToggleState() ? accentCyan : textPrimary);
  g.setColour(textColour);

  if (svgString.isNotEmpty()) {
      // Dynamic color injection into the SVG stroke/fill
      juce::String hexColour = textColour.toDisplayString(false);
      svgString = svgString.replace("currentColor", "#" + hexColour);

      auto xml = juce::XmlDocument::parse(svgString);
      if (xml != nullptr) {
          auto drawable = juce::Drawable::createFromSVG(*xml);
          if (drawable != nullptr) {
              float iconSize = 14.0f;
              float gap = 4.0f;
              float textWidth = font.getStringWidthFloat(text);
              float totalContentWidth = iconSize + gap + textWidth;

              // Calculate start position to center the icon + text block together horizontally
              float startX = bounds.getX() + std::max(0.0f, (bounds.getWidth() - totalContentWidth) * 0.5f);
              float iconY = bounds.getY() + (bounds.getHeight() - iconSize) * 0.5f;

              auto iconArea = juce::Rectangle<float>(startX, iconY, iconSize, iconSize);
              drawable->drawWithin(g, iconArea, juce::RectanglePlacement::centred, 1.0f);

              // Position text area to right of icon
              bounds = juce::Rectangle<float>(startX + iconSize + gap, bounds.getY(), textWidth + 4.0f, bounds.getHeight());
          }
      }
  }

  // Draw text perfectly centered both vertically and horizontally
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
