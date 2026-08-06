#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif

namespace openwav
{

class AboutDialog : public juce::Component
{
public:
    AboutDialog();
    ~AboutDialog() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

    void showDialog();
    void hideDialog();

private:
    juce::Label titleLabel { {}, "OWMB" };
    juce::Label subtitleLabel { {}, "OpenWav Media Box" };
    juce::Label versionLabel { {}, "Version 1.0.0" };
    juce::Label descriptionLabel;
    juce::Label licenseLabel { {}, "License: MIT License (Open Source)" };
    juce::Label copyrightLabel { {}, "Copyright \xc2\xa9 2026 OWMB Contributors" };

    juce::TextButton githubButton { "GitHub Repo" };
    juce::TextButton websiteButton { "Website" };
    juce::TextButton closeButton { "Close" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AboutDialog)
};

} // namespace openwav
