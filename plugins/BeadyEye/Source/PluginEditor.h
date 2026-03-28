#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
/**
 * BeadyEye Plugin Editor — WebView UI
 *
 * CRITICAL member declaration order (prevents DAW crash on unload):
 * 1. Parameter relays    (destroyed LAST)
 * 2. WebBrowserComponent (destroyed middle)
 * 3. Parameter attachments (destroyed FIRST)
 */
class BeadyEyeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::Timer
{
public:
    BeadyEyeAudioProcessorEditor (BeadyEyeAudioProcessor&);
    ~BeadyEyeAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void timerCallback() override;

private:
    //==========================================================================
    // 1. PARAMETER RELAYS (destroyed last)
    juce::WebSliderRelay       timeRelay       { "time" };
    juce::WebSliderRelay       sizeRelay       { "size" };
    juce::WebSliderRelay       pitchRelay      { "pitch" };
    juce::WebSliderRelay       shapeRelay      { "shape" };
    juce::WebSliderRelay       densityRelay    { "density" };
    juce::WebSliderRelay       feedbackRelay   { "feedback" };
    juce::WebSliderRelay       dryWetRelay     { "dry_wet" };
    juce::WebSliderRelay       reverbRelay     { "reverb" };
    juce::WebSliderRelay       outputVolRelay  { "output_vol" };
    juce::WebToggleButtonRelay freezeRelay     { "freeze" };
    juce::WebToggleButtonRelay densitySyncRelay{ "density_sync" };
    juce::WebSliderRelay       timeAttenRelay  { "time_atten" };
    juce::WebSliderRelay       sizeAttenRelay  { "size_atten" };
    juce::WebSliderRelay       pitchAttenRelay { "pitch_atten" };
    juce::WebSliderRelay       shapeAttenRelay { "shape_atten" };

    // 2. WEBBROWSERCOMPONENT (destroyed middle)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. PARAMETER ATTACHMENTS (destroyed first)
    std::unique_ptr<juce::WebSliderParameterAttachment>       timeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       sizeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       pitchAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       shapeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       densityAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       feedbackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       dryWetAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       reverbAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       outputVolAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> freezeAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> densitySyncAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       timeAttenAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       sizeAttenAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       pitchAttenAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       shapeAttenAttachment;

    //==========================================================================
    // Resource provider
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static const char* getMimeForExtension (const juce::String& extension);
    static juce::String getExtension (juce::String filename);

    BeadyEyeAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeadyEyeAudioProcessorEditor)
};
