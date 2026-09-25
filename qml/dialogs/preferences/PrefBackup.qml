// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../../controls"

// Preferences page: Automatic subtitle file saving and backup archival intervals.
Flickable {
    id: root
    contentHeight: mainLayout.implicitHeight
    boundsBehavior: Flickable.StopAtBounds

    signal changed()

    function restoreDefaults() {
        chkAutosaveEnable.checked = true;
        spinAutosaveInterval.value = 60;
        txtAutosavePath.text = "?user/autosave";
        chkAutosaveAfterChange.checked = false;
        chkBackupEnable.checked = true;
        txtAutobackupPath.text = "?user/autobackup";
        root.changed();
    }

    function savePreferences() {
        // Persist backup preferences
    }

    property var activeFolderTarget: null
    FolderDialog {
        id: backupFolderDialog
        title: qsTr("Select Directory")
        onAccepted: {
            if (root.activeFolderTarget) {
                root.activeFolderTarget.text = selectedFolder.toString().replace("file:///", "");
                root.changed();
            }
        }
    }

    ColumnLayout {
        id: mainLayout
        width: root.width - 8
        spacing: 8

        NativeGroupBox {
            title: qsTr("Automatic Save")
            Layout.fillWidth: true
            implicitHeight: colAutosave.implicitHeight + 28

            ColumnLayout {
                id: colAutosave
                anchors.fill: parent
                spacing: 4

                NativeCheckBox {
                    id: chkAutosaveEnable
                    text: qsTr("Enable")
                    checked: true
                    onCheckedChanged: root.changed()
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Interval in seconds") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.fillWidth: true }
                    NativeSpinBox {
                        id: spinAutosaveInterval
                        from: 1; to: 3600; value: 60
                        Layout.preferredWidth: 80
                        onValueModified: root.changed()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Path") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 50 }
                    NativeTextBox { id: txtAutosavePath; text: "?user/autosave"; Layout.fillWidth: true; onTextChanged: root.changed() }
                    NativeButton {
                        text: qsTr("Browse..."); Layout.preferredWidth: 65
                        onClicked: { root.activeFolderTarget = txtAutosavePath; backupFolderDialog.open(); }
                    }
                }

                NativeCheckBox {
                    id: chkAutosaveAfterChange
                    text: qsTr("Autosave after every change")
                    checked: false
                    onCheckedChanged: root.changed()
                }
            }
        }

        NativeGroupBox {
            title: qsTr("Automatic Backup")
            Layout.fillWidth: true
            implicitHeight: colBackup.implicitHeight + 28

            ColumnLayout {
                id: colBackup
                anchors.fill: parent
                spacing: 4

                NativeCheckBox {
                    id: chkBackupEnable
                    text: qsTr("Enable")
                    checked: true
                    onCheckedChanged: root.changed()
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Path") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 50 }
                    NativeTextBox { id: txtAutobackupPath; text: "?user/autobackup"; Layout.fillWidth: true; onTextChanged: root.changed() }
                    NativeButton {
                        text: qsTr("Browse..."); Layout.preferredWidth: 65
                        onClicked: { root.activeFolderTarget = txtAutobackupPath; backupFolderDialog.open(); }
                    }
                }
            }
        }
    }
}
