# Backing Track Trigger

A simple one-shot VST3 sampler plugin designed for MuseScore 4. Load a long WAV file (backing track, stem, vocal part) and trigger playback with any MIDI note.

## Features

- **One-Shot Playback**: Plays the entire sample from start to finish
- **Any MIDI Note Triggers**: No pitch variation - all notes play at original pitch
- **Restart on New Note**: Send another note to restart from the beginning
- **Stop on Note-Off**: Release the key to stop playback
- **Waveform Display**: Visual representation of your loaded sample
- **Supports Multiple Formats**: WAV, AIFF, MP3, FLAC, OGG

## Use Case

Perfect for playing backing tracks synchronized with your MuseScore score. Simply:

1. Add this plugin as an instrument track
2. Load your backing track WAV file
3. Place a single whole note at the start of your score
4. Press play - your backing track plays in sync!

## Building

### Prerequisites

- macOS with Xcode installed
- CMake 3.22 or later

### Build Commands

```bash
# Configure the build
cmake -B build -G Xcode

# Build the plugin
cmake --build build --config Release
```

The built plugin will be automatically copied to your system's VST3 folder.

### Manual Installation

If needed, copy the plugin manually:

```bash
cp -r build/BackingTrackTrigger_artefacts/Release/VST3/BackingTrackTrigger.vst3 \
      /Library/Audio/Plug-Ins/VST3/
```

## License

MIT License - Feel free to use and modify.
