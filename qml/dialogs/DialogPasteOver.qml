// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogPasteOver: Field selector dialog for selective overwrite pasting across dialogue rows.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Paste Over")
    implicitWidth: 320
    implicitHeight: 420

    // Emitted when field selection is confirmed for paste over operation
    signal pasteOverRequested(var fields)

    property var fieldList: [
        { name: qsTr("Comment"), key: "comment", checked: false },
        { name: qsTr("Layer"), key: "layer", checked: false },
        { name: qsTr("Start Time"), key: "start", checked: false },
        { name: qsTr("End Time"), key: "end", checked: false },
        { name: qsTr("Style"), key: "style", checked: false },
        { name: qsTr("Actor"), key: "actor", checked: false },
        { name: qsTr("Margin Left"), key: "margin_l", checked: false },
        { name: qsTr("Margin Right"), key: "margin_r", checked: false },
        { name: qsTr("Margin Vertical"), key: "margin_v", checked: false },
        { name: qsTr("Effect"), key: "effect", checked: false },
        { name: qsTr("Text"), key: "text", checked: true }
    ]

    function checkAll(chk) {
        var items = dialog.fieldList;
        for (var i = 0; i < items.length; ++i) items[i].checked = chk;
        dialog.fieldList = [].concat(items);
    }

    function checkTimes() {
        checkAll(false);
        var items = dialog.fieldList;
        items[2].checked = true; // Start time
        items[3].checked = true; // End time
        dialog.fieldList = [].concat(items);
    }

    function checkText() {
        checkAll(false);
        var items = dialog.fieldList;
        items[10].checked = true; // Subtitle text
        dialog.fieldList = [].concat(items);
    }

    function onOkClicked() {
        var sel = {};
        for (var i = 0; i < fieldList.length; ++i) {
            sel[fieldList[i].key] = fieldList[i].checked;
        }
        dialog.pasteOverRequested(sel);
        dialog.close();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        NativeGroupBox {
            title: qsTr("Fields")
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 4

                Text {
                    text: qsTr("Please select the fields that you want to paste over:"); font.pixelSize: 11
                    font.family: uiTheme.uiFont
                    color: "#333333"
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#ffffff"
                    border.color: "#7f9db9"
                    border.width: 1

                    ListView {
                        id: lvFields
                        anchors.fill: parent
                        anchors.margins: 2
                        clip: true
                        model: dialog.fieldList

                        delegate: Item {
                            width: lvFields.width
                            height: 22

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 6
                                spacing: 4

                                NativeCheckBox {
                                    text: modelData.name
                                    checked: modelData.checked
                                    onCheckedChanged: {
                                        var items = dialog.fieldList;
                                        items[index].checked = checked;
                                        dialog.fieldList = items;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Quick preset selection buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            NativeButton {
                text: qsTr("All"); Layout.fillWidth: true
                onClicked: dialog.checkAll(true)
            }
            NativeButton {
                text: qsTr("None"); Layout.fillWidth: true
                onClicked: dialog.checkAll(false)
            }
            NativeButton {
                text: qsTr("&Times"); Layout.fillWidth: true
                onClicked: dialog.checkTimes()
            }
            NativeButton {
                text: qsTr("&Text"); Layout.fillWidth: true
                onClicked: dialog.checkText()
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
                onClicked: dialog.onOkClicked()
            }

            NativeButton {
                text: qsTr("Cancel"); Layout.preferredWidth: 75
                onClicked: dialog.close()
            }

            NativeButton {
                text: qsTr("Help"); Layout.preferredWidth: 75
                onClicked: Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Editing_Subtitles/#paste-over")
            }
        }
    }
}
