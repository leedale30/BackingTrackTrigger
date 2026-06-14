#pragma once

#include "PluginProcessor.h"

//==============================================================================
/**
 * Waveform display: draws the loaded sample, lets you click to set the start
 * offset, zoom (+/-) and pan (scroll wheel), and accepts drag-and-dropped
 * audio files.
 */
class WaveformDisplay : public juce::Component,
                        public juce::Timer,
                        public juce::FileDragAndDropTarget {
public:
  explicit WaveformDisplay(BackingTrackTriggerProcessor &p) : processor(p) {
    startTimerHz(30);
  }

  void paint(juce::Graphics &g) override;
  void timerCallback() override;
  void mouseDown(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &,
                      const juce::MouseWheelDetails &wheel) override;

  // FileDragAndDropTarget
  bool isInterestedInFileDrag(const juce::StringArray &files) override;
  void fileDragEnter(const juce::StringArray &, int, int) override;
  void fileDragExit(const juce::StringArray &) override;
  void filesDropped(const juce::StringArray &files, int, int) override;

  void setZoom(float newZoom);
  float getZoom() const { return zoomLevel; }
  void setViewOffset(float offset);
  float getViewOffset() const { return viewOffset; }

  std::function<void()> onOffsetChanged;
  std::function<void(const juce::File &)> onFileDropped;

private:
  BackingTrackTriggerProcessor &processor;
  float zoomLevel = 1.0f;
  float viewOffset = 0.0f; // 0 = start, 1 = end
  bool fileBeingDragged = false;
};

//==============================================================================
/** Simple peak level meter with smooth decay. */
class LevelMeter : public juce::Component, public juce::Timer {
public:
  explicit LevelMeter(BackingTrackTriggerProcessor &p) : processor(p) {
    startTimerHz(30);
  }
  void paint(juce::Graphics &g) override;
  void timerCallback() override;

private:
  BackingTrackTriggerProcessor &processor;
  float level = 0.0f;
};

//==============================================================================
class BackingTrackTriggerEditor : public juce::AudioProcessorEditor {
public:
  explicit BackingTrackTriggerEditor(BackingTrackTriggerProcessor &);
  ~BackingTrackTriggerEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  using APVTS = juce::AudioProcessorValueTreeState;

  void loadButtonClicked();
  void doLoadFile(const juce::File &file);
  void updateSampleInfo();
  void applyOffsetFromInput();
  void styleButton(juce::TextButton &b, juce::Colour colour);

  BackingTrackTriggerProcessor &processorRef;

  // Transport / file
  juce::TextButton loadButton{"Load"};
  juce::TextButton clearButton{"Clear"};
  juce::TextButton playButton{"Play"};
  juce::TextButton stopButton{"Stop"};

  // Waveform + zoom + meter
  WaveformDisplay waveformDisplay;
  LevelMeter levelMeter;
  juce::TextButton zoomInButton{"+"};
  juce::TextButton zoomOutButton{"-"};
  juce::TextButton resetOffsetButton{"Reset"};

  // Parameter controls
  juce::Slider gainSlider;
  juce::Slider triggerNoteSlider;
  juce::ToggleButton loopButton{"Loop"};
  juce::ToggleButton retriggerButton{"Retrigger"};
  juce::ToggleButton noteOffButton{"Note-Off Stops"};
  juce::ToggleButton followButton{"Follow Transport"};
  juce::ToggleButton embedButton{"Embed in project"};

  std::unique_ptr<APVTS::SliderAttachment> gainAttach;
  std::unique_ptr<APVTS::SliderAttachment> triggerNoteAttach;
  std::unique_ptr<APVTS::ButtonAttachment> loopAttach;
  std::unique_ptr<APVTS::ButtonAttachment> retriggerAttach;
  std::unique_ptr<APVTS::ButtonAttachment> noteOffAttach;
  std::unique_ptr<APVTS::ButtonAttachment> followAttach;

  // Labels
  juce::Label sampleNameLabel;
  juce::Label durationLabel;
  juce::Label fileInfoLabel;
  juce::Label hostInfoLabel;
  juce::Label offsetDisplayLabel;
  juce::Label offsetInputLabel;
  juce::Label gainLabel;
  juce::Label triggerNoteLabel;
  juce::TextEditor offsetInput;
  juce::Label instructionLabel;

  std::unique_ptr<juce::FileChooser> fileChooser;
  juce::TooltipWindow tooltipWindow{this};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackingTrackTriggerEditor)
};
