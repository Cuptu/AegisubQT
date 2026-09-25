// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogStylingAssistant: Rapid line-by-line subtitle styling assistant providing
// keyboard-driven style assignment, line navigation, and audio/video preview playback.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    title: qsTr("Styling Assistant")
    iconSource: "../../assets/icons_native/styling_toolbutton_16.png"
    implicitWidth: 540
    implicitHeight: 450

    property int currentLineNumber: 1
    property string currentLineTime: "0:00:01.00 - 0:00:04.00"
    property string currentStyle: "Default"
    property string currentText: qsTr("Current subtitle line text.")

    property var availableStyles: (typeof nativeSubtitleModel !== "undefined" && nativeSubtitleModel && nativeSubtitleModel.styleNames.length > 0) ? nativeSubtitleModel.styleNames : ["Default"]

    // Emitted when selected style is applied to current dialogue row
    signal styleApplied(string styleName)

    // Navigation and playback signals
    signal prevRequested()
    signal nextRequested()
    signal playAudioRequested()
    signal playVideoRequested()

    function acceptCurrent() {
        dialog.styleApplied(txtStyleName.text);
        dialog.nextRequested();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Current line metadata and subtitle text display
        NativeGroupBox {
            title: qsTr("Current line") + " (#" + dialog.currentLineNumber + " [" + dialog.currentLineTime + "])"
            Layout.fillWidth: true
            implicitHeight: 80

            Rectangle {
                anchors.fill: parent
                color: "#ffffff"
                border.color: "#7f9db9"
                border.width: 1

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 4

                    TextArea {
                        text: dialog.currentText
                        readOnly: true
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        wrapMode: TextArea.Wrap
                        background: null
                    }
                }
            }
        }

        // Lower section: style catalog on left, keybindings and controls on right
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // Available script styles list
            NativeGroupBox {
                title: qsTr("Styles available")
                Layout.preferredWidth: 160
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    color: "#ffffff"
                    border.color: "#7f9db9"
                    border.width: 1

                    ListView {
                        id: lvStyles
                        anchors.fill: parent
                        anchors.margins: 1
                        clip: true
                        model: dialog.availableStyles

                        delegate: Rectangle {
                            width: lvStyles.width
                            height: 22
                            color: txtStyleName.text === modelData ? "#3399ff" : (stMouse.containsMouse ? "#e5f1fb" : "transparent")

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData
                                font.pixelSize: 11
                                font.family: uiTheme.uiFont
                                renderType: Text.NativeRendering
                                color: txtStyleName.text === modelData ? "#ffffff" : "#000000"
                            }

                            MouseArea {
                                id: stMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: txtStyleName.text = modelData
                                onDoubleClicked: {
                                    txtStyleName.text = modelData;
                                    dialog.acceptCurrent();
                                }
                            }
                        }
                    }
                }
            }

            // Style assignment input, hotkey hints, and playback actions
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 6

                NativeGroupBox {
                    title: qsTr("Set style")
                    Layout.fillWidth: true
                    implicitHeight: 52

                    RowLayout {
                        anchors.fill: parent
                        NativeTextBox {
                            id: txtStyleName
                            text: dialog.currentStyle
                            Layout.fillWidth: true
                            Keys.onReturnPressed: dialog.acceptCurrent()
                        }
                    }
                }

                NativeGroupBox {
                    title: qsTr("Keys")
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 3

                        GridLayout {
                            columns: 2
                            columnSpacing: 16
                            rowSpacing: 2
                            Layout.fillWidth: true

                            Text { text: qsTr("Accept changes:"); font.pixelSize: 11; color: "#555555" }
                            Text { text: "Enter"; font.pixelSize: 11; font.bold: true }

                            Text { text: qsTr("Previous line:"); font.pixelSize: 11; color: "#555555" }
                            Text { text: "Page Up"; font.pixelSize: 11; font.bold: true }

                            Text { text: qsTr("Next line:"); font.pixelSize: 11; color: "#555555" }
                            Text { text: "Page Down"; font.pixelSize: 11; font.bold: true }

                            Text { text: qsTr("Play audio:"); font.pixelSize: 11; color: "#555555" }
                            Text { text: "F1"; font.pixelSize: 11; font.bold: true }

                            Text { text: qsTr("Play video:"); font.pixelSize: 11; color: "#555555" }
                            Text { text: "F2"; font.pixelSize: 11; font.bold: true }
                        }

                        Item { Layout.fillHeight: true }

                        NativeCheckBox {
                            text: qsTr("Scroll to current line")
                            checked: true
                        }
                    }
                }

                NativeGroupBox {
                    title: qsTr("Actions")
                    Layout.fillWidth: true
                    implicitHeight: 52

                    RowLayout {
                        anchors.fill: parent
                        spacing: 8

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
                    Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Styling_Assistant/")
                }
            }
        }
    }
}
