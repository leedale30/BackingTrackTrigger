// Standalone unit tests for BackingTrackTriggerProcessor.
//
// These exercise the DSP and state logic directly (no host, no GUI window):
//  - loading + automatic resampling from a non-host sample rate
//  - sample-accurate MIDI triggering and audible output
//  - output stays finite and bounded
//  - gain parameter scales the output
//  - state round-trip by file path
//  - state round-trip with embedded audio (survives the source file vanishing)
//
// Built only when BTT_BUILD_TESTS=ON. Returns non-zero if any check fails.

#include "../Source/PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace {
int failures = 0;

void check(bool condition, const juce::String &what) {
  if (condition) {
    juce::Logger::writeToLog("  PASS  " + what);
  } else {
    ++failures;
    juce::Logger::writeToLog("  FAIL  " + what);
  }
}

// Write a stereo sine sweep to a WAV file at the given rate; return the file.
juce::File makeTestWav(double sampleRate, double seconds) {
  auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                  .getChildFile("btt_test_" + juce::String((int)sampleRate) +
                                ".wav");
  file.deleteFile();

  const int numSamples = (int)(sampleRate * seconds);
  juce::AudioBuffer<float> tone(2, numSamples);
  for (int ch = 0; ch < 2; ++ch) {
    auto *d = tone.getWritePointer(ch);
    for (int i = 0; i < numSamples; ++i)
      d[i] = 0.5f * std::sin(juce::MathConstants<float>::twoPi * 440.0f *
                             (float)i / (float)sampleRate);
  }

  juce::WavAudioFormat wav;
  if (auto *os = file.createOutputStream().release()) {
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(os, sampleRate, 2, 16, {}, 0));
    if (writer != nullptr)
      writer->writeFromAudioSampleBuffer(tone, 0, numSamples);
    else
      delete os;
  }
  return file;
}

// Render `numBlocks` blocks, triggering a note in the first block. Returns the
// peak magnitude seen across all blocks and flags any non-finite sample.
float renderTriggered(BackingTrackTriggerProcessor &p, double rate,
                      int blockSize, int numBlocks, bool &allFinite) {
  juce::AudioBuffer<float> buffer(2, blockSize);
  float peak = 0.0f;
  allFinite = true;

  for (int b = 0; b < numBlocks; ++b) {
    buffer.clear();
    juce::MidiBuffer midi;
    if (b == 0)
      midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);

    p.processBlock(buffer, midi);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
      for (int i = 0; i < buffer.getNumSamples(); ++i) {
        const float s = buffer.getSample(ch, i);
        if (!std::isfinite(s))
          allFinite = false;
        peak = juce::jmax(peak, std::abs(s));
      }
  }
  juce::ignoreUnused(rate);
  return peak;
}
} // namespace

int main() {
  juce::ScopedJuceInitialiser_GUI juceInit;
  juce::Logger::writeToLog("=== BackingTrackTrigger unit tests ===");

  const double hostRate = 44100.0;
  const int blockSize = 512;
  auto wav48 = makeTestWav(48000.0, 1.0);

  // --- Load + resample -------------------------------------------------------
  {
    BackingTrackTriggerProcessor p;
    p.prepareToPlay(hostRate, blockSize);
    p.loadSample(wav48);

    check(p.hasSampleLoaded(), "sample loads");
    check(std::abs(p.getOriginalSampleRate() - 48000.0) < 1.0,
          "original sample rate preserved (48 kHz)");
    check(p.isResampled(), "48 kHz source is resampled to 44.1 kHz host");
    check(std::abs(p.getSampleLengthSeconds() - 1.0) < 0.05,
          "duration is ~1.0 s after resampling");
  }

  // --- Triggering + audible, bounded output ----------------------------------
  {
    BackingTrackTriggerProcessor p;
    p.prepareToPlay(hostRate, blockSize);
    p.loadSample(wav48);

    bool finite = false;
    const float peak = renderTriggered(p, hostRate, blockSize, 8, finite);
    check(peak > 0.1f, "MIDI note produces audible output");
    check(finite, "output contains no NaN/Inf");
    check(peak < 1.5f, "output stays bounded");
    check(p.isPlaying(), "still playing partway through a 1 s sample");
  }

  // --- Gain parameter scales output ------------------------------------------
  {
    BackingTrackTriggerProcessor p;
    p.prepareToPlay(hostRate, blockSize);
    p.loadSample(wav48);

    if (auto *gain = p.apvts.getParameter("gain"))
      gain->setValueNotifyingHost(
          p.apvts.getParameterRange("gain").convertTo0to1(-12.0f));

    bool finite = false;
    const float quiet = renderTriggered(p, hostRate, blockSize, 8, finite);

    if (auto *gain = p.apvts.getParameter("gain"))
      gain->setValueNotifyingHost(
          p.apvts.getParameterRange("gain").convertTo0to1(0.0f));
    BackingTrackTriggerProcessor p2;
    p2.prepareToPlay(hostRate, blockSize);
    p2.loadSample(wav48);
    const float loud = renderTriggered(p2, hostRate, blockSize, 8, finite);

    check(quiet < loud, "-12 dB gain is quieter than 0 dB");
  }

  // --- State round-trip by path ----------------------------------------------
  {
    BackingTrackTriggerProcessor a;
    a.prepareToPlay(hostRate, blockSize);
    a.loadSample(wav48);
    a.setStartOffsetSeconds(0.25);

    juce::MemoryBlock state;
    a.getStateInformation(state);

    BackingTrackTriggerProcessor b;
    b.prepareToPlay(hostRate, blockSize);
    b.setStateInformation(state.getData(), (int)state.getSize());

    check(b.hasSampleLoaded(), "state restore reloads sample from path");
    check(std::abs(b.getStartOffsetSeconds() - 0.25) < 0.01,
          "start offset survives state round-trip");
  }

  // --- Embedded state survives the source file disappearing ------------------
  {
    auto wavTemp = makeTestWav(44100.0, 0.5);

    BackingTrackTriggerProcessor a;
    a.prepareToPlay(hostRate, blockSize);
    a.loadSample(wavTemp);
    a.setEmbedEnabled(true);

    juce::MemoryBlock state;
    a.getStateInformation(state);

    wavTemp.deleteFile(); // the original is now gone

    BackingTrackTriggerProcessor b;
    b.prepareToPlay(hostRate, blockSize);
    b.setStateInformation(state.getData(), (int)state.getSize());

    check(b.hasSampleLoaded(),
          "embedded audio restores even after the source file is deleted");

    bool finite = false;
    const float peak = renderTriggered(b, hostRate, blockSize, 4, finite);
    check(peak > 0.1f && finite, "embedded sample plays back correctly");
  }

  wav48.deleteFile();

  juce::Logger::writeToLog(failures == 0
                               ? "=== ALL TESTS PASSED ==="
                               : "=== " + juce::String(failures) +
                                     " CHECK(S) FAILED ===");
  return failures == 0 ? 0 : 1;
}
