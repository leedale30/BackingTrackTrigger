#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace ids {
static const juce::String gain{"gain"};
static const juce::String loop{"loop"};
static const juce::String noteOffStops{"noteOffStops"};
static const juce::String triggerNote{"triggerNote"};
static const juce::String startOffset{"startOffset"};
static const juce::String fadeIn{"fadeIn"};
static const juce::String fadeOut{"fadeOut"};
static const juce::String retrigger{"retrigger"};
static const juce::String followTransport{"followTransport"};
} // namespace ids

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
BackingTrackTriggerProcessor::createLayout() {
  using namespace juce;
  AudioProcessorValueTreeState::ParameterLayout layout;

  layout.add(std::make_unique<AudioParameterFloat>(
      ParameterID{ids::gain, 1}, "Gain",
      NormalisableRange<float>(-60.0f, 12.0f, 0.1f), 0.0f,
      AudioParameterFloatAttributes().withLabel("dB")));

  layout.add(std::make_unique<AudioParameterBool>(ParameterID{ids::loop, 1},
                                                  "Loop", false));

  layout.add(std::make_unique<AudioParameterBool>(
      ParameterID{ids::noteOffStops, 1}, "Note-Off Stops", false));

  // 0..127 = trigger only on that note; 128 = trigger on any note.
  layout.add(std::make_unique<AudioParameterInt>(
      ParameterID{ids::triggerNote, 1}, "Trigger Note", 0, 128, 128,
      AudioParameterIntAttributes().withStringFromValueFunction(
          [](int v, int) {
            return v >= 128 ? String("Any")
                            : MidiMessage::getMidiNoteName(v, true, true, 4);
          })));

  layout.add(std::make_unique<AudioParameterFloat>(
      ParameterID{ids::startOffset, 1}, "Start Offset",
      NormalisableRange<float>(0.0f, 600000.0f, 1.0f), 0.0f,
      AudioParameterFloatAttributes().withLabel("ms")));

  layout.add(std::make_unique<AudioParameterFloat>(
      ParameterID{ids::fadeIn, 1}, "Fade In",
      NormalisableRange<float>(0.0f, 2000.0f, 1.0f, 0.4f), 3.0f,
      AudioParameterFloatAttributes().withLabel("ms")));

  layout.add(std::make_unique<AudioParameterFloat>(
      ParameterID{ids::fadeOut, 1}, "Fade Out",
      NormalisableRange<float>(0.0f, 2000.0f, 1.0f, 0.4f), 8.0f,
      AudioParameterFloatAttributes().withLabel("ms")));

  layout.add(std::make_unique<AudioParameterBool>(
      ParameterID{ids::retrigger, 1}, "Retrigger", true));

  layout.add(std::make_unique<AudioParameterBool>(
      ParameterID{ids::followTransport, 1}, "Follow Transport", true));

  return layout;
}

//==============================================================================
BackingTrackTriggerProcessor::BackingTrackTriggerProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createLayout()) {
  formatManager.registerBasicFormats();

  gainParam = apvts.getRawParameterValue(ids::gain);
  loopParam = apvts.getRawParameterValue(ids::loop);
  noteOffStopsParam = apvts.getRawParameterValue(ids::noteOffStops);
  triggerNoteParam = apvts.getRawParameterValue(ids::triggerNote);
  startOffsetParam = apvts.getRawParameterValue(ids::startOffset);
  fadeInParam = apvts.getRawParameterValue(ids::fadeIn);
  fadeOutParam = apvts.getRawParameterValue(ids::fadeOut);
  retriggerParam = apvts.getRawParameterValue(ids::retrigger);
  followTransportParam = apvts.getRawParameterValue(ids::followTransport);
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

  const double oldRate = currentSampleRate.load();
  currentSampleRate = sampleRate;

  gainSmoothed.reset(sampleRate, 0.02);
  gainSmoothed.setCurrentAndTargetValue(
      juce::Decibels::decibelsToGain(gainParam->load(), -60.0f));

  playState = PlayState::Idle;
  playingFlag = false;
  playPos = 0;
  fadeGain = 0.0f;
  fadeTarget = 0.0f;
  lastHostPosition = 0;
  wasHostPlaying = false;

  // If the host rate changed, rebuild the playback buffer from the pristine
  // source (never from already-resampled audio).
  if (std::abs(oldRate - sampleRate) > 0.5) {
    if (auto cur = getSample()) {
      auto rebuilt = SampleBuffer::Ptr(new SampleBuffer());
      rebuilt->source = cur->source;
      rebuilt->sourceSampleRate = cur->sourceSampleRate;
      rebuilt->sourceNumChannels = cur->sourceNumChannels;
      rebuilt->sourceBitsPerSample = cur->sourceBitsPerSample;
      rebuilt->name = cur->name;
      rebuilt->fullPath = cur->fullPath;
      prepareForRate(*rebuilt, sampleRate);
      publishSample(rebuilt);
    }
  }
}

void BackingTrackTriggerProcessor::releaseResources() {
  playState = PlayState::Idle;
  playingFlag = false;
  playPos = 0;
  fadeGain = 0.0f;
}

bool BackingTrackTriggerProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  const auto &out = layouts.getMainOutputChannelSet();
  return out == juce::AudioChannelSet::mono() ||
         out == juce::AudioChannelSet::stereo();
}

//==============================================================================
void BackingTrackTriggerProcessor::startVoice(int64_t offset, int sampleLen) {
  playPos = juce::jlimit<int64_t>(0, juce::jmax(0, sampleLen - 1), offset);
  playState = PlayState::Playing;
  fadeGain = 0.0f;
  fadeTarget = 1.0f;
  playingFlag = true;
}

void BackingTrackTriggerProcessor::beginFadeOut() {
  if (playState != PlayState::Idle) {
    playState = PlayState::FadingOut;
    fadeTarget = 0.0f;
  }
}

int64_t BackingTrackTriggerProcessor::currentOffsetSamples(double sr,
                                                           int sampleLen) const {
  const double offsetSec = startOffsetParam->load() / 1000.0;
  int64_t s = static_cast<int64_t>(offsetSec * sr);
  return juce::jlimit<int64_t>(0, juce::jmax(0, sampleLen - 1), s);
}

void BackingTrackTriggerProcessor::renderSegment(juce::AudioBuffer<float> &out,
                                                 int startSample, int numSamples,
                                                 const SampleBuffer &data,
                                                 int64_t offset, bool looping,
                                                 int fadeOutSamples) {
  if (numSamples <= 0 || playState == PlayState::Idle)
    return;

  const int sampleLen = data.audio.getNumSamples();
  const int srcCh = data.audio.getNumChannels();
  const int outCh = out.getNumChannels();

  for (int i = 0; i < numSamples; ++i) {
    if (playState == PlayState::Idle)
      break;

    // Pre-emptive fade-out so a sample that doesn't end on a zero crossing
    // doesn't click when it stops.
    if (playState == PlayState::Playing && !looping &&
        playPos >= static_cast<int64_t>(sampleLen - fadeOutSamples)) {
      playState = PlayState::FadingOut;
      fadeTarget = 0.0f;
    }

    if (fadeGain < fadeTarget)
      fadeGain = juce::jmin(fadeTarget, fadeGain + fadeInInc);
    else if (fadeGain > fadeTarget)
      fadeGain = juce::jmax(fadeTarget, fadeGain - fadeOutInc);

    const float g = fadeGain * gainSmoothed.getNextValue();
    const int p = static_cast<int>(playPos);

    for (int ch = 0; ch < outCh; ++ch) {
      const int sc = juce::jmin(ch, srcCh - 1);
      out.addSample(ch, startSample + i, data.audio.getSample(sc, p) * g);
    }

    ++playPos;

    if (playPos >= sampleLen) {
      if (looping)
        playPos = offset;
      else
        playState = PlayState::Idle;
    }

    if (playState == PlayState::FadingOut && fadeGain <= 0.0001f)
      playState = PlayState::Idle;
  }

  if (playState == PlayState::Idle)
    playingFlag = false;
}

void BackingTrackTriggerProcessor::processBlock(
    juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  buffer.clear();

  const double sr = currentSampleRate.load();
  const int numSamples = buffer.getNumSamples();

  // --- Snapshot parameters for this block ------------------------------------
  gainSmoothed.setTargetValue(
      juce::Decibels::decibelsToGain(gainParam->load(), -60.0f));
  const bool looping = loopParam->load() > 0.5f;
  const bool noteOffStops = noteOffStopsParam->load() > 0.5f;
  const int trigNote = static_cast<int>(triggerNoteParam->load());
  const bool retrig = retriggerParam->load() > 0.5f;
  const bool followTransport = followTransportParam->load() > 0.5f;

  const int fadeInSamples =
      juce::jmax(1, static_cast<int>(fadeInParam->load() * 0.001 * sr));
  const int fadeOutSamples =
      juce::jmax(1, static_cast<int>(fadeOutParam->load() * 0.001 * sr));
  fadeInInc = 1.0f / static_cast<float>(fadeInSamples);
  fadeOutInc = 1.0f / static_cast<float>(fadeOutSamples);

  // --- Acquire the current sample without blocking the audio thread ----------
  SampleBuffer::Ptr data;
  {
    const juce::SpinLock::ScopedTryLockType lock(sampleLock);
    if (lock.isLocked())
      data = currentSample;
  }

  if (data == nullptr || data->audio.getNumSamples() == 0) {
    playState = PlayState::Idle;
    playingFlag = false;
    publishedPos = 0;
    outputLevel = 0.0f;
    return;
  }

  const int sampleLen = data->audio.getNumSamples();
  const int64_t offset = currentOffsetSamples(sr, sampleLen);

  // --- Host transport: reset on stop or rewind -------------------------------
  if (followTransport) {
    if (auto *playHead = getPlayHead()) {
      if (auto position = playHead->getPosition()) {
        const bool hostPlaying = position->getIsPlaying();
        if (auto hostSamples = position->getTimeInSamples()) {
          if (*hostSamples < lastHostPosition - 1000) {
            playState = PlayState::Idle;
            playingFlag = false;
          }
          lastHostPosition = *hostSamples;
        }
        if (wasHostPlaying && !hostPlaying) {
          playState = PlayState::Idle;
          playingFlag = false;
        }
        wasHostPlaying = hostPlaying;
      }
    }
  }

  // --- Manual transport requests (block-aligned) -----------------------------
  if (stopRequest.exchange(false))
    beginFadeOut();
  if (triggerRequest.exchange(false))
    startVoice(offset, sampleLen);

  // --- Sample-accurate MIDI handling -----------------------------------------
  int cursor = 0;
  for (const auto metadata : midiMessages) {
    const auto msg = metadata.getMessage();
    const int t = juce::jlimit(0, numSamples, metadata.samplePosition);

    renderSegment(buffer, cursor, t - cursor, *data, offset, looping,
                  fadeOutSamples);
    cursor = t;

    const bool matchesNote =
        (trigNote >= 128) || (msg.getNoteNumber() == trigNote);

    if (msg.isNoteOn() && msg.getVelocity() > 0) {
      if (matchesNote && (playState == PlayState::Idle || retrig))
        startVoice(offset, sampleLen);
    } else if (msg.isNoteOff() ||
               (msg.isNoteOn() && msg.getVelocity() == 0)) {
      if (matchesNote && noteOffStops)
        beginFadeOut();
    }
  }
  renderSegment(buffer, cursor, numSamples - cursor, *data, offset, looping,
                fadeOutSamples);

  // --- Publish state for the editor ------------------------------------------
  publishedPos = playPos;
  playingFlag = (playState != PlayState::Idle);
  outputLevel = buffer.getMagnitude(0, numSamples);
}

//==============================================================================
bool BackingTrackTriggerProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *BackingTrackTriggerProcessor::createEditor() {
  return new BackingTrackTriggerEditor(*this);
}

//==============================================================================
void BackingTrackTriggerProcessor::triggerPlayback() { triggerRequest = true; }
void BackingTrackTriggerProcessor::stopPlayback() { stopRequest = true; }

//==============================================================================
void BackingTrackTriggerProcessor::resampleInto(
    const juce::AudioBuffer<float> &src, double srcRate,
    juce::AudioBuffer<float> &dst, double dstRate) {
  const int numChannels = src.getNumChannels();
  const int srcLen = src.getNumSamples();

  if (std::abs(srcRate - dstRate) < 0.5 || srcLen == 0) {
    dst = src; // straight copy
    return;
  }

  const double ratio = srcRate / dstRate; // input samples per output sample
  const int dstLen =
      static_cast<int>(std::ceil(srcLen * dstRate / srcRate));
  dst.setSize(numChannels, dstLen);

  for (int ch = 0; ch < numChannels; ++ch) {
    juce::LagrangeInterpolator interp;
    interp.reset();
    interp.process(ratio, src.getReadPointer(ch), dst.getWritePointer(ch),
                   dstLen);
  }
}

void BackingTrackTriggerProcessor::prepareForRate(SampleBuffer &s,
                                                  double hostRate) {
  s.playbackSampleRate = hostRate;
  if (std::abs(s.sourceSampleRate - hostRate) > 0.5) {
    resampleInto(s.source, s.sourceSampleRate, s.audio, hostRate);
    s.wasResampled = true;
  } else {
    s.audio = s.source;
    s.wasResampled = false;
  }
}

SampleBuffer::Ptr BackingTrackTriggerProcessor::createSampleFromReader(
    juce::AudioFormatReader &reader, const juce::String &name,
    const juce::String &path) const {
  auto s = SampleBuffer::Ptr(new SampleBuffer());
  s->name = name;
  s->fullPath = path;
  s->sourceSampleRate = reader.sampleRate;
  s->sourceNumChannels = static_cast<int>(reader.numChannels);
  s->sourceBitsPerSample = static_cast<int>(reader.bitsPerSample);

  const int len = static_cast<int>(reader.lengthInSamples);
  s->source.setSize(static_cast<int>(reader.numChannels), len);
  reader.read(&s->source, 0, len, 0, true, true);
  return s;
}

void BackingTrackTriggerProcessor::publishSample(SampleBuffer::Ptr newSample) {
  if (newSample != nullptr)
    samplePool.add(newSample);
  {
    const juce::SpinLock::ScopedLockType lock(sampleLock);
    currentSample = newSample;
  }
  freeUnusedSamples();

  if (onSampleChanged)
    onSampleChanged();
}

void BackingTrackTriggerProcessor::freeUnusedSamples() {
  for (int i = samplePool.size(); --i >= 0;) {
    SampleBuffer::Ptr held(samplePool[i]);
    // refCount 2 == only the pool entry + this local => nobody else uses it.
    if (held->getReferenceCount() == 2)
      samplePool.remove(i);
  }
}

void BackingTrackTriggerProcessor::loadSample(const juce::File &file) {
  std::unique_ptr<juce::AudioFormatReader> reader(
      formatManager.createReaderFor(file));
  if (reader == nullptr) {
    DBG("Failed to load sample: " + file.getFullPathName());
    return;
  }

  auto s = createSampleFromReader(*reader, file.getFileName(),
                                  file.getFullPathName());
  prepareForRate(*s, currentSampleRate.load());

  setParamValue(ids::startOffset, 0.0f);
  publishSample(s);
}

void BackingTrackTriggerProcessor::clearSample() { publishSample(nullptr); }

//==============================================================================
juce::MemoryBlock
BackingTrackTriggerProcessor::encodeSampleToFlac(const SampleBuffer &s) const {
  juce::MemoryBlock block;
  if (s.source.getNumSamples() == 0)
    return block;

  juce::FlacAudioFormat flac;
  auto stream = std::make_unique<juce::MemoryOutputStream>(block, false);
  const int bits = juce::jlimit(16, 24, s.sourceBitsPerSample);

  std::unique_ptr<juce::AudioFormatWriter> writer(flac.createWriterFor(
      stream.get(), s.sourceSampleRate,
      static_cast<unsigned int>(s.source.getNumChannels()), bits, {}, 5));

  if (writer != nullptr) {
    stream.release(); // writer now owns the stream
    writer->writeFromAudioSampleBuffer(s.source, 0, s.source.getNumSamples());
  }
  return block;
}

SampleBuffer::Ptr BackingTrackTriggerProcessor::decodeSampleFromFlac(
    const void *data, size_t size, const juce::String &name) const {
  juce::FlacAudioFormat flac;
  auto stream = std::make_unique<juce::MemoryInputStream>(data, size, false);
  std::unique_ptr<juce::AudioFormatReader> reader(
      flac.createReaderFor(stream.release(), true));
  if (reader == nullptr)
    return nullptr;

  return createSampleFromReader(*reader, name, {});
}

//==============================================================================
void BackingTrackTriggerProcessor::getStateInformation(
    juce::MemoryBlock &destData) {
  auto state = apvts.copyState();
  const bool embed = embedSample.load();
  state.setProperty("embedSample", embed, nullptr);

  if (auto cur = getSample()) {
    state.setProperty("samplePath", cur->fullPath, nullptr);
    state.setProperty("sampleName", cur->name, nullptr);

    if (embed) {
      auto flac = encodeSampleToFlac(*cur);
      if (flac.getSize() > 0)
        state.setProperty("sampleFlac", juce::var(flac), nullptr);
    }
  }

  juce::MemoryOutputStream stream(destData, false);
  state.writeToStream(stream);
}

void BackingTrackTriggerProcessor::setStateInformation(const void *data,
                                                       int sizeInBytes) {
  auto tree =
      juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
  if (!tree.isValid())
    return;

  embedSample = static_cast<bool>(tree.getProperty("embedSample", false));
  const juce::String path = tree.getProperty("samplePath", "").toString();
  const juce::String name = tree.getProperty("sampleName", "").toString();

  // Restore the sample first. loadSample() resets the start-offset parameter,
  // so we must apply the saved parameter values (replaceState) *afterwards*.
  bool restored = false;
  if (tree.hasProperty("sampleFlac")) {
    if (auto *mb = tree.getProperty("sampleFlac").getBinaryData()) {
      auto displayName = name.isNotEmpty() ? name : juce::String("Embedded");
      if (auto s = decodeSampleFromFlac(mb->getData(), mb->getSize(),
                                        displayName)) {
        s->fullPath = path;
        prepareForRate(*s, currentSampleRate.load());
        publishSample(s);
        restored = true;
      }
    }
  }

  if (!restored && path.isNotEmpty()) {
    juce::File file(path);
    if (file.existsAsFile())
      loadSample(file);
  }

  apvts.replaceState(tree);
}

//==============================================================================
void BackingTrackTriggerProcessor::setParamValue(const juce::String &id,
                                                  float value) {
  if (auto *p = apvts.getParameter(id)) {
    const auto range = apvts.getParameterRange(id);
    p->setValueNotifyingHost(range.convertTo0to1(value));
  }
}

void BackingTrackTriggerProcessor::setStartOffsetSeconds(double seconds) {
  setParamValue(ids::startOffset, static_cast<float>(seconds * 1000.0));
}

double BackingTrackTriggerProcessor::getStartOffsetSeconds() const {
  return startOffsetParam->load() / 1000.0;
}

void BackingTrackTriggerProcessor::setStartOffsetFromProgress(float progress) {
  auto s = getSample();
  if (s == nullptr || s->audio.getNumSamples() == 0)
    return;

  progress = juce::jlimit(0.0f, 1.0f, progress);
  const double lenSec = s->audio.getNumSamples() / s->playbackSampleRate;
  setParamValue(ids::startOffset,
                static_cast<float>(progress * lenSec * 1000.0));
}

//==============================================================================
SampleBuffer::Ptr BackingTrackTriggerProcessor::getSample() const {
  const juce::SpinLock::ScopedLockType lock(sampleLock);
  return currentSample;
}

bool BackingTrackTriggerProcessor::hasSampleLoaded() const {
  auto s = getSample();
  return s != nullptr && s->audio.getNumSamples() > 0;
}

juce::String BackingTrackTriggerProcessor::getSampleName() const {
  auto s = getSample();
  return s != nullptr ? s->name : juce::String();
}

double BackingTrackTriggerProcessor::getSampleLengthSeconds() const {
  auto s = getSample();
  if (s == nullptr || s->audio.getNumSamples() == 0)
    return 0.0;
  return s->audio.getNumSamples() / s->playbackSampleRate;
}

float BackingTrackTriggerProcessor::getPlaybackProgress() const {
  auto s = getSample();
  if (s == nullptr || s->audio.getNumSamples() == 0)
    return 0.0f;
  return static_cast<float>(publishedPos.load()) /
         static_cast<float>(s->audio.getNumSamples());
}

double BackingTrackTriggerProcessor::getOriginalSampleRate() const {
  auto s = getSample();
  return s != nullptr ? s->sourceSampleRate : 44100.0;
}

int BackingTrackTriggerProcessor::getOriginalNumChannels() const {
  auto s = getSample();
  return s != nullptr ? s->sourceNumChannels : 2;
}

int BackingTrackTriggerProcessor::getOriginalBitsPerSample() const {
  auto s = getSample();
  return s != nullptr ? s->sourceBitsPerSample : 16;
}

bool BackingTrackTriggerProcessor::isResampled() const {
  auto s = getSample();
  return s != nullptr && s->wasResampled;
}

//==============================================================================
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new BackingTrackTriggerProcessor();
}
