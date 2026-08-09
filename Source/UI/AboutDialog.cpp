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

    setSize(480, 350);
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
    
    if (dialogWindow == nullptr)
    {
        juce::DialogWindow::LaunchOptions opts;
        opts.content.setNonOwned(this);
        opts.dialogTitle = "About OWMB";
        opts.dialogBackgroundColour = OpenWavLookAndFeel::bgCard;
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = true;
        opts.resizable = false;
        
        dialogWindow = opts.launchAsync();
    }
}

void AboutDialog::hideDialog()
{
    if (dialogWindow != nullptr)
        dialogWindow->exitModalState(0);
}

void AboutDialog::mouseDown(const juce::MouseEvent& /*event*/)
{
}

void AboutDialog::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgCard);

    // Separator line above buttons
    auto bounds = getLocalBounds();
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.6f));
    g.drawHorizontalLine(static_cast<int>(bounds.getBottom() - 56.0f), 20.0f, bounds.getRight() - 20.0f);
}

void AboutDialog::resized()
{
    auto bounds = getLocalBounds();
    
    int cx = bounds.getX();
    int cy = bounds.getY();
    int cw = bounds.getWidth();

    int curY = cy + 16;
    
    if (logoComponent.getImage().isValid())
    {
        logoComponent.setBounds(cx + (cw - 64) / 2, curY, 64, 64);
        curY += 76;
    }

    titleLabel.setBounds(cx, curY, cw, 30);
    curY += 28;
    subtitleLabel.setBounds(cx, curY, cw, 24);
    curY += 24;
    versionLabel.setBounds(cx, curY, cw, 20);
    curY += 32;

    descriptionLabel.setBounds(cx + 20, curY, cw - 40, 40);
    curY += 48;

    licenseButton.setBounds(cx + (cw - 200) / 2, curY, 200, 24);
    curY += 32;

    copyrightLabel.setBounds(cx, curY, cw, 20);
    
    // Bottom Buttons Row
    int btnWidth = 100;
    int gap = 16;
    int totalWidth = btnWidth * 3 + gap * 2;
    int startX = cx + (cw - totalWidth) / 2;
    int btnY = bounds.getBottom() - 44;

    githubButton.setBounds(startX, btnY, btnWidth, 28);
    websiteButton.setBounds(startX + btnWidth + gap, btnY, btnWidth, 28);
    closeButton.setBounds(startX + (btnWidth + gap) * 2, btnY, btnWidth, 28);
}

} // namespace openwav
