// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Subtitle grid table view with sortable columns, selection highlight, and context menus.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import "../project/AssUtils.js" as AssUtils

Rectangle {
    id: gridAreaRoot
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumHeight: 100
    color: "#ffffff"
    radius: 0
    border.color: "#d0d0d0"
    border.width: 1
    clip: true

    required property var project
    property var videoCtrl: null

    signal statusMessage(string text)
    signal createAudioClipRequested()

    // Column width metrics and visibility states (12 columns)
    property int col0Width: 28
    property int colLayerWidth: 36
    property int col1Width: 76
    property int col2Width: 76
    property int col3Width: 42
    property int col4Width: 72
    property int colActorWidth: 65
    property int colEffectWidth: 60
    property int colMarginLeftWidth: 36
    property int colMarginRightWidth: 36
    property int colMarginVertWidth: 36

    property bool showColLayer: false
    property bool showColCPS: true
    property bool showColActor: false
    property bool showColEffect: false
    property bool showColMarginLeft: false
    property bool showColMarginRight: false
    property bool showColMarginVert: false

    // Grid palette constants matching Aegisub BaseGrid styling
    readonly property color colWinHeader: "#a5cfe7"
    readonly property color colWinHeaderBorder: "#808080"
    readonly property color colWinLeftCol: "#c4ecc9"
    readonly property color colWinSelectionBg: "#ceffea"
    readonly property color colWinInFrame: "#fffdea"
    readonly property color colWinGridLines: "#bebebe"
    readonly property color colWinActiveBorder: "#ff5bef"

    // Typography and row height: 20px line height (dc.GetCharHeight() + 4), 12px font
    readonly property string gridFontFamily: "Microsoft YaHei UI"
    readonly property int gridFontSize: 12
    readonly property int gridLineHeight: 20

    function getCpsBgColor(cpsVal, baseBg) {
        var c = parseInt(cpsVal, 10);
        if (isNaN(c) || c <= 0) return baseBg;
        if (c > 22) return "#ffb0b0";
        if (c > 18) return "#ffe6a0";
        return baseBg;
    }

    function formatGridText(rawText, mode) {
        if (!rawText) return "";
        if (mode === 0) return rawText;
        if (mode === 1) return rawText.replace(/\{[^}]*\}/g, "❚");
        if (mode === 2) return rawText.replace(/\{[^}]*\}/g, "");
        return rawText;
    }

    // Auto-expand optional columns when content is present, matching Aegisub GridColumn::UpdateWidth logic
    function checkDynamicColumns() {
        if (!project || !project.subtitleModel) return;
        var model = project.subtitleModel;
        var count = model.rowCount ? model.rowCount() : (model.count || 0);
        var hasActor = false;
        var hasLayer = false;
        var hasEffect = false;
        for (var i = 0; i < count; ++i) {
            var item = model.get(i);
            if (item) {
                if (!hasActor && item.actor && item.actor.trim() !== "") hasActor = true;
                if (!hasLayer && item.layer && parseInt(item.layer, 10) > 0) hasLayer = true;
                if (!hasEffect && item.effect && item.effect.trim() !== "") hasEffect = true;
            }
            if (hasActor && hasLayer && hasEffect) break;
        }
        if (hasActor) gridAreaRoot.showColActor = true;
        if (hasLayer) gridAreaRoot.showColLayer = true;
        if (hasEffect) gridAreaRoot.showColEffect = true;
    }

    Connections {
        target: gridAreaRoot.project
        function onDataModified() {
            gridAreaRoot.checkDynamicColumns();
        }
    }

    Component.onCompleted: {
        gridAreaRoot.checkDynamicColumns();
    }

    // Component and menu aliases
    property alias gridView: gridView
    property alias headerContextMenu: headerContextMenu
    property alias gridContextMenu: gridContextMenu
    property alias itemInsertBefore: itemInsertBefore
    property alias itemSwap: itemSwap
    property alias itemDuplicate: itemDuplicate

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Grid header bar
        Rectangle {
            id: gridHeader
            Layout.fillWidth: true
            implicitHeight: gridAreaRoot.gridLineHeight
            color: gridAreaRoot.colWinHeader
            border.color: gridAreaRoot.colWinHeaderBorder
            border.width: 1
            antialiasing: false

            component GridHeaderCell: Item {
                id: ghCell
                property string title: ""
                property int colIndex: 0
                property alias itemWidth: ghCell.width
                signal widthResized(int newW)
                height: parent.height

                Rectangle {
                    anchors.fill: parent
                    color: hMouse.containsMouse ? "#c2dcf0" : "transparent"
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 4
                    anchors.rightMargin: 4
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: ghCell.title
                        font.bold: true
                        font.pixelSize: gridAreaRoot.gridFontSize
                        font.family: gridAreaRoot.gridFontFamily
                        renderType: Text.NativeRendering
                        color: "#000000"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    Text {
                        visible: gridAreaRoot.project.sortColumn === ghCell.colIndex
                        text: gridAreaRoot.project.sortAscending ? "▲" : "▼"
                        font.pixelSize: 8
                        color: "#005a9e"
                    }
                }

                MouseArea {
                    id: hMouse
                    anchors.fill: parent
                    anchors.rightMargin: 5
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            var p = mapToItem(gridAreaRoot, mouse.x, mouse.y);
                            headerContextMenu.x = Math.max(0, Math.min(gridAreaRoot.width - headerContextMenu.implicitWidth - 5, p.x));
                            headerContextMenu.y = Math.max(0, Math.min(gridAreaRoot.height - 200, p.y));
                            headerContextMenu.open();
                        } else {
                            gridAreaRoot.project.sortByColumn(ghCell.colIndex);
                        }
                    }
                }

                MouseArea {
                    width: 5
                    height: parent.height
                    anchors.right: parent.right
                    cursorShape: Qt.SplitHCursor
                    preventStealing: true
                    property int startX: 0
                    property int startW: 0
                    onPressed: (mouse) => {
                        startX = mouse.x;
                        startW = ghCell.width;
                    }
                    onPositionChanged: (mouse) => {
                        if (pressed) {
                            var diff = mouse.x - startX;
                            ghCell.widthResized(Math.max(20, startW + diff));
                        }
                    }
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            var p = mapToItem(gridAreaRoot, mouse.x, mouse.y);
                            headerContextMenu.x = Math.max(0, Math.min(gridAreaRoot.width - headerContextMenu.implicitWidth - 5, p.x));
                            headerContextMenu.y = Math.max(0, Math.min(gridAreaRoot.height - 200, p.y));
                            headerContextMenu.open();
                        }
                    }
                }
            }

            Row {
                anchors.fill: parent
                spacing: 0

                // Col 0: Line number
                GridHeaderCell {
                    width: gridAreaRoot.col0Width
                    title: "#"
                    colIndex: 0
                    onWidthResized: (w) => gridAreaRoot.col0Width = w
                }
                Rectangle { width: 1; height: parent.height; color: gridAreaRoot.colWinHeaderBorder; antialiasing: false }

                // Col Layer: Layer
                GridHeaderCell {
                    visible: gridAreaRoot.showColLayer
                    width: gridAreaRoot.colLayerWidth
                    title: qsTr("L")
                    colIndex: 1
                    onWidthResized: (w) => gridAreaRoot.colLayerWidth = w
                }
                Rectangle { visible: gridAreaRoot.showColLayer; width: 1; height: parent.height; color: gridAreaRoot.colWinHeaderBorder; antialiasing: false }

                // Col 1: Start time
                GridHeaderCell {
                    width: gridAreaRoot.col1Width
                    title: qsTr("Start")
                    colIndex: 2
                    onWidthResized: (w) => gridAreaRoot.col1Width = w
                }
                Rectangle { width: 1; height: parent.height; color: gridAreaRoot.colWinHeaderBorder; antialiasing: false }

                // Col 2: End time
                GridHeaderCell {
                    width: gridAreaRoot.col2Width
                    title: qsTr("End")
                    colIndex: 3
                    onWidthResized: (w) => gridAreaRoot.col2Width = w
                }
                Rectangle { width: 1; height: parent.height; color: gridAreaRoot.colWinHeaderBorder; antialiasing: false }

                // Col 3: Characters per second (CPS)
                GridHeaderCell {
                    visible: gridAreaRoot.showColCPS
                    width: gridAreaRoot.col3Width
                    title: qsTr("CPS")
                    colIndex: 4
                    onWidthResized: (w) => gridAreaRoot.col3Width = w
                }
                Rectangle { visible: gridAreaRoot.showColCPS; width: 1; height: parent.height; color: gridAreaRoot.colWinHeaderBorder; antialiasing: false }

                // Col 4: Style
                GridHeaderCell {
                    width: gridAreaRoot.col4Width
                    title: qsTr("Style")
                    colIndex: 5
                    onWidthResized: (w) => gridAreaRoot.col4Width = w
                }
                Rectangle { width: 1; height: parent.height; color: gridAreaRoot.colWinHeaderBorder; antialiasing: false }

                // Col Actor: Actor
                GridHeaderCell {
                    visible: gridAreaRoot.showColActor
                    width: gridAreaRoot.colActorWidth
                    title: qsTr("Actor")
                    colIndex: 6
                    onWidthResized: (w) => gridAreaRoot.colActorWidth = w
                }
                Rectangle { visible: gridAreaRoot.showColActor; width: 1; height: parent.height; color: gridAreaRoot.colWinHeaderBorder; antialiasing: false }

                // Col Effect: Effect
                GridHeaderCell {
                    visible: gridAreaRoot.showColEffect
                    width: gridAreaRoot.colEffectWidth
                    title: qsTr("Effect")
                    colIndex: 7
                    onWidthResized: (w) => gridAreaRoot.colEffectWidth = w
                }
                Rectangle { visible: gridAreaRoot.showColEffect; width: 1; height: parent.height; color: gridAreaRoot.colWinHeaderBorder; antialiasing: false }

                // Col Margin Left: Left margin
                GridHeaderCell {
                    visible: gridAreaRoot.showColMarginLeft
                    width: gridAreaRoot.colMarginLeftWidth
                    title: qsTr("Left")
                    colIndex: 8
                    onWidthResized: (w) => gridAreaRoot.colMarginLeftWidth = w
                }
                Rectangle { visible: gridAreaRoot.showColMarginLeft; width: 1; height: parent.height; color: gridAreaRoot.colWinHeaderBorder; antialiasing: false }

                // Col Margin Right: Right margin
                GridHeaderCell {
                    visible: gridAreaRoot.showColMarginRight
                    width: gridAreaRoot.colMarginRightWidth
                    title: qsTr("Right")
                    colIndex: 9
                    onWidthResized: (w) => gridAreaRoot.colMarginRightWidth = w
                }
                Rectangle { visible: gridAreaRoot.showColMarginRight; width: 1; height: parent.height; color: gridAreaRoot.colWinHeaderBorder; antialiasing: false }

                // Col Margin Vert: Vertical margin
                GridHeaderCell {
                    visible: gridAreaRoot.showColMarginVert
                    width: gridAreaRoot.colMarginVertWidth
                    title: qsTr("Vert")
                    colIndex: 10
                    onWidthResized: (w) => gridAreaRoot.colMarginVertWidth = w
                }
                Rectangle { visible: gridAreaRoot.showColMarginVert; width: 1; height: parent.height; color: gridAreaRoot.colWinHeaderBorder; antialiasing: false }

                // Col 5: Subtitle text (fills remaining width)
                Item {
                    width: Math.max(80, gridHeader.width - (
                        gridAreaRoot.col0Width + 1 +
                        (gridAreaRoot.showColLayer ? gridAreaRoot.colLayerWidth + 1 : 0) +
                        gridAreaRoot.col1Width + 1 +
                        gridAreaRoot.col2Width + 1 +
                        (gridAreaRoot.showColCPS ? gridAreaRoot.col3Width + 1 : 0) +
                        gridAreaRoot.col4Width + 1 +
                        (gridAreaRoot.showColActor ? gridAreaRoot.colActorWidth + 1 : 0) +
                        (gridAreaRoot.showColEffect ? gridAreaRoot.colEffectWidth + 1 : 0) +
                        (gridAreaRoot.showColMarginLeft ? gridAreaRoot.colMarginLeftWidth + 1 : 0) +
                        (gridAreaRoot.showColMarginRight ? gridAreaRoot.colMarginRightWidth + 1 : 0) +
                        (gridAreaRoot.showColMarginVert ? gridAreaRoot.colMarginVertWidth + 1 : 0)
                    ))
                    height: parent.height
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Text"); font.bold: true
                        font.pixelSize: gridAreaRoot.gridFontSize
                        font.family: gridAreaRoot.gridFontFamily
                        renderType: Text.NativeRendering
                        color: "#000000"
                    }
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: (mouse) => {
                            if (mouse.button === Qt.RightButton) {
                                var p = mapToItem(gridAreaRoot, mouse.x, mouse.y);
                                headerContextMenu.x = Math.max(0, Math.min(gridAreaRoot.width - headerContextMenu.implicitWidth - 5, p.x));
                                headerContextMenu.y = Math.max(0, Math.min(gridAreaRoot.height - 200, p.y));
                                headerContextMenu.open();
                            } else {
                                gridAreaRoot.project.sortByColumn(11);
                            }
                        }
                    }
                }
            }
        }

        // Table Rows Viewport
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // Col 0 (# column) background gutter extending down continuously, matching Aegisub BaseGrid styling
            Rectangle {
                width: gridAreaRoot.col0Width
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                color: gridAreaRoot.colWinLeftCol
                antialiasing: false
            }

            ListView {
                id: gridView
                anchors.fill: parent
                model: gridAreaRoot.project.subtitleModel
                boundsBehavior: Flickable.StopAtBounds

                delegate: Item {
                    id: rowItem
                    width: gridView.width
                    height: gridAreaRoot.gridLineHeight
                    property bool isSelected: gridAreaRoot.project.isRowSelected(index)
                    property int curTimeMs: gridAreaRoot.videoCtrl ? Math.round(gridAreaRoot.videoCtrl.currentTime * 1000.0) : 0
                    property bool isInFrame: {
                        var s = AssUtils.assToMs(model.start);
                        var e = AssUtils.assToMs(model.end);
                        return curTimeMs >= s && curTimeMs < e;
                    }
                    property color cellBgColor: rowItem.isSelected ? gridAreaRoot.colWinSelectionBg : (rowItem.isInFrame ? gridAreaRoot.colWinInFrame : "#ffffff")

                    Row {
                        anchors.fill: parent
                        spacing: 0

                        // Col 0: Line number cell (styled with LeftCol mint green #c4ecc9, unchanged on selection in Aegisub)
                        Rectangle {
                            width: gridAreaRoot.col0Width
                            height: parent.height
                            color: gridAreaRoot.colWinLeftCol
                            antialiasing: false
                        Text {
                            anchors.centerIn: parent
                            text: model.lineNumber
                            font.pixelSize: gridAreaRoot.gridFontSize
                            font.family: gridAreaRoot.gridFontFamily
                            renderType: Text.NativeRendering
                            color: "#000000"
                        }
                    }
                    Rectangle { width: 1; height: parent.height; color: gridAreaRoot.colWinGridLines; antialiasing: false }

                    // Col Layer: Layer
                    Rectangle {
                        visible: gridAreaRoot.showColLayer
                        width: gridAreaRoot.colLayerWidth
                        height: parent.height
                        color: rowItem.cellBgColor
                        antialiasing: false
                        Text {
                            anchors.centerIn: parent
                            text: (typeof model.layer !== "undefined") ? model.layer : "0"
                            font.pixelSize: gridAreaRoot.gridFontSize
                            font.family: gridAreaRoot.gridFontFamily
                            renderType: Text.NativeRendering
                            color: "#000000"
                        }
                    }
                    Rectangle { visible: gridAreaRoot.showColLayer; width: 1; height: parent.height; color: gridAreaRoot.colWinGridLines; antialiasing: false }

                    // Col 1: Start time
                    Rectangle {
                        width: gridAreaRoot.col1Width
                        height: parent.height
                        color: rowItem.cellBgColor
                        antialiasing: false
                        Text {
                            anchors.centerIn: parent
                            text: model.start
                            font.pixelSize: gridAreaRoot.gridFontSize
                            font.family: gridAreaRoot.gridFontFamily
                            renderType: Text.NativeRendering
                            color: "#000000"
                        }
                    }
                    Rectangle { width: 1; height: parent.height; color: gridAreaRoot.colWinGridLines; antialiasing: false }

                    // Col 2: End time
                    Rectangle {
                        width: gridAreaRoot.col2Width
                        height: parent.height
                        color: rowItem.cellBgColor
                        antialiasing: false
                        Text {
                            anchors.centerIn: parent
                            text: model.end
                            font.pixelSize: gridAreaRoot.gridFontSize
                            font.family: gridAreaRoot.gridFontFamily
                            renderType: Text.NativeRendering
                            color: "#000000"
                        }
                    }
                    Rectangle { width: 1; height: parent.height; color: gridAreaRoot.colWinGridLines; antialiasing: false }

                    // Col 3: Characters per second (CPS)
                    Rectangle {
                        visible: gridAreaRoot.showColCPS
                        width: gridAreaRoot.col3Width
                        height: parent.height
                        color: gridAreaRoot.getCpsBgColor(model.cps, rowItem.cellBgColor)
                        antialiasing: false
                        Text {
                            anchors.centerIn: parent
                            text: model.cps
                            font.pixelSize: gridAreaRoot.gridFontSize
                            font.family: gridAreaRoot.gridFontFamily
                            renderType: Text.NativeRendering
                            color: {
                                var c = parseInt(model.cps, 10);
                                if (c > 22) return "#800000";
                                if (c > 18) return "#604000";
                                return "#000000";
                            }
                        }
                    }
                    Rectangle { visible: gridAreaRoot.showColCPS; width: 1; height: parent.height; color: gridAreaRoot.colWinGridLines; antialiasing: false }

                    // Col 4: Style
                    Rectangle {
                        width: gridAreaRoot.col4Width
                        height: parent.height
                        color: rowItem.cellBgColor
                        antialiasing: false
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: model.style
                            font.pixelSize: gridAreaRoot.gridFontSize
                            font.family: gridAreaRoot.gridFontFamily
                            renderType: Text.NativeRendering
                            color: "#000000"
                        }
                    }
                    Rectangle { width: 1; height: parent.height; color: gridAreaRoot.colWinGridLines; antialiasing: false }

                    // Col Actor: Actor
                    Rectangle {
                        visible: gridAreaRoot.showColActor
                        width: gridAreaRoot.colActorWidth
                        height: parent.height
                        color: rowItem.cellBgColor
                        antialiasing: false
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: (typeof model.actor !== "undefined") ? model.actor : ""
                            font.pixelSize: gridAreaRoot.gridFontSize
                            font.family: gridAreaRoot.gridFontFamily
                            renderType: Text.NativeRendering
                            color: "#000000"
                        }
                    }
                    Rectangle { visible: gridAreaRoot.showColActor; width: 1; height: parent.height; color: gridAreaRoot.colWinGridLines; antialiasing: false }

                    // Col Effect: Effect
                    Rectangle {
                        visible: gridAreaRoot.showColEffect
                        width: gridAreaRoot.colEffectWidth
                        height: parent.height
                        color: rowItem.cellBgColor
                        antialiasing: false
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: (typeof model.effect !== "undefined") ? model.effect : ""
                            font.pixelSize: gridAreaRoot.gridFontSize
                            font.family: gridAreaRoot.gridFontFamily
                            renderType: Text.NativeRendering
                            color: "#000000"
                        }
                    }
                    Rectangle { visible: gridAreaRoot.showColEffect; width: 1; height: parent.height; color: gridAreaRoot.colWinGridLines; antialiasing: false }

                    // Col Margin Left: Left margin
                    Rectangle {
                        visible: gridAreaRoot.showColMarginLeft
                        width: gridAreaRoot.colMarginLeftWidth
                        height: parent.height
                        color: rowItem.cellBgColor
                        antialiasing: false
                        Text {
                            anchors.centerIn: parent
                            text: (typeof model.marginLeft !== "undefined") ? model.marginLeft : "0"
                            font.pixelSize: gridAreaRoot.gridFontSize
                            font.family: gridAreaRoot.gridFontFamily
                            renderType: Text.NativeRendering
                            color: "#000000"
                        }
                    }
                    Rectangle { visible: gridAreaRoot.showColMarginLeft; width: 1; height: parent.height; color: gridAreaRoot.colWinGridLines; antialiasing: false }

                    // Col Margin Right: Right margin
                    Rectangle {
                        visible: gridAreaRoot.showColMarginRight
                        width: gridAreaRoot.colMarginRightWidth
                        height: parent.height
                        color: rowItem.cellBgColor
                        antialiasing: false
                        Text {
                            anchors.centerIn: parent
                            text: (typeof model.marginRight !== "undefined") ? model.marginRight : "0"
                            font.pixelSize: gridAreaRoot.gridFontSize
                            font.family: gridAreaRoot.gridFontFamily
                            renderType: Text.NativeRendering
                            color: "#000000"
                        }
                    }
                    Rectangle { visible: gridAreaRoot.showColMarginRight; width: 1; height: parent.height; color: gridAreaRoot.colWinGridLines; antialiasing: false }

                    // Col Margin Vert: Vertical margin
                    Rectangle {
                        visible: gridAreaRoot.showColMarginVert
                        width: gridAreaRoot.colMarginVertWidth
                        height: parent.height
                        color: rowItem.cellBgColor
                        antialiasing: false
                        Text {
                            anchors.centerIn: parent
                            text: (typeof model.marginVert !== "undefined") ? model.marginVert : "0"
                            font.pixelSize: gridAreaRoot.gridFontSize
                            font.family: gridAreaRoot.gridFontFamily
                            renderType: Text.NativeRendering
                            color: "#000000"
                        }
                    }
                    Rectangle { visible: gridAreaRoot.showColMarginVert; width: 1; height: parent.height; color: gridAreaRoot.colWinGridLines; antialiasing: false }

                    // Col 5: Subtitle text
                    Rectangle {
                        width: Math.max(80, gridView.width - (
                            gridAreaRoot.col0Width + 1 +
                            (gridAreaRoot.showColLayer ? gridAreaRoot.colLayerWidth + 1 : 0) +
                            gridAreaRoot.col1Width + 1 +
                            gridAreaRoot.col2Width + 1 +
                            (gridAreaRoot.showColCPS ? gridAreaRoot.col3Width + 1 : 0) +
                            gridAreaRoot.col4Width + 1 +
                            (gridAreaRoot.showColActor ? gridAreaRoot.colActorWidth + 1 : 0) +
                            (gridAreaRoot.showColEffect ? gridAreaRoot.colEffectWidth + 1 : 0) +
                            (gridAreaRoot.showColMarginLeft ? gridAreaRoot.colMarginLeftWidth + 1 : 0) +
                            (gridAreaRoot.showColMarginRight ? gridAreaRoot.colMarginRightWidth + 1 : 0) +
                            (gridAreaRoot.showColMarginVert ? gridAreaRoot.colMarginVertWidth + 1 : 0)
                        ))
                        height: parent.height
                        color: rowItem.cellBgColor
                        clip: true
                        antialiasing: false
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 6
                            anchors.right: parent.right
                            anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            text: gridAreaRoot.formatGridText(model.text, gridAreaRoot.project.tagHidingMode)
                            elide: Text.ElideRight
                            font.pixelSize: gridAreaRoot.gridFontSize
                            font.family: gridAreaRoot.gridFontFamily
                            renderType: Text.NativeRendering
                            color: "#000000"
                        }
                    }
                }

                // Bottom grid line divider
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: gridAreaRoot.colWinGridLines
                    antialiasing: false
                }

                // Active row indicator border (magenta box matching Aegisub BaseGrid active row)
                Rectangle {
                    visible: rowItem.isSelected
                    anchors.fill: parent
                    color: "transparent"
                    border.color: gridAreaRoot.colWinActiveBorder
                    border.width: 1
                    antialiasing: false
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            if (!gridAreaRoot.project.isRowSelected(index)) {
                                gridAreaRoot.project.selectRow(index, false, false);
                            }
                            var p = mapToItem(gridAreaRoot, mouse.x, mouse.y);
                            gridContextMenu.x = Math.max(0, Math.min(gridAreaRoot.width - gridContextMenu.implicitWidth - 5, p.x));
                            gridContextMenu.y = Math.max(0, Math.min(gridAreaRoot.height - 430, p.y));
                            gridContextMenu.open();
                        } else {
                            var isMulti = (Qt.platform.os === "osx" || Qt.platform.os === "macos")
                                ? Boolean(mouse.modifiers & Qt.MetaModifier)
                                : Boolean(mouse.modifiers & Qt.ControlModifier);
                            gridAreaRoot.project.selectRow(index, isMulti, mouse.modifiers & Qt.ShiftModifier);
                        }
                    }
                }
            }

            // Viewport background context menu trigger
            MouseArea {
                anchors.fill: parent
                z: -1
                acceptedButtons: Qt.RightButton
                onClicked: (mouse) => {
                    var p = mapToItem(gridAreaRoot, mouse.x, mouse.y);
                    gridContextMenu.x = Math.max(0, Math.min(gridAreaRoot.width - gridContextMenu.implicitWidth - 5, p.x));
                    gridContextMenu.y = Math.max(0, Math.min(gridAreaRoot.height - 430, p.y));
                    gridContextMenu.open();
                }
            }
        }
    }
}

    // Header column visibility context menu
    Popup {
        id: headerContextMenu
        implicitWidth: 175
        padding: 0
        topPadding: 3
        bottomPadding: 3
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 4
            color: "#ffffff"
            border.color: "#dcdcdc"
            border.width: 1
        }

        contentItem: Column {
            spacing: 0
            width: parent.width

            component HeaderMenuItem: Rectangle {
                id: hMi
                property string itemText: ""
                property bool isCheckable: true
                property bool isChecked: false
                signal triggered()

                width: parent.width
                height: 21
                color: hMiArea.containsMouse ? "#e8e8e8" : "transparent"
                radius: 4

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    Text {
                        text: hMi.isChecked ? "✓" : ""
                        font.pixelSize: 11
                        font.bold: true
                        color: "#0078d4"
                        Layout.preferredWidth: 14
                        visible: hMi.isCheckable
                    }

                    Text {
                        Layout.fillWidth: true
                        text: hMi.itemText
                        font.pixelSize: 12
                        font.family: gridAreaRoot.gridFontFamily
                        renderType: Text.NativeRendering
                        color: "#1e1e1e"
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                MouseArea {
                    id: hMiArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        hMi.triggered();
                        headerContextMenu.close();
                    }
                }
            }

            HeaderMenuItem {
                itemText: qsTr("Layer")
                isChecked: gridAreaRoot.showColLayer
                onTriggered: gridAreaRoot.showColLayer = !gridAreaRoot.showColLayer
            }
            HeaderMenuItem {
                itemText: qsTr("Characters Per Second")
                isChecked: gridAreaRoot.showColCPS
                onTriggered: gridAreaRoot.showColCPS = !gridAreaRoot.showColCPS
            }
            HeaderMenuItem {
                itemText: qsTr("Actor")
                isChecked: gridAreaRoot.showColActor
                onTriggered: gridAreaRoot.showColActor = !gridAreaRoot.showColActor
            }
            HeaderMenuItem {
                itemText: qsTr("Effect")
                isChecked: gridAreaRoot.showColEffect
                onTriggered: gridAreaRoot.showColEffect = !gridAreaRoot.showColEffect
            }
            HeaderMenuItem {
                itemText: qsTr("Left Margin")
                isChecked: gridAreaRoot.showColMarginLeft
                onTriggered: gridAreaRoot.showColMarginLeft = !gridAreaRoot.showColMarginLeft
            }
            HeaderMenuItem {
                itemText: qsTr("Right Margin")
                isChecked: gridAreaRoot.showColMarginRight
                onTriggered: gridAreaRoot.showColMarginRight = !gridAreaRoot.showColMarginRight
            }
            HeaderMenuItem {
                itemText: qsTr("Vertical Margin")
                isChecked: gridAreaRoot.showColMarginVert
                onTriggered: gridAreaRoot.showColMarginVert = !gridAreaRoot.showColMarginVert
            }
            Rectangle {
                width: parent.width - 12
                height: 1
                color: "#e5e5e5"
                anchors.horizontalCenter: parent.horizontalCenter
            }
            HeaderMenuItem {
                itemText: qsTr("Reset columns")
                isCheckable: false
                onTriggered: {
                    gridAreaRoot.col0Width = 28;
                    gridAreaRoot.colLayerWidth = 36;
                    gridAreaRoot.col1Width = 76;
                    gridAreaRoot.col2Width = 76;
                    gridAreaRoot.col3Width = 42;
                    gridAreaRoot.col4Width = 72;
                    gridAreaRoot.colActorWidth = 65;
                    gridAreaRoot.colEffectWidth = 60;
                    gridAreaRoot.colMarginLeftWidth = 36;
                    gridAreaRoot.colMarginRightWidth = 36;
                    gridAreaRoot.colMarginVertWidth = 36;
                    gridAreaRoot.showColLayer = false;
                    gridAreaRoot.showColCPS = true;
                    gridAreaRoot.showColActor = false;
                    gridAreaRoot.showColEffect = false;
                    gridAreaRoot.showColMarginLeft = false;
                    gridAreaRoot.showColMarginRight = false;
                    gridAreaRoot.showColMarginVert = false;
                }
            }
        }
    }

    // Subtitle grid row context menu
    Popup {
        id: gridContextMenu
        implicitWidth: 232
        padding: 0
        topPadding: 3
        bottomPadding: 3
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 4
            color: "#ffffff"
            border.color: "#dcdcdc"
            border.width: 1
        }

        contentItem: Column {
            spacing: 0
            width: parent.width

            component ContextMenuItem: Rectangle {
                id: cItem
                property string itemText: ""
                property string itemShortcut: ""
                property string itemIcon: ""
                property bool itemEnabled: true
                signal triggered()

                width: parent.width - 6
                anchors.horizontalCenter: parent.horizontalCenter
                height: 20
                radius: 4
                color: (hovered && itemEnabled) ? "#e8e8e8" : "transparent"
                border.color: "transparent"

                property bool hovered: itemMouse.containsMouse

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 6

                    Item {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        Image {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            fillMode: Image.Pad
                            smooth: false
                            source: cItem.itemIcon
                            visible: cItem.itemIcon !== ""
                            opacity: cItem.itemEnabled ? 1.0 : 0.35
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: cItem.itemText
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        renderType: Text.NativeRendering
                        color: cItem.itemEnabled ? "#1e1e1e" : "#a0a0a0"
                        elide: Text.ElideRight
                    }

                    Text {
                        text: cItem.itemShortcut
                        font.pixelSize: 11
                        font.family: uiTheme.uiFont
                        renderType: Text.NativeRendering
                        color: cItem.itemEnabled ? "#6e6e6e" : "#b0b0b0"
                        visible: cItem.itemShortcut !== ""
                    }
                }

                MouseArea {
                    id: itemMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: cItem.itemEnabled ? Qt.ArrowCursor : Qt.ArrowCursor
                    onClicked: {
                        if (cItem.itemEnabled) {
                            gridContextMenu.close();
                            cItem.triggered();
                        }
                    }
                }
            }

            component ContextMenuSep: Item {
                width: parent.width
                height: 3
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width - 12
                    height: 1
                    color: "#e5e5e5"
                }
            }

            // Insert line before
            ContextMenuItem {
                id: itemInsertBefore
                itemText: qsTr("&Insert (before)")
                onTriggered: {
                    var vTime = gridAreaRoot.videoCtrl ? gridAreaRoot.videoCtrl.currentTime : 0;
                    gridAreaRoot.project.insertLine(true, false, vTime);
                }
            }
            // Insert line after
            ContextMenuItem {
                itemText: qsTr("Insert (after)")
                onTriggered: {
                    var vTime = gridAreaRoot.videoCtrl ? gridAreaRoot.videoCtrl.currentTime : 0;
                    gridAreaRoot.project.insertLine(false, false, vTime);
                }
            }
            // Insert line before at video time
            ContextMenuItem {
                itemText: qsTr("Insert at video time (before)")
                onTriggered: {
                    var vTime = gridAreaRoot.videoCtrl ? gridAreaRoot.videoCtrl.currentTime : 0;
                    gridAreaRoot.project.insertLine(true, true, vTime);
                }
            }
            // Insert line after at video time
            ContextMenuItem {
                itemText: qsTr("Insert at video time (after)")
                onTriggered: {
                    var vTime = gridAreaRoot.videoCtrl ? gridAreaRoot.videoCtrl.currentTime : 0;
                    gridAreaRoot.project.insertLine(false, true, vTime);
                }
            }

            ContextMenuSep {}

            // Duplicate selected lines
            ContextMenuItem {
                id: itemDuplicate
                itemText: qsTr("&Duplicate Lines")
                onTriggered: gridAreaRoot.project.duplicateSelectedLines()
            }
            // Split lines before current frame
            ContextMenuItem {
                itemText: qsTr("Split lines before current frame")
                itemShortcut: "Ctrl-D"
                onTriggered: {
                    var vTime = gridAreaRoot.videoCtrl ? gridAreaRoot.videoCtrl.currentTime : 0;
                    gridAreaRoot.project.splitLineAtFrame(-1, vTime);
                }
            }
            // Split lines after current frame
            ContextMenuItem {
                itemText: qsTr("Split lines after current frame")
                itemShortcut: "Ctrl-Shift-D"
                onTriggered: {
                    var vTime = gridAreaRoot.videoCtrl ? gridAreaRoot.videoCtrl.currentTime : 0;
                    gridAreaRoot.project.splitLineAtFrame(1, vTime);
                }
            }

            ContextMenuSep {}

            // Swap two selected lines
            ContextMenuItem {
                id: itemSwap
                itemText: qsTr("Swap Lines")
                itemIcon: "../../assets/icons_native/arrow_sort_16.png"
                itemEnabled: gridAreaRoot.project.selectedIndices.length === 2
                onTriggered: gridAreaRoot.project.swapSelectedLines()
            }
            // Join lines (concat)
            ContextMenuItem {
                itemText: qsTr("&Join (concatenate)")
                itemEnabled: gridAreaRoot.project.selectedIndices.length >= 2
                onTriggered: gridAreaRoot.project.joinSelectedLines(0)
            }
            // Join lines (keep first)
            ContextMenuItem {
                itemText: qsTr("Join (keep first)")
                itemEnabled: gridAreaRoot.project.selectedIndices.length >= 2
                onTriggered: gridAreaRoot.project.joinSelectedLines(1)
            }
            // Join lines (as karaoke)
            ContextMenuItem {
                itemText: qsTr("Join (as Karaoke)")
                itemEnabled: gridAreaRoot.project.selectedIndices.length >= 2
                onTriggered: gridAreaRoot.project.joinSelectedLines(2)
            }

            ContextMenuSep {}

            // Make continuous (change start)
            ContextMenuItem {
                itemText: qsTr("&Make times continuous (change start)")
                onTriggered: gridAreaRoot.project.makeTimesContinuous(true)
            }
            // Make continuous (change end)
            ContextMenuItem {
                itemText: qsTr("&Make times continuous (change end)")
                onTriggered: gridAreaRoot.project.makeTimesContinuous(false)
            }
            // Recombine lines
            ContextMenuItem {
                itemText: qsTr("Recom&bine Lines")
                itemEnabled: gridAreaRoot.project.selectedIndices.length >= 2
                onTriggered: gridAreaRoot.project.recombineSelectedLines()
            }

            ContextMenuSep {}

            // Create audio clip
            ContextMenuItem {
                itemText: qsTr("Create audio clip")
                onTriggered: gridAreaRoot.createAudioClipRequested()
            }

            ContextMenuSep {}

            // Cut lines
            ContextMenuItem {
                itemText: qsTr("Cu&t Lines")
                itemShortcut: "Ctrl-X"
                itemIcon: "../../assets/icons_native/cut_button_16.png"
                onTriggered: gridAreaRoot.project.cutSelectedLines()
            }
            // Copy lines
            ContextMenuItem {
                itemText: qsTr("&Copy Lines")
                itemShortcut: "Ctrl-C"
                itemIcon: "../../assets/icons_native/copy_button_16.png"
                onTriggered: gridAreaRoot.project.copySelectedLines()
            }
            // Paste lines
            ContextMenuItem {
                itemText: qsTr("&Paste Lines")
                itemShortcut: "Ctrl-V"
                itemIcon: "../../assets/icons_native/paste_button_16.png"
                onTriggered: gridAreaRoot.project.pasteLines(false)
            }
            // Paste lines over
            ContextMenuItem {
                itemText: qsTr("Paste Lines &Over...")
                itemShortcut: "Ctrl-Shift-V"
                onTriggered: gridAreaRoot.project.pasteLines(true)
            }

            ContextMenuSep {}

            // Delete lines
            ContextMenuItem {
                itemText: qsTr("De&lete Lines")
                itemShortcut: "Ctrl-Delete"
                itemIcon: "../../assets/icons_native/delete_button_16.png"
                onTriggered: gridAreaRoot.project.deleteSelectedLines()
            }

            ContextMenuSep {}

            // Export video clip of selected line
            ContextMenuItem {
                itemText: qsTr("Export &Clip from Video...")
                itemIcon: "../../assets/icons_native/export_menu_16.png"
                onTriggered: {
                    if (typeof videoController === "undefined" || !videoController || !videoController.hasVideo) return;
                    var selStart = (typeof audioController !== "undefined" && audioController) ? audioController.selectionStart : videoController.activeSubStart;
                    var selEnd = (typeof audioController !== "undefined" && audioController) ? audioController.selectionEnd : videoController.activeSubEnd;
                    if (selEnd <= selStart) return;

                    var dlDir = StandardPaths.writableLocation(StandardPaths.DownloadLocation);
                    var outName = (dlDir ? dlDir : ".") + "/clip_" + selStart + "_" + selEnd + ".mp4";
                    var ok = videoController.exportClip(outName, selStart, selEnd, true);
                    if (ok) {
                        gridAreaRoot.statusMessage(qsTr("Clip exported: ") + outName);
                    } else {
                        gridAreaRoot.statusMessage(qsTr("Clip export failed."));
                    }
                }
            }
        }
    }
}

