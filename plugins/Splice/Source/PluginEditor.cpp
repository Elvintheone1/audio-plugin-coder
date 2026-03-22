#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include <unordered_map>

//==============================================================================
SpliceAudioProcessorEditor::SpliceAudioProcessorEditor (SpliceAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // CRITICAL: Create attachments BEFORE WebView (proven pattern from BeadyEye)
    ampAttackAttachment   = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("amp_attack"),   ampAttackRelay);
    ampDecayAttachment    = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("amp_decay"),    ampDecayRelay);
    ampSustainAttachment  = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("amp_sustain"),  ampSustainRelay);
    ampReleaseAttachment  = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("amp_release"),  ampReleaseRelay);

    filterCutoffAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("filter_cutoff"), filterCutoffRelay);
    filterResAttachment    = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("filter_res"),    filterResRelay);
    filterTypeAttachment   = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("filter_type"),   filterTypeRelay);

    fenvDepthAttachment  = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("fenv_depth"),  fenvDepthRelay);
    fenvAttackAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("fenv_attack"), fenvAttackRelay);
    fenvDecayAttachment  = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("fenv_decay"),  fenvDecayRelay);

    outputVolAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("output_vol"), outputVolRelay);
    volDbAttachment     = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("vol_db"),     volDbRelay);
    pitchAttachment     = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("pitch"),      pitchRelay);
    rootNoteAttachment  = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("root_note"),  rootNoteRelay);
    speedAttachment     = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("speed"),      speedRelay);
    panAttachment       = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("pan"),        panRelay);
    panRndAttachment    = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("pan_rnd"),    panRndRelay);
    bpmAttachment       = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("bpm"),        bpmRelay);

    eqLowAttachment     = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("eq_low"),      eqLowRelay);
    eqLowMidAttachment  = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("eq_low_mid"),  eqLowMidRelay);
    eqHighMidAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("eq_high_mid"), eqHighMidRelay);
    eqHighAttachment    = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("eq_high"),     eqHighRelay);

    gridAttachment         = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("grid"),          gridRelay);
    playbackModeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("playback_mode"), playbackModeRelay);
    reelDirAttachment      = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("reel_dir"),      reelDirRelay);
    sliceDirAttachment     = std::make_unique<juce::WebSliderParameterAttachment> (*audioProcessor.apvts.getParameter ("slice_dir"),     sliceDirRelay);

    arpHoldAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (*audioProcessor.apvts.getParameter ("arp_hold"), arpHoldRelay);

    // Create WebView
    webView = std::make_unique<juce::WebBrowserComponent> (
        juce::WebBrowserComponent::Options{}
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (
                juce::WebBrowserComponent::Options::WinWebView2{}
                    .withUserDataFolder (juce::File::getSpecialLocation (
                        juce::File::SpecialLocationType::tempDirectory)))
            .withNativeIntegrationEnabled()
            .withResourceProvider ([this] (const auto& url) { return getResource (url); })
            .withOptionsFrom (ampAttackRelay)
            .withOptionsFrom (ampDecayRelay)
            .withOptionsFrom (ampSustainRelay)
            .withOptionsFrom (ampReleaseRelay)
            .withOptionsFrom (filterCutoffRelay)
            .withOptionsFrom (filterResRelay)
            .withOptionsFrom (filterTypeRelay)
            .withOptionsFrom (fenvDepthRelay)
            .withOptionsFrom (fenvAttackRelay)
            .withOptionsFrom (fenvDecayRelay)
            .withOptionsFrom (outputVolRelay)
            .withOptionsFrom (volDbRelay)
            .withOptionsFrom (pitchRelay)
            .withOptionsFrom (rootNoteRelay)
            .withOptionsFrom (speedRelay)
            .withOptionsFrom (panRelay)
            .withOptionsFrom (panRndRelay)
            .withOptionsFrom (gridRelay)
            .withOptionsFrom (playbackModeRelay)
            .withOptionsFrom (reelDirRelay)
            .withOptionsFrom (sliceDirRelay)
            .withOptionsFrom (arpHoldRelay)
            .withOptionsFrom (bpmRelay)
            .withOptionsFrom (eqLowRelay)
            .withOptionsFrom (eqLowMidRelay)
            .withOptionsFrom (eqHighMidRelay)
            .withOptionsFrom (eqHighRelay)
    );

    addAndMakeVisible (*webView);
    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    // Header bar: Load button + bonus drag-and-drop
    header.onFileDrop  = [this] (const juce::File& f) { audioProcessor.loadReelFile (f); };
    header.onLoadClick = [this] { launchFileChooser(); };
    addAndMakeVisible (header);

    setSize (1000, 630);   // 30px header + 600px WebView
    startTimerHz (15);
}

SpliceAudioProcessorEditor::~SpliceAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void SpliceAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void SpliceAudioProcessorEditor::resized()
{
    const int headerH = 30;
    header.setBounds (0, 0, getWidth(), headerH);
    if (webView) webView->setBounds (0, headerH, getWidth(), getHeight() - headerH);
}

//==============================================================================
void SpliceAudioProcessorEditor::launchFileChooser()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load audio reel",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.mp3;*.ogg;*.flac;*.caf;*.m4a");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (result.existsAsFile())
                audioProcessor.loadReelFile (result);
        });
}

//==============================================================================
void SpliceAudioProcessorEditor::timerCallback()
{
    // One-shot startup: offer to re-open the last loaded reel
    if (! startupDialogShown)
    {
        startupDialogShown = true;
        auto lastFile = audioProcessor.getLastReelFile();
        if (lastFile.existsAsFile())
        {
            juce::Component::SafePointer<SpliceAudioProcessorEditor> safeThis (this);
            juce::MessageManager::callAsync ([safeThis, lastFile]
            {
                if (safeThis == nullptr) return;
                juce::AlertWindow::showAsync (
                    juce::MessageBoxOptions()
                        .withIconType (juce::MessageBoxIconType::QuestionIcon)
                        .withTitle ("Load last reel?")
                        .withMessage (lastFile.getFileName())
                        .withButton ("Load")
                        .withButton ("Skip"),
                    [safeThis, lastFile] (int result)
                    {
                        if (safeThis == nullptr) return;
                        if (result == 1)
                            safeThis->audioProcessor.loadReelFile (lastFile);
                    });
            });
        }
    }

    // Update header bar label
    auto name = audioProcessor.getReelName();
    auto newLabel = name.isEmpty()
        ? juce::String ("no reel loaded")
        : ("\xe2\x97\x89 " + name);  // ◉ filename

    if (header.labelText != newLabel)
    {
        header.labelText = newLabel;
        header.repaint();
    }

    // Push reel name to WebView JS
    if (!webView || !webView->isVisible()) return;
    auto display = name.isEmpty() ? "no reel loaded" : name;

    try
    {
        webView->evaluateJavascript (
            "if (window.updateReelName) window.updateReelName('" +
            display.replace ("'", "\\'") + "');");

        const int slice = audioProcessor.activeSliceIdx.load();
        webView->evaluateJavascript (
            "if (window.updateActiveSlice) window.updateActiveSlice(" +
            juce::String (slice) + ");");
    }
    catch (...) {}
}

//==============================================================================
// RESOURCE PROVIDER — serves embedded BinaryData assets
//==============================================================================

std::optional<juce::WebBrowserComponent::Resource>
SpliceAudioProcessorEditor::getResource (const juce::String& url)
{
    static const std::unordered_map<juce::String, const char*> mimeMap =
    {
        { "html", "text/html"       },
        { "css",  "text/css"        },
        { "js",   "text/javascript" },
        { "json", "application/json"},
        { "png",  "image/png"       },
        { "svg",  "image/svg+xml"   }
    };

    auto resourcePath = url.fromFirstOccurrenceOf (
        juce::WebBrowserComponent::getResourceProviderRoot(), false, false);

    // ── Slice toggle API ─────────────────────────────────────────────────────
    // JS calls: fetch(window.location.origin + '/_/slice/N')
    // On macOS the scheme handler passes only the URL path ("/_/slice/N"), not the full URL,
    // so resourcePath (stripped of root) will be empty. Check url directly.
    if (url.contains ("/_/slice/") || resourcePath.startsWith ("_/slice/"))
    {
        const int idx = url.fromLastOccurrenceOf ("/", false, false).getIntValue();
        if (idx >= 0 && idx < 64)
            audioProcessor.sliceActive[static_cast<size_t> (idx)]
                = ! audioProcessor.sliceActive[static_cast<size_t> (idx)];
        return juce::WebBrowserComponent::Resource ({ std::vector<std::byte> {}, "text/plain" });
    }

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    auto path = resourcePath.substring (1);  // strip leading '/'

    // Determine MIME type
    auto ext  = path.fromLastOccurrenceOf (".", false, false).toLowerCase();
    auto mime = "text/plain";
    auto it   = mimeMap.find (ext);
    if (it != mimeMap.end()) mime = it->second;

    const char* data = nullptr;
    int size = 0;

    if (path == "index.html")
    {
        data = BinaryData::index_html;
        size = BinaryData::index_htmlSize;
        mime = "text/html";
    }

    if (data != nullptr && size > 0)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), mime };
    }

    // Fallback: 404 page
    juce::String fallback = "<!DOCTYPE html><html><body style='background:#1A1A1A;color:#E8E8E8'>"
                            "<h2>Splice: resource not found</h2><p>" + path + "</p></body></html>";
    std::vector<std::byte> fb (static_cast<size_t> (fallback.length()));
    std::memcpy (fb.data(), fallback.toRawUTF8(), static_cast<size_t> (fallback.length()));
    return juce::WebBrowserComponent::Resource { std::move (fb), "text/html" };
}
