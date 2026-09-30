// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogTranslation: Streamlined translation assistant presenting original text,
// translation input with hotkey-driven line navigation, and audio/video playback integration.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"
import "../project/TranslationUtils.js" as TranslationUtils

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Translation Assistant")
    iconSource: "../../assets/icons_native/translation_toolbutton_16.png"
    implicitWidth: 540
    implicitHeight: 460

    property int currentLineNumber: 1
    property int totalLines: 10
    property string currentLineTime: ""
    property string originalText: ""
    property bool hasCommittedSource: false
    property string committedSource: ""
    readonly property string sourceText: hasCommittedSource ? committedSource : originalText
    property int blockIndex: -1
    readonly property var sourceBlocks: TranslationUtils.blocks(sourceText)
    property alias enablePreview: previewCheck.checked
    property bool closeAfterLast: false
    signal previewRequested()

    function previewLine() {
        if (visible && enablePreview) previewRequested();
    }
    function navigateBlock(direction) {
        var next = TranslationUtils.nextPlain(TranslationUtils.blocks(sourceText), blockIndex, direction);
        if (next >= 0) {
            blockIndex = next;
            txtTrans.text = "";
            txtTrans.forceActiveFocus();
            previewLine();
        } else if (direction < 0) prevRequested();
        else nextRequested();
    }
    function previewChanges() {
        if (blockIndex >= 0) commitRequested(txtTrans.text, blockIndex, false);
        txtTrans.text = "";
        txtTrans.forceActiveFocus();
        previewLine();
    }
    function handleKey(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (event.modifiers === Qt.AltModifier) insertOriginal();
            else if (event.modifiers === Qt.NoModifier) acceptCurrent();
            else return;
        } else if (event.modifiers !== Qt.NoModifier) return;
        else if (event.key === Qt.Key_PageUp) navigateBlock(-1);
        else if (event.key === Qt.Key_PageDown) navigateBlock(1);
        else if (event.key === Qt.Key_F1) playAudioRequested();
        else if (event.key === Qt.Key_F2) playVideoRequested();
        else if (event.key === Qt.Key_F8) previewChanges();
        else return;
        event.accepted = true;
    }
    Shortcut { sequence: "PgUp"; context: Qt.WindowShortcut; enabled: dialog.visible; onActivated: dialog.navigateBlock(-1) }
    Shortcut { sequence: "PgDown"; context: Qt.WindowShortcut; enabled: dialog.visible; onActivated: dialog.navigateBlock(1) }
    Shortcut { sequence: "F1"; context: Qt.WindowShortcut; enabled: dialog.visible; onActivated: dialog.playAudioRequested() }
    Shortcut { sequence: "F2"; context: Qt.WindowShortcut; enabled: dialog.visible; onActivated: dialog.playVideoRequested() }
    Shortcut { sequence: "F8"; context: Qt.WindowShortcut; enabled: dialog.visible; onActivated: dialog.previewChanges() }
    Shortcut { sequence: "Alt+Return"; context: Qt.WindowShortcut; enabled: dialog.visible; onActivated: dialog.insertOriginal() }
    Shortcut { sequence: "Alt+Enter"; context: Qt.WindowShortcut; enabled: dialog.visible; onActivated: dialog.insertOriginal() }

    // Emitted when translation text is submitted for current line
    signal commitRequested(string text, int blockIndex, bool autoNext)

    // Playback and navigation requests
    signal auditionRequested()
    signal prevRequested()
    signal nextRequested()
    signal playAudioRequested()
    signal playVideoRequested()

    function acceptCurrent() {
        if (dialog.blockIndex >= 0)
            dialog.commitRequested(txtTrans.text, dialog.blockIndex, true);
    }

    function insertOriginal() {
        if (dialog.blockIndex >= 0)
            txtTrans.insert(txtTrans.cursorPosition, dialog.sourceBlocks[dialog.blockIndex].text);
        txtTrans.forceActiveFocus();
    }

    function resetForLine() {
        hasCommittedSource = false;
        committedSource = "";
        blockIndex = TranslationUtils.firstPlain(TranslationUtils.blocks(originalText));
        txtTrans.text = "";
        txtTrans.forceActiveFocus();
        previewLine();
    }

    function setCommittedSource(text) {
        committedSource = text;
        hasCommittedSource = true;
    }

    function advanceAfterCommit() {
        txtTrans.text = "";
        var next = TranslationUtils.nextPlain(TranslationUtils.blocks(sourceText), blockIndex, 1);
        if (next >= 0) {
            blockIndex = next;
            txtTrans.forceActiveFocus();
            previewLine();
        } else {
            closeAfterLast = true;
            nextRequested();
            closeAfterLast = false;
        }
    }

    onAboutToShow: resetForLine()
    onOpened: previewLine()
    onCurrentLineNumberChanged: resetForLine()
    onOriginalTextChanged: {
        if (!hasCommittedSource) resetForLine();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Original source dialogue line
        NativeGroupBox {
            title: qsTr("Original") + " (" + dialog.currentLineNumber + "/" + dialog.totalLines + ")"
            Layout.fillWidth: true
            implicitHeight: 90

            Rectangle {
                anchors.fill: parent
                color: "#ffffff"
                border.color: "#7f9db9"
                border.width: 1

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 4

                    TextArea {
                        text: dialog.sourceText
                        readOnly: true
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        wrapMode: TextArea.Wrap
                        background: null
                    }
                }
            }
        }

        // Translated text input editor
        NativeGroupBox {
            title: qsTr("Translation")
            Layout.fillWidth: true
            implicitHeight: 90

            Rectangle {
                anchors.fill: parent
                color: "#ffffff"
                border.color: "#7f9db9"
                border.width: 1

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 4

                    TextArea {
                        id: txtTrans
                        objectName: "translation-input"
                        text: ""
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        wrapMode: TextArea.Wrap
                        background: null
                        focus: true
                        Keys.onPressed: (event) => dialog.handleKey(event)
                    }
                }
            }
        }

        // Lower section: hotkey guide and playback controls
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // Keyboard shortcuts and navigation hints
            NativeGroupBox {
                title: qsTr("Keys")
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 2

                    GridLayout {
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 2
                        Layout.fillWidth: true

                        Text { text: qsTr("Accept changes:"); font.pixelSize: 11; color: "#555555" }
                        Text { text: "Enter"; font.pixelSize: 11; font.bold: true }

                        Text { text: qsTr("Line break:"); font.pixelSize: 11; color: "#555555" }
                        Text { text: "Shift+Enter"; font.pixelSize: 11; font.bold: true }

                        Text { text: qsTr("Previous line:"); font.pixelSize: 11; color: "#555555" }
                        Text { text: "Page Up"; font.pixelSize: 11; font.bold: true }

                        Text { text: qsTr("Next line:"); font.pixelSize: 11; color: "#555555" }
                        Text { text: "Page Down"; font.pixelSize: 11; font.bold: true }

                        Text { text: qsTr("Insert original text:"); font.pixelSize: 11; color: "#555555" }
                        Text { text: "Alt+Enter"; font.pixelSize: 11; font.bold: true }
                        Text { text: qsTr("Preview changes:"); font.pixelSize: 11; color: "#555555" }
                        Text { text: "F8"; font.pixelSize: 11; font.bold: true }
                        Text { text: qsTr("Play audio / video:"); font.pixelSize: 11; color: "#555555" }
                        Text { text: "F1 / F2"; font.pixelSize: 11; font.bold: true }
                    }

                    Item { Layout.fillHeight: true }

                    NativeCheckBox {
                        id: previewCheck
                        objectName: "translation-preview"
                        text: qsTr("Enable &preview")
                        checked: true
                        onToggled: dialog.previewLine()
                    }
                }
            }

            // Audio/video playback and copy commands
            NativeGroupBox {
                title: qsTr("Actions")
                Layout.preferredWidth: 140
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    NativeButton {
                        text: qsTr("&Play Audio")
                        Layout.fillWidth: true
                        onClicked: dialog.playAudioRequested()
                    }
                    NativeButton {
                        text: qsTr("Play &Video")
                        Layout.fillWidth: true
                        onClicked: dialog.playVideoRequested()
                    }
                    NativeButton {
                        text: qsTr("&Insert original")
                        Layout.fillWidth: true
                        onClicked: dialog.insertOriginal()
                    }
                }
            }
        }

        // Dialog action buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Item { Layout.fillWidth: true }

            NativeButton {
                text: qsTr("OK"); isDefault: true
                Layout.preferredWidth: 75
                onClicked: dialog.acceptCurrent()
            }

            NativeButton {
                text: qsTr("Cancel"); Layout.preferredWidth: 75
                onClicked: dialog.close()
            }

            NativeButton {
                text: qsTr("Help"); Layout.preferredWidth: 75
                onClicked: {
                    Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Translation_Assistant/")
                }
            }
        }
    }
}
