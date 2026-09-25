// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogAutomation: Automation 4 Lua script manager for registering, inspecting,
// reloading, and unloading global and document-local automation macros.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../controls"

NativeDialogFrame {
    id: dialog
    title: qsTr("Automation Manager")
    iconSource: "../../assets/icons_native/automation_toolbutton_16.png"
    implicitWidth: 640
    implicitHeight: 340

    // Emitted to broadcast informational status notifications
    signal statusMessage(string msg)

    property var scripts: (typeof automationManager !== "undefined" && automationManager.scripts && automationManager.scripts.length > 0) ? automationManager.scripts : [
        { isGlobal: true, name: "kara-templater", filename: "kara-templater.lua", desc: qsTr("Karaoke templater macro script (Automation 4 Lua)") },
        { isGlobal: true, name: "clean-info", filename: "clean-info.lua", desc: qsTr("Clean unused script headers and metadata") },
        { isGlobal: false, name: "custom-fade", filename: "fade.lua", desc: qsTr("Batch fade-in and fade-out tag generator") }
    ]

    property int selectedScriptIndex: 0

    FileDialog {
        id: fileDialogScript
        title: qsTr("Add Automation Script")
        nameFilters: ["Lua Scripts (*.lua)", "All Files (*.*)"]
        onAccepted: {
            if (typeof automationManager !== "undefined") {
                automationManager.addScript(selectedFile.toString(), false);
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Automation script table view
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#7f9db9"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Header row (Type, Name, Filename, Description)
                Rectangle {
                    Layout.fillWidth: true
                    height: 22
                    color: "#f0f0f0"

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: "#d0d0d0"
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 8

                        Text { text: qsTr("Type"); font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 35 }
                        Text { text: qsTr("Name"); font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 120 }
                        Text { text: qsTr("Filename"); font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 120 }
                        Text { text: qsTr("Description"); font.pixelSize: 11; font.bold: true; Layout.fillWidth: true }
                    }
                }

                // Script records: global autoloaded vs script-local macros
                ListView {
                    id: lvScripts
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: dialog.scripts
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        width: lvScripts.width
                        height: 22
                        color: dialog.selectedScriptIndex === index ? "#3399ff" : (index % 2 === 0 ? "#ffffff" : "#f8f9fa")

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            spacing: 8

                            Text {
                                text: modelData.isGlobal ? "G" : "L"
                                font.bold: true
                                font.pixelSize: 11
                                color: dialog.selectedScriptIndex === index ? "#ffffff" : (modelData.isGlobal ? "#0066cc" : "#2e7d32")
                                Layout.preferredWidth: 35
                            }
                            Text {
                                text: modelData.name
                                font.pixelSize: 11
                                color: dialog.selectedScriptIndex === index ? "#ffffff" : "#000000"
                                Layout.preferredWidth: 120
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.filename
                                font.pixelSize: 11
                                color: dialog.selectedScriptIndex === index ? "#ffffff" : "#666666"
                                Layout.preferredWidth: 120
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.desc || modelData.description || ""
                                font.pixelSize: 11
                                color: dialog.selectedScriptIndex === index ? "#ffffff" : "#444444"
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: dialog.selectedScriptIndex = index
                        }
                    }
                }
            }
        }

        // Script management actions
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            NativeButton {
                text: qsTr("&Add...")
                Layout.preferredWidth: 75
                onClicked: fileDialogScript.open()
            }
            NativeButton {
                text: qsTr("&Remove")
                Layout.preferredWidth: 65
                enabled: dialog.selectedScriptIndex >= 0 && dialog.scripts[dialog.selectedScriptIndex] && !dialog.scripts[dialog.selectedScriptIndex].isGlobal
                onClicked: {
                    if (dialog.selectedScriptIndex >= 0 && typeof automationManager !== "undefined") {
                        automationManager.removeScript(dialog.selectedScriptIndex);
                        dialog.selectedScriptIndex = Math.max(0, dialog.selectedScriptIndex - 1);
                    }
                }
            }
            NativeButton {
                text: qsTr("&Reload")
                Layout.preferredWidth: 85
                onClicked: {
                    if (dialog.selectedScriptIndex >= 0 && typeof automationManager !== "undefined") {
                        automationManager.reloadScript(dialog.selectedScriptIndex);
                        dialog.statusMessage(qsTr("Reloaded script"));
                    }
                }
            }
            NativeButton {
                text: qsTr("&Info...")
                Layout.preferredWidth: 85
                onClicked: {
                    if (dialog.selectedScriptIndex >= 0 && dialog.scripts[dialog.selectedScriptIndex]) {
                        var sc = dialog.scripts[dialog.selectedScriptIndex];
                        dialog.statusMessage(sc.name + " (" + (sc.version || "1.0") + ") by " + (sc.author || "Unknown") + " - " + (sc.description || sc.desc || ""));
                    }
                }
            }
            NativeButton {
                text: qsTr("Rescan &Autoload Dir")
                Layout.fillWidth: true
                onClicked: {
                    if (typeof automationManager !== "undefined") {
                        automationManager.scanAutoloadFolder();
                        dialog.statusMessage(qsTr("Rescanned autoload folder"));
                    }
                }
            }

            NativeButton {
                text: qsTr("Close"); isDefault: true
                Layout.preferredWidth: 65
                onClicked: dialog.close()
            }

            NativeButton {
                text: qsTr("Help")
                Layout.preferredWidth: 65
                onClicked: Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Automation/")
            }
        }
    }
}
