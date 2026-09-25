// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogSpellChecker: Spell verification dialog providing dictionary lookup,
// candidate replacement suggestions, language selection, and user dictionary word addition.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    title: qsTr("Spell Checker")
    iconSource: "../../assets/icons_native/spellcheck_toolbutton_16.png"
    implicitWidth: 460
    implicitHeight: 330

    // Emitted when user accepts a replacement word
    signal replaceRequested(string origWord, string newWord, bool replaceAll)

    // Emitted when user ignores a reported word
    signal ignoreRequested(string origWord, bool ignoreAll)

    // Emitted when user adds a word to custom user dictionary
    signal addWordRequested(string word)
    signal removeWordRequested(string word)

    property string misspelledWord: "teh"
    property var suggestions: ["the", "ten", "tea", "tech"]

    function onReplace(replaceAll) {
        dialog.replaceRequested(dialog.misspelledWord, txtReplace.text, replaceAll);
        // Advance demonstration state for interactive UI verification
        dialog.misspelledWord = "recieve";
        txtReplace.text = "receive";
        dialog.suggestions = ["receive", "receipt", "relieve"];
    }

    function onIgnore(ignoreAll) {
        dialog.ignoreRequested(dialog.misspelledWord, ignoreAll);
        dialog.misspelledWord = "colour";
        txtReplace.text = "color";
        dialog.suggestions = ["color", "colon", "cooler"];
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Misspelled word display and replacement input
        GridLayout {
            columns: 2
            columnSpacing: 8
            rowSpacing: 4
            Layout.fillWidth: true

            Text { text: qsTr("Misspelled word:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
            NativeTextBox {
                id: txtOrig
                text: dialog.misspelledWord
                readOnly: true
                Layout.fillWidth: true
            }

            Text { text: qsTr("Replace with:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
            NativeTextBox {
                id: txtReplace
                text: dialog.suggestions[0] || ""
                Layout.fillWidth: true
            }
        }

        // Suggestions panel and dictionary actions
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // Candidate suggestions list and dictionary language selector
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#ffffff"
                    border.color: "#7f9db9"
                    border.width: 1

                    ListView {
                        id: lvSuggestions
                        anchors.fill: parent
                        anchors.margins: 1
                        clip: true
                        model: dialog.suggestions

                        delegate: Rectangle {
                            width: lvSuggestions.width
                            height: 22
                            color: lvSuggestions.currentIndex === index ? "#3399ff" : (sugMouse.containsMouse ? "#e5f1fb" : "transparent")

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData
                                font.pixelSize: 11
                                font.family: uiTheme.uiFont
                                renderType: Text.NativeRendering
                                color: lvSuggestions.currentIndex === index ? "#ffffff" : "#000000"
                            }

                            MouseArea {
                                id: sugMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    lvSuggestions.currentIndex = index;
                                    txtReplace.text = modelData;
                                }
                                onDoubleClicked: {
                                    lvSuggestions.currentIndex = index;
                                    txtReplace.text = modelData;
                                    dialog.onReplace(false);
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text { text: qsTr("Language:"); font.pixelSize: 11 }
                    NativeComboBox {
                        id: cmbLang
                        Layout.fillWidth: true
                        model: ["en_US (English - United States)", "en_GB (English - Great Britain)", "zh_CN (Chinese - Simplified)"]
                        currentIndex: 0
                    }
                }
            }

            // Spell check command actions
            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                spacing: 4

                NativeButton {
                    text: qsTr("&Replace"); isDefault: true
                    Layout.preferredWidth: 95
                    onClicked: dialog.onReplace(false)
                }
                NativeButton {
                    text: qsTr("Replace &All"); Layout.preferredWidth: 95
                    onClicked: dialog.onReplace(true)
                }
                NativeButton {
                    text: qsTr("&Ignore"); Layout.preferredWidth: 95
                    onClicked: dialog.onIgnore(false)
                }
                NativeButton {
                    text: qsTr("Ignore A&ll"); Layout.preferredWidth: 95
                    onClicked: dialog.onIgnore(true)
                }
                NativeButton {
                    text: qsTr("Add to &Dictionary"); Layout.preferredWidth: 95
                    onClicked: dialog.addWordRequested(txtOrig.text)
                }
                NativeButton {
                    text: qsTr("Remove from &Dictionary"); Layout.preferredWidth: 95
                    enabled: false
                    onClicked: dialog.removeWordRequested(txtOrig.text)
                }
                Item { height: 6 }
                NativeButton {
                    text: qsTr("Cancel"); Layout.preferredWidth: 95
                    onClicked: dialog.close()
                }
            }
        }
    }
}
