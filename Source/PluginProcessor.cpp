#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
BackingTrackTriggerProcessor::BackingTrackTriggerProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)) {
  // Register audio formats we can load (WAV, AIFF, MP3, etc.)
  formatManager.registerBasicFormats();
}

BackingTrackTriggerProcessor::~BackingTrackTriggerProcessor() {}

//==============================================================================
const juce::String BackingTrackTriggerProcessor::getName() const {
  return JucePlugin_Name;
}

bool BackingTrackTriggerProcessor::acceptsMidi() const { return true; }

bool BackingTrackTriggerProcessor::producesMidi() const { return false; }

bool BackingTrackTriggerProcessor::isMidiEffect() const { return false; }

double BackingTrackTriggerProcessor::getTailLengthSeconds() const {
  return 0.0;
}

int BackingTrackTriggerProcessor::getNumPrograms() { return 1; }

int BackingTrackTriggerProcessor::getCurrentProgram() { return 0; }

void BackingTrackTriggerProcessor::setCurrentProgram(int index) {
  juce::ignoreUnused(index);
}

const juce::String BackingTrackTriggerProcessor::getProgramName(int index) {
  juce::ignoreUnused(index);
  return {};
}

void BackingTrackTriggerProcessor::changeProgramName(
    int index, const juce::String &newName) {
  juce::ignoreUnused(index, newName);
}

//==============================================================================
void BackingTrackTriggerProcessor::prepareToPlay(double sampleRate,
                                                 int samplesPerBlock) {
  juce::ignoreUnused(samplesPerBlock);

  double oldSampleRate = currentSampleRate;
  currentSampleRate = sampleRate;

  // Reset playback state when preparing
  playing = false;
  triggered = false;
  playbackPosition = 0;
  lastHostPosition = 0;
  wasHostPlaying = false;

  // If the host sample rate changed and we have a sample loaded,
  // we need to reload/resample it
  if (oldSampleRate != currentSampleRate && sampleBuffer.getNumSamples() > 0 &&
      oldSampleRate > 0) {
    // Reload the sample to resample it correctly
    if (!loadedSampleName.isEmpty()) {
      juce::File file(loadedSampleName);
      if (file.existsAsFile()) {
        loadSample(file);
      }
    }
  }
}

void BackingTrackTriggerProcessor::releaseResources() {
  // Reset playback state
  playing = false;
  triggered = false;
  playbackPosition = 0;
}

bool BackingTrackTriggerProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  // Support mono or stereo output
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

  return true;
}

void BackingTrackTriggerProcessor::processBlock(
    juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;

  // Clear output buffer first
  buffer.clear();

  // Get host playhead information to detect transport changes
  if (auto *playHead = getPlayHead()) {
    if (auto position = playHead->getPosition()) {
      bool isHostPlaying = position->getIsPlaying();

      // Get the current position in samples if available
      if (auto hostTimeInSamples = position->getTimeInSamples()) {
        int64_t currentHostPos = *hostTimeInSamples;

        // Detect if host has rewound or jumped back
        if (currentHostPos < lastHostPosition - 1000) {
          // Host rewound - reset our playback
          resetPlayback();
        }

        lastHostPosition = currentHostPos;
      }

      // Detect if host stopped playing
      if (wasHostPlaying && !isHostPlaying) {
        // Host stopped - reset our playback position
        resetPlayback();
      }

      wasHostPlaying = isHostPlaying;
    }
  }

  // Process MIDI messages - ONLY trigger on note-on events
  // NOTE-OFF IS IGNORED - sample plays to completion!
  for (const auto metadata : midiMessages) {
    auto message = metadata.getMessage();

    if (message.isNoteOn() && message.getVelocity() > 0) {
      // Only actual note-on with velocity > 0 triggers playback
      startPlayback();
      triggered = true;
    }
    // Note-off is intentionally IGNORED - sample plays until it finishes
  }

  // Only play if we were explicitly triggered by a MIDI note AND we're playing
  if (!playing || !triggered || sampleBuffer.getNumSamples() == 0)
    return;

  // Get current playback position
  int64_t currentPos = playbackPosition.load();
  int numSamplesToProcess = buffer.getNumSamples();
  int sampleLength = sampleBuffer.getNumSamples();

  // Check if we've reached the end of the sample
  if (currentPos >= sampleLength) {
    playing = false;
    triggered = false;
    playbackPosition = startOffsetSamples.load(); // Reset to start offset
    return;
  }

  // Calculate how many samples we can copy
  int samplesToEnd = static_cast<int>(sampleLength - currentPos);
  int samplesToCopy = std::min(numSamplesToProcess, samplesToEnd);

  // Copy sample data to output
  int numOutputChannels = buffer.getNumChannels();
  int numSampleChannels = sampleBuffer.getNumChannels();

  for (int channel = 0; channel < numOutputChannels; ++channel) {
    // If sample is mono and output is stereo, duplicate the channel
    int sourceChannel = std::min(channel, numSampleChannels - 1);

    buffer.copyFrom(channel, 0, sampleBuffer, sourceChannel,
                    static_cast<int>(currentPos), samplesToCopy);
  }

  // Update playback position
  playbackPosition = currentPos + samplesToCopy;

  // Check if we've reached the end
  if (playbackPosition >= sampleLength) {
    playing = false;
    triggered = false;
    playbackPosition = startOffsetSamples.load(); // Reset to start offset
  }
}

//==============================================================================
bool BackingTrackTriggerProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *BackingTrackTriggerProcessor::createEditor() {
  return new BackingTrackTriggerEditor(*this);
}

//==============================================================================
void BackingTrackTriggerProcessor::getStateInformation(
    juce::MemoryBlock &destData) {
  // Save the loaded sample path and start offset
  juce::MemoryOutputStream stream(destData, true);
  stream.writeString(loadedSampleName);
  stream.writeDouble(getStartOffsetSeconds());
}

void BackingTrackTriggerProcessor::setStateInformation(const void *data,
                                                       int sizeInBytes) {
  // Restore sample from saved state
  juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
  juce::String samplePath = stream.readString();
  double savedOffset = 0.0;
  if (!stream.isExhausted()) {
    savedOffset = stream.readDouble();
  }

  if (samplePath.isNotEmpty()) {
    juce::File file(samplePath);
    if (file.existsAsFile()) {
      loadSample(file);
      setStartOffsetSeconds(savedOffset);
    }
  }
}

//==============================================================================
// Start offset control
void BackingTrackTriggerProcessor::setStartOffsetSeconds(double offsetSeconds) {
  if (currentSampleRate > 0) {
    int64_t offsetSamples =
        static_cast<int64_t>(offsetSeconds * currentSampleRate);
    // Clamp to valid range
    offsetSamples =
        std::max(int64_t(0),
                 std::min(offsetSamples,
                          static_cast<int64_t>(sampleBuffer.getNumSamples())));
    startOffsetSamples = offsetSamples;
  }
}

double BackingTrackTriggerProcessor::getStartOffsetSeconds() const {
  if (currentSampleRate > 0) {
    return static_cast<double>(startOffsetSamples.load()) / currentSampleRate;
  }
  return 0.0;
}

void BackingTrackTriggerProcessor::setStartOffsetFromProgress(float progress) {
  if (sampleBuffer.getNumSamples() > 0) {
    progress = std::max(0.0f, std::min(1.0f, progress));
    int64_t offsetSamples =
        static_cast<int64_t>(progress * sampleBuffer.getNumSamples());
    startOffsetSamples = offsetSamples;
  }
}

//==============================================================================
// High-quality resampling using cubic Hermite interpolation
void BackingTrackTriggerProcessor::resampleBufferHighQuality(
    double sourceSampleRate, double targetSampleRate) {
  if (sourceSampleRate == targetSampleRate || sampleBuffer.getNumSamples() == 0)
    return;

  double ratio = targetSampleRate / sourceSampleRate;
  int originalLength = sampleBuffer.getNumSamples();
  int newLength = static_cast<int>(std::round(originalLength * ratio));
  int numChannels = sampleBuffer.getNumChannels();

  // Create a new buffer for the resampled audio
  juce::AudioBuffer<float> resampledBuffer(numChannels, newLength);

  // Use cubic interpolation for better quality
  for (int channel = 0; channel < numChannels; ++channel) {
    const float *sourceData = sampleBuffer.getReadPointer(channel);
    float *destData = resampledBuffer.getWritePointer(channel);

    for (int i = 0; i < newLength; ++i) {
      double sourcePos = i / ratio;
      int idx = static_cast<int>(std::floor(sourcePos));
      double frac = sourcePos - idx;

      // Cubic Hermite interpolation for better quality
      float y0 = (idx > 0) ? sourceData[idx - 1] : sourceData[0];
      float y1 = (idx < originalLength) ? sourceData[idx]
                                        : sourceData[originalLength - 1];
      float y2 = (idx + 1 < originalLength) ? sourceData[idx + 1]
                                            : sourceData[originalLength - 1];
      float y3 = (idx + 2 < originalLength) ? sourceData[idx + 2]
                                            : sourceData[originalLength - 1];

      // Cubic interpolation formula
      double c0 = y1;
      double c1 = 0.5 * (y2 - y0);
      double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
      double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);

      destData[i] =
          static_cast<float>(((c3 * frac + c2) * frac + c1) * frac + c0);
    }
  }

  // Replace the original buffer with the resampled one
  sampleBuffer = std::move(resampledBuffer);
  wasResampled = true;

  DBG("Resampled from " + juce::String(sourceSampleRate) + " Hz to " +
      juce::String(targetSampleRate) + " Hz (" + juce::String(originalLength) +
      " -> " + juce::String(newLength) + " samples)");
}

void BackingTrackTriggerProcessor::loadSample(const juce::File &file) {
  // Stop any current playback
  stopPlayback();
  triggered = false;
  wasResampled = false;
  startOffsetSamples = 0;

  // Try to load the audio file
  std::unique_ptr<juce::AudioFormatReader> reader(
      formatManager.createReaderFor(file));

  if (reader != nullptr) {
    // Store original file info
    originalSampleRate = reader->sampleRate;
    originalNumChannels = static_cast<int>(reader->numChannels);
    originalBitsPerSample = static_cast<int>(reader->bitsPerSample);

    // Allocate buffer for the entire sample
    sampleBuffer.setSize(static_cast<int>(reader->numChannels),
                         static_cast<int>(reader->lengthInSamples));

    // Read the entire file into our buffer
    reader->read(&sampleBuffer, 0, static_cast<int>(reader->lengthInSamples), 0,
                 true, true);

    // Store sample info
    loadedSampleName = file.getFullPathName();

    DBG("Loaded sample: " + file.getFileName() + " (" +
        juce::String(reader->lengthInSamples) + " samples, " +
        juce::String(reader->numChannels) + " channels, " +
        juce::String(reader->sampleRate) + " Hz, " +
        juce::String(reader->bitsPerSample) + " bit)");

    // Resample if the file's sample rate doesn't match the host
    if (currentSampleRate > 0 &&
        std::abs(originalSampleRate - currentSampleRate) > 0.1) {
      resampleBufferHighQuality(originalSampleRate, currentSampleRate);
    }
  } else {
    // Clear buffer if load failed
    sampleBuffer.setSize(0, 0);
    loadedSampleName = "";
    originalSampleRate = 44100.0;
    originalNumChannels = 2;
    originalBitsPerSample = 16;
    DBG("Failed to load sample: " + file.getFullPathName());
  }
}

void BackingTrackTriggerProcessor::startPlayback() {
  if (sampleBuffer.getNumSamples() > 0) {
    playbackPosition = startOffsetSamples.load(); // Start from offset
    playing = true;
    triggered = true;
  }
}

void BackingTrackTriggerProcessor::stopPlayback() {
  playing = false;
  triggered = false;
  playbackPosition = startOffsetSamples.load(); // Reset to start offset
}

void BackingTrackTriggerProcessor::resetPlayback() {
  playing = false;
  triggered = false;
  playbackPosition = startOffsetSamples.load(); // Reset to start offset
}

double BackingTrackTriggerProcessor::getSampleLengthSeconds() const {
  if (sampleBuffer.getNumSamples() == 0)
    return 0.0;

  // Use current sample rate for accurate duration
  return static_cast<double>(sampleBuffer.getNumSamples()) / currentSampleRate;
}

float BackingTrackTriggerProcessor::getPlaybackProgress() const {
  if (sampleBuffer.getNumSamples() == 0)
    return 0.0f;

  return static_cast<float>(playbackPosition.load()) /
         static_cast<float>(sampleBuffer.getNumSamples());
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new BackingTrackTriggerProcessor();
}
