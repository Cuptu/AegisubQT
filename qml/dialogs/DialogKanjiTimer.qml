// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogKanjiTimer: Syllable and timing copy assistant synchronizing Romaji karaoke
// timing tags (\k) to Kanji / Japanese text dialogue rows.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Kanji Timer")
    iconSource: "../../assets/icons_native/timing_processor_toolbutton_16.png"
    implicitWidth: 540
    implicitHeight: 380

    // Emitted when timing transfer is requested between source and destination styles
    signal copyTimingRequested(string srcStyle, string dstStyle)
    signal statusMessage(string msg)

    property var stylesList: ["Default", "Romaji", "Kanji", "Sign", "Alt"]

    property string sourceLine: "{\\k15}sa{\\k20}ku{\\k35}ra"
    property string destLine: "桜"

    function doStart() {
        dialog.copyTimingRequested(cmbSource.currentText, cmbDest.currentText);
        dialog.close();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Source and destination style selection
        NativeGroupBox {
            title: qsTr("Styles")
            Layout.fillWidth: true
            implicitHeight: 65

            RowLayout {
                anchors.fill: parent
                spacing: 12

                Text { text: qsTr("Source style:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeComboBox {
                    id: cmbSource
                    Layout.fillWidth: true
                    model: dialog.stylesList
                    currentIndex: 1 // Romaji
                }

                Text { text: qsTr("Dest style:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                NativeComboBox {
                    id: cmbDest
                    Layout.fillWidth: true
                    model: dialog.stylesList
                    currentIndex: 2 // Kanji
                }
            }
        }

        // Syllable alignment and inspection
        NativeGroupBox {
            title: qsTr("Syllable matching")
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#ffffff"
                    border.color: "#7f9db9"
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        RowLayout {
                            Text { text: qsTr("Source: "); font.bold: true; font.pixelSize: 12; color: "#0055aa" }
                            Text { text: dialog.sourceLine; font.pixelSize: 13; font.family: uiTheme.uiFont }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#e0e0e0" }

                        RowLayout {
                            Text { text: qsTr("Dest: "); font.bold: true; font.pixelSize: 12; color: "#aa0055" }
                            Text { text: dialog.destLine; font.pixelSize: 16; font.family: uiTheme.uiFont }
                        }
                    }
                }

                // Syllable boundary adjustment controls
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    NativeButton {
                        text: qsTr("Source +")
                        Layout.fillWidth: true
                        onClicked: dialog.statusMessage(qsTr("Increased source syllable group"))
                    }
                    NativeButton {
                        text: qsTr("Source -")
                        Layout.fillWidth: true
                        onClicked: dialog.statusMessage(qsTr("Decreased source syllable group"))
                    }
                    NativeButton {
                        text: qsTr("Dest +")
                        Layout.fillWidth: true
                        onClicked: dialog.statusMessage(qsTr("Increased destination character group"))
                    }
                    NativeButton {
                        text: qsTr("Dest -")
                        Layout.fillWidth: true
                        onClicked: dialog.statusMessage(qsTr("Decreased destination character group"))
                    }
                    NativeButton {
                        text: qsTr("&Auto match")
                        Layout.fillWidth: true
                        onClicked: dialog.statusMessage(qsTr("Auto-matched Romaji syllables to Kanji characters"))
                    }
                    NativeButton {
                        text: qsTr("&Accept")
                        Layout.fillWidth: true
                        onClicked: dialog.statusMessage(qsTr("Accepted syllable match"))
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
                text: qsTr("&Start!")
                isDefault: true
                Layout.preferredWidth: 75
                onClicked: dialog.doStart()
            }

            NativeButton {
                text: qsTr("Close"); Layout.preferredWidth: 75
                onClicked: dialog.close()
            }

            NativeButton {
                text: qsTr("Help"); Layout.preferredWidth: 75
                onClicked: Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Kanji_Timer/")
            }
        }
    }
}
