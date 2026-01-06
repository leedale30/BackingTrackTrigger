#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

//==============================================================================
/**
 * BackingTrackTrigger - A one-shot sampler for playing backing tracks
 * 
 * This plugin loads a WAV file and plays it when any MIDI note is received.
 * - Any MIDI Note-On: Starts/restarts playback from beginning
 * - Any MIDI Note-Off: Stops playback
 * - No pitch shifting: all notes play at original sample pitch
 */
class BackingTrackTriggerProcessor : public juce::AudioProcessor
{
public:
    BackingTrackTriggerProcessor();
    ~BackingTrackTriggerProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
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
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Sample loading and playback control
    void loadSample(const juce::File& file);
    void startPlayback();
    void stopPlayback();
    
    bool hasSampleLoaded() const { return sampleBuffer.getNumSamples() > 0; }
    juce::String getSampleName() const { return loadedSampleName; }
    double getSampleLengthSeconds() const;
    bool isPlaying() const { return playing; }
    float getPlaybackProgress() const;
    
    // Get sample buffer for waveform display
    const juce::AudioBuffer<float>& getSampleBuffer() const { return sampleBuffer; }

private:
    //==============================================================================
    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> sampleBuffer;
    juce::String loadedSampleName;
    double loadedSampleRate = 44100.0;
    
    // Playback state
    std::atomic<bool> playing { false };
    std::atomic<int64_t> playbackPosition { 0 };
    
    // Current sample rate from host
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackingTrackTriggerProcessor)
};
