#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GazelleAudioProcessor::GazelleAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GAZELLE_STATE", createParameterLayout())
{
}

GazelleAudioProcessor::~GazelleAudioProcessor() {}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
GazelleAudioProcessor::createParameterLayout()
{
    using Param   = juce::AudioParameterFloat;
    using ParamB  = juce::AudioParameterBool;
    using NRange  = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // FILTER
    params.push_back (std::make_unique<Param> ("cutoff",        "Cutoff",
        NRange (20.0f, 20000.0f, 0.0f, 0.25f), 800.0f));
    params.push_back (std::make_unique<Param> ("resonance",     "Resonance",
        NRange (0.0f, 1.0f), 0.6f));
    params.push_back (std::make_unique<Param> ("spread",        "Spread",
        NRange (-1.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<Param> ("filter_mode",   "LP→HP",
        NRange (0.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<Param> ("filter1_level", "Level I",
        NRange (0.0f, 1.0f), 1.0f));
    params.push_back (std::make_unique<Param> ("filter2_level", "Level II",
        NRange (0.0f, 1.0f), 0.8f));

    // ENVELOPES (per trigger path)
    params.push_back (std::make_unique<Param> ("attack1", "Attack I",
        NRange (0.5f, 50.0f, 0.0f, 0.4f), 2.0f));
    params.push_back (std::make_unique<Param> ("decay1",  "Decay I",
        NRange (5.0f, 8000.0f, 0.0f, 0.3f), 300.0f));
    params.push_back (std::make_unique<Param> ("attack2", "Attack II",
        NRange (0.5f, 50.0f, 0.0f, 0.4f), 2.0f));
    params.push_back (std::make_unique<Param> ("decay2",  "Decay II",
        NRange (5.0f, 8000.0f, 0.0f, 0.3f), 300.0f));

    // DISTORTION
    params.push_back (std::make_unique<Param> ("drive",         "Drive",
        NRange (0.0f, 1.0f), 0.4f));
    params.push_back (std::make_unique<Param> ("dist_amount",   "Dist",
        NRange (0.0f, 1.0f), 0.3f));
    params.push_back (std::make_unique<Param> ("dist_feedback", "Dist Feedback",
        NRange (0.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<ParamB> ("feedback_path", "FB Path", false));
    params.push_back (std::make_unique<Param> ("eq_tilt",       "EQ Tilt",
        NRange (-1.0f, 1.0f), 0.0f));

    // FX ENGINE
    params.push_back (std::make_unique<Param> ("fx_type", "FX Type",
        NRange (0.0f, 2.0f, 1.0f), 0.0f));   // step=1 → integer choice
    params.push_back (std::make_unique<Param> ("fx_p1",   "FX P1",
        NRange (0.0f, 1.0f), 0.5f));
    params.push_back (std::make_unique<Param> ("fx_p2",   "FX P2",
        NRange (0.0f, 1.0f), 0.5f));
    params.push_back (std::make_unique<Param> ("fx_wet",  "FX Wet",
        NRange (0.0f, 1.0f), 0.3f));

    // TRIGGERS (momentary gate: JS sets true on mousedown, false on mouseup)
    params.push_back (std::make_unique<ParamB> ("trigger1", "Trigger I",  false));
    params.push_back (std::make_unique<ParamB> ("trigger2", "Trigger II", false));

    return { params.begin(), params.end() };
}

//==============================================================================
void GazelleAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // Reset envelopes and filters
    env1.reset();
    env2.reset();
    filter1.reset();
    filter2.reset();

    // Reset tilt EQ state
    tiltLowState = 0.0f;

    // Reset feedback
    feedbackSample = 0.0f;

    // Tape delay buffers
    tapeBufL.assign (kTapeMaxSamples, 0.0f);
    tapeBufR.assign (kTapeMaxSamples, 0.0f);
    tapePosL = tapePosR = 0;
    tapeFilterL = tapeFilterR = 0.0f;
    tapeLfoPhase = 0.0f;

    // Ring mod phases
    ringPhaseL = ringPhaseR = 0.0f;

    // Plate reverb
    initPlateReverb (sampleRate);

    // Pre-delay buffer
    preDelayBufL.assign (kPreDelayMax, 0.0f);
    preDelayBufR.assign (kPreDelayMax, 0.0f);
    preDelayPos = 0;
}

void GazelleAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool GazelleAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    // No audio input required (synth), but accept stereo input if routed
    auto inSet = layouts.getMainInputChannelSet();
    if (!inSet.isDisabled() && inSet != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

//==============================================================================
float GazelleAudioProcessor::nextRandom() noexcept
{
    prngState ^= prngState << 13;
    prngState ^= prngState >> 17;
    prngState ^= prngState << 5;
    return static_cast<float>(prngState) * 4.656612873077393e-10f;  // /2^31
}

//==============================================================================
// MOSFET-inspired asymmetric saturation.
// Positive half: soft tanh-based with 2nd harmonic from DC bias.
// Negative half: same tanh but attenuated (harder clip character).
float GazelleAudioProcessor::distort (float x, float driveGain, float distAmount) noexcept
{
    x *= driveGain;
    const float bias = distAmount * 0.25f;           // asymmetric DC shift
    const float sat  = 1.0f + distAmount * 3.0f;    // saturation drive
    float out = std::tanh ((x + bias) * sat) - std::tanh (bias * sat);
    return out * (1.0f / std::tanh (sat));           // normalize to ~[-1, 1]
}

//==============================================================================
// One-pole shelving tilt EQ (pivot ~2 kHz).
// tilt: -1 = boost low / cut high; +1 = boost high / cut low
float GazelleAudioProcessor::applyTiltEQ (float x, float tilt) noexcept
{
    const float coeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi
                                          * 2000.0f / static_cast<float>(currentSampleRate));
    tiltLowState += coeff * (x - tiltLowState);
    float high = x - tiltLowState;

    // ±12 dB at extremes
    float lowGain  = std::pow (10.0f, std::max (0.0f, -tilt) * 12.0f / 20.0f);
    float highGain = std::pow (10.0f, std::max (0.0f,  tilt) * 12.0f / 20.0f);
    return tiltLowState * lowGain + high * highGain;
}

//==============================================================================
// TAPE DELAY
float GazelleAudioProcessor::readCircular (const std::vector<float>& buf,
                                            int writePos, float delaySamples) const noexcept
{
    int sz = static_cast<int>(buf.size());
    delaySamples = juce::jlimit (0.0f, static_cast<float>(sz - 1), delaySamples);
    int   dInt  = static_cast<int>(delaySamples);
    float dFrac = delaySamples - static_cast<float>(dInt);
    int r0 = (writePos - dInt     + sz * 2) % sz;
    int r1 = (writePos - dInt - 1 + sz * 2) % sz;
    return buf[static_cast<size_t>(r0)] + dFrac * (buf[static_cast<size_t>(r1)]
                                                  - buf[static_cast<size_t>(r0)]);
}

std::pair<float, float> GazelleAudioProcessor::processTapeDelay (float inL, float inR,
                                                                   float p1, float p2) noexcept
{
    // p1: delay time 5–800 ms
    float delayMs      = 5.0f + p1 * 795.0f;
    float delaySamples = delayMs * 0.001f * static_cast<float>(currentSampleRate);

    // LFO wobble (~0.4% of delay time, 3.5 Hz)
    float wobble = std::sin (tapeLfoPhase) * delaySamples * 0.004f;
    tapeLfoPhase += juce::MathConstants<float>::twoPi * 3.5f
                    / static_cast<float>(currentSampleRate);
    if (tapeLfoPhase > juce::MathConstants<float>::twoPi)
        tapeLfoPhase -= juce::MathConstants<float>::twoPi;

    float readSamples = delaySamples + wobble;
    float outL = readCircular (tapeBufL, tapePosL, readSamples);
    float outR = readCircular (tapeBufR, tapePosR, readSamples);

    // p2: feedback amount (0–0.8) + tape saturation
    float fbAmt    = p2 * 0.8f;
    float lpCoeff  = 0.15f + (1.0f - p2) * 0.45f;   // more rolloff at high fb
    tapeFilterL = tapeFilterL + lpCoeff * (outL - tapeFilterL);
    tapeFilterR = tapeFilterR + lpCoeff * (outR - tapeFilterR);

    float fbL = std::tanh (tapeFilterL * (1.0f + p2 * 2.0f)) * fbAmt;
    float fbR = std::tanh (tapeFilterR * (1.0f + p2 * 2.0f)) * fbAmt;

    tapeBufL[static_cast<size_t>(tapePosL)] = inL + fbL;
    tapeBufR[static_cast<size_t>(tapePosR)] = inR + fbR;
    tapePosL = (tapePosL + 1) % kTapeMaxSamples;
    tapePosR = (tapePosR + 1) % kTapeMaxSamples;

    return { outL, outR };
}

//==============================================================================
// RING MODULATOR
std::pair<float, float> GazelleAudioProcessor::processRingMod (float inL, float inR,
                                                                float p1, float p2) noexcept
{
    // p1: carrier frequency 20–4000 Hz (exponential)
    float carrierFreq = 20.0f * std::pow (200.0f, p1);

    float cL = std::sin (ringPhaseL);
    float cR = std::sin (ringPhaseR);

    float step = juce::MathConstants<float>::twoPi / static_cast<float>(currentSampleRate);
    ringPhaseL += step * carrierFreq;
    ringPhaseR += step * (carrierFreq + 2.0f);   // +2 Hz L/R detuning for stereo width
    if (ringPhaseL > juce::MathConstants<float>::twoPi) ringPhaseL -= juce::MathConstants<float>::twoPi;
    if (ringPhaseR > juce::MathConstants<float>::twoPi) ringPhaseR -= juce::MathConstants<float>::twoPi;

    // p2: modulation depth 0→1
    float outL = inL * (1.0f - p2) + inL * cL * p2;
    float outR = inR * (1.0f - p2) + inR * cR * p2;
    return { outL, outR };
}

//==============================================================================
// VINTAGE PLATE REVERB (Dattorro topology, pre-delay)
std::pair<float, float> GazelleAudioProcessor::processVintagePlate (float inL, float inR,
                                                                      float p1, float p2) noexcept
{
    // p1: pre-delay 0–80 ms
    int pdSamples = static_cast<int>(p1 * 80.0f * 0.001f * static_cast<float>(currentSampleRate));
    pdSamples = juce::jlimit (0, kPreDelayMax - 1, pdSamples);

    preDelayBufL[static_cast<size_t>(preDelayPos)] = inL;
    preDelayBufR[static_cast<size_t>(preDelayPos)] = inR;
    int rdPos = (preDelayPos - pdSamples + kPreDelayMax) % kPreDelayMax;
    float pdL = preDelayBufL[static_cast<size_t>(rdPos)];
    float pdR = preDelayBufR[static_cast<size_t>(rdPos)];
    preDelayPos = (preDelayPos + 1) % kPreDelayMax;

    // p2: decay amount (maps to plate reverb amount)
    return tickPlate (pdL, pdR, p2);
}

//==============================================================================
std::pair<float, float> GazelleAudioProcessor::processFX (float inL, float inR,
                                                            int fxType,
                                                            float p1, float p2) noexcept
{
    switch (fxType)
    {
        case 0:  return processTapeDelay     (inL, inR, p1, p2);
        case 1:  return processRingMod       (inL, inR, p1, p2);
        case 2:  return processVintagePlate  (inL, inR, p1, p2);
        default: return { inL, inR };
    }
}

//==============================================================================
// DATTORRO PLATE — INIT
void GazelleAudioProcessor::initPlateReverb (double sr)
{
    const double s = sr / 29761.0;
    auto S = [&s] (int n) { return std::max (1, static_cast<int>(static_cast<double>(n) * s)); };

    apf[0].init (S(142) + 1, S(142));
    apf[1].init (S(107) + 1, S(107));
    apf[2].init (S(379) + 1, S(379));
    apf[3].init (S(277) + 1, S(277));
    apf[4].init (S(2000),     S(672));
    apf[5].init (S(1800) + 1, S(1800));
    apf[6].init (S(2200),     S(908));
    apf[7].init (S(2656) + 1, S(2656));

    dl[0].init (S(4453) + 1);
    dl[1].init (S(3163) + 1);
    dl[2].init (S(4217) + 1);
    dl[3].init (S(3163) + 1);

    rvFullDly[0] = S(4453) - 1;
    rvFullDly[1] = S(3163) - 1;
    rvFullDly[2] = S(4217) - 1;
    rvFullDly[3] = S(3163) - 1;

    rvTapL[0] = S(266);  rvTapL[1] = S(2974);
    rvTapL[2] = S(1913); rvTapL[3] = S(1996);
    rvTapL[4] = S(1990); rvTapL[5] = S(187);
    rvTapL[6] = S(1066); rvTapL[7] = S(353);

    rvTapR[0] = S(353);  rvTapR[1] = S(3627);
    rvTapR[2] = S(1228); rvTapR[3] = S(2673);
    rvTapR[4] = S(2111); rvTapR[5] = S(335);
    rvTapR[6] = S(121);  rvTapR[7] = S(205);

    rvLP1 = rvLP2 = rvFdL = rvFdR = 0.0f;
}

//==============================================================================
// DATTORRO PLATE — TICK (identical to BeadyEye except struct names)
std::pair<float, float> GazelleAudioProcessor::tickPlate (float inL, float inR,
                                                           float amount) noexcept
{
    const float decay = 0.5f + amount * 0.28f;

    float damp;
    if (amount <= 0.75f)
        damp = 0.0005f + amount * 0.03f;
    else
        damp = 0.0005f + 0.75f * 0.03f + (amount - 0.75f) * 0.8f;

    static constexpr float kDiff1 = 0.70f;
    static constexpr float kDiff2 = 0.50f;
    static constexpr float kIn    = 0.75f;
    static constexpr float kIn2   = 0.625f;

    // NaN guard
    if (!std::isfinite (rvFdL) || !std::isfinite (rvFdR) ||
        !std::isfinite (rvLP1) || !std::isfinite (rvLP2))
    {
        for (auto& a : apf) std::fill (a.buf.begin(), a.buf.end(), 0.0f);
        for (auto& d : dl)  std::fill (d.buf.begin(), d.buf.end(), 0.0f);
        rvFdL = rvFdR = rvLP1 = rvLP2 = 0.0f;
        return { inL, inR };
    }

    float x = (inL + inR) * 0.5f;
    x = apf[0].tick (x, kIn);
    x = apf[1].tick (x, kIn);
    x = apf[2].tick (x, kIn2);
    x = apf[3].tick (x, kIn2);

    float tL = x + rvFdR;
    float tR = x + rvFdL;

    tL = apf[4].tick (tL, kDiff1);
    dl[0].write (tL);
    const float d0 = dl[0].at (rvFullDly[0]);
    rvLP1 = d0 + damp * (rvLP1 - d0);
    tL = rvLP1 * decay;
    tL = apf[5].tick (tL, kDiff2);
    dl[1].write (tL);
    rvFdL = dl[1].at (rvFullDly[1]);

    tR = apf[6].tick (tR, kDiff1);
    dl[2].write (tR);
    const float d2 = dl[2].at (rvFullDly[2]);
    rvLP2 = d2 + damp * (rvLP2 - d2);
    tR = rvLP2 * decay;
    tR = apf[7].tick (tR, kDiff2);
    dl[3].write (tR);
    rvFdR = dl[3].at (rvFullDly[3]);

    float wetL =   dl[2].at (rvTapL[0]) + dl[2].at (rvTapL[1])
                 - dl[3].at (rvTapL[2]) - dl[3].at (rvTapL[3])
                 - apf[4].at (rvTapL[4]) - apf[4].at (rvTapL[5])
                 - dl[0].at (rvTapL[6])
                 - apf[5].at (rvTapL[7]);

    float wetR =   dl[0].at (rvTapR[0]) + dl[0].at (rvTapR[1])
                 - dl[1].at (rvTapR[2]) - dl[1].at (rvTapR[3])
                 - apf[6].at (rvTapR[4]) - apf[6].at (rvTapR[5])
                 - dl[2].at (rvTapR[6])
                 - apf[7].at (rvTapR[7]);

    wetL = std::tanh (wetL * 0.4f);
    wetR = std::tanh (wetR * 0.4f);

    return { inL + (wetL - inL) * amount,
             inR + (wetR - inR) * amount };
}

//==============================================================================
void GazelleAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Synthesizer: clear output, generate from scratch
    buffer.clear();

    if (buffer.getNumSamples() == 0)
        return;

    //── Read parameters ──────────────────────────────────────────────────────
    const float cutoff       = apvts.getRawParameterValue ("cutoff")       ->load();
    const float resonance    = apvts.getRawParameterValue ("resonance")    ->load();
    const float spread       = apvts.getRawParameterValue ("spread")       ->load();
    const float filterMode   = apvts.getRawParameterValue ("filter_mode")  ->load();
    const float level1       = apvts.getRawParameterValue ("filter1_level")->load();
    const float level2       = apvts.getRawParameterValue ("filter2_level")->load();

    const float attack1Ms    = apvts.getRawParameterValue ("attack1")->load();
    const float decay1Ms     = apvts.getRawParameterValue ("decay1") ->load();
    const float attack2Ms    = apvts.getRawParameterValue ("attack2")->load();
    const float decay2Ms     = apvts.getRawParameterValue ("decay2") ->load();

    const float drive        = apvts.getRawParameterValue ("drive")        ->load();
    const float distAmt      = apvts.getRawParameterValue ("dist_amount")  ->load();
    const float distFb       = apvts.getRawParameterValue ("dist_feedback")->load();
    const bool  fbPath       = apvts.getRawParameterValue ("feedback_path")->load() > 0.5f;
    const float eqTilt       = apvts.getRawParameterValue ("eq_tilt")      ->load();

    const int   fxType       = juce::roundToInt (apvts.getRawParameterValue ("fx_type")->load());
    const float fxP1         = apvts.getRawParameterValue ("fx_p1") ->load();
    const float fxP2         = apvts.getRawParameterValue ("fx_p2") ->load();
    const float fxWet        = apvts.getRawParameterValue ("fx_wet") ->load();

    const bool  trig1        = apvts.getRawParameterValue ("trigger1")->load() > 0.5f;
    const bool  trig2        = apvts.getRawParameterValue ("trigger2")->load() > 0.5f;

    //── MIDI trigger detection ────────────────────────────────────────────────
    for (const auto& m : midiMessages)
    {
        auto msg = m.getMessage();
        if (msg.isNoteOn() && msg.getVelocity() > 0)
        {
            // Note < 60 → trigger path 1; note >= 60 → trigger path 2
            if (msg.getNoteNumber() < 60)
                env1.retrigger();
            else
                env2.retrigger();
        }
    }

    //── Pre-compute envelope coefficients (per block) ────────────────────────
    const float sr = static_cast<float>(currentSampleRate);

    // One-pole time constant: coeff = exp(-1 / (T_seconds * fs))
    auto makeAttackCoeff = [sr] (float ms) {
        return std::exp (-1000.0f / (ms * sr));
    };
    auto makeDecayCoeff = [sr] (float ms) {
        return std::exp (-1000.0f / (ms * sr));
    };

    const float ac1 = makeAttackCoeff (attack1Ms);
    const float dc1 = makeDecayCoeff  (decay1Ms);
    const float ac2 = makeAttackCoeff (attack2Ms);
    const float dc2 = makeDecayCoeff  (decay2Ms);

    //── Pre-compute filter coefficients ──────────────────────────────────────
    // Spread: filter1 shifted up, filter2 shifted down by exp(spread * 0.5)
    float spreadFactor = std::exp (spread * 0.5f);   // 0.61–1.0–1.65 for spread -1→+1
    float cutoff1 = juce::jlimit (20.0f, 20000.0f, cutoff * spreadFactor);
    float cutoff2 = juce::jlimit (20.0f, 20000.0f, cutoff / spreadFactor);
    filter1.updateCoeffs (cutoff1, resonance, currentSampleRate);
    filter2.updateCoeffs (cutoff2, resonance, currentSampleRate);

    //── Distortion drive gain: 1x to 20x ────────────────────────────────────
    const float driveGain = 1.0f + drive * 19.0f;

    //── Output pointers ──────────────────────────────────────────────────────
    const int numOut = buffer.getNumChannels();
    float* outL = numOut > 0 ? buffer.getWritePointer (0) : nullptr;
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    //── Per-sample DSP loop ──────────────────────────────────────────────────
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        // ── Envelopes ─────────────────────────────────────────────────────
        const float env1Level = env1.tick (trig1, ac1, dc1);
        const float env2Level = env2.tick (trig2, ac2, dc2);

        // ── Noise bursts shaped by envelope ───────────────────────────────
        const float noise1 = nextRandom() * env1Level * level1;
        const float noise2 = nextRandom() * env2Level * level2;

        // ── Dual SVF filters ──────────────────────────────────────────────
        const float filt1 = filter1.tick (noise1, filterMode);
        const float filt2 = filter2.tick (noise2, filterMode);

        // ── Sum filter paths (mono before distortion) ─────────────────────
        const float summed = (filt1 + filt2) * 0.5f;

        // ── Add feedback (1-sample delay, causal) ─────────────────────────
        float distIn = summed + feedbackSample * distFb;

        // ── Tilt EQ ───────────────────────────────────────────────────────
        distIn = applyTiltEQ (distIn, eqTilt);

        // ── MOSFET distortion ─────────────────────────────────────────────
        float distOut = distort (distIn, driveGain, distAmt);

        // ── FX engine ─────────────────────────────────────────────────────
        auto [fxL, fxR] = processFX (distOut, distOut, fxType, fxP1, fxP2);

        // ── Update feedback for next sample ───────────────────────────────
        if (fbPath)
            feedbackSample = std::tanh ((fxL + fxR) * 0.5f);   // through FX
        else
            feedbackSample = std::tanh (distOut);               // direct

        // Clamp to prevent blow-up
        feedbackSample = juce::jlimit (-1.0f, 1.0f, feedbackSample);

        // ── FX wet/dry mix ────────────────────────────────────────────────
        const float mixL = distOut + (fxL - distOut) * fxWet;
        const float mixR = distOut + (fxR - distOut) * fxWet;

        // ── Write to output ───────────────────────────────────────────────
        if (outL) outL[i] = juce::jlimit (-1.0f, 1.0f, mixL);
        if (outR) outR[i] = juce::jlimit (-1.0f, 1.0f, mixR);
    }
}

//==============================================================================
const juce::String GazelleAudioProcessor::getName() const { return "Gazelle"; }

//==============================================================================
void GazelleAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GazelleAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
bool GazelleAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* GazelleAudioProcessor::createEditor()
{
    return new GazelleAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GazelleAudioProcessor();
}
