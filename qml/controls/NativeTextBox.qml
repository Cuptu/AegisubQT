// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls

// Single-line text input field with desktop focus highlights and native font rendering.
TextField {
    id: txt
    implicitHeight: 21
    font.pixelSize: 12
    font.family: uiTheme.uiFont
    renderType: Text.NativeRendering
    color: txt.enabled ? "#000000" : "#6d6d6d"
    selectedTextColor: "#ffffff"
    selectionColor: "#0078d7"
    leftPadding: 4
    rightPadding: 4
    topPadding: 1
    bottomPadding: 1
    verticalAlignment: Text.AlignVCenter

    background: Rectangle {
        radius: 0
        border.width: 1
        border.color: {
            if (!txt.enabled) return "#cccccc";
            if (txt.activeFocus) return "#0078d7";
            if (txt.hovered) return "#000000";
            return "#7a7a7a";
        }
        color: txt.enabled ? "#ffffff" : "#f0f0f0"
    }
}
