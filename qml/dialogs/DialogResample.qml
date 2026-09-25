// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogResample: Script resolution resampling utility recalculating coordinates,
// font sizes, margins, drawing commands, and vector clip paths for target resolutions.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Resample Resolution")
    iconSource: "../../assets/icons_native/resample_toolbutton_16.png"
    implicitWidth: 420
    implicitHeight: 410

    property var videoCtrl: null
    property var project: null

    // Emitted when resampling parameters are confirmed
    signal resampleRequested(int srcW, int srcH, int dstW, int dstH, bool resampleMargins)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        // Source script resolution
        NativeGroupBox {
            title: qsTr("Source Resolution")
            Layout.fillWidth: true
            implicitHeight: 68

            RowLayout {
                anchors.fill: parent
                spacing: 6

                Text { text: qsTr("Width:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { id: txtSrcW; text: "1280"; Layout.preferredWidth: 60; validator: IntValidator { bottom: 100; top: 10000 } }

                Text { text: qsTr("Height:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { id: txtSrcH; text: "720"; Layout.preferredWidth: 60; validator: IntValidator { bottom: 100; top: 10000 } }

                Item { Layout.fillWidth: true }

                NativeButton {
                    text: qsTr("From &script"); Layout.preferredWidth: 90
                    onClicked: {
                        if (dialog.project) {
                            txtSrcW.text = String(dialog.project.getScriptInfo("PlayResX", 1920));
                            txtSrcH.text = String(dialog.project.getScriptInfo("PlayResY", 1080));
                        } else {
                            txtSrcW.text = "1280";
                            txtSrcH.text = "720";
                        }
                    }
                }
            }
        }

        // Target destination resolution
        NativeGroupBox {
            title: qsTr("Destination Resolution")
            Layout.fillWidth: true
            implicitHeight: 106

            ColumnLayout {
                anchors.fill: parent
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text { text: qsTr("Width:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                    NativeTextBox { id: txtDstW; text: "1920"; Layout.preferredWidth: 60; validator: IntValidator { bottom: 100; top: 10000 } }

                    Text { text: qsTr("Height:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                    NativeTextBox { id: txtDstH; text: "1080"; Layout.preferredWidth: 60; validator: IntValidator { bottom: 100; top: 10000 } }

                    Item { Layout.fillWidth: true }

                    NativeButton {
                        text: qsTr("From &video"); Layout.preferredWidth: 90
                        enabled: !!(dialog.videoCtrl && dialog.videoCtrl.hasVideo)
                        onClicked: {
                            if (dialog.videoCtrl) {
                                txtDstW.text = dialog.videoCtrl.videoWidth.toString();
                                txtDstH.text = dialog.videoCtrl.videoHeight.toString();
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Text { text: qsTr("Predefined:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                    NativeComboBox {
                        Layout.fillWidth: true
                        model: ["1080p (1920×1080)", "720p (1280×720)", "480p (640×480)", "4K UHD (3840×2160)"]
                        onActivated: (idx) => {
                            if (idx === 0) { txtDstW.text = "1920"; txtDstH.text = "1080"; }
                            else if (idx === 1) { txtDstW.text = "1280"; txtDstH.text = "720"; }
                            else if (idx === 2) { txtDstW.text = "640"; txtDstH.text = "480"; }
                            else if (idx === 3) { txtDstW.text = "3840"; txtDstH.text = "2160"; }
                        }
                    }
                }
            }
        }

        // Aspect ratio mismatch handling options
        NativeGroupBox {
            title: qsTr("Change aspect ratio")
            Layout.fillWidth: true
            implicitHeight: 68

            RowLayout {
                anchors.fill: parent
                spacing: 12
                RadioButton { id: radStretch; text: qsTr("Stretch"); checked: true; font.pixelSize: 12; font.family: uiTheme.uiFont }
                RadioButton { id: radPad; text: qsTr("Add borders"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                RadioButton { id: radCrop; text: qsTr("Crop"); font.pixelSize: 12; font.family: uiTheme.uiFont }
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
                onClicked: {
                    dialog.resampleRequested(parseInt(txtSrcW.text) || 1280, parseInt(txtSrcH.text) || 720, parseInt(txtDstW.text) || 1920, parseInt(txtDstH.text) || 1080, true);
                    dialog.close();
                }
            }

            NativeButton {
                text: qsTr("Cancel"); Layout.preferredWidth: 75
                onClicked: dialog.close()
            }

            NativeButton {
                text: qsTr("Help"); Layout.preferredWidth: 75
                onClicked: Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Resolution_Resamplers/")
            }
        }
    }
}
