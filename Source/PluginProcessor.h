#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/**
 * BackingTrackTrigger - A one-shot sampler for playing backing tracks
 *
 * This plugin loads a WAV file and plays it when any MIDI note is received.
 * - Any MIDI Note-On: Starts playback from start offset position
 * - Note-Off is IGNORED - sample plays to completion
 * - No pitch shifting: all notes play at original sample pitch
 * - Automatic resampling to match host sample rate
 * - Syncs with host transport (resets when host stops/rewinds)
 * - Adjustable start offset to skip silence at beginning
 */
class BackingTrackTriggerProcessor : public juce::AudioProcessor {
public:
  BackingTrackTriggerProcessor();
  ~BackingTrackTriggerProcessor() override;

  //==============================================================================
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  //==============================================================================
  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  //==============================================================================
  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  //==============================================================================
  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  //==============================================================================
  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  //==============================================================================
  // Sample loading and playback control
  void loadSample(const juce::File &file);
  void startPlayback();
  void stopPlayback();
  void resetPlayback(); // Reset position to beginning without stopping

  bool hasSampleLoaded() const { return sampleBuffer.getNumSamples() > 0; }
  juce::String getSampleName() const { return loadedSampleName; }
  double getSampleLengthSeconds() const;
  bool isPlaying() const { return playing; }
  float getPlaybackProgress() const;

  // Get sample buffer for waveform display
  const juce::AudioBuffer<float> &getSampleBuffer() const {
    return sampleBuffer;
  }

  // File info getters for UI display
  double getOriginalSampleRate() const { return originalSampleRate; }
  int getOriginalNumChannels() const { return originalNumChannels; }
  int getOriginalBitsPerSample() const { return originalBitsPerSample; }
  double getHostSampleRate() const { return currentSampleRate; }
  bool isResampled() const { return wasResampled; }

  // Start offset control (in seconds)
  void setStartOffsetSeconds(double offsetSeconds);
  double getStartOffsetSeconds() const;
  void setStartOffsetFromProgress(
      float progress); // Set offset from waveform click position

private:
  //==============================================================================
  void resampleBufferHighQuality(double sourceSampleRate,
                                 double targetSampleRate);

  juce::AudioFormatManager formatManager;
  juce::AudioBuffer<float> sampleBuffer;
  juce::String loadedSampleName;

  // Original file info (before resampling)
  double originalSampleRate = 44100.0;
  int originalNumChannels = 2;
  int originalBitsPerSample = 16;
  bool wasResampled = false;

  // Playback state
  std::atomic<bool> playing{false};
  std::atomic<bool> triggered{false}; // True only when triggered by MIDI note
  std::atomic<int64_t> playbackPosition{0};

  // Start offset (in samples, after resampling)
  std::atomic<int64_t> startOffsetSamples{0};

  // Host transport tracking
  int64_t lastHostPosition = 0;
  bool wasHostPlaying = false;

  // Current sample rate from host
  double currentSampleRate = 44100.0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackingTrackTriggerProcessor)
};
