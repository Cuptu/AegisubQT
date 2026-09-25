// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Aegisub main application window hosting layout panes, top menus, toolbar, and dialog bindings.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "views"
import "dialogs"
import "project"
import "controls"
import "project/AssUtils.js" as AssUtils

ApplicationWindow {
    id: root
    visible: true
    width: 1060
    height: 640
    minimumWidth: 800
    minimumHeight: 480
    title: (subProject.isModified ? "* " : "") + subProject.currentFileName + " - AegisubQT 4.0.0"

    onClosing: (close) => {
        if (isForceClosing) {
            close.accepted = true;
            return;
        }
        if (subProject.isModified) {
            close.accepted = false;
            root.confirmSaveAndProceed("exit");
        } else {
            close.accepted = true;
        }
    }

    // Standard Aegisub palette constants
    QtObject {
        id: pal
        readonly property color winActiveBorder: Qt.rgba(255/255, 91/255, 239/255, 1.0)
        readonly property color winSelectionBg: Qt.rgba(206/255, 255/255, 231/255, 1.0)
        readonly property color winInFrame: Qt.rgba(255/255, 253/255, 234/255, 1.0)
        readonly property color winLeftCol: Qt.rgba(196/255, 236/255, 201/255, 1.0)
        readonly property color winHeader: Qt.rgba(165/255, 207/255, 231/255, 1.0)
        readonly property color winHeaderBorder: "#759dbf"
        readonly property color winGridLines: Qt.rgba(190/255, 190/255, 190/255, 1.0)
        readonly property color winCommentBg: Qt.rgba(216/255, 222/255, 245/255, 1.0)
        readonly property color winStandardText: "#000000"

        readonly property color winBg: "#f0f0f0"
        readonly property color winBorder: "#bebebe"
        readonly property color winSunkenBg: "#ffffff"
        readonly property color winSunkenBorder: "#ababab"
        readonly property color winShadowDark: "#808080"
        readonly property color winHighlight: "#ffffff"
    }

    // Global UI fonts are provided by the C++ UiTheme context property "uiTheme"
    // (platform-native family via QGuiApplication / QFontDatabase).

    color: pal.winBg

    // Core subtitle project service
    SubtitleProject {
        id: subProject
        onStatusMessage: (msg) => root.statusMsgText = msg
        onDataModified: {
            // Upstream "Autosave after every change": debounce a snapshot shortly
            // after each document mutation when the preference is enabled.
            if (typeof aegisubCore !== "undefined" && aegisubCore
                    && aegisubCore.getSetting("Autosave/AfterChange", false)) {
                afterChangeSaveTimer.restart();
            }
        }
    }

    // Upstream View menu display modes: "subs" (grid only), "video" (video+grid),
    // "audio" (audio+grid), "full" (audio+video+grid).
    property string viewMode: "full"
    property bool toolbarVisible: true
    property bool videoDetached: false
    property bool overscanMask: false
    property bool hasVideoLoaded: (typeof videoController !== "undefined" && videoController) ? videoController.hasVideo : false
    property bool hasAudioLoaded: (typeof audioController !== "undefined" && audioController) ? audioController.hasAudio : false
    property bool showVideo: (viewMode === "video" || viewMode === "full") && hasVideoLoaded && !videoDetached
    property bool showAudio: (viewMode === "audio" || viewMode === "full") && hasAudioLoaded

    // Audio box height: defaults to 200px. Overridable via --audio-height.
    property int audioBoxHeight: (typeof audioHeightOverride !== "undefined" && audioHeightOverride > 0)
                                 ? audioHeightOverride : 200
    // Dynamic pane dimensions based on active media views and user zoom selection
    property int videoBoxWidth: 414
    property int topRowHeight: 318

    property string statusHelpText: ""
    property string statusMsgText: ""
    property int tagHidingMode: 2

    // Closing intercept and unsaved changes confirmation state
    property bool isForceClosing: false
    property string pendingAction: ""
    property var pendingActionData: null

    // Backward-compatibility model and data property aliases
    property alias project: subProject
    property alias subtitleModel: subProject.subtitleModel
    property alias currentSelectedIndex: subProject.currentSelectedIndex
    property alias selectedIndices: subProject.selectedIndices
    property alias initialSelectedText: subProject.initialSelectedText
    property alias snapPoints: subProject.snapPoints

    // Modal dialog manager
    DialogManager {
        id: dialogManager
        project: subProject
        videoCtrl: typeof videoController !== "undefined" ? videoController : null
        audioCtrl: typeof audioController !== "undefined" ? audioController : null
        onStatusMessage: (msg) => root.statusMsgText = msg
        onSaveConfirmed: root.handleSaveConfirmSave()
        onDiscardConfirmed: root.handleSaveConfirmDiscard()
        onCancelled: root.handleSaveConfirmCancel()
    }

    // Dialog aliases for external and test compatibility
    property alias dlgShiftTimes: dialogManager.dlgShiftTimes
    property alias dlgSelectLines: dialogManager.dlgSelectLines
    property alias dlgVideoDetails: dialogManager.dlgVideoDetails
    property alias dlgDummyVideo: dialogManager.dlgDummyVideo
    property alias dlgAbout: dialogManager.dlgAbout
    property alias dlgStyleManager: dialogManager.dlgStyleManager
    property alias dlgStyleEditor: dialogManager.dlgStyleEditor
    property alias dlgStylingAssistant: dialogManager.dlgStylingAssistant
    property alias dlgColorPicker: dialogManager.dlgColorPicker
    property alias dlgTimingProcessor: dialogManager.dlgTimingProcessor
    property alias dlgResample: dialogManager.dlgResample
    property alias dlgProperties: dialogManager.dlgProperties
    property alias dlgTranslation: dialogManager.dlgTranslation
    property alias dlgPasteOver: dialogManager.dlgPasteOver
    property alias dlgJumpTo: dialogManager.dlgJumpTo
    property alias dlgFontsCollector: dialogManager.dlgFontsCollector
    property alias dlgAttachments: dialogManager.dlgAttachments
    property alias dlgPreferences: dialogManager.dlgPreferences
    property alias dlgSearchReplace: dialogManager.dlgSearchReplace
    property alias dlgSpellChecker: dialogManager.dlgSpellChecker
    property alias dlgKanjiTimer: dialogManager.dlgKanjiTimer
    property alias dlgAutomation: dialogManager.dlgAutomation
    property alias dlgExport: dialogManager.dlgExport
    property alias dlgAutosave: dialogManager.dlgAutosave
    property alias dlgSaveConfirm: dialogManager.dlgSaveConfirm
    property alias dlgLog: dialogManager.dlgLog

    // UI component aliases
    property alias subtitleEditArea: subtitleEditBox.subtitleEditArea
    property alias gridView: subtitleGridArea.gridView
    property alias gridContextMenu: subtitleGridArea.gridContextMenu
    property alias headerContextMenu: subtitleGridArea.headerContextMenu
    property alias itemInsertBefore: subtitleGridArea.itemInsertBefore
    property alias itemSwap: subtitleGridArea.itemSwap
    property alias itemDuplicate: subtitleGridArea.itemDuplicate

    function saveSubtitlesRequested() {
        if (subProject.currentFileName && subProject.currentFileName !== qsTr("Untitled") && subProject.currentFileName !== "Untitled") {
            return subProject.saveSubtitles(subProject.currentFileName);
        } else {
            fileDialogSubSave.open();
            return false;
        }
    }

    FileDialog {
        id: fileDialogSubOpen
        title: qsTr("Open Subtitles")
        nameFilters: ["Advanced SubStation Alpha (*.ass *.ssa)", "SubRip (*.srt)", "All Files (*.*)"]
        onAccepted: {
            recentFiles.add("subtitles", selectedFile.toString());
            subProject.openSubtitles(selectedFile.toString());
        }
    }

    FileDialog {
        id: fileDialogSubSave
        title: qsTr("Save Subtitles as")
        fileMode: FileDialog.SaveFile
        nameFilters: ["Advanced SubStation Alpha (*.ass)", "All Files (*.*)"]
        onAccepted: {
            var ok = subProject.saveSubtitles(selectedFile.toString());
            recentFiles.add("subtitles", selectedFile.toString());
            if (ok && root.pendingAction !== "") {
                var act = root.pendingAction;
                var dat = root.pendingActionData;
                root.pendingAction = "";
                root.pendingActionData = null;
                root.executePendingAction(act, dat);
            }
        }
        onRejected: {
            root.pendingAction = "";
            root.pendingActionData = null;
        }
    }

    FileDialog {
        id: fileDialogVideoOpen
        title: qsTr("Open Video")
        nameFilters: ["Video Files (*.mkv *.mp4 *.avi *.webm *.ts)", "All Files (*.*)"]
        onAccepted: {
            recentFiles.add("video", selectedFile.toString());
            if (typeof videoController !== "undefined") {
                videoController.openVideo(selectedFile.toString());
            }
        }
    }

    FileDialog {
        id: fileDialogAudioOpen
        title: qsTr("Open Audio")
        nameFilters: ["Audio Files (*.wav *.mp3 *.aac *.flac *.m4a *.ogg)", "All Files (*.*)"]
        onAccepted: {
            recentFiles.add("audio", selectedFile.toString());
            if (typeof audioController !== "undefined") {
                audioController.openAudio(selectedFile.toString());
            }
        }
    }

    FileDialog {
        id: fileDialogKeyframesOpen
        title: qsTr("Open Keyframes")
        nameFilters: ["Keyframe Files (*.txt *.keyframes *.pass)", "All Files (*.*)"]
        onAccepted: {
            recentFiles.add("keyframes", selectedFile.toString());
            if (typeof videoController !== "undefined") {
                videoController.openKeyframesFile(selectedFile.toString());
            }
        }
    }

    FileDialog {
        id: fileDialogKeyframesSave
        title: qsTr("Save Keyframes")
        fileMode: FileDialog.SaveFile
        nameFilters: ["Keyframe Files (*.txt *.keyframes)", "All Files (*.*)"]
        onAccepted: {
            if (typeof videoController !== "undefined") {
                videoController.saveKeyframesFile(selectedFile.toString());
                root.statusMsgText = qsTr("Saved keyframes");
            }
        }
    }

    FileDialog {
        id: fileDialogTimecodesOpen
        title: qsTr("Open Timecodes")
        nameFilters: ["Timecode Files (*.txt *.tc)", "All Files (*.*)"]
        onAccepted: {
            recentFiles.add("timecodes", selectedFile.toString());
            if (typeof videoController !== "undefined") {
                videoController.openTimecodesFile(selectedFile.toString());
            }
        }
    }

    FileDialog {
        id: fileDialogTimecodesSave
        title: qsTr("Save Timecodes")
        fileMode: FileDialog.SaveFile
        nameFilters: ["Timecode Files (*.txt *.tc)", "All Files (*.*)"]
        onAccepted: {
            if (typeof videoController !== "undefined") {
                videoController.saveTimecodesFile(selectedFile.toString());
                root.statusMsgText = qsTr("Saved timecodes");
            }
        }
    }

    // Top menu bar
    menuBar: TopMenuBar {
        id: topMenuBar
        project: subProject
        dialogs: dialogManager
        videoCtrl: typeof videoController !== "undefined" ? videoController : null
        audioCtrl: typeof audioController !== "undefined" ? audioController : null
        videoDisplayCtrl: typeof videoDisplayController !== "undefined" ? videoDisplayController : null
        viewMode: root.viewMode
        toolbarVisible: root.toolbarVisible
        videoDetached: root.videoDetached
        overscanMask: root.overscanMask
        onStatusMessage: (msg) => root.statusMsgText = msg
        onNewSubtitlesRequested: root.confirmSaveAndProceed("new")
        onOpenSubtitlesRequested: root.confirmSaveAndProceed("open")
        onExitRequested: root.close()
        onJumpToLineStartRequested: root.jumpToLineStart()
        onJumpToLineEndRequested: root.jumpToLineEnd()
        onCycleTagHidingRequested: root.cycleTagHiding()
        onSaveSubtitlesRequested: root.saveSubtitlesRequested()
        onSaveSubtitlesAsRequested: fileDialogSubSave.open()
        onOpenVideoRequested: fileDialogVideoOpen.open()
        onOpenAudioRequested: fileDialogAudioOpen.open()
        onOpenKeyframesRequested: fileDialogKeyframesOpen.open()
        onSaveKeyframesRequested: fileDialogKeyframesSave.open()
        onOpenTimecodesRequested: fileDialogTimecodesOpen.open()
        onSaveTimecodesRequested: fileDialogTimecodesSave.open()
        onViewModeRequested: (mode) => { root.viewMode = mode }
        onToggleToolbarRequested: { root.toolbarVisible = !root.toolbarVisible }
        onDetachVideoChanged: (detached) => { root.videoDetached = detached }
        onOverscanMaskToggled: (mask) => { root.overscanMask = mask }
        onOpenRecentFileRequested: (type, path) => root.openRecentFile(type, path)
    }

    // Top action toolbar
    header: TopToolBar {
        id: topToolBar
        visible: root.toolbarVisible
        project: subProject
        dialogs: dialogManager
        audioCtrl: typeof audioController !== "undefined" ? audioController : null
        videoCtrl: typeof videoController !== "undefined" ? videoController : null
        videoDisplayCtrl: typeof videoDisplayController !== "undefined" ? videoDisplayController : null
        onStatusMessage: (msg) => root.statusMsgText = msg
        onStatusHelp: (msg) => root.statusHelpText = msg
        onNewSubtitlesRequested: root.confirmSaveAndProceed("new")
        onOpenSubtitlesRequested: root.confirmSaveAndProceed("open")
        onCycleTagHidingRequested: root.cycleTagHiding()
        onJumpToLineStartRequested: root.jumpToLineStart()
        onJumpToLineEndRequested: root.jumpToLineEnd()
        onSaveSubtitlesRequested: root.saveSubtitlesRequested()
    }

    // Upstream "Automatic Save / Automatic Backup": periodic snapshots of unsaved work
    // written to the user data directory (never overwrites the working file).
    Timer {
        id: autosaveTimer
        interval: Math.max(30, (typeof aegisubCore !== "undefined" && aegisubCore)
                               ? aegisubCore.getSetting("Autosave/IntervalSecs", 60) : 60) * 1000
        repeat: true
        running: subProject.isModified
        onTriggered: {
            var didSave = false;
            if (typeof aegisubCore !== "undefined" && aegisubCore && subProject.subtitleModel) {
                if (aegisubCore.getSetting("Autosave/Enabled", true)
                        && subProject.subtitleModel.saveBackup(true)) {
                    didSave = true;
                }
                if (aegisubCore.getSetting("Backup/Enabled", true)
                        && subProject.subtitleModel.saveBackup(false)) {
                    didSave = true;
                }
            }
            if (didSave) {
                root.statusMsgText = qsTr("Auto-save: safe copy written");
            }
        }
    }

    // Upstream "Autosave after every change": debounced snapshot timer (5s) so a
    // burst of edits triggers one snapshot instead of one per keystroke.
    Timer {
        id: afterChangeSaveTimer
        interval: 5000
        repeat: false
        onTriggered: {
            if (typeof aegisubCore !== "undefined" && aegisubCore && subProject.isModified && subProject.subtitleModel) {
                subProject.subtitleModel.saveBackup(true);
            }
        }
    }

    // Upstream "Detach Video": independent playback window mirroring the main video.
    DetachedVideoWindow {
        id: detachedVideoWindow
        visible: root.videoDetached && root.hasVideoLoaded
    }

    // Application-wide file drag-and-drop handler
    DropArea {
        id: windowDropArea
        anchors.fill: parent
        z: 999
        onEntered: (drag) => {
            if (drag.hasUrls) {
                drag.acceptProposedAction();
            }
        }
        onPositionChanged: (drag) => {
            if (drag.hasUrls) {
                drag.acceptProposedAction();
            }
        }
        onDropped: (drop) => {
            if (drop.hasUrls) {
                for (var i = 0; i < drop.urls.length; ++i) {
                    root.handleDroppedUrl(drop.urls[i]);
                }
                drop.acceptProposedAction();
            }
        }
    }

    // Visual drop target overlay with animated hint
    Rectangle {
        id: dropOverlay
        anchors.fill: parent
        z: 1000
        enabled: false
        visible: windowDropArea.containsDrag
        color: Qt.rgba(0, 0.48, 0.85, 0.12)
        border.color: "#007ad9"
        border.width: 2

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 40, 520)
            height: 110
            color: "#ffffff"
            radius: 8
            border.color: "#007ad9"
            border.width: 1

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 8

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Release the mouse to load media or subtitle files")
                    font.pixelSize: 16
                    font.bold: true
                    color: "#005bb5"
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Supports video (MP4, MKV, etc.), audio (WAV, MP3, etc.), subtitles (ASS, SRT, etc.), keyframes and timecodes")
                    font.pixelSize: 11
                    color: "#666666"
                }
            }
        }
    }

    // Main window layout
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 1

        // Top row: video area (left) + audio and subtitle edit box (right)
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.topRowHeight
            Layout.minimumHeight: root.topRowHeight
            Layout.maximumHeight: root.topRowHeight
            spacing: root.showVideo ? 3 : 0

            // Video pane (expanded on demand)
            VideoBox {
                id: videoBox
                visible: root.showVideo
                Layout.preferredWidth: root.showVideo ? root.videoBoxWidth : 0
                Layout.minimumWidth: root.showVideo ? 380 : 0
                Layout.maximumWidth: root.showVideo ? Math.max(380, root.width - 380) : 0
                Layout.fillHeight: true
                winBg: pal.winBg
                winBorder: pal.winBorder
                winSunkenBorder: pal.winSunkenBorder
                showOverscan: root.overscanMask
                playbackSuspended: root.videoDetached

                onZoomApplied: (zoomStr) => {
                    if (typeof videoDisplayController !== "undefined") {
                        root.videoBoxWidth = Math.min(root.width - 380, Math.max(380, Math.round(videoDisplayController.preferredBoxWidth)));
                        root.topRowHeight = Math.min(root.height - 220, Math.max(260, Math.round(videoDisplayController.preferredBoxHeight)));
                    }
                }
            }

            // Vertical pane divider (resizable splitter between video and audio/edit box)
            Rectangle {
                id: verticalSplitter
                visible: root.showVideo
                Layout.fillHeight: true
                width: 6
                color: "transparent"

                Rectangle {
                    anchors.centerIn: parent
                    width: 1
                    height: parent.height
                    color: "#b0b0b0"
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.SplitHCursor
                    hoverEnabled: true

                    property int startX: 0
                    property int startW: 0

                    onPressed: (mouse) => {
                        startX = mouse.x;
                        startW = root.videoBoxWidth;
                    }

                    onPositionChanged: (mouse) => {
                        if (pressed) {
                            var delta = mouse.x - startX;
                            var targetW = startW + delta;
                            var minW = 380;
                            var maxW = root.width - 380;
                            root.videoBoxWidth = Math.min(maxW, Math.max(minW, targetW));
                            startW = root.videoBoxWidth;
                        }
                    }
                }
            }

            // Audio display and subtitle edit pane
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: root.showAudio ? 2 : 0

                // Audio waveform/spectrum pane
                AudioBox {
                    id: audioBox
                    visible: root.showAudio
                    project: subProject
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.showAudio ? root.audioBoxHeight : 0
                    winBg: pal.winBg
                    winBorder: pal.winBorder
                    winSunkenBorder: pal.winSunkenBorder
                    onStatusMessage: (msg) => root.statusMsgText = msg
                }

                // Subtitle edit box
                SubtitleEditBox {
                    id: subtitleEditBox
                    project: subProject
                    dialogs: dialogManager
                    videoCtrl: typeof videoController !== "undefined" ? videoController : null
                    onStatusMessage: (msg) => root.statusMsgText = msg
                }
            }
        }

        // Horizontal splitter between media views and subtitle grid
        Rectangle {
            Layout.fillWidth: true
            height: 5
            color: "transparent"

            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: 1
                color: "#bebebe"
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SplitVCursor
                hoverEnabled: true

                property int startY: 0
                property int startH: 0

                onPressed: (mouse) => {
                    startY = mouse.y;
                    startH = root.topRowHeight;
                }

                onPositionChanged: (mouse) => {
                    if (pressed) {
                        var delta = mouse.y - startY;
                        var targetH = startH + delta;
                        var minH = 160;
                        var maxH = root.height - 180;
                        root.topRowHeight = Math.min(maxH, Math.max(minH, targetH));
                        startH = root.topRowHeight;
                    }
                }
            }
        }

        // Subtitle grid area
        SubtitleGridArea {
            id: subtitleGridArea
            project: subProject
            videoCtrl: typeof videoController !== "undefined" ? videoController : null
            onStatusMessage: (msg) => root.statusMsgText = msg
            onCreateAudioClipRequested: root.createAudioClip()
        }

        // Status bar (two-pane status display)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 20
            color: pal.winBg
            antialiasing: false

            // Top border divider
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: "#bebebe" }

            RowLayout {
                anchors.fill: parent
                anchors.topMargin: 1
                anchors.leftMargin: 2
                anchors.rightMargin: 2
                spacing: 2

                // Pane 0: Contextual help and action descriptions
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: pal.winSunkenBg
                    border.color: pal.winSunkenBorder
                    border.width: 1
                    antialiasing: false

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.statusHelpText
                        font.pixelSize: 11
                        font.family: uiTheme.uiFont
                        renderType: Text.NativeRendering
                        color: "#303030"
                        elide: Text.ElideRight
                    }
                }

                // Pane 1: Status notifications (180px fixed width)
                Rectangle {
                    Layout.preferredWidth: 180
                    Layout.minimumWidth: 180
                    Layout.maximumWidth: 180
                    Layout.fillHeight: true
                    color: pal.winSunkenBg
                    border.color: pal.winSunkenBorder
                    border.width: 1
                    antialiasing: false

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.statusMsgText
                        font.pixelSize: 11
                        font.family: uiTheme.uiFont
                        renderType: Text.NativeRendering
                        color: "#303030"
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    // Color picker result injection into subtitle edit area
    Connections {
        target: dialogManager.dlgColorPicker
        function onColorAccepted(col, assBgrCode, assAbgrCode) {
            var tag = "";
            if (dialogManager.dlgColorPicker.targetProp === "primary") tag = "{\\c" + assBgrCode + "}";
            else if (dialogManager.dlgColorPicker.targetProp === "secondary") tag = "{\\2c" + assBgrCode + "}";
            else if (dialogManager.dlgColorPicker.targetProp === "outline") tag = "{\\3c" + assBgrCode + "}";
            else if (dialogManager.dlgColorPicker.targetProp === "shadow") tag = "{\\4c" + assBgrCode + "}";
            if (tag.length > 0) {
                var curPos = subtitleEditBox.subtitleEditArea.cursorPosition;
                var txt = subtitleEditBox.subtitleEditArea.text;
                subtitleEditBox.subtitleEditArea.text = txt.slice(0, curPos) + tag + txt.slice(curPos);
                subtitleEditBox.subtitleEditArea.cursorPosition = curPos + tag.length;
            }
        }
    }

    // External C++ controller signals and bindings
    Binding {
        target: typeof displayController !== "undefined" ? displayController : null
        property: "extraSnapPoints"
        value: subProject.snapPoints
    }

    Connections {
        target: typeof displayController !== "undefined" ? displayController : null
        function onNextLineRequested() { subProject.selectRow(subProject.currentSelectedIndex + 1, false, false); }
        function onPrevLineRequested() { subProject.selectRow(subProject.currentSelectedIndex - 1, false, false); }
    }

    Connections {
        target: typeof audioController !== "undefined" ? audioController : null
        // commit + autoNext 的统一入口（按钮与快捷键行为一致，C++ 端发出）
        function onNextLineRequested() { subProject.selectRow(subProject.currentSelectedIndex + 1, false, false); }
        function onAudioError(msg) { root.statusMsgText = msg; }
    }

    Connections {
        target: typeof videoController !== "undefined" ? videoController : null
        function onVideoError(msg) { root.statusMsgText = msg; }
    }

    Connections {
        target: typeof videoDisplayController !== "undefined" ? videoDisplayController : null
        function onSubtitleTextChanged(newText) {
            if (subProject.currentSelectedIndex >= 0 && subProject.currentSelectedIndex < subProject.subtitleModel.count) {
                subProject.subtitleModel.setProperty(subProject.currentSelectedIndex, "text", newText);
                subtitleEditBox.subtitleEditArea.text = newText;
            }
        }
        function onCommitRequested(desc) {
            root.statusMsgText = qsTr("Visual typesetting committed: ") + desc;
        }
    }

    Connections {
        target: typeof videoController !== "undefined" ? videoController : null
        function onVideoInfoChanged() {
            if (videoController && videoController.hasVideo && videoController.videoWidth > 0) {
                // 原版不做自适应猜测档位：窗口缩放固定取 Video/Default Zoom（默认 100%），
                // 视频框尺寸 = 视频尺寸 × 窗口缩放，贴着面板左上角放置。
                if (typeof videoDisplayController !== "undefined") {
                    root.videoBoxWidth = Math.min(root.width - 380, Math.max(380, Math.round(videoDisplayController.preferredBoxWidth)));
                    root.topRowHeight = Math.min(root.height - 220, Math.max(260, Math.round(videoDisplayController.preferredBoxHeight)));
                }
            }
        }
    }

    // Global keyboard shortcuts
    Shortcut { sequence: "Ctrl+N"; onActivated: root.confirmSaveAndProceed("new") }
    Shortcut { sequence: "Ctrl+O"; onActivated: root.confirmSaveAndProceed("open") }
    Shortcut { sequence: "Ctrl+Q"; onActivated: root.close() }
    Shortcut { sequence: "Ctrl+S"; onActivated: root.saveSubtitlesRequested() }
    Shortcut { sequence: "Ctrl+Shift+S"; onActivated: fileDialogSubSave.open() }
    Shortcut { sequence: "Ctrl+Z"; onActivated: subProject.undo() }
    Shortcut { sequence: "Ctrl+Y"; onActivated: subProject.redo() }
    Shortcut { sequence: "Ctrl+X"; onActivated: subProject.cutSelectedLines() }
    Shortcut { sequence: "Ctrl+C"; onActivated: subProject.copySelectedLines() }
    Shortcut { sequence: "Ctrl+V"; onActivated: subProject.pasteLines(false) }
    Shortcut { sequence: "Ctrl+Shift+V"; onActivated: dialogManager.dlgPasteOver.open() }
    Shortcut { sequence: "Ctrl+A"; onActivated: subProject.selectAllRows() }
    Shortcut {
        sequence: "Ctrl+F"
        onActivated: {
            dialogManager.dlgSearchReplace.isReplaceMode = false;
            dialogManager.dlgSearchReplace.open();
        }
    }
    Shortcut {
        sequence: "Ctrl+H"
        onActivated: {
            dialogManager.dlgSearchReplace.isReplaceMode = true;
            dialogManager.dlgSearchReplace.open();
        }
    }
    Shortcut { sequence: "Delete"; onActivated: subProject.deleteSelectedLines() }
    Shortcut {
        sequence: "Return"
        onActivated: {
            if (subtitleEditBox.subtitleEditArea.activeFocus) {
                subProject.insertLine(false, false, typeof videoController !== "undefined" ? videoController.currentTime : 0);
            }
        }
    }

    // Subtitle save confirmation coordinator workflow
    function confirmSaveAndProceed(action, data) {
        if (subProject.isModified) {
            // 原版 frame_main.cpp:315-316：弹确认框前先停止音视频播放
            if (typeof audioController !== "undefined" && audioController) audioController.stop();
            if (typeof videoController !== "undefined" && videoController) videoController.pause();
            pendingAction = action;
            pendingActionData = data !== undefined ? data : null;
            dialogManager.dlgSaveConfirm.prompt(subProject.currentFileName, action, data);
        } else {
            executePendingAction(action, data);
        }
    }

    function executePendingAction(action, data) {
        if (action === "exit") {
            isForceClosing = true;
            root.close();
            Qt.quit();
        } else if (action === "new") {
            subProject.fileNew();
        } else if (action === "open") {
            fileDialogSubOpen.open();
        } else if (action === "drop") {
            if (data) {
                subProject.openSubtitles(data);
                var fName = data.split('/').pop().split('\\').pop();
                root.statusMsgText = qsTr("Loaded subtitles: ") + fName;
            }
        }
    }

    function handleSaveConfirmSave() {
        var hasValidFile = subProject.currentFileName &&
                           subProject.currentFileName !== qsTr("Untitled") &&
                           subProject.currentFileName !== "Untitled";
        if (hasValidFile) {
            subProject.saveSubtitles(subProject.currentFileName);
            // 原版 subs_controller.cpp:250 陷阱语义：
            // 保存失败（仍 modified）等效于取消退出，字幕绝不丢失
            if (!subProject.isModified) {
                var act = pendingAction;
                var dat = pendingActionData;
                pendingAction = "";
                pendingActionData = null;
                executePendingAction(act, dat);
            } else {
                pendingAction = "";
                pendingActionData = null;
            }
        } else {
            // 无文件名：弹另存为；用户在另存为对话框中取消同样等效于取消退出
            fileDialogSubSave.open();
        }
    }

    function handleSaveConfirmDiscard() {
        var act = pendingAction;
        var dat = pendingActionData;
        pendingAction = "";
        pendingActionData = null;
        executePendingAction(act, dat);
    }

    function handleSaveConfirmCancel() {
        pendingAction = "";
        pendingActionData = null;
    }

    // Helper action functions
    function createAudioClip() {
        if (subProject.subtitleModel.count === 0) return;
        var it = subProject.subtitleModel.get(subProject.currentSelectedIndex);
        root.statusMsgText = qsTr("Created audio clip for line #%1").arg(it.lineNumber);
    }

    function jumpToLineStart() {
        if (subProject.subtitleModel.count === 0 || typeof videoController === "undefined") return;
        var it = subProject.subtitleModel.get(subProject.currentSelectedIndex);
        var sMs = AssUtils.assToMs(it.start);
        videoController.seekTime(sMs / 1000.0);
        root.statusMsgText = qsTr("Jumped to the current line start time: ") + it.start;
    }

    function jumpToLineEnd() {
        if (subProject.subtitleModel.count === 0 || typeof videoController === "undefined") return;
        var it = subProject.subtitleModel.get(subProject.currentSelectedIndex);
        var eMs = AssUtils.assToMs(it.end);
        videoController.seekTime(eMs / 1000.0);
        root.statusMsgText = qsTr("Jumped to the current line end time: ") + it.end;
    }

    function cycleTagHiding() {
        tagHidingMode = (tagHidingMode + 1) % 3;
        subProject.tagHidingMode = tagHidingMode;
        var modes = [qsTr("Show all ASS tags"), qsTr("Simplify ASS tags (❚)"), qsTr("Hide ASS tags completely")];
        root.statusMsgText = qsTr("ASS tag display mode: ") + modes[tagHidingMode];
    }

    // Handles dropped URLs or file paths by routing to corresponding controllers
    function handleDroppedUrl(url) {
        var urlStr = url ? url.toString() : "";
        if (!urlStr) return;
        var localPath = urlStr;
        if (localPath.startsWith("file:///")) {
            var afterScheme = decodeURIComponent(localPath.substring(7));
            if (afterScheme.length >= 3 && afterScheme.charAt(0) === '/' && afterScheme.charAt(2) === ':') {
                localPath = afterScheme.substring(1);
            } else {
                localPath = afterScheme;
            }
        } else if (localPath.startsWith("file://")) {
            localPath = decodeURIComponent(localPath.substring(7));
        }

        var lower = localPath.toLowerCase();
        var fileName = localPath.split('/').pop().split('\\').pop();

        // 1. Subtitle files: .ass, .ssa, .srt, .sub, .vtt
        var subExts = [".ass", ".ssa", ".srt", ".sub", ".vtt"];
        if (subExts.some(ext => lower.endsWith(ext))) {
            recentFiles.add("subtitles", localPath);
            root.confirmSaveAndProceed("drop", localPath);
            return;
        }

        // 2. Video files: .mp4, .mkv, .avi, .webm, .mov, etc.
        var videoExts = [".mp4", ".mkv", ".avi", ".webm", ".mov", ".wmv", ".flv", ".ts", ".m2ts", ".m4v", ".ogv", ".mpg", ".mpeg", ".3gp", ".vob", ".rmvb"];
        if (videoExts.some(ext => lower.endsWith(ext))) {
            if (typeof videoController !== "undefined" && videoController) {
                recentFiles.add("video", localPath);
                videoController.openVideo(localPath);
                root.statusMsgText = qsTr("Loaded video: ") + fileName;
                if (typeof audioController !== "undefined" && audioController) {
                    audioController.openAudioFromVideo(localPath);
                }
            }
            return;
        }

        // 3. Audio files: .wav, .mp3, .aac, .flac, etc.
        var audioExts = [".wav", ".mp3", ".aac", ".flac", ".m4a", ".ogg", ".opus", ".wma", ".ac3", ".alac", ".aiff"];
        if (audioExts.some(ext => lower.endsWith(ext))) {
            if (typeof audioController !== "undefined" && audioController) {
                recentFiles.add("audio", localPath);
                audioController.openAudio(localPath);
                root.statusMsgText = qsTr("Loaded audio: ") + fileName;
            }
            return;
        }

        // 4. Keyframe files
        if (lower.endsWith(".keyframes") || lower.endsWith(".pass") || lower.endsWith(".key") || (lower.endsWith(".txt") && lower.includes("keyframe"))) {
            if (typeof videoController !== "undefined" && videoController) {
                recentFiles.add("keyframes", localPath);
                videoController.openKeyframesFile(localPath);
                root.statusMsgText = qsTr("Loaded keyframes: ") + fileName;
            }
            return;
        }

        // 5. Timecode files
        if (lower.endsWith(".tc") || lower.endsWith(".timecode") || (lower.endsWith(".txt") && lower.includes("timecode"))) {
            if (typeof videoController !== "undefined" && videoController) {
                recentFiles.add("timecodes", localPath);
                videoController.openTimecodesFile(localPath);
                root.statusMsgText = qsTr("Loaded timecodes: ") + fileName;
            }
            return;
        }

        // Fallback: try opening as video
        if (typeof videoController !== "undefined" && videoController) {
            recentFiles.add("video", localPath);
            videoController.openVideo(localPath);
            root.statusMsgText = qsTr("Loaded video: ") + fileName;
        }
    }

    // Opens a MRU entry by media type (upstream Recent Files menus).
    // Reuses the pendingAction pipeline so subtitle opens still confirm unsaved changes.
    function openRecentFile(type, path) {
        if (!path) return;
        if (type === "subtitles") {
            recentFiles.add("subtitles", path);
            confirmSaveAndProceed("drop", path);
        } else if (type === "video") {
            recentFiles.add("video", path);
            if (typeof videoController !== "undefined" && videoController) {
                videoController.openVideo(path);
            }
        } else if (type === "audio") {
            recentFiles.add("audio", path);
            if (typeof audioController !== "undefined" && audioController) {
                audioController.openAudio(path);
            }
        } else if (type === "keyframes") {
            recentFiles.add("keyframes", path);
            if (typeof videoController !== "undefined" && videoController) {
                videoController.openKeyframesFile(path);
            }
        } else if (type === "timecodes") {
            recentFiles.add("timecodes", path);
            if (typeof videoController !== "undefined" && videoController) {
                videoController.openTimecodesFile(path);
            }
        }
    }
}
