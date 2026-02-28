#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
class GazelleAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit GazelleAudioProcessorEditor (GazelleAudioProcessor&);
    ~GazelleAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    void timerCallback() override;

    std::optional<juce::WebBrowserComponent::Resource>
        getResource (const juce::String& url);

    static const char* getMimeForExtension (const juce::String& extension);
    static juce::String getExtension (juce::String filename);

    GazelleAudioProcessor& audioProcessor;

    //==========================================================================
    // ── 1. RELAYS (must be declared BEFORE webView) ──────────────────────────
    // Critical: C++ destroys in reverse-declaration order.
    // WebView must be destroyed BEFORE relays.

    // Filter
    juce::WebSliderRelay cutoffRelay        { "cutoff" };
    juce::WebSliderRelay resonanceRelay     { "resonance" };
    juce::WebSliderRelay spreadRelay        { "spread" };
    juce::WebSliderRelay filterModeRelay    { "filter_mode" };
    juce::WebSliderRelay filter1LevelRelay  { "filter1_level" };
    juce::WebSliderRelay filter2LevelRelay  { "filter2_level" };

    // Envelopes
    juce::WebSliderRelay attack1Relay       { "attack1" };
    juce::WebSliderRelay decay1Relay        { "decay1" };
    juce::WebSliderRelay attack2Relay       { "attack2" };
    juce::WebSliderRelay decay2Relay        { "decay2" };

    // Distortion
    juce::WebSliderRelay driveRelay         { "drive" };
    juce::WebSliderRelay distAmountRelay    { "dist_amount" };
    juce::WebSliderRelay distFeedbackRelay  { "dist_feedback" };
    juce::WebToggleButtonRelay feedbackPathRelay { "feedback_path" };
    juce::WebSliderRelay eqTiltRelay        { "eq_tilt" };

    // FX
    juce::WebSliderRelay fxTypeRelay        { "fx_type" };
    juce::WebSliderRelay fxP1Relay          { "fx_p1" };
    juce::WebSliderRelay fxP2Relay          { "fx_p2" };
    juce::WebSliderRelay fxWetRelay         { "fx_wet" };

    // Triggers
    juce::WebToggleButtonRelay trigger1Relay { "trigger1" };
    juce::WebToggleButtonRelay trigger2Relay { "trigger2" };

    //==========================================================================
    // ── 2. WEBVIEW (declared after relays) ───────────────────────────────────
    std::unique_ptr<juce::WebBrowserComponent> webView;

    //==========================================================================
    // ── 3. ATTACHMENTS (declared after webView) ───────────────────────────────

    // Filter
    std::unique_ptr<juce::WebSliderParameterAttachment>       cutoffAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       resonanceAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       spreadAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       filterModeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       filter1LevelAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       filter2LevelAttachment;

    // Envelopes
    std::unique_ptr<juce::WebSliderParameterAttachment>       attack1Attachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       decay1Attachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       attack2Attachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       decay2Attachment;

    // Distortion
    std::unique_ptr<juce::WebSliderParameterAttachment>       driveAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       distAmountAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       distFeedbackAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> feedbackPathAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       eqTiltAttachment;

    // FX
    std::unique_ptr<juce::WebSliderParameterAttachment>       fxTypeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       fxP1Attachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       fxP2Attachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       fxWetAttachment;

    // Triggers
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> trigger1Attachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> trigger2Attachment;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GazelleAudioProcessorEditor)
};
