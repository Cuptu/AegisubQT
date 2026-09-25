// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../controls"

// Preferences page: Automation script engine autoload paths and trace verbosity.
Flickable {
    id: root
    contentHeight: mainLayout.implicitHeight
    boundsBehavior: Flickable.StopAtBounds

    signal changed()

    function restoreDefaults() {
        txtBasePath.text = "?user/automation/autoload";
        txtIncludePath.text = "?data/automation/include";
        txtAutoloadPath.text = "?user/automation/autoload";
        cmbTraceLevel.currentIndex = 3;
        cmbAutoreload.currentIndex = 3;
        root.changed();
    }

    function savePreferences() {
        // Persist automation preferences
    }

    ColumnLayout {
        id: mainLayout
        width: root.width - 8
        spacing: 8

        NativeGroupBox {
            title: qsTr("General")
            Layout.fillWidth: true
            implicitHeight: colAuto.implicitHeight + 28

            ColumnLayout {
                id: colAuto
                anchors.fill: parent
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Base path") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 90 }
                    NativeTextBox { id: txtBasePath; text: "?user/automation/autoload"; Layout.fillWidth: true; onTextChanged: root.changed() }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Include path") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 90 }
                    NativeTextBox { id: txtIncludePath; text: "?data/automation/include"; Layout.fillWidth: true; onTextChanged: root.changed() }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Auto-load path") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 90 }
                    NativeTextBox { id: txtAutoloadPath; text: "?user/automation/autoload"; Layout.fillWidth: true; onTextChanged: root.changed() }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Trace level") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 90 }
                    NativeComboBox {
                        id: cmbTraceLevel
                        model: [qsTr("0: Fatal"), qsTr("1: Error"), qsTr("2: Warning"), qsTr("3: Hint"), qsTr("4: Debug"), qsTr("5: Trace")]
                        currentIndex: 3
                        Layout.fillWidth: true
                        onCurrentIndexChanged: root.changed()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Autoreload on Export") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 90 }
                    NativeComboBox {
                        id: cmbAutoreload
                        model: [qsTr("No scripts"), qsTr("Subtitle-local scripts"), qsTr("Global autoload scripts"), qsTr("All scripts")]
                        currentIndex: 3
                        Layout.fillWidth: true
                        onCurrentIndexChanged: root.changed()
                    }
                }
            }
        }
    }
}
