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
    // Amp Envelope — linear ADSR, one instance per voice
    struct AmpEnvelope
    {
        enum class State { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE } state = State::IDLE;
        float value        = 0.0f;
        float attackRate   = 0.0f;
        float decayRate    = 0.0f;
        float sustainLevel = 1.0f;
        float releaseRate  = 0.0f;

        void noteOn (float atk, float dec, float sus, double sr)
        {
            sustainLevel = juce::jlimit (0.0f, 1.0f, sus);
            attackRate   = (atk > 0.0f) ? (1.0f / (atk  * (float) sr)) : 1.0f;
            decayRate    = (dec > 0.0f) ? ((1.0f - sustainLevel) / (dec * (float) sr)) : (1.0f - sustainLevel);
            state = State::ATTACK;
        }

        void noteOff (float rel, double sr)
        {
            if (state != State::IDLE)
            {
                releaseRate = (rel > 0.0f) ? (value / (rel * (float) sr)) : value;
                state = State::RELEASE;
            }
        }

        float process()
        {
            switch (state)
            {
                case State::ATTACK:
                    value += attackRate;
                    if (value >= 1.0f)
                    {
                        value = 1.0f;
                        state = (sustainLevel >= 1.0f) ? State::SUSTAIN : State::DECAY;
                    }
                    break;

                case State::DECAY:
                    value -= decayRate;
                    if (value <= sustainLevel) { value = sustainLevel; state = State::SUSTAIN; }
                    break;

                case State::SUSTAIN:
                    break;

                case State::RELEASE:
                    value -= releaseRate;
                    if (value <= 0.0f) { value = 0.0f; state = State::IDLE; }
                    break;

                default: break;
            }
            return value;
        }

        bool isIdle() const { return state == State::IDLE; }
    };

    // Polyphonic voice — one slice playback + envelope
    struct SpliceVoice
    {
        bool  active      = false;
        bool  sliceEnded  = false;  // true once playhead reaches end (triggers release)
        int   midiNote    = -1;
        float playhead    = 0.0f;   // current read position (samples, float for sub-sample)
        float playRate    = 1.0f;   // samples-per-output-sample
        int   startSample = 0;
        int   endSample   = 0;
        float velocity    = 1.0f;
        int   age         = 0;      // for voice stealing (steal highest age = oldest)
        AmpEnvelope ampEnv;
    };

    static constexpr int kNumVoices = 8;
    SpliceVoice voices[kNumVoices];
    int voiceAge = 0;

    // Voice helpers
    int getGridValue() const;
    std::pair<int, int> getSliceRange (int sliceIdx) const;
    void allocateVoice (int midiNote, float velocity, int sliceIdx,
                        float atk, float dec, float sus, float rel);
    void releaseVoice  (int midiNote, float rel);

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpliceAudioProcessor)
};
