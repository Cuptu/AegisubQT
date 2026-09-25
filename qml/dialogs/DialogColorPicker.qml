// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogColorPicker: Color selection dialog providing 2D spectrum picking, RGB/HSV channels,
// ASS hex code formatting (&HBBGGRR&), color swatch comparison, and eyedropper activation.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Select Color")
    implicitWidth: 540
    implicitHeight: 380

    property color currentColor: "#ff5500"
    property color originalColor: "#ffffff"

    property int redVal: 255
    property int greenVal: 85
    property int blueVal: 0

    property real hueVal: 20
    property real satVal: 1.0
    property real valVal: 1.0

    property string targetProp: ""

    // Emitted when a color is confirmed
    signal colorSelected(color col)
    signal colorAccepted(color col, string assBgrCode, string assAbgrCode)

    // Emitted when user activates screen/video eyedropper
    signal dropperActivated()

    function updateFromRgb() {
        currentColor = Qt.rgba(redVal / 255.0, greenVal / 255.0, blueVal / 255.0, 1.0);
    }

    function updateFromHsv() {
        currentColor = Qt.hsva(hueVal / 360.0, satVal, valVal, 1.0);
        redVal = Math.round(currentColor.r * 255);
        greenVal = Math.round(currentColor.g * 255);
        blueVal = Math.round(currentColor.b * 255);
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // 2D spectrum canvas and hue/value selection area
            Rectangle {
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                color: "#ffffff"
                border.color: "#7f9db9"
                border.width: 1

                Canvas {
                    id: spectrumCanvas
                    anchors.fill: parent
                    anchors.margins: 2
                    onPaint: {
                        var ctx = getContext("2d");
                        var w = width;
                        var h = height;
                        var gradH = ctx.createLinearGradient(0, 0, w, 0);
                        gradH.addColorStop(0, "#ff0000");
                        gradH.addColorStop(0.17, "#ffff00");
                        gradH.addColorStop(0.33, "#00ff00");
                        gradH.addColorStop(0.5, "#00ffff");
                        gradH.addColorStop(0.67, "#0000ff");
                        gradH.addColorStop(0.83, "#ff00ff");
                        gradH.addColorStop(1, "#ff0000");
                        ctx.fillStyle = gradH;
                        ctx.fillRect(0, 0, w, h);

                        var gradV = ctx.createLinearGradient(0, 0, 0, h);
                        gradV.addColorStop(0, "rgba(255,255,255,0)");
                        gradV.addColorStop(1, "rgba(0,0,0,1)");
                        ctx.fillStyle = gradV;
                        ctx.fillRect(0, 0, w, h);
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: (mouse) => {
                        dialog.hueVal = (mouse.x / width) * 360;
                        dialog.valVal = 1.0 - (mouse.y / height);
                        dialog.satVal = 1.0;
                        dialog.updateFromHsv();
                    }
                }
            }

            // Color comparison preview and eyedropper activator
            ColumnLayout {
                Layout.preferredWidth: 90
                spacing: 6

                NativeGroupBox {
                    title: qsTr("Color preview")
                    Layout.fillWidth: true
                    implicitHeight: 90

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 2

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: dialog.currentColor
                            border.color: "#808080"
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: qsTr("Current")
                                font.pixelSize: 10
                                color: (dialog.redVal + dialog.greenVal + dialog.blueVal > 380) ? "#000000" : "#ffffff"
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: dialog.originalColor
                            border.color: "#808080"
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: qsTr("Original")
                                font.pixelSize: 10
                                color: "#000000"
                            }
                        }
                    }
                }

                NativeButton {
                    text: qsTr("Dropper"); Layout.fillWidth: true
                    onClicked: dialog.dropperActivated()
                }

                Item { Layout.fillHeight: true }
            }

            // RGB channels and ASS hexadecimal format code
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                NativeGroupBox {
                    title: "RGB"
                    Layout.fillWidth: true
                    implicitHeight: 95

                    GridLayout {
                        anchors.fill: parent
                        columns: 2
                        columnSpacing: 6
                        rowSpacing: 2

                        Text { text: qsTr("Red (R):"); font.pixelSize: 11 }
                        NativeSpinBox {
                            Layout.fillWidth: true
                            from: 0; to: 255; value: dialog.redVal
                            onValueModified: (v) => { dialog.redVal = v; dialog.updateFromRgb(); }
                        }

                        Text { text: qsTr("Green (G):"); font.pixelSize: 11 }
                        NativeSpinBox {
                            Layout.fillWidth: true
                            from: 0; to: 255; value: dialog.greenVal
                            onValueModified: (v) => { dialog.greenVal = v; dialog.updateFromRgb(); }
                        }

                        Text { text: qsTr("Blue (B):"); font.pixelSize: 11 }
                        NativeSpinBox {
                            Layout.fillWidth: true
                            from: 0; to: 255; value: dialog.blueVal
                            onValueModified: (v) => { dialog.blueVal = v; dialog.updateFromRgb(); }
                        }
                    }
                }

                NativeGroupBox {
                    title: qsTr("ASS")
                    Layout.fillWidth: true
                    implicitHeight: 52

                    RowLayout {
                        anchors.fill: parent
                        NativeTextBox {
                            text: "&H" + ("00" + dialog.blueVal.toString(16).toUpperCase()).slice(-2) + ("00" + dialog.greenVal.toString(16).toUpperCase()).slice(-2) + ("00" + dialog.redVal.toString(16).toUpperCase()).slice(-2) + "&"
                            readOnly: true
                            Layout.fillWidth: true
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
                text: qsTr("OK"); isDefault: true
                Layout.preferredWidth: 75
                onClicked: {
                    var hexBgr = "&H" + ("00" + dialog.blueVal.toString(16).toUpperCase()).slice(-2) + ("00" + dialog.greenVal.toString(16).toUpperCase()).slice(-2) + ("00" + dialog.redVal.toString(16).toUpperCase()).slice(-2) + "&";
                    var hexAbgr = "&H00" + ("00" + dialog.blueVal.toString(16).toUpperCase()).slice(-2) + ("00" + dialog.greenVal.toString(16).toUpperCase()).slice(-2) + ("00" + dialog.redVal.toString(16).toUpperCase()).slice(-2);
                    dialog.colorSelected(dialog.currentColor);
                    dialog.colorAccepted(dialog.currentColor, hexBgr, hexAbgr);
                    dialog.close();
                }
            }

            NativeButton {
                text: qsTr("Cancel"); Layout.preferredWidth: 75
                onClicked: dialog.close()
            }
        }
    }
}
