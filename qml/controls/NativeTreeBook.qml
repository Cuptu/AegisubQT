// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls

// Hierarchical tree navigation list for multi-page preferences dialogs.
Item {
    id: treeRoot

    property int currentIndex: 0
    signal itemSelected(int pageIndex)

    // Flat tree node list with parent hierarchy indexing preferences pages:
    //  0: General
    //  1: General / Default Styles
    //  2: Audio
    //  3: Video
    //  4: Interface
    //  5: Interface / Colours
    //  6: Interface / Hotkeys
    //  7: Backup
    //  8: Automation
    //  9: Advanced
    // 10: Advanced / Audio
    // 11: Advanced / Video

    property var treeItems: [
        { pageIndex: 0, text: qsTr("General"), level: 0, hasChildren: true, expanded: true, parentIndex: -1, isLastChild: false },
        { pageIndex: 1, text: qsTr("Default Styles"), level: 1, hasChildren: false, expanded: false, parentIndex: 0, isLastChild: true },
        { pageIndex: 2, text: qsTr("Audio"), level: 0, hasChildren: false, expanded: false, parentIndex: -1, isLastChild: false },
        { pageIndex: 3, text: qsTr("Video"), level: 0, hasChildren: false, expanded: false, parentIndex: -1, isLastChild: false },
        { pageIndex: 4, text: qsTr("Interface"), level: 0, hasChildren: true, expanded: true, parentIndex: -1, isLastChild: false },
        { pageIndex: 5, text: qsTr("Colours"), level: 1, hasChildren: false, expanded: false, parentIndex: 4, isLastChild: false },
        { pageIndex: 6, text: qsTr("Hotkeys"), level: 1, hasChildren: false, expanded: false, parentIndex: 4, isLastChild: true },
        { pageIndex: 7, text: qsTr("Backup"), level: 0, hasChildren: false, expanded: false, parentIndex: -1, isLastChild: false },
        { pageIndex: 8, text: qsTr("Automation"), level: 0, hasChildren: false, expanded: false, parentIndex: -1, isLastChild: false },
        { pageIndex: 9, text: qsTr("Advanced"), level: 0, hasChildren: true, expanded: true, parentIndex: -1, isLastChild: false },
        { pageIndex: 10, text: qsTr("Audio"), level: 1, hasChildren: false, expanded: false, parentIndex: 9, isLastChild: false },
        { pageIndex: 11, text: qsTr("Video"), level: 1, hasChildren: false, expanded: false, parentIndex: 9, isLastChild: true }
    ]

    Rectangle {
        anchors.fill: parent
        color: "#ffffff"
        border.color: "#7f9db9"
        border.width: 1

        ListView {
            id: lv
            anchors.fill: parent
            anchors.margins: 2
            clip: true
            model: treeRoot.treeItems
            boundsBehavior: Flickable.StopAtBounds

            delegate: Item {
                id: itemDelegate
                width: lv.width
                visible: modelData.level === 0 || isParentExpanded(modelData.parentIndex)
                height: visible ? 20 : 0

                function isParentExpanded(parentIdx) {
                    if (parentIdx < 0) return true;
                    for (var i = 0; i < treeRoot.treeItems.length; ++i) {
                        if (treeRoot.treeItems[i].pageIndex === parentIdx) {
                            return treeRoot.treeItems[i].expanded;
                        }
                    }
                    return true;
                }

                // Dotted tree connector guidelines
                Canvas {
                    id: canvas
                    anchors.fill: parent
                    visible: itemDelegate.visible

                    Connections {
                        target: treeRoot
                        function onTreeItemsChanged() { canvas.requestPaint(); }
                    }

                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.reset();
                        ctx.fillStyle = "#808080";

                        function drawV(x, y1, y2) {
                            for (var y = y1; y <= y2; y += 2) {
                                ctx.fillRect(x, y, 1, 1);
                            }
                        }
                        function drawH(x1, x2, y) {
                            for (var x = x1; x <= x2; x += 2) {
                                ctx.fillRect(x, y, 1, 1);
                            }
                        }

                        var trunkX = 11;
                        var subTrunkX = 25;
                        var midY = 10;

                        // 1. Trunk lines at x = 11 (connects root items up to index 9 "Advanced")
                        if (index <= 9) {
                            if (modelData.hasChildren) {
                                if (index > 0) {
                                    drawV(trunkX, 0, 4);
                                }
                                if (index < 9 || modelData.expanded) {
                                    drawV(trunkX, 15, 20);
                                }
                            } else if (modelData.level === 0) {
                                drawV(trunkX, 0, 20);
                                drawH(trunkX, 20, midY);
                            } else if (modelData.level === 1) {
                                drawV(trunkX, 0, 20);
                            }
                        }

                        // 2. Sub-trunk lines at x = 25 for level 1 children
                        if (modelData.level === 1) {
                            drawV(subTrunkX, 0, midY);
                            drawH(subTrunkX, 34, midY);
                            if (!modelData.isLastChild) {
                                drawV(subTrunkX, midY, 20);
                            }
                        }
                    }
                }

                // Expand / Collapse box (9x9 pixels centered at x = 11, y = 10)
                Rectangle {
                    id: expandBox
                    x: 7
                    anchors.verticalCenter: parent.verticalCenter
                    width: 9
                    height: 9
                    color: "#ffffff"
                    border.color: "#808080"
                    border.width: 1
                    visible: modelData.hasChildren

                    // Horizontal line "-"
                    Rectangle {
                        anchors.centerIn: parent
                        width: 5
                        height: 1
                        color: "#000000"
                    }

                    // Vertical line "|" if collapsed "+"
                    Rectangle {
                        anchors.centerIn: parent
                        width: 1
                        height: 5
                        color: "#000000"
                        visible: !modelData.expanded
                    }

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -4
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var items = treeRoot.treeItems;
                            items[index].expanded = !items[index].expanded;
                            treeRoot.treeItems = [].concat(items);
                        }
                    }
                }

                // Item Text Container with perfect alignment
                Rectangle {
                    id: textContainer
                    x: (modelData.level === 0) ? 22 : 36
                    anchors.verticalCenter: parent.verticalCenter
                    height: 18
                    width: labelText.implicitWidth + 6
                    color: treeRoot.currentIndex === modelData.pageIndex ? "#3399ff" : "transparent"

                    Text {
                        id: labelText
                        anchors.centerIn: parent
                        text: modelData.text
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        renderType: Text.NativeRendering
                        color: treeRoot.currentIndex === modelData.pageIndex ? "#ffffff" : "#000000"
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            treeRoot.currentIndex = modelData.pageIndex;
                            treeRoot.itemSelected(modelData.pageIndex);
                        }
                    }
                }
            }
        }
    }
}
