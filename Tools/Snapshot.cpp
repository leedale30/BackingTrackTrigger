// Renders the plugin editor to a PNG offline (no audio device, no window).
// Usage: BackingTrackTriggerSnapshot <output.png>
//
// Useful for documentation and for eyeballing UI changes in CI.

#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

static juce::File makeDemoWav() {
  auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                  .getChildFile("btt_snapshot_demo.wav");
  file.deleteFile();

  const double sr = 44100.0;
  const int n = (int)(sr * 3.0);
  juce::AudioBuffer<float> tone(2, n);
  for (int ch = 0; ch < 2; ++ch) {
    auto *d = tone.getWritePointer(ch);
    for (int i = 0; i < n; ++i) {
      const float t = (float)i / (float)sr;
      // An envelope + a couple of partials so the waveform looks like audio.
      const float env = 0.3f + 0.6f * std::abs(std::sin(t * 1.3f));
      d[i] = env * 0.7f *
             (std::sin(juce::MathConstants<float>::twoPi * 220.0f * t) +
              0.4f * std::sin(juce::MathConstants<float>::twoPi * 440.0f * t));
    }
  }

  juce::WavAudioFormat wav;
  if (auto *os = file.createOutputStream().release()) {
    std::unique_ptr<juce::AudioFormatWriter> w(
        wav.createWriterFor(os, sr, 2, 16, {}, 0));
    if (w != nullptr)
      w->writeFromAudioSampleBuffer(tone, 0, n);
    else
      delete os;
  }
  return file;
}

int main(int argc, char *argv[]) {
  juce::ScopedJuceInitialiser_GUI juceInit;

  const juce::File out(argc > 1 ? juce::String(argv[1])
                                : juce::String("editor.png"));

  BackingTrackTriggerProcessor processor;
  processor.prepareToPlay(44100.0, 512);

  auto wav = makeDemoWav();
  processor.loadSample(wav);
  processor.setStartOffsetSeconds(0.42);

  std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
  editor->setSize(700, 560);

  auto image = editor->createComponentSnapshot(editor->getLocalBounds());

  juce::PNGImageFormat png;
  out.deleteFile();
  if (auto os = out.createOutputStream()) {
    png.writeImageToStream(image, *os);
    juce::Logger::writeToLog("Wrote " + out.getFullPathName());
  }

  editor = nullptr;
  wav.deleteFile();
  return 0;
}
