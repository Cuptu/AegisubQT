// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls

// Windows 10 styled compact white tooltip with subtle rounded corners and elevation drop shadow.
ToolTip {
    id: tip
    topPadding: 3
    bottomPadding: 3
    leftPadding: 6
    rightPadding: 6
    font.pixelSize: 12
    font.family: uiTheme.uiFont
    delay: 700
    timeout: 6000

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 80
            easing.type: Easing.OutQuad
        }
    }
    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: 50
            easing.type: Easing.InQuad
        }
    }

    contentItem: Text {
        text: tip.text
        font: tip.font
        color: "#575757"
        renderType: Text.NativeRendering
        verticalAlignment: Text.AlignVCenter
    }

    background: Item {
        // Soft elevation drop shadow on right and bottom
        Rectangle {
            x: 1; y: 2
            width: parent.width + 2; height: parent.height + 2
            color: "#14000000"
            radius: 3
        }
        Rectangle {
            x: 0; y: 1
            width: parent.width + 1; height: parent.height + 1
            color: "#20000000"
            radius: 3
        }

        // Crisp compact white container with thin light gray border and rounded corners
        Rectangle {
            anchors.fill: parent
            color: "#ffffff"
            border.color: "#d0d0d0"
            border.width: 1
            radius: 3
        }
    }
}
