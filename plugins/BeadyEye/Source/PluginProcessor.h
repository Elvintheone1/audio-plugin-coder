#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>

//==============================================================================
// Grain data structure
struct Grain
{
    bool   active        = false;
    int    grainAge      = 0;
    int    grainLength   = 0;
    double readPosition  = 0.0;
    double phaseIncrement = 1.0;
    float  envelopeShape = 0.5f;
    bool   reverse       = false;
    float  panL          = 1.0f;
    float  panR          = 1.0f;
};

//==============================================================================
class BeadyEyeAudioProcessor : public juce::AudioProcessor
{
public:
    BeadyEyeAudioProcessor();
    ~BeadyEyeAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
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
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter state
    juce::AudioProcessorValueTreeState apvts;

    // Metering (written by audio thread, read by UI timer)
    std::atomic<float> inputPeakLevel  { 0.0f };
    std::atomic<float> outputPeakLevel { 0.0f };
    std::atomic<int>   activeGrainCount { 0 };

private:
    //==============================================================================
    // PARAMETER LAYOUT
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==============================================================================
    // CONSTANTS
    static constexpr int   kMaxGrains       = 32;
    static constexpr int   kEnvelopeLUTSize = 1024;
    static constexpr float kMaxBufferSeconds = 6.0f;
    static constexpr int   kMaxBufferLength  = 6 * 192000;  // 6s @ 192kHz
    static constexpr float kSilenceThreshold = 0.0001f;

    //==============================================================================
    // ENVELOPE LOOKUP TABLES
    std::array<float, kEnvelopeLUTSize> envRectangular {};
    std::array<float, kEnvelopeLUTSize> envPercussive  {};
    std::array<float, kEnvelopeLUTSize> envHann        {};
    std::array<float, kEnvelopeLUTSize> envPad         {};

    void  buildEnvelopeLUTs();
    float getGrainEnvelope (float shape, int age, int length) const;

    //==============================================================================
    // CIRCULAR BUFFER
    std::vector<float> circularBufferL;
    std::vector<float> circularBufferR;
    int   writeHead          = 0;
    int   bufferLength       = 0;
    int   totalSamplesWritten = 0;

    void  writeToCircularBuffer (float sampleL, float sampleR);
    float readFromCircularBuffer (const std::vector<float>& buffer, double position) const;

    //==============================================================================
    // GRAIN POOL
    std::array<Grain, kMaxGrains> grains {};

    Grain& findFreeGrain();

    void spawnGrain (float timeParam,  float sizeParam,  float pitchParam,  float shapeParam,
                     float timeAtten, float sizeAtten, float pitchAtten, float shapeAtten);

    void processGrains (float& outL, float& outR);

    //==============================================================================
    // GRAIN SCHEDULING
    double currentSampleRate    = 44100.0;
    int    samplesSinceLastGrain = 999999;
    int    currentGrainInterval  = 999999;
    double lastSyncSlot          = -1.0;
    bool   frozen                = false;

    int    silenceSampleCount    = 0;
    int    silenceTimeoutSamples = 0;

    int computeGrainInterval      (float densityParam) const;
    int computeSyncedGrainInterval (float densityParam, double bpm) const;
    float getSyncedBeatsPerGrain  (float densityParam) const;

    //==============================================================================
    // PRNG (xorshift32)
    uint32_t prngState = 0x12345678u;
    float nextRandom();

    //==============================================================================
    // DRY BUFFER (for dry/wet mix)
    juce::AudioBuffer<float> dryBuffer;

    //==============================================================================
    // FEEDBACK
    float feedbackSampleL = 0.0f;
    float feedbackSampleR = 0.0f;

    //==============================================================================
    // PARAMETER SMOOTHING
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOutputVol;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDryWet;

    //==============================================================================
    // DATTORRO PLATE REVERB
    struct APF
    {
        std::vector<float> buf;
        int writeIdx  = 0;
        int delayLen  = 1;

        void  init (int bufSize, int delay) { buf.assign(static_cast<size_t>(bufSize), 0.0f); writeIdx = 0; delayLen = delay; }
        float tick (float in, float g) noexcept
        {
            int readIdx = (writeIdx - delayLen + static_cast<int>(buf.size())) % static_cast<int>(buf.size());
            float delayed = buf[static_cast<size_t>(readIdx)];
            float out = -g * in + delayed; // Schroeder allpass: H(z) = (-g + z^-D)/(1 - g*z^-D), gain=1 at all freqs
            buf[static_cast<size_t>(writeIdx)] = in + g * delayed;
            writeIdx = (writeIdx + 1) % static_cast<int>(buf.size());
            return out;
        }
        // Overload with variable delay for LFO modulation (tank APFs only)
        float tick (float in, float g, int delay) noexcept
        {
            int sz = static_cast<int>(buf.size());
            int d = juce::jlimit (1, sz - 1, delay);
            int readIdx = (writeIdx - d + sz) % sz;
            float delayed = buf[static_cast<size_t>(readIdx)];
            float out = -g * in + delayed;
            buf[static_cast<size_t>(writeIdx)] = in + g * delayed;
            writeIdx = (writeIdx + 1) % sz;
            return out;
        }
        float at (int n) const noexcept
        {
            int sz = static_cast<int>(buf.size());
            int idx = (writeIdx - 1 - n + sz * 2) % sz;
            return buf[static_cast<size_t>(idx)];
        }
    };

    struct DL
    {
        std::vector<float> buf;
        int writeIdx = 0;

        void  init (int bufSize) { buf.assign(static_cast<size_t>(bufSize), 0.0f); writeIdx = 0; }
        void  write (float x) noexcept { buf[static_cast<size_t>(writeIdx)] = x; writeIdx = (writeIdx + 1) % static_cast<int>(buf.size()); }
        float at (int n) const noexcept
        {
            int sz = static_cast<int>(buf.size());
            int idx = (writeIdx - 1 - n + sz * 2) % sz;
            return buf[static_cast<size_t>(idx)];
        }
    };

    APF apf[8];   // [0..3] input diffusion, [4..5] L tank, [6..7] R tank
    DL  dl[4];    // [0..1] L tank, [2..3] R tank
    float rvLP1 = 0.0f, rvLP2 = 0.0f, rvFdL = 0.0f, rvFdR = 0.0f;
    int   rvFullDly[4] {};
    int   rvTapL[8]    {};
    int   rvTapR[8]    {};

    // LFO modulation for tank APFs (breaks up metallic resonances)
    double rvLfoPhase1 = 0.0;
    double rvLfoPhase2 = 0.0;
    int    rvBaseDelay4 = 0;
    int    rvBaseDelay6 = 0;

    void initReverb (double sr);
    std::pair<float, float> tickReverb (float inL, float inR, float amount) noexcept;

    //==============================================================================
    // DEBUG COUNTERS
    int debugSampleCounter      = 0;
    int grainsSpawnedThisSecond = 0;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeadyEyeAudioProcessor)
};
