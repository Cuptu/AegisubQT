// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Aegisub 1.0
import "../controls"
import "../project/AssUtils.js" as AssUtils

// Audio display and timing management box with waveform/spectrogram rendering,
// scrubbing controls, zoom/volume trackbars, karaoke syllable timing, and transport actions.
Item {
    id: audioBox
    implicitHeight: 200
    Layout.fillWidth: true

    property var project: null
    signal statusMessage(string text)

    property color winBg: "#f0f0f0"
    property color winBorder: "#bebebe"
    property color winSunkenBorder: "#ababab"
    property var controllerInstance: audioController

    // Coordinate projection helpers: time (ms) <-> viewport pixel space
    function timeToPx(ms) {
        if (!audioController || audioController.msPerPixel <= 0) return 0;
        return (ms / audioController.msPerPixel) - audioController.scrollLeft;
    }

    function pxToTime(px) {
        if (!audioController) return 0;
        return (audioController.scrollLeft + px) * audioController.msPerPixel;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 2

        // Upper section: audio display viewport (ruler, waveform/spectrogram, scrollbar) and right-side trackbars
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 2

            // Audio waveform and time ruler main display area
            Rectangle {
                id: audioViewContainer
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#000000"
                clip: true

                // Sunken border bevel
                Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: "#707070"; z: 20 }
                Rectangle { anchors.left: parent.left; height: parent.height; width: 1; color: "#707070"; z: 20 }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#ffffff"; z: 20 }
                Rectangle { anchors.right: parent.right; height: parent.height; width: 1; color: "#ffffff"; z: 20 }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 1
                    spacing: 0

                    // Spectrogram / waveform viewport (integrated C++ hardware-accelerated rendering)
                    Rectangle {
                        id: waveViewport
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#08040d"
                        clip: true

                        // C++ SpectrogramView: renders time ruler, audio samples, selection bounds, keyframes, and play cursor
                        SpectrogramView {
                            id: specView
                            anchors.fill: parent
                            audioController: audioBox.controllerInstance
                        }
                    }

                    // Horizontal scrollbar with selection marker overlay
                    Rectangle {
                        id: scrollBarItem
                        Layout.fillWidth: true
                        implicitHeight: 15
                        color: "#08040d"
                        border.color: audioInteractiveArea.activeFocus ? "#cdf0e2" : "#5991dc"
                        border.width: 1

                        property real totalPx: audioController ? Math.max(1.0, (audioController.duration * 1000.0) / audioController.msPerPixel) : 1000
                        property real viewPx: waveViewport.width > 0 ? waveViewport.width : 800
                        property real thumbW: Math.max(10, Math.min(width, width * (viewPx / totalPx)))
                        property real thumbX: Math.max(0, Math.min(width - thumbW, width * (audioController ? (audioController.scrollLeft / totalPx) : 0)))

                        // Selection span marker in scrollbar trough
                        Rectangle {
                            property real selStartMs: audioController ? audioController.selectionStart : 0
                            property real selEndMs: audioController ? audioController.selectionEnd : 0
                            property real durMs: audioController ? Math.max(1.0, audioController.duration * 1000.0) : 1
                            x: Math.max(0, scrollBarItem.width * (selStartMs / durMs))
                            width: Math.max(2, scrollBarItem.width * ((selEndMs - selStartMs) / durMs))
                            height: parent.height
                            y: 0
                            color: audioInteractiveArea.activeFocus ? "#526bd5" : "#412267"
                        }

                        // Scrollbar thumb indicator
                        Rectangle {
                            id: thumbRect
                            x: scrollBarItem.thumbX
                            y: 0
                            width: scrollBarItem.thumbW
                            height: parent.height
                            radius: 0
                            color: audioInteractiveArea.activeFocus ? "#cdf0e2" : "#5991dc"
                        }
                    }
                }

                // Input capture surface: forwards mouse, wheel, and key events
                // to AudioDisplayController where hit-testing and dragging state machines reside.
                // Origin corresponds to top-left of viewport inside the 1px sunken border.
                MouseArea {
                    id: audioInteractiveArea
                    anchors.fill: parent
                    anchors.margins: 1
                    hoverEnabled: true
                    preventStealing: true
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton

                    cursorShape: displayController.horizontalCursor ? Qt.SizeHorCursor : Qt.ArrowCursor

                    onPressed: (mouse) => {
                        forceActiveFocus();
                        displayController.mousePressed(mouse.x, mouse.y, mouse.button, mouse.modifiers);
                    }
                    onPositionChanged: (mouse) => {
                        displayController.mouseMoved(mouse.x, mouse.y, mouse.buttons);
                    }
                    onReleased: (mouse) => {
                        displayController.mouseReleased(mouse.x, mouse.y, mouse.button, mouse.modifiers);
                    }
                    onDoubleClicked: (mouse) => {
                        displayController.mouseDoubleClicked(mouse.x, mouse.y, mouse.button);
                    }
                    onExited: {
                        displayController.mouseLeft();
                    }
                    onWheel: (wheel) => {
                        displayController.wheel(wheel.angleDelta.x, wheel.angleDelta.y, wheel.modifiers);
                    }

                    // Forward key events to audio controller for hotkey command dispatch
                    Keys.onPressed: (event) => {
                        if (displayController.keyPressed(event.key, event.modifiers))
                            event.accepted = true;
                    }

                    onWidthChanged: displayController.setViewportSize(width, height)
                    onHeightChanged: displayController.setViewportSize(width, height)
                    Component.onCompleted: displayController.setViewportSize(width, height)
                }
            }

            // Right vertical controls (80px width): horizontal zoom, vertical amplitude, and playback volume
            Rectangle {
                Layout.fillHeight: true
                implicitWidth: 80
                color: audioBox.winBg

                // Slider 1: Horizontal zoom (-50 to +30 scale)
                Item {
                    id: hZoomItem
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 28

                    // Trackbar groove
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 8
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 8
                        width: 4
                        color: "#e7eaea"
                        border.color: "#d6d6d6"
                        border.width: 1
                    }

                    // Zoom thumb: maps vertical position linearly to [-50, +30]
                    Rectangle {
                        id: hZoomHandle
                        width: 21
                        height: 10
                        anchors.horizontalCenter: parent.horizontalCenter
                        // fracFromTop: (50 - zoomLevel) / 80.0
                        property real fracFromTop: audioController ? ((50.0 - audioController.zoomLevel) / 80.0) : 0.625
                        y: Math.max(3, Math.min(parent.height - height - 3, 3 + fracFromTop * (parent.height - height - 6)))
                        color: "#007ad9"
                        border.color: "#005bb5"
                        border.width: 1
                        radius: 0

                        MouseArea {
                            id: hZoomMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            ToolTip.visible: containsMouse
                            ToolTip.text: qsTr("Horizontal zoom")
                            ToolTip.delay: 700
                            drag.target: parent
                            drag.axis: Drag.YAxis
                            drag.minimumY: 3
                            drag.maximumY: hZoomItem.height - parent.height - 3
                            onPositionChanged: {
                                if (pressed && audioController) {
                                    var range = drag.maximumY - drag.minimumY;
                                    if (range > 0) {
                                        var frac = (parent.y - drag.minimumY) / range;
                                        var lvl = Math.round(50.0 - frac * 80.0);
                                        audioController.setZoomLevel(lvl);
                                    }
                                }
                            }
                        }
                    }
                }

                // Right slider column: vertical amplitude zoom and playback volume
                Item {
                    anchors.left: hZoomItem.right
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom

                    // Parallel amplitude and volume sliders
                    Row {
                        anchors.top: parent.top
                        anchors.bottom: linkBtn.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottomMargin: 4
                        spacing: 0

                        // Slider 2: Vertical waveform / spectrogram amplitude scale
                        Item {
                            id: vZoomItem
                            width: 24
                            height: parent.height

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: parent.top
                                anchors.topMargin: 8
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 8
                                width: 4
                                color: "#e7eaea"
                                border.color: "#d6d6d6"
                                border.width: 1
                            }

                            Rectangle {
                                id: vZoomHandle
                                width: 21
                                height: 10
                                anchors.horizontalCenter: parent.horizontalCenter
                                property real fracFromTop: audioController ? (1.0 - (audioController.verticalZoom / 100.0)) : 0.5
                                y: Math.max(3, Math.min(parent.height - height - 3, 3 + fracFromTop * (parent.height - height - 6)))
                                color: "#007ad9"
                                border.color: "#005bb5"
                                border.width: 1
                                radius: 0

                                MouseArea {
                                    id: vZoomMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    ToolTip.visible: containsMouse
                                    ToolTip.text: qsTr("Vertical zoom")
                                    ToolTip.delay: 700
                                    drag.target: parent
                                    drag.axis: Drag.YAxis
                                    drag.minimumY: 3
                                    drag.maximumY: vZoomItem.height - parent.height - 3
                                    onPositionChanged: {
                                        if (pressed && audioController) {
                                            var range = drag.maximumY - drag.minimumY;
                                            if (range > 0) {
                                                var frac = 1.0 - (parent.y - drag.minimumY) / range;
                                                audioController.setVerticalZoom(Math.round(frac * 100));
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Slider 3: Audio playback volume (disabled when linked with zoom)
                        Item {
                            id: volItem
                            width: 24
                            height: parent.height

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: parent.top
                                anchors.topMargin: 8
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 8
                                width: 4
                                color: "#e7eaea"
                                border.color: "#d6d6d6"
                                border.width: 1
                            }

                            readonly property bool isLinked: audioController ? audioController.linkVolumeAndZoom : false

                            Rectangle {
                                id: volHandle
                                width: 21
                                height: 10
                                anchors.horizontalCenter: parent.horizontalCenter
                                property real fracFromTop: audioController ? (1.0 - (audioController.volume / 100.0)) : 0.0
                                y: Math.max(3, Math.min(parent.height - height - 3, 3 + fracFromTop * (parent.height - height - 6)))
                                color: volItem.isLinked ? "#cccccc" : "#007ad9"
                                border.color: volItem.isLinked ? "#aaaaaa" : "#005bb5"
                                border.width: 1
                                radius: 0

                                MouseArea {
                                    id: volMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    ToolTip.visible: containsMouse
                                    ToolTip.text: qsTr("Audio Volume")
                                    ToolTip.delay: 700
                                    enabled: !volItem.isLinked
                                    drag.target: parent
                                    drag.axis: Drag.YAxis
                                    drag.minimumY: 3
                                    drag.maximumY: volItem.height - parent.height - 3
                                    onPositionChanged: {
                                        if (pressed && audioController) {
                                            var range = drag.maximumY - drag.minimumY;
                                            if (range > 0) {
                                                var frac = 1.0 - (parent.y - drag.minimumY) / range;
                                                audioController.setVolume(Math.round(frac * 100));
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Lock toggle: links volume and vertical zoom adjustments
                    Rectangle {
                        id: linkBtn
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 4
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 48
                        height: 16
                        color: audioController && audioController.linkVolumeAndZoom ? "#7fff7f" : "#a0e0a0"
                        radius: 0
                        border.color: "#70b070"
                        border.width: 1

                        // Link icon indicator
                        Rectangle {
                            anchors.centerIn: parent
                            width: 14
                            height: 7
                            color: "transparent"
                            border.color: "#0090e0"
                            border.width: 1.5
                            radius: 0
                        }

                        MouseArea {
                            id: linkMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            ToolTip.visible: containsMouse
                            ToolTip.text: qsTr("Link vertical zoom and volume sliders")
                            ToolTip.delay: 700
                            onClicked: {
                                if (audioController) audioController.setLinkVolumeAndZoom(!audioController.linkVolumeAndZoom);
                            }
                        }
                    }
                }
            }
        }

        // Karaoke syllable timing toolbar
        Rectangle {
            id: karaokeBar
            Layout.fillWidth: true
            implicitHeight: (audioController && audioController.karaokeMode) ? 32 : 0
            visible: implicitHeight > 0
            color: "#f7f9fa"
            border.color: "#d0d0d0"
            border.width: 1
            clip: true

            property var syllables: []

            function parseCurrentLine() {
                if (!audioBox.project || !audioBox.project.subtitleModel || audioBox.project.currentSelectedIndex >= audioBox.project.subtitleModel.count || audioBox.project.currentSelectedIndex < 0) return;
                var item = audioBox.project.subtitleModel.get(audioBox.project.currentSelectedIndex);
                if (!item) return;
                var txt = item.text;
                var kMatches = txt.match(/\{\\k[0-9]+\}[^{]*/g);
                var res = [];
                if (kMatches && kMatches.length > 0) {
                    for (var i = 0; i < kMatches.length; i++) {
                        var m = kMatches[i];
                        var durMatch = m.match(/\{\\k([0-9]+)\}(.*)/);
                        if (durMatch) {
                            res.push({ dur: parseInt(durMatch[1]), text: durMatch[2] });
                        }
                    }
                } else {
                    var words = txt.replace(/\{[^}]*\}/g, "").split("");
                    var totalDurationCs = Math.max(10, Math.round((AssUtils.assToMs(item.end) - AssUtils.assToMs(item.start)) / 10.0));
                    var perWord = Math.max(1, Math.floor(totalDurationCs / Math.max(1, words.length)));
                    for (var j = 0; j < words.length; j++) {
                        if (words[j].trim() === "") continue;
                        res.push({ dur: perWord, text: words[j] });
                    }
                }
                syllables = res;
            }

            Connections {
                target: audioController ? audioController : null
                function onKaraokeModeChanged() {
                    if (audioController && audioController.karaokeMode) {
                        karaokeBar.parseCurrentLine();
                    }
                }
            }

            Connections {
                target: audioBox.project ? audioBox.project : null
                function onLineSelected(idx, item) {
                    if (audioController && audioController.karaokeMode) {
                        karaokeBar.parseCurrentLine();
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 6

                Text {
                    text: qsTr("Karaoke syllables:"); font.pixelSize: 12
                    font.bold: true
                    font.family: uiTheme.uiFont
                    renderType: Text.NativeRendering
                    color: "#0066cc"
                    Layout.alignment: Qt.AlignVCenter
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    Row {
                        spacing: 4
                        anchors.verticalCenter: parent.verticalCenter

                        Repeater {
                            model: karaokeBar.syllables
                            delegate: Rectangle {
                                height: 22
                                width: Math.max(36, sylText.width + 16)
                                radius: 0
                                color: sylMouse.containsMouse ? "#cce8ff" : "#ffffff"
                                border.color: sylMouse.containsMouse ? "#0078d4" : "#b0b0b0"
                                border.width: 1

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 0
                                    Text {
                                        id: sylText
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.text
                                        font.pixelSize: 12
                                        font.bold: true
                                        font.family: "Verdana, Segoe UI, Microsoft YaHei, sans-serif"
                                        renderType: Text.NativeRendering
                                        color: "#1e1e1e"
                                    }
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.dur + "cs"
                                        font.pixelSize: 9
                                        font.family: uiTheme.monoFont
                                        renderType: Text.NativeRendering
                                        color: "#888888"
                                    }
                                }

                                MouseArea {
                                    id: sylMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Syllable: ") + modelData.text + " (" + (modelData.dur * 10) + "ms)"
                                    ToolTip.delay: 700
                                }
                            }
                        }
                    }
                }

                Button {
                    text: qsTr("Apply \\k tags"); implicitHeight: 20
                    padding: 4
                    background: Rectangle {
                        radius: 0
                        color: parent.pressed ? "#005a9e" : (parent.hovered ? "#0078d4" : "#0066cc")
                    }
                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        renderType: Text.NativeRendering
                        color: "#ffffff"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        var kText = "";
                        for (var i = 0; i < karaokeBar.syllables.length; i++) {
                            var s = karaokeBar.syllables[i];
                            kText += "{\\k" + s.dur + "}" + s.text;
                        }
                        if (audioBox.project && audioBox.project.subtitleModel && audioBox.project.currentSelectedIndex >= 0) {
                            audioBox.project.subtitleModel.setProperty(audioBox.project.currentSelectedIndex, "text", kText);
                            audioBox.project.dataModified();
                            audioBox.statusMessage(qsTr("Created karaoke %1 tags").arg("\\k"));
                        }
                    }
                }
            }
        }

        // Transport toolbar: navigation, playback preview, lead-in/out, commit, and display toggles
        RowLayout {
            Layout.fillWidth: true
            implicitHeight: 22
            spacing: 1

            component AudioBtn: ToolButton {
                id: aBtn
                property string iconSrc: ""
                property string tipText: ""
                property bool isToggle: false
                implicitWidth: 20
                implicitHeight: 20

                contentItem: Image {
                    anchors.centerIn: parent
                    source: aBtn.iconSrc
                    width: 16
                    height: 16
                    fillMode: Image.Pad
                    smooth: false
                }

                background: Rectangle {
                    radius: 0
                    color: aBtn.checked ? "#cce8ff" : (aBtn.pressed ? "#0000001a" : (aBtn.hovered ? "#0000000f" : "transparent"))
                    border.color: aBtn.checked ? "#99d1ff" : "transparent"
                    border.width: 1
                }

                ToolTip.visible: hovered
                ToolTip.text: tipText
                ToolTip.delay: 700
            }

            // Group 1: Navigation and playback
            AudioBtn {
                iconSrc: "../../assets/icons_native/button_prev_16.png"
                tipText: qsTr("Previous line or syllable") + " (Z / Left)"
                onClicked: {
                    if (audioBox.project && audioBox.project.currentSelectedIndex > 0) {
                        audioBox.project.selectRow(audioBox.project.currentSelectedIndex - 1, false, false);
                    }
                }
            }
            AudioBtn {
                iconSrc: "../../assets/icons_native/button_next_16.png"
                tipText: qsTr("Next line or syllable") + " (X / Right)"
                onClicked: {
                    if (audioBox.project && audioBox.project.subtitleModel && audioBox.project.currentSelectedIndex < audioBox.project.subtitleModel.count - 1) {
                        audioBox.project.selectRow(audioBox.project.currentSelectedIndex + 1, false, false);
                    }
                }
            }
            AudioBtn {
                iconSrc: "../../assets/icons_native/button_playsel_16.png"
                tipText: qsTr("Play audio until the end of the selection is reached") + " (S / Space)"
                onClicked: { if (audioController) audioController.playSelection(); }
            }
            AudioBtn {
                iconSrc: "../../assets/icons_native/button_playline_16.png"
                tipText: qsTr("Play the audio for the current line") + " (R)"
                onClicked: { if (audioController) audioController.playCurrentLine(); }
            }
            AudioBtn {
                iconSrc: "../../assets/icons_native/button_stop_16.png"
                tipText: qsTr("Stop audio and video playback") + " (H)"
                onClicked: { if (audioController) audioController.stop(); }
            }

            // Separator
            Rectangle { width: 1; height: 16; color: "#bebebe"; Layout.margins: 1 }

            // Group 2: Boundary preview (500ms intervals) and play-to-end
            AudioBtn {
                iconSrc: "../../assets/icons_native/button_playfivehbefore_16.png"
                tipText: qsTr("Play 500 ms before selection") + " (Q)"
                onClicked: { if (audioController) audioController.play500msBefore(); }
            }
            AudioBtn {
                iconSrc: "../../assets/icons_native/button_playfivehafter_16.png"
                tipText: qsTr("Play 500 ms after selection") + " (W)"
                onClicked: { if (audioController) audioController.play500msAfter(); }
            }
            AudioBtn {
                iconSrc: "../../assets/icons_native/button_playfirstfiveh_16.png"
                tipText: qsTr("Play first 500 ms of selection") + " (E)"
                onClicked: { if (audioController) audioController.playFirst500ms(); }
            }
            AudioBtn {
                iconSrc: "../../assets/icons_native/button_playlastfiveh_16.png"
                tipText: qsTr("Play last 500 ms of selection") + " (D)"
                onClicked: { if (audioController) audioController.playLast500ms(); }
            }
            AudioBtn {
                iconSrc: "../../assets/icons_native/button_playtoend_16.png"
                tipText: qsTr("Play from selection start to end of file") + " (T)"
                onClicked: { if (audioController) audioController.playToEnd(); }
            }

            // Separator
            Rectangle { width: 1; height: 16; color: "#bebebe"; Layout.margins: 1 }

            // Group 3: Lead-in and lead-out offsets
            AudioBtn {
                iconSrc: "../../assets/icons_native/button_leadin_16.png"
                tipText: qsTr("Add the lead in time to the selected lines") + " (C)"
                onClicked: { if (audioController) audioController.leadIn(audioController.leadInMs); }
            }
            AudioBtn {
                iconSrc: "../../assets/icons_native/button_leadout_16.png"
                tipText: qsTr("Add the lead out time to the selected lines") + " (V)"
                // Default 100/350 ms originates from upstream Audio/Lead/IN and Audio/Lead/OUT preferences
                onClicked: { if (audioController) audioController.leadOut(audioController.leadOutMs); }
            }

            // Separator
            Rectangle { width: 1; height: 16; color: "#bebebe"; Layout.margins: 1 }

            // Group 4: Commit timing and scroll-to-selection
            AudioBtn {
                iconSrc: "../../assets/icons_native/button_audio_commit_16.png"
                tipText: qsTr("Commit any pending audio timing changes") + " (G / Enter)"
                // Line advancement is triggered by C++ AudioController::commit via nextLineRequested signal,
                // ensuring identical behavior between UI buttons and keyboard hotkeys without duplicate QML logic
                onClicked: { if (audioController) audioController.commit(); }
            }
            AudioBtn {
                iconSrc: "../../assets/icons_native/button_audio_goto_16.png"
                tipText: qsTr("Scroll the audio display to center on the current audio selection")
                onClicked: {
                    if (audioController) {
                        audioController.scrollRangeInView(audioController.selectionStart, audioController.selectionEnd);
                    }
                }
            }

            // Separator
            Rectangle { width: 1; height: 16; color: "#bebebe"; Layout.margins: 1 }

            // Group 5: Automatic commit, scroll, and spectrum mode toggles
            AudioBtn {
                iconSrc: "../../assets/icons_native/toggle_audio_autocommit_16.png"
                tipText: qsTr("Automatically commit all changes")
                isToggle: true
                checked: audioController ? audioController.autoCommit : false
                onClicked: { if (audioController) audioController.setAutoCommit(!audioController.autoCommit); }
            }
            AudioBtn {
                iconSrc: "../../assets/icons_native/toggle_audio_nextcommit_16.png"
                tipText: qsTr("Automatically go to next line on commit")
                isToggle: true
                checked: audioController ? audioController.autoNext : false
                onClicked: { if (audioController) audioController.setAutoNext(!audioController.autoNext); }
            }
            AudioBtn {
                iconSrc: "../../assets/icons_native/toggle_audio_autoscroll_16.png"
                tipText: qsTr("Auto scroll audio display to selected line")
                isToggle: true
                checked: audioController ? audioController.autoScroll : true
                onClicked: { if (audioController) audioController.setAutoScroll(!audioController.autoScroll); }
            }
            AudioBtn {
                iconSrc: "../../assets/icons_native/toggle_audio_spectrum_16.png"
                tipText: qsTr("Spectrum analyzer mode")
                isToggle: true
                checked: audioController ? audioController.spectrumMode : true
                onClicked: { if (audioController) audioController.toggleSpectrumMode(); }
            }
            AudioBtn {
                iconSrc: "../../assets/icons_native/toggle_audio_medusa_16.png"
                tipText: qsTr("Toggle global hotkey overrides (Medusa Mode)")
                isToggle: true
                checked: audioController ? audioController.medusaMode : false
                onClicked: { if (audioController) audioController.toggleMedusaMode(); }
            }

            // Separator
            Rectangle { width: 1; height: 16; color: "#bebebe"; Layout.margins: 1 }

            // Group 6: Karaoke timing mode
            AudioBtn {
                iconSrc: "../../assets/icons_native/kara_mode_16.png"
                tipText: qsTr("Toggle karaoke mode")
                isToggle: true
                checked: audioController ? audioController.karaokeMode : false
                onClicked: { if (audioController) audioController.setKaraokeMode(!audioController.karaokeMode); }
            }

            // Separator
            Rectangle { width: 1; height: 16; color: "#bebebe"; Layout.margins: 1 }

            Text {
                text: qsTr("Speed:")
                font.pixelSize: 11
                font.family: uiTheme.uiFont
                color: "#444444"
                Layout.alignment: Qt.AlignVCenter
            }

            NativeComboBox {
                id: speedCombo
                implicitWidth: 68
                implicitHeight: 20
                font.pixelSize: 11
                model: ["0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "2.0x"]
                currentIndex: 2 // 1.0x
                onActivated: function(index) {
                    var speeds = [0.5, 0.75, 1.0, 1.25, 1.5, 2.0];
                    if (audioController) {
                        audioController.setPlaybackSpeed(speeds[index]);
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }
    }
}

