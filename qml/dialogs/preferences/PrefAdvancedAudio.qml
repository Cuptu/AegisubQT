// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../../controls"

// Preferences page: Advanced audio driver providers, memory caching strategies, and spectrogram parameters.
Flickable {
    id: root
    contentHeight: mainLayout.implicitHeight
    boundsBehavior: Flickable.StopAtBounds

    signal changed()

    function restoreDefaults() {
        cmbAudioProvider.currentIndex = 0;
        cmbAudioPlayer.currentIndex = 0;
        cmbCacheType.currentIndex = 1;
        txtAudioCachePath.text = "?temp";
        cmbSpectrumQuality.currentIndex = 2;
        cmbFreqMapping.currentIndex = 4;
        spinCacheMax.value = 128;
        root.changed();
    }

    function savePreferences() {
        // Persist advanced audio preferences
    }

    FolderDialog {
        id: cacheFolderDialog
        title: qsTr("Select Cache Directory")
        onAccepted: {
            txtAudioCachePath.text = selectedFolder.toString().replace("file:///", "");
            root.changed();
        }
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
                    Text { text: qsTr("Audio provider") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 100 }
                    NativeComboBox { id: cmbAudioProvider; model: ["Default", "FFmpegSource", "Avisynth"]; Layout.fillWidth: true; onCurrentIndexChanged: root.changed() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Audio player") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 100 }
                    NativeComboBox { id: cmbAudioPlayer; model: ["Default", "PortAudio", "DirectSound"]; Layout.fillWidth: true; onCurrentIndexChanged: root.changed() }
                }
            }
        }

        NativeGroupBox {
            title: qsTr("Cache")
            Layout.fillWidth: true
            implicitHeight: colCache.implicitHeight + 28

            ColumnLayout {
                id: colCache
                anchors.fill: parent
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Cache type") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 100 }
                    NativeComboBox { id: cmbCacheType; model: [qsTr("None (NOT RECOMMENDED)"), "RAM", qsTr("Hard Disk")]; currentIndex: 1; Layout.fillWidth: true; onCurrentIndexChanged: root.changed() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Path") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 100 }
                    NativeTextBox { id: txtAudioCachePath; text: "?temp"; Layout.fillWidth: true; onTextChanged: root.changed() }
                    NativeButton {
                        text: qsTr("Browse..."); Layout.preferredWidth: 65
                        onClicked: cacheFolderDialog.open()
                    }
                }
            }
        }

        NativeGroupBox {
            title: qsTr("Spectrum")
            Layout.fillWidth: true
            implicitHeight: colSpectrum.implicitHeight + 28

            ColumnLayout {
                id: colSpectrum
                anchors.fill: parent
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Quality") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 120 }
                    NativeComboBox { id: cmbSpectrumQuality; model: [qsTr("Regular quality"), qsTr("Better quality"), qsTr("High quality"), qsTr("Insane quality")]; currentIndex: 2; Layout.fillWidth: true; onCurrentIndexChanged: root.changed() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Frequency mapping") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 120 }
                    NativeComboBox { id: cmbFreqMapping; model: [qsTr("Linear"), qsTr("Extended"), qsTr("Medium"), qsTr("Compressed"), qsTr("Logarithmic")]; currentIndex: 4; Layout.fillWidth: true; onCurrentIndexChanged: root.changed() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Cache memory max (MB)") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.fillWidth: true }
                    NativeSpinBox { id: spinCacheMax; from: 2; to: 1024; value: 128; Layout.preferredWidth: 80; onValueModified: root.changed() }
                }
            }
        }
    }
}
