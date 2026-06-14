# Changelog

All notable changes to Backing Track Trigger are documented here.

## [2.0.0] - 2025

A substantial overhaul focused on reliability, sync accuracy, and portability.

### Added
- **Sample-accurate MIDI triggering.** Playback now starts at the exact sample
  offset of the note within the block instead of the block boundary, so the
  track stays tight against the MuseScore / DAW transport.
- **Gain control** (-60 dB … +12 dB) with smoothing to avoid zipper noise.
- **Loop toggle** to repeat the sample until note-off / stop.
- **Note-Off Stops mode** — optionally fade the sample out when the key is
  released (default is still play-to-completion).
- **Trigger-note selector** — fire on any note, or restrict to one specific
  MIDI note (shown as a note name, e.g. `C4`).
- **Retrigger toggle** — choose whether a new note restarts an in-progress play.
- **Follow-Transport toggle** — enable/disable the auto-reset on host stop/rewind.
- **Click-free fades** — short fade-in/out (configurable) prevent pops when a
  sample doesn't begin or end on a zero crossing.
- **Drag-and-drop loading** — drop an audio file straight onto the waveform.
- **Output level meter.**
- **Embed-in-project option** — store the audio (lossless FLAC) inside the saved
  state so the backing track travels with the score when you share it.
- **Automatable parameters** via `AudioProcessorValueTreeState` — every control
  is saved with the project and exposed to host automation.
- **Unit test suite** (`-DBTT_BUILD_TESTS=ON`) covering load/resample,
  triggering, gain, and state round-trips (path + embedded).
- **GitHub Actions CI** building VST3/AU/Standalone on macOS, Windows and Linux
  and validating the plugin with pluginval (strictness 8).
- **LICENSE** file (MIT) and this changelog.

### Changed
- **Higher-quality resampling.** Replaced the hand-rolled cubic interpolation
  with JUCE's `LagrangeInterpolator`, and always resample from the pristine
  source rather than from already-resampled audio (so repeated host-rate
  changes don't degrade quality).
- README rewritten with cross-platform build instructions and full feature docs.

### Fixed
- **Audio-thread data race.** Loading a sample while audio was running could
  glitch or crash because the sample buffer was mutated on the message thread
  while the audio thread read it. Samples are now handed over as immutable,
  reference-counted objects with a lock-free read on the audio thread; old
  buffers are reclaimed on the message thread so the audio thread never
  allocates or frees memory.
- **No more file I/O on the audio path.** Host sample-rate changes rebuild the
  playback buffer on the message thread.
- README no longer contradicts the actual note-off behaviour.

## [1.2.0]
- Zoom controls (+/-), millisecond input field, scroll-wheel panning.

## [1.1.0]
- Version display in the UI.

## [1.0.0]
- Initial one-shot VST3 sampler: load a WAV, trigger with any MIDI note,
  waveform display, start offset, auto-resampling.
