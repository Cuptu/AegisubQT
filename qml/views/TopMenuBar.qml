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

    delegate: MenuBarItem {
        id: mbItem
        implicitHeight: 21
        contentItem: Text {
            text: mbItem.text.replace(/&/g, "")
            font.pixelSize: 12
            font.family: uiTheme.uiFont
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
        Action { text: qsTr("&New Subtitles") + "\tCtrl+N"; icon.source: "../../assets/icons_native/new_toolbutton_16.png"; onTriggered: menuBarRoot.newSubtitlesRequested() }
        Action { text: qsTr("&Open Subtitles...") + "\tCtrl+O"; icon.source: "../../assets/icons_native/open_toolbutton_16.png"; onTriggered: menuBarRoot.openSubtitlesRequested() }
        Action { text: qsTr("Open Subtitles with &Charset..."); icon.source: "../../assets/icons_native/open_toolbutton_16.png"; onTriggered: menuBarRoot.openSubtitlesRequested() }
        Action {
            text: qsTr("Open Subtitles from &Video"); icon.source: "../../assets/icons_native/open_toolbutton_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: menuBarRoot.statusMessage("从视频中打开字幕...")
        }
        Action { text: qsTr("Open &Autosaved Subtitles..."); icon.source: "../../assets/icons_native/open_toolbutton_16.png"; onTriggered: if (dialogs) dialogs.dlgAutosave.open() }
        Action {
            text: qsTr("&Save Subtitles") + "\tCtrl+S"
            icon.source: "../../assets/icons_native/save_toolbutton_16.png"
            enabled: !!(menuBarRoot.project && menuBarRoot.project.isModified)
            onTriggered: menuBarRoot.saveSubtitlesRequested()
        }
        Action { text: qsTr("Save Subtitles &as...") + "\tCtrl+Shift+S"; icon.source: "../../assets/icons_native/save_as_toolbutton_16.png"; onTriggered: menuBarRoot.saveSubtitlesAsRequested() }
        Action { text: qsTr("&Export Subtitles..."); icon.source: "../../assets/icons_native/export_menu_16.png"; onTriggered: if (dialogs) dialogs.dlgExport.open() }
        NativeMenu {
            title: qsTr("Recent &Subtitles")
            enabled: false
            Action { text: qsTr("No recent files"); enabled: false }
        }
        NativeMenuSep {}
        Action { text: qsTr("&Properties..."); icon.source: "../../assets/icons_native/properties_toolbutton_16.png"; onTriggered: if (dialogs) dialogs.dlgProperties.open() }
        Action { text: qsTr("&Attachments..."); icon.source: "../../assets/icons_native/attach_button_16.png"; onTriggered: if (dialogs) dialogs.dlgAttachments.open() }
        Action { text: qsTr("&Fonts Collector..."); icon.source: "../../assets/icons_native/font_collector_button_16.png"; onTriggered: if (dialogs) dialogs.dlgFontsCollector.open() }
        NativeMenuSep {}
        Action { text: qsTr("New &Window"); icon.source: "../../assets/icons_native/new_window_menu_16.png"; onTriggered: menuBarRoot.statusMessage("新建窗口") }
        Action { text: qsTr("E&xit") + "\tCtrl+Q"; onTriggered: menuBarRoot.exitRequested() }
    }

    // Edit menu
    NativeMenu {
        id: editMenu
        title: qsTr("&Edit")
        Action {
            text: (!menuBarRoot.project || !menuBarRoot.project.canUndo) ?
                  (qsTr("Nothing to &undo") + "\tCtrl+Z") :
                  (qsTr("&Undo %s").indexOf("%s") >= 0 ?
                   qsTr("&Undo %s").replace("%s", menuBarRoot.project.undoDescription) + "\tCtrl+Z" :
                   qsTr("&Undo") + "\tCtrl+Z")
            icon.source: "../../assets/icons_native/undo_button_16.png"
            enabled: !!(menuBarRoot.project && menuBarRoot.project.canUndo)
            onTriggered: if (menuBarRoot.project) menuBarRoot.project.undo()
        }
        Action {
            text: (!menuBarRoot.project || !menuBarRoot.project.canRedo) ?
                  (qsTr("Nothing to &redo") + "\tCtrl+Y") :
                  (qsTr("&Redo %s").indexOf("%s") >= 0 ?
                   qsTr("&Redo %s").replace("%s", menuBarRoot.project.redoDescription) + "\tCtrl+Y" :
                   qsTr("&Redo") + "\tCtrl+Y")
            icon.source: "../../assets/icons_native/redo_button_16.png"
            enabled: !!(menuBarRoot.project && menuBarRoot.project.canRedo)
            onTriggered: if (menuBarRoot.project) menuBarRoot.project.redo()
        }
        NativeMenuSep {}
        Action {
            text: qsTr("Cu&t Lines") + "\tCtrl+X"
            icon.source: "../../assets/icons_native/cut_button_16.png"
            enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
            onTriggered: menuBarRoot.project.cutSelectedLines()
        }
        Action {
            text: qsTr("&Copy Lines") + "\tCtrl+C"
            icon.source: "../../assets/icons_native/copy_button_16.png"
            enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
            onTriggered: menuBarRoot.project.copySelectedLines()
        }
        Action { text: qsTr("&Paste Lines") + "\tCtrl+V"; icon.source: "../../assets/icons_native/paste_button_16.png"; onTriggered: menuBarRoot.project.pasteLines(false) }
        Action {
            text: qsTr("Paste &Over...") + "\tCtrl+Shift+V"
            enabled: !!(menuBarRoot.project && menuBarRoot.project.selectedIndices.length > 0)
            onTriggered: if (dialogs) dialogs.dlgPasteOver.open()
        }
        NativeMenuSep {}
        Action { text: qsTr("&Find...") + "\tCtrl+F"; icon.source: "../../assets/icons_native/find_button_16.png"; onTriggered: if (dialogs) { dialogs.dlgSearchReplace.isReplaceMode = false; dialogs.dlgSearchReplace.open(); } }
        Action { text: qsTr("Find &Next") + "\tF3"; icon.source: "../../assets/icons_native/find_next_menu_16.png"; onTriggered: menuBarRoot.statusMessage("查找下一个") }
        Action { text: qsTr("&Replace...") + "\tCtrl+H"; icon.source: "../../assets/icons_native/find_replace_menu_16.png"; onTriggered: if (dialogs) { dialogs.dlgSearchReplace.isReplaceMode = true; dialogs.dlgSearchReplace.open(); } }
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
            onTriggered: menuBarRoot.statusMessage("按卡拉OK音节分割行")
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
        Action { text: qsTr("Select &All") + "\tCtrl+A"; onTriggered: menuBarRoot.project.selectAllRows() }
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
        Action {
            text: qsTr("&Close Video"); icon.source: "../../assets/icons_native/close_video_menu_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: {
                if (menuBarRoot.videoCtrl) menuBarRoot.videoCtrl.closeVideo();
                menuBarRoot.statusMessage("已关闭视频");
            }
        }
        NativeMenu {
            title: qsTr("Recent &Videos")
            enabled: false
            Action { text: qsTr("No recent files"); enabled: false }
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
                menuBarRoot.statusMessage(qsTr("已关闭时间码文件"));
            }
        }
        NativeMenu {
            title: qsTr("Recent &Timecodes")
            enabled: false
            Action { text: qsTr("No recent files"); enabled: false }
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
                menuBarRoot.statusMessage(qsTr("已关闭关键帧"));
            }
        }
        NativeMenu {
            title: qsTr("Recent &Keyframes")
            enabled: false
            Action { text: qsTr("No recent files"); enabled: false }
        }
        NativeMenuSep {}
        Action {
            text: qsTr("Detach &Video"); icon.source: "../../assets/icons_native/detach_video_menu_16.png"
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: menuBarRoot.statusMessage("拆分视频窗口")
        }
        NativeMenu {
            title: qsTr("Set &Zoom")
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            Action { text: "&50%"; onTriggered: menuBarRoot.statusMessage("视频缩放 50%") }
            Action { text: "&100%"; onTriggered: menuBarRoot.statusMessage("视频缩放 100%") }
            Action { text: "&200%"; onTriggered: menuBarRoot.statusMessage("视频缩放 200%") }
        }
        NativeMenu {
            title: qsTr("Override &Aspect Ratio")
            enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            Action { text: qsTr("&Default"); onTriggered: menuBarRoot.statusMessage("宽高比: 默认") }
            Action { text: qsTr("&Fullscreen (4:3)"); onTriggered: menuBarRoot.statusMessage("宽高比: 4:3") }
            Action { text: qsTr("&Widescreen (16:9)"); onTriggered: menuBarRoot.statusMessage("宽高比: 16:9") }
            Action { text: qsTr("&Cinematic (2.35)"); onTriggered: menuBarRoot.statusMessage("宽高比: 2.35:1") }
            Action { text: qsTr("C&ustom..."); onTriggered: menuBarRoot.statusMessage("自定义宽高比...") }
        }
        Action {
            text: qsTr("Show &Overscan Mask"); enabled: !!(menuBarRoot.videoCtrl && menuBarRoot.videoCtrl.hasVideo)
            onTriggered: menuBarRoot.statusMessage("切换过扫描遮罩")
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
                        menuBarRoot.statusMessage(qsTr("已从当前视频提取音频并加载"));
                    } else {
                        menuBarRoot.statusMessage(qsTr("提取视频音频失败"));
                    }
                }
            }
        }
        Action {
            text: qsTr("&Close Audio"); icon.source: "../../assets/icons_native/close_audio_menu_16.png"
            enabled: !!(menuBarRoot.audioCtrl && menuBarRoot.audioCtrl.hasAudio)
            onTriggered: {
                if (menuBarRoot.audioCtrl) menuBarRoot.audioCtrl.closeAudio();
                menuBarRoot.statusMessage("已关闭音频");
            }
        }
        NativeMenu {
            title: qsTr("Recent &Audio")
            enabled: false
            Action { text: qsTr("No recent files"); enabled: false }
        }
        NativeMenuSep {}
        Action {
            text: qsTr("&Spectrum Display"); enabled: !!(menuBarRoot.audioCtrl && menuBarRoot.audioCtrl.hasAudio)
            onTriggered: {
                if (menuBarRoot.audioCtrl) {
                    menuBarRoot.audioCtrl.waveformMode = false;
                    menuBarRoot.statusMessage("音频视图切换为: 频谱");
                }
            }
        }
        Action {
            text: qsTr("&Waveform Display"); enabled: !!(menuBarRoot.audioCtrl && menuBarRoot.audioCtrl.hasAudio)
            onTriggered: {
                if (menuBarRoot.audioCtrl) {
                    menuBarRoot.audioCtrl.waveformMode = true;
                    menuBarRoot.statusMessage("音频视图切换为: 波形");
                }
            }
        }
        Action { text: qsTr("Open 2h30 Blank Audio"); onTriggered: menuBarRoot.statusMessage("已加载空白音频") }
        Action { text: qsTr("Open 2h30 Noise Audio"); onTriggered: menuBarRoot.statusMessage("已加载噪声音频") }
    }

    // Automation menu
    NativeMenu {
        id: autoMenu
        title: qsTr("A&utomation")
        Action { text: qsTr("&Automation..."); icon.source: "../../assets/icons_native/automation_toolbutton_16.png"; onTriggered: if (dialogs) dialogs.dlgAutomation.open() }
        Action {
            text: qsTr("&Reload Automation Scripts"); onTriggered: {
                if (typeof automationManager !== "undefined") automationManager.reloadAll();
                menuBarRoot.statusMessage("已重新载入所有自动化脚本");
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
        Action { text: qsTr("&Options..."); icon.source: "../../assets/icons_native/options_button_16.png"; onTriggered: if (dialogs) dialogs.dlgPreferences.open() }
        NativeMenuSep {}
        Action { text: qsTr("S&ubs Only View"); onTriggered: menuBarRoot.statusMessage("切换视图: 仅字幕") }
        Action { text: qsTr("&Video+Subs View"); onTriggered: menuBarRoot.statusMessage("切换视图: 视频+字幕") }
        Action { text: qsTr("&Audio+Subs View"); onTriggered: menuBarRoot.statusMessage("切换视图: 音频+字幕") }
        Action { text: qsTr("&Full view"); onTriggered: menuBarRoot.statusMessage("切换视图: 完全模式") }
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
            onTriggered: menuBarRoot.statusMessage("开启和关闭主工具栏")
        }
    }

    // Help menu
    NativeMenu {
        title: qsTr("&Help")
        Action { text: qsTr("&Contents") + "\tF1"; icon.source: "../../assets/icons_native/contents_button_16.png"; onTriggered: Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/") }
        NativeMenuSep {}
        Action { text: qsTr("&Website"); icon.source: "../../assets/icons_native/website_button_16.png"; onTriggered: Qt.openUrlExternally("http://www.aegisub.org/") }
        Action { text: qsTr("&Bug Tracker..."); icon.source: "../../assets/icons_native/bugtracker_button_16.png"; onTriggered: Qt.openUrlExternally("https://github.com/Aegisub/Aegisub/issues") }
        NativeMenuSep {}
        Action { text: qsTr("&IRC Channel..."); icon.source: "../../assets/icons_native/irc_button_16.png"; onTriggered: menuBarRoot.statusMessage("IRC: #aegisub on irc.rizon.net") }
        Action { text: qsTr("&Check for Updates..."); onTriggered: menuBarRoot.statusMessage("当前已是最新版本") }
        Action { text: qsTr("&About Aegisub..."); icon.source: "../../assets/icons_native/about_menu_16.png"; onTriggered: if (dialogs) dialogs.dlgAbout.open() }
        Action { text: qsTr("&Log Window..."); icon.source: "../../assets/icons_native/about_menu_16.png"; onTriggered: menuBarRoot.statusMessage("打开日志窗口...") }
    }
}
