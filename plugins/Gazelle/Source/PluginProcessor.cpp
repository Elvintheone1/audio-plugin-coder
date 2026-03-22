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
    // Cutoff stored as 0-1 internally; C++ maps norm→Hz as 40*125^norm (40–5000 Hz).
    // JS uses the same formula — display always matches the actual filter frequency.
    params.push_back (std::make_unique<Param> ("cutoff",        "Cutoff",
        NRange (0.0f, 1.0f), 0.35f));   // 0.35 ≈ 217 Hz
    params.push_back (std::make_unique<Param> ("resonance",     "Resonance",
        NRange (0.0f, 1.0f), 0.6f));
    params.push_back (std::make_unique<Param> ("spread",        "Spread",
        NRange (0.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<Param> ("filter1_mode",  "LP→HP i",
        NRange (0.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<Param> ("filter2_mode",  "LP→HP ii",
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

    // PITCH SWEEP (bridged-T: how many semitones above target the filter starts on trigger)
    params.push_back (std::make_unique<Param> ("pitch1", "Pitch Sweep I",
        NRange (0.0f, 24.0f), 8.0f));
    params.push_back (std::make_unique<Param> ("pitch2", "Pitch Sweep II",
        NRange (0.0f, 24.0f), 8.0f));

    // SATURATION (single hero knob)
    params.push_back (std::make_unique<Param> ("saturation", "Saturation",
        NRange (0.0f, 1.0f), 0.0f));

    // 3-BAND EQ (Sunn Beta Bass style: -1→-15dB, 0→0dB, 1→+15dB)
    params.push_back (std::make_unique<Param> ("eq_bass",   "Bass",
        NRange (-1.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<Param> ("eq_mid",    "Mid",
        NRange (-1.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<Param> ("eq_treble", "Treble",
        NRange (-1.0f, 1.0f), 0.0f));

    // VCA — output level after saturation
    params.push_back (std::make_unique<Param> ("output_level", "Output",
        NRange (0.0f, 1.0f), 1.0f));

    // FX ENGINE (series: ring mod → tape delay → plate reverb)
    params.push_back (std::make_unique<Param> ("fx_type", "Ring",
        NRange (0.0f, 1.0f), 0.0f));   // ring mod amount: 0=sum only, 1=product only
    params.push_back (std::make_unique<Param> ("fx_p1",   "Delay",
        NRange (0.0f, 1.0f), 0.5f));   // tape delay time
    params.push_back (std::make_unique<Param> ("fx_p2",   "Plate",
        NRange (0.0f, 1.0f), 0.3f));   // plate reverb decay
    params.push_back (std::make_unique<Param> ("fx_wet",  "FX Mix",
        NRange (0.0f, 1.0f), 0.3f));

    // FX — individual controls
    params.push_back (std::make_unique<Param> ("delay_feedback",  "Delay Fdbk",
        NRange (0.0f, 1.0f), 0.35f));
    params.push_back (std::make_unique<Param> ("delay_mix",       "Delay Mix",
        NRange (0.0f, 1.0f), 0.5f));
    params.push_back (std::make_unique<Param> ("plate_predelay",  "Pre-Delay",
        NRange (0.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<Param> ("plate_mix",       "Plate Mix",
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

    // Reset 3-band EQ biquad states and compute initial coefficients
    eqBass.reset();   eqBass.setLowShelf  (100.0f,  0.0f, sampleRate);
    eqMid.reset();    eqMid.setPeakingEQ  (400.0f,  0.8f, 0.0f, sampleRate);
    eqTreble.reset(); eqTreble.setHighShelf (2000.0f, 0.0f, sampleRate);

    // Tape delay buffers
    tapeBufL.assign (kTapeMaxSamples, 0.0f);
    tapeBufR.assign (kTapeMaxSamples, 0.0f);
    tapePosL = tapePosR = 0;
    tapeFilterL = tapeFilterR = 0.0f;
    tapeLfoPhase = 0.0f;

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
    // Cast to int32_t BEFORE float conversion: int32 range [-2^31, +2^31-1]
    // → output ∈ [-1, +1] (bipolar white noise, mean = 0).
    // The old uint32 cast gave [0, 2] (unipolar) which biased the SVF integrators
    // toward +1, triggering the IC clip guard constantly at high resonance.
    return static_cast<float>(static_cast<int32_t>(prngState)) * 4.656612873077393e-10f;
}

//==============================================================================
// Amp-style two-stage saturation — single control knob.
//
// sat 0 → clean bypass.
// sat 0–0.5 → pre-amp warmth: gentle tanh on positive half (tube-like 2nd harmonic).
// sat 0.5–1 → power-amp break-up: exponential drive into asymmetric clip.
//             Positive half: tanh (soft, musical).
//             Negative half: steeper tanh (solid-state transistor push-pull character).
// DC bias (sat²×0.25) introduces asymmetry/even harmonics throughout.
// VCA (output_level param) handles resulting level — no automatic gain compensation.
float GazelleAudioProcessor::distort (float x, float sat) noexcept
{
    if (sat < 1e-4f) return x;

    // preGain 1× (sat=0) → 8× (sat=1) drives the signal into the clip zone.
    // No level normalization: tanh output is naturally bounded ±1.
    // As sat rises: signal compresses toward ±1, adding harmonics and sustain.
    // Asymmetric clip (pos = tanh, neg = harder tanh) gives even harmonics
    // (tube-like 2nd harmonic) without requiring a DC bias term.
    const float preGain = std::pow (8.0f, sat);
    const float driven  = x * preGain;

    if (driven >= 0.0f)
        return std::tanh (driven);
    else
        return std::tanh (driven * 1.3f) / 1.3f;   // harder negative clip
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
        return { 0.0f, 0.0f };
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

    // Return raw wet signal — caller applies its own wet/dry mix via plate_mix param.
    return { wetL, wetR };
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
    // Cutoff: 0-1 norm, piecewise log.
    // 0→0.65  = 20–400 Hz   (65% knob travel, ~4.3 oct — fine resolution for perc).
    // 0.65→1  = 400–15000 Hz (35% knob travel, ~5.2 oct — coarser upper range).
    // Matches JS display formula exactly.
    const float cutoffNorm   = apvts.getRawParameterValue ("cutoff")       ->load();
    const float cutoff       = (cutoffNorm <= 0.65f)
        ? 20.0f  * std::pow (20.0f,  cutoffNorm / 0.65f)
        : 400.0f * std::pow (37.5f,  (cutoffNorm - 0.65f) / 0.35f);
    const float resonance    = apvts.getRawParameterValue ("resonance")    ->load();
    const float spread       = apvts.getRawParameterValue ("spread")       ->load();
    const float filter1Mode  = apvts.getRawParameterValue ("filter1_mode") ->load();
    const float filter2Mode  = apvts.getRawParameterValue ("filter2_mode") ->load();
    const float level1       = apvts.getRawParameterValue ("filter1_level")->load();
    const float level2       = apvts.getRawParameterValue ("filter2_level")->load();

    const float attack1Ms    = apvts.getRawParameterValue ("attack1")->load();
    const float decay1Ms     = apvts.getRawParameterValue ("decay1") ->load();
    const float attack2Ms    = apvts.getRawParameterValue ("attack2")->load();
    const float decay2Ms     = apvts.getRawParameterValue ("decay2") ->load();

    const float sat          = apvts.getRawParameterValue ("saturation")   ->load();
    const float eqBassParam  = apvts.getRawParameterValue ("eq_bass")      ->load();
    const float eqMidParam   = apvts.getRawParameterValue ("eq_mid")       ->load();
    const float eqTrebleParam= apvts.getRawParameterValue ("eq_treble")    ->load();
    const float outputLevel  = apvts.getRawParameterValue ("output_level") ->load();

    //── Update 3-band EQ coefficients (per block, cheap) ─────────────────────
    // Bass: low shelf at 100 Hz  (covers 20–200 Hz fundamental/sub body)
    // Mid:  peaking EQ at 400 Hz (covers 200–800 Hz bark/grind body)
    // Treble: high shelf at 2 kHz (covers 1 kHz+ presence/harmonics)
    constexpr float kEqMaxDb = 15.0f;
    eqBass.setLowShelf   (100.0f,  eqBassParam   * kEqMaxDb, currentSampleRate);
    eqMid.setPeakingEQ   (400.0f,  0.8f, eqMidParam   * kEqMaxDb, currentSampleRate);
    eqTreble.setHighShelf (2000.0f, eqTrebleParam * kEqMaxDb, currentSampleRate);

    const float ringAmount      = apvts.getRawParameterValue ("fx_type")        ->load();
    const float fxP1            = apvts.getRawParameterValue ("fx_p1")          ->load();
    const float fxP2            = apvts.getRawParameterValue ("fx_p2")          ->load();
    const float fxWet           = apvts.getRawParameterValue ("fx_wet")         ->load();
    const float delayFeedback   = apvts.getRawParameterValue ("delay_feedback") ->load();
    const float delayMix        = apvts.getRawParameterValue ("delay_mix")      ->load();
    const float platePre        = apvts.getRawParameterValue ("plate_predelay") ->load();
    const float plateMix        = apvts.getRawParameterValue ("plate_mix")      ->load();

    const bool  trig1        = apvts.getRawParameterValue ("trigger1")->load() > 0.5f;
    const bool  trig2        = apvts.getRawParameterValue ("trigger2")->load() > 0.5f;

    const float pitchSweep1  = apvts.getRawParameterValue ("pitch1")->load();
    const float pitchSweep2  = apvts.getRawParameterValue ("pitch2")->load();

    //── MIDI trigger detection ────────────────────────────────────────────────
    for (const auto& m : midiMessages)
    {
        auto msg = m.getMessage();
        if (msg.isNoteOn() && msg.getVelocity() > 0)
        {
            // Note < 60 → trigger path 1; note >= 60 → trigger path 2
            if (msg.getNoteNumber() < 60)
            {
                env1.retrigger();
                pitch1State = pitchSweep1;
            }
            else
            {
                env2.retrigger();
                pitch2State = pitchSweep2;
            }
        }
    }

    //── Block-level rising-edge detection for button triggers ────────────────
    if (trig1 && !lastTrig1) pitch1State = pitchSweep1;
    if (trig2 && !lastTrig2) pitch2State = pitchSweep2;
    lastTrig1 = trig1;
    lastTrig2 = trig2;

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

    //── Pre-compute spread cutoffs ────────────────────────────────────────────
    // Spread: semitone-based, unipolar [0,1].
    // Minimum 3-semitone total detuning (±1.5 semi) baked in at spread=0.
    // This guarantees the two filters are NEVER in unison so filt1×filt2 always
    // produces inharmonic sidebands rather than a clean octave (filt1² = f+2f).
    // At spread=1: ±12 semitones per side (full octave apart each way).
    const float halfSpreadSemi  = 1.5f + spread * 10.5f;   // [1.5, 12.0] semitones per side
    const float spreadFactor    = std::pow (2.0f, halfSpreadSemi / 12.0f);
    const float cutoff1Base = juce::jlimit (20.0f, 15000.0f, cutoff / spreadFactor);  // i  — lower
    const float cutoff2Base = juce::jlimit (20.0f, 15000.0f, cutoff * spreadFactor);  // ii — higher

    //── Pitch sweep decay coefficients (bridged-T behaviour) ─────────────────
    // Pitch settles to the target cutoff ~10× faster than the amplitude decay.
    // Minimum 3 ms to prevent aliasing on very short decays.
    const float pitchDecay1Ms = std::min (80.0f, std::max (3.0f, decay1Ms * 0.05f));
    const float pitchDecay2Ms = std::min (80.0f, std::max (3.0f, decay2Ms * 0.05f));
    const float pitchDc1 = std::exp (-1000.0f / (pitchDecay1Ms * sr));
    const float pitchDc2 = std::exp (-1000.0f / (pitchDecay2Ms * sr));

    // Filter coefficients are updated per-sample (pitch-swept).

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

        // ── Resonance smoothing (one-pole, ~11ms τ at 44.1kHz) ───────────
        // Smooths coefficient changes per-sample so abrupt knob turns don't
        // cause discontinuities in the filter's integrator states → no crackle.
        smoothedResonance += 0.002f * (resonance - smoothedResonance);

        // ── Bridged-T pitch sweep: decay semitone offset toward 0 ────────
        // On trigger, pitch1State = pitchSweep1 semitones above target.
        // Decays ~10× faster than amplitude → filter sweeps down to target.
        // This is the elastic "boing" character of Bridged-T percussion.
        pitch1State *= pitchDc1;
        pitch2State *= pitchDc2;

        // ── Envelope → cutoff modulation (scaled by pitch sweep amount) ──
        // Envelope pushes cutoff up on trigger — but only when sweep > 0.
        // sweepNorm=0 → no pitch movement at all; sweepNorm=1 → up to +1 oct.
        // At sweep=0 the filter stays locked on the cutoff knob frequency.
        const float sweepNorm1    = pitchSweep1 / 24.0f;
        const float sweepNorm2    = pitchSweep2 / 24.0f;
        const float envCutoffMod1 = 1.0f + env1Level * sweepNorm1;
        const float envCutoffMod2 = 1.0f + env2Level * sweepNorm2;
        const float instCutoff1 = cutoff1Base * std::pow (2.0f, pitch1State / 12.0f) * envCutoffMod1;
        const float instCutoff2 = cutoff2Base * std::pow (2.0f, pitch2State / 12.0f) * envCutoffMod2;
        filter1.updateCoeffs (instCutoff1, smoothedResonance, currentSampleRate);
        filter2.updateCoeffs (instCutoff2, smoothedResonance, currentSampleRate);

        // ── Dual SVF filters ──────────────────────────────────────────────
        const float filt1 = filter1.tick (noise1, filter1Mode);
        const float filt2 = filter2.tick (noise2, filter2Mode);

        // ── Ring mod blend ────────────────────────────────────────────────
        // directSum:   (filt1+filt2)/2 — normal parallel mix, range ±1
        // ringProduct: filt1×filt2     — creates sum (f1+f2) and difference
        //              (f1−f2) sidebands.  At spread=0 (min 3 semi) this is
        //              always inharmonic; at large spread gives metallic clang.
        //              Product of two ±1 signals is bounded ±1 — same range.
        // ringAmount=0 → pure sum; ringAmount=1 → pure product; blend between.
        const float directSum   = (filt1 + filt2) * 0.5f;
        const float ringProduct =  filt1 * filt2;
        float sig = directSum * (1.0f - ringAmount) + ringProduct * ringAmount;

        // ── 3-band EQ (Bass shelf → Mid peak → Treble shelf) ─────────────
        sig = eqBass.tick (sig);
        sig = eqMid.tick  (sig);
        sig = eqTreble.tick (sig);

        // ── Amp saturation ────────────────────────────────────────────────
        const float distOut = distort (sig, sat);

        // ── VCA — output level after saturation ───────────────────────────
        const float vcaSig = distOut * outputLevel;

        // ── FX engine: tape delay → plate (series) ────────────────────────
        // 1. Tape delay: fxP1 = time, delayFeedback = feedback amount
        auto [dlWetL, dlWetR] = processTapeDelay (vcaSig, vcaSig, fxP1, delayFeedback);
        const float afterDelL = vcaSig + (dlWetL - vcaSig) * delayMix;
        const float afterDelR = vcaSig + (dlWetR - vcaSig) * delayMix;

        // 2. Plate reverb: platePre = pre-delay, fxP2 = decay; returns raw wet
        auto [plWetL, plWetR] = processVintagePlate (afterDelL, afterDelR, platePre, fxP2);
        const float fxL = afterDelL + (plWetL - afterDelL) * plateMix;
        const float fxR = afterDelR + (plWetR - afterDelR) * plateMix;

        // 3. Master FX wet/dry — blends full FX chain against dry VCA signal
        const float mixL = vcaSig + (fxL - vcaSig) * fxWet;
        const float mixR = vcaSig + (fxR - vcaSig) * fxWet;

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
