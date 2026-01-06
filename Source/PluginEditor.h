#pragma once

#include "PluginProcessor.h"

//==============================================================================
/**
 * Waveform display component showing the loaded sample
 * Click on the waveform to set the start offset position
 * Supports zoom for more precise control
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

    // Draw waveform with zoom
    auto waveformBounds = bounds.reduced(10.0f, 15.0f);
    float midY = waveformBounds.getCentreY();
    float height = waveformBounds.getHeight() / 2.0f;

    int numSamples = buffer.getNumSamples();

    // Calculate visible range based on zoom
    int visibleSamples = static_cast<int>(numSamples / zoomLevel);
    int startSampleView =
        static_cast<int>(viewOffset * (numSamples - visibleSamples));
    int endSampleView = std::min(startSampleView + visibleSamples, numSamples);

    float samplesPerPixel =
        static_cast<float>(visibleSamples) / waveformBounds.getWidth();

    // Create gradient for waveform
    juce::ColourGradient gradient(
        juce::Colour(0xff00d9ff), waveformBounds.getX(), midY,
        juce::Colour(0xff0099ff), waveformBounds.getRight(), midY, false);
    g.setGradientFill(gradient);

    juce::Path waveformPath;
    waveformPath.startNewSubPath(waveformBounds.getX(), midY);

    for (float x = 0; x < waveformBounds.getWidth(); x += 1.0f) {
      int startSample = startSampleView + static_cast<int>(x * samplesPerPixel);
      int endSample = std::min(
          startSample + static_cast<int>(samplesPerPixel) + 1, numSamples);

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
      int startSample = startSampleView + static_cast<int>(x * samplesPerPixel);
      int endSample = std::min(
          startSample + static_cast<int>(samplesPerPixel) + 1, numSamples);

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
    double offsetSeconds = processor.getStartOffsetSeconds();
    double offsetSamples = offsetSeconds * processor.getHostSampleRate();

    // Check if offset is in visible range
    if (offsetSamples >= startSampleView && offsetSamples < endSampleView) {
      float offsetProgress =
          static_cast<float>(offsetSamples - startSampleView) / visibleSamples;
      float offsetX =
          waveformBounds.getX() + offsetProgress * waveformBounds.getWidth();

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
      float playbackSample = processor.getPlaybackProgress() * numSamples;
      if (playbackSample >= startSampleView && playbackSample < endSampleView) {
        float progress = (playbackSample - startSampleView) / visibleSamples;
        float lineX =
            waveformBounds.getX() + progress * waveformBounds.getWidth();
        g.setColour(juce::Colours::white);
        g.drawLine(lineX, waveformBounds.getY(), lineX,
                   waveformBounds.getBottom(), 2.0f);
      }
    }

    // Draw zoom info at bottom
    g.setColour(juce::Colour(0xff666666));
    g.setFont(10.0f);
    juce::String zoomText =
        (zoomLevel > 1.01f) ? juce::String::formatted(
                                  "Zoom: %.0fx | Click to set start", zoomLevel)
                            : "Click to set start position";
    g.drawText(zoomText, bounds.removeFromBottom(15),
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
      float clickProgress = (clickX - bounds.getX()) / bounds.getWidth();
      clickProgress = std::max(0.0f, std::min(1.0f, clickProgress));

      // Convert click to actual sample position accounting for zoom
      int numSamples = processor.getSampleBuffer().getNumSamples();
      int visibleSamples = static_cast<int>(numSamples / zoomLevel);
      int startSampleView =
          static_cast<int>(viewOffset * (numSamples - visibleSamples));

      int clickedSample =
          startSampleView + static_cast<int>(clickProgress * visibleSamples);
      float actualProgress = static_cast<float>(clickedSample) / numSamples;

      processor.setStartOffsetFromProgress(actualProgress);
      repaint();

      if (onOffsetChanged)
        onOffsetChanged();
    }
  }

  void setZoom(float newZoom) {
    zoomLevel = std::max(1.0f, std::min(100.0f, newZoom));
    repaint();
  }

  float getZoom() const { return zoomLevel; }

  void setViewOffset(float offset) {
    viewOffset = std::max(0.0f, std::min(1.0f, offset));
    repaint();
  }

  float getViewOffset() const { return viewOffset; }

  // Scroll wheel to adjust view position when zoomed
  void mouseWheelMove(const juce::MouseEvent &,
                      const juce::MouseWheelDetails &wheel) override {
    if (zoomLevel > 1.01f) {
      viewOffset =
          std::max(0.0f, std::min(1.0f, viewOffset - wheel.deltaY * 0.1f));
      repaint();
    }
  }

  std::function<void()> onOffsetChanged;

private:
  BackingTrackTriggerProcessor &processor;
  float zoomLevel = 1.0f;
  float viewOffset = 0.0f; // 0 = start, 1 = end
};

//==============================================================================
/**
 * Plugin editor with file browser, zoomable waveform, and millisecond input
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
  juce::TextButton resetOffsetButton{"Reset"};
  juce::TextButton zoomInButton{"+"};
  juce::TextButton zoomOutButton{"-"};
  juce::Label sampleNameLabel;
  juce::Label durationLabel;
  juce::Label fileInfoLabel;
  juce::Label hostInfoLabel;
  juce::Label offsetDisplayLabel;
  juce::Label offsetInputLabel;
  juce::TextEditor offsetInput; // Text input for milliseconds
  juce::Label instructionLabel;
  WaveformDisplay waveformDisplay;

  std::unique_ptr<juce::FileChooser> fileChooser;

  void loadButtonClicked();
  void updateSampleInfo();
  void applyOffsetFromInput();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackingTrackTriggerEditor)
};
