// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogJumpTo: Navigation dialog for seeking to a specific video frame or timestamp.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"
import "../project/AssUtils.js" as AssUtils

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Jump to")
    iconSource: "../../assets/icons_native/jumpto_button_16.png"
    implicitWidth: 260
    implicitHeight: 140

    property int targetFrame: 0
    property string targetTime: "0:00:00.00"
    property real fps: 23.976

    // Emitted when target frame or timestamp is confirmed for seeking
    signal jumpRequested(int frame, string timeStr, real timeSec)

    function onFrameEdited() {
        var f = parseInt(txtFrame.text) || 0;
        targetFrame = Math.max(0, f);
        var sec = targetFrame / (fps > 0 ? fps : 23.976);
        targetTime = AssUtils.msToAss(Math.round(sec * 1000));
        txtTime.text = targetTime;
    }

    function onTimeEdited() {
        var ms = AssUtils.assToMs(txtTime.text);
        var sec = ms / 1000.0;
        targetTime = txtTime.text;
        targetFrame = Math.round(sec * (fps > 0 ? fps : 23.976));
        txtFrame.text = targetFrame.toString();
    }

    function onOkClicked() {
        var ms = AssUtils.assToMs(txtTime.text);
        var sec = ms / 1000.0;
        dialog.jumpRequested(targetFrame, targetTime, sec);
        dialog.close();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // Frame index and timestamp input fields
        GridLayout {
            Layout.alignment: Qt.AlignHCenter
            columns: 2
            columnSpacing: 8
            rowSpacing: 6

            Text {
                text: qsTr("Frame: ")
                font.pixelSize: 12
                font.family: uiTheme.uiFont
                renderType: Text.NativeRendering
            }

            NativeTextBox {
                id: txtFrame
                text: dialog.targetFrame.toString()
                implicitWidth: 120
                validator: IntValidator { bottom: 0; top: 10000000 }
                onTextChanged: {
                    if (activeFocus) dialog.onFrameEdited();
                }
                Keys.onReturnPressed: dialog.onOkClicked()
            }

            Text {
                text: qsTr("Time: ")
                font.pixelSize: 12
                font.family: uiTheme.uiFont
                renderType: Text.NativeRendering
            }

            NativeTextBox {
                id: txtTime
                text: dialog.targetTime
                implicitWidth: 120
                onTextChanged: {
                    if (activeFocus) dialog.onTimeEdited();
                }
                Keys.onReturnPressed: dialog.onOkClicked()
            }
        }

        Item { Layout.fillHeight: true }

        // Dialog action buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Item { Layout.fillWidth: true }

            NativeButton {
                text: qsTr("OK"); isDefault: true
                Layout.preferredWidth: 75
                onClicked: dialog.onOkClicked()
            }

            NativeButton {
                text: qsTr("Cancel"); Layout.preferredWidth: 75
                onClicked: dialog.close()
            }
        }
    }
}
