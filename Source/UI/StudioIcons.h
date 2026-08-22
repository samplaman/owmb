#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif

namespace openwav
{
namespace StudioIcons
{

inline std::unique_ptr<juce::Drawable> createFromSvgString(const juce::String& svgXml)
{
    auto xml = juce::XmlDocument::parse(svgXml);
    if (xml != nullptr)
        return juce::Drawable::createFromSVG(*xml);
    return nullptr;
}

// -----------------------------------------------------------------------------
// Play Triangle SVG
// -----------------------------------------------------------------------------
inline juce::String getPlaySvg(const juce::String& color = "currentColor")
{
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"" + color + "\" stroke=\"none\">"
           "<polygon points=\"6 3 20 12 6 21 6 3\"/>"
           "</svg>";
}

// -----------------------------------------------------------------------------
// Download Arrow SVG
// -----------------------------------------------------------------------------
inline juce::String getDownloadSvg(const juce::String& color = "currentColor")
{
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"" + color + "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
           "<path d=\"M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4\"/>"
           "<polyline points=\"7 10 12 15 17 10\"/>"
           "<line x1=\"12\" y1=\"15\" x2=\"12\" y2=\"3\"/>"
           "</svg>";
}

// -----------------------------------------------------------------------------
// Checkmark SVG
// -----------------------------------------------------------------------------
inline juce::String getCheckSvg(const juce::String& color = "#10B981")
{
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"" + color + "\" stroke-width=\"2.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
           "<polyline points=\"20 6 9 17 4 12\"/>"
           "</svg>";
}

// -----------------------------------------------------------------------------
// Wi-Fi / Radio Beacon SVG
// -----------------------------------------------------------------------------
inline juce::String getWifiSvg(const juce::String& color = "currentColor")
{
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"" + color + "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
           "<path d=\"M5 12.55a11 11 0 0 1 14.08 0\"/>"
           "<path d=\"M1.42 9a16 16 0 0 1 21.16 0\"/>"
           "<path d=\"M8.53 16.11a6 6 0 0 1 6.95 0\"/>"
           "<circle cx=\"12\" cy=\"20\" r=\"1\" fill=\"" + color + "\"/>"
           "</svg>";
}

// -----------------------------------------------------------------------------
// Refresh Rotate SVG
// -----------------------------------------------------------------------------
inline juce::String getRefreshSvg(const juce::String& color = "currentColor")
{
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"" + color + "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
           "<polyline points=\"23 4 23 10 17 10\"/>"
           "<path d=\"M20.49 15a9 9 0 1 1-2.12-9.36L23 10\"/>"
           "</svg>";
}

// -----------------------------------------------------------------------------
// Folder SVG
// -----------------------------------------------------------------------------
inline juce::String getFolderSvg(const juce::String& color = "currentColor")
{
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"" + color + "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
           "<path d=\"M4 20h16a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.93a2 2 0 0 1-1.66-.9l-.82-1.2A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13c0 1.1.9 2 2 2Z\"/>"
           "</svg>";
}

// -----------------------------------------------------------------------------
// Copy Document SVG
// -----------------------------------------------------------------------------
inline juce::String getCopySvg(const juce::String& color = "currentColor")
{
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"" + color + "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
           "<rect width=\"14\" height=\"14\" x=\"8\" y=\"8\" rx=\"2\" ry=\"2\"/>"
           "<path d=\"M4 16c-1.1 0-2-.9-2-2V4c0-1.1.9-2 2-2h10c1.1 0 2 .9 2 2\"/>"
           "</svg>";
}

// -----------------------------------------------------------------------------
// Search / Scan SVG
// -----------------------------------------------------------------------------
inline juce::String getSearchSvg(const juce::String& color = "currentColor")
{
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"" + color + "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
           "<circle cx=\"11\" cy=\"11\" r=\"8\"/>"
           "<line x1=\"21\" y1=\"21\" x2=\"16.65\" y2=\"16.65\"/>"
           "</svg>";
}

// -----------------------------------------------------------------------------
// Smartphone SVG
// -----------------------------------------------------------------------------
inline juce::String getSmartphoneSvg(const juce::String& color = "currentColor")
{
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"" + color + "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
           "<rect width=\"14\" height=\"20\" x=\"5\" y=\"2\" rx=\"2\" ry=\"2\"/>"
           "<line x1=\"12\" y1=\"18\" x2=\"12.01\" y2=\"18\"/>"
           "</svg>";
}

// -----------------------------------------------------------------------------
// Check Circle SVG
// -----------------------------------------------------------------------------
inline juce::String getCheckCircleSvg(const juce::String& color = "#10B981")
{
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"" + color + "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
           "<path d=\"M22 11.08V12a10 10 0 1 1-5.93-9.14\"/>"
           "<polyline points=\"22 4 12 14.01 9 11.01\"/>"
           "</svg>";
}

// -----------------------------------------------------------------------------
// Radio Broadcast Circle SVG
// -----------------------------------------------------------------------------
inline juce::String getRadioSvg(const juce::String& color = "currentColor")
{
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"" + color + "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
           "<circle cx=\"12\" cy=\"12\" r=\"2\" fill=\"" + color + "\"/>"
           "<path d=\"M16.24 7.76a6 6 0 0 1 0 8.49m-8.48-.01a6 6 0 0 1 0-8.49m11.31-2.82a10 10 0 0 1 0 14.14m-14.14 0a10 10 0 0 1 0-14.14\"/>"
           "</svg>";
}

} // namespace StudioIcons

// -----------------------------------------------------------------------------
// SvgIconButton Component
// -----------------------------------------------------------------------------
class SvgIconButton : public juce::Button
{
public:
    SvgIconButton(const juce::String& buttonText = {}, const juce::String& svgXml = {})
        : juce::Button(buttonText), labelText(buttonText)
    {
        if (svgXml.isNotEmpty())
            setSvg(svgXml);
    }

    void setSvg(const juce::String& svgXml)
    {
        svgDrawable = StudioIcons::createFromSvgString(svgXml);
        repaint();
    }

    void setCustomColours(juce::Colour bg, juce::Colour text, juce::Colour border)
    {
        customBg = bg;
        customText = text;
        customBorder = border;
        hasCustomColours = true;
        repaint();
    }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        const float cornerSize = 5.0f;

        juce::Colour bg = hasCustomColours ? customBg : juce::Colours::transparentBlack;
        juce::Colour fg = hasCustomColours ? customText : juce::Colours::white;
        juce::Colour border = hasCustomColours ? customBorder : juce::Colour(0x44FFFFFF);

        if (!isEnabled())
        {
            bg = bg.withAlpha(0.3f);
            fg = fg.withAlpha(0.4f);
            border = border.withAlpha(0.2f);
        }
        else if (shouldDrawButtonAsDown)
        {
            bg = bg.withMultipliedBrightness(0.8f).withAlpha(0.9f);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            bg = bg.withMultipliedBrightness(1.15f);
            border = juce::Colours::white.withAlpha(0.6f);
        }

        g.setColour(bg);
        g.fillRoundedRectangle(bounds, cornerSize);

        g.setColour(border);
        g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

        auto contentArea = bounds.reduced(6.0f, 4.0f);

        if (svgDrawable != nullptr)
        {
            if (labelText.isNotEmpty())
            {
                float iconSize = std::min(14.0f, contentArea.getHeight());
                auto iconArea = contentArea.removeFromLeft(iconSize);
                iconArea.setY(contentArea.getCentreY() - iconSize * 0.5f);
                iconArea.setHeight(iconSize);

                svgDrawable->drawWithin(g, iconArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
                contentArea.removeFromLeft(6.0f);

                g.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
                g.setColour(fg);
                g.drawText(labelText, contentArea, juce::Justification::centredLeft, true);
            }
            else
            {
                svgDrawable->drawWithin(g, contentArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
            }
        }
        else if (labelText.isNotEmpty())
        {
            g.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
            g.setColour(fg);
            g.drawText(labelText, contentArea, juce::Justification::centred, true);
        }
    }

private:
    juce::String labelText;
    std::unique_ptr<juce::Drawable> svgDrawable;
    juce::Colour customBg { juce::Colours::transparentBlack };
    juce::Colour customText { juce::Colours::white };
    juce::Colour customBorder { juce::Colour(0x44FFFFFF) };
    bool hasCustomColours { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SvgIconButton)
};

} // namespace openwav
