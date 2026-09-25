// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick

// Group box container with etched outline, top-aligned title label mask,
// and automatic layout sizing.
Item {
    id: groupBoxRoot
    property string title: ""
    default property alias content: container.data

    implicitWidth: container.childrenRect.width > 0 ? (container.childrenRect.width + 16) : Math.max(120, titleLabel.implicitWidth + 24)
    implicitHeight: container.childrenRect.height > 0 ? (container.childrenRect.height + 24) : 80

    // Group box outline
    Rectangle {
        id: borderRect
        anchors.fill: parent
        anchors.topMargin: 7
        color: "transparent"
        border.color: "#d0d0d0"
        border.width: 1
        radius: 0
    }

    // Header label covering the top border line
    Rectangle {
        x: 8
        y: 0
        height: titleLabel.implicitHeight
        width: titleLabel.implicitWidth + 8
        color: "#f0f0f0" // Masks the border line behind it

        Text {
            id: titleLabel
            anchors.centerIn: parent
            text: groupBoxRoot.title
            font.pixelSize: 12
            font.family: uiTheme.uiFont
            renderType: Text.NativeRendering
            color: "#000000"
        }
    }

    // Inner container for user components
    Item {
        id: container
        anchors.fill: parent
        anchors.topMargin: 16
        anchors.bottomMargin: 8
        anchors.leftMargin: 8
        anchors.rightMargin: 8
    }
}
