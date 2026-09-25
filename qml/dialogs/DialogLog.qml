// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogLog: Runtime diagnostics window mirroring the in-memory Qt log ring
// buffer (upstream Help > Log Window). Shows the most recent 1000 messages
// captured by the application-wide message handler.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    isModal: false
    title: qsTr("Log Window")
    implicitWidth: 620
    implicitHeight: 420

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Live log tail bound to the C++ AppLogBuffer model
        ListView {
            id: logView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: (typeof appLogModel !== "undefined") ? appLogModel : null
           ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                width: logView.width
                height: Math.max(18, logText.implicitHeight + 4)
                color: (modelData !== undefined && String(modelData).indexOf("[W]") === 0) ? "#fff4e5"
                       : (String(modelData).indexOf("[C]") === 0 || String(modelData).indexOf("[F]") === 0) ? "#ffe5e5"
                       : "transparent"

                Text {
                    id: logText
                    anchors.fill: parent
                    anchors.leftMargin: 4
                    text: String(modelData)
                    font.pixelSize: 11
                    font.family: uiTheme.monoFont
                    renderType: Text.NativeRendering
                    color: "#333333"
                    wrapMode: Text.WrapAnywhere
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            NativeButton {
                text: qsTr("Clear")
                implicitWidth: 72
                onClicked: {
                    if (typeof appLog !== "undefined") {
                        appLog.clear();
                    }
                }
            }
            NativeButton {
                text: qsTr("Close")
                implicitWidth: 72
                onClicked: dialog.close()
            }
            Item { Layout.fillWidth: true }
        }
    }
}
