# GestureCV - Hand Gesture to MIDI CC Router
# MIDI Effect plugin: receives OSC from Python gesture engine, outputs MIDI CC

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
    set(AU_MAIN_TYPE kAudioUnitType_MIDIProcessor)
elseif(UNIX)
    set(PLUGIN_FORMATS VST3 Standalone)
    set(NEEDS_WEBVIEW2 FALSE)
    set(NEEDS_WEB_BROWSER TRUE)
    set(WEBVIEW_BACKEND "WebKitGTK")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "GestureCV: Building for ${PLUGIN_FORMATS}")

juce_add_binary_data(GestureCV_WebUI
    SOURCES
        Source/ui/public/index.html
)

juce_add_plugin(GestureCV
    COMPANY_NAME "AJ"
    PLUGIN_MANUFACTURER_CODE AJzz
    PLUGIN_CODE GsCv
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "GestureCV"
    IS_MIDI_EFFECT TRUE
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES Fx Tools
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
)

target_sources(GestureCV
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_link_libraries(GestureCV
    PRIVATE
        GestureCV_WebUI
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_osc
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(GestureCV
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        _USE_MATH_DEFINES=1
)

if(WIN32)
    target_compile_definitions(GestureCV PUBLIC JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
endif()

if(APPLE)
    target_compile_definitions(GestureCV PUBLIC JUCE_USE_CURL=0)
endif()
