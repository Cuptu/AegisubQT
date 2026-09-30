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
    implicitWidth: 480
    implicitHeight: 570

    property var videoCtrl: null
    property var project: null
    property string errorMessage: ""
    property var matrixOptions: ["", "TV.601", "PC.601", "TV.709", "PC.709", "TV.FCC", "PC.FCC", "TV.240M", "PC.240M"]
    onAboutToShow: {
        errorMessage = "";
        if (project) {
            txtSrcW.text = String(project.getScriptInfo("PlayResX", 1280));
            txtSrcH.text = String(project.getScriptInfo("PlayResY", 720));
        }
    }

    // Emitted when resampling parameters are confirmed
    signal resampleRequested(var settings)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        // Source script resolution
        NativeGroupBox {
            title: qsTr("Source Resolution")
            Layout.fillWidth: true
            implicitHeight: 100

            ColumnLayout {
                anchors.fill: parent
                spacing: 6
                RowLayout {
                Layout.fillWidth: true

                Text { text: qsTr("Width:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { id: txtSrcW; objectName: "resample-source-width"; text: "1280"; Layout.preferredWidth: 60; validator: IntValidator { bottom: 1; top: 2147483647 } }

                Text { text: qsTr("Height:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { id: txtSrcH; objectName: "resample-source-height"; text: "720"; Layout.preferredWidth: 60; validator: IntValidator { bottom: 1; top: 2147483647 } }

                Item { Layout.fillWidth: true }

                NativeButton {
                    objectName: "resample-from-script"
                    text: qsTr("From &script"); Layout.preferredWidth: 90
                    onClicked: {
                        if (dialog.project) {
                            txtSrcW.text = String(dialog.project.getScriptInfo("PlayResX", 1920));
                            txtSrcH.text = String(dialog.project.getScriptInfo("PlayResY", 1080));
                            sourceMatrix.currentIndex = Math.max(0, dialog.matrixOptions.indexOf(String(dialog.project.getScriptInfo("YCbCr Matrix", ""))));
                        } else {
                            txtSrcW.text = "1280";
                            txtSrcH.text = "720";
                        }
                    }
                }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("YCbCr Matrix:"); font.family: uiTheme.uiFont }
                    NativeComboBox { id: sourceMatrix; objectName: "resample-source-matrix"; Layout.fillWidth: true; model: dialog.matrixOptions }
                }
            }
        }

        // Target destination resolution
        NativeGroupBox {
            title: qsTr("Destination Resolution")
            Layout.fillWidth: true
            implicitHeight: 136

            ColumnLayout {
                anchors.fill: parent
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text { text: qsTr("Width:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                    NativeTextBox { id: txtDstW; objectName: "resample-dest-width"; text: "1920"; onTextChanged: if (presets) presets.sync(); Layout.preferredWidth: 60; validator: IntValidator { bottom: 1; top: 2147483647 } }

                    Text { text: qsTr("Height:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                    NativeTextBox { id: txtDstH; objectName: "resample-dest-height"; text: "1080"; onTextChanged: if (presets) presets.sync(); Layout.preferredWidth: 60; validator: IntValidator { bottom: 1; top: 2147483647 } }

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
                        id: presets
                        objectName: "resample-preset"
                        Layout.fillWidth: true
                        model: [qsTr("Custom"), "1080p (1920×1080)", "720p (1280×720)", "480p (640×480)", "4K UHD (3840×2160)"]
                        function sync() {
                            currentIndex = Math.max(0, ["", "1920x1080", "1280x720", "640x480", "3840x2160"].indexOf(txtDstW.text + "x" + txtDstH.text));
                        }
                        Component.onCompleted: sync()
                        onActivated: (idx) => {
                            if (idx === 1) { txtDstW.text = "1920"; txtDstH.text = "1080"; }
                            else if (idx === 2) { txtDstW.text = "1280"; txtDstH.text = "720"; }
                            else if (idx === 3) { txtDstW.text = "640"; txtDstH.text = "480"; }
                            else if (idx === 4) { txtDstW.text = "3840"; txtDstH.text = "2160"; }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("YCbCr Matrix:"); font.family: uiTheme.uiFont }
                    NativeComboBox { id: destMatrix; objectName: "resample-dest-matrix"; Layout.fillWidth: true; model: dialog.matrixOptions }
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
                RadioButton { id: radStretch; objectName: "resample-stretch"; text: qsTr("Stretch"); checked: true; font.pixelSize: 12; font.family: uiTheme.uiFont }
                RadioButton { id: radPad; objectName: "resample-borders"; text: qsTr("Add borders"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                RadioButton { id: radCrop; objectName: "resample-crop"; text: qsTr("Crop"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                RadioButton { id: radManual; objectName: "resample-manual"; text: qsTr("Manual"); font.pixelSize: 12; font.family: uiTheme.uiFont }
            }
        }

        NativeGroupBox {
            title: qsTr("Margin offset")
            Layout.fillWidth: true
            implicitHeight: 90
            enabled: radManual.checked
            GridLayout {
                anchors.fill: parent
                columns: 4
                Text { text: qsTr("Left:"); font.family: uiTheme.uiFont }
                NativeTextBox { id: marginLeft; objectName: "resample-left"; text: "0"; onTextChanged: if (symmetrical.checked) marginRight.text = text; Layout.fillWidth: true; validator: IntValidator { bottom: -9999; top: 9999 } }
                Text { text: qsTr("Right:"); font.family: uiTheme.uiFont }
                NativeTextBox { id: marginRight; objectName: "resample-right"; text: "0"; enabled: !symmetrical.checked; Layout.fillWidth: true; validator: IntValidator { bottom: -9999; top: 9999 } }
                Text { text: qsTr("Top:"); font.family: uiTheme.uiFont }
                NativeTextBox { id: marginTop; objectName: "resample-top"; text: "0"; onTextChanged: if (symmetrical.checked) marginBottom.text = text; Layout.fillWidth: true; validator: IntValidator { bottom: -9999; top: 9999 } }
                Text { text: qsTr("Bottom:"); font.family: uiTheme.uiFont }
                NativeTextBox { id: marginBottom; objectName: "resample-bottom"; text: "0"; enabled: !symmetrical.checked; Layout.fillWidth: true; validator: IntValidator { bottom: -9999; top: 9999 } }
                CheckBox { id: symmetrical; objectName: "resample-symmetrical"; onCheckedChanged: if (checked) { marginRight.text = marginLeft.text; marginBottom.text = marginTop.text; } text: qsTr("Symmetrical"); checked: true; Layout.columnSpan: 4 }
            }
        }

        Item { Layout.fillHeight: true }
        Text { text: dialog.errorMessage; visible: text.length > 0; color: "#b00020"; wrapMode: Text.Wrap; Layout.fillWidth: true }

        // Dialog action buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Item { Layout.fillWidth: true }

            NativeButton {
                objectName: "resample-ok"
                text: qsTr("OK"); isDefault: true
                Layout.preferredWidth: 75
                onClicked: {
                    if (!txtSrcW.acceptableInput || !txtSrcH.acceptableInput || !txtDstW.acceptableInput || !txtDstH.acceptableInput) {
                        dialog.errorMessage = qsTr("Enter valid source and destination resolutions");
                        return;
                    }
                    if (radManual.checked && (!marginLeft.acceptableInput || !marginRight.acceptableInput || !marginTop.acceptableInput || !marginBottom.acceptableInput)) {
                        dialog.errorMessage = qsTr("Enter valid margin offsets");
                        return;
                    }
                    dialog.errorMessage = "";
                    var settings = {sourceX:Number(txtSrcW.text), sourceY:Number(txtSrcH.text),
                        destX:Number(txtDstW.text), destY:Number(txtDstH.text), mode:radManual.checked ? 3 : (radPad.checked ? 1 : (radCrop.checked ? 2 : 0)),
                        left:radManual.checked ? Number(marginLeft.text) : 0, right:radManual.checked ? Number(symmetrical.checked ? marginLeft.text : marginRight.text) : 0,
                        top:radManual.checked ? Number(marginTop.text) : 0, bottom:radManual.checked ? Number(symmetrical.checked ? marginTop.text : marginBottom.text) : 0};
                    if (sourceMatrix.currentIndex > 0 && destMatrix.currentIndex > 0) {
                        settings.sourceMatrix = sourceMatrix.currentText;
                        settings.destMatrix = destMatrix.currentText;
                    }
                    dialog.resampleRequested(settings);
                }
            }

            NativeButton {
                objectName: "resample-cancel"
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
