#include "AboutDialog.h"
#include "OpenWavLookAndFeel.h"
#if __has_include(<BinaryData.h>)
 #include <BinaryData.h>
#endif

namespace openwav
{

AboutDialog::AboutDialog()
{
    setAlwaysOnTop(true);
    setBufferedToImage(true);
    setVisible(false);

    // Icon Logo Setup
    juce::Image logoImage;
#if defined(JUCE_BINARYDATA_H_INCLUDED) || __has_include(<BinaryData.h>)
    logoImage = juce::ImageFileFormat::loadFrom(BinaryData::owmbico_png, static_cast<size_t>(BinaryData::owmbico_pngSize));
    if (logoImage.isNull())
        logoImage = juce::ImageFileFormat::loadFrom(BinaryData::owmblogo_png, static_cast<size_t>(BinaryData::owmblogo_pngSize));
#endif

    if (logoImage.isNull())
    {
        juce::File logoFile = juce::File::getCurrentWorkingDirectory().getChildFile("owmbico.png");
        if (!logoFile.existsAsFile())
            logoFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("owmbico.png");
        if (logoFile.existsAsFile())
            logoImage = juce::ImageFileFormat::loadFrom(logoFile);
    }

    if (!logoImage.isNull())
    {
        logoComponent.setImage(logoImage, juce::RectanglePlacement::xMid | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize);
        addAndMakeVisible(logoComponent);
    }

    // Title
    titleLabel.setFont(juce::Font(24.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    // Subtitle ("OpenWav Media Browser")
    subtitleLabel.setFont(juce::Font(15.0f).boldened());
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    subtitleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(subtitleLabel);

    // Version
    versionLabel.setFont(juce::Font(12.0f));
    versionLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFCBD5E1));
    versionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(versionLabel);

    // Description
    descriptionLabel.setFont(juce::Font(13.0f));
    descriptionLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFF1F5F9));
    descriptionLabel.setJustificationType(juce::Justification::centred);
    descriptionLabel.setText("High-performance Audio Sample Library Manager, DSP Analyzer, & Cloud Integrator.", juce::dontSendNotification);
    addAndMakeVisible(descriptionLabel);

    // License Button
    licenseButton.setTooltip("https://opensource.org/licenses/MIT");
    licenseButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1E293B));
    licenseButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);
    licenseButton.onClick = [] {
        juce::URL("https://opensource.org/licenses/MIT").launchInDefaultBrowser();
    };
    addAndMakeVisible(licenseButton);

    // Copyright
    copyrightLabel.setFont(juce::Font(12.0f));
    copyrightLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF94A3B8));
    copyrightLabel.setJustificationType(juce::Justification::centred);
    copyrightLabel.setText(juce::CharPointer_UTF8("Copyright \xc2\xa9 2026 OWMB Contributors"), juce::dontSendNotification);
    addAndMakeVisible(copyrightLabel);

    // GitHub Button
    githubButton.setButtonText("GitHub Repo");
    githubButton.setTooltip("https://github.com/samplaman/owmb");
    githubButton.onClick = [] {
        juce::URL("https://github.com/samplaman/owmb").launchInDefaultBrowser();
    };
    addAndMakeVisible(githubButton);

    // Website Button
    websiteButton.setButtonText("Website / Pixeldrain");
    websiteButton.setTooltip("https://pixeldrain.com/");
    websiteButton.onClick = [] {
        juce::URL("https://pixeldrain.com/").launchInDefaultBrowser();
    };
    addAndMakeVisible(websiteButton);

    // Close Button
    closeButton.setButtonText("Close");
    closeButton.onClick = [this] {
        hideDialog();
    };
    addAndMakeVisible(closeButton);

    lookAndFeelChanged();
}

void AboutDialog::lookAndFeelChanged()
{
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    subtitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    versionLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    descriptionLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    copyrightLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);

    licenseButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgHover);
    licenseButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentBlue);

    repaint();
}

void AboutDialog::showDialog()
{
    lookAndFeelChanged();
    setVisible(true);
    toFront(true);
}

void AboutDialog::hideDialog()
{
    setVisible(false);
}

void AboutDialog::mouseDown(const juce::MouseEvent& /*event*/)
{
    // Intercept mouse clicks so modal dialog blocks clicks behind it
}

void AboutDialog::paint(juce::Graphics& g)
{
    if (!isVisible())
        return;

    auto bounds = getLocalBounds();

    // 1. Semi-transparent backdrop overlay over full app window
    g.fillAll(juce::Colours::black.withAlpha(0.65f));

    // 2. Central Card Bounds
    int cardWidth = std::min(480, bounds.getWidth() - 40);
    int cardHeight = 350;
    auto cardBounds = bounds.withSizeKeepingCentre(cardWidth, cardHeight).toFloat();

    // Card background drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(cardBounds.translated(0.0f, 4.0f), 14.0f);

    // Card background
    g.setColour(OpenWavLookAndFeel::bgCard);
    g.fillRoundedRectangle(cardBounds, 14.0f);

    // Card border
    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.6f));
    g.drawRoundedRectangle(cardBounds, 14.0f, 1.5f);

    // Separator line above buttons
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.6f));
    g.drawHorizontalLine(static_cast<int>(cardBounds.getBottom() - 56.0f), cardBounds.getX() + 20.0f, cardBounds.getRight() - 20.0f);
}

void AboutDialog::resized()
{
    auto bounds = getLocalBounds();
    int cardWidth = std::min(480, bounds.getWidth() - 40);
    int cardHeight = 350;
    auto cardBounds = bounds.withSizeKeepingCentre(cardWidth, cardHeight);

    int cx = cardBounds.getX();
    int cy = cardBounds.getY();
    int cw = cardBounds.getWidth();

    int curY = cy + 16;
    if (logoComponent.isVisible())
    {
        logoComponent.setBounds(cardBounds.getCentreX() - 30, curY, 60, 60);
        curY += 66;
    }

    titleLabel.setBounds(cx + 20, curY, cw - 40, 26);
    curY += 28;

    subtitleLabel.setBounds(cx + 20, curY, cw - 40, 20);
    curY += 22;

    versionLabel.setBounds(cx + 20, curY, cw - 40, 18);
    curY += 24;

    descriptionLabel.setBounds(cx + 24, curY, cw - 48, 36);
    curY += 38;

    licenseButton.setBounds(cardBounds.getCentreX() - 110, curY, 220, 24);
    curY += 28;

    copyrightLabel.setBounds(cx + 20, curY, cw - 40, 18);

    // Buttons Row at bottom
    int btnY = cardBounds.getBottom() - 46;
    int btnWidth = 120;
    int gap = 10;
    int totalBtnsW = btnWidth * 3 + gap * 2;
    int startX = cardBounds.getCentreX() - totalBtnsW / 2;

    githubButton.setBounds(startX, btnY, btnWidth, 32);
    websiteButton.setBounds(startX + btnWidth + gap, btnY, btnWidth, 32);
    closeButton.setBounds(startX + (btnWidth + gap) * 2, btnY, btnWidth, 32);
}

} // namespace openwav
