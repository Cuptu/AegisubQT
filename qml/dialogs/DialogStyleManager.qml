// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogStyleManager: Two-pane manager for organizing, transferring, and modifying
// subtitle styles across persistent local storage catalogs and the active script.
// Fully connected to native C++ StyleStorageManager and SubtitleModel.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    title: qsTr("Styles Manager")
    iconSource: "../../assets/icons_native/style_toolbutton_16.png"
    width: implicitWidth
    height: implicitHeight
    implicitWidth: 800
    implicitHeight: 520

    property int storageSelectedIndex: 0
    property int currentSelectedIndex: 0

    // Emitted to broadcast informational status notifications
    signal statusMessage(string message)

    // Emitted when user initiates style editing:
    // styleItem: style model data object
    // isStorage: true if editing from storage catalog, false if active script
    // itemIndex: index of the style in its respective collection
    signal editStyleRequested(var styleItem, bool isStorage, int itemIndex)

    readonly property var storageStyles: (typeof styleStorageManager !== "undefined" && styleStorageManager) ? styleStorageManager.currentCatalogStyles : []
    readonly property var scriptStyles: (typeof nativeSubtitleModel !== "undefined" && nativeSubtitleModel) ? nativeSubtitleModel.styles : []
    readonly property var catalogList: (typeof styleStorageManager !== "undefined" && styleStorageManager) ? styleStorageManager.catalogNames : ["Default"]

    function copyToCurrent() {
        if (typeof styleStorageManager === "undefined" || !styleStorageManager || typeof nativeSubtitleModel === "undefined" || !nativeSubtitleModel) return;
        var it = styleStorageManager.getStyle(styleStorageManager.currentCatalog, storageSelectedIndex);
        if (it && it.name) {
            nativeSubtitleModel.addStyle(it);
            dialog.statusMessage(qsTr("Copied style [") + it.name + qsTr("] to the current script"));
        }
    }

    function copyToStorage() {
        if (typeof styleStorageManager === "undefined" || !styleStorageManager || typeof nativeSubtitleModel === "undefined" || !nativeSubtitleModel) return;
        var it = nativeSubtitleModel.getStyle(currentSelectedIndex);
        if (it && it.name) {
            styleStorageManager.addStyle(styleStorageManager.currentCatalog, it);
            dialog.statusMessage(qsTr("Copied style [") + it.name + qsTr("] to storage [") + styleStorageManager.currentCatalog + "]");
        }
    }

    function newStorageStyle() {
        if (typeof styleStorageManager === "undefined" || !styleStorageManager) return;
        var newName = "New Style " + (storageStyles.length + 1);
        styleStorageManager.addStyle(styleStorageManager.currentCatalog, {
            name: newName,
            font: "Arial",
            size: 48,
            primary: "&H00FFFFFF",
            secondary: "&H000000FF",
            outline: "&H00000000",
            shadow: "&H00000000",
            bold: false,
            italic: false,
            underline: false,
            strikeout: false,
            borderStyle: 1,
            outlineWidth: 2,
            shadowDepth: 2,
            alignment: 2,
            marginL: 10,
            marginR: 10,
            marginV: 10,
            scaleX: 100,
            scaleY: 100
        });
        storageSelectedIndex = Math.max(0, storageStyles.length - 1);
        dialog.statusMessage(qsTr("Created style: ") + newName);
    }

    function copyStorageStyle() {
        if (typeof styleStorageManager === "undefined" || !styleStorageManager) return;
        if (storageSelectedIndex >= 0 && storageSelectedIndex < storageStyles.length) {
            styleStorageManager.copyStyle(styleStorageManager.currentCatalog, storageSelectedIndex);
            storageSelectedIndex = Math.max(0, storageStyles.length - 1);
        }
    }

    function moveStorageUp() {
        if (typeof styleStorageManager === "undefined" || !styleStorageManager) return;
        if (storageSelectedIndex > 0 && storageSelectedIndex < storageStyles.length) {
            styleStorageManager.moveStyle(styleStorageManager.currentCatalog, storageSelectedIndex, storageSelectedIndex - 1);
            storageSelectedIndex--;
        }
    }

    function moveStorageDown() {
        if (typeof styleStorageManager === "undefined" || !styleStorageManager) return;
        if (storageSelectedIndex >= 0 && storageSelectedIndex < storageStyles.length - 1) {
            styleStorageManager.moveStyle(styleStorageManager.currentCatalog, storageSelectedIndex, storageSelectedIndex + 1);
            storageSelectedIndex++;
        }
    }

    function moveStorageToTop() {
        if (typeof styleStorageManager === "undefined" || !styleStorageManager) return;
        if (storageSelectedIndex > 0 && storageSelectedIndex < storageStyles.length) {
            styleStorageManager.moveStyle(styleStorageManager.currentCatalog, storageSelectedIndex, 0);
            storageSelectedIndex = 0;
        }
    }

    function moveStorageToBottom() {
        if (typeof styleStorageManager === "undefined" || !styleStorageManager) return;
        if (storageSelectedIndex >= 0 && storageSelectedIndex < storageStyles.length - 1) {
            styleStorageManager.moveStyle(styleStorageManager.currentCatalog, storageSelectedIndex, storageStyles.length - 1);
            storageSelectedIndex = storageStyles.length - 1;
        }
    }

    function newScriptStyle() {
        if (typeof nativeSubtitleModel === "undefined" || !nativeSubtitleModel) return;
        var newName = "New Style " + (scriptStyles.length + 1);
        nativeSubtitleModel.addStyle({
            name: newName,
            font: "Arial",
            size: 48,
            primary: "&H00FFFFFF",
            secondary: "&H000000FF",
            outline: "&H00000000",
            shadow: "&H00000000",
            bold: false,
            italic: false,
            underline: false,
            strikeout: false,
            borderStyle: 1,
            outlineWidth: 2,
            shadowDepth: 2,
            alignment: 2,
            marginL: 10,
            marginR: 10,
            marginV: 10,
            scaleX: 100,
            scaleY: 100
        });
        currentSelectedIndex = Math.max(0, scriptStyles.length - 1);
        dialog.statusMessage(qsTr("Created script style: ") + newName);
    }

    function copyScriptStyle() {
        if (typeof nativeSubtitleModel === "undefined" || !nativeSubtitleModel) return;
        if (currentSelectedIndex >= 0 && currentSelectedIndex < scriptStyles.length) {
            nativeSubtitleModel.copyStyle(currentSelectedIndex);
            currentSelectedIndex = Math.max(0, scriptStyles.length - 1);
        }
    }

    function moveScriptUp() {
        if (typeof nativeSubtitleModel === "undefined" || !nativeSubtitleModel) return;
        if (currentSelectedIndex > 0 && currentSelectedIndex < scriptStyles.length) {
            nativeSubtitleModel.moveStyle(currentSelectedIndex, currentSelectedIndex - 1);
            currentSelectedIndex--;
        }
    }

    function moveScriptDown() {
        if (typeof nativeSubtitleModel === "undefined" || !nativeSubtitleModel) return;
        if (currentSelectedIndex >= 0 && currentSelectedIndex < scriptStyles.length - 1) {
            nativeSubtitleModel.moveStyle(currentSelectedIndex, currentSelectedIndex + 1);
            currentSelectedIndex++;
        }
    }

    function moveScriptToTop() {
        if (typeof nativeSubtitleModel === "undefined" || !nativeSubtitleModel) return;
        if (currentSelectedIndex > 0 && currentSelectedIndex < scriptStyles.length) {
            nativeSubtitleModel.moveStyle(currentSelectedIndex, 0);
            currentSelectedIndex = 0;
        }
    }

    function moveScriptToBottom() {
        if (typeof nativeSubtitleModel === "undefined" || !nativeSubtitleModel) return;
        if (currentSelectedIndex >= 0 && currentSelectedIndex < scriptStyles.length - 1) {
            nativeSubtitleModel.moveStyle(currentSelectedIndex, scriptStyles.length - 1);
            currentSelectedIndex = scriptStyles.length - 1;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // Storage styles catalog panel
            NativeGroupBox {
                title: qsTr("Catalog of Available Styles")
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    // Catalog switcher and management buttons
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        NativeComboBox {
                            id: cmbCatalog
                            Layout.fillWidth: true
                            model: dialog.catalogList
                            currentIndex: Math.max(0, dialog.catalogList.indexOf(styleStorageManager ? styleStorageManager.currentCatalog : "Default"))
                            onActivated: (idx) => {
                                if (styleStorageManager && idx >= 0 && idx < dialog.catalogList.length) {
                                    styleStorageManager.currentCatalog = dialog.catalogList[idx];
                                }
                            }
                        }

                        NativeButton {
                            text: qsTr("New")
                            Layout.preferredWidth: 50
                            onClicked: {
                                var catName = qsTr("Catalog ") + (dialog.catalogList.length + 1);
                                if (styleStorageManager) {
                                    styleStorageManager.createCatalog(catName);
                                    dialog.statusMessage(qsTr("Created catalog: ") + catName);
                                }
                            }
                        }

                        NativeButton {
                            text: qsTr("Delete")
                            Layout.preferredWidth: 50
                            enabled: dialog.catalogList.length > 1
                            onClicked: {
                                if (styleStorageManager && dialog.catalogList.length > 1) {
                                    styleStorageManager.deleteCatalog(styleStorageManager.currentCatalog);
                                    dialog.statusMessage(qsTr("Deleted catalog"));
                                }
                            }
                        }
                    }

                    // Storage styles list view
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#ffffff"
                        border.color: "#7f9db9"
                        border.width: 1

                        ListView {
                            id: lvStorage
                            anchors.fill: parent
                            anchors.margins: 1
                            clip: true
                            model: dialog.storageStyles
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: Rectangle {
                                width: lvStorage.width
                                height: 22
                                color: dialog.storageSelectedIndex === index ? "#3399ff" : (sMouse.containsMouse ? "#e5f1fb" : "transparent")

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 6
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: (modelData.name || "Default") + " (" + (modelData.font || "Arial") + ", " + (modelData.size || 20) + "pt)"
                                    font.pixelSize: 11
                                    font.family: uiTheme.uiFont
                                    renderType: Text.NativeRendering
                                    color: dialog.storageSelectedIndex === index ? "#ffffff" : "#000000"
                                }

                                MouseArea {
                                    id: sMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: dialog.storageSelectedIndex = index
                                    onDoubleClicked: {
                                        dialog.storageSelectedIndex = index;
                                        if (styleStorageManager) {
                                            dialog.editStyleRequested(styleStorageManager.getStyle(styleStorageManager.currentCatalog, index), true, index);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Style lifecycle actions (new, edit, copy, delete)
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        NativeButton {
                            text: qsTr("&New")
                            Layout.fillWidth: true
                            onClicked: dialog.newStorageStyle()
                        }
                        NativeButton {
                            text: qsTr("&Edit")
                            Layout.fillWidth: true
                            onClicked: {
                                if (styleStorageManager && dialog.storageSelectedIndex >= 0 && dialog.storageSelectedIndex < dialog.storageStyles.length) {
                                    dialog.editStyleRequested(styleStorageManager.getStyle(styleStorageManager.currentCatalog, dialog.storageSelectedIndex), true, dialog.storageSelectedIndex);
                                }
                            }
                        }
                        NativeButton {
                            text: qsTr("&Copy")
                            Layout.fillWidth: true
                            onClicked: dialog.copyStorageStyle()
                        }
                        NativeButton {
                            text: qsTr("&Delete")
                            Layout.fillWidth: true
                            enabled: dialog.storageStyles.length > 0
                            onClicked: {
                                if (styleStorageManager && dialog.storageSelectedIndex >= 0 && dialog.storageSelectedIndex < dialog.storageStyles.length) {
                                    styleStorageManager.removeStyle(styleStorageManager.currentCatalog, dialog.storageSelectedIndex);
                                    dialog.storageSelectedIndex = Math.min(dialog.storageSelectedIndex, dialog.storageStyles.length - 1);
                                }
                            }
                        }
                    }

                    // Reordering controls
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        NativeButton {
                            text: "▲"
                            Layout.preferredWidth: 28
                            onClicked: dialog.moveStorageUp()
                        }
                        NativeButton {
                            text: "▼"
                            Layout.preferredWidth: 28
                            onClicked: dialog.moveStorageDown()
                        }
                        NativeButton {
                            text: qsTr("Move to top")
                            Layout.fillWidth: true
                            onClicked: dialog.moveStorageToTop()
                        }
                        NativeButton {
                            text: qsTr("Move to bottom")
                            Layout.fillWidth: true
                            onClicked: dialog.moveStorageToBottom()
                        }
                        NativeButton {
                            text: qsTr("&Sort styles")
                            Layout.fillWidth: true
                            onClicked: {
                                if (styleStorageManager) styleStorageManager.sortStyles(styleStorageManager.currentCatalog);
                            }
                        }
                    }
                }
            }

            // Transfer actions between storage and active script
            ColumnLayout {
                Layout.alignment: Qt.AlignVCenter
                spacing: 8

                NativeButton {
                    id: btnToScript
                    text: qsTr("Move to script") + " ->"
                    Layout.preferredWidth: Math.max(100, contentItem.implicitWidth + 16)
                    onClicked: dialog.copyToCurrent()
                }

                NativeButton {
                    id: btnToStorage
                    text: "<- " + qsTr("Move to style storage")
                    Layout.preferredWidth: Math.max(100, contentItem.implicitWidth + 16)
                    onClicked: dialog.copyToStorage()
                }
            }

            // Active script styles panel
            NativeGroupBox {
                title: qsTr("Styles available for current script")
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#ffffff"
                        border.color: "#7f9db9"
                        border.width: 1

                        ListView {
                            id: lvCurrent
                            anchors.fill: parent
                            anchors.margins: 1
                            clip: true
                            model: dialog.scriptStyles
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: Rectangle {
                                width: lvCurrent.width
                                height: 22
                                color: dialog.currentSelectedIndex === index ? "#3399ff" : (cMouse.containsMouse ? "#e5f1fb" : "transparent")

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 6
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: (modelData.name || "Default") + " (" + (modelData.font || "Arial") + ", " + (modelData.size || 20) + "pt)"
                                    font.pixelSize: 11
                                    font.family: uiTheme.uiFont
                                    renderType: Text.NativeRendering
                                    color: dialog.currentSelectedIndex === index ? "#ffffff" : "#000000"
                                }

                                MouseArea {
                                    id: cMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: dialog.currentSelectedIndex = index
                                    onDoubleClicked: {
                                        dialog.currentSelectedIndex = index;
                                        if (nativeSubtitleModel) {
                                            dialog.editStyleRequested(nativeSubtitleModel.getStyle(index), false, index);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Script style lifecycle actions
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        NativeButton {
                            text: qsTr("&New")
                            Layout.fillWidth: true
                            onClicked: dialog.newScriptStyle()
                        }
                        NativeButton {
                            text: qsTr("&Edit")
                            Layout.fillWidth: true
                            onClicked: {
                                if (nativeSubtitleModel && dialog.currentSelectedIndex >= 0 && dialog.currentSelectedIndex < dialog.scriptStyles.length) {
                                    dialog.editStyleRequested(nativeSubtitleModel.getStyle(dialog.currentSelectedIndex), false, dialog.currentSelectedIndex);
                                }
                            }
                        }
                        NativeButton {
                            text: qsTr("&Copy")
                            Layout.fillWidth: true
                            onClicked: dialog.copyScriptStyle()
                        }
                        NativeButton {
                            text: qsTr("&Delete")
                            Layout.fillWidth: true
                            enabled: dialog.scriptStyles.length > 1
                            onClicked: {
                                if (nativeSubtitleModel && dialog.currentSelectedIndex >= 0 && dialog.currentSelectedIndex < dialog.scriptStyles.length) {
                                    nativeSubtitleModel.removeStyle(dialog.currentSelectedIndex);
                                    dialog.currentSelectedIndex = Math.min(dialog.currentSelectedIndex, dialog.scriptStyles.length - 1);
                                }
                            }
                        }
                    }

                    // Script style reordering controls
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        NativeButton {
                            text: "▲"
                            Layout.preferredWidth: 28
                            onClicked: dialog.moveScriptUp()
                        }
                        NativeButton {
                            text: "▼"
                            Layout.preferredWidth: 28
                            onClicked: dialog.moveScriptDown()
                        }
                        NativeButton {
                            text: qsTr("Move to top")
                            Layout.fillWidth: true
                            onClicked: dialog.moveScriptToTop()
                        }
                        NativeButton {
                            text: qsTr("Move to bottom")
                            Layout.fillWidth: true
                            onClicked: dialog.moveScriptToBottom()
                        }
                        NativeButton {
                            text: qsTr("&Sort styles")
                            Layout.fillWidth: true
                            onClicked: {
                                if (nativeSubtitleModel) nativeSubtitleModel.sortStyles();
                            }
                        }
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
                text: qsTr("Close"); isDefault: true
                Layout.preferredWidth: 75
                onClicked: dialog.close()
            }

            NativeButton {
                text: qsTr("Help"); Layout.preferredWidth: 75
                onClicked: Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Styles/")
            }
        }
    }
}
