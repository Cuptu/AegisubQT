// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls

// Native Windows 10 styled dropdown combo box with rectangular geometry, crisp borders, and DWM animation.
ComboBox {
    id: combo
    implicitHeight: 22
    font.pixelSize: 12
    font.family: uiTheme.uiFont
    leftPadding: 8
    rightPadding: 24
    topPadding: 1
    bottomPadding: 1

    contentItem: Item {
        implicitHeight: 20

        Text {
            anchors.fill: parent
            text: combo.displayText
            font: combo.font
            renderType: Text.NativeRendering
            color: combo.enabled ? "#000000" : "#838383"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            visible: !combo.editable
        }

        TextInput {
            id: editorInput
            anchors.fill: parent
            text: combo.editText
            font: combo.font
            renderType: Text.NativeRendering
            color: combo.enabled ? "#000000" : "#838383"
            selectionColor: "#0078d7"
            selectedTextColor: "#ffffff"
            verticalAlignment: Text.AlignVCenter
            selectByMouse: true
            clip: true
            visible: combo.editable
            enabled: combo.editable
            onTextChanged: {
                if (combo.editText !== text) {
                    combo.editText = text;
                }
            }
            Connections {
                target: combo
                function onEditTextChanged() {
                    if (editorInput.text !== combo.editText) {
                        editorInput.text = combo.editText;
                    }
                }
            }
        }
    }

    // Windows 10 native style chevron indicator
    indicator: Item {
        x: combo.width - width - 6
        anchors.verticalCenter: parent.verticalCenter
        width: 12
        height: 12

        Canvas {
            id: chevronCanvas
            anchors.centerIn: parent
            width: 8
            height: 5
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                var strokeCol = !combo.enabled ? "#a0a0a0" : ((combo.popup.visible || combo.pressed) ? "#0078d7" : (combo.hovered ? "#000000" : "#555555"));
                ctx.strokeStyle = strokeCol;
                ctx.lineWidth = 1.2;
                ctx.lineCap = "round";
                ctx.lineJoin = "round";
                ctx.beginPath();
                ctx.moveTo(1, 1);
                ctx.lineTo(4, 4);
                ctx.lineTo(7, 1);
                ctx.stroke();
            }
            Connections {
                target: combo
                function onHoveredChanged() { chevronCanvas.requestPaint(); }
                function onPressedChanged() { chevronCanvas.requestPaint(); }
                function onEnabledChanged() { chevronCanvas.requestPaint(); }
            }
            Connections {
                target: combo.popup
                function onVisibleChanged() { chevronCanvas.requestPaint(); }
            }
        }
    }

    background: Rectangle {
        radius: 0
        border.width: 1
        border.color: {
            if (!combo.enabled) return "#dcdcdc";
            if (combo.popup.visible || combo.activeFocus) return "#0078d7";
            if (combo.hovered) return "#0078d7";
            return "#abadb3";
        }
        color: {
            if (!combo.enabled) return "#f0f0f0";
            if (combo.popup.visible || combo.pressed) return "#cce8ff";
            if (combo.hovered) return "#e5f1fb";
            return "#ffffff";
        }
    }

    popup: Popup {
        id: comboPopup
        y: combo.height - 1
        width: Math.max(combo.width, implicitWidth)
        implicitHeight: Math.min(260, contentItem.implicitHeight + 2)
        padding: 1
        topMargin: 4
        bottomMargin: 4

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

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: combo.popup.visible ? combo.delegateModel : null
            currentIndex: combo.highlightedIndex

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Item {
            // Soft Windows elevation drop shadow on right and bottom edges
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

            // Popup main container with crisp 1px active border
            Rectangle {
                anchors.fill: parent
                radius: 0
                border.color: "#0078d7"
                border.width: 1
                color: "#ffffff"
            }
        }
    }

    delegate: ItemDelegate {
        id: itemDel
        width: ListView.view ? ListView.view.width : combo.width - 2
        implicitHeight: 22
        padding: 0
        leftPadding: 8
        rightPadding: 8
        highlighted: combo.highlightedIndex === index

        contentItem: Text {
            text: (typeof modelData === "string") ? modelData : (modelData && modelData.name ? modelData.name : (modelData && modelData.text ? modelData.text : String(modelData)))
            font.pixelSize: combo.font.pixelSize
            font.family: combo.font.family
            font.weight: Font.Normal
            renderType: Text.NativeRendering
            color: {
                if (!itemDel.enabled) return "#838383";
                if (itemDel.highlighted) return "#ffffff";
                return "#000000";
            }
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 0
            anchors.fill: parent
            color: {
                if (itemDel.highlighted) return "#0078d7";
                if (itemDel.hovered) return "#e5f1fb";
                return "#ffffff";
            }
        }
    }
}
