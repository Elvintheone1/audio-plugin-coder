#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>
#include <cmath>

//==============================================================================
// AD Envelope (attack / decay, two-stage, monophonic)
struct AdEnvelope
{
    enum class Phase { Idle, Attack, Decay };
    Phase phase       = Phase::Idle;
    float level       = 0.0f;
    bool  prevTrigger = false;

    // Returns current envelope level after ticking one sample.
    // attackCoeff / decayCoeff are pre-computed per block from ms values.
    float tick (bool trigger, float attackCoeff, float decayCoeff) noexcept
    {
        if (trigger && !prevTrigger)   // rising edge → retrigger
        {
            phase = Phase::Attack;
            level = 0.0f;
        }
        prevTrigger = trigger;

        switch (phase)
        {
            case Phase::Idle:
                break;
            case Phase::Attack:
                level = 1.0f - (1.0f - level) * attackCoeff;
                if (level >= 0.999f) { level = 1.0f; phase = Phase::Decay; }
                break;
            case Phase::Decay:
                level *= decayCoeff;
                if (level < 0.0001f) { level = 0.0f; phase = Phase::Idle; }
                break;
        }
        return level;
    }

    void retrigger() noexcept { phase = Phase::Attack; level = 0.0f; }
    void reset()     noexcept { phase = Phase::Idle;   level = 0.0f; prevTrigger = false; }
};

//==============================================================================
// TPT State Variable Filter (Simper / Cytomic 2011)
struct TptSvf
{
    float ic1eq = 0.0f, ic2eq = 0.0f;  // integrator states
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float k  = 2.0f;                    // damping coefficient (2=no resonance, 0.05=self-osc)

    void updateCoeffs (float cutoffHz, float resonance, double sampleRate) noexcept
    {
        float fc = juce::jlimit (20.0f, static_cast<float>(sampleRate * 0.49), cutoffHz);
        float g  = std::tan (juce::MathConstants<float>::pi * fc / static_cast<float>(sampleRate));
        k  = 2.0f - resonance * 1.95f;  // k: 2 → 0.05 as resonance 0 → 1
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    // filterMode: 0 = LP, 1 = HP, 0.5 = BP (crossfade LP→HP)
    float tick (float in, float filterMode) noexcept
    {
        // Self-oscillation guard: soft-clip integrator states at high resonance
        if (k < 0.10f)
        {
            ic1eq = std::tanh (ic1eq * 0.95f);
            ic2eq = std::tanh (ic2eq * 0.95f);
        }

        float v3 = in - ic2eq;
        float v1 = a1 * ic1eq + a2 * v3;
        float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;

        float lp = v2;
        float hp = in - k * v1 - v2;
        return lp * (1.0f - filterMode) + hp * filterMode;
    }

    void reset() noexcept { ic1eq = ic2eq = 0.0f; }
};

//==============================================================================
// Dattorro Plate — Allpass (identical to BeadyEye)
struct GzAPF
{
    std::vector<float> buf;
    int writeIdx = 0;
    int delayLen = 1;

    void init (int bufSize, int delay)
    {
        buf.assign (static_cast<size_t>(bufSize), 0.0f);
        writeIdx = 0; delayLen = delay;
    }

    float tick (float in, float g) noexcept
    {
        int readIdx = (writeIdx - delayLen + static_cast<int>(buf.size()))
                      % static_cast<int>(buf.size());
        float delayed = buf[static_cast<size_t>(readIdx)];
        float out = -g * in + (1.0f - g * g) * delayed;  // |H|=1 allpass
        buf[static_cast<size_t>(writeIdx)] = in + g * delayed;
        writeIdx = (writeIdx + 1) % static_cast<int>(buf.size());
        return out;
    }

    float at (int n) const noexcept
    {
        int sz = static_cast<int>(buf.size());
        if (sz <= 0) return 0.0f;
        n = std::min (n, sz - 1);
        return buf[static_cast<size_t>((writeIdx - 1 - n + sz * 2) % sz)];
    }
};

//==============================================================================
// Dattorro Plate — Delay Line (identical to BeadyEye)
struct GzDL
{
    std::vector<float> buf;
    int writeIdx = 0;

    void init (int bufSize)
    {
        buf.assign (static_cast<size_t>(bufSize), 0.0f);
        writeIdx = 0;
    }

    void write (float x) noexcept
    {
        buf[static_cast<size_t>(writeIdx)] = x;
        writeIdx = (writeIdx + 1) % static_cast<int>(buf.size());
    }

    float at (int n) const noexcept
    {
        int sz = static_cast<int>(buf.size());
        if (sz <= 0) return 0.0f;
        n = std::min (n, sz - 1);
        return buf[static_cast<size_t>((writeIdx - 1 - n + sz * 2) % sz)];
    }
};

//==============================================================================
class GazelleAudioProcessor : public juce::AudioProcessor
{
public:
    GazelleAudioProcessor();
    ~GazelleAudioProcessor() override;

    //==============================================================================
    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    //==============================================================================
    int  getNumPrograms()   override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Public parameter state (WebView bound)
    juce::AudioProcessorValueTreeState apvts;

private:
    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==============================================================================
    double currentSampleRate = 44100.0;

    // PRNG (xorshift32)
    uint32_t prngState = 0x4A5A6A7Au;
    float nextRandom() noexcept;

    //==============================================================================
    // DUAL AD ENVELOPES
    AdEnvelope env1, env2;

    //==============================================================================
    // DUAL TPT SVF FILTERS
    TptSvf filter1, filter2;

    //==============================================================================
    // TILT EQ (one-pole shelving at ~2 kHz)
    float tiltLowState = 0.0f;

    //==============================================================================
    // FEEDBACK (1-sample delay)
    float feedbackSample = 0.0f;

    //==============================================================================
    // TAPE DELAY
    static constexpr int kTapeMaxSamples = 200000;  // ~800ms at 192kHz + headroom
    std::vector<float> tapeBufL, tapeBufR;
    int   tapePosL = 0, tapePosR = 0;
    float tapeFilterL = 0.0f, tapeFilterR = 0.0f;
    float tapeLfoPhase = 0.0f;

    // Interpolated read from circular delay buffer
    float readCircular (const std::vector<float>& buf, int writePos,
                        float delaySamples) const noexcept;

    //==============================================================================
    // RING MODULATOR
    float ringPhaseL = 0.0f, ringPhaseR = 0.0f;

    //==============================================================================
    // DATTORRO PLATE REVERB (for FX mode 2, Vintage Plate)
    GzAPF apf[8];   // [0..3] input diffusion, [4..5] L tank, [6..7] R tank
    GzDL  dl[4];    // [0..1] L tank, [2..3] R tank
    float rvLP1 = 0.0f, rvLP2 = 0.0f, rvFdL = 0.0f, rvFdR = 0.0f;
    int   rvFullDly[4] {};
    int   rvTapL[8]    {};
    int   rvTapR[8]    {};

    // Pre-delay buffer for plate reverb (max 80ms)
    static constexpr int kPreDelayMax = 16000;  // 80ms at 192kHz
    std::vector<float> preDelayBufL, preDelayBufR;
    int preDelayPos = 0;

    void initPlateReverb (double sr);
    std::pair<float, float> tickPlate (float inL, float inR, float decayAmount) noexcept;

    //==============================================================================
    // DSP HELPERS
    float distort     (float x, float driveGain, float distAmount) noexcept;
    float applyTiltEQ (float x, float tilt) noexcept;

    std::pair<float, float> processFX (float inL, float inR, int fxType,
                                       float p1, float p2) noexcept;
    std::pair<float, float> processTapeDelay (float inL, float inR,
                                              float p1, float p2) noexcept;
    std::pair<float, float> processRingMod   (float inL, float inR,
                                              float p1, float p2) noexcept;
    std::pair<float, float> processVintagePlate (float inL, float inR,
                                                  float p1, float p2) noexcept;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GazelleAudioProcessor)
};
