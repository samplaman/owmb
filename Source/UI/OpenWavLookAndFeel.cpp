#include "OpenWavLookAndFeel.h"
#include <algorithm>

namespace openwav {

juce::Colour OpenWavLookAndFeel::bgDark =
    juce::Colour::fromRGB(18, 18, 18);
juce::Colour OpenWavLookAndFeel::bgHeader =
    juce::Colour::fromRGB(30, 30, 30);
juce::Colour OpenWavLookAndFeel::bgCard =
    juce::Colour::fromRGB(32, 32, 32);
juce::Colour OpenWavLookAndFeel::bgHover =
    juce::Colour::fromRGB(48, 48, 48);
juce::Colour OpenWavLookAndFeel::accentCyan =
    juce::Colour::fromRGB(0, 200, 220);
juce::Colour OpenWavLookAndFeel::accentBlue =
    juce::Colour::fromRGB(0, 140, 255);
juce::Colour OpenWavLookAndFeel::textPrimary =
    juce::Colour::fromRGB(240, 240, 240);
juce::Colour OpenWavLookAndFeel::textSecondary =
    juce::Colour::fromRGB(160, 160, 160);
juce::Colour OpenWavLookAndFeel::borderColour =
    juce::Colour::fromRGB(50, 50, 50);
juce::Colour OpenWavLookAndFeel::favoriteRed =
    juce::Colour::fromRGB(230, 70, 70);

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

void OpenWavLookAndFeel::setDarkTheme(bool /*useDark*/)
{
    bgDark = juce::Colour::fromRGB(15, 16, 18);
    bgHeader = juce::Colour::fromRGB(26, 28, 33);
    bgCard = juce::Colour::fromRGB(22, 23, 27);
    bgHover = juce::Colour::fromRGB(37, 40, 48);
    accentCyan = hasCustomPrimaryColour ? customPrimaryColour : juce::Colour::fromRGB(0, 200, 220);
    accentBlue = hasCustomPrimaryColour ? customPrimaryColour.withMultipliedBrightness(0.85f) : juce::Colour::fromRGB(0, 140, 255);
    textPrimary = juce::Colour::fromRGB(240, 242, 245);
    textSecondary = juce::Colour::fromRGB(155, 160, 170);
    borderColour = juce::Colour::fromRGB(44, 47, 56);
    favoriteRed = juce::Colour::fromRGB(235, 75, 85);
}

OpenWavLookAndFeel::OpenWavLookAndFeel() {
#if JUCE_MAC || JUCE_IOS
  setDefaultSansSerifTypefaceName("SF Pro Text");
#elif JUCE_WINDOWS
  setDefaultSansSerifTypefaceName("Segoe UI");
#else
  setDefaultSansSerifTypefaceName("Inter");
#endif
  setDarkTheme(true);
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

  // PopupMenu (Right-click menus & dropdowns)
  if (isDarkTheme())
  {
      setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xf4161b22));
      setColour(juce::PopupMenu::textColourId, textPrimary);
      setColour(juce::PopupMenu::headerTextColourId, accentCyan);
      setColour(juce::PopupMenu::highlightedBackgroundColourId, bgHover);
      setColour(juce::PopupMenu::highlightedTextColourId, accentCyan);
  }
  else
  {
      setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xfafcfcfd));
      setColour(juce::PopupMenu::textColourId, textPrimary);
      setColour(juce::PopupMenu::headerTextColourId, textSecondary.darker(0.25f));
      setColour(juce::PopupMenu::highlightedBackgroundColourId, bgHover);
      setColour(juce::PopupMenu::highlightedTextColourId, textPrimary);
  }

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

  bool isIconOnly = (button.getComponentID() == "iconOnlyBtn" ||
                     button.getComponentID() == "editIconBtn" ||
                     button.getComponentID() == "headerViewBtn");

  bool hasExplicitBg = (backgroundColour != juce::Colour(0xff444444) &&
                        !backgroundColour.isTransparent() && backgroundColour != bgDark);

  // Fill determination
  juce::Colour fillColour = (isIconOnly && !hasExplicitBg) ? juce::Colours::transparentBlack : bgCard;

  if (hasExplicitBg) {
    fillColour = backgroundColour;
  }

  auto text = button.getButtonText().toLowerCase();
  if (text.contains("mute")) {
      if (button.getToggleState()) {
          fillColour = shouldDrawButtonAsHighlighted ? favoriteRed.withAlpha(0.36f) : favoriteRed.withAlpha(0.25f);
      } else if (shouldDrawButtonAsHighlighted) {
          fillColour = bgHover.brighter(0.22f);
      } else {
          fillColour = isIconOnly ? juce::Colours::transparentBlack : bgCard;
      }
  } else if (button.getToggleState()) {
    fillColour = shouldDrawButtonAsHighlighted ? accentCyan.withAlpha(0.28f) : accentCyan.withAlpha(0.18f);
  } else if (shouldDrawButtonAsDown) {
    fillColour = accentBlue.withAlpha(0.38f);
  } else if (shouldDrawButtonAsHighlighted) {
    fillColour = bgHover.brighter(0.22f);
  }

  if (button.getComponentID() == "tagButton") {
      cornerRadius = 4.0f;
      if (!button.getToggleState() && !shouldDrawButtonAsHighlighted && !shouldDrawButtonAsDown) {
          fillColour = juce::Colours::transparentBlack;
      }
  } else if (isIconOnly) {
      cornerRadius = 6.0f;
  }

  int edges = button.getConnectedEdgeFlags();
  bool curveTL = !(edges & (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnTop));
  bool curveTR = !(edges & (juce::Button::ConnectedOnRight | juce::Button::ConnectedOnTop));
  bool curveBL = !(edges & (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnBottom));
  bool curveBR = !(edges & (juce::Button::ConnectedOnRight | juce::Button::ConnectedOnBottom));

  juce::Path bgPath;
  bgPath.addRoundedRectangle(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                             cornerRadius, cornerRadius, curveTL, curveTR, curveBL, curveBR);

  g.setColour(fillColour);
  g.fillPath(bgPath);

  // Grouped buttons rely on the outer drawButtonGroup pill border for a streamlined look
  if (edges != 0)
      return;

  bool isMuteBtn = (text == "mute" || text == "unmute");
  juce::Colour border = isMuteBtn ? (button.getToggleState() ? favoriteRed : borderColour)
                                  : (button.getToggleState() ? accentCyan.withAlpha(0.8f) : borderColour);
  if (button.getComponentID() == "tagButton" && !button.getToggleState()) {
      border = shouldDrawButtonAsHighlighted ? borderColour.withAlpha(0.6f) : juce::Colours::transparentBlack;
  } else if (isIconOnly && !button.getToggleState() && !hasExplicitBg) {
      border = shouldDrawButtonAsHighlighted ? borderColour.withAlpha(0.8f) : juce::Colours::transparentBlack;
  }
  float stroke = button.getToggleState() ? 1.25f : 1.0f;
  g.setColour(border);
  g.strokePath(bgPath, juce::PathStrokeType(stroke));
}

void OpenWavLookAndFeel::drawButtonText(juce::Graphics &g,
                                        juce::TextButton &button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool /*shouldDrawButtonAsDown*/) {
  auto text = button.getButtonText();
  if (text.isEmpty() && button.getName().isNotEmpty()) {
      text = button.getName();
  }
  // Small padding inside the button
  auto bounds = button.getLocalBounds().toFloat().reduced(4.0f, 2.0f);

  if (button.getComponentID() == "tagButton") {
      auto font = getBodyFont(11.5f, true);
      g.setFont(font);
      juce::Colour textColour = button.getToggleState() ? accentCyan : textPrimary;
      g.setColour(textColour);

      auto textBounds = bounds.reduced(6.0f, 0.0f);
      int parenIdx = text.lastIndexOf("(");
      if (parenIdx > 0) {
          auto tagPart = text.substring(0, parenIdx).trim();
          auto countPart = text.substring(parenIdx).trim();
          g.drawText(tagPart, textBounds, juce::Justification::centredLeft, true);
          g.setColour(textColour.withAlpha(0.55f));
          g.drawText(countPart, textBounds, juce::Justification::centredRight, true);
      } else {
          g.drawText(text, textBounds, juce::Justification::centredLeft, true);
      }
      return;
  }

  juce::String svgString;

  struct SvgIconEntry
  {
      const char* match;
      const char* svg;
      bool isPrefix = false;
      const char* overrideText = nullptr;
  };

  static const SvgIconEntry iconTable[] = {
      { "+ Add Folder", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M4 20h16a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.93a2 2 0 0 1-1.66-.9l-.82-1.2A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2z\"/><line x1=\"12\" y1=\"10\" x2=\"12\" y2=\"16\"/><line x1=\"9\" y1=\"13\" x2=\"15\" y2=\"13\"/></svg>", false, "Add Folder" },
      { "Add Folder",   "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M4 20h16a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.93a2 2 0 0 1-1.66-.9l-.82-1.2A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2z\"/><line x1=\"12\" y1=\"10\" x2=\"12\" y2=\"16\"/><line x1=\"9\" y1=\"13\" x2=\"15\" y2=\"13\"/></svg>", false, "Add Folder" },
      { "Rescan",       "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M21.5 2v6h-6M21.34 15.57a10 10 0 1 1-.57-8.38l5.67-5.67\"/></svg>" },
      { "Settings",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"12\" cy=\"12\" r=\"3\"/><path d=\"M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z\"/></svg>" },
      { "List",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"8\" y1=\"6\" x2=\"21\" y2=\"6\"/><line x1=\"8\" y1=\"12\" x2=\"21\" y2=\"12\"/><line x1=\"8\" y1=\"18\" x2=\"21\" y2=\"18\"/><line x1=\"3\" y1=\"6\" x2=\"3.01\" y2=\"6\"/><line x1=\"3\" y1=\"12\" x2=\"3.01\" y2=\"12\"/><line x1=\"3\" y1=\"18\" x2=\"3.01\" y2=\"18\"/></svg>" },
      { "Folders",      "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z\"/></svg>" },
      { "Cloud",        "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M18 10h-1.26A8 8 0 1 0 9 20h9a5 5 0 0 0 0-10z\"/></svg>" },
      { "Library",      "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M2 3h6a4 4 0 0 1 4 4v14a3 3 0 0 0-3-3H2z\"/><path d=\"M22 3h-6a4 4 0 0 0-4 4v14a3 3 0 0 1 3-3h7z\"/></svg>", false, "Library" },
      { "Librarys",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M2 3h6a4 4 0 0 1 4 4v14a3 3 0 0 0-3-3H2z\"/><path d=\"M22 3h-6a4 4 0 0 0-4 4v14a3 3 0 0 1 3-3h7z\"/></svg>", false, "Library" },
      { "Libraries",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M2 3h6a4 4 0 0 1 4 4v14a3 3 0 0 0-3-3H2z\"/><path d=\"M22 3h-6a4 4 0 0 0-4 4v14a3 3 0 0 1 3-3h7z\"/></svg>", false, "Library" },
      { "Record",       "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3z\"/><path d=\"M19 10v2a7 7 0 0 1-14 0v-2\"/><line x1=\"12\" y1=\"19\" x2=\"12\" y2=\"22\"/></svg>" },
      { "RECORD",       "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3z\"/><path d=\"M19 10v2a7 7 0 0 1-14 0v-2\"/><line x1=\"12\" y1=\"19\" x2=\"12\" y2=\"22\"/></svg>" },
      { "Sample Map",   "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"2\" y=\"4\" width=\"20\" height=\"16\" rx=\"2\"/><line x1=\"6\" y1=\"4\" x2=\"6\" y2=\"13\"/><line x1=\"10\" y1=\"4\" x2=\"10\" y2=\"13\"/><line x1=\"14\" y1=\"4\" x2=\"14\" y2=\"13\"/><line x1=\"18\" y1=\"4\" x2=\"18\" y2=\"13\"/><line x1=\"2\" y1=\"13\" x2=\"22\" y2=\"13\"/></svg>" },
      { "SampleMap",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"2\" y=\"4\" width=\"20\" height=\"16\" rx=\"2\"/><line x1=\"6\" y1=\"4\" x2=\"6\" y2=\"13\"/><line x1=\"10\" y1=\"4\" x2=\"10\" y2=\"13\"/><line x1=\"14\" y1=\"4\" x2=\"14\" y2=\"13\"/><line x1=\"18\" y1=\"4\" x2=\"18\" y2=\"13\"/><line x1=\"2\" y1=\"13\" x2=\"22\" y2=\"13\"/></svg>" },
      { "Rack",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"2\" y=\"3\" width=\"20\" height=\"8\" rx=\"1.5\"/><rect x=\"2\" y=\"13\" width=\"20\" height=\"8\" rx=\"1.5\"/><line x1=\"5\" y1=\"7\" x2=\"5.01\" y2=\"7\"/><line x1=\"19\" y1=\"7\" x2=\"19.01\" y2=\"7\"/><line x1=\"9\" y1=\"7\" x2=\"15\" y2=\"7\"/><line x1=\"5\" y1=\"17\" x2=\"5.01\" y2=\"17\"/><line x1=\"19\" y1=\"17\" x2=\"19.01\" y2=\"17\"/><line x1=\"9\" y1=\"17\" x2=\"15\" y2=\"17\"/></svg>" },
      { "Performance",  "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"2\" y=\"3\" width=\"20\" height=\"8\" rx=\"1.5\"/><rect x=\"2\" y=\"13\" width=\"20\" height=\"8\" rx=\"1.5\"/><line x1=\"5\" y1=\"7\" x2=\"5.01\" y2=\"7\"/><line x1=\"19\" y1=\"7\" x2=\"19.01\" y2=\"7\"/><line x1=\"9\" y1=\"7\" x2=\"15\" y2=\"7\"/><line x1=\"5\" y1=\"17\" x2=\"5.01\" y2=\"17\"/><line x1=\"19\" y1=\"17\" x2=\"19.01\" y2=\"17\"/><line x1=\"9\" y1=\"17\" x2=\"15\" y2=\"17\"/></svg>" },
      { "FX Rack",      "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"2\" y=\"3\" width=\"20\" height=\"8\" rx=\"1.5\"/><rect x=\"2\" y=\"13\" width=\"20\" height=\"8\" rx=\"1.5\"/><line x1=\"5\" y1=\"7\" x2=\"5.01\" y2=\"7\"/><line x1=\"19\" y1=\"7\" x2=\"19.01\" y2=\"7\"/><line x1=\"9\" y1=\"7\" x2=\"15\" y2=\"7\"/><line x1=\"5\" y1=\"17\" x2=\"5.01\" y2=\"17\"/><line x1=\"19\" y1=\"17\" x2=\"19.01\" y2=\"17\"/><line x1=\"9\" y1=\"17\" x2=\"15\" y2=\"17\"/></svg>" },
      { "All",          "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M4 6h16M4 12h16M4 18h7\"/></svg>" },
      { ".wav",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M3 10v4M6 6v12M9 10v4M12 2v20M15 10v4M18 6v12M21 10v4\"/></svg>" },
      { ".mp3",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M9 18V5l12-2v13\"/><circle cx=\"6\" cy=\"18\" r=\"3\"/><circle cx=\"18\" cy=\"16\" r=\"3\"/></svg>" },
      { ".flac",        "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5\"/></svg>" },
      { ".ogg",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"12\" cy=\"12\" r=\"10\"/><circle cx=\"12\" cy=\"12\" r=\"3\"/></svg>" },
      { ".aiff",        "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M9 18V5l12-2v13\"/><circle cx=\"6\" cy=\"18\" r=\"3\"/><circle cx=\"18\" cy=\"16\" r=\"3\"/></svg>" },
      { "Play",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"5 3 19 12 5 21 5 3\"/></svg>" },
      { "Pause",        "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"6\" y=\"4\" width=\"4\" height=\"16\"/><rect x=\"14\" y=\"4\" width=\"4\" height=\"16\"/></svg>" },
      { "Stop",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"4\" y=\"4\" width=\"16\" height=\"16\" rx=\"2\" ry=\"2\"/></svg>" },
      { "Loop",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M17 2.1l4 4-4 4\"/><path d=\"M3 12a9 9 0 0 1 15-6.7L21 8\"/><path d=\"M7 21.9l-4-4 4-4\"/><path d=\"M21 12a9 9 0 0 1-15 6.7l-3-2.7\"/></svg>" },
      { "Slice",        "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"6\" cy=\"6\" r=\"3\"/><circle cx=\"6\" cy=\"18\" r=\"3\"/><line x1=\"9.8\" y1=\"8.2\" x2=\"21\" y2=\"19.4\"/><line x1=\"9.8\" y1=\"15.8\" x2=\"21\" y2=\"4.6\"/></svg>" },
      { "Analysis",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"18\" y1=\"20\" x2=\"18\" y2=\"10\"/><line x1=\"12\" y1=\"20\" x2=\"12\" y2=\"4\"/><line x1=\"6\" y1=\"20\" x2=\"6\" y2=\"14\"/></svg>" },
      { "Analyse",      "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"18\" y1=\"20\" x2=\"18\" y2=\"10\"/><line x1=\"12\" y1=\"20\" x2=\"12\" y2=\"4\"/><line x1=\"6\" y1=\"20\" x2=\"6\" y2=\"14\"/></svg>" },
      { "Edit",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"6\" cy=\"6\" r=\"3\"/><circle cx=\"6\" cy=\"18\" r=\"3\"/><line x1=\"20\" y1=\"4\" x2=\"8.12\" y2=\"15.88\"/><line x1=\"14.47\" y1=\"14.48\" x2=\"20\" y2=\"20\"/><line x1=\"8.12\" y1=\"8.12\" x2=\"12\" y2=\"12\"/></svg>" },
      { "Play Sel",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"5 3 15 12 5 21 5 3\"/><line x1=\"19\" y1=\"4\" x2=\"19\" y2=\"20\"/></svg>" },
      { "Crop",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M6 2v14a2 2 0 0 0 2 2h14\"/><path d=\"M18 22V8a2 2 0 0 0-2-2H2\"/></svg>" },
      { "Reset",        "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"1 4 1 10 7 10\"/><path d=\"M3.51 15a9 9 0 1 0 2.13-9.36L1 10\"/></svg>" },
      { "Snap 0-X",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M13 2L3 14h9l-1 8 10-12h-9l1-8z\"/></svg>" },
      { "Snap 0X",      "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M13 2L3 14h9l-1 8 10-12h-9l1-8z\"/></svg>" },
      { "Silence",      "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"1\" y1=\"1\" x2=\"23\" y2=\"23\"/><path d=\"M9 9v3a3 3 0 0 0 5.12 2.12M15 9.34V4a3 3 0 0 0-5.94-.6\"/></svg>" },
      { "Reverse",      "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"17 1 21 5 17 9\"/><path d=\"M3 11V9a4 4 0 0 1 4-4h14\"/><polyline points=\"7 23 3 19 7 15\"/><path d=\"M21 13v2a4 4 0 0 1-4 4H3\"/></svg>" },
      { "Normalize",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"22 12 18 12 15 21 9 3 6 12 2 12\"/></svg>" },
      { "+3dB",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"18 13 12 7 6 13\"/><line x1=\"12\" y1=\"7\" x2=\"12\" y2=\"21\"/><path d=\"M19 3v4M17 5h4\"/></svg>" },
      { "+3 dB",        "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"18 13 12 7 6 13\"/><line x1=\"12\" y1=\"7\" x2=\"12\" y2=\"21\"/><path d=\"M19 3v4M17 5h4\"/></svg>" },
      { "-3dB",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"6 11 12 17 18 11\"/><line x1=\"12\" y1=\"17\" x2=\"12\" y2=\"3\"/><line x1=\"17\" y1=\"5\" x2=\"21\" y2=\"5\"/></svg>" },
      { "-3 dB",        "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"6 11 12 17 18 11\"/><line x1=\"12\" y1=\"17\" x2=\"12\" y2=\"3\"/><line x1=\"17\" y1=\"5\" x2=\"21\" y2=\"5\"/></svg>" },
      { "2x Speed",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"13 19 22 12 13 5 13 19\"/><polygon points=\"2 19 11 12 2 5 2 19\"/></svg>" },
      { "2x",           "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"13 19 22 12 13 5 13 19\"/><polygon points=\"2 19 11 12 2 5 2 19\"/></svg>" },
      { "0.5x Speed",   "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"11 19 2 12 11 5 11 19\"/><polygon points=\"22 19 13 12 22 5 22 19\"/></svg>" },
      { "0.5x",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"11 19 2 12 11 5 11 19\"/><polygon points=\"22 19 13 12 22 5 22 19\"/></svg>" },
      { "Deverb",       "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"12\" y1=\"2\" x2=\"12\" y2=\"22\"/><line x1=\"17\" y1=\"5\" x2=\"17\" y2=\"19\"/><line x1=\"22\" y1=\"8\" x2=\"22\" y2=\"16\"/><line x1=\"7\" y1=\"8\" x2=\"7\" y2=\"16\"/><line x1=\"2\" y1=\"10\" x2=\"2\" y2=\"14\"/></svg>" },
      { "Bake Fades",   "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"22 6 12 13 2 6\"/><polyline points=\"22 18 12 13 2 18\"/></svg>" },
      { "Bake",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"22 6 12 13 2 6\"/><polyline points=\"22 18 12 13 2 18\"/></svg>" },
      { "Export",       "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4\"/><polyline points=\"7 10 12 15 17 10\"/><line x1=\"12\" y1=\"15\" x2=\"12\" y2=\"3\"/></svg>" },
      { "Export Selection", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4\"/><polyline points=\"7 10 12 15 17 10\"/><line x1=\"12\" y1=\"15\" x2=\"12\" y2=\"3\"/></svg>" },
      { "Revert",       "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8\"/><path d=\"M3 3v5h5\"/></svg>" },
      { "Revert Original", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8\"/><path d=\"M3 3v5h5\"/></svg>" },
      { "Select All",   "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"3\" y=\"3\" width=\"18\" height=\"18\" rx=\"2\"/><path d=\"M9 9h6v6H9z\"/></svg>" },
      { "Deselect All", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"3\" y=\"3\" width=\"18\" height=\"18\" rx=\"2\"/><line x1=\"9\" y1=\"9\" x2=\"15\" y2=\"15\"/><line x1=\"15\" y1=\"9\" x2=\"9\" y2=\"15\"/></svg>" },
      { "Auto-Trim",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"4\" y1=\"6\" x2=\"4\" y2=\"18\"/><line x1=\"20\" y1=\"6\" x2=\"20\" y2=\"18\"/><polyline points=\"8 12 12 12 16 12\"/></svg>" },
      { "Low Cut",      "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M2 18l6-12 14 0\"/></svg>" },
      { "Invert Phase", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6\"/></svg>" },
      { "Loop Sel",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"17 2 21 6 17 10\"/><path d=\"M3 11V9a3 3 0 0 1 3-3h15\"/><polyline points=\"7 22 3 18 7 14\"/><path d=\"M21 13v2a3 3 0 0 1-3 3H3\"/></svg>" },
      { "0-X Loop",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"2\" y1=\"12\" x2=\"22\" y2=\"12\"/><path d=\"M4 12c2-7 5-7 8 0s6 7 8 0\"/><circle cx=\"12\" cy=\"12\" r=\"2.5\"/></svg>" },
      { "DC Fix",       "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"3\" y1=\"12\" x2=\"21\" y2=\"12\"/><polyline points=\"12 4 12 20\"/><polyline points=\"8 8 12 4 16 8\"/></svg>" },
      { "Hi Cut",       "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M2 6h14l6 12\"/></svg>" },
      { "Mono Sum",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"9\" cy=\"12\" r=\"5\"/><circle cx=\"15\" cy=\"12\" r=\"5\"/></svg>" },
      { "Heal / Inpaint", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M14.7 6.3a1 1 0 0 0 0 1.4l1.6 1.6a1 1 0 0 0 1.4 0l3.77-3.77a6 6 0 0 1-7.94 7.94l-6.91 6.91a2.12 2.12 0 0 1-3-3l6.91-6.91a6 6 0 0 1 7.94-7.94l-3.76 3.76z\"/></svg>" },
      { "Heal",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M14.7 6.3a1 1 0 0 0 0 1.4l1.6 1.6a1 1 0 0 0 1.4 0l3.77-3.77a6 6 0 0 1-7.94 7.94l-6.91 6.91a2.12 2.12 0 0 1-3-3l6.91-6.91a6 6 0 0 1 7.94-7.94l-3.76 3.76z\"/></svg>" },
      { "De-Harmonic",  "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"4\" y1=\"4\" x2=\"4\" y2=\"20\"/><line x1=\"9\" y1=\"7\" x2=\"9\" y2=\"17\"/><line x1=\"14\" y1=\"10\" x2=\"14\" y2=\"14\"/><line x1=\"19\" y1=\"6\" x2=\"19\" y2=\"18\"/><line x1=\"2\" y1=\"2\" x2=\"22\" y2=\"22\"/></svg>" },
      { "Spectral Denoise", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M2 14c4-6 6-6 10 0s6 6 10 0\"/><line x1=\"3\" y1=\"3\" x2=\"21\" y2=\"21\"/></svg>" },
      { "Stereo Spread", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"7 8 3 12 7 16\"/><polyline points=\"17 8 21 12 17 16\"/><line x1=\"3\" y1=\"12\" x2=\"21\" y2=\"12\"/></svg>" },
      { "Warmth",       "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M8.5 14.5A2.5 2.5 0 0 0 11 12c0-1.38-.5-2-1-3-1.072-2.143-.224-4.054 2-6 .5 2.5 2 4.9 4 6.5 2 1.6 3 3.5 3 5.5a7 7 0 1 1-14 0c0-1.153.433-2.294 1-3a2.5 2.5 0 0 0 2.5 2.5z\"/></svg>" },
      { "Remove",       "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"3 6 5 6 21 6\"/><path d=\"M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2\"/></svg>" },
      { "Remove Spectral", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"3 6 5 6 21 6\"/><path d=\"M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2\"/></svg>" },
      { "+6dB",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"17 11 12 6 7 11\"/><polyline points=\"17 18 12 13 7 18\"/></svg>" },
      { "-6dB",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"7 13 12 18 17 13\"/><polyline points=\"7 6 12 11 17 6\"/></svg>" },
      { "Isolate",      "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"12\" cy=\"12\" r=\"9\"/><line x1=\"22\" y1=\"12\" x2=\"17\" y2=\"12\"/><line x1=\"7\" y1=\"12\" x2=\"2\" y2=\"12\"/><line x1=\"12\" y1=\"2\" x2=\"12\" y2=\"7\"/><line x1=\"12\" y1=\"17\" x2=\"12\" y2=\"22\"/></svg>" },
      { "Spectral",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"2\" y=\"2\" width=\"20\" height=\"20\" rx=\"3\"/><line x1=\"6\" y1=\"18\" x2=\"6\" y2=\"12\"/><line x1=\"10\" y1=\"18\" x2=\"10\" y2=\"6\"/><line x1=\"14\" y1=\"18\" x2=\"14\" y2=\"10\"/><line x1=\"18\" y1=\"18\" x2=\"18\" y2=\"8\"/></svg>" },
      { "Spectral Clean", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"2\" y=\"2\" width=\"20\" height=\"20\" rx=\"3\"/><line x1=\"6\" y1=\"18\" x2=\"6\" y2=\"12\"/><line x1=\"10\" y1=\"18\" x2=\"10\" y2=\"6\"/><line x1=\"14\" y1=\"18\" x2=\"14\" y2=\"10\"/><line x1=\"18\" y1=\"18\" x2=\"18\" y2=\"8\"/></svg>" },
      { "Mute",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M11 5L6 9H2v6h4l5 4V5z\"/><line x1=\"23\" y1=\"9\" x2=\"17\" y2=\"15\"/><line x1=\"17\" y1=\"9\" x2=\"23\" y2=\"15\"/></svg>" },
      { "Unmute",       "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M11 5L6 9H2v6h4l5 4V5z\"/><line x1=\"23\" y1=\"9\" x2=\"17\" y2=\"15\"/><line x1=\"17\" y1=\"9\" x2=\"23\" y2=\"15\"/></svg>" },
      { "Add Sample",   "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z\"/><polyline points=\"14 2 14 8 20 8\"/><line x1=\"12\" y1=\"18\" x2=\"12\" y2=\"12\"/><line x1=\"9\" y1=\"15\" x2=\"15\" y2=\"15\"/></svg>" },
      { "Loris Resynth", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M2 12c2-6 4-6 6 0s4 6 6 0 4-6 6 0\"/><circle cx=\"12\" cy=\"12\" r=\"1.5\" fill=\"currentColor\"/></svg>" },
      { "Delete Selected", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"3 6 5 6 21 6\"/><path d=\"M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2\"/><line x1=\"10\" y1=\"11\" x2=\"10\" y2=\"17\"/><line x1=\"14\" y1=\"11\" x2=\"14\" y2=\"17\"/></svg>" },
      { "Auto Pitch",   "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M9 18V5l12-2v13\"/><circle cx=\"6\" cy=\"18\" r=\"3\"/><circle cx=\"18\" cy=\"16\" r=\"3\"/></svg>" },
      { "Auto Chromatic", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"2\" y=\"5\" width=\"20\" height=\"14\" rx=\"2\"/><line x1=\"7\" y1=\"5\" x2=\"7\" y2=\"14\"/><line x1=\"12\" y1=\"5\" x2=\"12\" y2=\"14\"/><line x1=\"17\" y1=\"5\" x2=\"17\" y2=\"14\"/></svg>" },
      { "Auto Velocity", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"4\" y1=\"20\" x2=\"4\" y2=\"14\"/><line x1=\"9\" y1=\"20\" x2=\"9\" y2=\"10\"/><line x1=\"14\" y1=\"20\" x2=\"14\" y2=\"6\"/><line x1=\"19\" y1=\"20\" x2=\"19\" y2=\"3\"/></svg>" },
      { "Auto RR",      "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"12 2 2 7 12 12 22 7 12 2\"/><polyline points=\"2 12 12 17 22 12\"/><polyline points=\"2 17 12 22 22 17\"/></svg>" },
      { "Clear Map",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"3\" y=\"3\" width=\"18\" height=\"18\" rx=\"2\"/><line x1=\"9\" y1=\"9\" x2=\"15\" y2=\"15\"/><line x1=\"15\" y1=\"9\" x2=\"9\" y2=\"15\"/></svg>" },
      { "Save Map",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z\"/><polyline points=\"17 21 17 13 7 13 7 21\"/><polyline points=\"7 3 7 8 15 8\"/></svg>" },
      { "Load Map",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z\"/><polyline points=\"12 11 12 17\"/><polyline points=\"9 14 12 11 15 14\"/></svg>" },
      { "Load URL",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"12\" cy=\"12\" r=\"10\"/><line x1=\"2\" y1=\"12\" x2=\"22\" y2=\"12\"/><path d=\"M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z\"/></svg>" },
      { "Export ZIP",   "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z\"/><polyline points=\"3.27 6.96 12 12.01 20.73 6.96\"/><line x1=\"12\" y1=\"22.08\" x2=\"12\" y2=\"12\"/></svg>" },
      { "RR: Cycle",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"17 1 21 5 17 9\"/><path d=\"M3 11V9a4 4 0 0 1 4-4h14\"/><polyline points=\"7 23 3 19 7 15\"/><path d=\"M21 13v2a4 4 0 0 1-4 4H3\"/></svg>" },
      { "RR: Random",   "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"16 3 21 3 21 8\"/><line x1=\"4\" y1=\"20\" x2=\"21\" y2=\"3\"/><polyline points=\"21 16 21 21 16 21\"/><line x1=\"15\" y1=\"15\" x2=\"21\" y2=\"21\"/><line x1=\"4\" y1=\"4\" x2=\"9\" y2=\"9\"/></svg>" },
      { "RR: OFF",      "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"12\" cy=\"12\" r=\"9\"/><line x1=\"5\" y1=\"5\" x2=\"19\" y2=\"19\"/></svg>" },
      { "Pitch Track",  "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M12 3v12\"/><circle cx=\"9\" cy=\"16\" r=\"3\"/><path d=\"M12 3l6 3v4l-6-3\"/></svg>", true },
      { "One Shot",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"13 2 3 14 12 14 11 22 21 10 12 10 13 2\"/></svg>", true },
      { "Loop:",        "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M17 2.1l4 4-4 4\"/><path d=\"M3 12a9 9 0 0 1 15-6.7L21 8\"/><path d=\"M7 21.9l-4-4 4-4\"/><path d=\"M21 12a9 9 0 0 1-15 6.7l-3-2.7\"/></svg>", true },
      { "Rand Loop",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"16 3 21 3 21 8\"/><line x1=\"4\" y1=\"20\" x2=\"21\" y2=\"3\"/><polyline points=\"21 16 21 21 16 21\"/><line x1=\"15\" y1=\"15\" x2=\"21\" y2=\"21\"/><circle cx=\"6.5\" cy=\"6.5\" r=\"2.5\"/></svg>", true },
      { "Instrument",   "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"8\" cy=\"8\" r=\"4\"/><line x1=\"8\" y1=\"8\" x2=\"10.5\" y2=\"5.5\"/><circle cx=\"16\" cy=\"16\" r=\"4\"/><line x1=\"16\" y1=\"16\" x2=\"18.5\" y2=\"13.5\"/><line x1=\"15\" y1=\"6\" x2=\"20\" y2=\"6\"/><line x1=\"4\" y1=\"18\" x2=\"9\" y2=\"18\"/></svg>" },
      { "Vel:",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M3 20C8 20 12 12 21 4\"/><line x1=\"3\" y1=\"20\" x2=\"21\" y2=\"20\"/><line x1=\"3\" y1=\"4\" x2=\"3\" y2=\"20\"/></svg>", true },
      { "MIDI",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"12\" cy=\"12\" r=\"9\"/><circle cx=\"8\" cy=\"12\" r=\"1\" fill=\"currentColor\"/><circle cx=\"16\" cy=\"12\" r=\"1\" fill=\"currentColor\"/><circle cx=\"12\" cy=\"8\" r=\"1\" fill=\"currentColor\"/><circle cx=\"9.5\" cy=\"9.5\" r=\"1\" fill=\"currentColor\"/><circle cx=\"14.5\" cy=\"9.5\" r=\"1\" fill=\"currentColor\"/></svg>", true },
      { "Auto Sample",  "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"12\" cy=\"12\" r=\"9\"/><circle cx=\"12\" cy=\"12\" r=\"3\" fill=\"currentColor\"/><line x1=\"12\" y1=\"2\" x2=\"12\" y2=\"5\"/><line x1=\"12\" y1=\"19\" x2=\"12\" y2=\"22\"/><line x1=\"2\" y1=\"12\" x2=\"5\" y2=\"12\"/><line x1=\"19\" y1=\"12\" x2=\"22\" y2=\"12\"/></svg>" },
      { "AI Kit",       "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M12 2l2.4 7.2L22 12l-7.6 2.8L12 22l-2.4-7.2L2 12l7.6-2.8z\"/></svg>" },
      { "Auto-Tag Library", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M12 2l2.4 7.2L22 12l-7.6 2.8L12 22l-2.4-7.2L2 12l7.6-2.8z\"/></svg>" },
      { "REC",          "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"12\" cy=\"12\" r=\"8\"/><circle cx=\"12\" cy=\"12\" r=\"3.5\" fill=\"currentColor\"/></svg>" },
      { "PLAY",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"5 3 19 12 5 21 5 3\"/></svg>" },
      { "STOP",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"4\" y=\"4\" width=\"16\" height=\"16\" rx=\"2\" ry=\"2\"/></svg>" },
      { "PAUSE",        "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"6\" y=\"4\" width=\"4\" height=\"16\"/><rect x=\"14\" y=\"4\" width=\"4\" height=\"16\"/></svg>" },
      { "RESUME",       "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"5 3 14 12 5 21 5 3\"/><line x1=\"18\" y1=\"4\" x2=\"18\" y2=\"20\"/></svg>" },
      { "Undo",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"9 14 4 9 9 4\"/><path d=\"M20 20v-7a4 4 0 0 0-4-4H4\"/></svg>" },
      { "Redo",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"15 14 20 9 15 4\"/><path d=\"M4 20v-7a4 4 0 0 1 4-4h12\"/></svg>" },
      { "More DSP",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"12\" cy=\"12\" r=\"1\"/><circle cx=\"19\" cy=\"12\" r=\"1\"/><circle cx=\"5\" cy=\"12\" r=\"1\"/></svg>" },
      { "Loop Seam",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M17 2.1l4 4-4 4\"/><path d=\"M3 12a9 9 0 0 1 15-6.7L21 8\"/><circle cx=\"12\" cy=\"12\" r=\"2.5\"/></svg>" },
      { "Sampler Rack", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"2\" y=\"3\" width=\"20\" height=\"5\" rx=\"1\"/><rect x=\"2\" y=\"10\" width=\"20\" height=\"5\" rx=\"1\"/><rect x=\"2\" y=\"17\" width=\"20\" height=\"4\" rx=\"1\"/><line x1=\"6\" y1=\"5.5\" x2=\"6.01\" y2=\"5.5\"/><line x1=\"6\" y1=\"12.5\" x2=\"6.01\" y2=\"12.5\"/><line x1=\"10\" y1=\"5.5\" x2=\"18\" y2=\"5.5\"/><line x1=\"10\" y1=\"12.5\" x2=\"18\" y2=\"12.5\"/></svg>" },
      { "Collapse Unit", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2.2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"6 9 12 15 18 9\"/></svg>" },
      { "Expand Unit",  "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2.2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"9 18 15 12 9 6\"/></svg>" },
      { "Out:",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M18 8h1a4 4 0 0 1 0 8h-1\"/><path d=\"M2 8h16v8H2z\"/><line x1=\"6\" y1=\"12\" x2=\"6.01\" y2=\"12\"/><line x1=\"10\" y1=\"12\" x2=\"14\" y2=\"12\"/></svg>", true },
      { "Fold",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"4 14 10 14 10 20\"/><polyline points=\"20 10 14 10 14 4\"/><line x1=\"14\" y1=\"10\" x2=\"21\" y2=\"3\"/><line x1=\"3\" y1=\"21\" x2=\"10\" y2=\"14\"/></svg>", true },
      { "Cue Sheet",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"3\" y=\"3\" width=\"18\" height=\"18\" rx=\"2\"/><line x1=\"3\" y1=\"9\" x2=\"21\" y2=\"9\"/><line x1=\"3\" y1=\"15\" x2=\"21\" y2=\"15\"/><line x1=\"9\" y1=\"9\" x2=\"9\" y2=\"21\"/></svg>" },
      { "Mixer",        "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"4\" y1=\"21\" x2=\"4\" y2=\"14\"/><line x1=\"4\" y1=\"10\" x2=\"4\" y2=\"3\"/><line x1=\"12\" y1=\"21\" x2=\"12\" y2=\"12\"/><line x1=\"12\" y1=\"8\" x2=\"12\" y2=\"3\"/><line x1=\"20\" y1=\"21\" x2=\"20\" y2=\"16\"/><line x1=\"20\" y1=\"12\" x2=\"20\" y2=\"3\"/><line x1=\"1\" y1=\"14\" x2=\"7\" y2=\"14\"/><line x1=\"9\" y1=\"8\" x2=\"15\" y2=\"8\"/><line x1=\"17\" y1=\"16\" x2=\"23\" y2=\"16\"/></svg>" },
      { "Move Up",      "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"18 15 12 9 6 15\"/></svg>" },
      { "Move Down",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"6 9 12 15 18 9\"/></svg>" },
      { "CANCEL",       "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"18\" y1=\"6\" x2=\"6\" y2=\"18\"/><line x1=\"6\" y1=\"6\" x2=\"18\" y2=\"18\"/></svg>" },
      { "Cancel",       "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"18\" y1=\"6\" x2=\"6\" y2=\"18\"/><line x1=\"6\" y1=\"6\" x2=\"18\" y2=\"18\"/></svg>" },
      { "Cancel Scan",  "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"18\" y1=\"6\" x2=\"6\" y2=\"18\"/><line x1=\"6\" y1=\"6\" x2=\"18\" y2=\"18\"/></svg>" },
      { "Cancel All",   "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"18\" y1=\"6\" x2=\"6\" y2=\"18\"/><line x1=\"6\" y1=\"6\" x2=\"18\" y2=\"18\"/></svg>" },
      { "X",            "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"18\" y1=\"6\" x2=\"6\" y2=\"18\"/><line x1=\"6\" y1=\"6\" x2=\"18\" y2=\"18\"/></svg>" },
      { "CLEAR",        "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"3 6 5 6 21 6\"/><path d=\"M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2\"/></svg>" },
      { "Clear All",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"3 6 5 6 21 6\"/><path d=\"M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2\"/></svg>" },
      { "Clear Tags",   "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"3 6 5 6 21 6\"/><path d=\"M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2\"/></svg>" },
      { "TRIM",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M6 2v14a2 2 0 0 0 2 2h14\"/><path d=\"M18 22V8a2 2 0 0 0-2-2H2\"/></svg>" },
      { "Save to Library", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z\"/><polyline points=\"17 21 17 13 7 13 7 21\"/><polyline points=\"7 3 7 8 15 8\"/></svg>" },
      { "To Editor",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6\"/><polyline points=\"15 3 21 3 21 9\"/><line x1=\"10\" y1=\"14\" x2=\"21\" y2=\"3\"/></svg>" },
      { "To Sampler",   "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"2\" y=\"4\" width=\"20\" height=\"16\" rx=\"2\"/><line x1=\"6\" y1=\"4\" x2=\"6\" y2=\"13\"/><line x1=\"10\" y1=\"4\" x2=\"10\" y2=\"13\"/><line x1=\"14\" y1=\"4\" x2=\"14\" y2=\"13\"/><line x1=\"18\" y1=\"4\" x2=\"18\" y2=\"13\"/><line x1=\"2\" y1=\"13\" x2=\"22\" y2=\"13\"/></svg>" },
      { "Audio Setup",  "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"4\" y1=\"21\" x2=\"4\" y2=\"14\"/><line x1=\"4\" y1=\"10\" x2=\"4\" y2=\"3\"/><line x1=\"12\" y1=\"21\" x2=\"12\" y2=\"12\"/><line x1=\"12\" y1=\"8\" x2=\"12\" y2=\"3\"/><line x1=\"20\" y1=\"21\" x2=\"20\" y2=\"16\"/><line x1=\"20\" y1=\"12\" x2=\"20\" y2=\"3\"/><line x1=\"1\" y1=\"14\" x2=\"7\" y2=\"14\"/><line x1=\"9\" y1=\"8\" x2=\"15\" y2=\"8\"/><line x1=\"17\" y1=\"16\" x2=\"23\" y2=\"16\"/></svg>" },
      { "Reset EQ",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"1 4 1 10 7 10\"/><path d=\"M3.51 15a9 9 0 1 0 2.13-9.36L1 10\"/></svg>" },
      { "Auto",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"13 2 3 14 12 14 11 22 21 10 12 10 13 2\"/></svg>" },
      { "Resynth",      "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M2 12c2-6 4-6 6 0s4 6 6 0 4-6 6 0\"/><circle cx=\"12\" cy=\"12\" r=\"1.5\" fill=\"currentColor\"/></svg>" },
      { "SYNC",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71\"/><path d=\"M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71\"/></svg>" },
      { "+ Add Effect", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"12\" cy=\"12\" r=\"10\"/><line x1=\"12\" y1=\"8\" x2=\"12\" y2=\"16\"/><line x1=\"8\" y1=\"12\" x2=\"16\" y2=\"12\"/></svg>" },
      { "Rack Presets", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"12 2 2 7 12 12 22 7 12 2\"/><polyline points=\"2 17 12 22 22 17\"/><polyline points=\"2 12 12 17 22 12\"/></svg>" },
      { "Master Active", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M18.36 6.64a9 9 0 1 1-12.73 0\"/><line x1=\"12\" y1=\"2\" x2=\"12\" y2=\"12\"/></svg>" },
      { "Bypassed",     "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M18.36 6.64a9 9 0 1 1-12.73 0\"/><line x1=\"12\" y1=\"2\" x2=\"12\" y2=\"12\"/></svg>" },
      { "Master Bypass", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M18.36 6.64a9 9 0 1 1-12.73 0\"/><line x1=\"12\" y1=\"2\" x2=\"12\" y2=\"12\"/></svg>" },
      { "Expand All",   "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"7 13 12 18 17 13\"/><polyline points=\"7 6 12 11 17 6\"/></svg>" },
      { "Collapse All", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"17 11 12 6 7 11\"/><polyline points=\"17 18 12 13 7 18\"/></svg>" },
      { "Reset View",   "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"15 3 21 3 21 9\"/><polyline points=\"9 21 3 21 3 15\"/><line x1=\"21\" y1=\"3\" x2=\"14\" y2=\"10\"/><line x1=\"3\" y1=\"21\" x2=\"10\" y2=\"14\"/></svg>" },
      { "2D",           "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z\"/></svg>" },
      { "3D",           "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z\"/></svg>" },
      { "Spin",         "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"23 4 23 10 17 10\"/><path d=\"M20.49 15a9 9 0 1 1-2.12-9.36L23 10\"/></svg>" },
      { "Stop Spin",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"23 4 23 10 17 10\"/><path d=\"M20.49 15a9 9 0 1 1-2.12-9.36L23 10\"/></svg>" },
      { "Mouseover",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polygon points=\"11 5 6 9 2 9 2 15 6 15 11 19 11 5\"/><path d=\"M15.54 8.46a5 5 0 0 1 0 7.07\"/><path d=\"M19.07 4.93a10 10 0 0 1 0 14.14\"/></svg>", true },
      { "+",            "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"12\" y1=\"5\" x2=\"12\" y2=\"19\"/><line x1=\"5\" y1=\"12\" x2=\"19\" y2=\"12\"/></svg>" },
      { "-",            "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"5\" y1=\"12\" x2=\"19\" y2=\"12\"/></svg>" },
      { "<",            "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"15 18 9 12 15 6\"/></svg>" },
      { ">",            "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"9 18 15 12 9 6\"/></svg>" },
      { "Fetch Files",  "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"8 17 12 21 16 17\"/><line x1=\"12\" y1=\"12\" x2=\"12\" y2=\"21\"/><path d=\"M20.88 18.09A5 5 0 0 0 18 9h-1.26A8 8 0 1 0 3 16.29\"/></svg>" },
      { "OWMB Soundbank", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><ellipse cx=\"12\" cy=\"5\" rx=\"9\" ry=\"3\"/><path d=\"M21 12c0 1.66-4 3-9 3s-9-1.34-9-3\"/><path d=\"M3 5v14c0 1.66 4 3 9 3s9-1.34 9-3V5\"/></svg>" },
      { "Change Folder...", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z\"/></svg>" },
      { "Download Folder", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z\"/><polyline points=\"12 11 12 17\"/><polyline points=\"9 14 12 17 15 14\"/></svg>", true },
      { "Download All", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4\"/><polyline points=\"7 10 12 15 17 10\"/><line x1=\"12\" y1=\"15\" x2=\"12\" y2=\"3\"/></svg>" },
      { "Favorites",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M20.84 4.61a5.5 5.5 0 0 0-7.78 0L12 5.67l-1.06-1.06a5.5 5.5 0 0 0-7.78 7.78l1.06 1.06L12 21.23l7.78-7.78 1.06-1.06a5.5 5.5 0 0 0 0-7.78z\"/></svg>" },
      { "+ Add Custom Tag...", "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M20.59 13.41l-7.17 7.17a2 2 0 0 1-2.83 0L2 12V2h10l8.59 8.59a2 2 0 0 1 0 2.82z\"/><line x1=\"7\" y1=\"7\" x2=\"7.01\" y2=\"7\"/></svg>" },
      { "+ BPM",        "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"12\" cy=\"12\" r=\"10\"/><polyline points=\"12 6 12 12 16 14\"/></svg>" },
      { "Spectral:",    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"2\" y=\"2\" width=\"20\" height=\"20\" rx=\"3\"/><line x1=\"6\" y1=\"18\" x2=\"6\" y2=\"12\"/><line x1=\"10\" y1=\"18\" x2=\"10\" y2=\"6\"/><line x1=\"14\" y1=\"18\" x2=\"14\" y2=\"10\"/><line x1=\"18\" y1=\"18\" x2=\"18\" y2=\"8\"/></svg>", true },
  };

  for (const auto& entry : iconTable)
  {
      const bool matched = entry.isPrefix ? text.startsWith(entry.match) : (text == entry.match);
      if (matched)
      {
          svgString = entry.svg;
          if (entry.overrideText != nullptr)
              text = entry.overrideText;
          break;
      }
  }

  auto font = getBodyFont(11.5f, true);
  g.setFont(font);
  bool isMuteBtn = (button.getButtonText() == "Mute" || button.getButtonText() == "Unmute");
  juce::Colour textColour = isMuteBtn ? (button.getToggleState() ? favoriteRed : textPrimary) : (button.getToggleState() ? accentCyan : textPrimary);
  if (shouldDrawButtonAsHighlighted && !button.getToggleState() && !isMuteBtn)
      textColour = juce::Colours::white;
  if (!button.isEnabled())
      textColour = textColour.withAlpha(0.32f);
  g.setColour(textColour);

  if (svgString.isNotEmpty()) {
      // Dynamic color injection into the SVG stroke/fill
      juce::String hexColour = textColour.toDisplayString(false);
      svgString = svgString.replace("currentColor", "#" + hexColour);

      auto xml = juce::XmlDocument::parse(svgString);
      if (xml != nullptr) {
          auto drawable = juce::Drawable::createFromSVG(*xml);
          if (drawable != nullptr) {
              if (button.getComponentID() == "editIconBtn" || button.getComponentID() == "headerViewBtn" || button.getComponentID() == "iconOnlyBtn") {
                  float iconSize = (button.getComponentID() == "headerViewBtn") ? 16.0f : 15.0f;
                  auto iconArea = button.getLocalBounds().toFloat().withSizeKeepingCentre(iconSize, iconSize);
                  drawable->drawWithin(g, iconArea, juce::RectanglePlacement::centred, 1.0f);
                  return;
              }

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

// ─────────────────────────────────────────────────────────
//  ComboBox Styling
// ─────────────────────────────────────────────────────────
void OpenWavLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                      int buttonX, int buttonY, int buttonW, int buttonH,
                                      juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
    float cornerRadius = 6.0f;

    // Background fill
    juce::Colour fillCol = bgCard;
    if (!box.isEnabled())
        fillCol = bgDark.withAlpha(0.6f);
    else if (isButtonDown)
        fillCol = bgCard.darker(0.12f);
    else if (box.isMouseOver())
        fillCol = bgHover;

    g.setColour(fillCol);
    g.fillRoundedRectangle(bounds, cornerRadius);

    // Border
    juce::Colour borderCol = borderColour;
    float strokeWidth = 1.0f;
    if (box.isEnabled())
    {
        if (box.hasKeyboardFocus(true) || isButtonDown)
        {
            borderCol = accentCyan;
            strokeWidth = 1.4f;
        }
        else if (box.isMouseOver())
        {
            borderCol = borderColour.brighter(0.35f);
        }
    }
    else
    {
        borderCol = borderColour.withAlpha(0.4f);
    }

    g.setColour(borderCol);
    g.drawRoundedRectangle(bounds, cornerRadius, strokeWidth);

    // Downward chevron
    float arrowW = 8.0f;
    float arrowH = 4.5f;
    float cx = static_cast<float>(buttonX) + static_cast<float>(buttonW) * 0.5f;
    float cy = static_cast<float>(buttonY) + static_cast<float>(buttonH) * 0.5f;

    juce::Path arrow;
    arrow.startNewSubPath(cx - arrowW * 0.5f, cy - arrowH * 0.5f);
    arrow.lineTo(cx, cy + arrowH * 0.5f);
    arrow.lineTo(cx + arrowW * 0.5f, cy - arrowH * 0.5f);

    juce::Colour arrowCol = textSecondary;
    if (!box.isEnabled())
        arrowCol = textSecondary.withAlpha(0.3f);
    else if (isButtonDown)
        arrowCol = accentCyan;
    else if (box.isMouseOver())
        arrowCol = textPrimary;

    g.setColour(arrowCol);
    g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::JointStyle::mitered, juce::PathStrokeType::EndCapStyle::rounded));
}

void OpenWavLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 0, box.getWidth() - 28, box.getHeight());
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
}

// ─────────────────────────────────────────────────────────
//  PopupMenu Styling
// ─────────────────────────────────────────────────────────
void OpenWavLookAndFeel::preparePopupMenuWindow(juce::Component& window)
{
    window.setOpaque(false);
}

int OpenWavLookAndFeel::getPopupMenuBorderSize()
{
    return 6;
}

void OpenWavLookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                                   int standardMenuItemHeight,
                                                   int& idealWidth, int& idealHeight)
{
    juce::LookAndFeel_V4::getIdealPopupMenuItemSize(text, isSeparator, standardMenuItemHeight, idealWidth, idealHeight);
    if (!isSeparator && idealHeight < 28)
        idealHeight = 28;
}

void OpenWavLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    auto area = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    auto menuArea = area.reduced(0.5f);
    float cornerSize = 8.0f;

    bool dark = isDarkTheme();

    if (dark)
    {
        // Deep sleek dark background with slight translucency
        g.setColour(juce::Colour(0xf4161b22));
        g.fillRoundedRectangle(menuArea, cornerSize);

        // Subtle modern border
        g.setColour(borderColour.brighter(0.15f));
        g.drawRoundedRectangle(menuArea, cornerSize, 1.0f);

        // Top highlight bevel for elevated floating depth
        juce::Path highlight;
        highlight.addRoundedRectangle(menuArea.getX() + 1.0f, menuArea.getY() + 1.0f,
                                      menuArea.getWidth() - 2.0f, 1.0f, 0.5f);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillPath(highlight);
    }
    else
    {
        // Elevated crisp light background with subtle soft tint
        g.setColour(juce::Colour(0xfafcfcfd));
        g.fillRoundedRectangle(menuArea, cornerSize);

        // Defined modern border for light theme
        g.setColour(borderColour.darker(0.12f));
        g.drawRoundedRectangle(menuArea, cornerSize, 1.0f);

        // Top crisp highlight
        juce::Path highlight;
        highlight.addRoundedRectangle(menuArea.getX() + 1.0f, menuArea.getY() + 1.0f,
                                      menuArea.getWidth() - 2.0f, 1.0f, 0.5f);
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.fillPath(highlight);
    }
}

void OpenWavLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                           bool isSeparator, bool isActive, bool isHighlighted,
                                           bool isTicked, bool hasSubMenu, const juce::String& text,
                                           const juce::String& shortcutKeyText,
                                           const juce::Drawable* icon, const juce::Colour* textColourToUse)
{
    bool dark = isDarkTheme();

    if (isSeparator)
    {
        auto r = area.reduced(8, 0);
        float lineY = static_cast<float>(r.getCentreY());
        g.setColour(dark ? borderColour.withAlpha(0.5f) : borderColour.withAlpha(0.85f));
        g.drawLine(static_cast<float>(r.getX()), lineY, static_cast<float>(r.getRight()), lineY, 1.0f);
        return;
    }

    auto textColour = textPrimary;
    if (textColourToUse != nullptr)
        textColour = *textColourToUse;

    if (!isActive)
        textColour = textSecondary.withAlpha(0.45f);

    // Highlighted item pill with rounded corners
    if (isHighlighted && isActive)
    {
        auto highlightBounds = area.reduced(4, 1).toFloat();
        if (dark)
        {
            g.setColour(accentCyan.withAlpha(0.18f));
            g.fillRoundedRectangle(highlightBounds, 6.0f);
            g.setColour(accentCyan.withAlpha(0.4f));
            g.drawRoundedRectangle(highlightBounds, 6.0f, 0.8f);
            textColour = accentCyan;
        }
        else
        {
            if (hasCustomPrimaryColour)
            {
                g.setColour(customPrimaryColour.withAlpha(0.14f));
                g.fillRoundedRectangle(highlightBounds, 6.0f);
                g.setColour(customPrimaryColour.withAlpha(0.35f));
                g.drawRoundedRectangle(highlightBounds, 6.0f, 0.8f);
                textColour = customPrimaryColour.getBrightness() > 0.55f ? customPrimaryColour.darker(0.35f) : customPrimaryColour;
            }
            else
            {
                g.setColour(bgHover);
                g.fillRoundedRectangle(highlightBounds, 6.0f);
                g.setColour(borderColour);
                g.drawRoundedRectangle(highlightBounds, 6.0f, 0.8f);
                textColour = textPrimary;
            }
        }
    }

    auto r = area.reduced(12, 0);

    // Modern checkmark for ticked items
    if (isTicked)
    {
        auto tickArea = r.removeFromLeft(18).toFloat();
        float cx = tickArea.getCentreX();
        float cy = tickArea.getCentreY();
        juce::Path tickPath;
        tickPath.startNewSubPath(cx - 4.5f, cy);
        tickPath.lineTo(cx - 1.5f, cy + 3.5f);
        tickPath.lineTo(cx + 4.5f, cy - 3.5f);
        juce::Colour tickCol;
        if (dark)
        {
            tickCol = isActive ? accentCyan : textSecondary.withAlpha(0.5f);
        }
        else
        {
            if (hasCustomPrimaryColour)
                tickCol = isActive ? (customPrimaryColour.getBrightness() > 0.55f ? customPrimaryColour.darker(0.3f) : customPrimaryColour)
                                   : textSecondary.withAlpha(0.5f);
            else
                tickCol = isActive ? textPrimary : textSecondary.withAlpha(0.5f);
        }
        g.setColour(tickCol);
        g.strokePath(tickPath, juce::PathStrokeType(1.8f, juce::PathStrokeType::JointStyle::mitered, juce::PathStrokeType::EndCapStyle::rounded));
    }

    if (icon != nullptr)
    {
        auto iconArea = r.removeFromLeft(20).toFloat();
        icon->drawWithin(g, iconArea, juce::RectanglePlacement::centred, 1.0f);
    }

    g.setFont(juce::Font(juce::FontOptions(12.5f)));
    g.setColour(textColour);
    g.drawText(text, r, juce::Justification::centredLeft, true);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour(textSecondary.withAlpha(0.7f));
        g.drawText(shortcutKeyText, r, juce::Justification::centredRight, true);
    }

    if (hasSubMenu)
    {
        auto arrowArea = r.removeFromRight(12).toFloat();
        float cx = arrowArea.getCentreX();
        float cy = arrowArea.getCentreY();
        juce::Path p;
        p.startNewSubPath(cx - 2.5f, cy - 4.5f);
        p.lineTo(cx + 2.0f, cy);
        p.lineTo(cx - 2.5f, cy + 4.5f);
        juce::Colour arrowCol;
        if (dark)
            arrowCol = isActive ? (isHighlighted ? accentCyan : textSecondary) : textSecondary.withAlpha(0.35f);
        else
            arrowCol = isActive ? (isHighlighted ? textPrimary : textSecondary) : textSecondary.withAlpha(0.35f);
        g.setColour(arrowCol);
        g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    }
}

void OpenWavLookAndFeel::drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area,
                                                    const juce::String& sectionName)
{
    g.setFont(juce::Font(juce::FontOptions(10.5f).withStyle("Bold")));
    if (isDarkTheme())
        g.setColour(accentCyan.withAlpha(0.85f));
    else
        g.setColour(hasCustomPrimaryColour ? (customPrimaryColour.getBrightness() > 0.55f ? customPrimaryColour.darker(0.3f) : customPrimaryColour)
                                           : textSecondary.darker(0.25f));
    auto r = area.reduced(12, 0);
    g.drawText(sectionName.toUpperCase(), r, juce::Justification::centredLeft, true);
}

void OpenWavLookAndFeel::drawPopupMenuUpDownArrow(juce::Graphics& g, int width, int height, bool isScrollUpArrow)
{
    g.setColour(bgCard);
    g.fillRect(0, 0, width, height);

    float cx = static_cast<float>(width) * 0.5f;
    float cy = static_cast<float>(height) * 0.5f;
    float arrowW = 8.0f;
    float arrowH = 4.5f;

    juce::Path p;
    if (isScrollUpArrow)
    {
        p.startNewSubPath(cx - arrowW * 0.5f, cy + arrowH * 0.5f);
        p.lineTo(cx, cy - arrowH * 0.5f);
        p.lineTo(cx + arrowW * 0.5f, cy + arrowH * 0.5f);
    }
    else
    {
        p.startNewSubPath(cx - arrowW * 0.5f, cy - arrowH * 0.5f);
        p.lineTo(cx, cy + arrowH * 0.5f);
        p.lineTo(cx + arrowW * 0.5f, cy - arrowH * 0.5f);
    }

    g.setColour(isDarkTheme() ? accentCyan : textPrimary);
    g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::JointStyle::mitered, juce::PathStrokeType::EndCapStyle::rounded));
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

void OpenWavLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                          bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 1.5f);
    bool ticked = button.getToggleState();
    bool enabled = button.isEnabled();

    juce::Colour pillBg = ticked ? accentCyan.withAlpha(0.16f)
                                 : (shouldDrawButtonAsHighlighted ? bgHover : bgCard);
    juce::Colour pillBorder = ticked ? accentCyan.withAlpha(0.75f) : borderColour;

    if (!enabled)
    {
        pillBg = pillBg.withAlpha(0.4f);
        pillBorder = pillBorder.withAlpha(0.4f);
    }

    g.setColour(pillBg);
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(pillBorder);
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

    float dotR = 3.2f;
    float dotX = bounds.getX() + 10.0f;
    float dotY = bounds.getCentreY();
    g.setColour(ticked ? accentCyan : textSecondary.withAlpha(0.5f));
    g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);

    auto textArea = bounds.withTrimmedLeft(18.0f).reduced(2.0f, 0.0f);
    g.setColour((ticked ? accentCyan : textPrimary).withAlpha(enabled ? 1.0f : 0.4f));
    g.setFont(juce::Font(juce::FontOptions(11.5f).withStyle(ticked ? "Bold" : "Regular")));
    g.drawFittedText(button.getButtonText(), textArea.toNearestInt(), juce::Justification::centredLeft, 1);
}

void OpenWavLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar& /*scrollbar*/,
                                       int x, int y, int width, int height,
                                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                       bool isMouseOver, bool isMouseDown)
{
    if (thumbSize <= 0)
        return;

    juce::Rectangle<float> thumbBounds;
    if (isScrollbarVertical)
        thumbBounds = juce::Rectangle<float>(static_cast<float>(x) + 2.5f, static_cast<float>(thumbStartPosition) + 1.0f,
                                             std::max(3.0f, static_cast<float>(width) - 5.0f),
                                             std::max(12.0f, static_cast<float>(thumbSize) - 2.0f));
    else
        thumbBounds = juce::Rectangle<float>(static_cast<float>(thumbStartPosition) + 1.0f, static_cast<float>(y) + 2.5f,
                                             std::max(12.0f, static_cast<float>(thumbSize) - 2.0f),
                                             std::max(3.0f, static_cast<float>(height) - 5.0f));

    juce::Colour thumbCol = isMouseDown ? accentCyan
                                        : (isMouseOver ? accentCyan.withAlpha(0.75f) : borderColour.brighter(0.35f));
    g.setColour(thumbCol);
    g.fillRoundedRectangle(thumbBounds, std::min(thumbBounds.getWidth(), thumbBounds.getHeight()) * 0.5f);
}

juce::Font OpenWavLookAndFeel::getTitleFont(float size)
{
    return juce::Font(juce::FontOptions(size).withStyle("Bold"));
}

juce::Font OpenWavLookAndFeel::getSectionHeaderFont(float size)
{
    return juce::Font(juce::FontOptions(size).withStyle("Bold"));
}

juce::Font OpenWavLookAndFeel::getBodyFont(float size, bool bold)
{
    return juce::Font(juce::FontOptions(size).withStyle(bold ? "Bold" : "Regular"));
}

juce::Font OpenWavLookAndFeel::getCaptionFont(float size, bool italic)
{
    return juce::Font(juce::FontOptions(size).withStyle(italic ? "Italic" : "Regular"));
}

juce::Font OpenWavLookAndFeel::getMonoFont(float size, bool bold)
{
#if JUCE_MAC || JUCE_IOS
    static const juce::String monoName = "SF Mono";
#elif JUCE_WINDOWS
    static const juce::String monoName = "Consolas";
#else
    static const juce::String monoName = juce::Font::getDefaultMonospacedFontName();
#endif
    auto opts = juce::FontOptions(monoName, size, bold ? juce::Font::bold : juce::Font::plain);
    return juce::Font(opts);
}

void OpenWavLookAndFeel::groupButtons(const std::vector<juce::Button*>& buttons, bool vertical)
{
    std::vector<juce::Button*> valid;
    valid.reserve(buttons.size());
    for (auto* b : buttons)
        if (b != nullptr)
            valid.push_back(b);

    if (valid.size() < 2)
        return;

    for (size_t i = 0; i < valid.size(); ++i)
    {
        int flags = 0;
        if (vertical)
        {
            if (i > 0) flags |= juce::Button::ConnectedOnTop;
            if (i + 1 < valid.size()) flags |= juce::Button::ConnectedOnBottom;
        }
        else
        {
            if (i > 0) flags |= juce::Button::ConnectedOnLeft;
            if (i + 1 < valid.size()) flags |= juce::Button::ConnectedOnRight;
        }
        valid[i]->setConnectedEdges(flags);
        valid[i]->setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
}

void OpenWavLookAndFeel::drawButtonGroup(juce::Graphics& g, const std::vector<juce::Component*>& buttons)
{
    std::vector<juce::Rectangle<int>> rects;
    rects.reserve(buttons.size());
    juce::Rectangle<int> unionRect;

    for (auto* btn : buttons)
    {
        if (btn == nullptr || !btn->isVisible() || btn->getBounds().isEmpty())
            continue;
        auto r = btn->getBounds();
        rects.push_back(r);
        unionRect = unionRect.isEmpty() ? r : unionRect.getUnion(r);
    }

    if (rects.size() < 2 || unionRect.isEmpty())
        return;

    auto pill = unionRect.expanded(3, 2).toFloat();
    g.setColour(bgDark.withAlpha(0.78f));
    g.fillRoundedRectangle(pill, 7.0f);
    g.setColour(borderColour);
    g.drawRoundedRectangle(pill.reduced(0.5f), 7.0f, 1.0f);

    g.setColour(borderColour.withAlpha(0.75f));
    for (size_t i = 0; i + 1 < rects.size(); ++i)
    {
        const auto& a = rects[i];
        const auto& b = rects[i + 1];
        if (b.getY() >= a.getBottom() - 2)
        {
            int divY = (a.getBottom() + b.getY()) / 2;
            g.fillRect(unionRect.getX() + 4, divY, unionRect.getWidth() - 8, 1);
        }
        else
        {
            int divX = (a.getRight() + b.getX()) / 2;
            g.fillRect(divX, unionRect.getY() + 4, 1, unionRect.getHeight() - 8);
        }
    }
}

void OpenWavLookAndFeel::drawEmptyState(juce::Graphics& g, juce::Rectangle<int> bounds,
                                        const juce::String& title, const juce::String& subtitle)
{
    int cardW = std::min(420, std::max(240, bounds.getWidth() - 40));
    int cardH = 148;
    auto card = bounds.withSizeKeepingCentre(cardW, cardH).toFloat();

    g.setColour(bgCard);
    g.fillRoundedRectangle(card, 10.0f);
    g.setColour(borderColour);
    g.drawRoundedRectangle(card.reduced(0.5f), 10.0f, 1.0f);

    // Minimal centered waveform icon
    auto iconRect = juce::Rectangle<float>(card.getCentreX() - 16.0f, card.getY() + 24.0f, 32.0f, 24.0f);
    g.setColour(accentCyan.withAlpha(0.75f));
    float cx = iconRect.getX() + 4.0f;
    float midY = iconRect.getCentreY();
    const float heights[5] = { 6.0f, 14.0f, 20.0f, 12.0f, 7.0f };
    for (int i = 0; i < 5; ++i)
    {
        float h = heights[i];
        g.fillRoundedRectangle(cx + i * 5.5f, midY - h * 0.5f, 3.0f, h, 1.5f);
    }

    g.setColour(textPrimary);
    g.setFont(getTitleFont(15.0f));
    g.drawText(title, card.withTrimmedTop(58.0f).removeFromTop(24.0f).reduced(16.0f, 0.0f).toNearestInt(),
               juce::Justification::centred, true);

    g.setColour(textSecondary);
    g.setFont(getBodyFont(12.0f, false));
    g.drawFittedText(subtitle, card.withTrimmedTop(86.0f).reduced(20.0f, 8.0f).toNearestInt(),
                     juce::Justification::centredTop, 2);
}

void OpenWavLookAndFeel::drawDropOverlay(juce::Graphics& g, juce::Rectangle<int> bounds,
                                         const juce::String& label)
{
    auto area = bounds.toFloat().reduced(8.0f);
    g.setColour(accentCyan.withAlpha(0.10f));
    g.fillRoundedRectangle(area, 10.0f);

    juce::Path borderPath;
    borderPath.addRoundedRectangle(area.reduced(1.0f), 10.0f);
    float dashes[2] = { 8.0f, 6.0f };
    juce::PathStrokeType stroke(2.0f);
    juce::Path dashed;
    stroke.createDashedStroke(dashed, borderPath, dashes, 2);
    g.setColour(accentCyan);
    g.fillPath(dashed);

    auto font = getBodyFont(13.0f, true);
    juce::GlyphArrangement ga;
    ga.addLineOfText(font, label, 0.0f, 0.0f);
    float pillW = std::min(static_cast<float>(bounds.getWidth()) - 40.0f,
                           ga.getBoundingBox(0, -1, false).getWidth() + 36.0f);
    auto pill = bounds.toFloat().withSizeKeepingCentre(std::max(180.0f, pillW), 36.0f);

    g.setColour(bgHeader.withAlpha(0.96f));
    g.fillRoundedRectangle(pill, 8.0f);
    g.setColour(accentCyan);
    g.drawRoundedRectangle(pill.reduced(0.5f), 8.0f, 1.2f);
    g.setColour(textPrimary);
    g.setFont(font);
    g.drawText(label, pill.toNearestInt(), juce::Justification::centred, true);
}

juce::Typeface::Ptr OpenWavLookAndFeel::getTypefaceForFont(const juce::Font& font)
{
    juce::String style = font.getTypefaceStyle();
    bool isBold = font.isBold() || style.containsIgnoreCase("bold");
    bool isItalic = font.isItalic() || style.containsIgnoreCase("italic");

#if JUCE_MAC || JUCE_IOS
    static const juce::String preferredFonts[] = {
        "SF Pro Text", "SF Pro Display", ".AppleSystemUIFont", "Helvetica Neue", "Arial"
    };
#elif JUCE_WINDOWS
    static const juce::String preferredFonts[] = {
        "Segoe UI", "Aptos", "Calibri", "Arial"
    };
#else
    static const juce::String preferredFonts[] = {
        "Inter", "Roboto", "Ubuntu", "DejaVu Sans", "Liberation Sans", "Sans-Serif"
    };
#endif

    for (const auto& fontName : preferredFonts)
    {
        juce::FontOptions options;
        options = options.withName(fontName);
        if (isBold && isItalic)
            options = options.withStyle("Bold Italic");
        else if (isBold)
            options = options.withStyle("Bold");
        else if (isItalic)
            options = options.withStyle("Italic");
        else if (style.isNotEmpty())
            options = options.withStyle(style);
        else
            options = options.withStyle("Regular");

        auto tf = juce::Typeface::createSystemTypefaceFor(options);
        if (tf != nullptr)
            return tf;
    }

    return juce::LookAndFeel_V4::getTypefaceForFont(font);
}

juce::Font OpenWavLookAndFeel::getLabelFont(juce::Label& label)
{
    return label.getFont();
}

juce::Font OpenWavLookAndFeel::getTextButtonFont(juce::TextButton& /*button*/, int buttonHeight)
{
    return juce::Font(juce::FontOptions(std::min(13.0f, (float)buttonHeight * 0.52f)).withStyle("Bold"));
}

juce::Font OpenWavLookAndFeel::getComboBoxFont(juce::ComboBox& /*box*/)
{
    return juce::Font(juce::FontOptions(12.5f));
}

juce::Font OpenWavLookAndFeel::getPopupMenuFont()
{
    return juce::Font(juce::FontOptions(13.0f));
}

juce::Font OpenWavLookAndFeel::getAlertWindowTitleFont()
{
    return juce::Font(juce::FontOptions(15.0f).withStyle("Bold"));
}

juce::Font OpenWavLookAndFeel::getAlertWindowMessageFont()
{
    return juce::Font(juce::FontOptions(13.0f));
}

void OpenWavLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    g.setColour(juce::Colour(0xF0181B22)); // Deep modern dark tooltip background
    g.fillRoundedRectangle(bounds, 5.0f);

    g.setColour(borderColour.withAlpha(0.75f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);

    g.setColour(textPrimary);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawFittedText(text, bounds.toNearestInt().reduced(8, 3), juce::Justification::centred, 1);
}

juce::Rectangle<int> OpenWavLookAndFeel::getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    auto font = juce::Font(juce::FontOptions(12.0f));
    int textWidth = font.getStringWidth(tipText) + 20;
    int textHeight = 26;

    auto targetArea = juce::Rectangle<int>(screenPos.x + 10, screenPos.y + 16, textWidth, textHeight);
    return targetArea.constrainedWithin(parentArea);
}

} // namespace openwav
