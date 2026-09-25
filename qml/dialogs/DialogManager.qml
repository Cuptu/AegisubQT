// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogManager: Decoupled on-demand coordinator for dialog lifecycle management.
// Dialogs are loaded lazily via Loaders upon invocation, preventing eager instantiation
// of 25+ top-level windows at application startup while maintaining full API compatibility.
import QtQuick
import QtQuick.Controls
import "../project/AssUtils.js" as AssUtils

Item {
    id: manager
    anchors.fill: parent

    // Context bindings to core controllers
    required property var project
    property var videoCtrl: null
    property var audioCtrl: null

    // Emitted to broadcast informational status notifications to the main window
    signal statusMessage(string msg)
    signal saveConfirmed()
    signal discardConfirmed()
    signal cancelled()

    // Reusable lazy dialog loader encapsulating on-demand component instantiation
    component LazyDialog: Loader {
        active: false
        asynchronous: false

        function open() {
            if (!active) active = true;
            if (item && typeof item.open === "function") item.open();
        }

        function close() {
            if (item && typeof item.close === "function") item.close();
        }

        function prompt(targetFile, action, data) {
            if (!active) active = true;
            if (item && typeof item.prompt === "function") item.prompt(targetFile, action, data);
        }

        function triggerSave() {
            if (item && typeof item.triggerSave === "function") item.triggerSave();
        }

        function triggerDiscard() {
            if (item && typeof item.triggerDiscard === "function") item.triggerDiscard();
        }

        function triggerCancel() {
            if (item && typeof item.triggerCancel === "function") item.triggerCancel();
        }
    }

    // Dialog instance references for external menu and toolbar triggers
    property alias dlgShiftTimes: _dlgShiftTimes
    property alias dlgSelectLines: _dlgSelectLines
    property alias dlgVideoDetails: _dlgVideoDetails
    property alias dlgDummyVideo: _dlgDummyVideo
    property alias dlgAbout: _dlgAbout
    property alias dlgStyleManager: _dlgStyleManager
    property alias dlgStyleEditor: _dlgStyleEditor
    property alias dlgStylingAssistant: _dlgStylingAssistant
    property alias dlgColorPicker: _dlgColorPicker
    property alias dlgTimingProcessor: _dlgTimingProcessor
    property alias dlgResample: _dlgResample
    property alias dlgProperties: _dlgProperties
    property alias dlgTranslation: _dlgTranslation
    property alias dlgPasteOver: _dlgPasteOver
    property alias dlgJumpTo: _dlgJumpTo
    property alias dlgFontsCollector: _dlgFontsCollector
    property alias dlgAttachments: _dlgAttachments
    property alias dlgPreferences: _dlgPreferences
    property alias dlgLanguage: _dlgLanguage
    property alias dlgSearchReplace: _dlgSearchReplace
    property alias dlgSpellChecker: _dlgSpellChecker
    property alias dlgKanjiTimer: _dlgKanjiTimer
    property alias dlgAutomation: _dlgAutomation
    property alias dlgExport: _dlgExport
    property alias dlgAutosave: _dlgAutosave
    property alias dlgSaveConfirm: _dlgSaveConfirm
    property alias dlgLog: _dlgLog

    // Shift Times dialog: adjusts subtitle start/end timestamps
    LazyDialog {
        id: _dlgShiftTimes
        sourceComponent: Component {
            DialogShiftTimes {
                onShiftTimesRequested: (amountMs, isForward, affectMode, timeType) => {
                    var signedMs = isForward ? amountMs : -amountMs;
                    var shiftStart = (timeType === 0 || timeType === 1);
                    var shiftEnd = (timeType === 0 || timeType === 2);
                    manager.project.shiftTimes(signedMs, shiftStart, shiftEnd, affectMode);
                }
            }
        }
    }

    // Select Lines dialog: filters and selects dialogue rows by pattern or criteria
    LazyDialog {
        id: _dlgSelectLines
        sourceComponent: Component {
            DialogSelectLines {
                onSelectLinesRequested: (action, fieldIndex, mode, invert, matchCase, comments, dialogues, query) => {
                    manager.project.selectLines(action, fieldIndex, mode, invert, matchCase, comments, dialogues, query);
                }
            }
        }
    }

    // Video Details dialog: displays stream parameters, aspect ratio, and color space
    LazyDialog {
        id: _dlgVideoDetails
        sourceComponent: Component {
            DialogVideoDetails {}
        }
    }

    // Dummy Video dialog: generates synthetic video feeds for timing without video assets
    LazyDialog {
        id: _dlgDummyVideo
        sourceComponent: Component {
            DialogDummyVideo {
                onDummyVideoCreated: (w, h, col, fpsVal, frameCount, checker) => {
                    if (manager.videoCtrl) manager.videoCtrl.openDummyVideo(fpsVal, frameCount, w, h, col);
                    manager.statusMessage(qsTr("Created dummy video (") + w + "×" + h + "@" + fpsVal + "fps)");
                }
            }
        }
    }

    // About dialog: application credits and version information
    LazyDialog {
        id: _dlgAbout
        sourceComponent: Component {
            DialogAbout {}
        }
    }

    // Style Manager dialog: script and storage style catalogs
    LazyDialog {
        id: _dlgStyleManager
        sourceComponent: Component {
            DialogStyleManager {
                onEditStyleRequested: (styleItem, isStorage, itemIndex) => {
                    _dlgStyleEditor.loadStyle(styleItem, isStorage, itemIndex);
                    _dlgStyleEditor.open();
                }
                onStatusMessage: (msg) => manager.statusMessage(msg)
            }
        }
    }

    // Style Editor dialog: style attribute customization and preview
    LazyDialog {
        id: _dlgStyleEditor
        property var pendingStyleItem: null
        property bool isStorage: false
        property int styleIndex: 0
        property color primaryColor: "#ffffff"
        property color secondaryColor: "#00ffff"
        property color outlineColor: "#000000"
        property color shadowColor: "#000000"

        onPrimaryColorChanged: if (item) item.primaryColor = primaryColor
        onSecondaryColorChanged: if (item) item.secondaryColor = secondaryColor
        onOutlineColorChanged: if (item) item.outlineColor = outlineColor
        onShadowColorChanged: if (item) item.shadowColor = shadowColor

        sourceComponent: Component {
            DialogStyleEditor {
                primaryColor: _dlgStyleEditor.primaryColor
                secondaryColor: _dlgStyleEditor.secondaryColor
                outlineColor: _dlgStyleEditor.outlineColor
                shadowColor: _dlgStyleEditor.shadowColor
                onStyleSaved: (styleData) => {
                    if (_dlgStyleEditor.isStorage) {
                        if (typeof styleStorageManager !== "undefined" && styleStorageManager) {
                            styleStorageManager.updateStyle(styleStorageManager.currentCatalog, _dlgStyleEditor.styleIndex, styleData);
                        }
                    } else {
                        if (typeof nativeSubtitleModel !== "undefined" && nativeSubtitleModel) {
                            nativeSubtitleModel.setStyle(_dlgStyleEditor.styleIndex, styleData);
                        }
                    }
                    manager.statusMessage(qsTr("Style [") + styleData.name + qsTr("] updated"));
                }
                onOpenColorPickerRequested: (propName, col) => {
                    _dlgColorPicker.targetProp = "style_" + propName;
                    _dlgColorPicker.currentColor = col;
                    _dlgColorPicker.open();
                }
            }
        }

        function loadStyle(styleItem, isStorage, idx) {
            _dlgStyleEditor.isStorage = isStorage;
            _dlgStyleEditor.styleIndex = (idx !== undefined) ? idx : 0;
            _dlgStyleEditor.pendingStyleItem = styleItem;
            if (!active) active = true;
            if (item) item.loadStyle(styleItem, isStorage);
        }

        onItemChanged: {
            if (item && pendingStyleItem) {
                item.loadStyle(pendingStyleItem, isStorage);
            }
        }
    }

    // Styling Assistant: sequential line-by-line style applicator
    LazyDialog {
        id: _dlgStylingAssistant
        sourceComponent: Component {
            DialogStylingAssistant {
                currentLineNumber: manager.project.currentSelectedIndex + 1
                currentLineTime: (manager.project.subtitleModel.count > manager.project.currentSelectedIndex && manager.project.currentSelectedIndex >= 0) ?
                    (manager.project.subtitleModel.get(manager.project.currentSelectedIndex).start + " - " + manager.project.subtitleModel.get(manager.project.currentSelectedIndex).end) : ""
                currentStyle: (manager.project.subtitleModel.count > manager.project.currentSelectedIndex && manager.project.currentSelectedIndex >= 0) ?
                    (manager.project.subtitleModel.get(manager.project.currentSelectedIndex).style || "Default") : "Default"
                currentText: (manager.project.subtitleModel.count > manager.project.currentSelectedIndex && manager.project.currentSelectedIndex >= 0) ?
                    manager.project.subtitleModel.get(manager.project.currentSelectedIndex).text : ""

                onStyleApplied: (styleName) => {
                    if (manager.project.currentSelectedIndex >= 0 && manager.project.currentSelectedIndex < manager.project.subtitleModel.count) {
                        manager.project.subtitleModel.setProperty(manager.project.currentSelectedIndex, "style", styleName);
                        manager.statusMessage(qsTr("Applied style to line #%1: %2").arg(manager.project.currentSelectedIndex + 1).arg(styleName));
                    }
                }
                onPrevRequested: manager.project.selectRow(Math.max(0, manager.project.currentSelectedIndex - 1), false, false)
                onNextRequested: manager.project.selectRow(Math.min(manager.project.subtitleModel.count - 1, manager.project.currentSelectedIndex + 1), false, false)
                onPlayAudioRequested: {
                    if (manager.audioCtrl && manager.project.subtitleModel.count > manager.project.currentSelectedIndex && manager.project.currentSelectedIndex >= 0) {
                        var it = manager.project.subtitleModel.get(manager.project.currentSelectedIndex);
                        manager.audioCtrl.playRange(AssUtils.assToMs(it.start), AssUtils.assToMs(it.end));
                    }
                }
                onPlayVideoRequested: {
                    if (manager.videoCtrl && manager.project.subtitleModel.count > manager.project.currentSelectedIndex && manager.project.currentSelectedIndex >= 0) {
                        var it = manager.project.subtitleModel.get(manager.project.currentSelectedIndex);
                        manager.videoCtrl.seekTime(AssUtils.assToMs(it.start) / 1000.0);
                        manager.videoCtrl.play();
                    }
                }
            }
        }
    }

    // Color Picker dialog: ASS hexadecimal color tool with eyedropper support
    LazyDialog {
        id: _dlgColorPicker
        property string targetProp: ""
        property color currentColor: "#ff5500"

        signal colorSelected(color col)
        signal colorAccepted(color col, string assBgrCode, string assAbgrCode)
        signal dropperActivated()

        sourceComponent: Component {
            DialogColorPicker {
                targetProp: _dlgColorPicker.targetProp
                currentColor: _dlgColorPicker.currentColor
                onDropperActivated: {
                    _dlgColorPicker.dropperActivated();
                    manager.statusMessage(qsTr("Colour picker activated (click anywhere on the video to pick a colour)"));
                }
                onColorSelected: (col) => {
                    _dlgColorPicker.colorSelected(col);
                    if (targetProp === "style_primary") _dlgStyleEditor.primaryColor = col;
                    else if (targetProp === "style_secondary") _dlgStyleEditor.secondaryColor = col;
                    else if (targetProp === "style_outline") _dlgStyleEditor.outlineColor = col;
                    else if (targetProp === "style_shadow") _dlgStyleEditor.shadowColor = col;
                }
                onColorAccepted: (col, assBgrCode, assAbgrCode) => {
                    _dlgColorPicker.colorAccepted(col, assBgrCode, assAbgrCode);
                }
            }
        }

        function open() {
            if (!active) active = true;
            if (item) {
                item.targetProp = targetProp;
                item.currentColor = currentColor;
                item.open();
            }
        }
    }

    // Timing Processor dialog: lead-in/lead-out adjustment and adjacent gap snapping
    LazyDialog {
        id: _dlgTimingProcessor
        sourceComponent: Component {
            DialogTimingProcessor {
                onTimingProcessRequested: (leadIn, leadOut, gapThresh, bias, selectedOnly, allowedStyles) => {
                    manager.project.processTiming(leadIn, leadOut, gapThresh, bias, selectedOnly, allowedStyles);
                }
            }
        }
    }

    // Resample dialog: script coordinate and typography scaling
    LazyDialog {
        id: _dlgResample
        sourceComponent: Component {
            DialogResample {
                project: manager.project
                videoCtrl: manager.videoCtrl
                onResampleRequested: (srcW, srcH, dstW, dstH, resampleMargins) => {
                    if (manager.project) {
                        manager.project.setScriptInfo("PlayResX", dstW);
                        manager.project.setScriptInfo("PlayResY", dstH);
                    }
                    var rx = dstW / srcW;
                    var ry = dstH / srcH;
                    manager.statusMessage(qsTr("Resolution resampled: ") + srcW + "x" + srcH + " ➔ " + dstW + "x" + dstH + " (X: " + rx.toFixed(2) + ", Y: " + ry.toFixed(2) + ")");
                }
            }
        }
    }

    // Properties dialog: script metadata and PlayRes coordinate dimensions
    LazyDialog {
        id: _dlgProperties
        sourceComponent: Component {
            DialogProperties {
                project: manager.project
                videoCtrl: manager.videoCtrl
                onPropertiesUpdated: (title, orig, trans, resX, resY, wrapStyle, matrix) => {
                    manager.statusMessage(qsTr("Script properties updated (PlayRes: ") + resX + "×" + resY + ", YCbCr: " + matrix + ")");
                }
            }
        }
    }

    // Translation Assistant dialog: sequential line translation workbench
    LazyDialog {
        id: _dlgTranslation
        sourceComponent: Component {
            DialogTranslation {
                currentLineNumber: manager.project.currentSelectedIndex + 1
                totalLines: manager.project.subtitleModel.count
                currentLineTime: (manager.project.subtitleModel.count > manager.project.currentSelectedIndex && manager.project.currentSelectedIndex >= 0) ?
                    (manager.project.subtitleModel.get(manager.project.currentSelectedIndex).start + " - " + manager.project.subtitleModel.get(manager.project.currentSelectedIndex).end) : ""
                originalText: (manager.project.subtitleModel.count > manager.project.currentSelectedIndex && manager.project.currentSelectedIndex >= 0) ?
                    manager.project.subtitleModel.get(manager.project.currentSelectedIndex).text : ""

                onAuditionRequested: {
                    if (manager.audioCtrl && manager.project.subtitleModel.count > manager.project.currentSelectedIndex && manager.project.currentSelectedIndex >= 0) {
                        var it = manager.project.subtitleModel.get(manager.project.currentSelectedIndex);
                        manager.audioCtrl.playRange(AssUtils.assToMs(it.start), AssUtils.assToMs(it.end));
                    }
                }
                onPlayAudioRequested: {
                    if (manager.audioCtrl && manager.project.subtitleModel.count > manager.project.currentSelectedIndex && manager.project.currentSelectedIndex >= 0) {
                        var it = manager.project.subtitleModel.get(manager.project.currentSelectedIndex);
                        manager.audioCtrl.playRange(AssUtils.assToMs(it.start), AssUtils.assToMs(it.end));
                    }
                }
                onPlayVideoRequested: {
                    if (manager.videoCtrl && manager.project.subtitleModel.count > manager.project.currentSelectedIndex && manager.project.currentSelectedIndex >= 0) {
                        var it = manager.project.subtitleModel.get(manager.project.currentSelectedIndex);
                        manager.videoCtrl.seekTime(AssUtils.assToMs(it.start) / 1000.0);
                        manager.videoCtrl.play();
                    }
                }
                onPrevRequested: manager.project.selectRow(Math.max(0, manager.project.currentSelectedIndex - 1), false, false)
                onNextRequested: manager.project.selectRow(Math.min(manager.project.subtitleModel.count - 1, manager.project.currentSelectedIndex + 1), false, false)
                onCommitRequested: (text, autoNext) => {
                    if (manager.project.subtitleModel.count === 0 || manager.project.currentSelectedIndex < 0) return;
                    if (text.length > 0) {
                        manager.project.subtitleModel.setProperty(manager.project.currentSelectedIndex, "text", text);
                        manager.statusMessage(qsTr("Translation committed for line #%1").arg(manager.project.currentSelectedIndex + 1));
                    }
                    if (autoNext && manager.project.currentSelectedIndex < manager.project.subtitleModel.count - 1) {
                        manager.project.selectRow(manager.project.currentSelectedIndex + 1, false, false);
                    }
                }
            }
        }
    }

    // Paste Over dialog: field-selective clipboard pasting
    LazyDialog {
        id: _dlgPasteOver
        sourceComponent: Component {
            DialogPasteOver {
                onPasteOverRequested: (fields) => {
                    manager.project.pasteLines(true, null, fields);
                }
            }
        }
    }

    // Jump To dialog: seek video and subtitle cursor to frame or timestamp
    LazyDialog {
        id: _dlgJumpTo
        sourceComponent: Component {
            DialogJumpTo {
                fps: (manager.videoCtrl && manager.videoCtrl.fps > 0) ? manager.videoCtrl.fps : 23.976
                onJumpRequested: (frame, timeStr, timeSec) => {
                    if (manager.videoCtrl) manager.videoCtrl.seekTime(timeSec);
                    manager.statusMessage(qsTr("Jumped to frame #") + frame + " (" + timeStr + ")");
                }
            }
        }
    }

    // Fonts Collector dialog: collects and archives script font files
    LazyDialog {
        id: _dlgFontsCollector
        sourceComponent: Component {
            DialogFontsCollector {
                onStatusMessage: (msg) => manager.statusMessage(msg)
                onBrowseDirectoryRequested: manager.statusMessage(qsTr("Choose a folder to export fonts to..."))
            }
        }
    }

    // Attachments dialog: embedded fonts and graphics inside script
    LazyDialog {
        id: _dlgAttachments
        sourceComponent: Component {
            DialogAttachments {
                project: manager.project
                onStatusMessage: (msg) => manager.statusMessage(msg)
            }
        }
    }

    // Preferences dialog: application configuration hierarchy
    LazyDialog {
        id: _dlgPreferences
        property int currentPageIndex: 0

        sourceComponent: Component {
            DialogPreferences {
                currentPageIndex: _dlgPreferences.currentPageIndex
                onPreferencesSaved: manager.statusMessage(qsTr("Preferences saved"))
            }
        }

        function open() {
            if (!active) active = true;
            if (item) {
                item.currentPageIndex = currentPageIndex;
                item.open();
            }
        }
    }

    // Language selection dialog: native single-choice language picker
    LazyDialog {
        id: _dlgLanguage
        sourceComponent: Component {
            DialogLanguage {
                onLanguageSelected: (langCode) => {
                    if (typeof languageManager !== "undefined" && languageManager) {
                        manager.statusMessage(qsTr("Language switched: ") + languageManager.getDisplayName(langCode));
                    }
                }
            }
        }
    }

    // Search and Replace dialog: pattern matching across script fields
    LazyDialog {
        id: _dlgSearchReplace
        property bool hasReplace: true
        property bool isReplaceMode: true
        onIsReplaceModeChanged: hasReplace = isReplaceMode
        onHasReplaceChanged: isReplaceMode = hasReplace

        sourceComponent: Component {
            DialogSearchReplace {
                hasReplace: _dlgSearchReplace.hasReplace
                onFindRequested: (query, options) => {
                    manager.project.findNext(query, options);
                }
                onReplaceRequested: (query, replaceWith, options) => {
                    manager.project.findAndReplace(query, replaceWith, options, false);
                }
                onReplaceAllRequested: (query, replaceWith, options) => {
                    manager.project.findAndReplace(query, replaceWith, options, true);
                }
            }
        }

        function open() {
            if (!active) active = true;
            if (item) {
                item.hasReplace = hasReplace;
                item.open();
            }
        }
    }

    // Spell Checker dialog: dictionary lookup and inline suggestions
    LazyDialog {
        id: _dlgSpellChecker
        sourceComponent: Component {
            DialogSpellChecker {
                onReplaceRequested: (origWord, newWord, replaceAll) => {
                    manager.project.findAndReplace(origWord, newWord, { matchCase: true, field: "text" }, replaceAll);
                }
                onIgnoreRequested: (origWord, ignoreAll) => {
                    manager.statusMessage(qsTr("Ignored word: ") + origWord);
                }
                onAddWordRequested: (word) => {
                    manager.statusMessage(qsTr("Added word [") + word + qsTr("] to the user dictionary"));
                }
            }
        }
    }

    // Kanji Timer dialog: karaoke timing synchronization between styles
    LazyDialog {
        id: _dlgKanjiTimer
        sourceComponent: Component {
            DialogKanjiTimer {
                onCopyTimingRequested: (srcStyle, dstStyle) => {
                    manager.project.applyKanjiCopy(srcStyle, dstStyle);
                }
                onStatusMessage: (msg) => manager.statusMessage(msg)
            }
        }
    }

    // Automation dialog: Lua macro and export filter manager
    LazyDialog {
        id: _dlgAutomation
        sourceComponent: Component {
            DialogAutomation {
                onStatusMessage: (msg) => manager.statusMessage(msg)
            }
        }
    }

    // Export dialog: subtitle transformations and encoding selection
    LazyDialog {
        id: _dlgExport
        sourceComponent: Component {
            DialogExport {
                onExportRequested: (filters, charset) => {
                    manager.project.exportFiltered(filters, charset);
                }
                onStatusMessage: (msg) => manager.statusMessage(msg)
            }
        }
    }

    // Autosave dialog: crash backup browser and project restorer
    LazyDialog {
        id: _dlgAutosave
        sourceComponent: Component {
            DialogAutosave {
                onRestoreAutosaveRequested: (path) => {
                    manager.statusMessage(qsTr("Restored project snapshot: ") + path);
                }
                onStatusMessage: (msg) => manager.statusMessage(msg)
            }
        }
    }

    // Save Confirmation dialog: prompts user to save unsaved modifications before exit, new, or open
    LazyDialog {
        id: _dlgSaveConfirm
        sourceComponent: Component {
            DialogSaveConfirmation {
                onSaveConfirmed: manager.saveConfirmed()
                onDiscardConfirmed: manager.discardConfirmed()
                onCancelled: manager.cancelled()
            }
        }
    }

    // Log window: runtime diagnostics mirrored from the Qt message handler (upstream Help > Log Window)
    LazyDialog {
        id: _dlgLog
        sourceComponent: Component {
            DialogLog {}
        }
    }
}
