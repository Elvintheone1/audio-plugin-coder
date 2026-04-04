#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class GestureCVEditor : public juce::AudioProcessorEditor,
                         public juce::Timer
{
public:
    explicit GestureCVEditor (GestureCVProcessor&);
    ~GestureCVEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    GestureCVProcessor& processor;

    std::unique_ptr<juce::WebBrowserComponent> webView;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    void pushLanesToJS();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GestureCVEditor)
};
