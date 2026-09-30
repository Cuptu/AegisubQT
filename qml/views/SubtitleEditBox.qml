// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as PlatformDialogs
import "../controls"
import "../project/AssUtils.js" as AssUtils

// Primary subtitle line editor: handles timestamps, styling attributes,
// formatting tags, original text comparison, and text area input.
Rectangle {
    id: editBoxRoot
    Layout.fillWidth: true
    Layout.fillHeight: true
    color: "#f0f0f0"
    radius: 0
    border.color: "#d0d0d0"
    border.width: 1

    required property var project
    property var dialogs: null
    property var videoCtrl: null

    // Typography metrics: 13px editor text (10pt equivalent), 12px UI controls
    readonly property string editFontFamily: "Microsoft YaHei UI"
    readonly property int uiFontSize: 12
    readonly property int textEditFontSize: 13
    property bool syncingFromProject: false
    property bool frameMode: false
    readonly property bool frameAvailable: !!videoCtrl && !!videoCtrl.hasTimecodes

    onFrameModeChanged: syncTimingFromProject()
    onFrameAvailableChanged: {
        if (!frameAvailable) frameMode = false;
        syncTimingFromProject();
    }

    signal statusMessage(string text)
    property alias fontDialog: fontDialog

    function finishFormatting(result) {
        if (!result || !result.success) return;
        syncFromProject();
        subtitleEditArea.select(result.selectionStart, result.selectionEnd);
        subtitleEditArea.forceActiveFocus();
    }
    function toggleFormatting(tag) {
        if (!project || !project.subtitleModel) return;
        finishFormatting(project.subtitleModel.toggleInlineFormatting(project.selectedIndices,
            project.currentSelectedIndex, subtitleEditArea.selectionStart, subtitleEditArea.selectionEnd, tag));
    }
    function chooseFont() {
        if (!project || !project.subtitleModel) return;
        var state = project.subtitleModel.inlineFormattingState(project.currentSelectedIndex, subtitleEditArea.cursorPosition);
        if (!state.font) return;
        var font = Qt.font({family: state.font, pointSize: state.size, bold: state.b,
            italic: state.i, underline: state.u, strikeout: state.s});
        fontDialog.context = {model: project.subtitleModel, row: project.currentSelectedIndex,
            text: subtitleEditArea.text, start: subtitleEditArea.selectionStart, end: subtitleEditArea.selectionEnd,
            indices: project.selectedIndices.slice(), initialFont: font};
        fontDialog.selectedFont = font;
        fontDialog.open();
    }
    PlatformDialogs.FontDialog {
        id: fontDialog
        parentWindow: editBoxRoot.Window.window
        title: qsTr("Select font face and size")
        property var context: null
        onRejected: context = null
        onAccepted: {
            var saved = context;
            context = null;
            if (!saved || saved.model !== project.subtitleModel || saved.row !== project.currentSelectedIndex
                || saved.text !== project.subtitleModel.get(saved.row).text) return;
            if (selectedFont.toString() === saved.initialFont.toString()) return;
            editBoxRoot.finishFormatting(saved.model.applyInlineFont(saved.indices, saved.row,
                saved.start, saved.end, selectedFont));
        }
    }

    function syncTimingFromProject() {
        if (!project || !project.subtitleModel) return;
        var row = project.currentSelectedIndex;
        if (row < 0 || row >= project.subtitleModel.count) return;
        var model = project.subtitleModel;
        var start = model.getLineStartMs(row);
        var end = model.getLineEndMs(row);
        if (frameMode && frameAvailable) {
            var first = videoCtrl.frameAtTimeMs(start, 1);
            var last = videoCtrl.frameAtTimeMs(end, 2);
            startTimeField.text = String(first);
            endTimeField.text = String(last);
            durationField.text = String(Math.max(0, last - first + 1));
        } else {
            var line = model.get(row);
            startTimeField.text = line.start;
            endTimeField.text = line.end;
            durationField.text = AssUtils.msToAss(Math.max(0, end - start));
        }
    }

    function commitTiming(field, value) {
        if (!project || !project.subtitleModel || syncingFromProject) return;
        var model = project.subtitleModel;
        var row = project.currentSelectedIndex;
        if (row < 0 || row >= model.count) return;
        var start = model.getLineStartMs(row);
        var end = model.getLineEndMs(row);
        var number;
        if (frameMode && frameAvailable) {
            if (!/^\d+$/.test(value)) { syncTimingFromProject(); return; }
            number = Number(value);
            if (!Number.isSafeInteger(number) || number > 2147483647 || (field === "duration" && number < 1)) {
                syncTimingFromProject(); return;
            }
            if (field === "duration") {
                number += videoCtrl.frameAtTimeMs(start, 1) - 1;
                if (number > 2147483647) { syncTimingFromProject(); return; }
            }
            number = videoCtrl.timeAtFrameMs(number, field === "start" ? 1 : 2);
        } else {
            if (!/^\d+:[0-5]\d:[0-5]\d\.\d{2}$/.test(value)) { syncTimingFromProject(); return; }
            number = AssUtils.assToMs(value);
            if (field === "duration") number += start;
        }
        if (!Number.isFinite(number) || number < 0 || number > 2147483647) {
            syncTimingFromProject(); return;
        }
        if (field === "start") { start = number; end = Math.max(start, end); }
        else { end = number; start = Math.min(start, end); }
        if (start !== model.getLineStartMs(row) || end !== model.getLineEndMs(row)) {
            project.pushUndo(qsTr("change timing"));
            model.setLineTimes(row, start, end);
            project.dataModified();
        }
        syncTimingFromProject();
    }

    function getCleanLongestLine(txt) {
        if (!txt) return 0;
        var cleaned = txt.replace(/\{[^}]*\}/g, "");
        var lines = cleaned.split(/\\N|\\n|\r?\n/);
        var maxLen = 0;
        for (var i = 0; i < lines.length; ++i) {
            if (lines[i].length > maxLen) maxLen = lines[i].length;
        }
        return maxLen;
    }

    function syncFromProject() {
        if (!project || !project.subtitleModel || project.currentSelectedIndex < 0 || project.currentSelectedIndex >= project.subtitleModel.count) return;
        var item = project.subtitleModel.get(project.currentSelectedIndex);
        if (!item) return;
        syncingFromProject = true;
        syncTimingFromProject();
        layerSpinMid.value = item.layer || 0;
        marginLeft.text = "" + (item.marginLeft || 0);
        marginRight.text = "" + (item.marginRight || 0);
        marginVert.text = "" + (item.marginVert || 0);
        commentCheck.checked = !!item.isComment;
        actorCombo.editText = item.actor || "";
        effectCombo.editText = item.effect || "";
        var sIdx = styleCombo.model.indexOf(item.style || "Default");
        if (sIdx >= 0) styleCombo.currentIndex = sIdx;
        subtitleEditArea.text = item.text || "";
        syncingFromProject = false;
    }

    function setEditedProperty(name, value, description) {
        if (syncingFromProject || !project || !project.subtitleModel || project.currentSelectedIndex < 0 ||
                project.currentSelectedIndex >= project.subtitleModel.count)
            return false;
        var oldValue = project.subtitleModel.get(project.currentSelectedIndex)[name];
        if (oldValue === value) return false;
        project.pushUndo(description);
        if (!project.subtitleModel.setProperty(project.currentSelectedIndex, name, value)) return false;
        project.dataModified();
        return true;
    }

    Connections {
        target: editBoxRoot.project
        function onCurrentSelectedIndexChanged() {
            editBoxRoot.syncFromProject();
        }
        function onLineSelected(idx, item) {
            editBoxRoot.syncFromProject();
        }
        function onSearchMatchFound(row, start, length) {
            if (row !== editBoxRoot.project.currentSelectedIndex) return;
            subtitleEditArea.select(start, start + length);
            subtitleEditArea.forceActiveFocus();
        }
    }

    Connections {
        target: editBoxRoot.videoCtrl
        function onTimecodesChanged() { editBoxRoot.syncTimingFromProject(); }
    }
    Connections {
        target: editBoxRoot.project ? editBoxRoot.project.subtitleModel : null
        function onDataChanged() { editBoxRoot.syncTimingFromProject(); }
        function onModelReset() { editBoxRoot.syncFromProject(); }
    }

    Component.onCompleted: {
        editBoxRoot.syncFromProject();
    }

    property alias subtitleEditArea: subtitleEditArea
    property alias editAreaContextMenu: editAreaContextMenu
    property alias originalAreaContextMenu: originalAreaContextMenu
    property alias commentCheck: commentCheck
    property alias styleCombo: styleCombo
    property alias actorCombo: actorCombo
    property alias effectCombo: effectCombo
    property alias charCountBox: charCountBox
    property alias layerSpinMid: layerSpinMid
    property alias startTimeField: startTimeField
    property alias endTimeField: endTimeField
    property alias durationField: durationField
    property alias marginLeft: marginLeft
    property alias marginRight: marginRight
    property alias marginVert: marginVert
    property alias radioTime: radioTime
    property alias radioFrame: radioFrame
    property alias chkRaw: chkRaw

    ColumnLayout {
        id: editBoxCol
        anchors.fill: parent
        anchors.margins: 3
        spacing: 2

        // Header row: comment toggle, style selector, actor, effect, and character count
        RowLayout {
            Layout.fillWidth: true
            spacing: 3

            CheckBox {
                id: commentCheck
                text: qsTr("Comment"); font.pixelSize: editBoxRoot.uiFontSize
                font.family: editBoxRoot.editFontFamily
                contentItem: Text {
                    text: commentCheck.text
                    font: commentCheck.font
                    renderType: Text.NativeRendering
                    color: "#1e1e1e"
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: commentCheck.indicator.width + 4
                }
                ToolTip.text: qsTr("Comment this line out. Commented lines don't show up on screen.")
                ToolTip.visible: hovered
                onToggled: {
                    editBoxRoot.setEditedProperty("isComment", checked, qsTr("toggle comment"));
                }
            }

            // Style selector
            NativeComboBox {
                id: styleCombo
                model: (typeof nativeSubtitleModel !== "undefined" && nativeSubtitleModel) ? nativeSubtitleModel.styleNames : ["Default"]
                implicitWidth: 100
                implicitHeight: 21
                ToolTip.text: qsTr("Style for this line")
                ToolTip.visible: hovered
                onActivated: (idx) => {
                    editBoxRoot.setEditedProperty("style", currentText, qsTr("change style"));
                }
            }

            // Style editor launcher
            Button {
                text: qsTr("Edit"); implicitHeight: 21
                implicitWidth: Math.max(40, contentItem.implicitWidth + 14)
                font.pixelSize: editBoxRoot.uiFontSize
                font.family: editBoxRoot.editFontFamily
                contentItem: Text {
                    text: parent.text
                    color: "#1e1e1e"
                    font: parent.font
                    renderType: Text.NativeRendering
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.pressed ? "#d0d0d0" : (parent.hovered ? "#e0e0e0" : "#f5f5f5")
                    border.color: "#c0c0c0"
                    border.width: 1
                    radius: 0
                }
                onClicked: {
                    if (dialogs && dialogs.dlgStyleEditor) {
                        var curStyleName = styleCombo.currentText || "Default";
                        var st = (typeof nativeSubtitleModel !== "undefined" && nativeSubtitleModel) ? nativeSubtitleModel.getStyleByName(curStyleName) : null;
                        var stIdx = styleCombo.currentIndex;
                        dialogs.dlgStyleEditor.loadStyle(st, false, stIdx);
                        dialogs.dlgStyleEditor.open();
                    }
                }
            }

            // Actor selector
            NativeComboBox {
                id: actorCombo
                editable: true
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                implicitHeight: 21
                model: []
                ToolTip.text: qsTr("Actor name for this speech. This is only for reference, and is mainly useless.")
                ToolTip.visible: hovered
                onAccepted: {
                    editBoxRoot.setEditedProperty("actor", editText, qsTr("change actor"));
                }
            }

            // Effect selector
            NativeComboBox {
                id: effectCombo
                editable: true
                Layout.fillWidth: true
                Layout.preferredWidth: 90
                implicitHeight: 21
                model: []
                ToolTip.text: qsTr("Effect for this line. This can be used to store extra information for karaoke scripts, or for the effects supported by the renderer.")
                ToolTip.visible: hovered
                onAccepted: {
                    editBoxRoot.setEditedProperty("effect", editText, qsTr("change effect"));
                }
            }

            TextField {
                id: charCountBox
                property int countVal: editBoxRoot.getCleanLongestLine(subtitleEditArea.text)
                text: "" + countVal
                readOnly: true
                implicitHeight: 21
                implicitWidth: 32
                font.pixelSize: editBoxRoot.uiFontSize
                font.family: editBoxRoot.editFontFamily
                renderType: Text.NativeRendering
                horizontalAlignment: TextInput.AlignHCenter
                color: countVal > 30 ? "#c00000" : "#505050"
                background: Rectangle {
                    color: charCountBox.countVal > 30 ? "#ffc8c8" : "#f5f5f5"
                    radius: 0
                    border.color: charCountBox.countVal > 30 ? "#ff9999" : "#cecece"
                    border.width: 1
                }
                ToolTip.text: qsTr("Number of characters in the longest line of this subtitle.")
                ToolTip.visible: hovered
            }
        }

        // Timing row: layer, start/end timestamps, duration, and margins
        RowLayout {
            Layout.fillWidth: true
            spacing: 3

            SpinBox {
                id: layerSpinMid
                from: 0
                to: 999
                value: 0
                implicitHeight: 21
                implicitWidth: 44
                font.pixelSize: editBoxRoot.uiFontSize
                font.family: editBoxRoot.editFontFamily
                contentItem: TextInput {
                    text: layerSpinMid.textFromValue(layerSpinMid.value, layerSpinMid.locale)
                    color: "#1e1e1e"
                    font: layerSpinMid.font
                    renderType: Text.NativeRendering
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                    readOnly: !layerSpinMid.editable
                }
                background: Rectangle {
                    color: "#ffffff"
                    radius: 0
                    border.color: layerSpinMid.activeFocus ? "#0078d4" : "#cecece"
                    border.width: layerSpinMid.activeFocus ? 1.5 : 1
                }
                ToolTip.text: qsTr("Layer number"); ToolTip.visible: hovered
                onValueModified: {
                    if (editBoxRoot.project && editBoxRoot.project.currentSelectedIndex >= 0) {
                        editBoxRoot.setEditedProperty("layer", value, qsTr("change layer"));
                    }
                }
            }

            // Start timestamp
            TextField {
                id: startTimeField
                objectName: "editor-start-time"
                text: ""
                implicitWidth: 70
                implicitHeight: 21
                font.pixelSize: editBoxRoot.uiFontSize
                font.family: editBoxRoot.editFontFamily
                renderType: Text.NativeRendering
                color: "#1e1e1e"
                horizontalAlignment: TextInput.AlignHCenter
                selectByMouse: true
                background: Rectangle {
                    color: "#ffffff"
                    radius: 0
                    border.color: startTimeField.activeFocus ? "#0078d4" : "#cecece"
                    border.width: startTimeField.activeFocus ? 1.5 : 1
                }
                ToolTip.text: qsTr("Start time"); ToolTip.visible: hovered
                onEditingFinished: {
                    editBoxRoot.commitTiming("start", text);
                }
            }

            // End timestamp
            TextField {
                id: endTimeField
                objectName: "editor-end-time"
                text: ""
                implicitWidth: 70
                implicitHeight: 21
                font.pixelSize: editBoxRoot.uiFontSize
                font.family: editBoxRoot.editFontFamily
                renderType: Text.NativeRendering
                color: "#1e1e1e"
                horizontalAlignment: TextInput.AlignHCenter
                selectByMouse: true
                background: Rectangle {
                    color: "#ffffff"
                    radius: 0
                    border.color: endTimeField.activeFocus ? "#0078d4" : "#cecece"
                    border.width: endTimeField.activeFocus ? 1.5 : 1
                }
                ToolTip.text: qsTr("End time"); ToolTip.visible: hovered
                onEditingFinished: {
                    editBoxRoot.commitTiming("end", text);
                }
            }

            // Duration field: bidirectionally syncs with end timestamp
            TextField {
                id: durationField
                objectName: "editor-duration"
                text: "0:00:05.00"
                implicitWidth: 70
                implicitHeight: 21
                font.pixelSize: editBoxRoot.uiFontSize
                font.family: editBoxRoot.editFontFamily
                renderType: Text.NativeRendering
                horizontalAlignment: TextInput.AlignHCenter
                readOnly: false
                selectByMouse: true
                color: "#1e1e1e"
                background: Rectangle {
                    color: "#ffffff"
                    radius: 0
                    border.color: durationField.activeFocus ? "#0078d4" : "#cecece"
                    border.width: durationField.activeFocus ? 1.5 : 1
                }
                ToolTip.text: qsTr("Line duration"); ToolTip.visible: hovered
                onEditingFinished: {
                    editBoxRoot.commitTiming("duration", text);
                }
            }

            // Margins: left, right, and vertical
            TextField {
                id: marginLeft
                text: "0"
                implicitWidth: 32
                implicitHeight: 21
                font.pixelSize: editBoxRoot.uiFontSize
                font.family: editBoxRoot.editFontFamily
                renderType: Text.NativeRendering
                color: "#1e1e1e"
                horizontalAlignment: TextInput.AlignHCenter
                selectByMouse: true
                background: Rectangle {
                    color: "#ffffff"
                    radius: 0
                    border.color: marginLeft.activeFocus ? "#0078d4" : "#cecece"
                    border.width: marginLeft.activeFocus ? 1.5 : 1
                }
                ToolTip.text: qsTr("Left Margin (0 = default from style)"); ToolTip.visible: hovered
                onEditingFinished: {
                    if (!editBoxRoot.project || editBoxRoot.project.currentSelectedIndex < 0) return;
                    editBoxRoot.setEditedProperty("marginLeft", parseInt(text, 10) || 0, qsTr("change left margin"));
                }
            }
            TextField {
                id: marginRight
                text: "0"
                implicitWidth: 32
                implicitHeight: 21
                font.pixelSize: editBoxRoot.uiFontSize
                font.family: editBoxRoot.editFontFamily
                renderType: Text.NativeRendering
                color: "#1e1e1e"
                horizontalAlignment: TextInput.AlignHCenter
                selectByMouse: true
                background: Rectangle {
                    color: "#ffffff"
                    radius: 0
                    border.color: marginRight.activeFocus ? "#0078d4" : "#cecece"
                    border.width: marginRight.activeFocus ? 1.5 : 1
                }
                ToolTip.text: qsTr("Right Margin (0 = default from style)"); ToolTip.visible: hovered
                onEditingFinished: {
                    if (!editBoxRoot.project || editBoxRoot.project.currentSelectedIndex < 0) return;
                    editBoxRoot.setEditedProperty("marginRight", parseInt(text, 10) || 0, qsTr("change right margin"));
                }
            }
            TextField {
                id: marginVert
                text: "0"
                implicitWidth: 32
                implicitHeight: 21
                font.pixelSize: editBoxRoot.uiFontSize
                font.family: editBoxRoot.editFontFamily
                renderType: Text.NativeRendering
                color: "#1e1e1e"
                horizontalAlignment: TextInput.AlignHCenter
                selectByMouse: true
                background: Rectangle {
                    color: "#ffffff"
                    radius: 0
                    border.color: marginVert.activeFocus ? "#0078d4" : "#cecece"
                    border.width: marginVert.activeFocus ? 1.5 : 1
                }
                ToolTip.text: qsTr("Vertical Margin (0 = default from style)"); ToolTip.visible: hovered
                onEditingFinished: {
                    if (!editBoxRoot.project || editBoxRoot.project.currentSelectedIndex < 0) return;
                    editBoxRoot.setEditedProperty("marginVert", parseInt(text, 10) || 0, qsTr("change vertical margin"));
                }
            }

            Item { Layout.fillWidth: true }
        }

        // Formatting row: inline tag buttons, color pickers, commit, timing unit, and raw toggle
        RowLayout {
            Layout.fillWidth: true
            spacing: 2

            // Formatting tag buttons (bold, italic, underline, strikeout, font name)
            component Win32FormatBtn: ToolButton {
                id: fb
                property string svgIcon: ""
                implicitWidth: 20
                focusPolicy: Qt.NoFocus
                implicitHeight: 20
                padding: 1

                contentItem: Image {
                    source: fb.svgIcon
                    width: 16
                    height: 16
                    fillMode: Image.Pad
                    smooth: false
                    anchors.centerIn: parent
                }

                background: Rectangle {
                    radius: 0
                    color: fb.pressed ? "#0000001a" : (fb.hovered ? "#0000000f" : "transparent")
                    border.color: "transparent"
                }
            }

            Win32FormatBtn { objectName: "editor-bold"; svgIcon: "../../assets/icons_native/button_bold_16.png"; ToolTip.text: qsTr("Toggle bold (\\b) for the current selection or at the current cursor position") + " (Ctrl+B)"; ToolTip.visible: hovered; onClicked: editBoxRoot.toggleFormatting("b") }
            Win32FormatBtn { objectName: "editor-italic"; svgIcon: "../../assets/icons_native/button_italics_16.png"; ToolTip.text: qsTr("Toggle italics (\\i) for the current selection or at the current cursor position") + " (Ctrl+I)"; ToolTip.visible: hovered; onClicked: editBoxRoot.toggleFormatting("i") }
            Win32FormatBtn { objectName: "editor-underline"; svgIcon: "../../assets/icons_native/button_underline_16.png"; ToolTip.text: qsTr("Toggle underline (\\u) for the current selection or at the current cursor position") + " (Ctrl+U)"; ToolTip.visible: hovered; onClicked: editBoxRoot.toggleFormatting("u") }
            Win32FormatBtn { objectName: "editor-strikeout"; svgIcon: "../../assets/icons_native/button_strikeout_16.png"; ToolTip.text: qsTr("Toggle strikeout (\\s) for the current selection or at the current cursor position"); ToolTip.visible: hovered; onClicked: editBoxRoot.toggleFormatting("s") }

            Win32FormatBtn { objectName: "editor-font"; svgIcon: "../../assets/icons_native/button_fontname_16.png"; ToolTip.text: qsTr("Select a font face and size"); ToolTip.visible: hovered; onClicked: editBoxRoot.chooseFont() }

            // ASS color override buttons: primary (\c), secondary (\2c), outline (\3c), shadow (\4c)
            Win32FormatBtn {
                svgIcon: "../../assets/icons_native/button_color_one_16.png"
                ToolTip.text: qsTr("Set the primary fill color (\\c) at the cursor position"); ToolTip.visible: hovered
                onClicked: {
                    if (dialogs) {
                        dialogs.dlgColorPicker.targetProp = "primary";
                        dialogs.dlgColorPicker.open();
                    }
                }
            }
            Win32FormatBtn {
                svgIcon: "../../assets/icons_native/button_color_two_16.png"
                ToolTip.text: qsTr("Set the secondary (karaoke) fill color (\\2c) at the cursor position"); ToolTip.visible: hovered
                onClicked: {
                    if (dialogs) {
                        dialogs.dlgColorPicker.targetProp = "secondary";
                        dialogs.dlgColorPicker.open();
                    }
                }
            }
            Win32FormatBtn {
                svgIcon: "../../assets/icons_native/button_color_three_16.png"
                ToolTip.text: qsTr("Set the outline color (\\3c) at the cursor position"); ToolTip.visible: hovered
                onClicked: {
                    if (dialogs) {
                        dialogs.dlgColorPicker.targetProp = "outline";
                        dialogs.dlgColorPicker.open();
                    }
                }
            }
            Win32FormatBtn {
                svgIcon: "../../assets/icons_native/button_color_four_16.png"
                ToolTip.text: qsTr("Set the shadow color (\\4c) at the cursor position"); ToolTip.visible: hovered
                onClicked: {
                    if (dialogs) {
                        dialogs.dlgColorPicker.targetProp = "shadow";
                        dialogs.dlgColorPicker.open();
                    }
                }
            }

            // Commit and advance button (Enter)
            ToolButton {
                id: commitBtn
                implicitWidth: 20
                implicitHeight: 20
                padding: 1
                contentItem: Image {
                    source: "../../assets/icons_native/button_audio_commit_16.png"
                    width: 16
                    height: 16
                    fillMode: Image.Pad
                    smooth: false
                    anchors.centerIn: parent
                }
                background: Rectangle {
                    radius: 0
                    color: commitBtn.pressed ? "#c4ecc9" : (commitBtn.hovered ? "#e0fae4" : "transparent")
                    border.color: commitBtn.hovered ? "#70a070" : "transparent"
                    border.width: 1
                }
                ToolTip.text: qsTr("Move to the next subtitle line, creating a new one if needed") + " (Enter)"; ToolTip.visible: hovered
                onClicked: {
                    if (!editBoxRoot.project || editBoxRoot.project.subtitleModel.count === 0) return;
                    if (editBoxRoot.project.currentSelectedIndex < editBoxRoot.project.subtitleModel.count - 1) {
                        editBoxRoot.project.selectRow(editBoxRoot.project.currentSelectedIndex + 1, false, false);
                    } else {
                        editBoxRoot.project.insertLine(false, false, 0);
                    }
                }
            }

            // Time vs frame display toggle
            RadioButton {
                id: radioTime
                objectName: "editor-time-mode"
                text: qsTr("T&ime"); checked: !editBoxRoot.frameMode
                onClicked: editBoxRoot.frameMode = false
                ToolTip.text: qsTr("Time by h:mm:ss.cs"); ToolTip.visible: hovered
                font.pixelSize: editBoxRoot.uiFontSize
                font.family: editBoxRoot.editFontFamily
                contentItem: Text {
                    text: radioTime.text
                    font: radioTime.font
                    renderType: Text.NativeRendering
                    color: "#1e1e1e"
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: radioTime.indicator.width + 3
                }
            }
            RadioButton {
                id: radioFrame
                objectName: "editor-frame-mode"
                text: qsTr("F&rame"); enabled: editBoxRoot.frameAvailable
                checked: editBoxRoot.frameMode
                onClicked: editBoxRoot.frameMode = true
                ToolTip.text: qsTr("Time by frame number"); ToolTip.visible: hovered
                font.pixelSize: editBoxRoot.uiFontSize
                font.family: editBoxRoot.editFontFamily
                contentItem: Text {
                    text: radioFrame.text
                    font: radioFrame.font
                    renderType: Text.NativeRendering
                    color: radioFrame.enabled ? "#1e1e1e" : "#888888"
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: radioFrame.indicator.width + 3
                }
            }

            CheckBox {
                id: chkRaw
                text: qsTr("Show Original"); font.pixelSize: editBoxRoot.uiFontSize
                ToolTip.text: qsTr("Show the contents of the subtitle line when it was first selected above the edit box. This is sometimes useful when editing subtitles or translating subtitles into another language."); ToolTip.visible: hovered
                font.family: editBoxRoot.editFontFamily
                contentItem: Text {
                    text: chkRaw.text
                    font: chkRaw.font
                    renderType: Text.NativeRendering
                    color: "#1e1e1e"
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: chkRaw.indicator.width + 3
                }
            }

            Item { Layout.fillWidth: true }
        }

        // Original text comparison area (visible when raw compare mode is active)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: chkRaw.checked ? 46 : 0
            visible: chkRaw.checked
            color: "#f8f9fa"
            radius: 0
            border.color: "#d0d0d0"
            border.width: 1
            clip: true

            ScrollView {
                anchors.fill: parent
                anchors.margins: 2
                TextArea {
                    id: originalEditArea
                    text: editBoxRoot.project.initialSelectedText
                    readOnly: true
                    font.pixelSize: editBoxRoot.textEditFontSize
                    font.family: editBoxRoot.editFontFamily
                    renderType: Text.NativeRendering
                    color: "#555555"
                    wrapMode: TextArea.Wrap
                    selectByMouse: true
                    padding: 3
                    background: null

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        cursorShape: Qt.IBeamCursor
                        onPressed: (mouse) => {
                            originalEditArea.forceActiveFocus();
                            var clickPos = originalEditArea.positionAt(mouse.x, mouse.y);
                            if (originalEditArea.selectionStart === originalEditArea.selectionEnd ||
                                clickPos < originalEditArea.selectionStart ||
                                clickPos > originalEditArea.selectionEnd) {
                                originalEditArea.cursorPosition = clickPos;
                            }
                            originalAreaContextMenu.popup();
                        }
                    }
                }
            }
        }

        // Text manipulation actions: revert, clear line, clear text without tags, insert original
        RowLayout {
            Layout.fillWidth: true
            implicitHeight: chkRaw.checked ? 20 : 0
            visible: chkRaw.checked
            spacing: 4

            component ProofBtn: Button {
                id: pBtn
                implicitHeight: 18
                padding: 2
                background: Rectangle {
                    radius: 0
                    color: pBtn.pressed ? "#d0d0d0" : (pBtn.hovered ? "#e8e8e8" : "#f0f0f0")
                    border.color: "#bebebe"
                    border.width: 1
                }
                contentItem: Text {
                    text: pBtn.text
                    font.pixelSize: editBoxRoot.uiFontSize
                    font.family: editBoxRoot.editFontFamily
                    renderType: Text.NativeRendering
                    color: "#222222"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            ProofBtn {
                text: qsTr("Revert"); ToolTip.text: qsTr("Revert the active line to its initial state (shown in the upper editor)"); ToolTip.visible: hovered
                onClicked: subtitleEditArea.text = editBoxRoot.project.initialSelectedText
            }
            ProofBtn {
                text: qsTr("Clear"); ToolTip.text: qsTr("Clear the current line's text"); ToolTip.visible: hovered
                onClicked: subtitleEditArea.text = ""
            }
            ProofBtn {
                text: qsTr("Clear Text"); ToolTip.text: qsTr("Clear the current line's text, leaving override tags"); ToolTip.visible: hovered
                onClicked: {
                    var t = subtitleEditArea.text;
                    var tags = t.match(/\{[^}]*\}/g) || [];
                    subtitleEditArea.text = tags.join("");
                }
            }
            ProofBtn {
                text: qsTr("Insert Original"); ToolTip.text: qsTr("Insert the original line text at the cursor"); ToolTip.visible: hovered
                onClicked: subtitleEditArea.insert(subtitleEditArea.cursorPosition, editBoxRoot.project.initialSelectedText)
            }
            Item { Layout.fillWidth: true }
        }

        // Primary subtitle dialogue text editor
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitHeight: 78
            color: "#ffffff"
            radius: 0
            border.color: subtitleEditArea.activeFocus ? "#0078d4" : "#cecece"
            border.width: subtitleEditArea.activeFocus ? 1.5 : 1
            clip: true

            ScrollView {
                anchors.fill: parent
                anchors.margins: 2

                TextArea {
                    id: subtitleEditArea
                    text: (editBoxRoot.project.subtitleModel.count > 0 && editBoxRoot.project.currentSelectedIndex < editBoxRoot.project.subtitleModel.count && editBoxRoot.project.subtitleModel.get(editBoxRoot.project.currentSelectedIndex)) ? editBoxRoot.project.subtitleModel.get(editBoxRoot.project.currentSelectedIndex).text : ""
                    font.pixelSize: editBoxRoot.textEditFontSize
                    font.family: editBoxRoot.editFontFamily
                    renderType: Text.NativeRendering
                    color: "#000000"
                    wrapMode: TextArea.Wrap
                    selectByMouse: true
                    padding: 4
                    persistentSelection: true
                    background: null
                    onTextChanged: {
                        if (!editBoxRoot.syncingFromProject && editBoxRoot.project.currentSelectedIndex < editBoxRoot.project.subtitleModel.count && editBoxRoot.project.currentSelectedIndex >= 0) {
                            editBoxRoot.setEditedProperty("text", text, qsTr("edit text"))
                            if (editBoxRoot.videoCtrl) {
                                var item = editBoxRoot.project.subtitleModel.get(editBoxRoot.project.currentSelectedIndex)
                                editBoxRoot.videoCtrl.parseAndSetActiveSubtitle(item.start, item.end, text)
                            }
                        }
                    }
                    Keys.onPressed: (event) => {
                        if (event.modifiers === Qt.ControlModifier && (event.key === Qt.Key_B || event.key === Qt.Key_I || event.key === Qt.Key_U)) {
                            editBoxRoot.toggleFormatting(event.key === Qt.Key_B ? "b" : event.key === Qt.Key_I ? "i" : "u");
                            event.accepted = true;
                        } else if (event.text === "\\") {
                            tagAssistPopup.open();
                        } else if (event.key === Qt.Key_Menu || (event.modifiers & Qt.ShiftModifier && event.key === Qt.Key_F10)) {
                            editAreaContextMenu.popup();
                            event.accepted = true;
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        cursorShape: Qt.IBeamCursor
                        onPressed: (mouse) => {
                            subtitleEditArea.forceActiveFocus();
                            var clickPos = subtitleEditArea.positionAt(mouse.x, mouse.y);
                            if (subtitleEditArea.selectionStart === subtitleEditArea.selectionEnd ||
                                clickPos < subtitleEditArea.selectionStart ||
                                clickPos > subtitleEditArea.selectionEnd) {
                                subtitleEditArea.cursorPosition = clickPos;
                            }
                            editAreaContextMenu.popup();
                        }
                    }
                }
            }

            // ASS override tag auto-completion popup
            Popup {
                id: tagAssistPopup
                x: 20
                y: 20
                width: 140
                height: 170
                padding: 2
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                background: Rectangle {
                    radius: 4
                    color: "#ffffff"
                    border.color: "#dcdcdc"
                    border.width: 1
                }
                contentItem: ListView {
                    id: tagListView
                    clip: true
                    model: [
                        "pos(x,y)", "fad(in,out)", "an1", "an2", "an5", "an7", "an8",
                        "fn", "fs", "c&H", "1c&H", "3c&H", "4c&H", "b1", "i1",
                        "bord", "shad", "blur", "fscx", "fscy", "frz", "frx", "fry",
                        "k", "kf", "clip()", "t()", "N"
                    ]
                    delegate: Rectangle {
                        width: tagListView.width
                        height: 18
                        radius: 0
                        color: tagMa.containsMouse ? "#e8f2fe" : "transparent"
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: "\\" + modelData
                            font.pixelSize: 11
                            font.family: uiTheme.monoFont
                            color: "#0066cc"
                        }
                        MouseArea {
                            id: tagMa
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                subtitleEditArea.insert(subtitleEditArea.cursorPosition, modelData);
                                tagAssistPopup.close();
                            }
                        }
                    }
                }
            }
        }
    }

    NativeMenu {
        id: editAreaContextMenu
        Action {
            text: qsTr("&Undo") + "\tCtrl+Z"
            icon.source: "../../assets/icons_native/undo_button_16.png"
            enabled: subtitleEditArea.canUndo
            onTriggered: subtitleEditArea.undo()
        }
        Action {
            text: qsTr("&Redo") + "\tCtrl+Y"
            icon.source: "../../assets/icons_native/redo_button_16.png"
            enabled: subtitleEditArea.canRedo
            onTriggered: subtitleEditArea.redo()
        }
        NativeMenuSep {}
        Action {
            text: qsTr("Cu&t") + "\tCtrl+X"
            icon.source: "../../assets/icons_native/cut_button_16.png"
            enabled: subtitleEditArea.selectedText.length > 0
            onTriggered: subtitleEditArea.cut()
        }
        Action {
            text: qsTr("&Copy") + "\tCtrl+C"
            icon.source: "../../assets/icons_native/copy_button_16.png"
            enabled: subtitleEditArea.selectedText.length > 0
            onTriggered: subtitleEditArea.copy()
        }
        Action {
            text: qsTr("&Paste") + "\tCtrl+V"
            icon.source: "../../assets/icons_native/paste_button_16.png"
            enabled: subtitleEditArea.canPaste
            onTriggered: subtitleEditArea.paste()
        }
        Action {
            text: qsTr("&Delete") + "\tDel"
            icon.source: "../../assets/icons_native/delete_button_16.png"
            enabled: subtitleEditArea.selectedText.length > 0
            onTriggered: subtitleEditArea.remove(subtitleEditArea.selectionStart, subtitleEditArea.selectionEnd)
        }
        NativeMenuSep {}
        Action {
            text: qsTr("Select &All") + "\tCtrl+A"
            enabled: subtitleEditArea.length > 0
            onTriggered: subtitleEditArea.selectAll()
        }
        NativeMenuSep {}
        Action {
            text: qsTr("Split at cursor (preserve times)")
            enabled: !!editBoxRoot.project && subtitleEditArea.cursorPosition > 0 && subtitleEditArea.cursorPosition < subtitleEditArea.text.length
            onTriggered: editBoxRoot.project.splitLineAtCursor(subtitleEditArea.cursorPosition, 0, 0)
        }
        Action {
            text: qsTr("Split at cursor (estimate times)")
            enabled: !!editBoxRoot.project && subtitleEditArea.cursorPosition > 0 && subtitleEditArea.cursorPosition < subtitleEditArea.text.length
            onTriggered: editBoxRoot.project.splitLineAtCursor(subtitleEditArea.cursorPosition, 1, 0)
        }
        Action {
            text: qsTr("Split at cursor (at video frame)")
            enabled: !!editBoxRoot.project && !!editBoxRoot.videoCtrl && subtitleEditArea.cursorPosition > 0 && subtitleEditArea.cursorPosition < subtitleEditArea.text.length
            onTriggered: editBoxRoot.project.splitLineAtCursor(subtitleEditArea.cursorPosition, 2, editBoxRoot.videoCtrl.currentTime)
        }
    }

    NativeMenu {
        id: originalAreaContextMenu
        Action {
            text: qsTr("&Copy") + "\tCtrl+C"
            icon.source: "../../assets/icons_native/copy_button_16.png"
            enabled: originalEditArea.selectedText.length > 0
            onTriggered: originalEditArea.copy()
        }
        Action {
            text: qsTr("Select &All") + "\tCtrl+A"
            enabled: originalEditArea.length > 0
            onTriggered: originalEditArea.selectAll()
        }
    }
}

