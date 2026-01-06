#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BackingTrackTriggerProcessor::BackingTrackTriggerProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)) {
  // Register audio formats we can load (WAV, AIFF, etc.)
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
  currentSampleRate = sampleRate;
}

void BackingTrackTriggerProcessor::releaseResources() {}

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

  // Clear output buffer
  buffer.clear();

  // Process MIDI messages
  for (const auto metadata : midiMessages) {
    auto message = metadata.getMessage();

    if (message.isNoteOn()) {
      // Any note-on triggers playback from the beginning
      startPlayback();
    } else if (message.isNoteOff()) {
      // Any note-off stops playback
      stopPlayback();
    }
  }

  // If not playing or no sample loaded, output silence
  if (!playing || sampleBuffer.getNumSamples() == 0)
    return;

  // Get current playback position
  int64_t currentPos = playbackPosition.load();
  int numSamplesToProcess = buffer.getNumSamples();
  int sampleLength = sampleBuffer.getNumSamples();

  // Check if we've reached the end of the sample
  if (currentPos >= sampleLength) {
    playing = false;
    playbackPosition = 0;
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
    playbackPosition = 0;
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
  // Save the loaded sample path
  juce::MemoryOutputStream stream(destData, true);
  stream.writeString(loadedSampleName);
}

void BackingTrackTriggerProcessor::setStateInformation(const void *data,
                                                       int sizeInBytes) {
  // Restore sample from saved state
  juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
  juce::String samplePath = stream.readString();

  if (samplePath.isNotEmpty()) {
    juce::File file(samplePath);
    if (file.existsAsFile())
      loadSample(file);
  }
}

//==============================================================================
void BackingTrackTriggerProcessor::loadSample(const juce::File &file) {
  // Stop any current playback
  stopPlayback();

  // Try to load the audio file
  std::unique_ptr<juce::AudioFormatReader> reader(
      formatManager.createReaderFor(file));

  if (reader != nullptr) {
    // Allocate buffer for the entire sample
    sampleBuffer.setSize(static_cast<int>(reader->numChannels),
                         static_cast<int>(reader->lengthInSamples));

    // Read the entire file into our buffer
    reader->read(&sampleBuffer, 0, static_cast<int>(reader->lengthInSamples), 0,
                 true, true);

    // Store sample info
    loadedSampleName = file.getFullPathName();
    loadedSampleRate = reader->sampleRate;

    DBG("Loaded sample: " + file.getFileName() + " (" +
        juce::String(reader->lengthInSamples) + " samples, " +
        juce::String(reader->numChannels) + " channels, " +
        juce::String(reader->sampleRate) + " Hz)");
  } else {
    // Clear buffer if load failed
    sampleBuffer.setSize(0, 0);
    loadedSampleName = "";
    DBG("Failed to load sample: " + file.getFullPathName());
  }
}

void BackingTrackTriggerProcessor::startPlayback() {
  if (sampleBuffer.getNumSamples() > 0) {
    playbackPosition = 0;
    playing = true;
  }
}

void BackingTrackTriggerProcessor::stopPlayback() {
  playing = false;
  playbackPosition = 0;
}

double BackingTrackTriggerProcessor::getSampleLengthSeconds() const {
  if (sampleBuffer.getNumSamples() == 0)
    return 0.0;

  return static_cast<double>(sampleBuffer.getNumSamples()) / loadedSampleRate;
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
