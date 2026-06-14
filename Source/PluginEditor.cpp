#include "PluginEditor.h"

namespace {
constexpr float kCornerRadius = 8.0f;
const juce::Colour kAccent{0xff00d9ff};
const juce::Colour kOffsetGreen{0xff00ff66};
} // namespace

//==============================================================================
// WaveformDisplay
//==============================================================================
void WaveformDisplay::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();

  g.setColour(juce::Colour(0xff1a1a2e));
  g.fillRoundedRectangle(bounds, kCornerRadius);
  g.setColour(fileBeingDragged ? kAccent : juce::Colour(0xff4a4a6a));
  g.drawRoundedRectangle(bounds.reduced(1.0f), kCornerRadius,
                         fileBeingDragged ? 3.0f : 2.0f);

  auto sample = processor.getSample();
  if (sample == nullptr || sample->audio.getNumSamples() == 0) {
    g.setColour(juce::Colours::grey);
    g.setFont(16.0f);
    g.drawText(fileBeingDragged ? "Drop audio file to load"
                                : "No sample - click Load or drop an audio file",
               bounds, juce::Justification::centred);
    return;
  }

  const auto &buffer = sample->audio;
  const int numSamples = buffer.getNumSamples();

  auto waveformBounds = bounds.reduced(10.0f, 15.0f);
  const float midY = waveformBounds.getCentreY();
  const float height = waveformBounds.getHeight() / 2.0f;

  const int visibleSamples =
      juce::jmax(1, static_cast<int>(static_cast<float>(numSamples) / zoomLevel));
  const int startSampleView = static_cast<int>(
      viewOffset * static_cast<float>(numSamples - visibleSamples));
  const int endSampleView =
      juce::jmin(startSampleView + visibleSamples, numSamples);
  const float samplesPerPixel =
      static_cast<float>(visibleSamples) / waveformBounds.getWidth();

  juce::ColourGradient gradient(kAccent, waveformBounds.getX(), midY,
                                juce::Colour(0xff0099ff),
                                waveformBounds.getRight(), midY, false);
  g.setGradientFill(gradient);

  juce::Path waveformPath;
  waveformPath.startNewSubPath(waveformBounds.getX(), midY);

  auto peakAt = [&](float x) {
    int startSample = startSampleView + static_cast<int>(x * samplesPerPixel);
    int endSample = juce::jmin(
        startSample + static_cast<int>(samplesPerPixel) + 1, numSamples);
    float maxVal = 0.0f;
    for (int s = startSample; s < endSample; ++s)
      maxVal = juce::jmax(maxVal, std::abs(buffer.getSample(0, s)));
    return maxVal;
  };

  for (float x = 0; x < waveformBounds.getWidth(); x += 1.0f)
    waveformPath.lineTo(waveformBounds.getX() + x, midY - peakAt(x) * height);
  for (float x = waveformBounds.getWidth() - 1; x >= 0; x -= 1.0f)
    waveformPath.lineTo(waveformBounds.getX() + x, midY + peakAt(x) * height);

  waveformPath.closeSubPath();
  g.fillPath(waveformPath);

  // Start-offset marker.
  const double offsetSamples =
      processor.getStartOffsetSeconds() * sample->playbackSampleRate;
  if (offsetSamples >= startSampleView && offsetSamples < endSampleView) {
    const float p = static_cast<float>(offsetSamples - startSampleView) /
                    static_cast<float>(visibleSamples);
    const float ox = waveformBounds.getX() + p * waveformBounds.getWidth();
    g.setColour(kOffsetGreen);
    g.drawLine(ox, waveformBounds.getY(), ox, waveformBounds.getBottom(), 3.0f);
    juce::Path tri;
    tri.addTriangle(ox - 6, waveformBounds.getY(), ox + 6,
                    waveformBounds.getY(), ox, waveformBounds.getY() + 10);
    g.fillPath(tri);
  }

  // Playback head.
  if (processor.isPlaying()) {
    const float playbackSample =
        processor.getPlaybackProgress() * static_cast<float>(numSamples);
    if (playbackSample >= static_cast<float>(startSampleView) &&
        playbackSample < static_cast<float>(endSampleView)) {
      const float p = (playbackSample - static_cast<float>(startSampleView)) /
                      static_cast<float>(visibleSamples);
      const float lx = waveformBounds.getX() + p * waveformBounds.getWidth();
      g.setColour(juce::Colours::white);
      g.drawLine(lx, waveformBounds.getY(), lx, waveformBounds.getBottom(),
                 2.0f);
    }
  }

  g.setColour(juce::Colour(0xff666666));
  g.setFont(10.0f);
  g.drawText(zoomLevel > 1.01f
                 ? juce::String::formatted("Zoom %.0fx - click to set start",
                                           zoomLevel)
                 : juce::String("Click waveform to set start position"),
             bounds.removeFromBottom(15), juce::Justification::centred);
}

void WaveformDisplay::timerCallback() {
  if (processor.isPlaying())
    repaint();
}

void WaveformDisplay::mouseDown(const juce::MouseEvent &event) {
  auto sample = processor.getSample();
  if (sample == nullptr || sample->audio.getNumSamples() == 0)
    return;

  auto wb = getLocalBounds().toFloat().reduced(10.0f, 15.0f);
  float clickProgress = (static_cast<float>(event.x) - wb.getX()) / wb.getWidth();
  clickProgress = juce::jlimit(0.0f, 1.0f, clickProgress);

  const int numSamples = sample->audio.getNumSamples();
  const int visibleSamples =
      juce::jmax(1, static_cast<int>(static_cast<float>(numSamples) / zoomLevel));
  const int startSampleView = static_cast<int>(
      viewOffset * static_cast<float>(numSamples - visibleSamples));
  const int clickedSample =
      startSampleView +
      static_cast<int>(clickProgress * static_cast<float>(visibleSamples));

  processor.setStartOffsetFromProgress(static_cast<float>(clickedSample) /
                                       static_cast<float>(numSamples));
  repaint();
  if (onOffsetChanged)
    onOffsetChanged();
}

void WaveformDisplay::mouseWheelMove(const juce::MouseEvent &,
                                     const juce::MouseWheelDetails &wheel) {
  if (zoomLevel > 1.01f) {
    viewOffset = juce::jlimit(0.0f, 1.0f, viewOffset - wheel.deltaY * 0.1f);
    repaint();
  }
}

bool WaveformDisplay::isInterestedInFileDrag(const juce::StringArray &files) {
  for (const auto &f : files)
    if (f.matchesWildcard("*.wav", true) || f.matchesWildcard("*.aif*", true) ||
        f.matchesWildcard("*.mp3", true) || f.matchesWildcard("*.flac", true) ||
        f.matchesWildcard("*.ogg", true))
      return true;
  return false;
}

void WaveformDisplay::fileDragEnter(const juce::StringArray &, int, int) {
  fileBeingDragged = true;
  repaint();
}

void WaveformDisplay::fileDragExit(const juce::StringArray &) {
  fileBeingDragged = false;
  repaint();
}

void WaveformDisplay::filesDropped(const juce::StringArray &files, int, int) {
  fileBeingDragged = false;
  repaint();
  for (const auto &f : files) {
    juce::File file(f);
    if (file.existsAsFile() && onFileDropped) {
      onFileDropped(file);
      break;
    }
  }
}

void WaveformDisplay::setZoom(float newZoom) {
  zoomLevel = juce::jlimit(1.0f, 200.0f, newZoom);
  repaint();
}

void WaveformDisplay::setViewOffset(float offset) {
  viewOffset = juce::jlimit(0.0f, 1.0f, offset);
  repaint();
}

//==============================================================================
// LevelMeter
//==============================================================================
void LevelMeter::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();
  g.setColour(juce::Colour(0xff14141f));
  g.fillRoundedRectangle(bounds, 3.0f);

  const float clamped = juce::jlimit(0.0f, 1.0f, level);
  if (clamped > 0.001f) {
    auto fill = bounds.reduced(2.0f);
    fill = fill.removeFromBottom(fill.getHeight() * clamped);
    juce::ColourGradient grad(juce::Colour(0xff00ff66), 0, bounds.getBottom(),
                              juce::Colour(0xffff3030), 0, bounds.getY(), false);
    grad.addColour(0.7, juce::Colour(0xffffcc00));
    g.setGradientFill(grad);
    g.fillRoundedRectangle(fill, 2.0f);
  }
}

void LevelMeter::timerCallback() {
  const float target = processor.getOutputLevel();
  level = target > level ? target : level * 0.8f; // fast attack, slow release
  repaint();
}

//==============================================================================
// Editor
//==============================================================================
void BackingTrackTriggerEditor::styleButton(juce::TextButton &b,
                                            juce::Colour colour) {
  b.setColour(juce::TextButton::buttonColourId, colour);
  b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
  b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
}

BackingTrackTriggerEditor::BackingTrackTriggerEditor(
    BackingTrackTriggerProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p), waveformDisplay(p),
      levelMeter(p) {
  auto &state = processorRef.apvts;

  // Transport / file buttons.
  styleButton(loadButton, juce::Colour(0xff2d5a27));
  loadButton.onClick = [this] { loadButtonClicked(); };
  loadButton.setTooltip("Load an audio file (WAV/AIFF/MP3/FLAC/OGG)");
  addAndMakeVisible(loadButton);

  styleButton(clearButton, juce::Colour(0xff555555));
  clearButton.onClick = [this] {
    processorRef.clearSample();
    updateSampleInfo();
    waveformDisplay.repaint();
  };
  clearButton.setTooltip("Unload the current sample");
  addAndMakeVisible(clearButton);

  styleButton(playButton, juce::Colour(0xff1e88e5));
  playButton.onClick = [this] { processorRef.triggerPlayback(); };
  playButton.setTooltip("Audition the sample from the start offset");
  addAndMakeVisible(playButton);

  styleButton(stopButton, juce::Colour(0xffe53935));
  stopButton.onClick = [this] { processorRef.stopPlayback(); };
  addAndMakeVisible(stopButton);

  // Zoom + reset.
  styleButton(zoomInButton, juce::Colour(0xff444466));
  zoomInButton.onClick = [this] {
    waveformDisplay.setZoom(waveformDisplay.getZoom() * 2.0f);
  };
  addAndMakeVisible(zoomInButton);

  styleButton(zoomOutButton, juce::Colour(0xff444466));
  zoomOutButton.onClick = [this] {
    waveformDisplay.setZoom(waveformDisplay.getZoom() / 2.0f);
  };
  addAndMakeVisible(zoomOutButton);

  styleButton(resetOffsetButton, juce::Colour(0xff6a5acd));
  resetOffsetButton.onClick = [this] {
    processorRef.setStartOffsetSeconds(0.0);
    waveformDisplay.setZoom(1.0f);
    waveformDisplay.setViewOffset(0.0f);
    updateSampleInfo();
    waveformDisplay.repaint();
  };
  resetOffsetButton.setTooltip("Reset start offset and zoom");
  addAndMakeVisible(resetOffsetButton);

  // Waveform + meter.
  waveformDisplay.onOffsetChanged = [this] { updateSampleInfo(); };
  waveformDisplay.onFileDropped = [this](const juce::File &f) { doLoadFile(f); };
  addAndMakeVisible(waveformDisplay);
  addAndMakeVisible(levelMeter);

  // Gain slider.
  gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  gainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 20);
  gainSlider.setTextValueSuffix(" dB");
  gainSlider.setColour(juce::Slider::trackColourId, kAccent);
  gainSlider.setTooltip("Output level");
  addAndMakeVisible(gainSlider);
  gainAttach =
      std::make_unique<APVTS::SliderAttachment>(state, "gain", gainSlider);

  gainLabel.setText("Gain", juce::dontSendNotification);
  gainLabel.setColour(juce::Label::textColourId, juce::Colours::white);
  addAndMakeVisible(gainLabel);

  // Trigger-note selector.
  triggerNoteSlider.setSliderStyle(juce::Slider::IncDecButtons);
  triggerNoteSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 22);
  triggerNoteSlider.textFromValueFunction = [](double v) {
    const int iv = static_cast<int>(v);
    return iv >= 128 ? juce::String("Any")
                     : juce::MidiMessage::getMidiNoteName(iv, true, true, 4);
  };
  triggerNoteSlider.valueFromTextFunction = [](const juce::String &t) {
    if (t.trim().equalsIgnoreCase("any"))
      return 128.0;
    return static_cast<double>(t.getIntValue());
  };
  triggerNoteSlider.setTooltip("Which MIDI note triggers playback (Any = all)");
  addAndMakeVisible(triggerNoteSlider);
  triggerNoteAttach = std::make_unique<APVTS::SliderAttachment>(
      state, "triggerNote", triggerNoteSlider);

  triggerNoteLabel.setText("Trigger note", juce::dontSendNotification);
  triggerNoteLabel.setColour(juce::Label::textColourId, juce::Colours::white);
  addAndMakeVisible(triggerNoteLabel);

  // Toggle buttons.
  auto setupToggle = [this](juce::ToggleButton &b, const juce::String &tip) {
    b.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    b.setColour(juce::ToggleButton::tickColourId, kAccent);
    b.setTooltip(tip);
    addAndMakeVisible(b);
  };
  setupToggle(loopButton, "Loop the sample until note-off / stop");
  setupToggle(retriggerButton, "A new note restarts playback from the offset");
  setupToggle(noteOffButton, "Releasing the key fades the sample out");
  setupToggle(followButton,
              "Stop/rewind playback when the host transport stops");
  embedButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
  embedButton.setColour(juce::ToggleButton::tickColourId, kOffsetGreen);
  embedButton.setTooltip(
      "Save the audio inside the project so it travels with the score");
  embedButton.setToggleState(processorRef.isEmbedEnabled(),
                             juce::dontSendNotification);
  embedButton.onClick = [this] {
    processorRef.setEmbedEnabled(embedButton.getToggleState());
  };
  addAndMakeVisible(embedButton);

  loopAttach = std::make_unique<APVTS::ButtonAttachment>(state, "loop",
                                                         loopButton);
  retriggerAttach = std::make_unique<APVTS::ButtonAttachment>(
      state, "retrigger", retriggerButton);
  noteOffAttach = std::make_unique<APVTS::ButtonAttachment>(
      state, "noteOffStops", noteOffButton);
  followAttach = std::make_unique<APVTS::ButtonAttachment>(
      state, "followTransport", followButton);

  // Labels.
  sampleNameLabel.setFont(juce::Font(juce::FontOptions(16.0f).withStyle("Bold")));
  sampleNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
  addAndMakeVisible(sampleNameLabel);

  durationLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
  durationLabel.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(durationLabel);

  fileInfoLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
  fileInfoLabel.setColour(juce::Label::textColourId, kAccent);
  addAndMakeVisible(fileInfoLabel);

  hostInfoLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
  hostInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff88ff88));
  hostInfoLabel.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(hostInfoLabel);

  offsetDisplayLabel.setFont(
      juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
  offsetDisplayLabel.setColour(juce::Label::textColourId, kOffsetGreen);
  addAndMakeVisible(offsetDisplayLabel);

  offsetInputLabel.setText("Set ms:", juce::dontSendNotification);
  offsetInputLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
  offsetInputLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
  offsetInputLabel.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(offsetInputLabel);

  offsetInput.setJustification(juce::Justification::centred);
  offsetInput.setColour(juce::TextEditor::backgroundColourId,
                        juce::Colour(0xff2a2a4a));
  offsetInput.setColour(juce::TextEditor::textColourId, juce::Colours::white);
  offsetInput.setColour(juce::TextEditor::outlineColourId, kOffsetGreen);
  offsetInput.setInputRestrictions(8, "0123456789");
  offsetInput.setTextToShowWhenEmpty("0", juce::Colour(0xff666666));
  offsetInput.onReturnKey = [this] { applyOffsetFromInput(); };
  offsetInput.onFocusLost = [this] { applyOffsetFromInput(); };
  addAndMakeVisible(offsetInput);

  instructionLabel.setText(
      "MIDI note triggers playback - drop audio onto the waveform, "
      "+/- to zoom, scroll to pan.",
      juce::dontSendNotification);
  instructionLabel.setFont(
      juce::Font(juce::FontOptions(10.0f).withStyle("Italic")));
  instructionLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
  instructionLabel.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(instructionLabel);

  // Keep the UI in sync when state is restored or a sample is (re)loaded.
  processorRef.onSampleChanged = [this] {
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<
                                         BackingTrackTriggerEditor>(this)] {
      if (safe != nullptr) {
        safe->updateSampleInfo();
        safe->waveformDisplay.repaint();
      }
    });
  };

  updateSampleInfo();
  setSize(700, 560);
}

BackingTrackTriggerEditor::~BackingTrackTriggerEditor() {
  processorRef.onSampleChanged = nullptr;
}

//==============================================================================
void BackingTrackTriggerEditor::paint(juce::Graphics &g) {
  juce::ColourGradient gradient(juce::Colour(0xff0f0f23), 0, 0,
                                juce::Colour(0xff1a1a3e), 0,
                                static_cast<float>(getHeight()), false);
  g.setGradientFill(gradient);
  g.fillAll();

  g.setColour(juce::Colours::white);
  g.setFont(juce::Font(juce::FontOptions(24.0f).withStyle("Bold")));
  g.drawText("Backing Track Trigger", 20, 10, getWidth() - 40, 32,
             juce::Justification::centred);

  g.setColour(kAccent);
  g.setFont(12.0f);
  g.drawText("One-Shot Sample Player  |  v2.0.0", 20, 40, getWidth() - 40, 18,
             juce::Justification::centred);
}

void BackingTrackTriggerEditor::resized() {
  auto area = getLocalBounds().reduced(20);
  area.removeFromTop(48); // title

  auto infoRow = area.removeFromTop(24);
  sampleNameLabel.setBounds(infoRow.removeFromLeft(infoRow.getWidth() * 2 / 3));
  durationLabel.setBounds(infoRow);

  area.removeFromTop(2);
  auto fileInfoRow = area.removeFromTop(18);
  fileInfoLabel.setBounds(fileInfoRow.removeFromLeft(fileInfoRow.getWidth() / 2));
  hostInfoLabel.setBounds(fileInfoRow);

  area.removeFromTop(6);
  auto waveformRow = area.removeFromTop(180);
  levelMeter.setBounds(waveformRow.removeFromRight(14));
  waveformRow.removeFromRight(6);
  auto sideCol = waveformRow.removeFromRight(40);
  zoomInButton.setBounds(sideCol.removeFromTop(40));
  sideCol.removeFromTop(4);
  zoomOutButton.setBounds(sideCol.removeFromTop(40));
  sideCol.removeFromTop(4);
  resetOffsetButton.setBounds(sideCol.removeFromTop(30));
  waveformRow.removeFromRight(6);
  waveformDisplay.setBounds(waveformRow);

  area.removeFromTop(6);
  auto offsetRow = area.removeFromTop(26);
  offsetDisplayLabel.setBounds(offsetRow.removeFromLeft(offsetRow.getWidth() / 2));
  offsetInput.setBounds(offsetRow.removeFromRight(70));
  offsetInputLabel.setBounds(offsetRow);

  area.removeFromTop(8);
  auto transportRow = area.removeFromTop(36);
  const int bw = (transportRow.getWidth() - 30) / 4;
  loadButton.setBounds(transportRow.removeFromLeft(bw));
  transportRow.removeFromLeft(10);
  clearButton.setBounds(transportRow.removeFromLeft(bw));
  transportRow.removeFromLeft(10);
  playButton.setBounds(transportRow.removeFromLeft(bw));
  transportRow.removeFromLeft(10);
  stopButton.setBounds(transportRow);

  area.removeFromTop(10);
  auto gainRow = area.removeFromTop(28);
  gainLabel.setBounds(gainRow.removeFromLeft(50));
  gainSlider.setBounds(gainRow);

  area.removeFromTop(6);
  auto noteRow = area.removeFromTop(26);
  triggerNoteLabel.setBounds(noteRow.removeFromLeft(90));
  triggerNoteSlider.setBounds(noteRow.removeFromLeft(150));
  noteRow.removeFromLeft(15);
  embedButton.setBounds(noteRow);

  area.removeFromTop(8);
  auto toggleRow = area.removeFromTop(26);
  const int tw = toggleRow.getWidth() / 4;
  loopButton.setBounds(toggleRow.removeFromLeft(tw));
  retriggerButton.setBounds(toggleRow.removeFromLeft(tw));
  noteOffButton.setBounds(toggleRow.removeFromLeft(tw));
  followButton.setBounds(toggleRow);

  area.removeFromTop(8);
  instructionLabel.setBounds(area.removeFromTop(20));
}

//==============================================================================
void BackingTrackTriggerEditor::loadButtonClicked() {
  fileChooser = std::make_unique<juce::FileChooser>(
      "Select an audio file to load...", juce::File{},
      "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg");

  const auto flags = juce::FileBrowserComponent::openMode |
                     juce::FileBrowserComponent::canSelectFiles;

  fileChooser->launchAsync(flags, [this](const juce::FileChooser &fc) {
    auto file = fc.getResult();
    if (file.existsAsFile())
      doLoadFile(file);
  });
}

void BackingTrackTriggerEditor::doLoadFile(const juce::File &file) {
  processorRef.loadSample(file);
  waveformDisplay.setZoom(1.0f);
  waveformDisplay.setViewOffset(0.0f);
  updateSampleInfo();
  waveformDisplay.repaint();
}

void BackingTrackTriggerEditor::applyOffsetFromInput() {
  auto text = offsetInput.getText();
  if (text.isNotEmpty()) {
    const int ms = text.getIntValue();
    if (ms >= 0) {
      processorRef.setStartOffsetSeconds(ms / 1000.0);
      updateSampleInfo();
      waveformDisplay.repaint();
    }
  }
}

void BackingTrackTriggerEditor::updateSampleInfo() {
  const bool loaded = processorRef.hasSampleLoaded();

  if (loaded) {
    sampleNameLabel.setText(processorRef.getSampleName(),
                            juce::dontSendNotification);

    const double seconds = processorRef.getSampleLengthSeconds();
    durationLabel.setText(
        juce::String::formatted("%d:%02d", static_cast<int>(seconds) / 60,
                                static_cast<int>(seconds) % 60),
        juce::dontSendNotification);

    const juce::String channels =
        processorRef.getOriginalNumChannels() == 1 ? "Mono" : "Stereo";
    fileInfoLabel.setText(
        juce::String::formatted("File: %.0f Hz | %s | %d-bit",
                                processorRef.getOriginalSampleRate(),
                                channels.toRawUTF8(),
                                processorRef.getOriginalBitsPerSample()),
        juce::dontSendNotification);

    juce::String host = juce::String::formatted(
        "Host: %.0f Hz", processorRef.getHostSampleRate());
    if (processorRef.isResampled()) {
      host += " (resampled)";
      hostInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffaa00));
    } else {
      hostInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff88ff88));
    }
    hostInfoLabel.setText(host, juce::dontSendNotification);

    const double offsetSec = processorRef.getStartOffsetSeconds();
    const int offsetMs = static_cast<int>(offsetSec * 1000);
    if (offsetSec >= 1.0)
      offsetDisplayLabel.setText(
          juce::String::formatted("Start: %d:%02d.%03d (%d ms)",
                                  static_cast<int>(offsetSec) / 60,
                                  static_cast<int>(offsetSec) % 60,
                                  offsetMs % 1000, offsetMs),
          juce::dontSendNotification);
    else
      offsetDisplayLabel.setText(
          juce::String::formatted("Start offset: %d ms", offsetMs),
          juce::dontSendNotification);

    offsetInput.setText(juce::String(offsetMs), false);
  } else {
    sampleNameLabel.setText("No sample loaded", juce::dontSendNotification);
    durationLabel.setText("--:--", juce::dontSendNotification);
    fileInfoLabel.setText("", juce::dontSendNotification);
    offsetDisplayLabel.setText("", juce::dontSendNotification);
    offsetInput.setText("", false);
    hostInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff88ff88));
    hostInfoLabel.setText(juce::String::formatted(
                              "Host: %.0f Hz", processorRef.getHostSampleRate()),
                          juce::dontSendNotification);
  }
}
