#include "AboutDialog.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

AboutDialog::AboutDialog()
{
    setAlwaysOnTop(true);
    setBufferedToImage(true);
    setVisible(false);

    // Title
    titleLabel.setFont(juce::Font(22.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    // Subtitle
    subtitleLabel.setFont(juce::Font(14.0f).boldened());
    subtitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    subtitleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(subtitleLabel);

    // Version
    versionLabel.setFont(juce::Font(12.0f));
    versionLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    versionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(versionLabel);

    // Description
    descriptionLabel.setFont(juce::Font(12.0f));
    descriptionLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    descriptionLabel.setJustificationType(juce::Justification::centred);
    descriptionLabel.setText("High-performance Audio Sample Library Manager, DSP Analyzer, & Cloud Integrator.", juce::dontSendNotification);
    addAndMakeVisible(descriptionLabel);

    // License
    licenseLabel.setFont(juce::Font(12.0f).boldened());
    licenseLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentBlue);
    licenseLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(licenseLabel);

    // Copyright
    copyrightLabel.setFont(juce::Font(11.0f));
    copyrightLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    copyrightLabel.setJustificationType(juce::Justification::centred);
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
}

void AboutDialog::showDialog()
{
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
    int cardHeight = 290;
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
    int cardHeight = 290;
    auto cardBounds = bounds.withSizeKeepingCentre(cardWidth, cardHeight);

    int cx = cardBounds.getX();
    int cy = cardBounds.getY();
    int cw = cardBounds.getWidth();

    titleLabel.setBounds(cx + 20, cy + 18, cw - 40, 26);
    subtitleLabel.setBounds(cx + 20, cy + 46, cw - 40, 20);
    versionLabel.setBounds(cx + 20, cy + 68, cw - 40, 18);

    descriptionLabel.setBounds(cx + 24, cy + 96, cw - 48, 36);
    licenseLabel.setBounds(cx + 20, cy + 138, cw - 40, 20);
    copyrightLabel.setBounds(cx + 20, cy + 160, cw - 40, 18);

    // Buttons Row at bottom
    int btnY = cardBounds.getBottom() - 44;
    int btnWidth = 120;
    int gap = 10;
    int totalBtnsW = btnWidth * 3 + gap * 2;
    int startX = cardBounds.getCentreX() - totalBtnsW / 2;

    githubButton.setBounds(startX, btnY, btnWidth, 32);
    websiteButton.setBounds(startX + btnWidth + gap, btnY, btnWidth, 32);
    closeButton.setBounds(startX + (btnWidth + gap) * 2, btnY, btnWidth, 32);
}

} // namespace openwav
