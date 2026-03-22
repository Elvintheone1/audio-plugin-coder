# Splice — Musique Concrète Loop Slicer
# WebView-based UI, MIDI-driven slice playback engine

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
    set(AU_MAIN_TYPE kAudioUnitType_MusicDevice)
elseif(UNIX)
    set(PLUGIN_FORMATS VST3 LV2 Standalone)
    set(NEEDS_WEBVIEW2 FALSE)
    set(NEEDS_WEB_BROWSER TRUE)
    set(WEBVIEW_BACKEND "WebKitGTK")
    set(LV2_URI "https://github.com/noizefield/audio-plugin-coder/Splice")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "Splice: Building for ${PLUGIN_FORMATS}")
message(STATUS "Splice: WebView backend: ${WEBVIEW_BACKEND}")

# ============================================
# BINARY DATA (Web UI — all JS inlined in index.html)
# ============================================
juce_add_binary_data(Splice_WebUI
    SOURCES
        Source/ui/public/index.html
)

# ============================================
# PLUGIN DEFINITION
# ============================================
juce_add_plugin(Splice
    COMPANY_NAME              "Noizefield"
    PLUGIN_MANUFACTURER_CODE  AJzz
    PLUGIN_CODE               Splc
    FORMATS                   ${PLUGIN_FORMATS}
    PRODUCT_NAME              "Splice"
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    NEEDS_WEBVIEW2            ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER         ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES           "Instrument" "Sampler"
    AU_MAIN_TYPE              ${AU_MAIN_TYPE}
    LV2URI                    ${LV2_URI}
)

# ============================================
# SOURCE FILES
# ============================================
target_sources(Splice
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# ============================================
# LINK LIBRARIES
# ============================================
if(RUBBERBAND_FOUND)
    target_compile_definitions(Splice PRIVATE SPLICE_HAS_RUBBERBAND=1)
    target_include_directories(Splice PRIVATE ${RUBBERBAND_INCLUDE_DIRS})
endif()

target_link_libraries(Splice
    PRIVATE
        Splice_WebUI
        $<$<BOOL:${RUBBERBAND_FOUND}>:PkgConfig::RUBBERBAND>
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
target_compile_definitions(Splice
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        _USE_MATH_DEFINES=1
)

if(WIN32)
    target_compile_definitions(Splice PUBLIC JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
endif()

if(APPLE)
    target_compile_definitions(Splice PUBLIC JUCE_USE_CURL=0)
endif()

if(UNIX AND NOT APPLE)
    target_compile_definitions(Splice PUBLIC JUCE_USE_CURL=0 JUCE_JACK=1)
endif()
