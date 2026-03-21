#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>

//==============================================================================
class SpliceAudioProcessor : public juce::AudioProcessor
{
public:
    SpliceAudioProcessor();
    ~SpliceAudioProcessor() override;

    //==========================================================================
    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==========================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==========================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Parameter state (public for editor access)
    juce::AudioProcessorValueTreeState apvts;

    // Grid state: which slices are active (not JUCE params — serialized manually)
    // Layout: section 0..3, each section has up to 16 slices (flattened)
    std::array<bool, 64> sliceActive {};

    // SEQ lane values: 4 lanes × 16 steps (not JUCE params)
    std::array<std::array<float, 16>, 4> laneValues {};

    // Reel (loaded audio)
    void loadReelFile (const juce::File& file);
    juce::String getReelName() const { return reelName; }
    bool isReelLoaded() const { return reelBuffer.getNumSamples() > 0; }

    // Metering (written audio thread → read by UI timer)
    std::atomic<float> outputPeakLevel { 0.0f };

private:
    //==========================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    double currentSampleRate = 44100.0;

    // Loaded audio data
    juce::AudioBuffer<float> reelBuffer;
    double reelSampleRate = 44100.0;
    juce::String reelName;

    // Output smoothing (anti-zipper on volume changes)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOutputVol;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpliceAudioProcessor)
};
