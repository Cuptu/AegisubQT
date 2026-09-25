// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogProperties: ASS script properties editor configuring script info headers,
// target play/layout resolutions, wrap style, border scaling, and YCbCr matrix.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Script Properties")
    iconSource: "../../assets/icons_native/properties_toolbutton_16.png"
    implicitWidth: 430
    implicitHeight: 520

    property var project: null
    property var videoCtrl: null

    // Emitted when script properties are committed
    signal propertiesUpdated(string title, string orig, string trans, int resX, int resY, int wrapStyle, string matrix)

    onAboutToShow: loadValues()

    function loadValues() {
        if (!project) return;
        txtTitle.text = project.getScriptInfo("Title", qsTr("Untitled"));
        txtOriginalScript.text = project.getScriptInfo("Original Script", "");
        txtTranslation.text = project.getScriptInfo("Original Translation", "");
        txtEditing.text = project.getScriptInfo("Original Editing", "");
        txtTiming.text = project.getScriptInfo("Original Timing", "");
        txtSynchPoint.text = project.getScriptInfo("Synch Point", "");
        txtUpdatedBy.text = project.getScriptInfo("Script Updated By", "");
        txtUpdateDetails.text = project.getScriptInfo("Update Details", "");
        txtResX.text = project.getScriptInfo("PlayResX", 1920).toString();
        txtResY.text = project.getScriptInfo("PlayResY", 1080).toString();
        txtLayoutResX.text = project.getScriptInfo("LayoutResX", 0).toString();
        txtLayoutResY.text = project.getScriptInfo("LayoutResY", 0).toString();
        var wrap = parseInt(project.getScriptInfo("WrapStyle", 0)) || 0;
        cmbWrapStyle.currentIndex = Math.max(0, Math.min(3, wrap));
        var scaled = project.getScriptInfo("ScaledBorderAndShadow", "yes");
        chkScaleBorder.checked = (scaled.toString().toLowerCase() === "yes");
        var m = project.getScriptInfo("YCbCr Matrix", "None");
        var mIdx = cmbMatrix.find(m);
        cmbMatrix.currentIndex = mIdx >= 0 ? mIdx : 0;
    }

    function onOkClicked() {
        if (project) {
            project.setScriptInfo("Title", txtTitle.text);
            project.setScriptInfo("Original Script", txtOriginalScript.text);
            project.setScriptInfo("Original Translation", txtTranslation.text);
            project.setScriptInfo("Original Editing", txtEditing.text);
            project.setScriptInfo("Original Timing", txtTiming.text);
            project.setScriptInfo("Synch Point", txtSynchPoint.text);
            project.setScriptInfo("Script Updated By", txtUpdatedBy.text);
            project.setScriptInfo("Update Details", txtUpdateDetails.text);
            project.setScriptInfo("PlayResX", parseInt(txtResX.text) || 0);
            project.setScriptInfo("PlayResY", parseInt(txtResY.text) || 0);
            project.setScriptInfo("LayoutResX", parseInt(txtLayoutResX.text) || 0);
            project.setScriptInfo("LayoutResY", parseInt(txtLayoutResY.text) || 0);
            project.setScriptInfo("WrapStyle", cmbWrapStyle.currentIndex);
            project.setScriptInfo("ScaledBorderAndShadow", chkScaleBorder.checked ? "yes" : "no");
            project.setScriptInfo("YCbCr Matrix", cmbMatrix.currentText);
        }
        dialog.propertiesUpdated(txtTitle.text, txtOriginalScript.text, txtTranslation.text, parseInt(txtResX.text) || 0, parseInt(txtResY.text) || 0, cmbWrapStyle.currentIndex, cmbMatrix.currentText);
        dialog.close();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        // Script metadata fields
        NativeGroupBox {
            title: qsTr("Script")
            Layout.fillWidth: true
            implicitHeight: 226

            GridLayout {
                anchors.fill: parent
                columns: 2
                columnSpacing: 6
                rowSpacing: 3

                Text { text: qsTr("Title:"); font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 80 }
                NativeTextBox { id: txtTitle; Layout.fillWidth: true }

                Text { text: qsTr("Original script:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { id: txtOriginalScript; Layout.fillWidth: true }

                Text { text: qsTr("Translation:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { id: txtTranslation; Layout.fillWidth: true }

                Text { text: qsTr("Editing:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { id: txtEditing; Layout.fillWidth: true }

                Text { text: qsTr("Timing:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { id: txtTiming; Layout.fillWidth: true }

                Text { text: qsTr("Synch point:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { id: txtSynchPoint; Layout.fillWidth: true }

                Text { text: qsTr("Updated by:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { id: txtUpdatedBy; Layout.fillWidth: true }

                Text { text: qsTr("Update details:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox { id: txtUpdateDetails; Layout.fillWidth: true }
            }
        }

        // Resolution and color matrix settings
        NativeGroupBox {
            title: qsTr("Resolution")
            Layout.fillWidth: true
            implicitHeight: 110

            GridLayout {
                anchors.fill: parent
                columns: 5
                columnSpacing: 4
                rowSpacing: 4

                // Script play resolution
                Text { text: qsTr("Script: "); font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 62 }
                NativeTextBox {
                    id: txtResX
                    Layout.fillWidth: true
                    validator: IntValidator { bottom: 0; top: 10000 }
                }
                Text {
                    text: "×"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    Layout.preferredWidth: 12
                }
                NativeTextBox {
                    id: txtResY
                    Layout.fillWidth: true
                    validator: IntValidator { bottom: 0; top: 10000 }
                }
                NativeButton {
                    text: qsTr("From &video"); Layout.preferredWidth: 98
                    enabled: !!(dialog.videoCtrl && dialog.videoCtrl.hasVideo)
                    onClicked: {
                        if (dialog.videoCtrl) {
                            txtResX.text = dialog.videoCtrl.videoWidth.toString();
                            txtResY.text = dialog.videoCtrl.videoHeight.toString();
                        }
                    }
                }

                // Layout resolution
                Text { text: qsTr("Layout: "); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeTextBox {
                    id: txtLayoutResX
                    Layout.fillWidth: true
                    validator: IntValidator { bottom: 0; top: 10000 }
                }
                Text {
                    text: "×"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    Layout.preferredWidth: 12
                }
                NativeTextBox {
                    id: txtLayoutResY
                    Layout.fillWidth: true
                    validator: IntValidator { bottom: 0; top: 10000 }
                }
                NativeButton {
                    text: qsTr("From &video")
                    Layout.preferredWidth: 98
                    enabled: !!(dialog.videoCtrl && dialog.videoCtrl.hasVideo)
                    onClicked: {
                        if (dialog.videoCtrl) {
                            txtLayoutResX.text = dialog.videoCtrl.videoWidth.toString();
                            txtLayoutResY.text = dialog.videoCtrl.videoHeight.toString();
                        }
                    }
                }

                // Color space / YCbCr matrix
                Text { text: qsTr("YCbCr Matrix:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeComboBox {
                    id: cmbMatrix
                    Layout.columnSpan: 3
                    Layout.fillWidth: true
                    model: ["None", "TV.601", "PC.601", "TV.709", "PC.709", "TV.240m", "PC.240m", "TV.fcc", "PC.fcc"]
                }
                NativeButton {
                    text: qsTr("From &video")
                    Layout.preferredWidth: 98
                    enabled: !!(dialog.videoCtrl && dialog.videoCtrl.hasVideo)
                    onClicked: {
                        if (dialog.videoCtrl) {
                            var rec = (dialog.videoCtrl.videoHeight > 576 ? "TV.709" : "TV.601");
                            var idx = cmbMatrix.find(rec);
                            if (idx >= 0) cmbMatrix.currentIndex = idx;
                        }
                    }
                }
            }
        }

        // Script rendering options
        NativeGroupBox {
            title: qsTr("Options")
            Layout.fillWidth: true
            implicitHeight: 76

            ColumnLayout {
                anchors.fill: parent
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Text {
                        text: qsTr("Wrap style:") + " "
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        Layout.preferredWidth: 62
                    }
                    NativeComboBox {
                        id: cmbWrapStyle
                        Layout.fillWidth: true
                        model: [
                            qsTr("0: Header-heavy smart wrapping"),
                            qsTr("1: End-of-line word wrapping"),
                            qsTr("2: No word wrapping, \\n or \\N to break"),
                            qsTr("3: Lower-heavy smart wrapping")
                        ]
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Item { Layout.preferredWidth: 62 }
                    NativeCheckBox {
                        id: chkScaleBorder
                        text: qsTr("Scale border and shadow")
                        checked: true
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Scale border and shadow"); ToolTip.delay: 700
                    }
                }
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
                onClicked: {
                    Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Script_Properties/")
                }
            }
        }
    }
}
