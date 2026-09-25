// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls

// Cross-platform native window frame container for Aegisub dialogs,
// supporting both modal and modeless execution with OS-managed window decorations.
Window {
    id: dialogRoot
    visible: false

    property bool isModal: false
    modality: isModal ? Qt.WindowModal : Qt.NonModal

    // Bind transient parent to the main application window for lifecycle and z-order management
    transientParent: (typeof root !== "undefined") ? root : null
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowCloseButtonHint

    property string iconSource: ""
    default property alias content: dialogContent.data

    signal accepted()
    signal rejected()
    signal aboutToShow()
    signal opened()
    signal closed()

    property int implicitWidth: 0
    property int implicitHeight: 0

    // Default dimensions, overridden by derived dialog definitions
    width: (implicitWidth > 0) ? implicitWidth : 600
    height: (implicitHeight > 0) ? implicitHeight : 400
    minimumWidth: 240
    minimumHeight: 180

    property bool userRepositioned: false
    onXChanged: { if (visible) userRepositioned = true; }
    onYChanged: { if (visible) userRepositioned = true; }

    function centerOnParent() {
        if (!userRepositioned && transientParent) {
            dialogRoot.x = Math.max(0, transientParent.x + Math.round((transientParent.width - dialogRoot.width) / 2));
            dialogRoot.y = Math.max(0, transientParent.y + Math.round((transientParent.height - dialogRoot.height) / 2));
        }
    }

    function open() {
        if (!visible) {
            aboutToShow();
            centerOnParent();
            visible = true;
            opened();
        }
        requestActivate();
        raise();
    }

    function close() {
        if (visible) {
            visible = false;
            closed();
        }
    }

    onClosing: (closeEvent) => {
        rejected();
        closed();
    }

    Shortcut {
        sequence: "Escape"
        enabled: dialogRoot.visible
        onActivated: {
            dialogRoot.rejected();
            dialogRoot.close();
        }
    }

    Rectangle {
        id: bgRect
        anchors.fill: parent
        color: (typeof pal !== "undefined" && pal.winBg) ? pal.winBg : "#f0f0f0"

        Item {
            id: dialogContent
            anchors.fill: parent
        }
    }
}
