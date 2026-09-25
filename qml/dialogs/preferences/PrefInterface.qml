// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../../controls"

// Preferences page: Text edit box behavior, spellcheck dictionary paths, CPS metrics, and grid display.
Flickable {
    id: root
    contentHeight: mainLayout.implicitHeight
    boundsBehavior: Flickable.StopAtBounds

    signal changed()

    function restoreDefaults() {
        chkCallTips.checked = true;
        chkOverwriteTime.checked = false;
        chkShiftEnter.checked = true;
        chkSyntaxHighlight.checked = true;
        txtDictPath.text = "?data/dictionaries";
        spinMaxChars.value = 0;
        spinCpsWarning.value = 25;
        spinCpsError.value = 30;
        chkIgnoreWhitespace.checked = true;
        chkFocusGrid.checked = true;
        chkHighlightVisible.checked = true;
        chkHideOverrides.checked = false;
        root.changed();
    }

    function savePreferences() {
        // Persist interface preferences
    }

    FolderDialog {
        id: dictFolderDialog
        title: qsTr("Select Dictionaries Directory")
        onAccepted: {
            txtDictPath.text = selectedFolder.toString().replace("file:///", "");
            root.changed();
        }
    }

    ColumnLayout {
        id: mainLayout
        width: root.width - 8
        spacing: 8

        NativeGroupBox {
            title: qsTr("Edit Box")
            Layout.fillWidth: true
            implicitHeight: colEditBox.implicitHeight + 28

            ColumnLayout {
                id: colEditBox
                anchors.fill: parent
                spacing: 4

                NativeCheckBox { id: chkCallTips; text: qsTr("Enable call tips"); checked: true; onCheckedChanged: root.changed() }
                NativeCheckBox { id: chkOverwriteTime; text: qsTr("Overwrite in time boxes"); checked: false; onCheckedChanged: root.changed() }
                NativeCheckBox { id: chkShiftEnter; text: qsTr("Shift+Enter adds \\n"); checked: true; onCheckedChanged: root.changed() }
                NativeCheckBox { id: chkSyntaxHighlight; text: qsTr("Enable syntax highlighting"); checked: true; onCheckedChanged: root.changed() }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Dictionaries path") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 70 }
                    NativeTextBox { id: txtDictPath; text: "?data/dictionaries"; Layout.fillWidth: true; onTextChanged: root.changed() }
                    NativeButton {
                        text: qsTr("Browse..."); Layout.preferredWidth: 65
                        onClicked: dictFolderDialog.open()
                    }
                }
            }
        }

        NativeGroupBox {
            title: qsTr("Character Counter")
            Layout.fillWidth: true
            implicitHeight: colCharCount.implicitHeight + 28

            ColumnLayout {
                id: colCharCount
                anchors.fill: parent
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Maximum characters per line") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.fillWidth: true }
                    NativeSpinBox { id: spinMaxChars; from: 0; to: 1000; value: 0; Layout.preferredWidth: 80; onValueModified: root.changed() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Characters Per Second Warning Threshold") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.fillWidth: true }
                    NativeSpinBox { id: spinCpsWarning; from: 0; to: 1000; value: 25; Layout.preferredWidth: 80; onValueModified: root.changed() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Characters Per Second Error Threshold") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.fillWidth: true }
                    NativeSpinBox { id: spinCpsError; from: 0; to: 1000; value: 30; Layout.preferredWidth: 80; onValueModified: root.changed() }
                }
                NativeCheckBox { id: chkIgnoreWhitespace; text: qsTr("Ignore whitespace"); checked: true; onCheckedChanged: root.changed() }
            }
        }

        NativeGroupBox {
            title: qsTr("Grid")
            Layout.fillWidth: true
            implicitHeight: colGrid.implicitHeight + 28

            ColumnLayout {
                id: colGrid
                anchors.fill: parent
                spacing: 4

                NativeCheckBox { id: chkFocusGrid; text: qsTr("Focus grid on click"); checked: true; onCheckedChanged: root.changed() }
                NativeCheckBox { id: chkHighlightVisible; text: qsTr("Highlight visible subtitles"); checked: true; onCheckedChanged: root.changed() }
                NativeCheckBox { id: chkHideOverrides; text: qsTr("Hide overrides symbol"); checked: false; onCheckedChanged: root.changed() }
            }
        }
    }
}
