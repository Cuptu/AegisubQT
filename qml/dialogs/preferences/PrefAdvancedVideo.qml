// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../controls"

// Preferences page: Advanced video decoding backends and FFmpegSource thread parameters.
Flickable {
    id: root
    contentHeight: mainLayout.implicitHeight
    boundsBehavior: Flickable.StopAtBounds

    signal changed()

    function restoreDefaults() {
        cmbVideoProvider.currentIndex = 0;
        cmbSubtitlesProvider.currentIndex = 0;
        cmbLogLevel.currentIndex = 4;
        spinDecodeThreads.value = -1;
        chkUnsafeSeeking.checked = false;
        root.changed();
    }

    function savePreferences() {
        // Persist advanced video preferences
    }

    ColumnLayout {
        id: mainLayout
        width: root.width - 8
        spacing: 8

        NativeGroupBox {
            title: qsTr("Expert")
            Layout.fillWidth: true
            implicitHeight: colExpert.implicitHeight + 28

            ColumnLayout {
                id: colExpert
                anchors.fill: parent
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Video provider") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 100 }
                    NativeComboBox { id: cmbVideoProvider; model: ["FFmpegSource", "Avisynth"]; Layout.fillWidth: true; onCurrentIndexChanged: root.changed() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Subtitles provider") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 100 }
                    NativeComboBox { id: cmbSubtitlesProvider; model: ["libass", "csri"]; Layout.fillWidth: true; onCurrentIndexChanged: root.changed() }
                }
            }
        }

        NativeGroupBox {
            title: "FFmpegSource"
            Layout.fillWidth: true
            implicitHeight: colFFMS.implicitHeight + 28

            ColumnLayout {
                id: colFFMS
                anchors.fill: parent
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Debug log verbosity") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 120 }
                    NativeComboBox {
                        id: cmbLogLevel
                        model: [qsTr("Quiet"), qsTr("Panic"), qsTr("Fatal"), qsTr("Error"), qsTr("Warning"), qsTr("Info"), qsTr("Verbose"), qsTr("Debug")]
                        currentIndex: 4
                        Layout.fillWidth: true
                        onCurrentIndexChanged: root.changed()
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Decoding threads") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 120 }
                    NativeSpinBox {
                        id: spinDecodeThreads
                        from: -1; to: 64; value: -1
                        Layout.preferredWidth: 80
                        onValueModified: root.changed()
                    }
                }
                NativeCheckBox {
                    id: chkUnsafeSeeking
                    text: qsTr("Enable unsafe seeking")
                    checked: false
                    onCheckedChanged: root.changed()
                }
            }
        }
    }
}
