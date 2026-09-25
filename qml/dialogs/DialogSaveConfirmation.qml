// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

// DialogSaveConfirmation: Prompts user to save unsaved modifications before
// exiting, creating a new subtitle project, or opening a different file.
// Matches original Aegisub wxMessageBox(MB_YESNOCANCEL) without exclamation badge.
NativeDialogFrame {
    id: dlgSaveConfirm
    isModal: true
    title: qsTr("Unsaved changes")
    implicitWidth: 380
    implicitHeight: 115
    width: 380
    height: 115
    minimumWidth: 380
    minimumHeight: 115
    maximumWidth: 380
    maximumHeight: 115

    property string fileName: qsTr("Untitled")
    property string pendingAction: ""
    property var pendingData: null

    signal saveConfirmed()
    signal discardConfirmed()
    signal cancelled()

    function getBaseName(path) {
        if (!path || path === "Untitled" || path === qsTr("Untitled")) return qsTr("Untitled");
        var base = path.split('/').pop().split('\\').pop();
        return base || qsTr("Untitled");
    }

    function prompt(targetFile, action, data) {
        fileName = getBaseName(targetFile);
        pendingAction = action || "exit";
        pendingData = data !== undefined ? data : null;
        open();
    }

    function triggerSave() {
        close();
        saveConfirmed();
    }

    function triggerDiscard() {
        close();
        discardConfirmed();
    }

    function triggerCancel() {
        close();
        cancelled();
    }

    Item {
        anchors.fill: parent
        anchors.margins: 16

        // Message text matching Aegisub wxMessageBox (single prompt line, no icon badge)
        Text {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 4
            anchors.leftMargin: 2
            anchors.rightMargin: 2
            text: qsTr("Do you want to save changes to %s?").replace("%s", dlgSaveConfirm.fileName)
            font.family: uiTheme.uiFont
            font.pixelSize: 12
            color: "#000000"
            wrapMode: Text.Wrap
            renderType: Text.NativeRendering
        }

        // Action buttons
        RowLayout {
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            spacing: 8

            NativeButton {
                id: btnSave
                text: qsTr("&Save")
                isDefault: true
                focus: true
                onClicked: dlgSaveConfirm.triggerSave()
            }

            NativeButton {
                id: btnDiscard
                text: qsTr("&Don't Save")
                onClicked: dlgSaveConfirm.triggerDiscard()
            }

            NativeButton {
                id: btnCancel
                text: qsTr("&Cancel")
                onClicked: dlgSaveConfirm.triggerCancel()
            }
        }
    }

    // Keyboard shortcuts: supports standard desktop mnemonics and hotkeys
    Shortcut {
        sequence: "Return"
        enabled: dlgSaveConfirm.visible
        onActivated: dlgSaveConfirm.triggerSave()
    }
    Shortcut {
        sequence: "Enter"
        enabled: dlgSaveConfirm.visible
        onActivated: dlgSaveConfirm.triggerSave()
    }
    Shortcut {
        sequence: "Alt+S"
        enabled: dlgSaveConfirm.visible
        onActivated: dlgSaveConfirm.triggerSave()
    }
    Shortcut {
        sequence: "Alt+Y"
        enabled: dlgSaveConfirm.visible
        onActivated: dlgSaveConfirm.triggerSave()
    }
    Shortcut {
        sequence: "Alt+D"
        enabled: dlgSaveConfirm.visible
        onActivated: dlgSaveConfirm.triggerDiscard()
    }
    Shortcut {
        sequence: "Alt+N"
        enabled: dlgSaveConfirm.visible
        onActivated: dlgSaveConfirm.triggerDiscard()
    }
    Shortcut {
        sequence: "Alt+C"
        enabled: dlgSaveConfirm.visible
        onActivated: dlgSaveConfirm.triggerCancel()
    }
    onRejected: {
        cancelled();
    }
}
