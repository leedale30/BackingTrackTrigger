#pragma once

#include <atomic>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/**
 * Immutable, reference-counted container for a loaded sample.
 *
 * The message thread builds a fresh SampleBuffer and hands it to the audio
 * thread by swapping a pointer under a spin lock. Because the object is
 * reference counted, the audio thread can keep a SampleBuffer alive for the
 * duration of a processBlock() call even if the message thread swaps in a new
 * one mid-render. Old buffers are reclaimed on the message thread, so the
 * audio thread never allocates or frees memory.
 *
 *  - `source` holds the original audio at its native sample rate. It is the
 *    canonical copy: we resample from it (never from already-resampled data)
 *    and embed it when saving a portable project.
 *  - `audio` holds the playback-ready copy, resampled to the host rate.
 */
class SampleBuffer : public juce::ReferenceCountedObject {
public:
  using Ptr = juce::ReferenceCountedObjectPtr<SampleBuffer>;

  juce::AudioBuffer<float> source; // original, at sourceSampleRate
  juce::AudioBuffer<float> audio;  // playback-ready, at playbackSampleRate

  double sourceSampleRate = 44100.0;
  int sourceNumChannels = 2;
  int sourceBitsPerSample = 16;
  double playbackSampleRate = 44100.0;
  bool wasResampled = false;

  juce::String name;     // display name (file name)
  juce::String fullPath; // original full path (may be empty for embedded)
};

//==============================================================================
/**
 * BackingTrackTrigger - a one-shot sampler for playing backing tracks.
 *
 * Load an audio file and play it when a MIDI note arrives. Designed to stay
 * locked to a MuseScore (or any DAW) transport, with sample-accurate
 * triggering, click-free fades, looping, gain, and portable (embeddable)
 * project state.
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
  // Sample management (call from the message thread only).
  void loadSample(const juce::File &file);
  void clearSample();

  // Manual transport (thread-safe; takes effect at the next block).
  void triggerPlayback();
  void stopPlayback();

  //==============================================================================
  // Thread-safe queries for the editor.
  SampleBuffer::Ptr getSample() const;
  bool hasSampleLoaded() const;
  juce::String getSampleName() const;
  double getSampleLengthSeconds() const;
  bool isPlaying() const { return playingFlag.load(); }
  float getPlaybackProgress() const; // 0..1 within the loaded buffer
  float getOutputLevel() const { return outputLevel.load(); }

  double getOriginalSampleRate() const;
  int getOriginalNumChannels() const;
  int getOriginalBitsPerSample() const;
  double getHostSampleRate() const { return currentSampleRate.load(); }
  bool isResampled() const;

  // Start-offset helpers (wrap the "startOffset" parameter, stored in ms).
  void setStartOffsetSeconds(double seconds);
  double getStartOffsetSeconds() const;
  void setStartOffsetFromProgress(float progress0to1);

  // Embed-in-project toggle (not an automatable parameter).
  bool isEmbedEnabled() const { return embedSample.load(); }
  void setEmbedEnabled(bool shouldEmbed) { embedSample = shouldEmbed; }

  juce::AudioProcessorValueTreeState apvts;

  // Notifies the editor (if any) that the loaded sample changed.
  std::function<void()> onSampleChanged;

private:
  //==============================================================================
  enum class PlayState { Idle, Playing, FadingOut };

  static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

  void renderSegment(juce::AudioBuffer<float> &out, int startSample,
                     int numSamples, const SampleBuffer &data, int64_t offset,
                     bool looping, int fadeOutSamples);

  void startVoice(int64_t offset, int sampleLen);
  void beginFadeOut();

  // Build / resample / publish helpers (message thread).
  static void resampleInto(const juce::AudioBuffer<float> &src, double srcRate,
                           juce::AudioBuffer<float> &dst, double dstRate);
  static void prepareForRate(SampleBuffer &s, double hostRate);
  SampleBuffer::Ptr createSampleFromReader(juce::AudioFormatReader &reader,
                                           const juce::String &name,
                                           const juce::String &path) const;
  void publishSample(SampleBuffer::Ptr newSample);
  void freeUnusedSamples();
  int64_t currentOffsetSamples(double sr, int sampleLen) const;
  void setParamValue(const juce::String &id, float value);

  // Embedding (FLAC, original sample rate).
  juce::MemoryBlock encodeSampleToFlac(const SampleBuffer &s) const;
  SampleBuffer::Ptr decodeSampleFromFlac(const void *data, size_t size,
                                         const juce::String &name) const;

  //==============================================================================
  juce::AudioFormatManager formatManager;

  // RT-safe current-sample handoff.
  juce::SpinLock sampleLock;
  SampleBuffer::Ptr currentSample;                      // guarded by sampleLock
  juce::ReferenceCountedArray<SampleBuffer> samplePool; // message thread owns

  // Cached raw parameter pointers (lock-free reads on the audio thread).
  std::atomic<float> *gainParam = nullptr;
  std::atomic<float> *loopParam = nullptr;
  std::atomic<float> *noteOffStopsParam = nullptr;
  std::atomic<float> *triggerNoteParam = nullptr;
  std::atomic<float> *startOffsetParam = nullptr;
  std::atomic<float> *fadeInParam = nullptr;
  std::atomic<float> *fadeOutParam = nullptr;
  std::atomic<float> *retriggerParam = nullptr;
  std::atomic<float> *followTransportParam = nullptr;

  // Audio-thread playback state.
  PlayState playState = PlayState::Idle;
  int64_t playPos = 0;
  float fadeGain = 0.0f;   // current fade multiplier 0..1
  float fadeTarget = 0.0f; // 0 or 1
  float fadeInInc = 1.0f;  // per-sample, recomputed each block
  float fadeOutInc = 1.0f;
  juce::SmoothedValue<float> gainSmoothed;

  // Published to the editor.
  std::atomic<bool> playingFlag{false};
  std::atomic<int64_t> publishedPos{0};
  std::atomic<float> outputLevel{0.0f};

  // Message -> audio transport requests.
  std::atomic<bool> triggerRequest{false};
  std::atomic<bool> stopRequest{false};

  // Host transport tracking.
  std::atomic<double> currentSampleRate{44100.0};
  int64_t lastHostPosition = 0;
  bool wasHostPlaying = false;

  std::atomic<bool> embedSample{false};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackingTrackTriggerProcessor)
};
