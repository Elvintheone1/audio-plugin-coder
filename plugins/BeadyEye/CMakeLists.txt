# BeadyEye - Granular Texture Processor
# WebView-based UI, custom granular DSP engine

# ============================================
# PLATFORM-SPECIFIC CONFIGURATION
# ============================================
if(WIN32)
    set(PLUGIN_FORMATS VST3 Standalone)
    set(NEEDS_WEBVIEW2 TRUE)
    set(NEEDS_WEB_BROWSER FALSE)
    set(WEBVIEW_BACKEND "WebView2")
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU Standalone)
    set(NEEDS_WEBVIEW2 FALSE)
    set(NEEDS_WEB_BROWSER FALSE)
    set(WEBVIEW_BACKEND "WKWebView")
    set(AU_MAIN_TYPE kAudioUnitType_Effect)
elseif(UNIX)
    set(PLUGIN_FORMATS VST3 LV2 Standalone)
    set(NEEDS_WEBVIEW2 FALSE)
    set(NEEDS_WEB_BROWSER TRUE)
    set(WEBVIEW_BACKEND "WebKitGTK")
    set(LV2_URI "https://github.com/noizefield/audio-plugin-coder/BeadyEye")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "BeadyEye: Building for ${PLUGIN_FORMATS}")
message(STATUS "BeadyEye: WebView backend: ${WEBVIEW_BACKEND}")

# ============================================
# BINARY DATA (Web UI Resources — all JS inlined in index.html)
# ============================================
juce_add_binary_data(BeadyEye_WebUI
    SOURCES
        Source/ui/public/index.html
)

# ============================================
# PLUGIN DEFINITION
# ============================================
juce_add_plugin(BeadyEye
    COMPANY_NAME "Noizefield"
    PLUGIN_MANUFACTURER_CODE Nfld
    PLUGIN_CODE BdEy
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "BeadyEye"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES Fx Delay Modulation
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    LV2URI ${LV2_URI}
)

# ============================================
# SOURCE FILES
# ============================================
target_sources(BeadyEye
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# ============================================
# LINK LIBRARIES
# ============================================
target_link_libraries(BeadyEye
    PRIVATE
        BeadyEye_WebUI
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ============================================
# COMPILE DEFINITIONS
# ============================================
target_compile_definitions(BeadyEye
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        _USE_MATH_DEFINES=1
)

if(WIN32)
    target_compile_definitions(BeadyEye PUBLIC JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
endif()

if(APPLE)
    target_compile_definitions(BeadyEye PUBLIC JUCE_USE_CURL=0)
endif()

if(UNIX AND NOT APPLE)
    target_compile_definitions(BeadyEye PUBLIC JUCE_USE_CURL=0 JUCE_JACK=1)
endif()
