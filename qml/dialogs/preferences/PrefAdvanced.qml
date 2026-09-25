// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../controls"

// Preferences page: Advanced configuration notice and risk disclaimers.
Flickable {
    id: root
    contentHeight: mainLayout.implicitHeight
    boundsBehavior: Flickable.StopAtBounds

    signal changed()

    function restoreDefaults() {
        // No mutable options in base advanced page
        root.changed();
    }

    function savePreferences() {
    }

    ColumnLayout {
        id: mainLayout
        width: root.width - 8
        spacing: 8

        NativeGroupBox {
            title: qsTr("General")
            Layout.fillWidth: true
            implicitHeight: 120

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                Text {
                    text: qsTr("Changing these settings might result in bugs and/or crashes. Do not touch these unless you know what you're doing.")
                    font.bold: true
                    font.pixelSize: 13
                    font.family: uiTheme.uiFont
                    color: "#b71c1c"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
    }
}
