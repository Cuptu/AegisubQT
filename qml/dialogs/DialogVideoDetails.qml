// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogVideoDetails: Informational dialog displaying active video stream metadata,
// including resolution, framerate, frame count, duration, color matrix, and decoder backend.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Video Details")
    implicitWidth: 460
    implicitHeight: 380

    // Video stream metadata properties exposed for display
    property string fileName: "sample_video.mp4"
    property real fps: 23.976
    property int widthVal: 1920
    property int heightVal: 1080
    property int frameCount: 1440
    property string lengthStr: "0:01:00.00"
    property string colorMatrix: "TV.709"
    property string colorRange: "Limited"
    property string decoderName: "FFmpegSource"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Stream metadata property grid
        NativeGroupBox {
            title: qsTr("Video")
            Layout.fillWidth: true
            Layout.fillHeight: true

            GridLayout {
                anchors.fill: parent
                columns: 2
                columnSpacing: 8
                rowSpacing: 4

                Text { text: qsTr("File name:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { text: dialog.fileName; readOnly: true; Layout.fillWidth: true }

                Text { text: "FPS:"; font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { text: dialog.fps.toFixed(3); readOnly: true; Layout.fillWidth: true }

                Text { text: qsTr("Resolution:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { text: dialog.widthVal + "×" + dialog.heightVal + " (16:9)"; readOnly: true; Layout.fillWidth: true }

                Text { text: qsTr("Length:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { text: dialog.frameCount + " " + qsTr("frames") + " (" + dialog.lengthStr + ")"; readOnly: true; Layout.fillWidth: true }

                Text { text: qsTr("Color matrix:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { text: dialog.colorMatrix; readOnly: true; Layout.fillWidth: true }

                Text { text: qsTr("Override matrix:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { text: dialog.colorMatrix; readOnly: true; Layout.fillWidth: true }

                Text { text: qsTr("Color range:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { text: dialog.colorRange; readOnly: true; Layout.fillWidth: true }

                Text { text: qsTr("Decoder:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { text: dialog.decoderName; readOnly: true; Layout.fillWidth: true }
            }
        }

        // Dialog dismissal action
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }

            NativeButton {
                text: qsTr("OK"); isDefault: true
                Layout.preferredWidth: 75
                onClicked: dialog.close()
            }
        }
    }
}
