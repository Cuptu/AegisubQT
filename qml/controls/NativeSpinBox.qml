// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls

// Numeric spin box control with text field, suffix formatting, and
// vertical up/down stepper buttons.
Control {
    id: spinRoot

    property int from: 0
    property int to: 100000
    property real value: 0
    property real stepSize: 1
    property int decimals: 0
    property string suffix: ""
    property alias readOnly: input.readOnly

    signal valueModified(real val)

    function formatValue(number) {
        return Number(number.toFixed(decimals)).toString();
    }

    function normalizeValue(number) {
        var factor = Math.pow(10, decimals);
        return Math.round(Math.max(from, Math.min(to, number)) * factor) / factor;
    }

    implicitHeight: 21
    implicitWidth: 80

    onValueChanged: {
        if (!input.activeFocus) {
            input.text = spinRoot.formatValue(spinRoot.value) + (spinRoot.suffix ? " " + spinRoot.suffix : "");
        }
    }

    Component.onCompleted: {
        input.text = spinRoot.formatValue(spinRoot.value) + (spinRoot.suffix ? " " + spinRoot.suffix : "");
    }

    background: Rectangle {
        border.color: {
            if (!spinRoot.enabled) return "#cccccc";
            if (input.activeFocus) return "#0078d7";
            if (spinRoot.hovered) return "#000000";
            return "#7a7a7a";
        }
        border.width: 1
        color: spinRoot.enabled ? "#ffffff" : "#f0f0f0"
    }

    contentItem: Item {
        anchors.fill: parent

        TextInput {
            id: input
            anchors.left: parent.left
            anchors.right: btnColumn.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: 4
            anchors.rightMargin: 2
            verticalAlignment: TextInput.AlignVCenter
            font.pixelSize: 12
            font.family: uiTheme.uiFont
            renderType: Text.NativeRendering
            color: spinRoot.enabled ? "#000000" : "#6d6d6d"
            selectByMouse: true
            selectionColor: "#0078d7"
            selectedTextColor: "#ffffff"

            onEditingFinished: {
                var num = parseFloat(text.replace(",", "."));
                if (isNaN(num)) num = spinRoot.from;
                num = spinRoot.normalizeValue(num);
                spinRoot.applyUserValue(num);
                text = spinRoot.formatValue(num) + (spinRoot.suffix ? " " + spinRoot.suffix : "");
            }

            Keys.onUpPressed: spinRoot.stepUp()
            Keys.onDownPressed: spinRoot.stepDown()
        }

        // Stepper buttons column
        Rectangle {
            id: btnColumn
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 17
            color: "#e1e1e1"
            border.color: "#adadad"
            border.width: 1

            // Up Button
            Item {
                id: upBtn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: Math.floor(parent.height / 2)

                Rectangle {
                    anchors.fill: parent
                    color: {
                        if (!spinRoot.enabled || spinRoot.value >= spinRoot.to) return "#f0f0f0";
                        if (upMouse.pressed) return "#cce4f7";
                        if (upMouse.containsMouse) return "#e5f1fb";
                        return "transparent";
                    }
                }

                Canvas {
                    anchors.centerIn: parent
                    width: 7
                    height: 4
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.reset();
                        ctx.moveTo(3.5, 0);
                        ctx.lineTo(7, 4);
                        ctx.lineTo(0, 4);
                        ctx.closePath();
                        ctx.fillStyle = (spinRoot.enabled && spinRoot.value < spinRoot.to) ? "#000000" : "#838383";
                        ctx.fill();
                    }
                }

                MouseArea {
                    id: upMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: spinRoot.enabled && spinRoot.value < spinRoot.to
                    onClicked: spinRoot.stepUp()
                }
            }

            // Separator between buttons
            Rectangle {
                anchors.top: upBtn.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#adadad"
            }

            // Down Button
            Item {
                id: downBtn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: Math.floor(parent.height / 2)

                Rectangle {
                    anchors.fill: parent
                    color: {
                        if (!spinRoot.enabled || spinRoot.value <= spinRoot.from) return "#f0f0f0";
                        if (downMouse.pressed) return "#cce4f7";
                        if (downMouse.containsMouse) return "#e5f1fb";
                        return "transparent";
                    }
                }

                Canvas {
                    anchors.centerIn: parent
                    width: 7
                    height: 4
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.reset();
                        ctx.moveTo(0, 0);
                        ctx.lineTo(7, 0);
                        ctx.lineTo(3.5, 4);
                        ctx.closePath();
                        ctx.fillStyle = (spinRoot.enabled && spinRoot.value > spinRoot.from) ? "#000000" : "#838383";
                        ctx.fill();
                    }
                }

                MouseArea {
                    id: downMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: spinRoot.enabled && spinRoot.value > spinRoot.from
                    onClicked: spinRoot.stepDown()
                }
            }
        }
    }

    function applyUserValue(number) {
        var next = normalizeValue(number);
        // Let the owner update its bound value before falling back to local
        // state, so a user step does not discard a live value binding.
        valueModified(next);
        if (value !== next) value = next;
    }

    function stepUp() {
        applyUserValue(value + stepSize);
        input.text = formatValue(value) + (suffix ? " " + suffix : "");
    }

    function stepDown() {
        applyUserValue(value - stepSize);
        input.text = formatValue(value) + (suffix ? " " + suffix : "");
    }
}
