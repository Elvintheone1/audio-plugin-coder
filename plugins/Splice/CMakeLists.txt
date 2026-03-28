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
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(PLUGIN_FORMATS AUv3 Standalone)
    set(NEEDS_WEBVIEW2 FALSE)
    set(NEEDS_WEB_BROWSER FALSE)
    set(WEBVIEW_BACKEND "WKWebView")
    set(AU_MAIN_TYPE kAudioUnitType_MusicDevice)
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
    set(LV2_URI "https://github.com/ajstudios/audio-plugin-coder/Splice")
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
    COMPANY_NAME              "AJ"
    PLUGIN_MANUFACTURER_CODE  AJzz
    PLUGIN_CODE               Splc
    FORMATS                   ${PLUGIN_FORMATS}
    PRODUCT_NAME              "Splice"
    BUNDLE_ID                 "com.aj.splice"
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    NEEDS_WEBVIEW2            ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER         ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES           "Instrument" "Sampler"
    AU_MAIN_TYPE              ${AU_MAIN_TYPE}
    LV2URI                    ${LV2_URI}
    PLIST_TO_MERGE            "<plist><dict><key>UIFileSharingEnabled</key><true/><key>LSSupportsOpeningDocumentsInPlace</key><true/></dict></plist>"
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
target_link_libraries(Splice
    PRIVATE
        Splice_WebUI
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

if(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_definitions(Splice PUBLIC JUCE_USE_CURL=0)
endif()

if(UNIX AND NOT APPLE)
    target_compile_definitions(Splice PUBLIC JUCE_USE_CURL=0 JUCE_JACK=1)
endif()

# ============================================
# iOS: Code signing + team ID
# ============================================
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    if(DEFINED APC_IOS_TEAM_ID)
        set(_ios_sign_id "Apple Development")
        if(DEFINED APC_IOS_CODE_SIGN_IDENTITY)
            set(_ios_sign_id "${APC_IOS_CODE_SIGN_IDENTITY}")
        endif()
        foreach(_ios_tgt Splice_AUv3 Splice_Standalone Splice)
            if(TARGET ${_ios_tgt})
                set_target_properties(${_ios_tgt} PROPERTIES
                    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM     "${APC_IOS_TEAM_ID}"
                    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY   "${_ios_sign_id}"
                    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "YES"
                )
            endif()
        endforeach()
    endif()
endif()
