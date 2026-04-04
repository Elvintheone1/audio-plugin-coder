#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_osc/juce_osc.h>
#include <atomic>
#include <array>

//==============================================================================
// GestureCV — MIDI Effect plugin
// Receives OSC /gesture/lanes [f×8] from Python gesture engine,
// routes each lane to a configurable MIDI CC output.
//==============================================================================

static constexpr int kNumLanes = 8;

struct LaneParams
{
    juce::AudioParameterInt*   ccNumber  = nullptr;  // 0–127
    juce::AudioParameterFloat* scale     = nullptr;  // 0.0–2.0
    juce::AudioParameterFloat* offset    = nullptr;  // -1.0–1.0
    juce::AudioParameterFloat* curve     = nullptr;  // -1.0–1.0
    juce::AudioParameterFloat* smooth    = nullptr;  // 0.0–1.0
};

class GestureCVProcessor  : public juce::AudioProcessor,
                             public juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    GestureCVProcessor();
    ~GestureCVProcessor() noexcept override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override { return "GestureCV"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==========================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // OSC
    void oscMessageReceived (const juce::OSCMessage& message) override;
    void rebindOSC();

    // Lane values — written by OSC thread, read by audio thread
    std::array<std::atomic<float>, kNumLanes> laneValues;

    // Connection status for UI
    std::atomic<bool>  oscConnected { false };
    std::atomic<int>   oscPacketsReceived { 0 };

    juce::AudioProcessorValueTreeState apvts;

    // Per-lane params array (populated in constructor)
    std::array<LaneParams, kNumLanes> laneParams;

    // Global params
    juce::AudioParameterInt*   oscPortParam   = nullptr;
    juce::AudioParameterInt*   midiChannelParam = nullptr;
    juce::AudioParameterFloat* globalSmoothParam = nullptr;

private:
    juce::OSCReceiver oscReceiver;
    int currentOscPort = 9000;

    // Audio-thread state
    std::array<float, kNumLanes> laneSmoothed {};
    std::array<int,   kNumLanes> lastCcVal    {};
    double currentSampleRate = 44100.0;

    float applyCurve (float x, float curve) const noexcept;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GestureCVProcessor)
};
