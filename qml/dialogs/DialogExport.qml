// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogExport: Subtitle export dialog allowing pipeline filter configuration
// (framerate transforms, style fixes, metadata cleanup) and output text encoding selection.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Export")
    iconSource: "../../assets/icons_native/export_menu_16.png"
    implicitWidth: 420
    implicitHeight: 460

    // Emitted when user proceeds with subtitle export:
    // filters: string array of active filter names
    // charset: target text encoding name
    signal exportRequested(var filters, string charset)

    // Emitted to broadcast informational status notifications
    signal statusMessage(string msg)

    property var filtersModel: [
        { name: "Transform Framerate", checked: false, desc: qsTr("Transform subtitle times to a different framerate.") },
        { name: "Fix Styles", checked: true, desc: qsTr("Fixes missing or invalid styles.") },
        { name: "Clean Script Info", checked: true, desc: qsTr("Removes unused script info.") },
        { name: "Karaoke Template", checked: false, desc: qsTr("Generates karaoke templates.") }
    ]

    property int selectedFilterIndex: 0

    function setAll(chk) {
        var items = dialog.filtersModel;
        for (var i = 0; i < items.length; ++i) items[i].checked = chk;
        dialog.filtersModel = [].concat(items);
    }

    function doExport() {
        var chosen = [];
        for (var i = 0; i < filtersModel.length; ++i) {
            if (filtersModel[i].checked) chosen.push(filtersModel[i].name);
        }
        dialog.exportRequested(chosen, cmbCharset.currentText);
        dialog.close();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Export filter pipeline
        NativeGroupBox {
            title: qsTr("Filters")
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
                        id: lvFilters
                        anchors.fill: parent
                        anchors.margins: 2
                        clip: true
                        model: dialog.filtersModel

                        delegate: Rectangle {
                            width: lvFilters.width
                            height: 22
                            color: dialog.selectedFilterIndex === index ? "#e5f1fb" : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 4
                                anchors.rightMargin: 4
                                spacing: 4

                                NativeCheckBox {
                                    text: modelData.name
                                    checked: modelData.checked
                                    onCheckedChanged: {
                                        var items = dialog.filtersModel;
                                        items[index].checked = checked;
                                        dialog.filtersModel = items;
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                propagateComposedEvents: true
                                onClicked: (mouse) => {
                                    dialog.selectedFilterIndex = index;
                                    mouse.accepted = false;
                                }
                            }
                        }
                    }
                }

                // Filter selection and reordering actions
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    NativeButton {
                        text: qsTr("Move &Up")
                        Layout.fillWidth: true
                        onClicked: {
                            if (dialog.selectedFilterIndex > 0) {
                                var arr = dialog.filtersModel.slice();
                                var tmp = arr[dialog.selectedFilterIndex];
                                arr[dialog.selectedFilterIndex] = arr[dialog.selectedFilterIndex - 1];
                                arr[dialog.selectedFilterIndex - 1] = tmp;
                                dialog.selectedFilterIndex--;
                                dialog.filtersModel = arr;
                            }
                        }
                    }
                    NativeButton {
                        text: qsTr("Move &Down")
                        Layout.fillWidth: true
                        onClicked: {
                            if (dialog.selectedFilterIndex < dialog.filtersModel.length - 1) {
                                var arr = dialog.filtersModel.slice();
                                var tmp = arr[dialog.selectedFilterIndex];
                                arr[dialog.selectedFilterIndex] = arr[dialog.selectedFilterIndex + 1];
                                arr[dialog.selectedFilterIndex + 1] = tmp;
                                dialog.selectedFilterIndex++;
                                dialog.filtersModel = arr;
                            }
                        }
                    }
                    NativeButton { text: qsTr("Select &All"); Layout.fillWidth: true; onClicked: dialog.setAll(true) }
                    NativeButton { text: qsTr("Select &None"); Layout.fillWidth: true; onClicked: dialog.setAll(false) }
                }

                // Active filter description preview
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 48
                    color: "#f8f9fa"
                    border.color: "#d0d0d0"
                    border.width: 1

                    Text {
                        anchors.fill: parent
                        anchors.margins: 4
                        text: dialog.filtersModel[dialog.selectedFilterIndex] ? dialog.filtersModel[dialog.selectedFilterIndex].desc : ""
                        font.pixelSize: 11
                        font.family: uiTheme.uiFont
                        color: "#444444"
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        // Text encoding / charset selection
        NativeGroupBox {
            title: qsTr("Text encoding:")
            Layout.fillWidth: true
            implicitHeight: 52

            RowLayout {
                anchors.fill: parent
                spacing: 6

                NativeComboBox {
                    id: cmbCharset
                    Layout.fillWidth: true
                    model: ["UTF-8", "UTF-16LE", "UTF-16BE", "GB2312", "GBK", "Big5", "Shift_JIS", "EUC-KR", "ISO-8859-1"]
                    currentIndex: 0
                }
            }
        }

        // Dialog action buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Item { Layout.fillWidth: true }

            NativeButton {
                text: qsTr("&Export...")
                isDefault: true
                Layout.preferredWidth: 80
                onClicked: dialog.doExport()
            }

            NativeButton {
                text: qsTr("Cancel"); Layout.preferredWidth: 75
                onClicked: dialog.close()
            }

            NativeButton {
                text: qsTr("Help"); Layout.preferredWidth: 75
                onClicked: Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Exporting/")
            }
        }
    }
}
