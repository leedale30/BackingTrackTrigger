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
    waveformDisplay.setZoom(1.0f);
    waveformDisplay.setViewOffset(0.0f);
    updateSampleInfo();
    waveformDisplay.repaint();
  };
  addAndMakeVisible(resetOffsetButton);

  // Zoom buttons
  zoomInButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xff444466));
  zoomInButton.setColour(juce::TextButton::textColourOnId,
                         juce::Colours::white);
  zoomInButton.onClick = [this] {
    waveformDisplay.setZoom(waveformDisplay.getZoom() * 2.0f);
  };
  addAndMakeVisible(zoomInButton);

  zoomOutButton.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff444466));
  zoomOutButton.setColour(juce::TextButton::textColourOnId,
                          juce::Colours::white);
  zoomOutButton.onClick = [this] {
    waveformDisplay.setZoom(waveformDisplay.getZoom() / 2.0f);
  };
  addAndMakeVisible(zoomOutButton);

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

  // File info label
  fileInfoLabel.setFont(juce::Font(11.0f));
  fileInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00d9ff));
  fileInfoLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(fileInfoLabel);

  // Host info label
  hostInfoLabel.setFont(juce::Font(11.0f));
  hostInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff88ff88));
  hostInfoLabel.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(hostInfoLabel);

  // Offset display label
  offsetDisplayLabel.setFont(juce::Font(12.0f, juce::Font::bold));
  offsetDisplayLabel.setColour(juce::Label::textColourId,
                               juce::Colour(0xff00ff00));
  offsetDisplayLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(offsetDisplayLabel);

  // Offset input label
  offsetInputLabel.setText("Set ms:", juce::dontSendNotification);
  offsetInputLabel.setFont(juce::Font(11.0f));
  offsetInputLabel.setColour(juce::Label::textColourId,
                             juce::Colour(0xffaaaaaa));
  offsetInputLabel.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(offsetInputLabel);

  // Offset text input
  offsetInput.setFont(juce::Font(12.0f));
  offsetInput.setJustification(juce::Justification::centred);
  offsetInput.setColour(juce::TextEditor::backgroundColourId,
                        juce::Colour(0xff2a2a4a));
  offsetInput.setColour(juce::TextEditor::textColourId, juce::Colours::white);
  offsetInput.setColour(juce::TextEditor::outlineColourId,
                        juce::Colour(0xff00ff00));
  offsetInput.setInputRestrictions(8, "0123456789");
  offsetInput.setTextToShowWhenEmpty("0", juce::Colour(0xff666666));
  offsetInput.onReturnKey = [this] { applyOffsetFromInput(); };
  offsetInput.onFocusLost = [this] { applyOffsetFromInput(); };
  addAndMakeVisible(offsetInput);

  // Instruction label
  instructionLabel.setText(
      "MIDI Note-On triggers playback. Use +/- to zoom, scroll wheel to pan.",
      juce::dontSendNotification);
  instructionLabel.setFont(juce::Font(10.0f, juce::Font::italic));
  instructionLabel.setColour(juce::Label::textColourId,
                             juce::Colour(0xff888888));
  instructionLabel.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(instructionLabel);

  // Waveform display
  waveformDisplay.onOffsetChanged = [this] { updateSampleInfo(); };
  addAndMakeVisible(waveformDisplay);

  // Update display
  updateSampleInfo();

  // Larger size
  setSize(650, 450);
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

  // Subtitle with version
  g.setColour(juce::Colour(0xff00d9ff));
  g.setFont(juce::Font(12.0f));
  g.drawText("One-Shot Sample Player for MuseScore  |  v1.2.0", 20, 40,
             getWidth() - 40, 20, juce::Justification::centred);
}

void BackingTrackTriggerEditor::resized() {
  auto area = getLocalBounds().reduced(20);

  // Reserve top space for title
  area.removeFromTop(65);

  // Sample info row
  auto infoRow = area.removeFromTop(25);
  sampleNameLabel.setBounds(infoRow.removeFromLeft(infoRow.getWidth() * 2 / 3));
  durationLabel.setBounds(infoRow);

  area.removeFromTop(5);

  // File info row
  auto fileInfoRow = area.removeFromTop(18);
  fileInfoLabel.setBounds(
      fileInfoRow.removeFromLeft(fileInfoRow.getWidth() / 2));
  hostInfoLabel.setBounds(fileInfoRow);

  area.removeFromTop(8);

  // Waveform display with zoom buttons on the side
  auto waveformRow = area.removeFromTop(180);
  auto zoomButtonArea = waveformRow.removeFromRight(35);
  zoomInButton.setBounds(zoomButtonArea.removeFromTop(40));
  zoomButtonArea.removeFromTop(5);
  zoomOutButton.setBounds(zoomButtonArea.removeFromTop(40));
  waveformDisplay.setBounds(waveformRow);

  area.removeFromTop(8);

  // Offset row: display on left, input on right
  auto offsetRow = area.removeFromTop(25);
  offsetDisplayLabel.setBounds(
      offsetRow.removeFromLeft(offsetRow.getWidth() / 2));
  offsetInput.setBounds(offsetRow.removeFromRight(70));
  offsetInputLabel.setBounds(offsetRow);

  area.removeFromTop(10);

  // Button row
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
                               waveformDisplay.setZoom(1.0f);
                               waveformDisplay.setViewOffset(0.0f);
                               updateSampleInfo();
                               waveformDisplay.repaint();
                             }
                           });
}

void BackingTrackTriggerEditor::applyOffsetFromInput() {
  juce::String text = offsetInput.getText();
  if (text.isNotEmpty()) {
    int ms = text.getIntValue();
    if (ms >= 0) {
      double seconds = ms / 1000.0;
      processorRef.setStartOffsetSeconds(seconds);
      updateSampleInfo();
      waveformDisplay.repaint();
    }
  }
}

void BackingTrackTriggerEditor::updateSampleInfo() {
  if (processorRef.hasSampleLoaded()) {
    juce::File file(processorRef.getSampleName());
    sampleNameLabel.setText(file.getFileName(), juce::dontSendNotification);

    double seconds = processorRef.getSampleLengthSeconds();
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    durationLabel.setText(juce::String::formatted("%d:%02d", mins, secs),
                          juce::dontSendNotification);

    juce::String channelStr =
        (processorRef.getOriginalNumChannels() == 1) ? "Mono" : "Stereo";
    juce::String fileInfo = juce::String::formatted(
        "File: %.0f Hz | %s | %d-bit", processorRef.getOriginalSampleRate(),
        channelStr.toRawUTF8(), processorRef.getOriginalBitsPerSample());
    fileInfoLabel.setText(fileInfo, juce::dontSendNotification);

    juce::String hostInfo = juce::String::formatted(
        "Host: %.0f Hz", processorRef.getHostSampleRate());
    if (processorRef.isResampled()) {
      hostInfo += " (Resampled)";
      hostInfoLabel.setColour(juce::Label::textColourId,
                              juce::Colour(0xffffaa00));
    } else {
      hostInfoLabel.setColour(juce::Label::textColourId,
                              juce::Colour(0xff88ff88));
    }
    hostInfoLabel.setText(hostInfo, juce::dontSendNotification);

    // Offset display
    double offsetSec = processorRef.getStartOffsetSeconds();
    int offsetMs = static_cast<int>(offsetSec * 1000);

    if (offsetSec >= 1.0) {
      int offsetMins = static_cast<int>(offsetSec) / 60;
      int offsetSecs = static_cast<int>(offsetSec) % 60;
      offsetDisplayLabel.setText(
          juce::String::formatted("Start: %d:%02d.%03d (%d ms)", offsetMins,
                                  offsetSecs, offsetMs % 1000, offsetMs),
          juce::dontSendNotification);
    } else if (offsetMs > 0) {
      offsetDisplayLabel.setText(
          juce::String::formatted("Start Offset: %d ms", offsetMs),
          juce::dontSendNotification);
    } else {
      offsetDisplayLabel.setText("Start Offset: 0 ms",
                                 juce::dontSendNotification);
    }

    // Update input field (without triggering callback)
    offsetInput.setText(juce::String(offsetMs), false);

  } else {
    sampleNameLabel.setText("No sample loaded", juce::dontSendNotification);
    durationLabel.setText("--:--", juce::dontSendNotification);
    fileInfoLabel.setText("", juce::dontSendNotification);
    offsetDisplayLabel.setText("", juce::dontSendNotification);
    offsetInput.setText("", false);

    juce::String hostInfo = juce::String::formatted(
        "Host: %.0f Hz", processorRef.getHostSampleRate());
    hostInfoLabel.setColour(juce::Label::textColourId,
                            juce::Colour(0xff88ff88));
    hostInfoLabel.setText(hostInfo, juce::dontSendNotification);
  }
}
