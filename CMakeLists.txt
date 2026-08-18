cmake_minimum_required(VERSION 3.22)
project(OpenWav VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/utf-8)
elseif (UNIX AND NOT APPLE)
    add_compile_options(
        -U_FORTIFY_SOURCE
        -D_FORTIFY_SOURCE=0
        -Wno-format-truncation
        -Wno-stringop-overflow
        -Wno-array-bounds
    )
endif()

# Fetch JUCE from GitHub if not already available locally
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
)
FetchContent_MakeAvailable(juce)

if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
endif()

set(PLUGIN_FORMATS VST3 Standalone)
if (APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

# Declare Audio Plugin & Standalone application target
juce_add_plugin(OpenWav
    COMPANY_NAME "OWMB"
    BUNDLE_ID "com.samplaman.owmb"
    MICROPHONE_PERMISSION_TEXT "OWMB requires microphone access for audio sample recording."
    HARDENED_RUNTIME_ENABLED TRUE
    HARDENED_RUNTIME_OPTIONS
        "com.apple.security.cs.allow-jit"
        "com.apple.security.cs.allow-unsigned-executable-memory"
        "com.apple.security.cs.disable-library-validation"
        "com.apple.security.device.audio-input"
    PLUGIN_MANUFACTURER_NAME "OWMB"
    PLUGIN_MANUFACTURER_CODE Owmb
    PLUGIN_CODE Owmb
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "OWMB"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/splashit.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/splashit.png"
)

juce_generate_juce_header(OpenWav)

juce_add_binary_data(OpenWavAssets
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/owmblogo.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/owmbico.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/mainpixeldrainlogo_cropped.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/splashit.png"
)

target_sources(OpenWav PRIVATE
    Source/Models/MediaItem.h
    Source/Database/TagDatabaseManager.h
    Source/Database/TagDatabaseManager.cpp
    Source/Scanner/LibraryScanner.h
    Source/Scanner/LibraryScanner.cpp
    Source/Audio/AudioEngine.h
    Source/Audio/AudioEngine.cpp
    Source/UI/OpenWavLookAndFeel.h
    Source/UI/OpenWavLookAndFeel.cpp
    Source/UI/HeaderBarComponent.h
    Source/UI/HeaderBarComponent.cpp
    Source/UI/TagPanelComponent.h
    Source/UI/TagPanelComponent.cpp
    Source/UI/SampleTableComponent.h
    Source/UI/SampleTableComponent.cpp
    Source/UI/SampleCloudComponent.h
    Source/UI/SampleCloudComponent.cpp
    Source/UI/LibrariesComponent.h
    Source/UI/LibrariesComponent.cpp
    Source/UI/WaveformTransportComponent.h
    Source/UI/WaveformTransportComponent.cpp
    Source/UI/RecorderComponent.h
    Source/UI/RecorderComponent.cpp
    Source/UI/ScanProgressDialog.h
    Source/UI/ScanProgressDialog.cpp
    Source/UI/AboutDialog.h
    Source/UI/AboutDialog.cpp
    Source/UI/ConvertDialog.h
    Source/UI/ConvertDialog.cpp
    Source/UI/SimilarityGraphPopup.h
    Source/UI/SimilarityGraphPopup.cpp
    Source/UI/AnalysisComponent.h
    Source/UI/AnalysisComponent.cpp
    Source/UI/EditComponent.h
    Source/UI/EditComponent.cpp
    Source/UI/SampleMapComponent.h
    Source/UI/SampleMapComponent.cpp
    Source/UI/PerformanceComponent.h
    Source/UI/PerformanceComponent.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
)

target_compile_definitions(OpenWav PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1
)

if (UNIX AND NOT APPLE)
    target_compile_definitions(OpenWav PRIVATE JUCE_USE_CURL=1)
    find_package(CURL REQUIRED)
    target_link_libraries(OpenWav PRIVATE ${CURL_LIBRARIES})
else()
    target_compile_definitions(OpenWav PRIVATE JUCE_USE_CURL=0)
endif()

target_link_libraries(OpenWav PRIVATE
    OpenWavAssets
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
    juce::juce_opengl
    )
