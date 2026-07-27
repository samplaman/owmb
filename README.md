# OpenWav - JUCE Tag-Based Media Browser Audio Plugin

**OpenWav** is an open-source, cross-platform audio plugin (VST3, AU, Standalone) and media browser built with JUCE and C++17. It allows music producers, sound designers, and sample collectors to index, tag, search, audition, and organize `.wav`, `.mp3`, `.flac`, `.ogg`, and `.aiff` audio files, with seamless drag-and-drop into DAWs (Ableton Live, FL Studio, Logic Pro, Reaper, Cubase, Bitwig).

---

## Key Features

- 🏷️ **Tag-Based Searching & Auto-Inference**: Automatically infers tags from folder structures and filenames (`#Kick`, `#Snare`, `#HiHat`, `#Loop`, `#OneShot`, `#Bass`, `#Synth`, `#808`, `#Vocal`, `#FX`, etc.).
- 🔍 **Instant Text & Multi-Tag Filters**: Combine text search, tag clouds (AND/OR mode), format filters (`.WAV`, `.MP3`, `.FLAC`, `.OGG`, `.AIFF`), and favorites/ratings.
- ⚡ **Asynchronous Non-Blocking Scanner**: Multithreaded background directory scanner that indexes large sample libraries instantly without freezing the DAW UI.
- 🌊 **Interactive Waveform Preview**: High-resolution waveform visualization with playhead scrubbing, loop toggle, auto-play on select, and gain/volume control.
- 🎛️ **DAW Drag-and-Drop**: Drag audio samples directly from the sample table or transport waveform into any DAW.
- 📁 **Folder Drag-and-Drop Target**: Drag any folder into the OpenWav plugin window to immediately index its audio contents.
- 🎨 **Modern Dark Aesthetic**: Sleek obsidian dark-mode interface with cyan accents, rounded pill tags, and customizable rating stars.

---

## Building OpenWav

### Requirements
- **CMake** 3.22 or higher
- C++17 compatible compiler (Visual Studio 2022 / MSVC, Clang, or GCC)
- **Git** (for automatically fetching JUCE 8 via CMake `FetchContent`)

### Build Steps

```bash
# 1. Clone or navigate to directory
cd openwav

# 2. Configure build directory with CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Compile VST3 & Standalone targets
cmake --build build --config Release
```

The compiled binaries will be output in:
- **VST3 Plugin**: `build/OpenWav_artefacts/Release/VST3/OpenWav Media Browser.vst3`
- **Standalone App**: `build/OpenWav_artefacts/Release/Standalone/OpenWav Media Browser.exe`

---

## Architecture Overview

```
openwav/
├── CMakeLists.txt              # CMake build configuration fetching JUCE 8
└── Source/
    ├── Audio/                  # Sample playback & transport engine
    │   ├── AudioEngine.h
    │   └── AudioEngine.cpp
    ├── Database/               # Persistent JSON metadata library index & tag manager
    │   ├── TagDatabaseManager.h
    │   └── TagDatabaseManager.cpp
    ├── Models/                 # MediaItem data structure & serialization
    │   └── MediaItem.h
    ├── Scanner/                # Asynchronous multi-threaded directory scanner
    │   ├── LibraryScanner.h
    │   └── LibraryScanner.cpp
    ├── UI/                     # Custom LookAndFeel & modern UI components
    │   ├── HeaderBarComponent.h / .cpp
    │   ├── TagPanelComponent.h / .cpp
    │   ├── SampleTableComponent.h / .cpp
    │   ├── WaveformTransportComponent.h / .cpp
    │   └── OpenWavLookAndFeel.h / .cpp
    ├── PluginProcessor.h / .cpp # JUCE AudioProcessor lifecycle
    └── PluginEditor.h / .cpp    # JUCE AudioProcessorEditor window host
```

---

## License

OpenWav is open-source under the MIT License.
