// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls

// Standard push button control with Windows desktop metrics, default-button
// accent border, mnemonic stripping, and native text rendering.
Button {
    id: btn
    property bool isDefault: false
    implicitHeight: 23
    implicitWidth: Math.max(75, contentText.implicitWidth + 16)
    leftPadding: 8
    rightPadding: 8
    topPadding: 2
    bottomPadding: 2

    contentItem: Text {
        id: contentText
        text: btn.text.replace(/&/g, "")
        font.pixelSize: 12
        font.family: uiTheme.uiFont
        renderType: Text.NativeRendering
        color: btn.enabled ? "#000000" : "#838383"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 0
        border.width: 1
        border.color: {
            if (!btn.enabled) return "#bfbfbf";
            if (btn.pressed) return "#005499";
            if (btn.hovered) return "#0078d7";
            if (btn.isDefault || btn.activeFocus) return "#0078d7";
            return "#adadad";
        }
        color: {
            if (!btn.enabled) return "#cccccc";
            if (btn.pressed) return "#cce4f7";
            if (btn.hovered) return "#e5f1fb";
            return "#e1e1e1";
        }

        // Inner default accent border
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            color: "transparent"
            border.color: (btn.isDefault && btn.enabled && !btn.hovered && !btn.pressed) ? "#0078d7" : "transparent"
            border.width: 1
            visible: btn.isDefault
        }
    }
}
