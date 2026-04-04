#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

static const char* kLaneIds[kNumLanes] = {
    "hand_x", "hand_y", "hand_z", "spread", "pinch", "wrist_tilt", "curl_index", "curl_ring"
};

static const int kDefaultCC[kNumLanes] = { 1, 11, 74, 71, 73, 91, 70, 72 };

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout GestureCVProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < kNumLanes; ++i)
    {
        juce::String id (kLaneIds[i]);
        layout.add (std::make_unique<juce::AudioParameterInt>   (id + "_cc",     id + " CC",     0,   127, kDefaultCC[i]));
        layout.add (std::make_unique<juce::AudioParameterFloat> (id + "_scale",  id + " Scale",  0.0f, 2.0f, 1.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (id + "_offset", id + " Offset", -1.0f, 1.0f, 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (id + "_curve",  id + " Curve",  -1.0f, 1.0f, 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (id + "_smooth", id + " Smooth", 0.0f,  1.0f, 0.2f));
    }

    layout.add (std::make_unique<juce::AudioParameterInt>   ("osc_port",      "OSC Port",       1024, 65535, 9000));
    layout.add (std::make_unique<juce::AudioParameterInt>   ("midi_channel",  "MIDI Channel",   1,    16,    1));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("global_smooth", "Global Smooth",  0.0f, 1.0f,  0.0f));

    return layout;
}

//==============================================================================
GestureCVProcessor::GestureCVProcessor()
    : AudioProcessor (BusesProperties()),  // MIDI effect — no audio buses
      apvts (*this, nullptr, "GestureCVState", createParameterLayout())
{
    for (int i = 0; i < kNumLanes; ++i)
    {
        laneValues[i].store (0.0f);
        laneSmoothed[i] = 0.0f;
        lastCcVal[i]    = -1;

        juce::String id (kLaneIds[i]);
        laneParams[i].ccNumber = dynamic_cast<juce::AudioParameterInt*>   (apvts.getParameter (id + "_cc"));
        laneParams[i].scale    = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (id + "_scale"));
        laneParams[i].offset   = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (id + "_offset"));
        laneParams[i].curve    = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (id + "_curve"));
        laneParams[i].smooth   = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (id + "_smooth"));
    }

    oscPortParam      = dynamic_cast<juce::AudioParameterInt*>   (apvts.getParameter ("osc_port"));
    midiChannelParam  = dynamic_cast<juce::AudioParameterInt*>   (apvts.getParameter ("midi_channel"));
    globalSmoothParam = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("global_smooth"));

    oscReceiver.addListener (this);
    rebindOSC();
}

GestureCVProcessor::~GestureCVProcessor() noexcept
{
    oscReceiver.disconnect();
}

//==============================================================================
void GestureCVProcessor::rebindOSC()
{
    int port = oscPortParam ? (int)*oscPortParam : 9000;
    if (port == currentOscPort && oscConnected.load())
        return;

    oscReceiver.disconnect();
    oscConnected.store (oscReceiver.connect (port));
    currentOscPort = port;
}

void GestureCVProcessor::oscMessageReceived (const juce::OSCMessage& message)
{
    if (message.getAddressPattern().toString() != "/gesture/lanes")
        return;
    if (message.size() < kNumLanes)
        return;

    for (int i = 0; i < kNumLanes; ++i)
    {
        if (message[i].isFloat32())
            laneValues[i].store (message[i].getFloat32());
    }

    oscPacketsReceived.fetch_add (1, std::memory_order_relaxed);
}

//==============================================================================
void GestureCVProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    laneSmoothed.fill (0.0f);
    lastCcVal.fill (-1);
    rebindOSC();
}

//==============================================================================
float GestureCVProcessor::applyCurve (float x, float curve) const noexcept
{
    x = juce::jlimit (0.0f, 1.0f, x);
    if (curve > 0.0f)
        return std::pow (x, 1.0f + curve * 3.0f);
    if (curve < 0.0f)
        return 1.0f - std::pow (1.0f - x, 1.0f - curve * 3.0f);
    return x;
}

//==============================================================================
void GestureCVProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();

    // Check if OSC port changed
    if (oscPortParam && (int)*oscPortParam != currentOscPort)
        rebindOSC();

    const int midiCh = midiChannelParam ? (int)*midiChannelParam : 1;

    for (int i = 0; i < kNumLanes; ++i)
    {
        auto& p = laneParams[i];
        if (!p.ccNumber) continue;

        // Read directly — smoothing is handled by one-euro filter in Python
        float raw = laneValues[i].load (std::memory_order_relaxed);

        // Curve
        float curved = applyCurve (raw, p.curve->get());

        // Scale + offset
        float out = juce::jlimit (0.0f, 1.0f, curved * p.scale->get() + p.offset->get());

        // Quantize
        int ccVal = juce::roundToInt (out * 127.0f);

        if (ccVal == lastCcVal[i])
            continue;

        lastCcVal[i] = ccVal;

        // Emit MIDI CC at sample 0
        midi.addEvent (juce::MidiMessage::controllerEvent (midiCh, (int)*p.ccNumber, ccVal), 0);
    }
}

//==============================================================================
juce::AudioProcessorEditor* GestureCVProcessor::createEditor()
{
    return new GestureCVEditor (*this);
}

void GestureCVProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GestureCVProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GestureCVProcessor();
}
