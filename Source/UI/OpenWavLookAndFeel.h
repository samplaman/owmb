#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif

namespace openwav
{

class OpenWavLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OpenWavLookAndFeel();
    ~OpenWavLookAndFeel() override = default;

    // Color Constants
    static juce::Colour bgDark;
    static juce::Colour bgHeader;
    static juce::Colour bgCard;
    static juce::Colour bgHover;
    static juce::Colour accentCyan;
    static juce::Colour accentBlue;
    static juce::Colour textPrimary;
    static juce::Colour textSecondary;
    static juce::Colour borderColour;
    static juce::Colour favoriteRed;

    // Theme switching
    static void setDarkTheme(bool useDark);
    static bool isDarkTheme() { return bgDark.getBrightness() < 0.5f; }
    static void setPrimaryColour(juce::Colour colour);
    static void resetPrimaryColour();
    static bool hasCustomPrimaryColour;
    static juce::Colour customPrimaryColour;
    void updateColors();

    // JUCE LookAndFeel Overrides
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                             const juce::Colour& backgroundColour,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    void drawTableHeaderColumn(juce::Graphics& g, juce::TableHeaderComponent& header,
                                const juce::String& columnName, int columnId,
                                int width, int height, bool isMouseOver,
                                bool isMouseDown, int columnFlags) override;

    void drawTableHeaderBackground(juce::Graphics& g, juce::TableHeaderComponent& header) override;

    void drawTextEditorOutline(juce::Graphics& g, int width, int height,
                               juce::TextEditor& textEditor) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override;

    // ComboBox Styling
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

    // PopupMenu Styling
    void preparePopupMenuWindow(juce::Component& window) override;
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;
    int getPopupMenuBorderSize() override;
    int getIdealPopupMenuItemHeight(const juce::String& text, bool isSeparator, int standardMenuItemHeight) override;

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area,
                                    const juce::String& sectionName) override;
    void drawPopupMenuUpDownArrow(juce::Graphics& g, int width, int height, bool isScrollUpArrow) override;

    void drawTickBox(juce::Graphics& g, juce::Component& component,
                     float x, float y, float w, float h,
                     bool ticked, bool isEnabled,
                     bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;

    // Modern Typography System
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& font) override;
    juce::Font getLabelFont(juce::Label& label) override;
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
    juce::Font getComboBoxFont(juce::ComboBox& box) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
};

} // namespace openwav
