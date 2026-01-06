#pragma once

#include "PluginProcessor.h"

//==============================================================================
/**
 * Waveform display component showing the loaded sample
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
      g.drawText("No sample loaded", bounds, juce::Justification::centred);
      return;
    }

    // Draw waveform
    auto waveformBounds = bounds.reduced(10.0f, 20.0f);
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

    // Draw playback position line
    if (processor.isPlaying()) {
      float progress = processor.getPlaybackProgress();
      float lineX =
          waveformBounds.getX() + progress * waveformBounds.getWidth();

      g.setColour(juce::Colours::white);
      g.drawLine(lineX, waveformBounds.getY(), lineX,
                 waveformBounds.getBottom(), 2.0f);
    }
  }

  void timerCallback() override {
    if (processor.isPlaying())
      repaint();
  }

private:
  BackingTrackTriggerProcessor &processor;
};

//==============================================================================
/**
 * Plugin editor with file browser, waveform display, and file info
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
  juce::Label sampleNameLabel;
  juce::Label durationLabel;
  juce::Label fileInfoLabel; // Shows sample rate, channels, bit depth
  juce::Label hostInfoLabel; // Shows host sample rate & resampling status
  juce::Label instructionLabel;
  WaveformDisplay waveformDisplay;

  std::unique_ptr<juce::FileChooser> fileChooser;

  void loadButtonClicked();
  void updateSampleInfo();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackingTrackTriggerEditor)
};
