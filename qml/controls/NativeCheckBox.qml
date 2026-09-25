// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls

// Modern Windows styled checkbox control with fluent accent indicator and native text rendering.
CheckBox {
    id: chk
    implicitHeight: 22
    font.pixelSize: 12
    font.family: uiTheme.uiFont
    spacing: 7

    indicator: Rectangle {
        id: ind
        implicitWidth: 14
        implicitHeight: 14
        x: chk.leftPadding
        anchors.verticalCenter: parent.verticalCenter
        radius: 3
        border.width: 1
        border.color: {
            if (!chk.enabled) return "#cccccc";
            if (chk.checked) {
                if (chk.pressed) return "#005299";
                if (chk.hovered) return "#005fb8";
                return "#0067c0";
            }
            if (chk.hovered) return "#0067c0";
            return "#767676";
        }
        color: {
            if (!chk.enabled) return chk.checked ? "#cccccc" : "#f0f0f0";
            if (chk.checked) {
                if (chk.pressed) return "#005299";
                if (chk.hovered) return "#005fb8";
                return "#0067c0";
            }
            if (chk.hovered) return "#f7f7f7";
            return "#ffffff";
        }

        Canvas {
            id: checkCanvas
            anchors.fill: parent
            visible: chk.checked
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                ctx.strokeStyle = chk.enabled ? "#ffffff" : "#838383";
                ctx.lineWidth = 1.8;
                ctx.lineCap = "round";
                ctx.lineJoin = "round";
                ctx.beginPath();
                ctx.moveTo(width * 0.24, height * 0.52);
                ctx.lineTo(width * 0.44, height * 0.72);
                ctx.lineTo(width * 0.78, height * 0.28);
                ctx.stroke();
            }
            Connections {
                target: chk
                function onCheckedChanged() { checkCanvas.requestPaint(); }
                function onEnabledChanged() { checkCanvas.requestPaint(); }
            }
        }
    }

    contentItem: Text {
        leftPadding: chk.indicator.width + chk.spacing
        text: chk.text
        font: chk.font
        renderType: Text.NativeRendering
        color: chk.enabled ? "#000000" : "#838383"
        verticalAlignment: Text.AlignVCenter
    }
}
