// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls

// Thin horizontal divider for menu item groups, aligned with the icon gutter.
MenuSeparator {
    id: sep
    implicitHeight: 5
    topPadding: 2
    bottomPadding: 2
    leftPadding: 0
    rightPadding: 0

    contentItem: Item {
        anchors.fill: parent
        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 28
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            height: 1
            color: "#e5e5e5"
        }
    }
}
