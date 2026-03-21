#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
/**
 * SpliceAudioProcessorEditor — WebView UI
 *
 * CRITICAL member declaration order (prevents DAW crash on unload):
 *   1. Parameter relays    (destroyed LAST)
 *   2. WebBrowserComponent (destroyed middle)
 *   3. Parameter attachments (destroyed FIRST)
 */
class SpliceAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public juce::Timer
{
public:
    SpliceAudioProcessorEditor (SpliceAudioProcessor&);
    ~SpliceAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void timerCallback() override;

private:
    //==========================================================================
    // 1. PARAMETER RELAYS (declared first → destroyed last)

    // Float params
    juce::WebSliderRelay ampAttackRelay  { "amp_attack"  };
    juce::WebSliderRelay ampDecayRelay   { "amp_decay"   };
    juce::WebSliderRelay ampSustainRelay { "amp_sustain" };
    juce::WebSliderRelay ampReleaseRelay { "amp_release" };

    juce::WebSliderRelay filterCutoffRelay { "filter_cutoff" };
    juce::WebSliderRelay filterResRelay    { "filter_res"    };
    juce::WebSliderRelay filterTypeRelay   { "filter_type"   };

    juce::WebSliderRelay fenvDepthRelay  { "fenv_depth"  };
    juce::WebSliderRelay fenvAttackRelay { "fenv_attack" };
    juce::WebSliderRelay fenvDecayRelay  { "fenv_decay"  };

    juce::WebSliderRelay outputVolRelay { "output_vol" };

    // Choice params (treated as float sliders via index)
    juce::WebSliderRelay gridRelay         { "grid"          };
    juce::WebSliderRelay playbackModeRelay { "playback_mode" };
    juce::WebSliderRelay reelDirRelay      { "reel_dir"      };
    juce::WebSliderRelay sliceDirRelay     { "slice_dir"     };

    // Bool param
    juce::WebToggleButtonRelay arpHoldRelay { "arp_hold" };

    // 2. WEBBROWSERCOMPONENT (destroyed middle)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. PARAMETER ATTACHMENTS (declared last → destroyed first)
    std::unique_ptr<juce::WebSliderParameterAttachment> ampAttackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> ampDecayAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> ampSustainAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> ampReleaseAttachment;

    std::unique_ptr<juce::WebSliderParameterAttachment> filterCutoffAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> filterResAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> filterTypeAttachment;

    std::unique_ptr<juce::WebSliderParameterAttachment> fenvDepthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> fenvAttackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> fenvDecayAttachment;

    std::unique_ptr<juce::WebSliderParameterAttachment> outputVolAttachment;

    std::unique_ptr<juce::WebSliderParameterAttachment> gridAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> playbackModeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> reelDirAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> sliceDirAttachment;

    std::unique_ptr<juce::WebToggleButtonParameterAttachment> arpHoldAttachment;

    //==========================================================================
    // Resource provider
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    SpliceAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpliceAudioProcessorEditor)
};
