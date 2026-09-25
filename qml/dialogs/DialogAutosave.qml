// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogAutosave: Recovery browser for autosaved project snapshots and backup versions.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Open Autosave File")
    iconSource: "../../assets/icons_native/open_toolbutton_16.png"
    implicitWidth: 650
    implicitHeight: 380

    // Emitted when a snapshot file path is selected for recovery
    signal fileSelected(string fullPath)
    signal restoreAutosaveRequested(string path)

    // Emitted to broadcast informational status notifications
    signal statusMessage(string msg)

    // Snapshot inventory grouped by original document, sourced from the real
    // autosave/backup directories (AegisubCoreBridge::listBackupFiles).
    property var filesList: []

    property int selectedFileIndex: 0
    property int selectedVersionIndex: 0

    function rebuildFilesList() {
        var flat = (typeof aegisubCore !== "undefined" && aegisubCore) ? aegisubCore.listBackupFiles() : [];
        var groups = {};
        var order = [];
        for (var i = 0; i < flat.length; ++i) {
            var e = flat[i];
            var base = String(e.path).split('/').pop();
            // "stem.YYYYMMDD-hhmmss.AUTOSAVE.ass" -> "stem"
            var m = base.match(/^(.+)\.\d{8}-\d{6}\.(AUTOSAVE|BACKUP)\.ass$/);
            var name = m ? m[1] : base;
            if (!groups[name]) {
                groups[name] = { name: name, versions: [] };
                order.push(groups[name]);
            }
            groups[name].versions.push({
                time: e.time + " [" + String(e.kind).toUpperCase() + "]",
                path: e.path
            });
        }
        filesList = order;
        selectedFileIndex = 0;
        selectedVersionIndex = 0;
    }

    onVisibleChanged: {
        if (visible) {
            rebuildFilesList();
        }
    }

    function openSelected() {
        if (selectedFileIndex >= 0 && selectedFileIndex < filesList.length) {
            var file = filesList[selectedFileIndex];
            if (selectedVersionIndex >= 0 && selectedVersionIndex < file.versions.length) {
                var p = file.versions[selectedVersionIndex].path;
                dialog.fileSelected(p);
                dialog.restoreAutosaveRequested(p);
                dialog.close();
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // Autosaved file list
            NativeGroupBox {
                title: qsTr("Files")
                Layout.fillWidth: true
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    color: "#ffffff"
                    border.color: "#7f9db9"
                    border.width: 1

                    ListView {
                        id: lvFiles
                        anchors.fill: parent
                        anchors.margins: 1
                        clip: true
                        model: dialog.filesList

                        delegate: Rectangle {
                            width: lvFiles.width
                            height: 22
                            color: dialog.selectedFileIndex === index ? "#3399ff" : (fMouse.containsMouse ? "#e5f1fb" : "transparent")

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.name
                                font.pixelSize: 11
                                font.family: uiTheme.uiFont
                                renderType: Text.NativeRendering
                                color: dialog.selectedFileIndex === index ? "#ffffff" : "#000000"
                            }

                            MouseArea {
                                id: fMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    dialog.selectedFileIndex = index;
                                    dialog.selectedVersionIndex = 0;
                                }
                            }
                        }
                    }
                }
            }

            // Backup snapshot versions for selected file
            NativeGroupBox {
                title: qsTr("Versions")
                Layout.fillWidth: true
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    color: "#ffffff"
                    border.color: "#7f9db9"
                    border.width: 1

                    ListView {
                        id: lvVersions
                        anchors.fill: parent
                        anchors.margins: 1
                        clip: true
                        model: dialog.filesList[dialog.selectedFileIndex] ? dialog.filesList[dialog.selectedFileIndex].versions : []

                        delegate: Rectangle {
                            width: lvVersions.width
                            height: 22
                            color: dialog.selectedVersionIndex === index ? "#3399ff" : (vMouse.containsMouse ? "#e5f1fb" : "transparent")

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.time
                                font.pixelSize: 11
                                font.family: uiTheme.uiFont
                                renderType: Text.NativeRendering
                                color: dialog.selectedVersionIndex === index ? "#ffffff" : "#000000"
                            }

                            MouseArea {
                                id: vMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: dialog.selectedVersionIndex = index
                                onDoubleClicked: {
                                    dialog.selectedVersionIndex = index;
                                    dialog.openSelected();
                                }
                            }
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
                text: qsTr("Open")
                isDefault: true
                Layout.preferredWidth: 75
                onClicked: dialog.openSelected()
            }

            NativeButton {
                text: qsTr("Cancel"); Layout.preferredWidth: 75
                onClicked: dialog.close()
            }
        }
    }
}
