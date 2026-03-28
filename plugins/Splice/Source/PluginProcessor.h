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

    // SEQ step counts + speed mults (written UI thread via resource provider)
    std::array<int, 4> seqStepCount { 16, 16, 16, 16 };
    std::array<int, 4> seqSpeedMult { 2, 2, 2, 2 };  // index into kSpeedTable; 2 = x1

    // SEQ active step per lane (written audio thread, read UI timer)
    std::atomic<int>   seqLitStep0 { 0 };
    std::atomic<int>   seqLitStep1 { 0 };
    std::atomic<int>   seqLitStep2 { 0 };
    std::atomic<int>   seqLitStep3 { 0 };

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

    // Smoothed dB values for EQ — prevents coefficient-change crackling
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> eqLowSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> eqLowMidSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> eqHighMidSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> eqHighSmooth;

    // Tape Age — one-pole HF rolloff state (L/R) + crackle burst state
    float tapeLP_L     = 0.0f, tapeLP_R = 0.0f;
    float crackleEnv   = 0.0f;   // current burst envelope (0 = silent)
    float crackleDecay = 0.0f;   // per-sample decay multiplier
    float crackleAmp   = 0.0f;   // burst amplitude

    // Wow & Flutter — stereo modulated delay line
    static constexpr int kWFBufSize = 4096;   // ~85ms @ 48kHz
    std::array<float, kWFBufSize> wfBufL {}, wfBufR {};
    int    wfWritePos    = 0;
    double wfPhaseWow    = 0.0;   // 0.8 Hz LFO
    double wfPhaseFlutter = 0.0;  // 8.0 Hz LFO

    //==========================================================================
    // Amp Envelope — linear ADSR, one instance per voice
    struct AmpEnvelope
    {
        enum class State { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE } state = State::IDLE;
        float value        = 0.0f;
        float progress     = 0.0f;   // linear 0..1 within current stage
        float attackRate   = 0.0f;
        float decayRate    = 0.0f;
        float sustainLevel = 1.0f;
        float releaseRate  = 0.0f;
        float releaseStart = 1.0f;   // amplitude captured at noteOff
        float attackShape  = 0.0f;   // -1..+1 (0 = linear)
        float decayShape   = 0.0f;
        float releaseShape = 0.0f;

        // exponent = 4^shape → [0.25 .. 1.0 .. 4.0]
        static float applyShape (float p, float shape) noexcept
        {
            if (std::abs (shape) < 0.001f) return p;
            return std::pow (p, std::pow (4.0f, shape));
        }

        void noteOn (float atk, float dec, float sus, double sr)
        {
            progress     = 0.0f;
            value        = 0.0f;
            sustainLevel = juce::jlimit (0.0f, 1.0f, sus);
            attackRate   = (atk > 0.0f) ? (1.0f / (atk  * (float) sr)) : 1.0f;
            decayRate    = (dec > 0.0f) ? (1.0f / (dec  * (float) sr)) : 1.0f;
            state = State::ATTACK;
        }

        void noteOff (float rel, double sr)
        {
            if (state != State::IDLE)
            {
                releaseStart = value;
                releaseRate  = (rel > 0.0f) ? (1.0f / (rel * (float) sr)) : 1.0f;
                progress = 0.0f;
                state = State::RELEASE;
            }
        }

        float process()
        {
            switch (state)
            {
                case State::ATTACK:
                    progress += attackRate;
                    if (progress >= 1.0f)
                    {
                        progress = 0.0f;
                        value    = 1.0f;
                        state    = (sustainLevel >= 1.0f) ? State::SUSTAIN : State::DECAY;
                    }
                    else { value = applyShape (progress, attackShape); }
                    break;

                case State::DECAY:
                    progress += decayRate;
                    if (progress >= 1.0f)
                    {
                        value = sustainLevel;
                        state = State::SUSTAIN;
                    }
                    else { value = 1.0f - applyShape (progress, decayShape) * (1.0f - sustainLevel); }
                    break;

                case State::SUSTAIN:
                    break;

                case State::RELEASE:
                    progress += releaseRate;
                    if (progress >= 1.0f)
                    {
                        value = 0.0f;
                        state = State::IDLE;
                    }
                    else { value = releaseStart * (1.0f - applyShape (progress, releaseShape)); }
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
        float  volRandMult    = 1.0f;     // per-voice volume randomization multiplier
        juce::dsp::LadderFilter<float> ladderL, ladderR; // per-voice Moog ladder filter
        AmpEnvelope fenvEnv;              // filter envelope (sustain=0 → AD shape)
        int    age             = 0;
        double slicePhase      = 0.0;    // BPM clock accumulator [0..1)
        AmpEnvelope ampEnv;

        // LadderFilter is non-copyable — copy all scalar state, reset filters on dst
        void copyStateFrom (const SpliceVoice& src)
        {
            active = src.active;  gateOpen = src.gateOpen;  sliceEnded = src.sliceEnded;
            midiNote = src.midiNote;  currentSliceIdx = src.currentSliceIdx;
            pingpongDir = src.pingpongDir;  playhead = src.playhead;
            playRate = src.playRate;  startSample = src.startSample;  endSample = src.endSample;
            velocity = src.velocity;  pan = src.pan;
            filterCutoff = src.filterCutoff;  filterRes = src.filterRes;
            volRandMult = src.volRandMult;
            fenvEnv = src.fenvEnv;  ampEnv = src.ampEnv;
            age = src.age;  slicePhase = src.slicePhase;
            ladderL.reset();  ladderR.reset();
        }
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
    bool   lastHoldState     = false;

    // Arp state
    static constexpr int kMaxArpNotes = 32;
    int    arpHeldNotes[kMaxArpNotes] {};
    float  arpHeldVels [kMaxArpNotes] {};
    int    arpNoteCount  = 0;
    int    arpSeqIdx     = 0;
    int    arpPingDir    = 1;
    double arpPhase      = 0.0;

    // SEQ engine state
    std::array<double, 4> seqPhase     {};
    std::array<int, 4>    seqStep      {};
    int    seqPingDir    = 1;

    // Voice helpers
    int getGridValue() const;
    std::pair<int, int> getSliceRange (int sliceIdx) const;
    void allocateVoice      (int midiNote, float velocity, int sliceIdx,
                             float atk, float dec, float sus,
                             int dirOverride = -1, float extraPitchSt = 0.0f);
    void releaseVoice       (int midiNote, float rel);
    void setupSlicePlayback (SpliceVoice& v, int sliceDirIdx,
                             float extraPitchSt = 0.0f);
    int  advanceSeqSlice    (int currentIdx, int& pingDir, int reelDirIdx, int totalSlices) const;
    void advanceToNextSlice (SpliceVoice& v, int reelDirIdx, int sliceDirIdx, int totalSlices);

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpliceAudioProcessor)
};
