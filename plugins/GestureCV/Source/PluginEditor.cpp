#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

GestureCVEditor::GestureCVEditor (GestureCVProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    webView = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options{}
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (
                juce::WebBrowserComponent::Options::WinWebView2{}
                    .withUserDataFolder (juce::File::getSpecialLocation (
                        juce::File::SpecialLocationType::tempDirectory)))
            .withNativeIntegrationEnabled()
            .withResourceProvider ([this] (const auto& url) { return getResource (url); }));

    addAndMakeVisible (*webView);
    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setSize (900, 480);
    startTimerHz (30);
}

GestureCVEditor::~GestureCVEditor()
{
    stopTimer();
}

void GestureCVEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d0d));
}

void GestureCVEditor::resized()
{
    if (webView) webView->setBounds (getLocalBounds());
}

void GestureCVEditor::timerCallback()
{
    pushLanesToJS();
}

std::optional<juce::WebBrowserComponent::Resource>
GestureCVEditor::getResource (const juce::String& url)
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
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), mime };
    }

    return std::nullopt;
}

void GestureCVEditor::pushLanesToJS()
{
    juce::String json = "[";
    for (int i = 0; i < kNumLanes; ++i)
    {
        json += juce::String (processor.laneValues[i].load(), 3);
        if (i < kNumLanes - 1) json += ",";
    }
    json += "]";

    bool connected = processor.oscConnected.load();
    juce::String js = "if(window.updateLanes) window.updateLanes("
                      + json + "," + (connected ? "true" : "false") + ");";

    webView->evaluateJavascript (js, {});
}
