#pragma once

#include "PluginProcessor.h"

//==============================================================================
/**
 * Waveform display component showing the loaded sample
 * Click on the waveform to set the start offset position
 */
class WaveformDisplay : public juce::Component, public juce::Timer {
public:
  WaveformDisplay(BackingTrackTriggerProcessor &p) : processor(p) {
    startTimerHz(30); // Update 30 times per second for playback position
  }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour(juce::Colour(0xff1a1a2e));
    g.fillRoundedRectangle(bounds, 8.0f);

    // Border
    g.setColour(juce::Colour(0xff4a4a6a));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 8.0f, 2.0f);

    const auto &buffer = processor.getSampleBuffer();

    if (buffer.getNumSamples() == 0) {
      // No sample loaded - show placeholder
      g.setColour(juce::Colours::grey);
      g.setFont(16.0f);
      g.drawText("No sample loaded - Click 'Load Sample' to begin", bounds,
                 juce::Justification::centred);
      return;
    }

    // Draw waveform
    auto waveformBounds = bounds.reduced(10.0f, 15.0f);
    float midY = waveformBounds.getCentreY();
    float height = waveformBounds.getHeight() / 2.0f;

    // Create gradient for waveform
    juce::ColourGradient gradient(
        juce::Colour(0xff00d9ff), waveformBounds.getX(), midY,
        juce::Colour(0xff0099ff), waveformBounds.getRight(), midY, false);
    g.setGradientFill(gradient);

    juce::Path waveformPath;
    int numSamples = buffer.getNumSamples();
    float samplesPerPixel =
        static_cast<float>(numSamples) / waveformBounds.getWidth();

    // Draw approximate waveform (using max values for each pixel column)
    waveformPath.startNewSubPath(waveformBounds.getX(), midY);

    for (float x = 0; x < waveformBounds.getWidth(); x += 1.0f) {
      int startSample = static_cast<int>(x * samplesPerPixel);
      int endSample =
          std::min(static_cast<int>((x + 1) * samplesPerPixel), numSamples);

      float maxVal = 0.0f;
      for (int s = startSample; s < endSample; ++s) {
        float val = std::abs(buffer.getSample(0, s));
        maxVal = std::max(maxVal, val);
      }

      float y = midY - maxVal * height;
      waveformPath.lineTo(waveformBounds.getX() + x, y);
    }

    // Mirror for bottom half
    for (float x = waveformBounds.getWidth() - 1; x >= 0; x -= 1.0f) {
      int startSample = static_cast<int>(x * samplesPerPixel);
      int endSample =
          std::min(static_cast<int>((x + 1) * samplesPerPixel), numSamples);

      float maxVal = 0.0f;
      for (int s = startSample; s < endSample; ++s) {
        float val = std::abs(buffer.getSample(0, s));
        maxVal = std::max(maxVal, val);
      }

      float y = midY + maxVal * height;
      waveformPath.lineTo(waveformBounds.getX() + x, y);
    }

    waveformPath.closeSubPath();
    g.fillPath(waveformPath);

    // Draw start offset marker (green line)
    float startOffset = static_cast<float>(processor.getStartOffsetSeconds() *
                                           processor.getHostSampleRate()) /
                        numSamples;
    if (startOffset > 0.0f && startOffset < 1.0f) {
      float offsetX =
          waveformBounds.getX() + startOffset * waveformBounds.getWidth();
      g.setColour(juce::Colour(0xff00ff00)); // Bright green
      g.drawLine(offsetX, waveformBounds.getY(), offsetX,
                 waveformBounds.getBottom(), 3.0f);

      // Draw small triangle at top
      juce::Path triangle;
      triangle.addTriangle(offsetX - 6, waveformBounds.getY(), offsetX + 6,
                           waveformBounds.getY(), offsetX,
                           waveformBounds.getY() + 10);
      g.fillPath(triangle);
    }

    // Draw playback position line (white)
    if (processor.isPlaying()) {
      float progress = processor.getPlaybackProgress();
      float lineX =
          waveformBounds.getX() + progress * waveformBounds.getWidth();

      g.setColour(juce::Colours::white);
      g.drawLine(lineX, waveformBounds.getY(), lineX,
                 waveformBounds.getBottom(), 2.0f);
    }

    // Draw instruction text at bottom
    g.setColour(juce::Colour(0xff666666));
    g.setFont(10.0f);
    g.drawText("Click to set start position", bounds.removeFromBottom(15),
               juce::Justification::centred);
  }

  void timerCallback() override {
    if (processor.isPlaying())
      repaint();
  }

  void mouseDown(const juce::MouseEvent &event) override {
    if (processor.hasSampleLoaded()) {
      auto bounds = getLocalBounds().toFloat().reduced(10.0f, 15.0f);
      float clickX = static_cast<float>(event.x);
      float progress = (clickX - bounds.getX()) / bounds.getWidth();
      progress = std::max(0.0f, std::min(1.0f, progress));

      processor.setStartOffsetFromProgress(progress);
      repaint();

      // Notify parent to update offset display
      if (onOffsetChanged)
        onOffsetChanged();
    }
  }

  std::function<void()> onOffsetChanged;

private:
  BackingTrackTriggerProcessor &processor;
};

//==============================================================================
/**
 * Plugin editor with file browser, large waveform display, and start offset
 * control
 */
class BackingTrackTriggerEditor : public juce::AudioProcessorEditor {
public:
  BackingTrackTriggerEditor(BackingTrackTriggerProcessor &);
  ~BackingTrackTriggerEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  BackingTrackTriggerProcessor &processorRef;

  // UI Components
  juce::TextButton loadButton{"Load Sample"};
  juce::TextButton playButton{"Play"};
  juce::TextButton stopButton{"Stop"};
  juce::TextButton resetOffsetButton{"Reset Start"};
  juce::Label sampleNameLabel;
  juce::Label durationLabel;
  juce::Label fileInfoLabel; // Shows sample rate, channels, bit depth
  juce::Label hostInfoLabel; // Shows host sample rate & resampling status
  juce::Label offsetLabel;   // Shows current start offset
  juce::Label instructionLabel;
  WaveformDisplay waveformDisplay;

  std::unique_ptr<juce::FileChooser> fileChooser;

  void loadButtonClicked();
  void updateSampleInfo();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackingTrackTriggerEditor)
};
