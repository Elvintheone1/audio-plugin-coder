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
        laneValues[0][step] = static_cast<float> (step) / 15.0f;  // pos: spread across slices
        laneValues[1][step] = 0.75f;                               // vel: 75%
        laneValues[2][step] = 0.5f;                                // pitch: center (no offset)
        laneValues[3][step] = 0.9f;                                // dir: forward
    }
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
    auto atkRange  = juce::NormalisableRange<float> (0.001f, 2.0f, 0.0f, 0.5f);
    auto relRange  = juce::NormalisableRange<float> (0.001f, 4.0f, 0.0f, 0.5f);
    auto linRange  = juce::NormalisableRange<float> (0.0f, 1.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> ("amp_attack",  "Attack",  atkRange,  0.005f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("amp_decay",   "Decay",   atkRange,  0.1f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("amp_sustain", "Sustain", linRange,  1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("amp_release", "Release", relRange,  0.1f));

    // ── Group 4: Filter ───────────────────────────────────
    auto cutoffRange = juce::NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f);
    layout.add (std::make_unique<juce::AudioParameterFloat>  ("filter_cutoff", "Cutoff",  cutoffRange, 18000.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>  ("filter_res",    "Res",     linRange,    0.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("filter_type",   "Filter Type",
                                                               juce::StringArray { "LP", "BP", "HP" }, 0));

    // ── Group 5: Filter Envelope ──────────────────────────
    auto bipolarRange = juce::NormalisableRange<float> (-1.0f, 1.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fenv_depth",  "FEnv Depth",  bipolarRange, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fenv_attack", "FEnv Attack", atkRange,     0.01f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("fenv_decay",  "FEnv Decay",  relRange,     0.2f));

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
void SpliceAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    smoothedOutputVol.reset (sampleRate, 0.05);  // 50 ms ramp
    smoothedOutputVol.setCurrentAndTargetValue (apvts.getRawParameterValue ("output_vol")->load());
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
void SpliceAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumOutputChannels = getTotalNumOutputChannels();

    if (buffer.getNumSamples() == 0)
        return;

    // Silence unused channels
    for (int ch = buffer.getNumChannels(); ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Apply output volume (Phase 4.0 stub — full DSP added in Phase 4.1)
    smoothedOutputVol.setTargetValue (apvts.getRawParameterValue ("output_vol")->load());

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float gain = smoothedOutputVol.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[sample] *= gain;
    }

    // Update peak level for metering
    outputPeakLevel.store (buffer.getMagnitude (0, numSamples));
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
}

void SpliceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
    {
        auto newState = juce::ValueTree::fromXml (*xmlState);
        apvts.replaceState (newState);

        // Restore grid state
        auto gridStr = newState.getProperty ("sliceActive", "").toString();
        for (int i = 0; i < std::min (64, gridStr.length()); ++i)
            sliceActive[static_cast<size_t> (i)] = (gridStr[i] == '1');

        // Restore SEQ lane values
        for (int lane = 0; lane < 4; ++lane)
        {
            auto laneStr = newState.getProperty ("lane" + juce::String (lane), "").toString();
            auto parts   = juce::StringArray::fromTokens (laneStr, ",", "");
            for (int step = 0; step < std::min (16, parts.size()); ++step)
                laneValues[static_cast<size_t> (lane)][static_cast<size_t> (step)] = parts[step].getFloatValue();
        }
    }
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
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpliceAudioProcessor();
}
