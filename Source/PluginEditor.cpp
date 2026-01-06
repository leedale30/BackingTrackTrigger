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

  // Set up the Reset Offset button
  resetOffsetButton.setColour(juce::TextButton::buttonColourId,
                              juce::Colour(0xff6a5acd));
  resetOffsetButton.setColour(juce::TextButton::textColourOnId,
                              juce::Colours::white);
  resetOffsetButton.onClick = [this] {
    processorRef.setStartOffsetSeconds(0.0);
    updateSampleInfo();
    waveformDisplay.repaint();
  };
  addAndMakeVisible(resetOffsetButton);

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

  // File info label (sample rate, channels, bit depth)
  fileInfoLabel.setFont(juce::Font(11.0f));
  fileInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00d9ff));
  fileInfoLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(fileInfoLabel);

  // Host info label (host sample rate, resampling status)
  hostInfoLabel.setFont(juce::Font(11.0f));
  hostInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff88ff88));
  hostInfoLabel.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(hostInfoLabel);

  // Offset label
  offsetLabel.setFont(juce::Font(12.0f, juce::Font::bold));
  offsetLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00ff00));
  offsetLabel.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(offsetLabel);

  // Instruction label
  instructionLabel.setText(
      "MIDI Note-On triggers playback. Sample plays to completion.",
      juce::dontSendNotification);
  instructionLabel.setFont(juce::Font(11.0f, juce::Font::italic));
  instructionLabel.setColour(juce::Label::textColourId,
                             juce::Colour(0xff888888));
  instructionLabel.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(instructionLabel);

  // Waveform display - set callback for offset changes
  waveformDisplay.onOffsetChanged = [this] { updateSampleInfo(); };
  addAndMakeVisible(waveformDisplay);

  // Update display with current sample info
  updateSampleInfo();

  // Set the editor size - LARGER to accommodate bigger waveform
  setSize(600, 420);
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
  g.setFont(juce::Font(24.0f, juce::Font::bold));
  g.drawText("Backing Track Trigger", 20, 10, getWidth() - 40, 35,
             juce::Justification::centred);

  // Subtitle
  g.setColour(juce::Colour(0xff00d9ff));
  g.setFont(juce::Font(12.0f));
  g.drawText("One-Shot Sample Player for MuseScore", 20, 40, getWidth() - 40,
             20, juce::Justification::centred);
}

void BackingTrackTriggerEditor::resized() {
  auto area = getLocalBounds().reduced(20);

  // Reserve top space for title
  area.removeFromTop(65);

  // Sample info row (filename and duration)
  auto infoRow = area.removeFromTop(25);
  sampleNameLabel.setBounds(infoRow.removeFromLeft(infoRow.getWidth() * 2 / 3));
  durationLabel.setBounds(infoRow);

  area.removeFromTop(5);

  // File info row (sample rate, channels, bit depth on left; host info on
  // right)
  auto fileInfoRow = area.removeFromTop(18);
  fileInfoLabel.setBounds(
      fileInfoRow.removeFromLeft(fileInfoRow.getWidth() / 2));
  hostInfoLabel.setBounds(fileInfoRow);

  area.removeFromTop(8);

  // Waveform display - LARGER
  waveformDisplay.setBounds(area.removeFromTop(180));

  area.removeFromTop(8);

  // Offset display
  offsetLabel.setBounds(area.removeFromTop(20));

  area.removeFromTop(10);

  // Button row - 4 buttons now
  auto buttonRow = area.removeFromTop(40);
  int buttonWidth = (buttonRow.getWidth() - 30) / 4;

  loadButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
  buttonRow.removeFromLeft(10);
  playButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
  buttonRow.removeFromLeft(10);
  stopButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
  buttonRow.removeFromLeft(10);
  resetOffsetButton.setBounds(buttonRow);

  area.removeFromTop(10);

  // Instruction label
  instructionLabel.setBounds(area.removeFromTop(20));
}

//==============================================================================
void BackingTrackTriggerEditor::loadButtonClicked() {
  fileChooser = std::make_unique<juce::FileChooser>(
      "Select an audio file to load...", juce::File{},
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

    // File info: original sample rate, channels, bit depth
    juce::String channelStr =
        (processorRef.getOriginalNumChannels() == 1) ? "Mono" : "Stereo";
    juce::String fileInfo = juce::String::formatted(
        "File: %.0f Hz | %s | %d-bit", processorRef.getOriginalSampleRate(),
        channelStr.toRawUTF8(), processorRef.getOriginalBitsPerSample());
    fileInfoLabel.setText(fileInfo, juce::dontSendNotification);

    // Host info: host sample rate and resampling status
    juce::String hostInfo = juce::String::formatted(
        "Host: %.0f Hz", processorRef.getHostSampleRate());
    if (processorRef.isResampled()) {
      hostInfo += " (Resampled)";
      hostInfoLabel.setColour(juce::Label::textColourId,
                              juce::Colour(0xffffaa00)); // Orange if resampled
    } else {
      hostInfoLabel.setColour(juce::Label::textColourId,
                              juce::Colour(0xff88ff88)); // Green if matched
    }
    hostInfoLabel.setText(hostInfo, juce::dontSendNotification);

    // Start offset display
    double offsetSec = processorRef.getStartOffsetSeconds();
    if (offsetSec > 0.001) {
      int offsetMs = static_cast<int>(offsetSec * 1000);
      if (offsetSec >= 1.0) {
        int offsetMins = static_cast<int>(offsetSec) / 60;
        int offsetSecs = static_cast<int>(offsetSec) % 60;
        offsetLabel.setText(
            juce::String::formatted("Start Offset: %d:%02d.%03d", offsetMins,
                                    offsetSecs, offsetMs % 1000),
            juce::dontSendNotification);
      } else {
        offsetLabel.setText(
            juce::String::formatted("Start Offset: %d ms", offsetMs),
            juce::dontSendNotification);
      }
    } else {
      offsetLabel.setText("Start Offset: 0 (click waveform to set)",
                          juce::dontSendNotification);
    }

  } else {
    sampleNameLabel.setText("No sample loaded", juce::dontSendNotification);
    durationLabel.setText("--:--", juce::dontSendNotification);
    fileInfoLabel.setText("", juce::dontSendNotification);
    offsetLabel.setText("", juce::dontSendNotification);

    // Still show host sample rate even without a sample
    juce::String hostInfo = juce::String::formatted(
        "Host: %.0f Hz", processorRef.getHostSampleRate());
    hostInfoLabel.setColour(juce::Label::textColourId,
                            juce::Colour(0xff88ff88));
    hostInfoLabel.setText(hostInfo, juce::dontSendNotification);
  }
}
