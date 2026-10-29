# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.1.0] - 2026-10-28

### Added
* **Collapsible Scanned Folders Overview (`FolderBrowserComponent`)**:
  * New dedicated folder hierarchy browser displaying registered scanned folders and subfolders.
  * Hierarchical tree list with smooth kinetic scrolling, real-time sample count badges, and subfolder tallies.
  * Smart auto-unfolding of empty single-child folder chains.
  * Top toolbar with **Expand All** and **Collapse All** controls.
  * Sample row integration with 2× Retina play/pause indicators, tag badges, duration, format, sample rate, interactive 5-star ratings, and favorite toggles.
  * External drag-and-drop from folder rows directly into external DAWs (Ableton Live, FL Studio, etc.).
  * Keyboard navigation (`Up`/`Down` auditioning, `Right`/`Left` expand/collapse).
  * Folder right-click context menu: *Reveal in Explorer*, *Rescan Folder*, and *Remove from Scanned Folders*.
* **Header View Switcher (`HeaderBarComponent`)**:
  * Added `ViewMode::Folders` button positioned between **List** and **Cloud** views.
  * Custom Feather-style vector folder SVG icon in `OpenWavLookAndFeel`.
* **Galaxy View Mouseover Auditioning (`SampleCloudComponent`)**:
  * Added on/off toggle button in the top-left map overlay for hands-free hover auditioning.
  * Expanded trigger radius specifically for mouseover mode so fast cursor movement across clusters reliably triggers audio.
* **Safety Safeguards & Confirmation Prompts**:
  * Added an asynchronous confirmation dialog before removing a folder from Scanned Folders (both via the left panel `"X"` button and via right-click context menus).
  * Clear explanation in dialog that audio files on disk remain safe and only library database indices are cleared.
* **Compound Tag & Semantic Keywords**:
  * Added recognition for compound drum types: `sub_kick`, `bass_drum`, `open_hihat`, `closed_hihat`, `side_stick`, and `clave`.
  * Added negation rule handling (e.g., `no_kick`).

### Changed
* **Single Source of Truth (SSoT) Category Engine (`TagDatabaseManager`)**:
  * Unified semantic categorization across all views and query algorithms (`getPrimaryCategoryTag`, `getCanonicalCategory`, `getCanonicalCategoryForItem`, `areCategoriesCompatible`, `classifyFromAudioFeatures`).
  * Re-tagging now preserves manual user tags and acoustic DSP tags.
* **Acoustic Feature Extraction & Header Parsing (`LibraryScanner`)**:
  * Standardized audio loop window to 4.5 seconds with leading and trailing silence trimming.
  * Calibrated physical crossover frequency bands at 140 Hz, 800 Hz, and 4 kHz.
  * Standardized Zero-Crossing Rate (`zcr`) and High Frequency Ratio (`highFreqRatio`) against a 44.1 kHz grid to ensure sample-rate invariance.
  * Dynamic RIFF chunk-walking in `parseWavHeaderFast` to correctly handle non-standard header orders (`JUNK`, `bext`, `iXML`, `LIST` preceding `fmt `) with 2-byte word alignment.
* **Galaxy View Snapping & Highlighting**:
  * Fine-tuned magnetic cursor snapping at high zoom levels to prevent skipping over intermediate samples in dense clusters.
  * Enlarged cursor proximity highlight glow.
* **Path-Prefix Boundary Matching**:
  * Tightened `TagDatabaseManager::removeScanFolder` to enforce strict trailing path separator boundaries, preventing removal of `C:\Samples` from accidentally wiping sibling folders like `C:\Samples2`.
* **Build System Decoupling**:
  * Added CMake option `OWMB_COPY_PLUGIN_AFTER_BUILD` to allow isolated local builds without overwriting the system VST3 plugin directory.

### Fixed
* **Audio Auditioning State**:
  * Fixed sample re-click behavior so clicking an already playing sample resets and restarts playback cleanly from the start rather than spawning overlapping voices.
* **Similar Sounds Carousel Hitbox**:
  * Expanded the click trigger hitbox on the mini-list previous/next carousel buttons across the entire button surface.
* **Musical Key & BPM Detection**:
  * Fixed key extraction false positives from technical abbreviations (e.g. `FM` synthesis vs. `Fm` minor, `-6dB` vs. `Db`, isolated characters `A`–`G`).
  * Synchronized `#*BPM` tags with `item.bpm` for host transport alignment.
* **Character Encoding Artifacts**:
  * Standardized all text formatting through `juce::CharPointer_UTF8` to eliminate Windows-1252/Latin-1 encoding mismatches (e.g., `â□□` replacing em-dashes `—` and bullet separators `•`).
