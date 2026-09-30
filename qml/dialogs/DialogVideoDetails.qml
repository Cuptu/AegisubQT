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

    property var videoCtrl: null
    property var project: null
    property var details: ({hasVideo: false})
    property string scriptMatrix: "None"

    function refreshDetails() {
        details = videoCtrl ? videoCtrl.videoDetails : {hasVideo: false};
        scriptMatrix = project ? String(project.getScriptInfo("YCbCr Matrix", "None")) : "None";
    }
    onAboutToShow: refreshDetails()
    Connections {
        target: dialog.videoCtrl
        function onVideoInfoChanged() { dialog.refreshDetails(); }
        function onTimecodesChanged() { dialog.refreshDetails(); }
    }

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
                NativeTextBox { objectName: "video-details-file"; text: dialog.details.hasVideo ? dialog.details.fileName : qsTr("No video open"); readOnly: true; Layout.fillWidth: true }

                Text { text: "FPS:"; font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { objectName: "video-details-fps"; text: dialog.details.hasVideo ? Number(dialog.details.fps).toFixed(3) : "—"; readOnly: true; Layout.fillWidth: true }

                Text { text: qsTr("Resolution:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { objectName: "video-details-resolution"; text: dialog.details.hasVideo ? dialog.details.width + "×" + dialog.details.height + " (" + dialog.details.aspectRatio + ")" : "—"; readOnly: true; Layout.fillWidth: true }

                Text { text: qsTr("Length:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { objectName: "video-details-length"; text: dialog.details.hasVideo ? dialog.details.frameCount + " " + qsTr("frames") + " (" + dialog.details.length + ")" : "—"; readOnly: true; Layout.fillWidth: true }

                Text { text: qsTr("Color matrix:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { objectName: "video-details-matrix"; text: dialog.details.hasVideo ? dialog.details.colorMatrix : "—"; readOnly: true; Layout.fillWidth: true }

                Text { text: qsTr("Script matrix:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { objectName: "video-details-script-matrix"; text: dialog.scriptMatrix; readOnly: true; Layout.fillWidth: true }

                Text { text: qsTr("Color range:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { objectName: "video-details-range"; text: dialog.details.hasVideo ? dialog.details.colorRange : "—"; readOnly: true; Layout.fillWidth: true }

                Text { text: qsTr("Decoder:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { objectName: "video-details-decoder"; text: dialog.details.hasVideo ? dialog.details.decoder : "—"; readOnly: true; Layout.fillWidth: true }
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
