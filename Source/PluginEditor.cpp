#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
BackingTrackTriggerEditor::BackingTrackTriggerEditor(
    BackingTrackTriggerProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p), waveformDisplay(p) {
  // Set up the Load button
  loadButton.setColour(juce::TextButton::buttonColourId,
                       juce::Colour(0xff2d5a27));
  loadButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
  loadButton.onClick = [this] { loadButtonClicked(); };
  addAndMakeVisible(loadButton);

  // Set up the Play button
  playButton.setColour(juce::TextButton::buttonColourId,
                       juce::Colour(0xff1e88e5));
  playButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
  playButton.onClick = [this] { processorRef.startPlayback(); };
  addAndMakeVisible(playButton);

  // Set up the Stop button
  stopButton.setColour(juce::TextButton::buttonColourId,
                       juce::Colour(0xffe53935));
  stopButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
  stopButton.onClick = [this] { processorRef.stopPlayback(); };
  addAndMakeVisible(stopButton);

  // Sample name label
  sampleNameLabel.setFont(juce::Font(16.0f, juce::Font::bold));
  sampleNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
  sampleNameLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(sampleNameLabel);

  // Duration label
  durationLabel.setFont(juce::Font(14.0f));
  durationLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
  durationLabel.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(durationLabel);

  // Instruction label
  instructionLabel.setText("Any MIDI note triggers playback. Note-off stops.",
                           juce::dontSendNotification);
  instructionLabel.setFont(juce::Font(12.0f, juce::Font::italic));
  instructionLabel.setColour(juce::Label::textColourId,
                             juce::Colour(0xff888888));
  instructionLabel.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(instructionLabel);

  // Waveform display
  addAndMakeVisible(waveformDisplay);

  // Update display with current sample info
  updateSampleInfo();

  // Set the editor size
  setSize(500, 300);
}

BackingTrackTriggerEditor::~BackingTrackTriggerEditor() {}

//==============================================================================
void BackingTrackTriggerEditor::paint(juce::Graphics &g) {
  // Dark gradient background
  juce::ColourGradient gradient(juce::Colour(0xff0f0f23), 0, 0,
                                juce::Colour(0xff1a1a3e), 0,
                                static_cast<float>(getHeight()), false);
  g.setGradientFill(gradient);
  g.fillAll();

  // Title
  g.setColour(juce::Colours::white);
  g.setFont(juce::Font(22.0f, juce::Font::bold));
  g.drawText("Backing Track Trigger", 20, 10, getWidth() - 40, 30,
             juce::Justification::centred);

  // Subtitle
  g.setColour(juce::Colour(0xff00d9ff));
  g.setFont(juce::Font(12.0f));
  g.drawText("One-Shot Sample Player for MuseScore", 20, 35, getWidth() - 40,
             20, juce::Justification::centred);
}

void BackingTrackTriggerEditor::resized() {
  auto area = getLocalBounds().reduced(20);

  // Reserve top space for title
  area.removeFromTop(60);

  // Sample info row
  auto infoRow = area.removeFromTop(25);
  sampleNameLabel.setBounds(infoRow.removeFromLeft(infoRow.getWidth() / 2));
  durationLabel.setBounds(infoRow);

  area.removeFromTop(10);

  // Waveform display
  waveformDisplay.setBounds(area.removeFromTop(120));

  area.removeFromTop(15);

  // Button row
  auto buttonRow = area.removeFromTop(40);
  int buttonWidth = (buttonRow.getWidth() - 20) / 3;

  loadButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
  buttonRow.removeFromLeft(10);
  playButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
  buttonRow.removeFromLeft(10);
  stopButton.setBounds(buttonRow);

  area.removeFromTop(10);

  // Instruction label
  instructionLabel.setBounds(area.removeFromTop(20));
}

//==============================================================================
void BackingTrackTriggerEditor::loadButtonClicked() {
  fileChooser = std::make_unique<juce::FileChooser>(
      "Select a WAV file to load...", juce::File{},
      "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg");

  auto fileChooserFlags = juce::FileBrowserComponent::openMode |
                          juce::FileBrowserComponent::canSelectFiles;

  fileChooser->launchAsync(fileChooserFlags,
                           [this](const juce::FileChooser &fc) {
                             auto file = fc.getResult();

                             if (file.existsAsFile()) {
                               processorRef.loadSample(file);
                               updateSampleInfo();
                               waveformDisplay.repaint();
                             }
                           });
}

void BackingTrackTriggerEditor::updateSampleInfo() {
  if (processorRef.hasSampleLoaded()) {
    // Extract just the filename from the full path
    juce::File file(processorRef.getSampleName());
    sampleNameLabel.setText(file.getFileName(), juce::dontSendNotification);

    // Format duration as MM:SS
    double seconds = processorRef.getSampleLengthSeconds();
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    durationLabel.setText(juce::String::formatted("%d:%02d", mins, secs),
                          juce::dontSendNotification);
  } else {
    sampleNameLabel.setText("No sample loaded", juce::dontSendNotification);
    durationLabel.setText("--:--", juce::dontSendNotification);
  }
}
