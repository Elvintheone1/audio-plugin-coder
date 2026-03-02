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
        // Exponential k curve: Q goes from ~0.5 (woody) to ~67 (long ring)
        // k = 2·exp(-5·resonance): at res=0 → k≈2 (Q≈0.5), at res=1 → k≈0.013 (Q≈77)
        k  = std::max (0.015f, 2.0f * std::exp (-5.0f * resonance));
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    // filterMode: 0 = LP, 1 = HP, 0.5 = BP (crossfade LP→HP)
    float tick (float in, float filterMode) noexcept
    {
        // Self-oscillation guard: hard-clip IC states only when they exceed ±1.
        // Replacing tanh (which ran EVERY sample and added ~14% damping/sample,
        // killing any ring in <2ms) with a threshold clip that fires only when
        // the integrators are genuinely overloading. Ring is preserved completely.
        if (k < 0.10f)
        {
            if (ic1eq >  1.0f) ic1eq =  1.0f;
            else if (ic1eq < -1.0f) ic1eq = -1.0f;
            if (ic2eq >  1.0f) ic2eq =  1.0f;
            else if (ic2eq < -1.0f) ic2eq = -1.0f;
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
// Second-order biquad filter — Transposed Direct Form II.
// Supports low shelf, peaking EQ, and high shelf (Audio EQ Cookbook, RBJ).
struct Biquad
{
    float s1 = 0.0f, s2 = 0.0f;
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;

    float tick (float x) noexcept
    {
        float y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { s1 = s2 = 0.0f; }

    // Low shelving, slope S = 1.  gainDb ∈ [-15, +15]
    void setLowShelf (float fc, float gainDb, double sr) noexcept
    {
        float A   = std::pow (10.0f, gainDb / 40.0f);
        float w0  = juce::MathConstants<float>::twoPi * fc / static_cast<float>(sr);
        float cw  = std::cos (w0);
        float alp = std::sin (w0) / std::sqrt (2.0f);  // S = 1
        float sqA = std::sqrt (A);
        float b0n = A * ((A+1) - (A-1)*cw + 2*sqA*alp);
        float b1n = 2*A * ((A-1) - (A+1)*cw);
        float b2n = A * ((A+1) - (A-1)*cw - 2*sqA*alp);
        float a0  = (A+1) + (A-1)*cw + 2*sqA*alp;
        float a1n = -2 * ((A-1) + (A+1)*cw);
        float a2n = (A+1) + (A-1)*cw - 2*sqA*alp;
        applyCoeffs (b0n, b1n, b2n, a0, a1n, a2n);
    }

    // Peaking EQ.  gainDb ∈ [-15, +15]
    void setPeakingEQ (float fc, float Q, float gainDb, double sr) noexcept
    {
        float A   = std::pow (10.0f, gainDb / 40.0f);
        float w0  = juce::MathConstants<float>::twoPi * fc / static_cast<float>(sr);
        float alp = std::sin (w0) / (2.0f * Q);
        float cw  = std::cos (w0);
        float b0n = 1 + alp * A;
        float b1n = -2 * cw;
        float b2n = 1 - alp * A;
        float a0  = 1 + alp / A;
        float a1n = -2 * cw;
        float a2n = 1 - alp / A;
        applyCoeffs (b0n, b1n, b2n, a0, a1n, a2n);
    }

    // High shelving, slope S = 1.  gainDb ∈ [-15, +15]
    void setHighShelf (float fc, float gainDb, double sr) noexcept
    {
        float A   = std::pow (10.0f, gainDb / 40.0f);
        float w0  = juce::MathConstants<float>::twoPi * fc / static_cast<float>(sr);
        float cw  = std::cos (w0);
        float alp = std::sin (w0) / std::sqrt (2.0f);  // S = 1
        float sqA = std::sqrt (A);
        float b0n = A * ((A+1) + (A-1)*cw + 2*sqA*alp);
        float b1n = -2*A * ((A-1) + (A+1)*cw);
        float b2n = A * ((A+1) + (A-1)*cw - 2*sqA*alp);
        float a0  = (A+1) - (A-1)*cw + 2*sqA*alp;
        float a1n = 2 * ((A-1) - (A+1)*cw);
        float a2n = (A+1) - (A-1)*cw - 2*sqA*alp;
        applyCoeffs (b0n, b1n, b2n, a0, a1n, a2n);
    }

private:
    void applyCoeffs (float b0n, float b1n, float b2n,
                      float a0,  float a1n, float a2n) noexcept
    {
        b0 = b0n / a0;  b1 = b1n / a0;  b2 = b2n / a0;
        a1 = a1n / a0;  a2 = a2n / a0;
    }
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
    // PITCH SWEEP (bridged-T style — frequency sweeps down from trigger to target)
    float pitch1State = 0.0f;  // semitones above target, decays to 0 after trigger
    float pitch2State = 0.0f;
    bool  lastTrig1   = false; // for rising-edge detection per block
    bool  lastTrig2   = false;

    //==============================================================================
    // DUAL TPT SVF FILTERS
    TptSvf filter1, filter2;
    float smoothedResonance = 0.6f;  // one-pole smoothed resonance (prevents crackle on knob turns)

    //==============================================================================
    // 3-BAND ACTIVE EQ (Sunn Beta Bass style — bass shelf, mid peak, treble shelf)
    Biquad eqBass, eqMid, eqTreble;

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
    float distort (float x, float sat) noexcept;

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
