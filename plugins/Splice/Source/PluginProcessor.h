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
    std::array<bool, 64> sliceActive;  // true = active (plays), false = muted (skipped)

    // SEQ lane values: 4 lanes × 16 steps (not JUCE params)
    std::array<std::array<float, 16>, 4> laneValues {};

    // Reel (loaded audio)
    void loadReelFile (const juce::File& file);
    juce::String getReelName() const { return reelName; }
    bool isReelLoaded() const { return reelBuffer.getNumSamples() > 0; }
    juce::File getLastReelFile() const;

    // Persistent settings (last loaded file, etc.)
    juce::ApplicationProperties appProperties;

    // Metering (written audio thread → read by UI timer)
    std::atomic<float> outputPeakLevel  { 0.0f };
    std::atomic<int>   activeSliceIdx   { -1   };  // -1 = no voice playing

private:
    //==========================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    double currentSampleRate = 44100.0;
    int    lastGridVal       = 1;   // track grid changes to reset slice selections

    // Loaded audio data
    juce::AudioBuffer<float> reelBuffer;
    double reelSampleRate = 44100.0;
    juce::String reelName;

    // Output smoothing (anti-zipper on volume changes)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOutputVol;

    // Global 4-band EQ (low shelf / low-mid peak / high-mid peak / high shelf)
    using EqBand = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                   juce::dsp::IIR::Coefficients<float>>;
    juce::dsp::ProcessorChain<EqBand, EqBand, EqBand, EqBand> eqChain;

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
            value        = 0.0f;   // reset for clean retrigger at each slice
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
        bool   active          = false;
        bool   gateOpen        = false;  // true while MIDI note is held (poly mode)
        bool   sliceEnded      = false;  // non-poly modes: triggers release at slice end
        int    midiNote        = -1;
        int    currentSliceIdx = 0;
        int    pingpongDir     = 1;      // +1 or -1 for pingpong reel direction
        float  playhead        = 0.0f;
        float  playRate        = 1.0f;   // positive (fwd) or negative (rev)
        int    startSample     = 0;
        int    endSample       = 0;
        float  velocity        = 1.0f;
        float  pan             = 0.0f;   // stereo position [-1..1], computed at spawn
        float  filterCutoff   = 18000.0f; // Hz, set at spawn (randomizable)
        float  filterRes      = 0.0f;     // 0..1
        float  svfIc1L = 0.0f, svfIc2L = 0.0f; // SVF state — left
        float  svfIc1R = 0.0f, svfIc2R = 0.0f; // SVF state — right
        AmpEnvelope fenvEnv;              // filter envelope (sustain=0 → AD shape)
        int    age             = 0;
        double slicePhase      = 0.0;    // BPM clock accumulator [0..1)
        AmpEnvelope ampEnv;
    };

    static constexpr int kNumVoices = 32;
    SpliceVoice voices[kNumVoices];
    int voiceAge = 0;

    // Global Poly-mode sequencer state
    double polySlicePhase    = 0.0;
    int    polySeqSliceIdx   = 0;
    int    polySeqPingDir    = 1;
    int    polyMidiNote      = -1;
    float  polyVelocity      = 1.0f;
    bool   polyGateOpen      = false;
    int    polyHeldNoteCount = 0;

    // Voice helpers
    int getGridValue() const;
    std::pair<int, int> getSliceRange (int sliceIdx) const;
    void allocateVoice      (int midiNote, float velocity, int sliceIdx,
                             float atk, float dec, float sus);
    void releaseVoice       (int midiNote, float rel);
    void setupSlicePlayback (SpliceVoice& v, int sliceDirIdx);
    int  advanceSeqSlice    (int currentIdx, int& pingDir, int reelDirIdx, int totalSlices) const;
    void advanceToNextSlice (SpliceVoice& v, int reelDirIdx, int sliceDirIdx, int totalSlices);

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpliceAudioProcessor)
};
