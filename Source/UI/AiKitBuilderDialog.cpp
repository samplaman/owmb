#include "AiKitBuilderDialog.h"

namespace openwav
{

AiKitBuilderDialog::AiKitBuilderDialog(TagDatabaseManager& db,
                                       std::function<void(const AiKitBuildResult&)> onApplyCallback)
    : dbManager(db), onApply(std::move(onApplyCallback))
{
    titleLabel.setFont(juce::Font(16.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setFont(juce::Font(12.0f));
    subtitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(subtitleLabel);

    promptEditor.setMultiLine(true);
    promptEditor.setReturnKeyStartsNewLine(false);
    promptEditor.setText("Lo-fi boom bap drum kit with warm tape saturation and 25% reverb");
    promptEditor.setColour(juce::TextEditor::backgroundColourId, OpenWavLookAndFeel::bgDark);
    promptEditor.setColour(juce::TextEditor::textColourId, OpenWavLookAndFeel::textPrimary);
    promptEditor.setColour(juce::TextEditor::outlineColourId, OpenWavLookAndFeel::borderColour);
    addAndMakeVisible(promptEditor);

    auto setupPreset = [this](juce::TextButton& btn, const juce::String& promptText) {
        btn.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
        btn.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textPrimary);
        btn.onClick = [this, promptText] { promptEditor.setText(promptText); };
        addAndMakeVisible(btn);
    };
    setupPreset(presetBtn1, "Lo-fi boom bap drum kit with warm tape saturation and 25% reverb");
    setupPreset(presetBtn2, "Punchy 808 trap drum kit with heavy saturation and tight compression");
    setupPreset(presetBtn3, "Lush ambient pad instrument with slow attack, chorus, and hall reverb");
    setupPreset(presetBtn4, "Tight house drum kit with bright hats, delay, and subtle reverb");

    apiKeyLabel.setFont(juce::Font(11.5f).boldened());
    apiKeyLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(apiKeyLabel);

    apiKeyEditor.setPasswordCharacter('*');
    apiKeyEditor.setTextToShowWhenEmpty("Leave blank for instant local semantic AI", OpenWavLookAndFeel::textSecondary.withAlpha(0.6f));
    apiKeyEditor.setColour(juce::TextEditor::backgroundColourId, OpenWavLookAndFeel::bgDark);
    apiKeyEditor.setColour(juce::TextEditor::textColourId, OpenWavLookAndFeel::textPrimary);
    apiKeyEditor.setColour(juce::TextEditor::outlineColourId, OpenWavLookAndFeel::borderColour);
    addAndMakeVisible(apiKeyEditor);

    statusLabel.setFont(juce::Font(12.0f).boldened());
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(statusLabel);

    buildButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.25f));
    buildButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);
    buildButton.onClick = [this] { runBuild(); };
    addAndMakeVisible(buildButton);

    cancelButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    cancelButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textSecondary);
    cancelButton.onClick = [this] {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(cancelButton);
}

void AiKitBuilderDialog::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);
    OpenWavLookAndFeel::drawPanelCard(g, getLocalBounds().reduced(10).toFloat(), 8.0f, false);
}

void AiKitBuilderDialog::resized()
{
    auto area = getLocalBounds().reduced(22);
    titleLabel.setBounds(area.removeFromTop(24));
    subtitleLabel.setBounds(area.removeFromTop(32));
    area.removeFromTop(8);

    promptEditor.setBounds(area.removeFromTop(68));
    area.removeFromTop(8);

    auto chipRow = area.removeFromTop(26);
    int chipW = (chipRow.getWidth() - 18) / 4;
    presetBtn1.setBounds(chipRow.removeFromLeft(chipW));
    chipRow.removeFromLeft(6);
    presetBtn2.setBounds(chipRow.removeFromLeft(chipW));
    chipRow.removeFromLeft(6);
    presetBtn3.setBounds(chipRow.removeFromLeft(chipW));
    chipRow.removeFromLeft(6);
    presetBtn4.setBounds(chipRow);

    area.removeFromTop(10);
    auto keyRow = area.removeFromTop(26);
    apiKeyLabel.setBounds(keyRow.removeFromLeft(165));
    apiKeyEditor.setBounds(keyRow);

    area.removeFromTop(8);
    statusLabel.setBounds(area.removeFromTop(22));

    auto btnRow = area.removeFromBottom(32);
    buildButton.setBounds(btnRow.removeFromRight(160));
    btnRow.removeFromRight(8);
    cancelButton.setBounds(btnRow.removeFromRight(90));
}

void AiKitBuilderDialog::runBuild()
{
    juce::String prompt = promptEditor.getText().trim();
    if (prompt.isEmpty())
    {
        statusLabel.setText("Please enter a prompt describing your kit or instrument.", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::favoriteRed);
        return;
    }

    auto result = AiKitBuilder::buildFromPrompt(prompt, dbManager, apiKeyEditor.getText());
    statusLabel.setText(result.summary, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, result.success ? OpenWavLookAndFeel::accentCyan : OpenWavLookAndFeel::favoriteRed);

    if (result.success && onApply)
    {
        onApply(result);
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(1);
    }
}

} // namespace openwav
