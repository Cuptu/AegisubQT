// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Top menu bar hosting File, Edit, Subtitle, Timing, Video, Audio, Automation, View, and Help menus.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

MenuBar {
    id: menuBarRoot
    implicitHeight: 23

    required property var project
    property var dialogs: null
    property var videoCtrl: null
    property var audioCtrl: null
    property var videoDisplayCtrl: null
    property string viewMode: "full"
    property bool toolbarVisible: true
    property bool videoDetached: false
    property bool overscanMask: false

    signal statusMessage(string text)
    signal newSubtitlesRequested()
    signal exitRequested()
    signal jumpToLineStartRequested()
    signal jumpToLineEndRequested()
    signal cycleTagHidingRequested()
    signal openSubtitlesRequested()
    signal saveSubtitlesRequested()
    signal saveSubtitlesAsRequested()
    signal openVideoRequested()
    signal openAudioRequested()
    signal openKeyframesRequested()
    signal saveKeyframesRequested()
    signal openTimecodesRequested()
    signal saveTimecodesRequested()
    signal viewModeRequested(string mode)
    signal toggleToolbarRequested()
    signal detachVideoChanged(bool detached)
    signal overscanMaskToggled(bool mask)
    signal openRecentFileRequested(string type, string path)

    // Queries the GitHub Releases API and opens the release page when a newer
    // build exists (replaces the legacy fake "up to date" placeholder).
    function checkForUpdates() {
        var xhr = new XMLHttpRequest();
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== XMLHttpRequest.DONE) return;
            if (xhr.status === 200) {
                try {
                    var rel = JSON.parse(xhr.responseText);
                    var remote = (rel.tag_name || "").replace(/^v/i, "");
                    var currentVer = Qt.application.version || "4.0.2";
                    if (remote && remote !== currentVer) {
                        menuBarRoot.statusMessage(qsTr("New version %1 found, opening release page...").arg(remote));
                        Qt.openUrlExternally("https://github.com/Cuptu/AegisubQT/releases/latest");
                    } else {
                        menuBarRoot.statusMessage(qsTr("Already running the latest version (%1)").arg(currentVer));
                    }
                    return;
                } catch (e) {
                    // fall through to the failure message
                }
            }
            menuBarRoot.statusMessage(qsTr("Update check failed: cannot access GitHub Releases"));
        };
        xhr.open("GET", "https://api.github.com/repos/Cuptu/AegisubQT/releases/latest");
        xhr.send();
    }

    property alias fileMenu: fileMenu
    property alias editMenu: editMenu
    property alias subtitleMenu: subtitleMenu
    property alias videoMenu: videoMenu
    property alias autoMenu: autoMenu

    background: Rectangle {
        color: "#f0f0f0"
        border.color: "#dcdcdc"
        border.width: 1
        antialiasing: false
    }

    // Explicit Row layout ensures the child MenuBarItem delegates are always instantiated
    // regardless of whether the platform style inherits standard template fallbacks.
    contentItem: Row {
        spacing: menuBarRoot.spacing
        Repeater {
            model: menuBarRoot.contentModel
        }
    }

    delegate: MenuBarItem {
        id: mbItem
        implicitHeight: 21
        leftPadding: 8
        rightPadding: 8
        implicitWidth: Math.max(20, contentItem.implicitWidth + leftPadding + rightPadding)
        contentItem: Text {
            text: (mbItem.text || "").replace(/&/g, "")
            font.pixelSize: 12
            font.family: (typeof uiTheme !== "undefined" && uiTheme) ? uiTheme.uiFont : ""
            renderType: Text.NativeRendering
            color: "#000000"
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: 0
            color: mbItem.highlighted ? "#00000018" : "transparent"
        }
    }

    // File menu
    NativeMenu {
        id: fileMenu
        title: qsTr("&File")
        Action {
            text: qsTr("&New Subtitles") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⌘N" : "\tCtrl+N")
            shortcut: StandardKey.New
            icon.source: "../../assets/icons_native/new_toolbutton_16.png"
            onTriggered: menuBarRoot.newSubtitlesRequested()
        }
        Action {
            text: qsTr("&Open Subtitles...") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⌘O" : "\tCtrl+O")
            shortcut: StandardKey.Open
            icon.source: "../../assets/icons_native/open_toolbutton_16.png"
            onTriggered: menuBarRoot.openSubtitlesRequested()
        }
        Action { text: qsTr("Open Subtitles with &Charset..."); icon.source: "../../assets/icons_native/open_toolbutton_16.png"; onTriggered: menuBarRoot.openSubtitlesRequested() }
        Action {
            text: qsTr("Open Subtitles from &Video"); icon.source: "../../assets/icons_native/open_toolbutton_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: {
                var extracted = aegisubCore.extractSubtitlesFromVideo(menuBarRoot.videoCtrl ? menuBarRoot.videoCtrl.videoPath : "");
                if (extracted) {
                    menuBarRoot.openRecentFileRequested("subtitles", extracted);
                } else {
                    menuBarRoot.statusMessage(qsTr("Failed to extract subtitles from video (requires ffmpeg and a subtitle track in the container)"));
                }
            }
        }
        Action { text: qsTr("Open &Autosaved Subtitles..."); icon.source: "../../assets/icons_native/open_toolbutton_16.png"; onTriggered: if (dialogs) dialogs.dlgAutosave.open() }
        NativeMenu {
            id: recentSubsMenu
            title: qsTr("Recent &Subtitles")
            Instantiator {
                model: (typeof recentFiles !== "undefined") ? recentFiles.entries("subtitles") : []
                onObjectAdded: (index, object) => recentSubsMenu.insertAction(index, object)
                onObjectRemoved: (index, object) => recentSubsMenu.removeAction(object)
                delegate: Action {
                    text: (typeof recentFiles !== "undefined" && recentFiles.exists(modelData)) ? modelData
                          : (modelData + qsTr(" (missing)"))
                    enabled: (typeof recentFiles !== "undefined") ? recentFiles.exists(modelData) : false
                    onTriggered: menuBarRoot.openRecentFileRequested("subtitles", modelData)
                }
            }
            Action {
                text: qsTr("&Clear Recent Subtitles")
                enabled: (typeof recentFiles !== "undefined") && recentFiles.entries("subtitles").length > 0
                onTriggered: if (typeof recentFiles !== "undefined") recentFiles.clear("subtitles")
            }
        }
        Action {
            text: qsTr("&Save Subtitles") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⌘S" : "\tCtrl+S")
            shortcut: StandardKey.Save
            icon.source: "../../assets/icons_native/save_toolbutton_16.png"
            enabled: !!(menuBarRoot.project && menuBarRoot.project.isModified)
            onTriggered: menuBarRoot.saveSubtitlesRequested()
        }
        Action {
            text: qsTr("Save Subtitles &as...") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⇧⌘S" : "\tCtrl+Shift+S")
            shortcut: StandardKey.SaveAs
            icon.source: "../../assets/icons_native/save_as_toolbutton_16.png"
            onTriggered: menuBarRoot.saveSubtitlesAsRequested()
        }
        Action { text: qsTr("&Export Subtitles..."); icon.source: "../../assets/icons_native/export_menu_16.png"; onTriggered: if (dialogs) dialogs.dlgExport.open() }
        NativeMenuSep {}
        Action { text: qsTr("&Properties..."); icon.source: "../../assets/icons_native/properties_toolbutton_16.png"; onTriggered: if (dialogs) dialogs.dlgProperties.open() }
        Action { text: qsTr("&Attachments..."); icon.source: "../../assets/icons_native/attach_button_16.png"; onTriggered: if (dialogs) dialogs.dlgAttachments.open() }
        Action { text: qsTr("&Fonts Collector..."); icon.source: "../../assets/icons_native/font_collector_button_16.png"; onTriggered: if (dialogs) dialogs.dlgFontsCollector.open() }
        NativeMenuSep {}
        Action { text: qsTr("New &Window"); icon.source: "../../assets/icons_native/new_window_menu_16.png"; onTriggered: aegisubCore.launchNewInstance() }
        Action {
            text: qsTr("E&xit") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⌘Q" : "\tCtrl+Q")
            shortcut: StandardKey.Quit
            Action.menuRole: Action.QuitRole
            onTriggered: menuBarRoot.exitRequested()
        }
    }

    // Edit menu
    NativeMenu {
        id: editMenu
        title: qsTr("&Edit")
        Action {
            text: (!menuBarRoot.project || !menuBarRoot.project.canUndo) ?
                  (qsTr("Nothing to &undo") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⌘Z" : "\tCtrl+Z")) :
                  (qsTr("&Undo %s").indexOf("%s") >= 0 ?
                   qsTr("&Undo %s").replace("%s", menuBarRoot.project.undoDescription) + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⌘Z" : "\tCtrl+Z") :
                   qsTr("&Undo") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⌘Z" : "\tCtrl+Z"))
            shortcut: StandardKey.Undo
            icon.source: "../../assets/icons_native/undo_button_16.png"
            enabled: !!(menuBarRoot.project && menuBarRoot.project.canUndo)
            onTriggered: if (menuBarRoot.project) menuBarRoot.project.undo()
        }
        Action {
            text: (!menuBarRoot.project || !menuBarRoot.project.canRedo) ?
                  (qsTr("Nothing to &redo") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⇧⌘Z" : "\tCtrl+Y")) :
                  (qsTr("&Redo %s").indexOf("%s") >= 0 ?
                   qsTr("&Redo %s").replace("%s", menuBarRoot.project.redoDescription) + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⇧⌘Z" : "\tCtrl+Y") :
                   qsTr("&Redo") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⇧⌘Z" : "\tCtrl+Y"))
            shortcut: StandardKey.Redo
            icon.source: "../../assets/icons_native/redo_button_16.png"
            enabled: !!(menuBarRoot.project && menuBarRoot.project.canRedo)
            onTriggered: if (menuBarRoot.project) menuBarRoot.project.redo()
        }
        NativeMenuSep {}
        Action {
            text: qsTr("Cu&t Lines") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⌘X" : "\tCtrl+X")
            shortcut: StandardKey.Cut
            icon.source: "../../assets/icons_native/cut_button_16.png"
            enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
            onTriggered: menuBarRoot.project.cutSelectedLines()
        }
        Action {
            text: qsTr("&Copy Lines") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⌘C" : "\tCtrl+C")
            shortcut: StandardKey.Copy
            icon.source: "../../assets/icons_native/copy_button_16.png"
            enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
            onTriggered: menuBarRoot.project.copySelectedLines()
        }
        Action {
            text: qsTr("&Paste Lines") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⌘V" : "\tCtrl+V")
            shortcut: StandardKey.Paste
            icon.source: "../../assets/icons_native/paste_button_16.png"
            onTriggered: menuBarRoot.project.pasteLines(false)
        }
        Action {
            text: qsTr("Paste &Over...") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⇧⌘V" : "\tCtrl+Shift+V")
            enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
            onTriggered: if (dialogs) dialogs.dlgPasteOver.open()
        }
        NativeMenuSep {}
        Action {
            text: qsTr("&Find...") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⌘F" : "\tCtrl+F")
            shortcut: StandardKey.Find
            icon.source: "../../assets/icons_native/find_button_16.png"
            onTriggered: if (dialogs) { dialogs.dlgSearchReplace.isReplaceMode = false; dialogs.dlgSearchReplace.open(); }
        }
        Action {
            text: qsTr("Find &Next") + "\tF3"
            shortcut: StandardKey.FindNext
            icon.source: "../../assets/icons_native/find_next_menu_16.png"
            onTriggered: if (menuBarRoot.project) menuBarRoot.project.findNext()
        }
        Action {
            text: qsTr("&Replace...") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⌘H" : "\tCtrl+H")
            shortcut: StandardKey.Replace
            icon.source: "../../assets/icons_native/find_replace_menu_16.png"
            onTriggered: if (dialogs) { dialogs.dlgSearchReplace.isReplaceMode = true; dialogs.dlgSearchReplace.open(); }
        }
    }

    // Subtitle menu
    NativeMenu {
        id: subtitleMenu
        title: qsTr("&Subtitle")
        Action { text: qsTr("&Styles Manager..."); icon.source: "../../assets/icons_native/style_toolbutton_16.png"; onTriggered: if (dialogs) dialogs.dlgStyleManager.open() }
        Action { text: qsTr("Stylin&g Assistant..."); icon.source: "../../assets/icons_native/styling_toolbutton_16.png"; onTriggered: if (dialogs) dialogs.dlgStylingAssistant.open() }
        Action { text: qsTr("&Translation Assistant..."); icon.source: "../../assets/icons_native/translation_toolbutton_16.png"; onTriggered: if (dialogs) dialogs.dlgTranslation.open() }
        Action { text: qsTr("&Resample Resolution..."); icon.source: "../../assets/icons_native/resample_toolbutton_16.png"; onTriggered: if (dialogs) dialogs.dlgResample.open() }
        Action { text: qsTr("&Spell Checker..."); icon.source: "../../assets/icons_native/spellcheck_toolbutton_16.png"; onTriggered: if (dialogs) dialogs.dlgSpellChecker.open() }
        NativeMenuSep {}
        NativeMenu {
            title: qsTr("&Insert Lines")
            Action {
                text: qsTr("&Before Current"); enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
                onTriggered: menuBarRoot.project.insertLine(true, false)
            }
            Action {
                text: qsTr("&After Current"); enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
                onTriggered: menuBarRoot.project.insertLine(false, false)
            }
            Action {
                text: qsTr("Before Current, at Video Time"); enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo && menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
                onTriggered: menuBarRoot.project.insertLine(true, true, menuBarRoot.videoCtrl ? menuBarRoot.videoCtrl.currentTime : 0)
            }
            Action {
                text: qsTr("After Current, at Video Time"); enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo && menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
                onTriggered: menuBarRoot.project.insertLine(false, true, menuBarRoot.videoCtrl ? menuBarRoot.videoCtrl.currentTime : 0)
            }
        }
        Action {
            text: qsTr("&Duplicate Lines")
            enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
            onTriggered: menuBarRoot.project.duplicateSelectedLines()
        }
        Action {
            text: qsTr("Split lines before current frame") + "\tCtrl+D"; enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo && menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
            onTriggered: menuBarRoot.project.splitLineAtFrame(-1, menuBarRoot.videoCtrl ? menuBarRoot.videoCtrl.currentTime : 0)
        }
        Action {
            text: qsTr("Split lines after current frame") + "\tCtrl+Shift+D"; enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo && menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
            onTriggered: menuBarRoot.project.splitLineAtFrame(1, menuBarRoot.videoCtrl ? menuBarRoot.videoCtrl.currentTime : 0)
        }
        Action {
            text: qsTr("De&lete Lines") + "\tCtrl+Delete"
            icon.source: "../../assets/icons_native/delete_button_16.png"
            enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
            onTriggered: menuBarRoot.project.deleteSelectedLines()
        }
        NativeMenuSep {}
        NativeMenu {
            title: qsTr("Join Lines")
            Action {
                text: qsTr("&Concatenate"); enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length >= 2)
                onTriggered: menuBarRoot.project.joinSelectedLines(0)
            }
            Action {
                text: qsTr("Keep &First"); enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length >= 2)
                onTriggered: menuBarRoot.project.joinSelectedLines(1)
            }
            Action {
                text: qsTr("As &Karaoke"); enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length >= 2)
                onTriggered: menuBarRoot.project.joinSelectedLines(2)
            }
        }
        Action {
            text: qsTr("Recom&bine Lines"); enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length >= 2)
            onTriggered: menuBarRoot.project.recombineSelectedLines()
        }
        Action {
            text: qsTr("Split by Karaoke"); enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
            onTriggered: menuBarRoot.project.splitSelectedByKaraoke()
        }
        NativeMenuSep {}
        NativeMenu {
            title: qsTr("Sort All Lines")
            Action { text: qsTr("&Start Time"); onTriggered: menuBarRoot.project.sortLines("start", false) }
            Action { text: qsTr("&End Time"); onTriggered: menuBarRoot.project.sortLines("end", false) }
            Action { text: qsTr("&Style"); onTriggered: menuBarRoot.project.sortLines("style", false) }
            Action { text: qsTr("Act&or"); onTriggered: menuBarRoot.project.sortLines("actor", false) }
            Action { text: qsTr("E&ffect"); onTriggered: menuBarRoot.project.sortLines("effect", false) }
            Action { text: qsTr("&Layer"); onTriggered: menuBarRoot.project.sortByColumn(1) }
        }
        NativeMenu {
            title: qsTr("Sort Selected Lines")
            enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length >= 2)
            Action { text: qsTr("&Start Time"); onTriggered: menuBarRoot.project.sortLines("start", true) }
            Action { text: qsTr("&End Time"); onTriggered: menuBarRoot.project.sortLines("end", true) }
            Action { text: qsTr("&Style"); onTriggered: menuBarRoot.project.sortLines("style", true) }
            Action { text: qsTr("Act&or"); onTriggered: menuBarRoot.project.sortLines("actor", true) }
            Action { text: qsTr("E&ffect"); onTriggered: menuBarRoot.project.sortLines("effect", true) }
            Action { text: qsTr("&Layer"); onTriggered: menuBarRoot.project.sortByColumn(1) }
        }
        Action {
            text: qsTr("Swap Lines"); icon.source: "../../assets/icons_native/arrow_sort_16.png"
            enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length === 2)
            onTriggered: menuBarRoot.project.swapSelectedLines()
        }
        Action { text: qsTr("Select &Lines..."); icon.source: "../../assets/icons_native/select_lines_button_16.png"; onTriggered: if (dialogs) dialogs.dlgSelectLines.open() }
        Action {
            text: qsTr("Select &All") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⌘A" : "\tCtrl+A")
            shortcut: StandardKey.SelectAll
            onTriggered: menuBarRoot.project.selectAllRows()
        }
    }

    // Timing menu
    NativeMenu {
        title: qsTr("&Timing")
        Action { text: qsTr("&Shift Times...") + "\tCtrl+I"; icon.source: "../../assets/icons_native/shift_times_toolbutton_16.png"; onTriggered: if (dialogs) dialogs.dlgShiftTimes.open() }
        Action { text: qsTr("&Timing Post-Processor..."); icon.source: "../../assets/icons_native/timing_processor_toolbutton_16.png"; onTriggered: if (dialogs) dialogs.dlgTimingProcessor.open() }
        Action { text: qsTr("&Kanji Timer..."); icon.source: "../../assets/icons_native/kara_timing_copier_16.png"; onTriggered: if (dialogs) dialogs.dlgKanjiTimer.open() }
        NativeMenuSep {}
        Action {
            text: qsTr("Snap Start to &Video") + "\tCtrl+3"
            icon.source: "../../assets/icons_native/substart_to_video_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: menuBarRoot.project.snapStartTimeToVideo(menuBarRoot.videoCtrl ? menuBarRoot.videoCtrl.currentTime : 0)
        }
        Action {
            text: qsTr("Snap End to &Video") + "\tCtrl+4"
            icon.source: "../../assets/icons_native/subend_to_video_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: menuBarRoot.project.snapEndTimeToVideo(menuBarRoot.videoCtrl ? menuBarRoot.videoCtrl.currentTime : 0)
        }
        Action {
            text: qsTr("Snap to &Scene") + "\tCtrl+Shift+D"
            icon.source: "../../assets/icons_native/snap_subs_to_scene_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: menuBarRoot.project.snapToKeyframes(menuBarRoot.project.snapPoints)
        }
        Action {
            text: qsTr("Shift to &Current Frame"); icon.source: "../../assets/icons_native/shift_to_frame_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: menuBarRoot.project.shiftToCurrentFrame(menuBarRoot.videoCtrl ? menuBarRoot.videoCtrl.currentTime : 0)
        }
        NativeMenuSep {}
        NativeMenu {
            title: qsTr("Make Times &Continuous")
            Action {
                text: qsTr("Change &Start"); enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
                onTriggered: menuBarRoot.project.makeTimesContinuous(true)
            }
            Action {
                text: qsTr("Change &End"); enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
                onTriggered: menuBarRoot.project.makeTimesContinuous(false)
            }
        }
    }

    // Video menu
    NativeMenu {
        id: videoMenu
        title: qsTr("&Video")
        Action { text: qsTr("&Open Video..."); icon.source: "../../assets/icons_native/open_video_menu_16.png"; onTriggered: menuBarRoot.openVideoRequested() }
        NativeMenu {
            id: recentVideoMenu
            title: qsTr("Recent &Videos")
            Instantiator {
                model: (typeof recentFiles !== "undefined") ? recentFiles.entries("video") : []
                onObjectAdded: (index, object) => recentVideoMenu.insertAction(index, object)
                onObjectRemoved: (index, object) => recentVideoMenu.removeAction(object)
                delegate: Action {
                    text: (typeof recentFiles !== "undefined" && recentFiles.exists(modelData)) ? modelData
                          : (modelData + qsTr(" (missing)"))
                    enabled: (typeof recentFiles !== "undefined") ? recentFiles.exists(modelData) : false
                    onTriggered: menuBarRoot.openRecentFileRequested("video", modelData)
                }
            }
            Action {
                text: qsTr("&Clear Recent Videos")
                enabled: (typeof recentFiles !== "undefined") && recentFiles.entries("video").length > 0
                onTriggered: if (typeof recentFiles !== "undefined") recentFiles.clear("video")
            }
        }
        Action {
            text: qsTr("&Close Video"); icon.source: "../../assets/icons_native/close_video_menu_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: {
                if (menuBarRoot.videoCtrl) menuBarRoot.videoCtrl.closeVideo();
                menuBarRoot.statusMessage(qsTr("Video closed"));
            }
        }
        Action { text: qsTr("Use &Dummy Video..."); icon.source: "../../assets/icons_native/use_dummy_video_menu_16.png"; onTriggered: if (dialogs) dialogs.dlgDummyVideo.open() }
        Action {
            text: qsTr("Show &Video Details"); icon.source: "../../assets/icons_native/show_video_details_menu_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: if (dialogs) dialogs.dlgVideoDetails.open()
        }
        NativeMenuSep {}
        Action { text: qsTr("Open Timecodes File..."); icon.source: "../../assets/icons_native/open_timecodes_menu_16.png"; onTriggered: menuBarRoot.openTimecodesRequested() }
        Action {
            text: qsTr("Save Timecodes File..."); icon.source: "../../assets/icons_native/save_timecodes_menu_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: menuBarRoot.saveTimecodesRequested()
        }
        Action {
            text: qsTr("Close Timecodes File"); icon.source: "../../assets/icons_native/close_timecodes_menu_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasTimecodes)
            onTriggered: {
                if (menuBarRoot.videoCtrl) menuBarRoot.videoCtrl.closeTimecodesFile();
                menuBarRoot.statusMessage(qsTr("Timecode file closed"));
            }
        }
        NativeMenu {
            id: recentTimecodesMenu
            title: qsTr("Recent &Timecodes")
            Instantiator {
                model: (typeof recentFiles !== "undefined") ? recentFiles.entries("timecodes") : []
                onObjectAdded: (index, object) => recentTimecodesMenu.insertAction(index, object)
                onObjectRemoved: (index, object) => recentTimecodesMenu.removeAction(object)
                delegate: Action {
                    text: (typeof recentFiles !== "undefined" && recentFiles.exists(modelData)) ? modelData
                          : (modelData + qsTr(" (missing)"))
                    enabled: (typeof recentFiles !== "undefined") ? recentFiles.exists(modelData) : false
                    onTriggered: menuBarRoot.openRecentFileRequested("timecodes", modelData)
                }
            }
            Action {
                text: qsTr("&Clear Recent Timecodes")
                enabled: (typeof recentFiles !== "undefined") && recentFiles.entries("timecodes").length > 0
                onTriggered: if (typeof recentFiles !== "undefined") recentFiles.clear("timecodes")
            }
        }
        NativeMenuSep {}
        Action { text: qsTr("Open Keyframes..."); icon.source: "../../assets/icons_native/open_keyframes_menu_16.png"; onTriggered: menuBarRoot.openKeyframesRequested() }
        Action {
            text: qsTr("Save Keyframes..."); icon.source: "../../assets/icons_native/save_keyframes_menu_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasKeyframes)
            onTriggered: menuBarRoot.saveKeyframesRequested()
        }
        Action {
            text: qsTr("Close Keyframes"); icon.source: "../../assets/icons_native/close_keyframes_menu_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasCustomKeyframes)
            onTriggered: {
                if (menuBarRoot.videoCtrl) menuBarRoot.videoCtrl.closeKeyframes();
                menuBarRoot.statusMessage(qsTr("Keyframes closed"));
            }
        }
        NativeMenu {
            id: recentKeyframesMenu
            title: qsTr("Recent &Keyframes")
            Instantiator {
                model: (typeof recentFiles !== "undefined") ? recentFiles.entries("keyframes") : []
                onObjectAdded: (index, object) => recentKeyframesMenu.insertAction(index, object)
                onObjectRemoved: (index, object) => recentKeyframesMenu.removeAction(object)
                delegate: Action {
                    text: (typeof recentFiles !== "undefined" && recentFiles.exists(modelData)) ? modelData
                          : (modelData + qsTr(" (missing)"))
                    enabled: (typeof recentFiles !== "undefined") ? recentFiles.exists(modelData) : false
                    onTriggered: menuBarRoot.openRecentFileRequested("keyframes", modelData)
                }
            }
            Action {
                text: qsTr("&Clear Recent Keyframes")
                enabled: (typeof recentFiles !== "undefined") && recentFiles.entries("keyframes").length > 0
                onTriggered: if (typeof recentFiles !== "undefined") recentFiles.clear("keyframes")
            }
        }
        NativeMenuSep {}
        Action {
            text: qsTr("Detach &Video"); icon.source: "../../assets/icons_native/detach_video_menu_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            checkable: true
            checked: menuBarRoot.videoDetached
            onTriggered: menuBarRoot.detachVideoChanged(!menuBarRoot.videoDetached)
        }
        NativeMenu {
            title: qsTr("Set &Zoom")
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            Action {
                text: "&50%"; checkable: true
                checked: menuBarRoot.videoDisplayCtrl && Math.abs(menuBarRoot.videoDisplayCtrl.windowZoom - 0.5) < 0.01
                onTriggered: if (menuBarRoot.videoDisplayCtrl) menuBarRoot.videoDisplayCtrl.setWindowZoom(0.5)
            }
            Action {
                text: "&75%"; checkable: true
                checked: menuBarRoot.videoDisplayCtrl && Math.abs(menuBarRoot.videoDisplayCtrl.windowZoom - 0.75) < 0.01
                onTriggered: if (menuBarRoot.videoDisplayCtrl) menuBarRoot.videoDisplayCtrl.setWindowZoom(0.75)
            }
            Action {
                text: "&100%"; checkable: true
                checked: menuBarRoot.videoDisplayCtrl && Math.abs(menuBarRoot.videoDisplayCtrl.windowZoom - 1.0) < 0.01
                onTriggered: if (menuBarRoot.videoDisplayCtrl) menuBarRoot.videoDisplayCtrl.setWindowZoom(1.0)
            }
            Action {
                text: "&150%"; checkable: true
                checked: menuBarRoot.videoDisplayCtrl && Math.abs(menuBarRoot.videoDisplayCtrl.windowZoom - 1.5) < 0.01
                onTriggered: if (menuBarRoot.videoDisplayCtrl) menuBarRoot.videoDisplayCtrl.setWindowZoom(1.5)
            }
            Action {
                text: "&200%"; checkable: true
                checked: menuBarRoot.videoDisplayCtrl && Math.abs(menuBarRoot.videoDisplayCtrl.windowZoom - 2.0) < 0.01
                onTriggered: if (menuBarRoot.videoDisplayCtrl) menuBarRoot.videoDisplayCtrl.setWindowZoom(2.0)
            }
        }
        NativeMenu {
            title: qsTr("Override &Aspect Ratio")
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            Action {
                text: qsTr("&Default"); checkable: true
                checked: menuBarRoot.videoDisplayCtrl && menuBarRoot.videoDisplayCtrl.arOverride === 0
                onTriggered: if (menuBarRoot.videoDisplayCtrl) menuBarRoot.videoDisplayCtrl.setArOverride(0)
            }
            Action {
                text: qsTr("&Fullscreen (4:3)"); checkable: true
                checked: menuBarRoot.videoDisplayCtrl && Math.abs(menuBarRoot.videoDisplayCtrl.arOverride - 4/3) < 0.01
                onTriggered: if (menuBarRoot.videoDisplayCtrl) menuBarRoot.videoDisplayCtrl.setArOverride(4/3)
            }
            Action {
                text: qsTr("&Widescreen (16:9)"); checkable: true
                checked: menuBarRoot.videoDisplayCtrl && Math.abs(menuBarRoot.videoDisplayCtrl.arOverride - 16/9) < 0.01
                onTriggered: if (menuBarRoot.videoDisplayCtrl) menuBarRoot.videoDisplayCtrl.setArOverride(16/9)
            }
            Action {
                text: qsTr("&Cinematic (2.35)"); checkable: true
                checked: menuBarRoot.videoDisplayCtrl && Math.abs(menuBarRoot.videoDisplayCtrl.arOverride - 2.35) < 0.01
                onTriggered: if (menuBarRoot.videoDisplayCtrl) menuBarRoot.videoDisplayCtrl.setArOverride(2.35)
            }
            Action {
                text: qsTr("C&ustom...")
                onTriggered: {
                    arCustomInput.text = (menuBarRoot.videoDisplayCtrl && menuBarRoot.videoDisplayCtrl.arOverride > 0)
                            ? menuBarRoot.videoDisplayCtrl.arOverride.toFixed(3) : "2.00"
                    arCustomPopup.open()
                }
            }
        }
        Action {
            text: qsTr("Show &Overscan Mask"); enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            checkable: true
            checked: menuBarRoot.overscanMask
            onTriggered: menuBarRoot.overscanMaskToggled(!menuBarRoot.overscanMask)
        }
        Action {
            text: qsTr("Reset Video &Pan")
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: {
                if (typeof videoDisplayController !== "undefined") {
                    videoDisplayController.resetPanZoom();
                }
                menuBarRoot.statusMessage(qsTr("Reset the video's position in the video display"));
            }
        }
        NativeMenuSep {}
        Action {
            text: qsTr("&Jump to...") + "\tCtrl+G"
            icon.source: "../../assets/icons_native/jumpto_button_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: if (dialogs) dialogs.dlgJumpTo.open()
        }
        Action {
            text: qsTr("Jump to &Start") + "\tCtrl+1"
            icon.source: "../../assets/icons_native/video_to_substart_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: menuBarRoot.jumpToLineStartRequested()
        }
        Action {
            text: qsTr("Jump to &End") + "\tCtrl+2"
            icon.source: "../../assets/icons_native/video_to_subend_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: menuBarRoot.jumpToLineEndRequested()
        }
    }

    // Audio menu
    NativeMenu {
        title: qsTr("&Audio")
        Action { text: qsTr("&Open Audio File..."); icon.source: "../../assets/icons_native/open_audio_menu_16.png"; onTriggered: menuBarRoot.openAudioRequested() }
        Action {
            text: qsTr("Open Audio from &Video"); icon.source: "../../assets/icons_native/open_audio_from_video_menu_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: {
                if (menuBarRoot.audioCtrl) {
                    var ok = menuBarRoot.audioCtrl.openAudioFromVideo(menuBarRoot.videoCtrl ? menuBarRoot.videoCtrl.videoPath : "");
                    if (ok) {
                        menuBarRoot.statusMessage(qsTr("Audio extracted from the current video and loaded"));
                    } else {
                        menuBarRoot.statusMessage(qsTr("Failed to extract audio from video"));
                    }
                }
            }
        }
        Action {
            text: qsTr("&Close Audio"); icon.source: "../../assets/icons_native/close_audio_menu_16.png"
            enabled: !!(menuBarRoot.audioCtrl && menuBarRoot.audioCtrl.hasAudio)
            onTriggered: {
                if (menuBarRoot.audioCtrl) menuBarRoot.audioCtrl.closeAudio();
                menuBarRoot.statusMessage(qsTr("Audio closed"));
            }
        }
        NativeMenu {
            id: recentAudioMenu
            title: qsTr("Recent &Audio")
            Instantiator {
                model: (typeof recentFiles !== "undefined") ? recentFiles.entries("audio") : []
                onObjectAdded: (index, object) => recentAudioMenu.insertAction(index, object)
                onObjectRemoved: (index, object) => recentAudioMenu.removeAction(object)
                delegate: Action {
                    text: (typeof recentFiles !== "undefined" && recentFiles.exists(modelData)) ? modelData
                          : (modelData + qsTr(" (missing)"))
                    enabled: (typeof recentFiles !== "undefined") ? recentFiles.exists(modelData) : false
                    onTriggered: menuBarRoot.openRecentFileRequested("audio", modelData)
                }
            }
            Action {
                text: qsTr("&Clear Recent Audio")
                enabled: (typeof recentFiles !== "undefined") && recentFiles.entries("audio").length > 0
                onTriggered: if (typeof recentFiles !== "undefined") recentFiles.clear("audio")
            }
        }
        NativeMenuSep {}
        Action {
            text: qsTr("&Spectrum Display"); enabled: !!(menuBarRoot.audioCtrl && menuBarRoot.audioCtrl.hasAudio)
            onTriggered: {
                if (menuBarRoot.audioCtrl) {
                    menuBarRoot.audioCtrl.waveformMode = false;
                    menuBarRoot.statusMessage(qsTr("Audio view switched to: spectrum"));
                }
            }
        }
        Action {
            text: qsTr("&Waveform Display"); enabled: !!(menuBarRoot.audioCtrl && menuBarRoot.audioCtrl.hasAudio)
            onTriggered: {
                if (menuBarRoot.audioCtrl) {
                    menuBarRoot.audioCtrl.waveformMode = true;
                    menuBarRoot.statusMessage(qsTr("Audio view switched to: waveform"));
                }
            }
        }
        Action {
            text: qsTr("Open 2h30 Blank Audio")
            onTriggered: {
                if (menuBarRoot.audioCtrl && menuBarRoot.audioCtrl.openBlankAudio()) {
                    menuBarRoot.statusMessage(qsTr("Loaded 2h30m blank audio (virtually synthesised)"));
                }
            }
        }
        Action {
            text: qsTr("Open 2h30 Noise Audio")
            onTriggered: {
                if (menuBarRoot.audioCtrl && menuBarRoot.audioCtrl.openNoiseAudio()) {
                    menuBarRoot.statusMessage(qsTr("Loaded 2h30m white noise audio (virtually synthesised)"));
                }
            }
        }
    }

    // Automation menu
    NativeMenu {
        id: autoMenu
        title: qsTr("A&utomation")
        Action { text: qsTr("&Automation..."); icon.source: "../../assets/icons_native/automation_toolbutton_16.png"; onTriggered: if (dialogs) dialogs.dlgAutomation.open() }
        Action {
            text: qsTr("&Reload Automation Scripts"); onTriggered: {
                if (typeof automationManager !== "undefined") automationManager.reloadAll();
                menuBarRoot.statusMessage(qsTr("Reloaded all automation scripts"));
            }
        }
        NativeMenuSep {}
        Instantiator {
            model: (typeof automationManager !== "undefined") ? automationManager.macros : []
            onObjectAdded: (index, object) => autoMenu.insertAction(index + 3, object)
            onObjectRemoved: (index, object) => autoMenu.removeAction(object)
            delegate: Action {
                text: modelData.name
                onTriggered: {
                    if (typeof automationManager !== "undefined" && menuBarRoot.project) {
                        automationManager.runMacro(modelData.id, menuBarRoot.project, menuBarRoot.project.currentSelectedIndex, menuBarRoot.project.selectedIndices);
                    }
                }
            }
        }
    }

    // View menu
    NativeMenu {
        title: qsTr("Vie&w")
        Action { text: qsTr("&Language..."); icon.source: "../../assets/icons_native/languages_menu_16.png"; onTriggered: if (dialogs) { dialogs.dlgLanguage.open(); } }
        Action {
            text: qsTr("&Options...") + ((Qt.platform.os === "osx" || Qt.platform.os === "macos") ? "\t⌘," : "")
            shortcut: StandardKey.Preferences
            Action.menuRole: Action.PreferencesRole
            icon.source: "../../assets/icons_native/options_button_16.png"
            onTriggered: if (dialogs) dialogs.dlgPreferences.open()
        }
        NativeMenuSep {}
        Action {
            text: qsTr("S&ubs Only View"); checkable: true
            checked: menuBarRoot.viewMode === "subs"
            onTriggered: menuBarRoot.viewModeRequested("subs")
        }
        Action {
            text: qsTr("&Video+Subs View"); checkable: true
            checked: menuBarRoot.viewMode === "video"
            onTriggered: menuBarRoot.viewModeRequested("video")
        }
        Action {
            text: qsTr("&Audio+Subs View"); checkable: true
            checked: menuBarRoot.viewMode === "audio"
            onTriggered: menuBarRoot.viewModeRequested("audio")
        }
        Action {
            text: qsTr("&Full view"); checkable: true
            checked: menuBarRoot.viewMode === "full"
            onTriggered: menuBarRoot.viewModeRequested("full")
        }
        NativeMenuSep {}
        Action {
            text: qsTr("Sh&ow Tags")
            checkable: true
            checked: menuBarRoot.project ? menuBarRoot.project.tagHidingMode === 0 : false
            onTriggered: {
                if (menuBarRoot.project) menuBarRoot.project.tagHidingMode = 0;
                menuBarRoot.statusMessage(qsTr("ASS Override Tag mode set to show full tags."));
            }
        }
        Action {
            text: qsTr("S&implify Tags")
            checkable: true
            checked: menuBarRoot.project ? menuBarRoot.project.tagHidingMode === 1 : false
            onTriggered: {
                if (menuBarRoot.project) menuBarRoot.project.tagHidingMode = 1;
                menuBarRoot.statusMessage(qsTr("ASS Override Tag mode set to simplify tags."));
            }
        }
        Action {
            text: qsTr("&Hide Tags")
            checkable: true
            checked: menuBarRoot.project ? menuBarRoot.project.tagHidingMode === 2 : true
            onTriggered: {
                if (menuBarRoot.project) menuBarRoot.project.tagHidingMode = 2;
                menuBarRoot.statusMessage(qsTr("ASS Override Tag mode set to hide tags."));
            }
        }
        NativeMenuSep {}
        Action {
            text: qsTr("Toggle &Toolbar")
            checkable: true
            checked: menuBarRoot.toolbarVisible
            onTriggered: menuBarRoot.toggleToolbarRequested()
        }
    }

    // Help menu
    NativeMenu {
        title: qsTr("&Help")
        Action { text: qsTr("&Contents") + "\tF1"; icon.source: "../../assets/icons_native/contents_button_16.png"; onTriggered: Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/") }
        NativeMenuSep {}
        Action { text: qsTr("&Website"); icon.source: "../../assets/icons_native/website_button_16.png"; onTriggered: Qt.openUrlExternally("https://github.com/Cuptu/AegisubQT") }
        Action { text: qsTr("&Bug Tracker..."); icon.source: "../../assets/icons_native/bugtracker_button_16.png"; onTriggered: Qt.openUrlExternally("https://github.com/Cuptu/AegisubQT/issues") }
        NativeMenuSep {}
        Action { text: qsTr("&Check for Updates..."); onTriggered: menuBarRoot.checkForUpdates() }
        Action {
            text: qsTr("&About Aegisub...")
            Action.menuRole: Action.AboutRole
            icon.source: "../../assets/icons_native/about_menu_16.png"
            onTriggered: if (dialogs) dialogs.dlgAbout.open()
        }
        Action { text: qsTr("&Log Window..."); icon.source: "../../assets/icons_native/about_menu_16.png"; onTriggered: if (dialogs) dialogs.dlgLog.open() }
    }

    // Custom aspect ratio input popup (upstream "Custom" entry in Override AR).
    Popup {
        id: arCustomPopup
        x: (menuBarRoot.width - width) / 2
        y: menuBarRoot.height + 4
        width: 220
        height: 108
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: "#f0f0f0"
            border.color: "#a0a0a0"
            border.width: 1
        }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8
            Text {
                text: qsTr("Enter a custom aspect ratio (width/height):")
                font.pixelSize: 12
                font.family: uiTheme.uiFont
            }
            NativeTextBox {
                id: arCustomInput
                Layout.fillWidth: true
                text: "2.00"
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 6
                NativeButton {
                    text: qsTr("OK")
                    implicitWidth: 64
                    onClicked: {
                        var ar = parseFloat(arCustomInput.text);
                        if (!isNaN(ar) && ar > 0.05 && menuBarRoot.videoDisplayCtrl) {
                            menuBarRoot.videoDisplayCtrl.setArOverride(ar);
                            menuBarRoot.statusMessage(qsTr("Aspect ratio overridden to %1").arg(ar.toFixed(3)));
                        }
                        arCustomPopup.close();
                    }
                }
                NativeButton {
                    text: qsTr("Cancel")
                    implicitWidth: 64
                    onClicked: arCustomPopup.close()
                }
            }
        }
    }
}
