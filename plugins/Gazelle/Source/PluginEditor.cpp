#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include <unordered_map>

//==============================================================================
GazelleAudioProcessorEditor::GazelleAudioProcessorEditor (GazelleAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // ── Create attachments FIRST (before WebBrowserComponent) ────────────────
    // Filter
    cutoffAttachment        = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("cutoff"),        cutoffRelay);
    resonanceAttachment     = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("resonance"),     resonanceRelay);
    spreadAttachment        = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("spread"),        spreadRelay);
    filter1ModeAttachment   = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("filter1_mode"),  filter1ModeRelay);
    filter2ModeAttachment   = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("filter2_mode"),  filter2ModeRelay);
    filter1LevelAttachment  = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("filter1_level"), filter1LevelRelay);
    filter2LevelAttachment  = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("filter2_level"), filter2LevelRelay);

    // Envelopes
    attack1Attachment       = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("attack1"), attack1Relay);
    decay1Attachment        = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("decay1"),  decay1Relay);
    attack2Attachment       = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("attack2"), attack2Relay);
    decay2Attachment        = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("decay2"),  decay2Relay);

    // Pitch sweep
    pitch1Attachment        = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("pitch1"), pitch1Relay);
    pitch2Attachment        = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("pitch2"), pitch2Relay);

    // Saturation + EQ + VCA
    saturationAttachment    = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("saturation"),    saturationRelay);
    eqBassAttachment        = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("eq_bass"),       eqBassRelay);
    eqMidAttachment         = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("eq_mid"),        eqMidRelay);
    eqTrebleAttachment      = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("eq_treble"),     eqTrebleRelay);
    outputLevelAttachment   = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("output_level"),  outputLevelRelay);

    // FX
    fxTypeAttachment        = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("fx_type"), fxTypeRelay);
    fxP1Attachment          = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("fx_p1"),   fxP1Relay);
    fxP2Attachment          = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("fx_p2"),   fxP2Relay);
    fxWetAttachment         = std::make_unique<juce::WebSliderParameterAttachment>
        (*audioProcessor.apvts.getParameter ("fx_wet"),  fxWetRelay);

    // Triggers
    trigger1Attachment      = std::make_unique<juce::WebToggleButtonParameterAttachment>
        (*audioProcessor.apvts.getParameter ("trigger1"), trigger1Relay);
    trigger2Attachment      = std::make_unique<juce::WebToggleButtonParameterAttachment>
        (*audioProcessor.apvts.getParameter ("trigger2"), trigger2Relay);

    // ── Create WebBrowserComponent AFTER all attachments ──────────────────────
    webView = std::make_unique<juce::WebBrowserComponent> (
        juce::WebBrowserComponent::Options{}
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (
                juce::WebBrowserComponent::Options::WinWebView2{}
                    .withUserDataFolder (
                        juce::File::getSpecialLocation (
                            juce::File::SpecialLocationType::tempDirectory)))
            .withNativeIntegrationEnabled()
            .withResourceProvider ([this] (const auto& url) { return getResource (url); })
            // Filter relays
            .withOptionsFrom (cutoffRelay)
            .withOptionsFrom (resonanceRelay)
            .withOptionsFrom (spreadRelay)
            .withOptionsFrom (filter1ModeRelay)
            .withOptionsFrom (filter2ModeRelay)
            .withOptionsFrom (filter1LevelRelay)
            .withOptionsFrom (filter2LevelRelay)
            // Envelope relays
            .withOptionsFrom (attack1Relay)
            .withOptionsFrom (decay1Relay)
            .withOptionsFrom (attack2Relay)
            .withOptionsFrom (decay2Relay)
            // Pitch sweep relays
            .withOptionsFrom (pitch1Relay)
            .withOptionsFrom (pitch2Relay)
            // Saturation + EQ + VCA relays
            .withOptionsFrom (saturationRelay)
            .withOptionsFrom (eqBassRelay)
            .withOptionsFrom (eqMidRelay)
            .withOptionsFrom (eqTrebleRelay)
            .withOptionsFrom (outputLevelRelay)
            // FX relays
            .withOptionsFrom (fxTypeRelay)
            .withOptionsFrom (fxP1Relay)
            .withOptionsFrom (fxP2Relay)
            .withOptionsFrom (fxWetRelay)
            // Trigger relays
            .withOptionsFrom (trigger1Relay)
            .withOptionsFrom (trigger2Relay)
    );

    addAndMakeVisible (*webView);
    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setSize (1000, 500);
    startTimerHz (30);
}

GazelleAudioProcessorEditor::~GazelleAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void GazelleAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0E0E0E));
}

void GazelleAudioProcessorEditor::resized()
{
    if (webView) webView->setBounds (getLocalBounds());
}

//==============================================================================
void GazelleAudioProcessorEditor::timerCallback()
{
    // Reserved for future metering — Gazelle has no metering UI in v1
    (void) audioProcessor;
}

//==============================================================================
// RESOURCE PROVIDER — serves embedded HTML to the WebView
//==============================================================================

const char* GazelleAudioProcessorEditor::getMimeForExtension (const juce::String& extension)
{
    static const std::unordered_map<juce::String, const char*> mimeMap =
    {
        { "html", "text/html"        },
        { "css",  "text/css"         },
        { "js",   "text/javascript"  },
        { "json", "application/json" },
        { "png",  "image/png"        },
        { "svg",  "image/svg+xml"    }
    };
    auto it = mimeMap.find (extension.toLowerCase());
    return (it != mimeMap.end()) ? it->second : "text/plain";
}

juce::String GazelleAudioProcessorEditor::getExtension (juce::String filename)
{
    return filename.fromLastOccurrenceOf (".", false, false);
}

std::optional<juce::WebBrowserComponent::Resource>
GazelleAudioProcessorEditor::getResource (const juce::String& url)
{
    auto resourcePath = url.fromFirstOccurrenceOf (
        juce::WebBrowserComponent::getResourceProviderRoot(), false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    auto path = resourcePath.substring (1);

    const char* data = nullptr;
    int size = 0;
    juce::String mime;

    if (path == "index.html")
    {
        data = BinaryData::index_html;
        size = BinaryData::index_htmlSize;
        mime = "text/html";
    }

    if (data && size > 0)
    {
        std::vector<std::byte> bytes (static_cast<size_t>(size));
        std::memcpy (bytes.data(), data, static_cast<size_t>(size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), mime };
    }

    // Fallback
    juce::String fallback = "<!DOCTYPE html><html><body style='background:#0E0E0E;color:#C4960A'>"
                            "<h2>Gazelle: resource not found</h2><p>" + path + "</p></body></html>";
    std::vector<std::byte> fb (static_cast<size_t>(fallback.length()));
    std::memcpy (fb.data(), fallback.toRawUTF8(), static_cast<size_t>(fallback.length()));
    return juce::WebBrowserComponent::Resource { std::move (fb), "text/html" };
}
