// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../controls"

// Preferences page: Keyboard shortcut mappings and user hotkey overrides.
Item {
    id: root

    signal changed()

    property int hotkeySelectedIndex: 0
    property ListModel hotkeysModel: ListModel {
        ListElement { key: "Ctrl+O"; cmd: "subtitle/open"; desc: "Open subtitles" }
        ListElement { key: "Ctrl+S"; cmd: "subtitle/save"; desc: "Save subtitles" }
        ListElement { key: "Ctrl+Z"; cmd: "edit/undo"; desc: "Undo" }
        ListElement { key: "Ctrl+Y"; cmd: "edit/redo"; desc: "Redo" }
        ListElement { key: "Space"; cmd: "audio/play/selection"; desc: "Play audio selection" }
        ListElement { key: "Ctrl+Space"; cmd: "video/play"; desc: "Play/Pause video" }
        ListElement { key: "Ctrl+Shift+T"; cmd: "time/shift"; desc: "Shift times" }
    }

    function restoreDefaults() {
        hotkeysModel.clear();
        hotkeysModel.append({ key: "Ctrl+O", cmd: "subtitle/open", desc: "Open subtitles" });
        hotkeysModel.append({ key: "Ctrl+S", cmd: "subtitle/save", desc: "Save subtitles" });
        hotkeysModel.append({ key: "Ctrl+Z", cmd: "edit/undo", desc: "Undo" });
        hotkeysModel.append({ key: "Ctrl+Y", cmd: "edit/redo", desc: "Redo" });
        hotkeysModel.append({ key: "Space", cmd: "audio/play/selection", desc: "Play audio selection" });
        hotkeysModel.append({ key: "Ctrl+Space", cmd: "video/play", desc: "Play/Pause video" });
        hotkeysModel.append({ key: "Ctrl+Shift+T", cmd: "time/shift", desc: "Shift times" });
        hotkeySelectedIndex = 0;
        root.changed();
    }

    function savePreferences() {
        // Persist hotkey mappings
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            NativeTextBox {
                placeholderText: qsTr("Search...")
                Layout.fillWidth: true
            }
            NativeButton {
                text: qsTr("&New")
                Layout.preferredWidth: 65
                onClicked: {
                    root.hotkeysModel.append({ key: "F" + (root.hotkeysModel.count + 1), cmd: "custom/macro", desc: qsTr("User Shortcut") });
                    root.hotkeySelectedIndex = root.hotkeysModel.count - 1;
                    root.changed();
                }
            }
            NativeButton {
                text: qsTr("&Edit")
                Layout.preferredWidth: 65
                enabled: root.hotkeySelectedIndex >= 0 && root.hotkeySelectedIndex < root.hotkeysModel.count
                onClicked: root.changed()
            }
            NativeButton {
                text: qsTr("&Delete")
                Layout.preferredWidth: 65
                enabled: root.hotkeySelectedIndex >= 0 && root.hotkeySelectedIndex < root.hotkeysModel.count
                onClicked: {
                    if (root.hotkeySelectedIndex >= 0 && root.hotkeySelectedIndex < root.hotkeysModel.count) {
                        root.hotkeysModel.remove(root.hotkeySelectedIndex);
                        root.hotkeySelectedIndex = Math.max(0, root.hotkeySelectedIndex - 1);
                        root.changed();
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#7f9db9"
            border.width: 1

            ListView {
                anchors.fill: parent
                anchors.margins: 2
                clip: true
                model: root.hotkeysModel
                delegate: Rectangle {
                    width: parent ? parent.width : 0
                    height: 22
                    color: root.hotkeySelectedIndex === index ? "#3399ff" : (index % 2 === 0 ? "#ffffff" : "#f8f9fa")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 8

                        Text { text: model.key; font.bold: true; font.pixelSize: 11; color: root.hotkeySelectedIndex === index ? "#ffffff" : "#000000"; Layout.preferredWidth: 80 }
                        Text { text: model.cmd; font.pixelSize: 11; color: root.hotkeySelectedIndex === index ? "#e0f0ff" : "#0066cc"; Layout.preferredWidth: 120 }
                        Text { text: model.desc; font.pixelSize: 11; color: root.hotkeySelectedIndex === index ? "#ffffff" : "#444444"; Layout.fillWidth: true; elide: Text.ElideRight }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.hotkeySelectedIndex = index
                    }
                }
            }
        }
    }
}
