#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SpliceAudioProcessor::SpliceAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    , apvts (*this, nullptr, "SPLICE_STATE", createParameterLayout())
{
    sliceActive.fill (true);

    // Default lane values: pos=spread, vel=full, pitch=center, dir=fwd
    for (size_t step = 0; step < 16; ++step)
    {
        laneValues[0][step] = static_cast<float> (step) / 15.0f;
        laneValues[1][step] = 0.75f;
        laneValues[2][step] = 0.5f;
        laneValues[3][step] = 0.9f;
    }

    // Persistent settings
    juce::PropertiesFile::Options opts;
    opts.applicationName     = "Splice";
    opts.filenameSuffix      = "settings";
    opts.osxLibrarySubFolder = "Application Support";
    opts.folderName          = "Noizefield/Splice";
    appProperties.setStorageParameters (opts);
}

SpliceAudioProcessor::~SpliceAudioProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SpliceAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ── Group 1: Global ───────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "playback_mode", "Mode",
        juce::StringArray { "poly", "slice", "arp", "seq" }, 0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "grid", "Grid",
        juce::StringArray { "1", "2", "4", "8", "16" }, 2));  // default = "4"

    // ── Group 2: Direction ────────────────────────────────
    juce::StringArray dirs { "fwd", "rev", "pingpong", "random" };
    layout.add (std::make_unique<juce::AudioParameterChoice> ("reel_dir",  "Reel Dir",  dirs, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("slice_dir", "Slice Dir", dirs, 0));

    // ── Group 3: Amp Envelope ─────────────────────────────
    // True log scaling: value = start × (end/start)^norm
    // Gives equal knob travel per decade — fine resolution 1-100ms, coarser above.
    auto logFrom = [] (float start, float end, float norm)
        { return start * std::pow (end / start, norm); };
    auto logTo   = [] (float start, float end, float value)
        { return std::log (value / start) / std::log (end / start); };

    auto atkRange = juce::NormalisableRange<float> (0.0005f, 0.3f, logFrom, logTo); // 0.5–300 ms
    auto relRange = juce::NormalisableRange<float> (0.001f, 1.0f, logFrom, logTo); // 1 ms–1 s
    auto linRange = juce::NormalisableRange<float> (0.0f, 1.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> ("amp_attack",  "Attack",  linRange,  0.02f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("amp_decay",   "Decay",   linRange,  0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("amp_sustain", "Sustain", linRange,  1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("amp_release", "Release", relRange,  0.08f));

    // ── Group 4: Filter ───────────────────────────────────
    auto cutoffRange = juce::NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f);
    layout.add (std::make_unique<juce::AudioParameterFloat>  ("filter_cutoff", "Cutoff",  cutoffRange, 18000.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>  ("filter_res",    "Res",     linRange,    0.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("filter_type",   "Filter Type",
                                                               juce::StringArray { "LP", "BP", "HP" }, 0));

    // ── Group 5: Filter Envelope ──────────────────────────
    auto bipolarRange = juce::NormalisableRange<float> (-1.0f, 1.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fenv_depth",  "FEnv Depth",  bipolarRange, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fenv_attack", "FEnv Attack", linRange, 0.05f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fenv_decay",  "FEnv Decay",  linRange, 0.50f));

    // ── Group 5b: Envelope Shapes ─────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> ("amp_attack_shape",  "Amp Atk Shape",  bipolarRange, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("amp_decay_shape",   "Amp Dec Shape",  bipolarRange, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("amp_release_shape", "Amp Rel Shape",  bipolarRange, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fenv_attack_shape", "FEnv Atk Shape", bipolarRange, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fenv_decay_shape",  "FEnv Dec Shape", bipolarRange, 0.0f));

    // ── Group 6: ARP ──────────────────────────────────────
    juce::StringArray arpRates { "1/4", "1/8", "1/16", "1/32" };
    layout.add (std::make_unique<juce::AudioParameterChoice> ("arp_rate",    "Arp Rate",    arpRates, 2));
    layout.add (std::make_unique<juce::AudioParameterBool>   ("arp_hold",    "Hold",        false));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("arp_pattern", "Arp Pattern",
                                                               juce::StringArray { "up", "down", "updown", "as-played", "random" }, 0));

    // ── Group 7: Mod Matrix ───────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterChoice> ("lfo_shape", "LFO Shape",
                                                               juce::StringArray { "sine", "tri", "square", "saw", "random" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat>  ("lfo_rate",  "LFO Rate",  juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.5f), 1.0f));
    layout.add (std::make_unique<juce::AudioParameterBool>   ("lfo_sync",  "LFO Sync",  true));
    layout.add (std::make_unique<juce::AudioParameterFloat>  ("lfo_depth", "LFO Depth", linRange, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterInt>    ("cc_number", "CC Number", 0, 127, 1));
    layout.add (std::make_unique<juce::AudioParameterFloat>  ("cc_depth",  "CC Depth",  linRange, 0.0f));

    // ── Group 8: SEQ Global ───────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterChoice> ("seq_step_length", "SEQ Step Length",
                                                               juce::StringArray { "1/4", "1/8", "1/16", "1/32" }, 2));

    // ── Group 9: SEQ Lanes (×4) ───────────────────────────
    juce::StringArray traversalPatterns { "forward", "reverse", "snake", "diagonal", "random", "drunk" };
    const char* laneNames[] = { "pos", "vel", "pitch", "dir" };
    for (auto lane : laneNames)
    {
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::String ("lane_") + lane + "_steps",
            juce::String ("Lane ") + lane + " Steps", 1, 16, 16));
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::String ("lane_") + lane + "_pattern",
            juce::String ("Lane ") + lane + " Pattern",
            traversalPatterns, 2));  // default = snake
    }

    // ── Group 10: Output ──────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> ("output_vol", "Volume", linRange, 0.75f));

    // ── Group 11: Signal chain ────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> ("vol_db",  "Vol dB",
        juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("pitch", "Pitch",
        juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("root_note", "Root Note", 0, 127, 60));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("speed", "Speed",
        juce::NormalisableRange<float> (0.0f, 8.0f, 1.0f), 2.0f));  // 9 steps: 0=/3 … 8=x16, default=2=x1
    layout.add (std::make_unique<juce::AudioParameterFloat> ("pan",     "Pan",
        juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("pan_rnd", "Pan Rnd", linRange, 0.0f));

    // ── Group 12: Global EQ (±12 dB shelves + peaks) ────
    layout.add (std::make_unique<juce::AudioParameterFloat> ("eq_low",      "EQ Low",      juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("eq_low_mid",  "EQ Low Mid",  juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("eq_high_mid", "EQ High Mid", juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("eq_high",     "EQ High",     juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f));

    // ── Group 13: Randomization depths ───────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> ("rand_oct",    "Rand Oct",    linRange, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("rand_fifth",  "Rand Fifth",  linRange, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("rand_cutoff", "Rand Cutoff", linRange, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("rand_env",    "Rand Env",    linRange, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("rand_vol",     "Rand Vol",     linRange, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("rand_jitter",  "Rand Jitter",  linRange, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("rand_stutter", "Rand Stutter", linRange, 0.0f));

    // ── Group 12: Tape ────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> ("tape_age",    "Tape Age",    linRange, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("wow_flutter", "Wow Flutter", linRange, 0.0f));

    // ── Group 11: Tempo ───────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> ("bpm", "BPM",
        juce::NormalisableRange<float> (60.0f, 200.0f), 120.0f));

    return layout;
}

//==============================================================================
const juce::String SpliceAudioProcessor::getName() const { return JucePlugin_Name; }
bool SpliceAudioProcessor::acceptsMidi()  const { return true; }
bool SpliceAudioProcessor::producesMidi() const { return false; }
bool SpliceAudioProcessor::isMidiEffect() const { return false; }
double SpliceAudioProcessor::getTailLengthSeconds() const { return 2.0; }

int SpliceAudioProcessor::getNumPrograms()    { return 1; }
int SpliceAudioProcessor::getCurrentProgram() { return 0; }
void SpliceAudioProcessor::setCurrentProgram (int) {}
const juce::String SpliceAudioProcessor::getProgramName (int) { return {}; }
void SpliceAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void SpliceAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    smoothedOutputVol.reset (sampleRate, 0.05);  // 50 ms ramp
    smoothedOutputVol.setCurrentAndTargetValue (apvts.getRawParameterValue ("output_vol")->load());

    eqLowSmooth    .reset (sampleRate, 0.05);
    eqLowMidSmooth .reset (sampleRate, 0.05);
    eqHighMidSmooth.reset (sampleRate, 0.05);
    eqHighSmooth   .reset (sampleRate, 0.05);
    eqLowSmooth    .setCurrentAndTargetValue (apvts.getRawParameterValue ("eq_low")     ->load());
    eqLowMidSmooth .setCurrentAndTargetValue (apvts.getRawParameterValue ("eq_low_mid") ->load());
    eqHighMidSmooth.setCurrentAndTargetValue (apvts.getRawParameterValue ("eq_high_mid")->load());
    eqHighSmooth   .setCurrentAndTargetValue (apvts.getRawParameterValue ("eq_high")    ->load());

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 2;
    eqChain.prepare (spec);

    // Prepare per-voice ladder filters (mono, one per channel)
    juce::dsp::ProcessSpec monoSpec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 1 };
    for (auto& v : voices)
    {
        v.ladderL.prepare (monoSpec);
        v.ladderR.prepare (monoSpec);
    }

    // Tape / Wow&Flutter reset
    tapeLP_L = tapeLP_R = crackleEnv = crackleDecay = crackleAmp = 0.0f;
    wfBufL.fill (0.0f);
    wfBufR.fill (0.0f);
    wfWritePos = 0;
    wfPhaseWow = wfPhaseFlutter = 0.0;
}

void SpliceAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SpliceAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

//==============================================================================
// Voice helpers

int SpliceAudioProcessor::getGridValue() const
{
    int idx = static_cast<int> (apvts.getRawParameterValue ("grid")->load());
    const int vals[] = { 1, 2, 4, 8, 16 };
    return vals[juce::jlimit (0, 4, idx)];
}

std::pair<int, int> SpliceAudioProcessor::getSliceRange (int sliceIdx) const
{
    const int totalSamples = reelBuffer.getNumSamples();
    if (totalSamples == 0) return { 0, 0 };

    const int gridVal     = getGridValue();
    const int totalSlices = 4 * gridVal;
    sliceIdx = juce::jlimit (0, totalSlices - 1, sliceIdx);

    const int sliceLen = totalSamples / totalSlices;
    const int start    = sliceIdx * sliceLen;
    const int end      = std::min (start + sliceLen, totalSamples);
    return { start, end };
}

// Two-segment envelope time mapping:
//   0–40% knob  → 0.5ms–40ms absolute (linear)
//   40–100% knob → 40ms–95% of slice duration (linear)
static float normToEnvTime (float norm, float sliceDur) noexcept
{
    const float kAbsMin = 0.0005f;   // 0.5ms
    const float kAbsMax = 0.040f;    // 40ms
    const float kBreak  = 0.4f;
    if (norm <= kBreak)
        return kAbsMin + (norm / kBreak) * (kAbsMax - kAbsMin);
    const float t = (norm - kBreak) / (1.0f - kBreak);
    return kAbsMax + t * (juce::jmax (kAbsMax, 0.95f * sliceDur) - kAbsMax);
}

void SpliceAudioProcessor::allocateVoice (int midiNote, float velocity, int sliceIdx,
                                          float atk, float dec, float sus,
                                          int dirOverride, float extraPitchSt)
{
    if (reelBuffer.getNumSamples() == 0) return;

    const int sliceDirIdx = (dirOverride >= 0)
        ? dirOverride
        : static_cast<int> (apvts.getRawParameterValue ("slice_dir")->load());

    // Voice stealing priority: idle → oldest releasing → oldest sustaining
    int victimIdx = -1;

    // 1. Find a free (idle) voice
    for (int i = 0; i < kNumVoices; ++i)
        if (! voices[i].active) { victimIdx = i; break; }

    // 2. Steal the oldest releasing voice (lowest age = allocated first = furthest in release)
    if (victimIdx < 0)
    {
        int minAge = INT_MAX;
        for (int i = 0; i < kNumVoices; ++i)
            if (voices[i].active && ! voices[i].gateOpen && voices[i].age < minAge)
                { minAge = voices[i].age; victimIdx = i; }
    }

    // 3. Last resort: steal the oldest sustaining voice
    if (victimIdx < 0)
    {
        int minAge = INT_MAX;
        for (int i = 0; i < kNumVoices; ++i)
            if (voices[i].active && voices[i].age < minAge)
                { minAge = voices[i].age; victimIdx = i; }
    }

    if (victimIdx < 0) victimIdx = 0; // should never happen with 32 voices

    const float panCenter = apvts.getRawParameterValue ("pan")    ->load();
    const float panRnd    = apvts.getRawParameterValue ("pan_rnd")->load();
    const float rndOffset = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) * panRnd;

    auto& v              = voices[victimIdx];
    v.active             = true;
    v.gateOpen           = true;
    v.pan                = juce::jlimit (-1.0f, 1.0f, panCenter + rndOffset);
    // ── Randomization depths ─────────────────────────────────────────────────
    auto rnd = [] { return juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f; }; // [-1,1]
    const float randOctDepth    = apvts.getRawParameterValue ("rand_oct")   ->load();
    const float randFifthDepth  = apvts.getRawParameterValue ("rand_fifth") ->load();
    const float randCutoffDepth = apvts.getRawParameterValue ("rand_cutoff")->load();
    const float randEnvDepth    = apvts.getRawParameterValue ("rand_env")   ->load();
    const float randVolDepth    = apvts.getRawParameterValue ("rand_vol")   ->load();

    // Oct: discrete octave shift — depth controls max spread (0→1 oct, 0.33→1, 0.67→2, 1→3)
    {
        const int maxOct = juce::jlimit (0, 3, static_cast<int> (std::ceil (randOctDepth * 3.0f)));
        if (maxOct > 0)
        {
            const int octShift = static_cast<int> (std::floor (
                (juce::Random::getSystemRandom().nextFloat() * (2 * maxOct + 1)))) - maxOct;
            extraPitchSt += static_cast<float> (octShift * 12);
        }
    }

    // 5ths: depth = probability of applying ±7 semitone shift
    if (juce::Random::getSystemRandom().nextFloat() < randFifthDepth)
        extraPitchSt += (juce::Random::getSystemRandom().nextFloat() < 0.5f) ? 7.0f : -7.0f;

    // Cutoff: bipolar ±3 octaves (log) at depth=1
    const float baseCutoff = apvts.getRawParameterValue ("filter_cutoff")->load();
    v.filterCutoff = juce::jlimit (20.0f, 20000.0f,
                         baseCutoff * std::pow (2.0f, rnd() * randCutoffDepth * 3.0f));

    v.filterRes          = apvts.getRawParameterValue ("filter_res")->load();
    v.ladderL.reset();
    v.ladderR.reset();
    v.sliceEnded         = false;
    v.midiNote           = midiNote;
    v.currentSliceIdx    = sliceIdx;
    v.pingpongDir        = 1;
    v.velocity           = velocity;
    v.age                = voiceAge++;
    v.slicePhase         = 0.0;

    // Vol: bipolar ±50% at depth=1 (clamped 0.1..2.0 to prevent silence/clipping)
    v.volRandMult = juce::jlimit (0.1f, 2.0f, 1.0f + rnd() * randVolDepth * 0.5f);

    setupSlicePlayback (v, sliceDirIdx, extraPitchSt);

    // Convert normalized time params (0-1) → actual seconds using slice duration
    const float sliceDur = (v.endSample - v.startSample) / (float) currentSampleRate;

    // ENV: bipolar randomization on atk/dec norms before time conversion
    const float envRnd = rnd() * randEnvDepth * 0.4f;
    const float atkRnd = juce::jlimit (0.0f, 1.0f, atk + rnd() * randEnvDepth * 0.4f);
    const float decRnd = juce::jlimit (0.0f, 1.0f, dec + rnd() * randEnvDepth * 0.4f);
    const float atkSec = normToEnvTime (atkRnd, sliceDur);
    const float decSec = normToEnvTime (decRnd, sliceDur);
    (void) envRnd;

    v.ampEnv.noteOn (atkSec, decSec, sus, currentSampleRate);
    v.ampEnv.attackShape  = apvts.getRawParameterValue ("amp_attack_shape") ->load();
    v.ampEnv.decayShape   = apvts.getRawParameterValue ("amp_decay_shape")  ->load();
    v.ampEnv.releaseShape = apvts.getRawParameterValue ("amp_release_shape")->load();

    // Filter envelope — AD shape (sustain=0, decays fully back to base cutoff)
    const float fenvAtkNorm = apvts.getRawParameterValue ("fenv_attack")->load();
    const float fenvDecNorm = apvts.getRawParameterValue ("fenv_decay") ->load();
    const float fenvAtkSec  = normToEnvTime (fenvAtkNorm, sliceDur);
    const float fenvDecSec  = normToEnvTime (fenvDecNorm, sliceDur);
    v.fenvEnv.noteOn (fenvAtkSec, fenvDecSec, 0.0f, currentSampleRate);
    v.fenvEnv.attackShape = apvts.getRawParameterValue ("fenv_attack_shape")->load();
    v.fenvEnv.decayShape  = apvts.getRawParameterValue ("fenv_decay_shape") ->load();

    // Stutter: mute this slice (flag suppresses output; envelope + BPM clock keep running)
    const float stutterProb = apvts.getRawParameterValue ("rand_stutter")->load();
    v.stuttered = (stutterProb > 0.0f && juce::Random::getSystemRandom().nextFloat() < stutterProb);
}

void SpliceAudioProcessor::releaseVoice (int midiNote, float rel)
{
    for (int i = 0; i < kNumVoices; ++i)
        if (voices[i].active && voices[i].midiNote == midiNote)
        {
            voices[i].gateOpen = false;
            voices[i].ampEnv.noteOff (rel, currentSampleRate);
        }
}

//==============================================================================
void SpliceAudioProcessor::setupSlicePlayback (SpliceVoice& v, int sliceDirIdx,
                                                float extraPitchSt)
{
    auto [start, end] = getSliceRange (v.currentSliceIdx);
    if (end <= start) return;

    const float globalPitch  = apvts.getRawParameterValue ("pitch")->load();
    const int   rootNote     = static_cast<int> (apvts.getRawParameterValue ("root_note")->load());
    const int   modeForPitch = static_cast<int> (apvts.getRawParameterValue ("playback_mode")->load());
    const float chromatic    = (modeForPitch == 1 || modeForPitch == 2 || modeForPitch == 3) ? 0.0f
                                    : static_cast<float> (v.midiNote - rootNote);
    const float totalPitchSt = globalPitch + chromatic + extraPitchSt;
    const bool  reverse      = (sliceDirIdx == 1);

    // Rate-based pitch
    v.startSample = start;
    v.endSample   = end;
    const float pitchFactor = std::pow (2.0f, totalPitchSt / 12.0f);
    const float baseRate    = static_cast<float> (reelSampleRate / currentSampleRate) * pitchFactor;

    if (reverse)
    {
        v.playRate = -baseRate;
        v.playhead = static_cast<float> (end - 1);
    }
    else
    {
        v.playRate = baseRate;
        v.playhead = static_cast<float> (start);
    }
}

int SpliceAudioProcessor::advanceSeqSlice (int currentIdx, int& pingDir,
                                            int reelDirIdx, int totalSlices) const
{
    // Advance one step per reel direction, skipping muted slices.
    // Safety cap = totalSlices so we don't spin forever if every slice is muted.
    int idx = currentIdx;
    for (int attempts = 0; attempts < totalSlices; ++attempts)
    {
        switch (reelDirIdx)
        {
            case 0: idx = (idx + 1) % totalSlices; break;
            case 1: idx = (idx - 1 + totalSlices) % totalSlices; break;
            case 2:
                idx += pingDir;
                if (idx >= totalSlices - 1 || idx <= 0) pingDir = -pingDir;
                idx = juce::jlimit (0, totalSlices - 1, idx);
                break;
            case 3: idx = juce::Random::getSystemRandom().nextInt (totalSlices); break;
            default: break;
        }
        if (idx < 64 && sliceActive[static_cast<size_t> (idx)]) break;
    }
    return idx;
}

void SpliceAudioProcessor::advanceToNextSlice (SpliceVoice& v, int reelDirIdx,
                                                int sliceDirIdx, int totalSlices)
{
    v.currentSliceIdx = advanceSeqSlice (v.currentSliceIdx, v.pingpongDir, reelDirIdx, totalSlices);
    setupSlicePlayback (v, sliceDirIdx);
}

//==============================================================================
void SpliceAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0) return;

    // Reset slice selections when grid changes
    {
        const int currentGridVal = getGridValue();
        if (currentGridVal != lastGridVal)
        {
            lastGridVal = currentGridVal;
            sliceActive.fill (true);
        }
    }

    // Hold toggle: when hold turns OFF, release all voices whose gate we were holding open
    const bool holdActive = apvts.getRawParameterValue ("arp_hold")->load() > 0.5f;
    if (lastHoldState && ! holdActive)
    {
        for (int i = 0; i < kNumVoices; ++i)
            if (voices[i].active && voices[i].gateOpen)
            {
                voices[i].gateOpen = false;
                voices[i].ampEnv.noteOff (apvts.getRawParameterValue ("amp_release")->load(),
                                          currentSampleRate);
            }
        polyGateOpen      = false;
        polyHeldNoteCount = 0;
        arpNoteCount      = 0;
        arpPhase          = 0.0;
    }
    lastHoldState = holdActive;

    // Stuck-note guard: if the Poly gate claims open but all voices are idle,
    // a note-off was missed — reset so the next key press works.
    if (polyGateOpen)
    {
        bool anyActive = false;
        for (int i = 0; i < kNumVoices && ! anyActive; ++i)
            anyActive = voices[i].active;
        if (! anyActive)
        {
            polyGateOpen      = false;
            polyHeldNoteCount = 0;
        }
    }

    // Clear output (we accumulate from voices)
    buffer.clear();

    // Read envelope + output params once per block
    const float atkS = apvts.getRawParameterValue ("amp_attack") ->load();
    const float decS = apvts.getRawParameterValue ("amp_decay")  ->load();
    const float sus  = apvts.getRawParameterValue ("amp_sustain")->load();
    const float relS = apvts.getRawParameterValue ("amp_release")->load();
    const int modeIdx = static_cast<int> (apvts.getRawParameterValue ("playback_mode")->load());

    // Reset SEQ phase + step counters when not in SEQ mode.
    // This aligns all lanes to step 0 on every disable, so re-enabling
    // always starts from a clean, synced state (traditional sequencer reset).
    if (modeIdx != 3)
    {
        seqPhase.fill (0.0);
        seqStep.fill  (0);
    }

    const float volDbFactor = std::pow (10.0f, apvts.getRawParameterValue ("vol_db")->load() / 20.0f);
    smoothedOutputVol.setTargetValue (apvts.getRawParameterValue ("output_vol")->load() * volDbFactor);

    // ── MIDI handling ────────────────────────────────────────────────────────
    if (reelBuffer.getNumSamples() > 0)
    {
        const int gridVal     = getGridValue();
        const int totalSlices = 4 * gridVal;
        const int reelDirMidi = static_cast<int> (apvts.getRawParameterValue ("reel_dir")->load());

        for (const auto meta : midiMessages)
        {
            const auto msg = meta.getMessage();

            if (msg.isNoteOn() && msg.getVelocity() > 0)
            {
                const int   note = msg.getNoteNumber();
                const float vel  = msg.getFloatVelocity();

                if (modeIdx == 0) // ── POLY: each key spawns its own voice stream ──
                {
                    polyMidiNote = note;
                    polyVelocity = vel;

                    if (! polyGateOpen)
                        polyGateOpen = true;
                    else
                        polyHeldNoteCount++;

                    // Every key press starts a fresh voice from the first active slice
                    int startSlice = 0;
                    switch (reelDirMidi)
                    {
                        case 1: startSlice = totalSlices - 1; break;
                        case 3: startSlice = juce::Random::getSystemRandom().nextInt (totalSlices); break;
                        default: startSlice = 0; break;
                    }
                    if (startSlice < 64 && ! sliceActive[static_cast<size_t> (startSlice)])
                        startSlice = advanceSeqSlice (startSlice, polySeqPingDir, reelDirMidi, totalSlices);

                    if (startSlice < 64 && sliceActive[static_cast<size_t> (startSlice)])
                        allocateVoice (note, vel, startSlice, atkS, decS, sus);
                }
                else if (modeIdx == 1) // ── SLICE: each key = one slice, one-shot ──
                {
                    const int sliceIdx = juce::jlimit (0, totalSlices - 1, note - 24);
                    if (sliceIdx < 64 && sliceActive[static_cast<size_t> (sliceIdx)])
                        allocateVoice (note, vel, sliceIdx, atkS, decS, sus);
                }
                else if (modeIdx == 2) // ── ARP: add note to held list ────────────
                {
                    // Avoid duplicates
                    bool found = false;
                    for (int n = 0; n < arpNoteCount; ++n)
                        if (arpHeldNotes[n] == note) { found = true; break; }
                    if (! found && arpNoteCount < kMaxArpNotes)
                    {
                        arpHeldNotes[arpNoteCount] = note;
                        arpHeldVels [arpNoteCount] = vel;
                        ++arpNoteCount;
                    }
                    // Reset phase on first note so playback starts promptly
                    if (arpNoteCount == 1)
                    {
                        arpPhase   = 0.0;
                        arpSeqIdx  = 0;
                        arpPingDir = 1;
                    }
                }
            }
            else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
            {
                if (modeIdx == 0) // ── POLY note-off ──────────────────────────
                {
                    if (holdActive)
                    {
                        // Hold is active: ignore key-up, keep voice running
                    }
                    else
                    {
                        releaseVoice (msg.getNoteNumber(), relS);

                        if (polyHeldNoteCount > 0)
                            polyHeldNoteCount--;
                        else
                            polyGateOpen = false;
                    }
                }
                else if (modeIdx == 1)
                {
                    releaseVoice (msg.getNoteNumber(), relS);
                }
                else if (modeIdx == 2) // ── ARP note-off ──────────────────────────
                {
                    if (! holdActive)
                    {
                        const int offNote = msg.getNoteNumber();
                        for (int n = 0; n < arpNoteCount; ++n)
                        {
                            if (arpHeldNotes[n] == offNote)
                            {
                                // Compact the array
                                for (int k = n; k < arpNoteCount - 1; ++k)
                                {
                                    arpHeldNotes[k] = arpHeldNotes[k + 1];
                                    arpHeldVels [k] = arpHeldVels [k + 1];
                                }
                                --arpNoteCount;
                                // Keep seqIdx in bounds
                                if (arpNoteCount > 0)
                                    arpSeqIdx = arpSeqIdx % arpNoteCount;
                                else
                                    arpPhase = 0.0;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Voice rendering ──────────────────────────────────────────────────────
    if (reelBuffer.getNumSamples() > 0 && numChannels > 0)
    {
        const float* reelL   = reelBuffer.getReadPointer (0);
        const float* reelR   = (reelBuffer.getNumChannels() > 1)
                               ? reelBuffer.getReadPointer (1) : reelL;
        const int    reelMax = reelBuffer.getNumSamples() - 2;

        float* outL = buffer.getWritePointer (0);
        float* outR = (numChannels > 1) ? buffer.getWritePointer (1) : outL;

        // BPM clock parameters
        const float  bpm          = apvts.getRawParameterValue ("bpm")->load();
        static constexpr float kSpeedTable[] = { 1.0f/3.0f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 8.0f, 16.0f };
        const int    speedIdx     = juce::jlimit (0, 8, static_cast<int> (apvts.getRawParameterValue ("speed")->load()));
        const float  speedFactor  = kSpeedTable[speedIdx];
        const int    gridVal      = getGridValue();
        const int    totalSlices  = 4 * gridVal;
        const double sampPerSlice = (60.0 * 4.0 / (bpm * speedFactor * gridVal)) * currentSampleRate;
        const double slicePhaseInc= 1.0 / sampPerSlice;

        const int reelDirIdx  = static_cast<int> (apvts.getRawParameterValue ("reel_dir")->load());

        // Ladder filter type and fenv parameters (read once per block, applied per-sample)
        const int   filterTypeIdx  = static_cast<int> (apvts.getRawParameterValue ("filter_type")->load());
        const float filterCutoffHz = apvts.getRawParameterValue ("filter_cutoff")->load();
        const float filterResVal   = apvts.getRawParameterValue ("filter_res")   ->load();
        const float fenvDepth      = apvts.getRawParameterValue ("fenv_depth")->load();

        // Map filter type to LadderFilterMode and push to all active voices
        static constexpr juce::dsp::LadderFilterMode kLadderModes[] = {
            juce::dsp::LadderFilterMode::LPF24,
            juce::dsp::LadderFilterMode::BPF24,
            juce::dsp::LadderFilterMode::HPF24
        };
        const auto ladderMode = kLadderModes[juce::jlimit (0, 2, filterTypeIdx)];
        for (auto& v : voices)
            if (v.active)
            {
                v.ladderL.setMode (ladderMode);
                v.ladderR.setMode (ladderMode);
            }
        const float fenvAtkBlock   = apvts.getRawParameterValue ("fenv_attack")->load();
        const float fenvDecBlock   = apvts.getRawParameterValue ("fenv_decay") ->load();
        const int   sliceDirIdx    = static_cast<int> (apvts.getRawParameterValue ("slice_dir")->load());

        // Randomization depths (read once per block, applied at each slice trigger)
        const float rndOctDepth    = apvts.getRawParameterValue ("rand_oct")   ->load();
        const float rndFifthDepth  = apvts.getRawParameterValue ("rand_fifth") ->load();
        const float rndCutoffDepth = apvts.getRawParameterValue ("rand_cutoff")->load();
        const float rndEnvDepth    = apvts.getRawParameterValue ("rand_env")   ->load();
        const float rndVolDepth     = apvts.getRawParameterValue ("rand_vol")    ->load();
        const float rndJitterDepth  = apvts.getRawParameterValue ("rand_jitter") ->load();
        const float rndPanDepth     = apvts.getRawParameterValue ("pan_rnd")     ->load();
        const float rndStutterProb  = apvts.getRawParameterValue ("rand_stutter")->load();

        // Arp pattern: build sorted note order once per block (≤32 elements)
        const int arpPatternIdx = static_cast<int> (apvts.getRawParameterValue ("arp_pattern")->load());
        int arpSorted[kMaxArpNotes];
        for (int n = 0; n < arpNoteCount; ++n) arpSorted[n] = n;  // indices into arpHeldNotes
        if (arpPatternIdx == 0 || arpPatternIdx == 1 || arpPatternIdx == 2) // up / down / updown
        {
            // Sort by note number ascending
            for (int a = 0; a < arpNoteCount - 1; ++a)
                for (int b = a + 1; b < arpNoteCount; ++b)
                    if (arpHeldNotes[arpSorted[a]] > arpHeldNotes[arpSorted[b]])
                        std::swap (arpSorted[a], arpSorted[b]);
            if (arpPatternIdx == 1) // down: reverse
                for (int a = 0, b = arpNoteCount - 1; a < b; ++a, --b)
                    std::swap (arpSorted[a], arpSorted[b]);
        }
        // as-played (3): arpSorted stays as-is (identity)
        // random (4): handled per-tick below

        // ── Sample-accurate rendering ─────────────────────────────────────────
        for (int s = 0; s < numSamples; ++s)
        {
            // ── Arp clock ─────────────────────────────────────────────────────
            if (modeIdx == 2 && arpNoteCount > 0)
            {
                arpPhase += slicePhaseInc;
                if (arpPhase >= 1.0)
                {
                    arpPhase -= 1.0;

                    // Advance sequence index
                    if (arpPatternIdx == 2) // updown pingpong
                    {
                        arpSeqIdx += arpPingDir;
                        if (arpSeqIdx >= arpNoteCount - 1 || arpSeqIdx <= 0)
                            arpPingDir = -arpPingDir;
                        arpSeqIdx = juce::jlimit (0, arpNoteCount - 1, arpSeqIdx);
                    }
                    else if (arpPatternIdx == 4) // random
                    {
                        arpSeqIdx = juce::Random::getSystemRandom().nextInt (arpNoteCount);
                    }
                    else
                    {
                        arpSeqIdx = (arpSeqIdx + 1) % arpNoteCount;
                    }

                    const int noteIdx  = (arpPatternIdx == 4) ? arpSeqIdx : arpSorted[arpSeqIdx % arpNoteCount];
                    const int note     = arpHeldNotes[noteIdx];
                    const float vel    = arpHeldVels [noteIdx];
                    const int sliceIdx = juce::jlimit (0, totalSlices - 1, note - 24);

                    if (sliceIdx < 64 && sliceActive[static_cast<size_t> (sliceIdx)])
                        allocateVoice (note, vel, sliceIdx, atkS, decS, sus);
                }
            }

            // ── SEQ clock ─────────────────────────────────────────────────────
            if (modeIdx == 3)
            {
                // Advance each lane independently using the shared speed table
                for (int L = 0; L < 4; ++L)
                {
                    const int   idx    = juce::jlimit (0, 8, seqSpeedMult[L]);
                    const float lSpeed = kSpeedTable[idx];
                    seqPhase[L] += slicePhaseInc * lSpeed;
                }

                // POS lane (L=0) drives triggers
                if (seqPhase[0] >= 1.0)
                {
                    seqPhase[0] -= 1.0;
                    seqStep[0] = (seqStep[0] + 1) % seqStepCount[0];

                    const float posVal   = laneValues[0][seqStep[0]];
                    const float velVal   = laneValues[1][seqStep[1]];
                    const float pitchVal = laneValues[2][seqStep[2]];
                    const float dirVal   = laneValues[3][seqStep[3]];

                    // Build active-slice pool: all toggle-ON slices across all sections
                    int activePool[64];
                    int poolSize = 0;
                    for (int i = 0; i < totalSlices && i < 64; ++i)
                        if (sliceActive[static_cast<size_t> (i)])
                            activePool[poolSize++] = i;

                    if (poolSize > 0)
                    {
                        // POS 0-1 → pool index (evenly distributed across active slices)
                        const int poolIdx  = juce::jlimit (0, poolSize - 1,
                                                juce::roundToInt (posVal * (poolSize - 1)));
                        const int sliceIdx = activePool[poolIdx];

                        // PITCH: 0-1 → ±24 semitones (0.5 = 0 st)
                        const float extraPitch = (pitchVal - 0.5f) * 48.0f;

                        // DIR: binary — below 0.5 = rev, at/above 0.5 = fwd
                        const int dirOverride = (dirVal < 0.5f) ? 1 : 0;

                        allocateVoice (60, velVal, sliceIdx, atkS, decS, sus,
                                       dirOverride, extraPitch);
                    }
                }

                // Advance non-POS lanes
                for (int L = 1; L < 4; ++L)
                {
                    if (seqPhase[L] >= 1.0)
                    {
                        seqPhase[L] -= 1.0;
                        seqStep[L] = (seqStep[L] + 1) % seqStepCount[L];
                    }
                }
            }

            // ── Render all active voices for this sample ──────────────────────
            for (int i = 0; i < kNumVoices; ++i)
            {
                auto& v = voices[i];
                if (! v.active) continue;

                if (modeIdx == 0 && v.gateOpen)
                {
                    // Per-voice BPM clock
                    v.slicePhase += slicePhaseInc;
                    if (v.slicePhase >= 1.0)
                    {
                        v.slicePhase -= 1.0;

                        // Ghost copy: duplicate voice state, enter release immediately
                        // — its tail smears freely into the next slice's audio territory
                        int ghostIdx = -1;
                        for (int j = 0; j < kNumVoices && ghostIdx < 0; ++j)
                            if (j != i && ! voices[j].active) ghostIdx = j;
                        if (ghostIdx < 0)
                        {
                            int minAge = INT_MAX;
                            for (int j = 0; j < kNumVoices; ++j)
                                if (j != i && voices[j].active && ! voices[j].gateOpen
                                    && voices[j].age < minAge)
                                    { minAge = voices[j].age; ghostIdx = j; }
                        }
                        if (ghostIdx >= 0)
                        {
                            voices[ghostIdx].copyStateFrom (v);
                            voices[ghostIdx].gateOpen   = false;
                            voices[ghostIdx].sliceEnded = true;
                            voices[ghostIdx].age        = voiceAge++;
                            voices[ghostIdx].active     = true;
                            voices[ghostIdx].ampEnv.noteOff (relS, currentSampleRate);
                        }

                        // Retrigger main voice for next slice
                        v.currentSliceIdx = advanceSeqSlice (v.currentSliceIdx, v.pingpongDir,
                                                              reelDirIdx, totalSlices);

                        // Apply per-slice randomization
                        {
                            auto rndF = [] { return juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f; };
                            float pitchRnd = 0.0f;
                            const int maxOct = juce::jlimit (0, 3, static_cast<int> (std::ceil (rndOctDepth * 3.0f)));
                            if (maxOct > 0)
                            {
                                const int oct = static_cast<int> (std::floor (
                                    juce::Random::getSystemRandom().nextFloat() * (2 * maxOct + 1))) - maxOct;
                                pitchRnd += static_cast<float> (oct * 12);
                            }
                            if (juce::Random::getSystemRandom().nextFloat() < rndFifthDepth)
                                pitchRnd += (juce::Random::getSystemRandom().nextFloat() < 0.5f) ? 7.0f : -7.0f;

                            v.filterCutoff = juce::jlimit (20.0f, 20000.0f,
                                filterCutoffHz * std::pow (2.0f, rndF() * rndCutoffDepth * 3.0f));
                            v.volRandMult  = juce::jlimit (0.1f, 2.0f, 1.0f + rndF() * rndVolDepth * 0.5f);

                            // Jitter: shift BPM clock phase ±depth×10% of slice period
                            // (positive = early trigger, negative = late trigger — both feel like timing instability)
                            if (rndJitterDepth > 0.0f)
                            {
                                v.slicePhase += rndF() * rndJitterDepth * 0.10;
                                v.slicePhase = juce::jlimit (0.0, 0.99, v.slicePhase);
                            }

                            setupSlicePlayback (v, sliceDirIdx, pitchRnd);
                            const float rsd = (v.endSample - v.startSample) / (float) currentSampleRate;
                            const float aR  = juce::jlimit (0.0f, 1.0f, atkS + rndF() * rndEnvDepth * 0.4f);
                            const float dR  = juce::jlimit (0.0f, 1.0f, decS + rndF() * rndEnvDepth * 0.4f);
                            v.ampEnv.noteOn  (normToEnvTime (aR, rsd), normToEnvTime (dR, rsd), sus, currentSampleRate);
                            v.fenvEnv.noteOn (normToEnvTime (fenvAtkBlock, rsd), normToEnvTime (fenvDecBlock, rsd), 0.0f, currentSampleRate);

                            // Pan re-randomization
                            if (rndPanDepth > 0.0f)
                            {
                                const float panCenter = apvts.getRawParameterValue ("pan")->load();
                                v.pan = juce::jlimit (-1.0f, 1.0f,
                                    panCenter + rndF() * rndPanDepth);
                            }

                            // Stutter: mute this slice (envelope + BPM clock keep running)
                            v.stuttered = (rndStutterProb > 0.0f &&
                                juce::Random::getSystemRandom().nextFloat() < rndStutterProb);
                        }
                        v.ladderL.reset();
                        v.ladderR.reset();
                    }
                }
                else if (modeIdx != 0 && ! v.sliceEnded)
                {
                    // Non-Poly: detect slice end → start release, but keep playing
                    const bool hitEnd = (v.playRate >= 0.0f)
                        ? v.playhead >= static_cast<float> (v.endSample - 1)
                        : v.playhead <= static_cast<float> (v.startSample);
                    if (hitEnd)
                    {
                        v.sliceEnded = true;
                        // Only start release from slice-end if key is still held.
                        // If already released, MIDI noteOff already started the release — don't restart it.
                        if (v.gateOpen)
                            v.ampEnv.noteOff (relS, currentSampleRate);
                    }
                }

                const float envGain = v.ampEnv.process();
                if (v.ampEnv.isIdle()) { v.active = false; continue; }

                const float* srcL   = reelL;
                const float* srcR   = reelR;
                const int    srcMax = reelMax;

                const int   pos  = juce::jlimit (0, srcMax, static_cast<int> (v.playhead));
                const float frac = juce::jlimit (0.0f, 1.0f, v.playhead - static_cast<float> (pos));

                float rawL = srcL[pos] + frac * (srcL[pos + 1] - srcL[pos]);
                float rawR = (outR != outL) ? srcR[pos] + frac * (srcR[pos + 1] - srcR[pos]) : rawL;

                // Ladder filter with fenv modulation (±4 octaves) and per-voice rand cutoff
                const float fenvVal   = v.fenvEnv.process();
                const float fenvShift = fenvVal * fenvDepth;
                const float modCutoff = juce::jlimit (20.0f, 19000.0f,
                    v.filterCutoff * std::pow (2.0f, fenvShift * 4.0f));

                v.ladderL.setCutoffFrequencyHz (modCutoff);
                v.ladderL.setResonance (filterResVal);
                v.ladderR.setCutoffFrequencyHz (modCutoff);
                v.ladderR.setResonance (filterResVal);

                {
                    float* pL = &rawL;
                    juce::dsp::AudioBlock<float> blkL (&pL, 1, 1);
                    v.ladderL.process (juce::dsp::ProcessContextReplacing<float> (blkL));
                }
                if (outR != outL)
                {
                    float* pR = &rawR;
                    juce::dsp::AudioBlock<float> blkR (&pR, 1, 1);
                    v.ladderR.process (juce::dsp::ProcessContextReplacing<float> (blkR));
                }
                else { rawR = rawL; }

                // Equal-power pan + envelope + velocity (stuttered slices output silence)
                const float angle = (v.pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                const float stutterMute = v.stuttered ? 0.0f : 1.0f;
                const float gainL = envGain * v.velocity * v.volRandMult * stutterMute * std::cos (angle);
                const float gainR = envGain * v.velocity * v.volRandMult * stutterMute * std::sin (angle);

                outL[s] += rawL * gainL;
                if (outR != outL) outR[s] += rawR * gainR;

                v.playhead += v.playRate;

                if (modeIdx == 0 && v.gateOpen)
                {
                    // Poly sustaining: clamp to slice boundary until the BPM tick fires
                    if (v.playRate >= 0.0f)
                        v.playhead = std::min (v.playhead, static_cast<float> (v.endSample - 1));
                    else
                        v.playhead = std::max (v.playhead, static_cast<float> (v.startSample));
                }
                else
                {
                    // All other voices run free — deactivate at buffer boundary
                    const float bufEnd = static_cast<float> (reelBuffer.getNumSamples() - 2);
                    if (v.playhead >= bufEnd || v.playhead < 0.0f)
                        v.active = false;
                }
            }

        }
    }

    // ── Output volume ────────────────────────────────────────────────────────
    for (int s = 0; s < numSamples; ++s)
    {
        const float gain = smoothedOutputVol.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[s] *= gain;
    }

    // ── Tape: Wow & Flutter + Age ─────────────────────────────────────────────
    {
        const float wfDepth  = apvts.getRawParameterValue ("wow_flutter")->load();
        const float ageDepth = apvts.getRawParameterValue ("tape_age")   ->load();

        // Wow & Flutter: max depth scaled to 60% of previous values
        // wow: 0.8 Hz, up to ±7.2ms; flutter: 8.0 Hz, up to ±0.9ms
        const double wfWowInc     = juce::MathConstants<double>::twoPi * 0.8  / currentSampleRate;
        const double wfFlutterInc = juce::MathConstants<double>::twoPi * 8.0  / currentSampleRate;
        const float  wowMaxSamp   = static_cast<float> (0.0072 * currentSampleRate);  // 7.2ms (was 12ms)
        const float  fltMaxSamp   = static_cast<float> (0.0009 * currentSampleRate);  // 0.9ms (was 1.5ms)
        const float  wfCenter     = wowMaxSamp + fltMaxSamp + 2.0f;

        // Tape Age coefficients
        // Saturation: prominent harmonic distortion with partial makeup gain
        // drive 1→15; sqrt(drive) partial makeup keeps harmonics audible without clipping
        const float ageDrive      = 1.0f + ageDepth * 14.0f;  // drive 1→15
        const float ageMakeup     = std::sqrt (ageDrive) / ageDrive;  // partial level recovery

        // HF rolloff: gentle, 20kHz → ~12kHz at age=1
        const float ageCutoff = 20000.0f * std::pow (0.60f, ageDepth);
        const float ageAlpha  = std::exp (-juce::MathConstants<float>::twoPi
                                          * ageCutoff / (float) currentSampleRate);

        // Hiss: scaled to 40% of previous max (~−60 dBFS peak at age=1)
        const float hissAmp = ageDepth * ageDepth * 0.002f;

        // Crackle: 40% of previous rate and amplitude
        const float crackleProbPerSamp = ageDepth * ageDepth * 0.000032f;

        float* chanL = buffer.getWritePointer (0);
        float* chanR = numChannels > 1 ? buffer.getWritePointer (1) : chanL;

        for (int s = 0; s < numSamples; ++s)
        {
            // --- Wow & Flutter ---
            if (wfDepth > 0.0f)
            {
                // Write current sample into circular buffer
                wfBufL[wfWritePos] = chanL[s];
                wfBufR[wfWritePos] = chanR[s];

                // Compute modulated read offset (samples)
                const float wowMod  = static_cast<float> (std::sin (wfPhaseWow))    * wowMaxSamp;
                const float fltMod  = static_cast<float> (std::sin (wfPhaseFlutter)) * fltMaxSamp;
                const float delayF  = wfCenter + (wowMod + fltMod) * wfDepth;
                const float readF   = static_cast<float> (wfWritePos) - delayF;
                const int   readI   = (static_cast<int> (std::floor (readF)) + kWFBufSize * 4) % kWFBufSize;
                const int   readI1  = (readI + 1) % kWFBufSize;
                const float frac    = readF - std::floor (readF);

                chanL[s] = wfBufL[readI] + frac * (wfBufL[readI1] - wfBufL[readI]);
                chanR[s] = wfBufR[readI] + frac * (wfBufR[readI1] - wfBufR[readI]);

                wfWritePos = (wfWritePos + 1) % kWFBufSize;
                wfPhaseWow    += wfWowInc;
                wfPhaseFlutter += wfFlutterInc;
                if (wfPhaseWow    > juce::MathConstants<double>::twoPi) wfPhaseWow    -= juce::MathConstants<double>::twoPi;
                if (wfPhaseFlutter > juce::MathConstants<double>::twoPi) wfPhaseFlutter -= juce::MathConstants<double>::twoPi;
            }

            // --- Tape Age ---
            if (ageDepth > 0.0f)
            {
                auto& rng = juce::Random::getSystemRandom();

                // 1. Saturation — prominent harmonic distortion, partial makeup gain
                chanL[s] = std::tanh (chanL[s] * ageDrive) * ageMakeup;
                chanR[s] = std::tanh (chanR[s] * ageDrive) * ageMakeup;

                // 2. Gentle HF rolloff (tape head gap + oxide)
                tapeLP_L = ageAlpha * tapeLP_L + (1.0f - ageAlpha) * chanL[s];
                tapeLP_R = ageAlpha * tapeLP_R + (1.0f - ageAlpha) * chanR[s];
                chanL[s] = tapeLP_L;
                chanR[s] = tapeLP_R;

                // 3. Hiss — tape oxide noise floor
                chanL[s] += (rng.nextFloat() * 2.0f - 1.0f) * hissAmp;
                chanR[s] += (rng.nextFloat() * 2.0f - 1.0f) * hissAmp;

                // 4. Crackle — physical tape defects (Poisson burst)
                if (crackleEnv < 0.01f && rng.nextFloat() < crackleProbPerSamp)
                {
                    crackleAmp   = (0.3f + rng.nextFloat() * 0.7f) * ageDepth * 0.04f;
                    crackleEnv   = 1.0f;
                    const float durSec = 0.001f + rng.nextFloat() * 0.003f; // 1–4 ms burst
                    crackleDecay = std::pow (0.001f, 1.0f / (durSec * (float) currentSampleRate));
                }
                if (crackleEnv > 0.001f)
                {
                    const float c = (rng.nextFloat() * 2.0f - 1.0f) * crackleEnv * crackleAmp;
                    chanL[s] += c;
                    chanR[s] += c * (0.85f + rng.nextFloat() * 0.3f); // slight stereo spread
                    crackleEnv *= crackleDecay;
                }
            }
        }
    }

    // ── Global EQ ────────────────────────────────────────────────────────────
    {
        eqLowSmooth    .setTargetValue (apvts.getRawParameterValue ("eq_low")     ->load());
        eqLowMidSmooth .setTargetValue (apvts.getRawParameterValue ("eq_low_mid") ->load());
        eqHighMidSmooth.setTargetValue (apvts.getRawParameterValue ("eq_high_mid")->load());
        eqHighSmooth   .setTargetValue (apvts.getRawParameterValue ("eq_high")    ->load());

        auto toLinear = [] (float db) { return std::pow (10.0f, db / 20.0f); };
        *eqChain.get<0>().state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf   (currentSampleRate,  100.0, 0.707, toLinear (eqLowSmooth    .getNextValue()));
        *eqChain.get<1>().state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate,  500.0, 0.707, toLinear (eqLowMidSmooth .getNextValue()));
        *eqChain.get<2>().state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, 2000.0, 0.707, toLinear (eqHighMidSmooth.getNextValue()));
        *eqChain.get<3>().state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf  (currentSampleRate, 8000.0, 0.707, toLinear (eqHighSmooth   .getNextValue()));

        juce::dsp::AudioBlock<float>              eqBlock (buffer);
        juce::dsp::ProcessContextReplacing<float> eqCtx (eqBlock);
        eqChain.process (eqCtx);
    }

    // Update peak level for UI
    outputPeakLevel.store (buffer.getMagnitude (0, numSamples));

    // Lit indicator: youngest active voice's slice (all modes)
    {
        int litSlice = -1, youngestAge = INT_MAX;
        for (int i = 0; i < kNumVoices; ++i)
            if (voices[i].active && voices[i].age < youngestAge)
                { youngestAge = voices[i].age; litSlice = voices[i].currentSliceIdx; }
        activeSliceIdx.store (litSlice);
    }

    // SEQ: push current step per lane for UI highlight
    seqLitStep0.store (seqStep[0]);
    seqLitStep1.store (seqStep[1]);
    seqLitStep2.store (seqStep[2]);
    seqLitStep3.store (seqStep[3]);
}

//==============================================================================
bool SpliceAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* SpliceAudioProcessor::createEditor()
{
    return new SpliceAudioProcessorEditor (*this);
}

//==============================================================================
void SpliceAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // Serialize grid state (64 bits as string)
    juce::String gridStr;
    for (size_t i = 0; i < 64; ++i)
        gridStr += sliceActive[i] ? "1" : "0";
    state.setProperty ("sliceActive", gridStr, nullptr);

    // Serialize SEQ lane values (4 lanes × 16 floats, comma-separated)
    for (size_t lane = 0; lane < 4; ++lane)
    {
        juce::String laneStr;
        for (size_t step = 0; step < 16; ++step)
            laneStr += (step > 0 ? "," : "") + juce::String (laneValues[lane][step], 3);
        state.setProperty ("lane" + juce::String (static_cast<int> (lane)), laneStr, nullptr);
    }

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);

    // Persist to appProperties so we can offer restore on next startup
    if (auto* settings = appProperties.getUserSettings())
        settings->setValue ("lastFullState", xml->toString());
}

void SpliceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr) return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;

    apvts.replaceState (state);

    // Restore grid
    if (state.hasProperty ("sliceActive"))
    {
        auto gridStr = state["sliceActive"].toString();
        for (size_t i = 0; i < 64 && i < (size_t) gridStr.length(); ++i)
            sliceActive[i] = (gridStr[(int) i] == '1');
    }

    // Restore SEQ lanes
    for (size_t lane = 0; lane < 4; ++lane)
    {
        auto key = "lane" + juce::String ((int) lane);
        if (state.hasProperty (key))
        {
            juce::StringArray tokens;
            tokens.addTokens (state[key].toString(), ",", "");
            for (size_t step = 0; step < 16 && step < (size_t) tokens.size(); ++step)
                laneValues[lane][step] = tokens[(int) step].getFloatValue();
        }
    }
}

bool SpliceAudioProcessor::hasSavedState() const
{
    if (auto* settings = const_cast<juce::ApplicationProperties&> (appProperties).getUserSettings())
        return settings->containsKey ("lastFullState");
    return false;
}

void SpliceAudioProcessor::restoreLastState()
{
    auto* settings = appProperties.getUserSettings();
    if (settings == nullptr) return;

    auto xmlStr = settings->getValue ("lastFullState");
    if (xmlStr.isEmpty()) return;

    auto xml = juce::XmlDocument::parse (xmlStr);
    if (xml == nullptr) return;

    juce::MemoryBlock data;
    copyXmlToBinary (*xml, data);
    setStateInformation (data.getData(), (int) data.getSize());
}

//==============================================================================
void SpliceAudioProcessor::loadReelFile (const juce::File& file)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (!reader) return;

    juce::AudioBuffer<float> newBuffer (static_cast<int> (reader->numChannels),
                                        static_cast<int> (reader->lengthInSamples));
    reader->read (&newBuffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);

    // Ensure stereo
    if (newBuffer.getNumChannels() == 1)
    {
        juce::AudioBuffer<float> stereo (2, newBuffer.getNumSamples());
        stereo.copyFrom (0, 0, newBuffer, 0, 0, newBuffer.getNumSamples());
        stereo.copyFrom (1, 0, newBuffer, 0, 0, newBuffer.getNumSamples());
        reelBuffer  = std::move (stereo);
    }
    else
    {
        reelBuffer  = std::move (newBuffer);
    }

    reelSampleRate = reader->sampleRate;
    reelName = file.getFileNameWithoutExtension();

    // Kill all voices — stale playheads into the old buffer cause erratic playback
    for (auto& v : voices) v.active = false;
    arpPhase = 0.0; polySlicePhase = 0.0;
    seqPhase.fill (0.0); seqStep.fill (0);

    // Persist path so "open last file?" dialog works on next launch
    if (auto* settings = appProperties.getUserSettings())
    {
        settings->setValue ("lastReelPath", file.getFullPathName());
        settings->saveIfNeeded();
    }
}

juce::File SpliceAudioProcessor::getLastReelFile() const
{
    auto* settings = const_cast<juce::ApplicationProperties&> (appProperties).getUserSettings();
    if (settings == nullptr) return {};
    auto path = settings->getValue ("lastReelPath", "");
    if (path.isEmpty()) return {};
    return juce::File (path);
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpliceAudioProcessor();
}
