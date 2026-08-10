#include "SimilarityGraphPopup.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

SimilarityGraphPopup::SimilarityGraphPopup()
{
    setInterceptsMouseClicks(false, false);
    setAlwaysOnTop(true);
}

void SimilarityGraphPopup::showComparison(const MediaItem* targetItem, const MediaItem* hoveredItem, juce::Point<int> screenPos)
{
    if (targetItem && hoveredItem)
    {
        currentTarget = *targetItem;
        currentHovered = *hoveredItem;
        hasData = true;
        
        setSize(220, 220);
        setTopLeftPosition(screenPos.x + 20, screenPos.y + 20);
        setVisible(true);
        repaint();
    }
    else
    {
        hidePopup();
    }
}

void SimilarityGraphPopup::hidePopup()
{
    hasData = false;
    setVisible(false);
}

void SimilarityGraphPopup::paint(juce::Graphics& g)
{
    if (!hasData) return;

    auto area = getLocalBounds().toFloat();
    
    // Glassmorphic background
    g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.85f));
    g.fillRoundedRectangle(area, 10.0f);
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.5f));
    g.drawRoundedRectangle(area, 10.0f, 1.0f);
    
    // Radar Chart Drawing
    auto center = area.getCentre();
    float radius = std::min(area.getWidth(), area.getHeight()) * 0.5f - 30.0f;
    
    // Draw radar background web (4 axes)
    g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.2f));
    for (int i = 1; i <= 3; ++i)
    {
        float r = radius * (i / 3.0f);
        juce::Path web;
        web.addPolygon(center, 4, r, juce::MathConstants<float>::pi * 0.25f);
        g.strokePath(web, juce::PathStrokeType(1.0f));
    }
    
    // Draw axes lines
    for (int i = 0; i < 4; ++i)
    {
        float angle = juce::MathConstants<float>::pi * 0.25f + i * juce::MathConstants<float>::halfPi;
        g.drawLine(center.x, center.y, center.x + std::sin(angle) * radius, center.y - std::cos(angle) * radius, 1.0f);
    }
    
    // Axis Labels
    g.setColour(OpenWavLookAndFeel::textSecondary);
    g.setFont(juce::Font(10.0f).boldened());
    float labelOffset = radius + 12.0f;
    const juce::String labels[4] = { "FREQ", "DECAY", "ZCR", "ENERGY" };
    for (int i = 0; i < 4; ++i)
    {
        float angle = juce::MathConstants<float>::pi * 0.25f + i * juce::MathConstants<float>::halfPi;
        auto p = center.translated(std::sin(angle) * labelOffset, -std::cos(angle) * labelOffset);
        g.drawText(labels[i], juce::Rectangle<float>(40, 20).withCentre(p).toNearestInt(), juce::Justification::centred, false);
    }
    
    // Get normalized feature arrays for target [0] and hovered [1]
    auto getNormFeatures = [](const MediaItem& item) -> std::array<float, 4> {
        return {
            juce::jlimit(0.0f, 1.0f, static_cast<float>(item.highFreqRatio)),           // FREQ
            juce::jlimit(0.0f, 1.0f, static_cast<float>(item.decayRatio)),               // DECAY
            juce::jlimit(0.0f, 1.0f, static_cast<float>(item.zcr)),                      // ZCR
            juce::jlimit(0.0f, 1.0f, static_cast<float>(item.crestFactor / 10.0f))       // ENERGY (Crest factor inverted?)
        };
    };
    
    auto targetF = getNormFeatures(currentTarget);
    auto hoveredF = getNormFeatures(currentHovered);
    
    auto createPolygon = [&](const std::array<float, 4>& features) -> juce::Path {
        juce::Path p;
        for (int i = 0; i < 4; ++i)
        {
            float angle = juce::MathConstants<float>::pi * 0.25f + i * juce::MathConstants<float>::halfPi;
            float r = radius * features[i];
            juce::Point<float> pt = center.translated(std::sin(angle) * r, -std::cos(angle) * r);
            if (i == 0) p.startNewSubPath(pt);
            else        p.lineTo(pt);
        }
        p.closeSubPath();
        return p;
    };
    
    // Draw Target Shape (Primary)
    juce::Path targetPath = createPolygon(targetF);
    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.2f));
    g.fillPath(targetPath);
    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.strokePath(targetPath, juce::PathStrokeType(1.5f));
    
    // Draw Hovered Shape (White)
    juce::Path hoveredPath = createPolygon(hoveredF);
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.fillPath(hoveredPath);
    g.setColour(juce::Colours::white);
    g.strokePath(hoveredPath, juce::PathStrokeType(1.5f));
    
    // Legend
    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.drawText("Target", area.removeFromBottom(24).withLeft(8).toNearestInt(), juce::Justification::bottomLeft, false);
    g.setColour(juce::Colours::white);
    g.drawText("Similar", area.removeFromBottom(24).withRight(area.getRight() - 8).toNearestInt(), juce::Justification::bottomRight, false);
}

} // namespace openwav
