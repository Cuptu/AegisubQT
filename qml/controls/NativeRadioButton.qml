// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls

// Modern Windows styled radio button control with circular accent indicator and native text rendering.
RadioButton {
    id: rad
    padding: 0
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    implicitHeight: 22
    font.pixelSize: 12
    font.family: uiTheme.uiFont
    spacing: 7

    indicator: Rectangle {
        implicitWidth: 14
        implicitHeight: 14
        x: rad.leftPadding
        anchors.verticalCenter: parent.verticalCenter
        radius: 7
        border.width: 1
        border.color: {
            if (!rad.enabled) return "#cccccc";
            if (rad.checked) {
                if (rad.pressed) return "#005299";
                if (rad.hovered) return "#005fb8";
                return "#0067c0";
            }
            if (rad.hovered) return "#0067c0";
            return "#767676";
        }
        color: rad.enabled ? "#ffffff" : "#f0f0f0"

        Rectangle {
            anchors.centerIn: parent
            width: 6
            height: 6
            radius: 3
            color: {
                if (!rad.enabled) return "#838383";
                if (rad.pressed) return "#005299";
                if (rad.hovered) return "#005fb8";
                return "#0067c0";
            }
            visible: rad.checked
        }
    }

    contentItem: Text {
        leftPadding: rad.indicator.width + rad.spacing
        text: rad.text
        font: rad.font
        renderType: Text.NativeRendering
        color: rad.enabled ? "#000000" : "#838383"
        verticalAlignment: Text.AlignVCenter
    }
}
