// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogSearchReplace: Interactive find and replace dialog for text, style, actor,
// and effect fields, supporting regex, case sensitivity, tag skipping, and scope restriction.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    property bool hasReplace: true
    title: hasReplace ? qsTr("Replace") : qsTr("Find")
    width: 620
    height: 330
    implicitWidth: 620
    implicitHeight: 330

    // Emitted when finding next occurrence of query string
    signal findRequested(string query, var options)

    // Emitted when replacing current occurrence and seeking next
    signal replaceRequested(string query, string replaceWith, var options)

    // Emitted when replacing all occurrences across target scope
    signal replaceAllRequested(string query, string replaceWith, var options)

    function getOptions() {
        return {
            matchCase: chkCase.checked,
            useRegex: chkRegex.checked,
            skipComments: chkSkipComments.checked,
            skipTags: chkSkipTags.checked,
            field: radFieldText.checked ? "text" : (radFieldStyle.checked ? "style" : (radFieldActor.checked ? "actor" : "effect")),
            selectedOnly: radSelectedRows.checked
        };
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Query parameters and execution actions
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            // Pattern inputs, match options, and syntax flags
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                GridLayout {
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4
                    Layout.fillWidth: true

                    Text { text: qsTr("Find what:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                    NativeComboBox {
                        id: cmbFind
                        Layout.fillWidth: true
                        editable: true
                    }

                    Text {
                        text: qsTr("Replace with:"); font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        visible: dialog.hasReplace
                    }
                    NativeComboBox {
                        id: cmbReplace
                        Layout.fillWidth: true
                        editable: true
                        visible: dialog.hasReplace
                    }
                }

                ColumnLayout {
                    spacing: 3
                    NativeCheckBox { id: chkCase; text: qsTr("Match &case") }
                    NativeCheckBox { id: chkRegex; text: qsTr("Use &regular expressions") }
                    NativeCheckBox { id: chkSkipComments; text: qsTr("&Skip Comments"); checked: true }
                    NativeCheckBox { id: chkSkipTags; text: qsTr("Skip &tags"); checked: true }
                }
            }

            // Search and replace command buttons
            ColumnLayout {
                spacing: 4
                Layout.alignment: Qt.AlignTop

                NativeButton {
                    text: qsTr("&Find next")
                    isDefault: true
                    Layout.preferredWidth: 90
                    onClicked: dialog.findRequested(cmbFind.editText, dialog.getOptions())
                }

                NativeButton {
                    text: qsTr("&Replace Next")
                    visible: dialog.hasReplace
                    Layout.preferredWidth: 90
                    onClicked: dialog.replaceRequested(cmbFind.editText, cmbReplace.editText, dialog.getOptions())
                }

                NativeButton {
                    text: qsTr("Replace &All")
                    visible: dialog.hasReplace
                    Layout.preferredWidth: 90
                    onClicked: dialog.replaceAllRequested(cmbFind.editText, cmbReplace.editText, dialog.getOptions())
                }

                NativeButton {
                    text: qsTr("Cancel"); Layout.preferredWidth: 90
                    onClicked: dialog.close()
                }
            }
        }

        // Target attribute field and row scope selectors
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            NativeGroupBox {
                title: qsTr("In Field")
                Layout.preferredWidth: 380
                Layout.fillWidth: true
                implicitHeight: 56

                RowLayout {
                    anchors.fill: parent
                    spacing: 6

                    NativeRadioButton { id: radFieldText; text: qsTr("&Text"); checked: true }
                    NativeRadioButton { id: radFieldStyle; text: qsTr("&Style") }
                    NativeRadioButton { id: radFieldActor; text: qsTr("Act&or") }
                    NativeRadioButton { id: radFieldEffect; text: qsTr("E&ffect") }
                }
            }

            NativeGroupBox {
                title: qsTr("Limit to")
                Layout.preferredWidth: 210
                implicitHeight: 56

                RowLayout {
                    anchors.fill: parent
                    spacing: 6

                    NativeRadioButton { id: radAllRows; text: qsTr("All &lines"); checked: true }
                    NativeRadioButton { id: radSelectedRows; text: qsTr("Selected &rows") }
                }
            }
        }
    }
}
