// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../controls"

// Preferences page: Default style catalog mappings for newly created or imported subtitles.
Flickable {
    id: root
    contentHeight: mainLayout.implicitHeight
    boundsBehavior: Flickable.StopAtBounds

    signal changed()

    function restoreDefaults() {
        cmbDefASS.currentIndex = 0;
        cmbDefMicroDVD.currentIndex = 0;
        cmbDefSRT.currentIndex = 0;
        cmbDefTTXT.currentIndex = 0;
        cmbDefTXT.currentIndex = 0;
        root.changed();
    }

    function savePreferences() {
        // Persist default style assignments
    }

    ColumnLayout {
        id: mainLayout
        width: root.width - 8
        spacing: 8

        NativeGroupBox {
            title: qsTr("Default style catalogs")
            Layout.fillWidth: true
            implicitHeight: 250

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                Text {
                    text: qsTr("The chosen style catalogs will be loaded when you start a new file or import files in the various formats.\n\nYou can set up style catalogs in the Style Manager.")
                    font.pixelSize: 11
                    font.family: uiTheme.uiFont
                    color: "#333333"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Item { height: 6 }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("New files") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 140 }
                    NativeComboBox { id: cmbDefASS; Layout.fillWidth: true; model: ["Default"]; currentIndex: 0; onCurrentIndexChanged: root.changed() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("MicroDVD import") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 140 }
                    NativeComboBox { id: cmbDefMicroDVD; Layout.fillWidth: true; model: ["Default"]; currentIndex: 0; onCurrentIndexChanged: root.changed() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("SRT import") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 140 }
                    NativeComboBox { id: cmbDefSRT; Layout.fillWidth: true; model: ["Default"]; currentIndex: 0; onCurrentIndexChanged: root.changed() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("TTXT import") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 140 }
                    NativeComboBox { id: cmbDefTTXT; Layout.fillWidth: true; model: ["Default"]; currentIndex: 0; onCurrentIndexChanged: root.changed() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Plain text import") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.preferredWidth: 140 }
                    NativeComboBox { id: cmbDefTXT; Layout.fillWidth: true; model: ["Default"]; currentIndex: 0; onCurrentIndexChanged: root.changed() }
                }
            }
        }
    }
}
