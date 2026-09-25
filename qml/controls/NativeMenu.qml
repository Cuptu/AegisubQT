// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Native Windows 10 styled popup menu control with rectangular geometry,
// crisp elevation drop shadow, DWM downward slide animation, and icon gutter.
Menu {
    id: nMenu
    implicitWidth: 250
    topPadding: 3
    bottomPadding: 3
    leftPadding: 1
    rightPadding: 1

    // Windows 10 native in-place fade transition (crisp, zero positional offset)
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

    background: Item {
        implicitWidth: 250

        // Soft Windows DWM elevation drop shadow on right and bottom edges
        Rectangle {
            x: 1; y: 2
            width: parent.width + 3; height: parent.height + 3
            color: "#0d000000"
            radius: 0
        }
        Rectangle {
            x: 1; y: 1
            width: parent.width + 2; height: parent.height + 2
            color: "#18000000"
            radius: 0
        }
        Rectangle {
            x: 0; y: 1
            width: parent.width + 1; height: parent.height + 1
            color: "#24000000"
            radius: 0
        }

        // Menu main container with crisp 1px border and sharp rectangular corners
        Rectangle {
            anchors.fill: parent
            color: "#ffffff"
            border.color: "#cccccc"
            border.width: 1
            radius: 0
        }
    }

    delegate: MenuItem {
        id: menuItem
        implicitHeight: 25
        implicitWidth: nMenu.width

        contentItem: Item {
            anchors.fill: parent

            // Left icon gutter area (16x16 icon centered in 28px column)
            Item {
                id: iconArea
                width: 28
                height: parent.height
                anchors.left: parent.left

                Image {
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                    fillMode: Image.Pad
                    source: menuItem.icon.source
                    visible: menuItem.icon.source !== ""
                    opacity: menuItem.enabled ? 1.0 : 0.35
                    smooth: false // preserve pixel crispness for 16x16 PNG bitmaps
                }

                Text {
                    anchors.centerIn: parent
                    text: "✓"
                    font.pixelSize: 12
                    font.bold: true
                    color: "#0078d4"
                    visible: menuItem.checkable && menuItem.checked && menuItem.icon.source === ""
                }
            }

            // Menu item title
            Text {
                anchors.left: iconArea.right
                anchors.leftMargin: 2
                anchors.right: accelText.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: menuItem.text.split("\t")[0].replace(/&/g, "")
                font.pixelSize: 12
                font.family: uiTheme.uiFont
                renderType: Text.NativeRendering
                color: menuItem.enabled ? "#000000" : "#8a8a8a"
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            // Shortcut / Accelerator text
            Text {
                id: accelText
                anchors.right: parent.right
                anchors.rightMargin: menuItem.subMenu !== null ? 22 : 12
                anchors.verticalCenter: parent.verticalCenter
                text: menuItem.text.indexOf("\t") !== -1 ? menuItem.text.split("\t")[1].replace(/\+/g, "-") : ""
                font.pixelSize: 11
                font.family: uiTheme.uiFont
                renderType: Text.NativeRendering
                color: menuItem.enabled ? "#000000" : "#8a8a8a"
                visible: text !== ""
            }
        }

        arrow: Canvas {
            x: parent.width - width - 10
            anchors.verticalCenter: parent.verticalCenter
            width: 5
            height: 9
            visible: menuItem.subMenu !== null
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                ctx.lineWidth = 1.3;
                ctx.strokeStyle = menuItem.enabled ? "#2b2b2b" : "#a6a6a6";
                ctx.beginPath();
                ctx.moveTo(1, 1);
                ctx.lineTo(4.5, 4.5);
                ctx.lineTo(1, 8);
                ctx.stroke();
            }
        }

        background: Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 1
            anchors.rightMargin: 1
            anchors.topMargin: 0
            anchors.bottomMargin: 0
            radius: 0
            color: menuItem.highlighted && menuItem.enabled ? "#e8e8e8" : "transparent"
        }
    }
}
