#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include <unordered_map>

//==============================================================================
BeadyEyeAudioProcessorEditor::BeadyEyeAudioProcessorEditor (BeadyEyeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // CRITICAL: Attachments BEFORE WebView (proven CloudWash pattern)
    timeAttachment       = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("time"),        timeRelay);
    sizeAttachment       = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("size"),        sizeRelay);
    pitchAttachment      = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("pitch"),       pitchRelay);
    shapeAttachment      = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("shape"),       shapeRelay);
    densityAttachment    = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("density"),     densityRelay);
    feedbackAttachment   = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("feedback"),    feedbackRelay);
    dryWetAttachment     = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("dry_wet"),     dryWetRelay);
    reverbAttachment     = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("reverb"),      reverbRelay);
    outputVolAttachment  = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("output_vol"),  outputVolRelay);
    freezeAttachment     = std::make_unique<juce::WebToggleButtonParameterAttachment>(*audioProcessor.apvts.getParameter("freeze"),       freezeRelay);
    densitySyncAttachment= std::make_unique<juce::WebToggleButtonParameterAttachment>(*audioProcessor.apvts.getParameter("density_sync"), densitySyncRelay);
    timeAttenAttachment  = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("time_atten"),  timeAttenRelay);
    sizeAttenAttachment  = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("size_atten"),  sizeAttenRelay);
    pitchAttenAttachment = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("pitch_atten"), pitchAttenRelay);
    shapeAttenAttachment = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("shape_atten"), shapeAttenRelay);

    // Create WebView
    webView = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options{}
            .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options(
                juce::WebBrowserComponent::Options::WinWebView2{}
                    .withUserDataFolder(juce::File::getSpecialLocation(juce::File::SpecialLocationType::tempDirectory)))
            .withNativeIntegrationEnabled()
            .withResourceProvider([this](const auto& url) { return getResource(url); })
            .withOptionsFrom(timeRelay)
            .withOptionsFrom(sizeRelay)
            .withOptionsFrom(pitchRelay)
            .withOptionsFrom(shapeRelay)
            .withOptionsFrom(densityRelay)
            .withOptionsFrom(feedbackRelay)
            .withOptionsFrom(dryWetRelay)
            .withOptionsFrom(reverbRelay)
            .withOptionsFrom(outputVolRelay)
            .withOptionsFrom(freezeRelay)
            .withOptionsFrom(densitySyncRelay)
            .withOptionsFrom(timeAttenRelay)
            .withOptionsFrom(sizeAttenRelay)
            .withOptionsFrom(pitchAttenRelay)
            .withOptionsFrom(shapeAttenRelay)
    );

    addAndMakeVisible(*webView);
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    setSize (900, 550);
    startTimerHz(30);
}

BeadyEyeAudioProcessorEditor::~BeadyEyeAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void BeadyEyeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void BeadyEyeAudioProcessorEditor::resized()
{
    if (webView) webView->setBounds(getLocalBounds());
}

//==============================================================================
void BeadyEyeAudioProcessorEditor::timerCallback()
{
    if (!webView || !webView->isVisible()) return;

    float inputLevel  = audioProcessor.inputPeakLevel.load();
    float outputLevel = audioProcessor.outputPeakLevel.load();
    int   activeGrains = audioProcessor.activeGrainCount.load();

    try
    {
        webView->evaluateJavascript(
            "if (window.updateMeters) { window.updateMeters(" +
            juce::String(inputLevel, 3) + ", " + juce::String(outputLevel, 3) + "); }");

        webView->evaluateJavascript(
            "if (window.updateGrainVisualization) { window.updateGrainVisualization(" +
            juce::String(activeGrains) + ", 0, 0); }");
    }
    catch (...) {}
}

//==============================================================================
// RESOURCE PROVIDER
//==============================================================================

const char* BeadyEyeAudioProcessorEditor::getMimeForExtension (const juce::String& extension)
{
    static const std::unordered_map<juce::String, const char*> mimeMap =
    {
        { "html", "text/html" },
        { "css",  "text/css" },
        { "js",   "text/javascript" },
        { "json", "application/json" },
        { "png",  "image/png" },
        { "svg",  "image/svg+xml" }
    };
    auto it = mimeMap.find(extension.toLowerCase());
    return (it != mimeMap.end()) ? it->second : "text/plain";
}

juce::String BeadyEyeAudioProcessorEditor::getExtension (juce::String filename)
{
    return filename.fromLastOccurrenceOf(".", false, false);
}

std::optional<juce::WebBrowserComponent::Resource>
BeadyEyeAudioProcessorEditor::getResource (const juce::String& url)
{
    auto resourcePath = url.fromFirstOccurrenceOf(
        juce::WebBrowserComponent::getResourceProviderRoot(), false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    auto path = resourcePath.substring(1);

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
        std::vector<std::byte> bytes(static_cast<size_t>(size));
        std::memcpy(bytes.data(), data, static_cast<size_t>(size));
        return juce::WebBrowserComponent::Resource { std::move(bytes), mime };
    }

    juce::String fallback = "<!DOCTYPE html><html><body style='background:#0A0A0F;color:#F0F0F5'>"
                            "<h1>BeadyEye: Not Found</h1><p>" + path + "</p></body></html>";
    std::vector<std::byte> fb(static_cast<size_t>(fallback.length()));
    std::memcpy(fb.data(), fallback.toRawUTF8(), static_cast<size_t>(fallback.length()));
    return juce::WebBrowserComponent::Resource { std::move(fb), "text/html" };
}
