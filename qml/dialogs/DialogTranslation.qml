// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogTranslation: Streamlined translation assistant presenting original text,
// translation input with hotkey-driven line navigation, and audio/video playback integration.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Translation Assistant")
    iconSource: "../../assets/icons_native/translation_toolbutton_16.png"
    implicitWidth: 540
    implicitHeight: 460

    property int currentLineNumber: 1
    property int totalLines: 10
    property string currentLineTime: ""
    property string originalText: "This is the source dialogue text to be translated."
    property string translationText: ""

    // Emitted when translation text is submitted for current line
    signal commitTranslation(string newText)
    signal commitRequested(string text, bool autoNext)

    // Playback and navigation requests
    signal auditionRequested()
    signal prevRequested()
    signal nextRequested()
    signal playAudioRequested()
    signal playVideoRequested()

    function acceptCurrent() {
        dialog.commitTranslation(txtTrans.text);
        dialog.commitRequested(txtTrans.text, true);
        dialog.nextRequested();
    }

    function insertOriginal() {
        txtTrans.text = dialog.originalText;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Original source dialogue line
        NativeGroupBox {
            title: qsTr("Original") + " (" + dialog.currentLineNumber + "/" + dialog.totalLines + ")"
            Layout.fillWidth: true
            implicitHeight: 90

            Rectangle {
                anchors.fill: parent
                color: "#ffffff"
                border.color: "#7f9db9"
                border.width: 1

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 4

                    TextArea {
                        text: dialog.originalText
                        readOnly: true
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        wrapMode: TextArea.Wrap
                        background: null
                    }
                }
            }
        }

        // Translated text input editor
        NativeGroupBox {
            title: qsTr("Translation")
            Layout.fillWidth: true
            implicitHeight: 90

            Rectangle {
                anchors.fill: parent
                color: "#ffffff"
                border.color: "#7f9db9"
                border.width: 1

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 4

                    TextArea {
                        id: txtTrans
                        text: dialog.translationText
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        wrapMode: TextArea.Wrap
                        background: null
                        focus: true
                        Keys.onReturnPressed: (event) => {
                            if (event.modifiers & Qt.ShiftModifier) {
                                event.accepted = false; // Allow newline with Shift+Enter
                            } else {
                                dialog.acceptCurrent();
                                event.accepted = true;
                            }
                        }
                    }
                }
            }
        }

        // Lower section: hotkey guide and playback controls
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // Keyboard shortcuts and navigation hints
            NativeGroupBox {
                title: qsTr("Keys")
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 2

                    GridLayout {
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 2
                        Layout.fillWidth: true

                        Text { text: qsTr("Accept changes:"); font.pixelSize: 11; color: "#555555" }
                        Text { text: "Enter"; font.pixelSize: 11; font.bold: true }

                        Text { text: qsTr("Line break:"); font.pixelSize: 11; color: "#555555" }
                        Text { text: "Shift+Enter"; font.pixelSize: 11; font.bold: true }

                        Text { text: qsTr("Previous line:"); font.pixelSize: 11; color: "#555555" }
                        Text { text: "Page Up"; font.pixelSize: 11; font.bold: true }

                        Text { text: qsTr("Next line:"); font.pixelSize: 11; color: "#555555" }
                        Text { text: "Page Down"; font.pixelSize: 11; font.bold: true }

                        Text { text: qsTr("Insert original text:"); font.pixelSize: 11; color: "#555555" }
                        Text { text: "Alt+Enter"; font.pixelSize: 11; font.bold: true }
                    }

                    Item { Layout.fillHeight: true }

                    NativeCheckBox {
                        text: qsTr("Enable &preview")
                        checked: true
                    }
                }
            }

            // Audio/video playback and copy commands
            NativeGroupBox {
                title: qsTr("Actions")
                Layout.preferredWidth: 140
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    NativeButton {
                        text: qsTr("&Play Audio")
                        Layout.fillWidth: true
                        onClicked: dialog.playAudioRequested()
                    }
                    NativeButton {
                        text: qsTr("Play &Video")
                        Layout.fillWidth: true
                        onClicked: dialog.playVideoRequested()
                    }
                    NativeButton {
                        text: qsTr("&Insert original")
                        Layout.fillWidth: true
                        onClicked: dialog.insertOriginal()
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
                text: qsTr("OK"); isDefault: true
                Layout.preferredWidth: 75
                onClicked: dialog.acceptCurrent()
            }

            NativeButton {
                text: qsTr("Cancel"); Layout.preferredWidth: 75
                onClicked: dialog.close()
            }

            NativeButton {
                text: qsTr("Help"); Layout.preferredWidth: 75
                onClicked: {
                    Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Translation_Assistant/")
                }
            }
        }
    }
}
