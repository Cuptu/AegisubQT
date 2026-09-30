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
    implicitHeight: 400

    property color currentColor: "#ff5500"
    property color originalColor: "#ffffff"

    property int redVal: 255
    property int greenVal: 85
    property int blueVal: 0
    // ASS transparency: 0 is opaque, 255 is transparent.
    property int alphaVal: 0

    property real hueVal: 20
    property real satVal: 1.0
    property real valVal: 1.0

    property string targetProp: ""
    property bool pickingScreen: false
    signal dropperError(string message)

    function startScreenPick() {
        if (typeof aegisubCore === "undefined" || !aegisubCore) {
            dropperError(qsTr("Screen colour picking is unavailable."));
            return;
        }
        pickingScreen = true;
        visible = false;
        pickDelay.start();
        dropperActivated();
    }
    function finishScreenPick(col) {
        if (!pickingScreen) return;
        pickDelay.stop();
        pickingScreen = false;
        if (col)
            currentColor = Qt.rgba(col.r, col.g, col.b, currentColor.a);
        visible = true;
        requestActivate();
        raise();
    }
    Timer {
        id: pickDelay
        interval: 80
        onTriggered: aegisubCore.beginScreenColorPick()
    }
    Connections {
        target: typeof aegisubCore !== "undefined" ? aegisubCore : null
        function onScreenColorPicked(col) { dialog.finishScreenPick(col); }
        function onScreenColorPickCancelled() { dialog.finishScreenPick(null); }
        function onScreenColorPickFailed(message) {
            if (!dialog.pickingScreen) return;
            dialog.finishScreenPick(null);
            dialog.dropperError(message);
        }
    }
    Component.onDestruction: {
        if (pickingScreen && typeof aegisubCore !== "undefined" && aegisubCore)
            aegisubCore.cancelScreenColorPick();
    }
    readonly property string assBgrCode: "&H" + byteHex(Math.round(currentColor.b * 255))
        + byteHex(Math.round(currentColor.g * 255)) + byteHex(Math.round(currentColor.r * 255)) + "&"
    readonly property string assAbgrCode: "&H" + byteHex(Math.round((1 - currentColor.a) * 255))
        + byteHex(Math.round(currentColor.b * 255)) + byteHex(Math.round(currentColor.g * 255))
        + byteHex(Math.round(currentColor.r * 255))

    // Emitted when a color is confirmed
    signal colorSelected(color col)
    signal colorAccepted(color col, string assBgrCode, string assAbgrCode)

    // Emitted when user activates screen/video eyedropper
    signal dropperActivated()

    function byteHex(value) { return ("00" + value.toString(16).toUpperCase()).slice(-2); }

    function syncChannels() {
        redVal = Math.round(currentColor.r * 255);
        greenVal = Math.round(currentColor.g * 255);
        blueVal = Math.round(currentColor.b * 255);
        alphaVal = Math.round((1 - currentColor.a) * 255);
        var maximum = Math.max(currentColor.r, currentColor.g, currentColor.b);
        var minimum = Math.min(currentColor.r, currentColor.g, currentColor.b);
        var delta = maximum - minimum;
        valVal = maximum;
        satVal = maximum > 0 ? delta / maximum : 0;
        if (delta > 0) {
            var hue;
            if (maximum === currentColor.r) hue = (currentColor.g - currentColor.b) / delta;
            else if (maximum === currentColor.g) hue = 2 + (currentColor.b - currentColor.r) / delta;
            else hue = 4 + (currentColor.r - currentColor.g) / delta;
            hueVal = ((hue * 60) + 360) % 360;
        }
    }

    onCurrentColorChanged: syncChannels()
    onAboutToShow: {
        originalColor = currentColor;
        syncChannels();
    }

    function acceptColor() {
        colorSelected(currentColor);
        colorAccepted(currentColor, assBgrCode, assAbgrCode);
        close();
    }

    function updateFromRgb() {
        currentColor = Qt.rgba(redVal / 255.0, greenVal / 255.0, blueVal / 255.0, 1.0 - alphaVal / 255.0);
    }

    function updateFromHsv() {
        currentColor = Qt.hsva(hueVal / 360.0, satVal, valVal, 1.0 - alphaVal / 255.0);
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
                    objectName: "color-picker-dropper"
                    text: qsTr("Dropper"); Layout.fillWidth: true
                    onClicked: dialog.startScreenPick()
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
                    implicitHeight: 125

                    GridLayout {
                        anchors.fill: parent
                        columns: 2
                        columnSpacing: 6
                        rowSpacing: 2

                        Text { text: qsTr("Red (R):"); font.pixelSize: 11 }
                        NativeSpinBox {
                            objectName: "color-picker-red"
                            Layout.fillWidth: true
                            from: 0; to: 255; value: dialog.redVal
                            onValueModified: (v) => { dialog.redVal = v; dialog.updateFromRgb(); }
                        }

                        Text { text: qsTr("Green (G):"); font.pixelSize: 11 }
                        NativeSpinBox {
                            objectName: "color-picker-green"
                            Layout.fillWidth: true
                            from: 0; to: 255; value: dialog.greenVal
                            onValueModified: (v) => { dialog.greenVal = v; dialog.updateFromRgb(); }
                        }

                        Text { text: qsTr("Blue (B):"); font.pixelSize: 11 }
                        NativeSpinBox {
                            objectName: "color-picker-blue"
                            Layout.fillWidth: true
                            from: 0; to: 255; value: dialog.blueVal
                            onValueModified: (v) => { dialog.blueVal = v; dialog.updateFromRgb(); }
                        }

                        Text { text: qsTr("Transparency:"); font.pixelSize: 11 }
                        NativeSpinBox {
                            objectName: "color-picker-alpha"
                            Layout.fillWidth: true
                            from: 0; to: 255; value: dialog.alphaVal
                            onValueModified: (v) => { dialog.alphaVal = v; dialog.updateFromRgb(); }
                            ToolTip.text: qsTr("ASS transparency: 0 = opaque, 255 = transparent")
                            ToolTip.visible: hovered
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
                            objectName: "color-picker-ass"
                            text: dialog.assAbgrCode
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
                objectName: "color-picker-ok"
                text: qsTr("OK"); isDefault: true
                Layout.preferredWidth: 75
                onClicked: dialog.acceptColor()
            }

            NativeButton {
                text: qsTr("Cancel"); Layout.preferredWidth: 75
                onClicked: dialog.close()
            }
        }
    }
}
