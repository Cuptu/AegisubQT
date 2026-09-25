// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogShiftTimes: Adjusts subtitle start and end timestamps by a specified time offset
// or frame count, with direction, scope filtering, and history tracking.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    title: qsTr("Shift Times")
    iconSource: "../../assets/icons_native/shift_times_toolbutton_16.png"
    width: 580
    height: 380
    implicitWidth: 580
    implicitHeight: 380

    // Emitted when user confirms timestamp adjustment:
    // amountMs: positive millisecond delta
    // isForward: true to shift forward (later), false backward (earlier)
    // affectMode: 0 = all rows, 1 = selected rows, 2 = selected onward
    // timeType: 0 = start & end, 1 = start only, 2 = end only
    signal shiftTimesRequested(int amountMs, bool isForward, int affectMode, int timeType)

    property var historyList: [
        { text: "500ms forward, s+e, all", ms: 500, forward: true, affect: 0, time: 0 },
        { text: "200ms backward, s+e, sel", ms: 200, forward: false, affect: 1, time: 0 }
    ]

    function getFrameMs() {
        if (typeof videoController !== "undefined" && videoController && videoController.fps > 0) {
            return 1000.0 / videoController.fps;
        }
        return 41.7083;
    }

    function applyHistory(idx) {
        if (idx >= 0 && idx < historyList.length) {
            var item = historyList[idx];
            txtTime.text = formatMs(item.ms);
            txtFrames.text = Math.round(item.ms / getFrameMs()).toString();
            radForward.checked = item.forward;
            radBackward.checked = !item.forward;
            if (item.affect === 0) radAll.checked = true;
            else if (item.affect === 1) radSelected.checked = true;
            else radOnward.checked = true;

            if (item.time === 0) radBothTimes.checked = true;
            else if (item.time === 1) radStartOnly.checked = true;
            else radEndOnly.checked = true;
        }
    }

    function formatMs(ms) {
        var cs = Math.floor((ms % 1000) / 10);
        var s = Math.floor(ms / 1000) % 60;
        var m = Math.floor(ms / 60000) % 60;
        var h = Math.floor(ms / 3600000);
        var pad = (n) => n < 10 ? "0" + n : n;
        return h + ":" + pad(m) + ":" + pad(s) + "." + pad(cs);
    }

    function parseMs(str) {
        if (!str) return 0;
        var parts = str.split(":");
        if (parts.length === 3) {
            var h = parseInt(parts[0]) || 0;
            var m = parseInt(parts[1]) || 0;
            var secParts = parts[2].split(".");
            var s = parseInt(secParts[0]) || 0;
            var cs = (secParts.length > 1 ? parseInt(secParts[1]) : 0) || 0;
            return (h * 3600 + m * 60 + s) * 1000 + cs * 10;
        }
        var num = parseInt(str);
        return isNaN(num) ? 500 : num;
    }

    function doShift() {
        var ms = radByTime.checked ? parseMs(txtTime.text) : Math.round((parseInt(txtFrames.text) || 0) * getFrameMs());
        var isFwd = radForward.checked;
        var affect = radAll.checked ? 0 : (radSelected.checked ? 1 : 2);
        var tType = radBothTimes.checked ? 0 : (radStartOnly.checked ? 1 : 2);

        // Append shift configuration to session history
        var histDesc = (radByTime.checked ? (ms + "ms") : (txtFrames.text + " frames")) + " " + (isFwd ? "forward" : "backward") + ", " + (tType === 0 ? "s+e" : (tType === 1 ? "s" : "e")) + ", " + (affect === 0 ? "all" : (affect === 1 ? "sel" : "onward"));
        var newHist = [].concat(dialog.historyList);
        newHist.unshift({ text: histDesc, ms: ms, forward: isFwd, affect: affect, time: tType });
        dialog.historyList = newHist;

        dialog.shiftTimesRequested(ms, isFwd, affect, tType);
        dialog.close();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        // Two-column layout: shift parameters on left, preset history on right
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // Shift parameter controls
            ColumnLayout {
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                spacing: 6

                // Shift magnitude and direction
                NativeGroupBox {
                    title: qsTr("Shift by")
                    Layout.fillWidth: true
                    implicitHeight: 90

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 4

                        GridLayout {
                            columns: 2
                            columnSpacing: 6
                            rowSpacing: 4

                            NativeRadioButton {
                                id: radByTime
                                text: qsTr("&Time:"); checked: true
                            }
                            NativeTextBox {
                                id: txtTime
                                text: "0:00:00.50"
                                enabled: radByTime.checked
                                Layout.fillWidth: true
                            }

                            NativeRadioButton {
                                id: radByFrames
                                text: qsTr("&Frames:")
                            }
                            NativeTextBox {
                                id: txtFrames
                                text: "12"
                                enabled: radByFrames.checked
                                Layout.fillWidth: true
                                validator: IntValidator { bottom: 0; top: 100000 }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            NativeRadioButton {
                                id: radForward
                                text: qsTr("&Forward")
                                checked: true
                            }
                            NativeRadioButton {
                                id: radBackward
                                text: qsTr("&Backward")
                            }
                        }
                    }
                }

                // Line selection scope
                NativeGroupBox {
                    title: qsTr("Affect")
                    Layout.fillWidth: true
                    implicitHeight: 88

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 4

                        NativeRadioButton {
                            id: radAll
                            text: qsTr("All rows"); checked: true
                        }
                        NativeRadioButton {
                            id: radSelected
                            text: qsTr("Selected &rows")
                        }
                        NativeRadioButton {
                            id: radOnward
                            text: qsTr("Selection onwards")
                        }
                    }
                }

                // Timestamp field target (start, end, or both)
                NativeGroupBox {
                    title: qsTr("Times")
                    Layout.fillWidth: true
                    implicitHeight: 88

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 4

                        NativeRadioButton {
                            id: radBothTimes
                            text: qsTr("&Start and End times")
                            checked: true
                        }
                        NativeRadioButton {
                            id: radStartOnly
                            text: qsTr("&Start times only")
                        }
                        NativeRadioButton {
                            id: radEndOnly
                            text: qsTr("&End times only")
                        }
                    }
                }
            }

            // Shift history list
            NativeGroupBox {
                title: qsTr("Load from history")
                Layout.preferredWidth: 310
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#ffffff"
                        border.color: "#7f9db9"
                        border.width: 1

                        ListView {
                            id: lvHistory
                            anchors.fill: parent
                            anchors.margins: 1
                            clip: true
                            model: dialog.historyList
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: Rectangle {
                                width: lvHistory.width
                                height: 20
                                color: lvHistory.currentIndex === index ? "#3399ff" : (hMouse.containsMouse ? "#e5f1fb" : "transparent")

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 4
                                    anchors.right: parent.right
                                    anchors.rightMargin: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.text
                                    font.pixelSize: 11
                                    font.family: uiTheme.uiFont
                                    renderType: Text.NativeRendering
                                    color: lvHistory.currentIndex === index ? "#ffffff" : "#000000"
                                    elide: Text.ElideRight
                                }

                                MouseArea {
                                    id: hMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: lvHistory.currentIndex = index
                                    onDoubleClicked: {
                                        lvHistory.currentIndex = index;
                                        dialog.applyHistory(index);
                                    }
                                }
                            }
                        }
                    }

                    NativeButton {
                        text: qsTr("&Clear")
                        Layout.fillWidth: true
                        onClicked: dialog.historyList = []
                    }
                }
            }
        }

        // Dialog action buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Item { Layout.fillWidth: true }

            NativeButton {
                text: qsTr("OK"); isDefault: true
                Layout.preferredWidth: 75
                onClicked: dialog.doShift()
            }

            NativeButton {
                text: qsTr("Cancel"); Layout.preferredWidth: 75
                onClicked: dialog.close()
            }

            NativeButton {
                text: qsTr("Help"); Layout.preferredWidth: 75
                onClicked: {
                    Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Shift_Times/")
                }
            }
        }
    }
}
