// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogDummyVideo: Configuration dialog for generating synthetic video streams
// with selectable resolution presets, solid color or checkerboard pattern, FPS, and frame count.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"
import "../project/AssUtils.js" as AssUtils

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Dummy video options")
    implicitWidth: 380
    implicitHeight: 290

    // Emitted when user confirms creation of a synthetic video stream:
    // w: width in pixels
    // h: height in pixels
    // col: canvas background color
    // fpsVal: frame rate
    // frameCount: total duration in frames
    // checker: true to render alternating checkerboard pattern
    signal dummyVideoCreated(int w, int h, color col, real fpsVal, int frameCount, bool checker)

    property var resolutions: [
        { name: "640×480 (SD fullscreen)", w: 640, h: 480 },
        { name: "704×480 (SD anamorphic)", w: 704, h: 480 },
        { name: "640×360 (SD widescreen)", w: 640, h: 360 },
        { name: "1024×576 (SuperPAL widescreen)", w: 1024, h: 576 },
        { name: "1280×720 (HD 720p)", w: 1280, h: 720 },
        { name: "1920×1080 (FHD 1080p)", w: 1920, h: 1080 },
        { name: "2560×1440 (QHD 1440p)", w: 2560, h: 1440 },
        { name: "3840×2160 (4K UHD 2160p)", w: 3840, h: 2160 },
        { name: "1080×1920 (FHD vertical)", w: 1080, h: 1920 }
    ]

    property color videoColor: "#202020"

    function updateDurationDisplay() {
        var fps = parseFloat(cmbFps.currentText) || 23.976;
        var frames = spinLength.value;
        var sec = frames / fps;
        return AssUtils.msToAss(Math.round(sec * 1000));
    }

    function onOkClicked() {
        var fps = parseFloat(cmbFps.currentText) || 23.976;
        dialog.dummyVideoCreated(spinWidth.value, spinHeight.value, dialog.videoColor, fps, spinLength.value, chkPattern.checked);
        dialog.close();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 8
            rowSpacing: 6

            Text { text: qsTr("Resolution:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
            NativeComboBox {
                id: cmbResPreset
                Layout.fillWidth: true
                model: dialog.resolutions.map(r => r.name)
                currentIndex: 5 // Default: 1920x1080 FHD
                onActivated: (idx) => {
                    spinWidth.value = dialog.resolutions[idx].w;
                    spinHeight.value = dialog.resolutions[idx].h;
                }
            }

            Item { width: 1; height: 1 }
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                NativeSpinBox {
                    id: spinWidth
                    Layout.fillWidth: true
                    from: 1; to: 10000; value: 1920
                }
                Text { text: "×"; font.pixelSize: 12 }
                NativeSpinBox {
                    id: spinHeight
                    Layout.fillWidth: true
                    from: 1; to: 10000; value: 1080
                }
            }

            Text { text: qsTr("Colour:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    width: 32
                    height: 19
                    color: dialog.videoColor
                    border.color: "#808080"
                    border.width: 1
                }

                NativeCheckBox {
                    id: chkPattern
                    text: qsTr("Checkerboard &pattern"); checked: false
                }
            }

            Text { text: qsTr("Frame rate (fps):"); font.pixelSize: 12; font.family: uiTheme.uiFont }
            NativeComboBox {
                id: cmbFps
                Layout.fillWidth: true
                model: ["23.976", "24", "25", "29.97", "30", "50", "59.94", "60"]
                currentIndex: 0
            }

            Text { text: qsTr("Duration (frames):"); font.pixelSize: 12; font.family: uiTheme.uiFont }
            NativeSpinBox {
                id: spinLength
                Layout.fillWidth: true
                from: 2; to: 36000000; value: 40000
            }

            Item { width: 1; height: 1 }
            Text {
                text: qsTr("Duration: ") + dialog.updateDurationDisplay()
                font.pixelSize: 11
                color: "#666666"
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

            NativeButton {
                text: qsTr("Help"); Layout.preferredWidth: 75
                onClicked: Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Video/#dummy-video")
            }
        }
    }
}
