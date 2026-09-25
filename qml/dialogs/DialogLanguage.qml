// Copyright (c) 2005 - 2026, Aegisub Project & Contributors
// SPDX-License-Identifier: BSD-3-Clause

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

// DialogLanguage: Language selection dialog faithfully matching the original
// Aegisub wxSingleChoiceDialog for switching UI localization locales.
NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Language")
    iconSource: "../../assets/icons_native/languages_menu_16.png"
    implicitWidth: 320
    implicitHeight: 360
    width: 320
    height: 360

    signal languageSelected(string langCode)

    property var languages: [
        { code: "zh_CN", name: "简体中文 (Simplified Chinese)" },
        { code: "zh_TW", name: "繁體中文 (Traditional Chinese)" },
        { code: "en_US", name: "English" },
        { code: "ja",    name: "日本語 (Japanese)" },
        { code: "ko",    name: "한국어 (Korean)" },
        { code: "fr_FR", name: "Français (French)" },
        { code: "de",    name: "Deutsch (German)" },
        { code: "ru",    name: "Русский (Russian)" },
        { code: "es",    name: "Español (Spanish)" }
    ]

    function syncCurrentSelection() {
        var curCode = (typeof languageManager !== "undefined" && languageManager) ?
                      languageManager.currentLanguage : "zh_CN";
        for (var i = 0; i < langListModel.count; ++i) {
            if (langListModel.get(i).code === curCode) {
                langListView.currentIndex = i;
                langListView.positionViewAtIndex(i, ListView.Contain);
                return;
            }
        }
        langListView.currentIndex = 0;
    }

    function acceptLanguage() {
        if (langListView.currentIndex >= 0 && langListView.currentIndex < langListModel.count) {
            var selectedCode = langListModel.get(langListView.currentIndex).code;
            dialog.languageSelected(selectedCode);
            if (typeof languageManager !== "undefined" && languageManager) {
                languageManager.setLanguage(selectedCode);
            }
        }
        dialog.close();
    }

    ListModel {
        id: langListModel
    }

    Component.onCompleted: {
        langListModel.clear();
        for (var i = 0; i < dialog.languages.length; ++i) {
            langListModel.append(dialog.languages[i]);
        }
        syncCurrentSelection();
    }

    onOpened: {
        syncCurrentSelection();
        langListView.forceActiveFocus();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // Prompt message
        Text {
            text: qsTr("Please choose a language:")
            font.pixelSize: 12
            font.family: uiTheme.uiFont
            color: "#000000"
            renderType: Text.NativeRendering
        }

        // Single-choice language list container
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#828790"
            border.width: 1

            ListView {
                id: langListView
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                focus: true
                boundsBehavior: Flickable.StopAtBounds
                model: langListModel

                ScrollBar.vertical: ScrollBar {
                    id: vbar
                    active: true
                    policy: ScrollBar.AsNeeded
                }

                delegate: Rectangle {
                    id: itemDelegate
                    width: langListView.width
                    height: 24
                    property bool isSelected: langListView.currentIndex === index
                    color: isSelected ? "#0078d7" : (itemMouse.containsMouse ? "#e5f3ff" : "transparent")

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        text: model.name
                        color: itemDelegate.isSelected ? "#ffffff" : "#000000"
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        elide: Text.ElideRight
                        renderType: Text.NativeRendering
                    }

                    MouseArea {
                        id: itemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            langListView.currentIndex = index;
                            langListView.forceActiveFocus();
                        }
                        onDoubleClicked: {
                            langListView.currentIndex = index;
                            dialog.acceptLanguage();
                        }
                    }
                }

                Keys.onUpPressed: {
                    if (currentIndex > 0) currentIndex--;
                }
                Keys.onDownPressed: {
                    if (currentIndex < count - 1) currentIndex++;
                }
                Keys.onReturnPressed: dialog.acceptLanguage()
                Keys.onEnterPressed: dialog.acceptLanguage()
            }
        }

        // Bottom dialog action buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            NativeButton {
                text: qsTr("OK")
                isDefault: true
                Layout.preferredWidth: 75
                onClicked: dialog.acceptLanguage()
            }

            NativeButton {
                text: qsTr("Cancel")
                Layout.preferredWidth: 75
                onClicked: dialog.close()
            }
        }
    }
}
