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
    juce::WebSliderRelay filter1ModeRelay   { "filter1_mode" };
    juce::WebSliderRelay filter2ModeRelay   { "filter2_mode" };
    juce::WebSliderRelay filter1LevelRelay  { "filter1_level" };
    juce::WebSliderRelay filter2LevelRelay  { "filter2_level" };

    // Envelopes
    juce::WebSliderRelay attack1Relay       { "attack1" };
    juce::WebSliderRelay decay1Relay        { "decay1" };
    juce::WebSliderRelay attack2Relay       { "attack2" };
    juce::WebSliderRelay decay2Relay        { "decay2" };

    // Pitch sweep
    juce::WebSliderRelay pitch1Relay        { "pitch1" };
    juce::WebSliderRelay pitch2Relay        { "pitch2" };

    // Saturation + EQ + VCA
    juce::WebSliderRelay saturationRelay    { "saturation" };
    juce::WebSliderRelay eqBassRelay        { "eq_bass" };
    juce::WebSliderRelay eqMidRelay         { "eq_mid" };
    juce::WebSliderRelay eqTrebleRelay      { "eq_treble" };
    juce::WebSliderRelay outputLevelRelay   { "output_level" };

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
    std::unique_ptr<juce::WebSliderParameterAttachment>       filter1ModeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       filter2ModeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       filter1LevelAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       filter2LevelAttachment;

    // Envelopes
    std::unique_ptr<juce::WebSliderParameterAttachment>       attack1Attachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       decay1Attachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       attack2Attachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       decay2Attachment;

    // Pitch sweep
    std::unique_ptr<juce::WebSliderParameterAttachment>       pitch1Attachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       pitch2Attachment;

    // Saturation + EQ + VCA
    std::unique_ptr<juce::WebSliderParameterAttachment>       saturationAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       eqBassAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       eqMidAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       eqTrebleAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       outputLevelAttachment;

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
