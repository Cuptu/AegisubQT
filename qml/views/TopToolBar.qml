// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Main application toolbar providing quick access to file, video, timing, style, and automation actions.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ToolBar {
    id: toolBarRoot
    implicitHeight: 26

    required property var project
    property var dialogs: null
    property var audioCtrl: null
    property var videoCtrl: null
    property var videoDisplayCtrl: null

    signal statusMessage(string text)
    signal statusHelp(string text)
    signal newSubtitlesRequested()
    signal cycleTagHidingRequested()
    signal jumpToLineStartRequested()
    signal jumpToLineEndRequested()
    signal openSubtitlesRequested()
    signal saveSubtitlesRequested()

    background: Rectangle {
        color: "#f0f0f0"
        border.color: "#d0d0d0"
        border.width: 1
        antialiasing: false
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 2
        anchors.rightMargin: 4
        spacing: 1

        component ClassicToolBtn: ToolButton {
            id: tb
            property string iconPath: ""
            property string tipText: ""
            implicitWidth: 22
            implicitHeight: 22
            padding: 1
            opacity: tb.enabled ? 1.0 : 0.35

            contentItem: Image {
                source: tb.iconPath
                width: 16
                height: 16
                fillMode: Image.Pad
                smooth: false
                anchors.centerIn: parent
            }

            background: Rectangle {
                radius: 0
                color: tb.pressed ? "#0000001a" : (tb.hovered ? "#0000000f" : "transparent")
                border.color: "transparent"
            }

            onHoveredChanged: {
                if (hovered) toolBarRoot.statusHelp(tipText);
                else toolBarRoot.statusHelp("");
            }

            ToolTip.visible: hovered && tipText !== ""
            ToolTip.text: tipText
            ToolTip.delay: 700
        }

        component Win32Separator: Item {
            implicitWidth: 7
            implicitHeight: 18
            Rectangle {
                anchors.centerIn: parent
                width: 1
                height: 14
                color: "#d0d0d0"
            }
        }

        // Group 1: File operations (new, open, save)
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/new_toolbutton_16.png"
            tipText: qsTr("New subtitles") + " (Ctrl-N)"
            onClicked: toolBarRoot.newSubtitlesRequested()
        }
        ClassicToolBtn {
            id: btnOpen
            iconPath: "../../assets/icons_native/open_toolbutton_16.png"
            tipText: qsTr("Open a subtitles file") + " (Ctrl-O)"
            onClicked: toolBarRoot.openSubtitlesRequested()
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/save_toolbutton_16.png"
            tipText: qsTr("Save the current subtitles") + " (Ctrl-S)"
            enabled: !!(toolBarRoot.project && toolBarRoot.project.isModified)
            onClicked: toolBarRoot.saveSubtitlesRequested()
        }


        Win32Separator {}

        // Group 2: Video navigation and zoom controls
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/jumpto_button_16.png"
            tipText: qsTr("Jump to frame or time") + " (Ctrl-G)"
            enabled: !!(toolBarRoot.videoCtrl && toolBarRoot.videoCtrl.hasVideo)
            onClicked: if (dialogs) dialogs.dlgJumpTo.open()
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/zoom_in_button_16.png"
            tipText: qsTr("Zoom video in") + " (Ctrl-KP_Add)"
            enabled: !!(toolBarRoot.videoCtrl && toolBarRoot.videoCtrl.hasVideo)
            onClicked: {
                if (toolBarRoot.videoDisplayCtrl) toolBarRoot.videoDisplayCtrl.zoomIn();
                else toolBarRoot.statusMessage(qsTr("Zoom video in"));
            }
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/zoom_out_button_16.png"
            tipText: qsTr("Zoom video out") + " (Ctrl-KP_Subtract)"
            enabled: !!(toolBarRoot.videoCtrl && toolBarRoot.videoCtrl.hasVideo)
            onClicked: {
                if (toolBarRoot.videoDisplayCtrl) toolBarRoot.videoDisplayCtrl.zoomOut();
                else toolBarRoot.statusMessage(qsTr("Zoom video out"));
            }
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/video_to_substart_16.png"
            tipText: qsTr("Jump the video to the start frame of current subtitle") + " (Ctrl-1)"
            enabled: !!(toolBarRoot.videoCtrl && toolBarRoot.videoCtrl.hasVideo)
            onClicked: toolBarRoot.jumpToLineStartRequested()
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/video_to_subend_16.png"
            tipText: qsTr("Jump the video to the end frame of current subtitle") + " (Ctrl-2)"
            enabled: !!(toolBarRoot.videoCtrl && toolBarRoot.videoCtrl.hasVideo)
            onClicked: toolBarRoot.jumpToLineEndRequested()
        }

        Win32Separator {}

        // Group 3: Timing synchronization and selection tools
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/substart_to_video_16.png"
            tipText: qsTr("Set start of selected subtitles to current video frame") + " (Ctrl-3)"
            enabled: !!(toolBarRoot.videoCtrl && toolBarRoot.videoCtrl.hasVideo)
            onClicked: {
                if (toolBarRoot.project && toolBarRoot.videoCtrl) {
                    toolBarRoot.project.snapStartTimeToVideo(toolBarRoot.videoCtrl.currentTime);
                }
            }
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/subend_to_video_16.png"
            tipText: qsTr("Set end of selected subtitles to current video frame") + " (Ctrl-4)"
            enabled: !!(toolBarRoot.videoCtrl && toolBarRoot.videoCtrl.hasVideo)
            onClicked: {
                if (toolBarRoot.project && toolBarRoot.videoCtrl) {
                    toolBarRoot.project.snapEndTimeToVideo(toolBarRoot.videoCtrl.currentTime);
                }
            }
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/select_visible_button_16.png"
            tipText: qsTr("Select all dialogue lines that are visible on the current video frame")
            enabled: !!(toolBarRoot.videoCtrl && toolBarRoot.videoCtrl.hasVideo)
            onClicked: {
                if (toolBarRoot.project && toolBarRoot.videoCtrl) {
                    toolBarRoot.project.selectVisibleLines(toolBarRoot.videoCtrl.currentTime);
                }
            }
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/snap_subs_to_scene_16.png"
            tipText: qsTr("Set start and end of subtitles to the keyframes around current video frame") + " (Ctrl-5)"
            enabled: !!(toolBarRoot.videoCtrl && toolBarRoot.videoCtrl.hasVideo)
            onClicked: {
                if (toolBarRoot.project) {
                    toolBarRoot.project.snapToKeyframes(toolBarRoot.project.snapPoints);
                }
            }
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/shift_to_frame_16.png"
            tipText: qsTr("Shift selection so that the active line starts at current frame") + " (Ctrl-6)"
            enabled: !!(toolBarRoot.videoCtrl && toolBarRoot.videoCtrl.hasVideo)
            onClicked: {
                if (toolBarRoot.project && toolBarRoot.videoCtrl) {
                    toolBarRoot.project.shiftToCurrentFrame(toolBarRoot.videoCtrl.currentTime);
                }
            }
        }


        Win32Separator {}

        // Group 4: Project managers (styles, properties, attachments, fonts)
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/style_toolbutton_16.png"
            tipText: qsTr("Open the styles manager")
            onClicked: if (dialogs) dialogs.dlgStyleManager.open()
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/properties_toolbutton_16.png"
            tipText: qsTr("Open script properties window")
            onClicked: if (dialogs) dialogs.dlgProperties.open()
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/attach_button_16.png"
            tipText: qsTr("Open the attachment manager dialog")
            onClicked: if (dialogs) dialogs.dlgAttachments.open()
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/font_collector_button_16.png"
            tipText: qsTr("Open fonts collector")
            onClicked: if (dialogs) dialogs.dlgFontsCollector.open()
        }

        Win32Separator {}

        // Group 5: Automation manager
        ClassicToolBtn {
            id: btnAutomation
            iconPath: "../../assets/icons_native/automation_toolbutton_16.png"
            tipText: qsTr("Open automation manager. Ctrl: Rescan autoload folder. Ctrl+Shift: Rescan autoload folder and reload all automation scripts")
            onClicked: if (dialogs) dialogs.dlgAutomation.open()
        }

        Win32Separator {}

        // Group 6: Timing and subtitle processing tools
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/shift_times_toolbutton_16.png"
            tipText: qsTr("Shift subtitles by time or frames") + " (Ctrl-I)"
            onClicked: if (dialogs) dialogs.dlgShiftTimes.open()
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/styling_toolbutton_16.png"
            tipText: qsTr("Open styling assistant")
            onClicked: if (dialogs) dialogs.dlgStylingAssistant.open()
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/translation_toolbutton_16.png"
            tipText: qsTr("Open translation assistant")
            onClicked: if (dialogs) dialogs.dlgTranslation.open()
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/resample_toolbutton_16.png"
            tipText: qsTr("Resample subtitles to maintain their current appearance at a different script resolution")
            onClicked: if (dialogs) dialogs.dlgResample.open()
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/timing_processor_toolbutton_16.png"
            tipText: qsTr("Post-process the subtitle timing to add lead-ins and lead-outs, snap timing to scene changes, etc.")
            onClicked: if (dialogs) dialogs.dlgTimingProcessor.open()
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/kara_timing_copier_16.png"
            tipText: qsTr("Open the Kanji timer copier")
            onClicked: if (dialogs) dialogs.dlgKanjiTimer.open()
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/spellcheck_toolbutton_16.png"
            tipText: qsTr("Open spell checker")
            onClicked: if (dialogs) dialogs.dlgSpellChecker.open()
        }

        Win32Separator {}

        // Group 7: Preferences and view display toggles
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/options_button_16.png"
            tipText: qsTr("Configure Aegisub") + " (Alt-O)"
            onClicked: if (dialogs) dialogs.dlgPreferences.open()
        }
        ClassicToolBtn {
            iconPath: "../../assets/icons_native/toggle_tag_hiding_16.png"
            tipText: qsTr("Cycle through tag hiding modes")
            onClicked: toolBarRoot.cycleTagHidingRequested()
        }

        Item { Layout.fillWidth: true }
    }
}
