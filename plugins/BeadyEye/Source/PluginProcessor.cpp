#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BeadyEyeAudioProcessor::BeadyEyeAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#else
    :
#endif
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    buildEnvelopeLUTs();
}

BeadyEyeAudioProcessor::~BeadyEyeAudioProcessor() {}

//==============================================================================
// ENVELOPE LOOKUP TABLE GENERATION
//==============================================================================

void BeadyEyeAudioProcessor::buildEnvelopeLUTs()
{
    const float pi = juce::MathConstants<float>::pi;

    for (int i = 0; i < kEnvelopeLUTSize; ++i)
    {
        float phase = static_cast<float>(i) / static_cast<float>(kEnvelopeLUTSize - 1);

        // Shape 0.0: Rectangular with 2% fade at edges (prevents clicks)
        {
            float fadeZone = 0.02f;
            if (phase < fadeZone)
                envRectangular[static_cast<size_t>(i)] = phase / fadeZone;
            else if (phase > (1.0f - fadeZone))
                envRectangular[static_cast<size_t>(i)] = (1.0f - phase) / fadeZone;
            else
                envRectangular[static_cast<size_t>(i)] = 1.0f;
        }

        // Shape 0.25: Percussive — fast attack (~5%), exponential decay
        {
            float attackEnd = 0.05f;
            if (phase < attackEnd)
                envPercussive[static_cast<size_t>(i)] = phase / attackEnd;
            else
            {
                float decayPhase = (phase - attackEnd) / (1.0f - attackEnd);
                envPercussive[static_cast<size_t>(i)] = std::exp(-4.0f * decayPhase);
            }
        }

        // Shape 0.5: Hann window (symmetric bell)
        envHann[static_cast<size_t>(i)] = 0.5f * (1.0f - std::cos(2.0f * pi * phase));

        // Shape 1.0: Pad — wide sustained envelope
        {
            float sustain = std::min(1.0f, envHann[static_cast<size_t>(i)] * 1.8f);
            envPad[static_cast<size_t>(i)] = sustain;
        }
    }
}

//==============================================================================
const juce::String BeadyEyeAudioProcessor::getName() const { return JucePlugin_Name; }
bool BeadyEyeAudioProcessor::acceptsMidi() const { return false; }
bool BeadyEyeAudioProcessor::producesMidi() const { return false; }
bool BeadyEyeAudioProcessor::isMidiEffect() const { return false; }
double BeadyEyeAudioProcessor::getTailLengthSeconds() const { return 2.0; }
int BeadyEyeAudioProcessor::getNumPrograms() { return 1; }
int BeadyEyeAudioProcessor::getCurrentProgram() { return 0; }
void BeadyEyeAudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String BeadyEyeAudioProcessor::getProgramName (int index) { juce::ignoreUnused (index); return "Default"; }
void BeadyEyeAudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused (index, newName); }

//==============================================================================
void BeadyEyeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = sampleRate;

    bufferLength = static_cast<int>(sampleRate * kMaxBufferSeconds);
    bufferLength = std::min(bufferLength, kMaxBufferLength);

    circularBufferL.assign(static_cast<size_t>(bufferLength), 0.0f);
    circularBufferR.assign(static_cast<size_t>(bufferLength), 0.0f);
    writeHead = 0;

    dryBuffer.setSize(2, samplesPerBlock);

    for (auto& g : grains)
        g.active = false;

    samplesSinceLastGrain = 999999;
    currentGrainInterval  = 999999;
    totalSamplesWritten   = 0;
    lastSyncSlot          = -1.0;
    silenceSampleCount    = 0;
    silenceTimeoutSamples = static_cast<int>(sampleRate * 0.05);  // 50ms
    frozen                = false;

    feedbackSampleL = 0.0f;
    feedbackSampleR = 0.0f;

    const float initOutputVol = apvts.getRawParameterValue("output_vol")->load();
    const float initDryWet    = apvts.getRawParameterValue("dry_wet")->load();
    smoothedOutputVol.reset (sampleRate, 0.005);
    smoothedOutputVol.setCurrentAndTargetValue (initOutputVol);
    smoothedDryWet.reset    (sampleRate, 0.005);
    smoothedDryWet.setCurrentAndTargetValue    (initDryWet);

    initReverb (sampleRate);
}

void BeadyEyeAudioProcessor::releaseResources()
{
    circularBufferL.clear();
    circularBufferR.clear();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool BeadyEyeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

//==============================================================================
// PRNG (xorshift32)
//==============================================================================

float BeadyEyeAudioProcessor::nextRandom()
{
    prngState ^= prngState << 13;
    prngState ^= prngState >> 17;
    prngState ^= prngState << 5;
    return static_cast<float>(prngState) / static_cast<float>(0xFFFFFFFFu);
}

//==============================================================================
// CIRCULAR BUFFER
//==============================================================================

void BeadyEyeAudioProcessor::writeToCircularBuffer (float sampleL, float sampleR)
{
    if (frozen || bufferLength == 0)
        return;

    circularBufferL[static_cast<size_t>(writeHead)] = sampleL;
    circularBufferR[static_cast<size_t>(writeHead)] = sampleR;
    writeHead = (writeHead + 1) % bufferLength;
}

float BeadyEyeAudioProcessor::readFromCircularBuffer (const std::vector<float>& buffer, double position) const
{
    if (bufferLength == 0)
        return 0.0f;

    while (position < 0.0)
        position += static_cast<double>(bufferLength);

    int posInt = static_cast<int>(position) % bufferLength;
    float frac = static_cast<float>(position - std::floor(position));

    int p0 = (posInt - 1 + bufferLength) % bufferLength;
    int p1 = posInt;
    int p2 = (posInt + 1) % bufferLength;
    int p3 = (posInt + 2) % bufferLength;

    float y0 = buffer[static_cast<size_t>(p0)];
    float y1 = buffer[static_cast<size_t>(p1)];
    float y2 = buffer[static_cast<size_t>(p2)];
    float y3 = buffer[static_cast<size_t>(p3)];

    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

//==============================================================================
// 4-SHAPE GRAIN ENVELOPE
//==============================================================================

float BeadyEyeAudioProcessor::getGrainEnvelope (float shape, int age, int length) const
{
    if (length <= 0) return 0.0f;

    float phase = juce::jlimit(0.0f, 1.0f, static_cast<float>(age) / static_cast<float>(length));

    float lutPos = phase * static_cast<float>(kEnvelopeLUTSize - 1);
    int idx = static_cast<int>(lutPos);
    float frac = lutPos - static_cast<float>(idx);

    if (idx >= kEnvelopeLUTSize - 1) idx = kEnvelopeLUTSize - 2;

    auto si  = static_cast<size_t>(idx);
    auto si1 = static_cast<size_t>(idx + 1);

    float rect = envRectangular[si] * (1.0f - frac) + envRectangular[si1] * frac;
    float perc = envPercussive[si]  * (1.0f - frac) + envPercussive[si1]  * frac;
    float hann = envHann[si]        * (1.0f - frac) + envHann[si1]        * frac;
    float pad  = envPad[si]         * (1.0f - frac) + envPad[si1]         * frac;

    float s = juce::jlimit(0.0f, 1.0f, shape) * 3.0f;

    if (s <= 1.0f) return rect * (1.0f - s) + perc * s;
    if (s <= 2.0f) return perc * (2.0f - s) + hann * (s - 1.0f);
    return hann * (3.0f - s) + pad * (s - 2.0f);
}

//==============================================================================
// BIPOLAR DENSITY SCHEDULER
//==============================================================================

int BeadyEyeAudioProcessor::computeGrainInterval (float densityParam) const
{
    float absDensity = std::abs(densityParam);
    if (absDensity < 0.01f) return 999999;

    float maxIntervalSec = 1.0f;
    float minIntervalSec = 1.0f / 32.0f;
    float intervalSec = maxIntervalSec * std::pow(minIntervalSec / maxIntervalSec, absDensity);

    return std::max(32, static_cast<int>(intervalSec * static_cast<float>(currentSampleRate)));
}

//==============================================================================
// TEMPO-SYNCED DENSITY SCHEDULER
//==============================================================================

int BeadyEyeAudioProcessor::computeSyncedGrainInterval (float densityParam, double bpm) const
{
    float absDensity = std::abs(densityParam);
    if (absDensity < 0.05f) return 999999;
    if (bpm <= 0.0) return computeGrainInterval(densityParam);

    static const float divisions[] = { 4.0f, 3.0f, 2.0f, 1.0f, 2.0f/3.0f, 0.5f, 1.0f/3.0f, 0.25f };
    static const int numDivisions = 8;

    float pos = (absDensity - 0.05f) / 0.95f * static_cast<float>(numDivisions - 1);
    int idx = std::max(0, std::min(numDivisions - 1, static_cast<int>(std::round(pos))));

    double intervalSec = static_cast<double>(divisions[idx]) * 60.0 / bpm;
    return std::max(32, static_cast<int>(intervalSec * currentSampleRate));
}

float BeadyEyeAudioProcessor::getSyncedBeatsPerGrain (float densityParam) const
{
    float absDensity = std::abs(densityParam);
    if (absDensity < 0.05f) return -1.0f;

    static const float divisions[] = { 4.0f, 3.0f, 2.0f, 1.0f, 2.0f/3.0f, 0.5f, 1.0f/3.0f, 0.25f };
    static const int numDivisions = 8;

    float pos = (absDensity - 0.05f) / 0.95f * static_cast<float>(numDivisions - 1);
    int idx = std::max(0, std::min(numDivisions - 1, static_cast<int>(std::round(pos))));

    return divisions[idx];
}

//==============================================================================
// GRAIN POOL
//==============================================================================

Grain& BeadyEyeAudioProcessor::findFreeGrain()
{
    for (auto& g : grains)
        if (!g.active) return g;

    int oldestIdx = 0, oldestAge = 0;
    for (int i = 0; i < kMaxGrains; ++i)
    {
        if (grains[static_cast<size_t>(i)].grainAge > oldestAge)
        {
            oldestAge = grains[static_cast<size_t>(i)].grainAge;
            oldestIdx = i;
        }
    }
    return grains[static_cast<size_t>(oldestIdx)];
}

//==============================================================================
// GRAIN SPAWNING
//==============================================================================

void BeadyEyeAudioProcessor::spawnGrain (float timeParam,  float sizeParam,  float pitchParam,  float shapeParam,
                                          float timeAtten, float sizeAtten, float pitchAtten, float shapeAtten)
{
    // Per-grain attenurandomization
    if (timeAtten > 0.0f)
        timeParam  = juce::jlimit (0.0f,   1.0f,  timeParam  + (nextRandom() * 2.0f - 1.0f) * timeAtten);
    if (sizeAtten > 0.0f)
        sizeParam  = juce::jlimit (-1.0f,  1.0f,  sizeParam  + (nextRandom() * 2.0f - 1.0f) * sizeAtten);
    if (pitchAtten > 0.0f)
        pitchParam = juce::jlimit (-24.0f, 24.0f, pitchParam + (nextRandom() * 2.0f - 1.0f) * pitchAtten * 24.0f);
    if (shapeAtten > 0.0f)
        shapeParam = juce::jlimit (0.0f,   1.0f,  shapeParam + (nextRandom() * 2.0f - 1.0f) * shapeAtten);

    Grain& g = findFreeGrain();
    g.active   = true;
    g.grainAge = 0;

    int maxReadBack = std::min(bufferLength, totalSamplesWritten);
    double readOffset = static_cast<double>(timeParam) * static_cast<double>(maxReadBack);
    g.readPosition = static_cast<double>((writeHead - static_cast<int>(readOffset) + bufferLength) % bufferLength);

    float absSize = std::abs(sizeParam);
    g.reverse = (sizeParam < 0.0f);

    // Cubic curve: finer resolution near 0 (small/medium grains) where playability matters most
    float sizeCurved = absSize * absSize * absSize;
    int minGrainSamples = static_cast<int>(currentSampleRate * 0.01);
    int maxGrainSamples = bufferLength / 2;
    g.grainLength = std::max(minGrainSamples,
        minGrainSamples + static_cast<int>(sizeCurved * static_cast<float>(maxGrainSamples - minGrainSamples)));

    g.phaseIncrement = std::pow(2.0, static_cast<double>(pitchParam) / 12.0);
    if (g.reverse) g.phaseIncrement = -g.phaseIncrement;

    g.envelopeShape = shapeParam;

    float panRand = nextRandom() * 0.6f - 0.3f;
    g.panL = 1.0f - std::max(0.0f, panRand);
    g.panR = 1.0f + std::min(0.0f, panRand);
}

//==============================================================================
// POLYPHONIC GRAIN PROCESSING
//==============================================================================

void BeadyEyeAudioProcessor::processGrains (float& outL, float& outR)
{
    outL = 0.0f;
    outR = 0.0f;
    int count = 0;

    for (auto& g : grains)
    {
        if (!g.active) continue;

        float sampleL = readFromCircularBuffer(circularBufferL, g.readPosition);
        float sampleR = readFromCircularBuffer(circularBufferR, g.readPosition);
        float envelope = getGrainEnvelope(g.envelopeShape, g.grainAge, g.grainLength);

        outL += sampleL * envelope * g.panL;
        outR += sampleR * envelope * g.panR;

        g.readPosition += g.phaseIncrement;
        while (g.readPosition < 0.0)           g.readPosition += static_cast<double>(bufferLength);
        while (g.readPosition >= static_cast<double>(bufferLength)) g.readPosition -= static_cast<double>(bufferLength);

        g.grainAge++;
        if (g.grainAge >= g.grainLength) g.active = false;
        else ++count;
    }

    if (count > 4)
    {
        float normFactor = 4.0f / static_cast<float>(count);
        outL *= normFactor;
        outR *= normFactor;
    }
}

//==============================================================================
// MAIN PROCESS BLOCK
//==============================================================================

void BeadyEyeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || bufferLength == 0) return;

    // ---- Read parameters ----
    float timeParam      = apvts.getRawParameterValue("time")->load();
    float sizeParam      = apvts.getRawParameterValue("size")->load();
    float pitchParam     = apvts.getRawParameterValue("pitch")->load();
    float shapeParam     = apvts.getRawParameterValue("shape")->load();
    float densityParam   = apvts.getRawParameterValue("density")->load();
    float feedbackParam  = apvts.getRawParameterValue("feedback")->load() * 0.75f;  // effective max 0.75
    float dryWetParam    = apvts.getRawParameterValue("dry_wet")->load();
    float reverbParam    = apvts.getRawParameterValue("reverb")->load();
    float outputVolParam = apvts.getRawParameterValue("output_vol")->load();
    bool  freezeParam    = apvts.getRawParameterValue("freeze")->load() > 0.5f;
    bool  densitySyncParam = apvts.getRawParameterValue("density_sync")->load() > 0.5f;

    float timeAttenParam  = apvts.getRawParameterValue("time_atten")->load();
    float sizeAttenParam  = apvts.getRawParameterValue("size_atten")->load();
    float pitchAttenParam = apvts.getRawParameterValue("pitch_atten")->load();
    float shapeAttenParam = apvts.getRawParameterValue("shape_atten")->load();

    smoothedOutputVol.setTargetValue (outputVolParam);
    smoothedDryWet.setTargetValue    (dryWetParam);

    frozen = freezeParam;

    // ---- Transport info ----
    double currentBpm  = 120.0;
    double ppqPosition = -1.0;
    bool   isPlaying   = false;

    if (auto* playHead = getPlayHead())
    {
        if (auto posInfo = playHead->getPosition())
        {
            if (auto bpm = posInfo->getBpm())     currentBpm  = *bpm;
            if (auto ppq = posInfo->getPpqPosition()) ppqPosition = *ppq;
            isPlaying = posInfo->getIsPlaying();
        }
    }

    // ---- Input metering ----
    float inPeak = 0.0f;
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
        inPeak = std::max(inPeak, buffer.getMagnitude(ch, 0, numSamples));
    inputPeakLevel.store(inPeak);

    // ---- Silence detection ----
    if (inPeak < kSilenceThreshold && !frozen)
        silenceSampleCount += numSamples;
    else
        silenceSampleCount = 0;
    bool inputSilent = (silenceSampleCount > silenceTimeoutSamples) && !frozen;

    // ---- Save dry ----
    dryBuffer.makeCopyOf(buffer, true);

    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getWritePointer(1);
    const float* dryL = dryBuffer.getReadPointer(0);
    const float* dryR = dryBuffer.getReadPointer(1);

    bool isDensityNegative = (densityParam < 0.0f);

    // ---- Sync scheduling setup ----
    float syncBeatsPerGrain = -1.0f;
    double ppqPerSample = 0.0;
    // Only use PPQ sync when transport is actually running
    bool useSyncMode = densitySyncParam && isPlaying && ppqPosition >= 0.0;

    if (useSyncMode)
    {
        syncBeatsPerGrain = getSyncedBeatsPerGrain(densityParam);
        if (currentBpm > 0.0)
            ppqPerSample = currentBpm / (60.0 * currentSampleRate);
    }

    int baseInterval = densitySyncParam
        ? computeSyncedGrainInterval(densityParam, currentBpm)
        : computeGrainInterval(densityParam);

    // Always update — if densitySyncParam is on and transport stops, free-mode fallback
    // needs a valid interval (without this it stays at 999999 → no grains when frozen + stopped)
    if (baseInterval < currentGrainInterval / 2 || baseInterval > currentGrainInterval * 2)
        currentGrainInterval = baseInterval;

    // ---- Per-sample processing ----
    for (int i = 0; i < numSamples; ++i)
    {
        // Feedback: additive model — dry is always preserved, grain energy accumulates on top
        const float fb = feedbackParam;
        const float writeL = std::tanh(dryL[i] + feedbackSampleL * fb);
        const float writeR = std::tanh(dryR[i] + feedbackSampleR * fb);
        writeToCircularBuffer(writeL, writeR);
        totalSamplesWritten = std::min(totalSamplesWritten + 1, bufferLength);

        // ---- Grain scheduling ----
        bool shouldSpawn = false;

        if (useSyncMode && syncBeatsPerGrain > 0.0f)
        {
            // PPQ-based phase-locked (only when transport running)
            double ppqNow = ppqPosition + static_cast<double>(i) * ppqPerSample;
            double currentSlot = std::floor(ppqNow / static_cast<double>(syncBeatsPerGrain));
            if (currentSlot != lastSyncSlot)
            {
                shouldSpawn = true;
                lastSyncSlot = currentSlot;
            }
        }
        else
        {
            // Free mode (also used when sync toggled but transport stopped — keeps grains alive)
            samplesSinceLastGrain++;
            if (samplesSinceLastGrain >= currentGrainInterval)
            {
                shouldSpawn = true;
                samplesSinceLastGrain = 0;

                if (!densitySyncParam && !isDensityNegative && baseInterval < 999999)
                {
                    float u = std::max(0.001f, nextRandom());
                    currentGrainInterval = std::max(32, static_cast<int>(-std::log(u) * static_cast<float>(baseInterval)));
                }
                else
                {
                    currentGrainInterval = baseInterval;
                }
            }
        }

        if (shouldSpawn && !inputSilent)
        {
            spawnGrain(timeParam, sizeParam, pitchParam, shapeParam,
                       timeAttenParam, sizeAttenParam, pitchAttenParam, shapeAttenParam);
            grainsSpawnedThisSecond++;
        }

        // ---- Process grains ----
        float grainL = 0.0f, grainR = 0.0f;
        processGrains(grainL, grainR);

        // Normalize grain output to [-1,1]: prevents reverb and output stage from seeing
        // large signals (up to 32 un-normalized grains), while preserving relative dynamics
        const float grainLn = std::tanh(grainL);
        const float grainRn = std::tanh(grainR);

        // ---- Store normalized grain for next sample's feedback ----
        feedbackSampleL = grainLn;
        feedbackSampleR = grainRn;

        // ---- Dry/Wet mix (smoothed) ----
        const float dw = smoothedDryWet.getNextValue();
        const float dryGain = std::cos(dw * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin(dw * juce::MathConstants<float>::halfPi);
        outL[i] = dryL[i] * dryGain + grainLn * wetGain;
        outR[i] = dryR[i] * dryGain + grainRn * wetGain;

        // ---- Reverb (always runs so tank state doesn't go stale while bypassed) ----
        auto [rvL, rvR] = tickReverb(outL[i], outR[i], reverbParam);
        outL[i] = rvL;
        outR[i] = rvR;

        // ---- Output volume (smoothed) ----
        const float vol = smoothedOutputVol.getNextValue();
        outL[i] *= vol;
        outR[i] *= vol;
    }

    // ---- Output metering ----
    float outPeak = 0.0f;
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        outPeak = std::max(outPeak, buffer.getMagnitude(ch, 0, numSamples));
    outputPeakLevel.store(outPeak);

    int count = 0;
    for (const auto& g : grains) if (g.active) count++;
    activeGrainCount.store(count);

    debugSampleCounter += numSamples;
    if (debugSampleCounter >= static_cast<int>(currentSampleRate))
    {
        debugSampleCounter = 0;
        grainsSpawnedThisSecond = 0;
    }
}

//==============================================================================
bool BeadyEyeAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* BeadyEyeAudioProcessor::createEditor()
{
    return new BeadyEyeAudioProcessorEditor (*this);
}

//==============================================================================
void BeadyEyeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void BeadyEyeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout BeadyEyeAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "time",  "Time",  juce::NormalisableRange<float>(0.0f,  1.0f,  0.001f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "size",  "Size",  juce::NormalisableRange<float>(-1.0f, 1.0f,  0.001f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "pitch", "Pitch", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),  0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "shape", "Shape", juce::NormalisableRange<float>(0.0f,  1.0f,  0.001f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "density", "Density", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "feedback",   "Feedback",  juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "dry_wet",    "Dry/Wet",   juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "reverb",     "Reverb",    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "output_vol", "Output",    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.75f));

    layout.add (std::make_unique<juce::AudioParameterBool>("freeze",       "Freeze",       false));
    layout.add (std::make_unique<juce::AudioParameterBool>("density_sync", "Density Sync", false));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "time_atten",  "Time Rand",  juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "size_atten",  "Size Rand",  juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "pitch_atten", "Pitch Rand", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "shape_atten", "Shape Rand", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    return layout;
}

//==============================================================================
// DATTORRO PLATE REVERB
// Reference delays at 29761 Hz, scaled to actual sample rate.
//==============================================================================

void BeadyEyeAudioProcessor::initReverb (double sr)
{
    const double s = sr / 29761.0;
    auto S = [&s] (int n) { return std::max (1, static_cast<int> (static_cast<double>(n) * s)); };

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
    rvTapL[6] = S(1066);
    rvTapL[7] = S(353);

    rvTapR[0] = S(353);  rvTapR[1] = S(3627);
    rvTapR[2] = S(1228); rvTapR[3] = S(2673);
    rvTapR[4] = S(2111); rvTapR[5] = S(335);
    rvTapR[6] = S(121);
    rvTapR[7] = S(205);

    rvLP1 = rvLP2 = rvFdL = rvFdR = 0.0f;
}

std::pair<float, float> BeadyEyeAudioProcessor::tickReverb (float inL, float inR, float amount) noexcept
{
    const float decay = 0.5f + amount * 0.28f;  // range [0.5, 0.78]

    // Plate damping: LP coefficient in the tank feedback path.
    float damp;
    if (amount <= 0.75f)
        damp = 0.0005f + amount * 0.03f;
    else
        damp = 0.0005f + 0.75f * 0.03f + (amount - 0.75f) * 0.8f;

    static constexpr float kDiff1 = 0.70f;
    static constexpr float kDiff2 = 0.50f;
    static constexpr float kIn    = 0.75f;
    static constexpr float kIn2   = 0.625f;

    // Guard against NaN/Inf tank state (can occur after sample-rate changes or in
    // certain hosts). Reset all reverb state cleanly rather than propagating garbage.
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
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BeadyEyeAudioProcessor();
}
