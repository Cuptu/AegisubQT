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
    property int value: 0
    property int stepSize: 1
    property string suffix: ""
    property alias readOnly: input.readOnly

    signal valueModified(int val)

    implicitHeight: 21
    implicitWidth: 80

    onValueChanged: {
        if (!input.activeFocus) {
            input.text = spinRoot.value.toString() + (spinRoot.suffix ? " " + spinRoot.suffix : "");
        }
    }

    Component.onCompleted: {
        input.text = spinRoot.value.toString() + (spinRoot.suffix ? " " + spinRoot.suffix : "");
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
                var num = parseInt(text.replace(/[^0-9\-]/g, ""));
                if (isNaN(num)) num = spinRoot.from;
                num = Math.max(spinRoot.from, Math.min(spinRoot.to, num));
                spinRoot.value = num;
                spinRoot.valueModified(num);
                text = num.toString() + (spinRoot.suffix ? " " + spinRoot.suffix : "");
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

    function stepUp() {
        if (value + stepSize <= to) {
            value += stepSize;
            valueModified(value);
        } else {
            value = to;
            valueModified(value);
        }
        input.text = value.toString() + (suffix ? " " + suffix : "");
    }

    function stepDown() {
        if (value - stepSize >= from) {
            value -= stepSize;
            valueModified(value);
        } else {
            value = from;
            valueModified(value);
        }
        input.text = value.toString() + (suffix ? " " + suffix : "");
    }
}
