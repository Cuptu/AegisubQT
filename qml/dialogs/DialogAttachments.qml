// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogAttachments: Attachment manager for embedded ASS script fonts and graphic resources.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../controls"

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Attachment List")
    iconSource: "../../assets/icons_native/attach_button_16.png"
    implicitWidth: 530
    implicitHeight: 320

    property var project: null
    property int selectedIndex: -1

    // Emitted to broadcast informational status notifications
    signal statusMessage(string message)

    FileDialog {
        id: openFontDialog
        title: qsTr("Attach Font")
        nameFilters: [qsTr("Font files (*.ttf *.otf *.ttc *.woff)"), qsTr("All files (*.*)")]
        onAccepted: {
            var path = selectedFile.toString();
            var filename = path.substring(Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\")) + 1);
            if (dialog.project && dialog.project.addAttachment) {
                dialog.project.addAttachment(filename, "Font", "[Fonts]");
            } else {
                dummyModel.append({ filename: filename, sizeStr: "Font", typeStr: "[Fonts]" });
            }
            dialog.statusMessage(qsTr("Attached font: %1").arg(filename));
        }
    }

    FileDialog {
        id: openGraphicDialog
        title: qsTr("Attach Graphics")
        nameFilters: [qsTr("Graphic files (*.bmp *.png *.jpg *.jpeg *.ico)"), qsTr("All files (*.*)")]
        onAccepted: {
            var path = selectedFile.toString();
            var filename = path.substring(Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\")) + 1);
            if (dialog.project && dialog.project.addAttachment) {
                dialog.project.addAttachment(filename, "Image", "[Graphics]");
            } else {
                dummyModel.append({ filename: filename, sizeStr: "Image", typeStr: "[Graphics]" });
            }
            dialog.statusMessage(qsTr("Attached graphics: %1").arg(filename));
        }
    }

    FolderDialog {
        id: extractFolderDialog
        title: qsTr("Select Folder to Extract Attachment")
        onAccepted: {
            if (dialog.selectedIndex >= 0 && dialog.selectedIndex < attachList.count) {
                var item = attachList.model.get(dialog.selectedIndex);
                var fname = item ? item.filename : "";
                dialog.statusMessage(qsTr("Extracted attachment %1 to %2").arg(fname).arg(selectedFolder.toString()));
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 6

        // Attachment table view
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#7a7a7a"
            border.width: 1

            Column {
                anchors.fill: parent

                // Column headers
                Rectangle {
                    width: parent.width
                    height: 22
                    color: "#f0f0f0"

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: "#d0d0d0"
                    }

                    Row {
                        anchors.fill: parent

                        // Filename column
                        Item {
                            width: 270
                            height: parent.height
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("Name"); font.pixelSize: 12
                                font.family: uiTheme.uiFont
                                renderType: Text.NativeRendering
                                color: "#000000"
                            }
                            Rectangle {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.margins: 2
                                width: 1
                                color: "#d0d0d0"
                            }
                        }

                        // File size column
                        Item {
                            width: 110
                            height: parent.height
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("Size"); font.pixelSize: 12
                                font.family: uiTheme.uiFont
                                renderType: Text.NativeRendering
                                color: "#000000"
                            }
                            Rectangle {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.margins: 2
                                width: 1
                                color: "#d0d0d0"
                            }
                        }

                        // Attachment category column
                        Item {
                            width: Math.max(100, parent.width - 380)
                            height: parent.height
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("Group"); font.pixelSize: 12
                                font.family: uiTheme.uiFont
                                renderType: Text.NativeRendering
                                color: "#000000"
                            }
                        }
                    }
                }

                // Attachment list view
                ListView {
                    id: attachList
                    width: parent.width
                    height: parent.height - 22
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: (dialog.project && dialog.project.attachmentModel) ? dialog.project.attachmentModel : dummyModel

                    ListModel {
                        id: dummyModel
                        ListElement { filename: "SourceHanSans-Medium.otf"; sizeStr: "8.4 MB"; typeStr: "[Fonts]" }
                        ListElement { filename: "watermark_logo.png"; sizeStr: "42.1 KB"; typeStr: "[Graphics]" }
                    }

                    delegate: Rectangle {
                        id: rowItem
                        width: attachList.width
                        height: 20
                        color: dialog.selectedIndex === index ? "#3399ff" : (index % 2 === 1 ? "#fafafa" : "#ffffff")

                        Row {
                            anchors.fill: parent

                            Item {
                                width: 270
                                height: parent.height
                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 6
                                    anchors.right: parent.right
                                    anchors.rightMargin: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: model.filename
                                    font.pixelSize: 12
                                    font.family: uiTheme.uiFont
                                    renderType: Text.NativeRendering
                                    color: dialog.selectedIndex === index ? "#ffffff" : "#000000"
                                    elide: Text.ElideRight
                                }
                            }

                            Item {
                                width: 110
                                height: parent.height
                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 6
                                    anchors.right: parent.right
                                    anchors.rightMargin: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: model.sizeStr
                                    font.pixelSize: 12
                                    font.family: uiTheme.uiFont
                                    renderType: Text.NativeRendering
                                    color: dialog.selectedIndex === index ? "#ffffff" : "#000000"
                                }
                            }

                            Item {
                                width: Math.max(100, parent.width - 380)
                                height: parent.height
                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 6
                                    anchors.right: parent.right
                                    anchors.rightMargin: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: model.typeStr
                                    font.pixelSize: 12
                                    font.family: uiTheme.uiFont
                                    renderType: Text.NativeRendering
                                    color: dialog.selectedIndex === index ? "#ffffff" : "#000000"
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: dialog.selectedIndex = index
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        z: -1
                        onClicked: dialog.selectedIndex = -1
                    }
                }
            }
        }

        // Attachment management action buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            NativeButton {
                text: qsTr("Attach &Font"); Layout.fillWidth: true
                Layout.preferredWidth: 80
                onClicked: openFontDialog.open()
            }

            NativeButton {
                text: qsTr("Attach &Graphics"); Layout.fillWidth: true
                Layout.preferredWidth: 80
                onClicked: openGraphicDialog.open()
            }

            NativeButton {
                text: qsTr("E&xtract"); Layout.fillWidth: true
                Layout.preferredWidth: 65
                enabled: dialog.selectedIndex !== -1 && attachList.count > 0
                onClicked: extractFolderDialog.open()
            }

            NativeButton {
                text: qsTr("&Delete"); Layout.fillWidth: true
                Layout.preferredWidth: 65
                enabled: dialog.selectedIndex !== -1 && attachList.count > 0
                onClicked: {
                    if (dialog.selectedIndex >= 0 && dialog.selectedIndex < attachList.count) {
                        if (dialog.project && dialog.project.removeAttachment) dialog.project.removeAttachment(dialog.selectedIndex);
                        else dummyModel.remove(dialog.selectedIndex);
                        dialog.selectedIndex = -1;
                        dialog.statusMessage(qsTr("Attachment removed"));
                    }
                }
            }

            NativeButton {
                text: qsTr("Help"); Layout.fillWidth: true
                Layout.preferredWidth: 60
                onClicked: {
                    Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Attaching_into_subtitles/")
                }
            }

            NativeButton {
                text: qsTr("Close"); isDefault: true
                Layout.fillWidth: true
                Layout.preferredWidth: 65
                onClicked: dialog.close()
            }
        }
    }
}
