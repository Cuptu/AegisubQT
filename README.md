<div align="center">

<img src="./assets/branding/banner.png" width="320" alt="AegisubQT Logo" />

# AegisubQT

**Next-Generation ASS/SSA Subtitle Editor Built with Qt 6 Quick & Modern C++20**

[![Release](https://img.shields.io/badge/Release-v4.0.2-0078D4?style=flat-square&logo=github&labelColor=1F2937)](https://github.com/Cuptu/AegisubQT/releases)
[![Status](https://img.shields.io/badge/Status-Release--Ready-10B981?style=flat-square&labelColor=1F2937)](https://github.com/Cuptu/AegisubQT)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-0078D4?style=flat-square&logo=windows&logoColor=white&labelColor=1F2937)](https://github.com/Cuptu/AegisubQT)
[![Language](https://img.shields.io/badge/Language-C%2B%2B%2020-00599C?style=flat-square&logo=c%2B%2B&logoColor=white&labelColor=1F2937)](https://en.cppreference.com/w/cpp/20)
[![Qt](https://img.shields.io/badge/Qt-6.11%2B%20Quick-41CD52?style=flat-square&logo=qt&logoColor=white&labelColor=1F2937)](https://www.qt.io/)
[![Lua](https://img.shields.io/badge/Automation-LuaJIT%202.1-000080?style=flat-square&logo=lua&logoColor=white&labelColor=1F2937)](https://luajit.org/)
[![License](https://img.shields.io/badge/License-BSD--3--Clause%20%2F%20MIT-F59E0B?style=flat-square&labelColor=1F2937)](./LICENSE)

[English](README.md) · [简体中文](README_zh.md) · [Architecture](#1-key-features--architectural-highlights) · [Upstream Mapping](#2-upstream-source-code-mapping) · [Build Guide](#4-build--compilation-guide)

</div>

> [!NOTE]
> **Modern Rewrite (v4.0.0+)**: AegisubQT is an independent open-source modernization of the industry-standard **Aegisub** subtitle editor, built on **Qt 6 Quick (QML)** and **C++20**. Version 4.0.0 carries forward the legacy of upstream Aegisub 3.2.x / 3.4.x into a new generation of high-performance desktop architectures.

**AegisubQT** is a free, cross-platform open-source tool for creating and modifying subtitles. Aegisub makes it quick and easy to time subtitles to audio, and features many powerful tools for styling them, including a built-in real-time video preview and GPU-accelerated audio spectrum.

---

## 1. Key Features & Architectural Highlights

- **Hardware-Accelerated Audio Spectrogram**: Custom QQuickItem (`SpectrogramItem`) backed by hardware GPU shaders (`spectrogram.vert.qsb`, `spectrogram.frag.qsb`) via the Qt Rendering Hardware Interface (QRHI). Replaces legacy synchronous CPU blitting (`audio_display.cpp`) with asynchronous zero-copy FFT caching.
- **Hardware Video Viewport & Playback Pipeline**: Integrated QtMultimedia FFmpeg 7.1 hardware decoding backend embedded in `VideoBox.qml`, seamlessly synchronized with the sub-millisecond audio playback engine and visual typography canvas.
- **AstraCore High-Performance Media Bridge**: Native C ABI bridge (`AstraCoreBridge`) interfacing with `AstraCore.Native.dll` for near-instant video demuxing, metadata probing, and zero-recode PCM audio extraction ("Open Audio from Video").
- **Keyframes & Timecodes Subsystem**: Native import/export for Keyframe format v1 (`.txt`) and VFR timecodes format v1/v2 (`.tc`), featuring real-time waveform/spectrogram keyframe boundary snapping and visual red marker rendering.
- **Vector-Accurate Visual Typography**: Precise coordinate transformations between script space (`PlayResX`, `PlayResY`) and visual display coordinates. Full vector-accurate visual typography tools (Cross, Drag, RotateZ, Scale, Rectangular Clip) matching ASS format specifications.
- **High-Performance Subtitle & Style Storage Models**: Reactive C++ subtitle model (`SubtitleModel`) and persistent dual-catalog style storage manager (`StyleStorageManager`) supporting non-linear history tracking, sub-millisecond time shifting, regex search & replace, and timing post-processing.
- **Embedded libaegisub C++20 Core**: Fully integrated, dependency-pruned `libaegisub` static engine providing tokenizer, karaoke syllable splitting, ASS time arithmetic, inline string encoding, and color formatters.
- **Automation 4 Lua Subsystem**: Native LuaJIT + LPeg + Win32 GDI / DirectWrite text metrics integration (`calculate_text_extents`), enabling standard Aegisub macros (such as `kara-templater.lua` and `clean-info.lua`) to execute without modifications.
- **Zero-Restart Dynamic Localization (i18n)**: Integrated `LanguageManager` supporting 9 locales (English, Simplified Chinese, Traditional Chinese, Japanese, Korean, German, French, Spanish, Russian) with dynamic on-the-fly switching.
- **Universal Drag-and-Drop Pipeline**: Direct file drop handling for video files (`.mp4`, `.mkv`, etc.), audio files (`.wav`, etc.), subtitle files (`.ass`, `.srt`), keyframe files (`.txt`), and timecode tables (`.tc`).
- **Native Desktop Human-Interface Framework**: Faithful recreation of Win32 desktop dialog ergonomics (`wxTreeCtrl`, `wxStaticBox`, `wxRadioBox`, `wxListView`) using high-performance QML primitives (`NativeDialogFrame`, `NativeTreeBook`, `NativeGroupBox`, `NativeSpinBox`).

---

## 2. Upstream Source Code Mapping

For maintainers and contributors familiar with upstream Aegisub (`Aegisub-master`), the following table maps legacy wxWidgets C++ source files to their corresponding Qt 6 Quick / C++20 implementations.

### 2.1 Core Application, Bridge & Lifecycle

| Upstream wxWidgets File (`src/`) | Qt Quick File (`aegisub_qt_quick/`) | Responsibility & Subsystem |
| :--- | :--- | :--- |
| `main.cpp` | `src/main.cpp` | Application entry point, CLI arguments (`--audio`, `--video`, `--ass`, `--lang`), engine initialization, font configuration. |
| `frame_main.cpp`, `frame_main.h` | `qml/Main.qml`, `qml/views/TopMenuBar.qml`, `qml/views/TopToolBar.qml` | Top-level window layout, menu bar dispatch, global hotkeys, toolbar commands. |
| `project.cpp`, `project.h` | `qml/project/SubtitleProject.qml`, `qml/project/AssUtils.js` | Project lifecycle, document dirty tracking, undo/redo stacks, ASS parser and serializer. |
| *(New AstraCore Integration)* | `src/bridge/AstraCoreBridge.h`, `src/bridge/AstraCoreBridge.cpp` | Native C ABI bridge to `AstraCore.Native.dll` for audio extraction and media probing. |
| *(New Language Manager)* | `src/bridge/LanguageManager.h`, `src/bridge/LanguageManager.cpp` | Multi-language translation catalog loader and dynamic locale switching. |

### 2.2 Video Viewport & Visual Typography

| Upstream wxWidgets File (`src/`) | Qt Quick File (`aegisub_qt_quick/`) | Responsibility & Subsystem |
| :--- | :--- | :--- |
| `video_controller.cpp`, `video_controller.h` | `src/video/VideoController.cpp`, `src/video/VideoController.h` | Video playback state machine, frame seeking, dummy video generation, keyframe/timecode import & export. |
| `video_display.cpp`, `video_display.h` | `src/video/VideoDisplayController.cpp`, `src/video/VideoDisplayController.h` | Viewport zoom, pan, middle-click navigation, script-to-display coordinate mapping. |
| `video_box.cpp`, `video_box.h` | `qml/views/VideoBox.qml` | Video surface QML container with QtMultimedia FFmpeg backend, timeline seek bar, playback controls, visual overlay canvas. |
| `visual_tool.cpp`, `visual_tool.h` | `src/video/VisualToolBase.cpp`, `src/video/VisualToolBase.h` | Base class for on-screen typography manipulation tools, override tag serialization (`\pos`, `\move`, `\clip`). |
| `visual_tool_cross.cpp`, `visual_tool_cross.h` | `src/video/VisualTools.cpp` (`VisualToolCross`) | Coordinate cursor and screen crosshair tool (`\pos`). |
| `visual_tool_drag.cpp`, `visual_tool_drag.h` | `src/video/VisualTools.cpp` (`VisualToolDrag`) | Subtitle position anchor, destination point, and origin manipulation (`\pos`, `\move`, `\org`). |
| `visual_tool_rotatez.cpp`, `visual_tool_rotatez.h` | `src/video/VisualTools.cpp` (`VisualToolRotateZ`) | On-screen angle compass protractor for Z-axis rotation (`\frz`). |
| `visual_tool_scale.cpp`, `visual_tool_scale.h` | `src/video/VisualTools.cpp` (`VisualToolScale`) | Interactive horizontal and vertical glyph scaling (`\fscx`, `\fscy`). |
| `visual_tool_clip.cpp`, `visual_tool_clip.h` | `src/video/VisualTools.cpp` (`VisualToolClip`) | Rectangular clipping box tool with corner resize handles (`\clip`). |

### 2.3 Audio Subsystem & Spectrogram

| Upstream wxWidgets File (`src/`) | Qt Quick File (`aegisub_qt_quick/`) | Responsibility & Subsystem |
| :--- | :--- | :--- |
| `audio_controller.cpp`, `audio_controller.h` | `src/audio/AudioController.cpp`, `src/audio/AudioController.h` | Audio track loading, background thread decoding, selection bounds, playback cursor dispatch, keyframe sync. |
| `audio_display.cpp`, `audio_display.h` | `src/audio/AudioDisplayController.cpp`, `src/audio/AudioDisplayController.h` | Audio timeline interaction, click-drag selection snapping, keyframe snapping. |
| `audio_renderer_spectrum.cpp` | `src/audio/AegisubStftCore.cpp`, `src/audio/AegisubStftCore.h` | Short-Time Fourier Transform (STFT), sliding window FFT calculation, frequency curve transforms. |
| `audio_display.cpp` (rendering logic) | `src/audio/SpectrogramItem.cpp`, `src/audio/SpectrogramShaderMaterial.cpp` | QRHI / OpenGL / Direct3D hardware-accelerated spectrogram vertex and fragment rendering. |
| `audio_colorscheme.cpp`, `audio_colorscheme.h` | `src/audio/AudioColorScheme.cpp`, `src/audio/AudioColorScheme.h` | Color palette generation for waveforms and spectrogram palettes. |
| `audio_provider_*.cpp` | `src/audio/AudioPcmProvider.cpp`, `src/audio/AudioPcmProvider.h` | Multi-format audio PCM stream decoding and resampling. |
| `audio_box.cpp`, `audio_box.h` | `qml/views/AudioBox.qml` | Audio waveform/spectrum view layout, playback buttons, zoom and volume sliders, keyframe marker lines. |

### 2.4 Subtitle Editor & Models

| Upstream wxWidgets File (`src/`) | Qt Quick File (`aegisub_qt_quick/`) | Responsibility & Subsystem |
| :--- | :--- | :--- |
| `subs_grid.cpp`, `subs_grid.h` | `qml/views/SubtitleGridArea.qml`, `src/model/SubtitleModel.cpp` | High-performance virtualized subtitle spreadsheet grid, C++ backing model, multi-row selection, context menus. |
| `subs_edit_box.cpp`, `subs_edit_box.h` | `qml/views/SubtitleEditBox.qml` | Subtitle text editor, syntax tag shortcuts, duration calculation, actor/effect dropdowns. |
| `subs_edit_ctrl.cpp`, `subs_edit_ctrl.h` | `qml/views/SubtitleEditBox.qml` | Real-time tag validation and commit dispatch. |
| *(New Style Storage Manager)* | `src/model/StyleStorageManager.h`, `src/model/StyleStorageManager.cpp` | Persistent dual-catalog style storage management (Storage vs Current Script). |

### 2.5 Dialogs & Utility Windows

| Upstream wxWidgets File (`src/`) | Qt Quick File (`aegisub_qt_quick/`) | Responsibility & Subsystem |
| :--- | :--- | :--- |
| `dialog_manager.cpp` | `qml/dialogs/DialogManager.qml` | Central dialog instance coordinator, signal aggregation, and view dispatch. |
| `dialog_preferences.cpp`, `preferences.cpp` | `qml/dialogs/DialogPreferences.qml` | 12-page tree-based preference configuration (`wxTreebook`), global options. |
| `dialog_shift_times.cpp` | `qml/dialogs/DialogShiftTimes.qml` | Time/frame shifting, direction toggles, history log list, and selection scoping. |
| `dialog_selection.cpp` | `qml/dialogs/DialogSelectLines.qml` | Advanced dialogue selection by field, regex, case sensitivity, dialogue/comment filters. |
| `dialog_timing_processor.cpp` | `qml/dialogs/DialogTimingProcessor.qml` | Lead-in/lead-out processing, adjacent dialogue snapping with bias, keyframe snapping. |
| `dialog_style_manager.cpp` | `qml/dialogs/DialogStyleManager.qml` | Dual-catalog style storage management (Storage vs Current Script), catalog presets. |
| `dialog_style_editor.cpp` | `qml/dialogs/DialogStyleEditor.qml` | Full ASS style editor: fonts, 4-channel colors, margins, numpad alignment, live preview. |
| `dialog_colorpicker.cpp` | `qml/dialogs/DialogColorPicker.qml` | 2D color spectrum, current/original swatches, screen dropper, ASS hex formatter. |
| `dialog_search_replace.cpp` | `qml/dialogs/DialogSearchReplace.qml` | Search and replace dialog, regex support, field limiting, tag skipping. |
| `dialog_spellchecker.cpp` | `qml/dialogs/DialogSpellChecker.qml` | Spell check assistant, dictionary lookup, replacement suggestions. |
| `dialog_styling_assistant.cpp` | `qml/dialogs/DialogStylingAssistant.qml` | Rapid keyboard-driven style assignment assistant (`Enter`, `Page Up`, `Page Down`). |
| `dialog_translation.cpp` | `qml/dialogs/DialogTranslation.qml` | Side-by-side subtitle translation assistant with audio/video preview triggers. |
| `dialog_video_details.cpp` | `qml/dialogs/DialogVideoDetails.qml` | Video metadata inspector (dimensions, FPS, frame count, color matrix, decoder). |
| `dialog_dummy_video.cpp` | `qml/dialogs/DialogDummyVideo.qml` | Custom synthetic dummy video generator (FPS, resolution, duration, test patterns). |
| `dialog_attachments.cpp` | `qml/dialogs/DialogAttachments.qml` | Embedded font and graphic attachment manager (`wxListView` report mode). |
| `dialog_properties.cpp` | `qml/dialogs/DialogProperties.qml` | Script properties: PlayResX/Y, Title, WrapStyle, ScaledBorderAndShadow, YCbCr Matrix. |
| `dialog_resample.cpp` | `qml/dialogs/DialogResample.qml` | Script resolution resampling, font size and margin scaling factors. |
| `dialog_jumpto.cpp` | `qml/dialogs/DialogJumpTo.qml` | Exact frame / timecode bidirectional jump dialog. |
| `dialog_fonts_collector.cpp` | `qml/dialogs/DialogFontsCollector.qml` | Project font usage analysis and directory collector. |
| `dialog_kanji_timer.cpp` | `qml/dialogs/DialogKanjiTimer.qml` | Romaji-to-Kanji karaoke timing copier. |
| `dialog_paste_over.cpp` | `qml/dialogs/DialogPasteOver.qml` | Selective field paste over (Times, Text, Style, Actor, Effect, Margins). |
| `dialog_autosave.cpp` | `qml/dialogs/DialogAutosave.qml` | Snapshot browser for automatic backups and autosaved scripts. |
| `dialog_automation.cpp` | `qml/dialogs/DialogAutomation.qml` | Automation 4 Lua script manager (autoload, unload, reload, script info). |
| `dialog_export.cpp` | `qml/dialogs/DialogExport.qml` | Subtitle export filter chain and character set encoder. |
| `dialog_about.cpp` | `qml/dialogs/DialogAbout.qml` | About dialog, version information, license, and author credits. |

### 2.6 Automation & Core Libraries

| Upstream File (`src/` / `libaegisub/`) | Qt Quick File (`aegisub_qt_quick/`) | Responsibility & Subsystem |
| :--- | :--- | :--- |
| `auto4_base.cpp`, `auto4_lua.cpp` | `src/automation/AutomationManager.cpp`, `src/automation/LuaScript.cpp` | Lua engine hosting, macro registry, filter registration, autoload directory scanning. |
| `auto4_lua_assfile.cpp` | `src/automation/LuaAssFileBridge.cpp` | Lua table <-> SubtitleProject bidirectional bridge for script mutation. |
| GDI/FreeType text extents | `src/automation/LuaTextExtents.cpp` | Native font metric measurement for `aegisub.text_extents`. |
| `libaegisub/ass/dialogue_parser.cpp` | `libaegisub/src/dialogue_parser.cpp` | Tokenizer for ASS override tags and plain text fragments. |
| `libaegisub/ass/karaoke.cpp` | `libaegisub/src/karaoke.cpp` | Syllable duration and karaoke tag parser (`\k`, `\K`, `\kf`, `\ko`). |
| `libaegisub/ass/time.cpp` | `libaegisub/src/time.cpp` | ASS time parsing (`h:mm:ss.cs`) and formatting arithmetic. |
| `libaegisub/common/color.cpp` | `libaegisub/src/color.cpp` | Color parser and ASS color code generator (`&HBBGGRR&`). |
| `libaegisub/ass/string_codec.cpp` | `libaegisub/src/string_codec.cpp` | Inline string encoder and decoder for ASS comments and project storage. |

---

## 3. Directory Layout

```text
AegisubQT/
├── assets/
│   ├── bin/                  # Native runtime dependencies (AstraCore.Native.dll)
│   ├── branding/             # Project banner and brand artwork
│   └── icons_native/         # High-resolution native PNG toolbar/dialog icons
├── libaegisub/               # Embedded, modern C++20 port of libaegisub algorithms
├── locale/                   # Compiled Qt .qm catalogs and source .ts files (8 translations + English)
├── luajit/                   # LuaJIT 2.1 runtime headers and library
├── qml/
│   ├── Main.qml              # Application root window and menu glue
│   ├── controls/             # Desktop-native UI controls (Win32 / native styling)
│   ├── dialogs/              # 25 dedicated desktop modal dialogs
│   ├── project/              # SubtitleProject data model and ASS utilities
│   └── views/                # Primary dockable panels (VideoBox, AudioBox, Grid, Edit)
├── shaders/                  # GLSL / QRHI spectrogram shaders (.vert, .frag)
├── src/
│   ├── audio/                # Audio playback, STFT engine, and spectrogram item
│   ├── automation/           # Lua automation runtime, bridge, and text metric calculation
│   ├── bridge/               # AstraCore, LanguageManager, and Core bridge interfaces
│   ├── model/                # SubtitleModel and StyleStorageManager C++ models
│   ├── video/                # Video controller, viewport, and visual typography tools
│   └── main.cpp              # Application entry point and engine bootstrap
├── CMakeLists.txt            # Modern CMake build script
├── README.md                 # Technical architecture guide & upstream mapping
└── README_zh.md              # Technical architecture guide (Simplified Chinese)
```

---

## 4. Build & Compilation Guide

### Prerequisites
- **Compiler**: Visual Studio 2022 (MSVC v143 x64) or modern Clang / GCC supporting C++20.
- **Framework**: Qt 6.8+ (with `Qt6::Core`, `Qt6::Gui`, `Qt6::Quick`, `Qt6::Qml`, `Qt6::QuickControls2`, `Qt6::ShaderTools`, `Qt6::Multimedia`).
- **Build System**: CMake 3.20+ and Ninja.

### Build Commands
```cmd
:: Initialize MSVC 64-bit developer environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

:: Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

:: Compile (produces build/AegisubQT.exe and deploys runtime DLLs & assets)
ninja -C build
```

### macOS Installation & Launch Notes
Because open-source release artifacts are ad-hoc signed without a paid Apple Developer ID notarization ticket, when opening the downloaded DMG and dragging the app into `/Applications`:
- **First Launch Prompt**: If macOS displays "cannot be opened because Apple cannot check it for malicious software" or "unidentified developer", navigate to **System Settings -> Privacy & Security**, scroll down to the Security section, and click **Open Anyway**.
- **Terminal Bypass**: Alternatively, clear the quarantine extended attribute directly via Terminal:
  ```bash
  xattr -cr /Applications/AegisubQT.app
  ```

---

## 5. License & Copyright

This project adopts a clean multi-license structure honoring original upstream authorship while providing permissive terms for newly authored components:

- **MIT License** (Copyright (c) 2026, Cuptu): Covers all newly authored, independent Qt Quick front-end components (`qml/`), desktop-native UI controls (`qml/controls/`), hardware QRHI spectrogram shaders (`shaders/`, `src/audio/SpectrogramItem.*`, `src/audio/SpectrogramShaderMaterial.*`), and modern runtime bridges (`src/bridge/`).
- **BSD 3-Clause License** (Copyright (c) 2005 - 2026, Rodrigo Braz Monteiro, Niels Martin Hansen, arch1t3cht, and Aegisub Contributors): Covers core application lifecycle, video viewport manipulation, audio subsystem, and automation pipelines in `src/`.
- **ISC License** (Copyright (c) 2010 - 2014, Thomas Goyne): Covers the modernized, embedded `libaegisub/` C++20 core algorithm library.

For the full legal texts of each license, see [LICENSE](LICENSE).
