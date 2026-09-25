// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogStyleEditor: Subtitle style definition dialog configuring font typography,
// colors, margins, numpad alignment, outline/shadow, transforms, and live preview.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Style Editor")
    iconSource: "../../assets/icons_native/style_toolbutton_16.png"
    width: implicitWidth
    height: implicitHeight
    implicitWidth: 620
    implicitHeight: 520

    // Style attributes exposed for binding and editing
    property string styleName: "Default"
    property string fontFace: "Microsoft YaHei UI"
    property int fontSize: 48
    property bool isBold: false
    property bool isItalic: false
    property bool isUnderline: false
    property bool isStrikeout: false

    property color primaryColor: "#ffffff"
    property color secondaryColor: "#00ffff"
    property color outlineColor: "#000000"
    property color shadowColor: "#000000"

    property int marginLeft: 10
    property int marginRight: 10
    property int marginVertical: 10

    property int alignment: 2 // 1-9 ASS numpad alignment
    property real outlineWidth: 2.0
    property real shadowDistance: 2.0
    property int borderStyle: 1

    property real scaleX: 100
    property real scaleY: 100
    property real rotationAngle: 0
    property real spacingPx: 0
    property int encodingCode: 1

    // Emitted when style properties are applied or saved
    signal styleSaved(var styleData)

    // Emitted when user clicks a color swatch to select a color
    signal openColorPickerRequested(string propName, color currentColor)

    function loadStyle(styleItem, isStorage) {
        if (!styleItem) return;
        dialog.styleName = styleItem.name || "Default";
        txtStyleName.text = dialog.styleName;
        if (styleItem.font) {
            var fIdx = cmbFont.find(styleItem.font);
            if (fIdx >= 0) {
                cmbFont.currentIndex = fIdx;
            } else {
                var fList = cmbFont.model.slice();
                fList.push(styleItem.font);
                cmbFont.model = fList;
                cmbFont.currentIndex = fList.length - 1;
            }
        }
        if (styleItem.size !== undefined) spinFontSize.value = Math.round(styleItem.size);
        chkBold.checked = !!styleItem.bold;
        chkItalic.checked = !!styleItem.italic;
        chkUnderline.checked = !!styleItem.underline;
        chkStrikeout.checked = !!styleItem.strikeout;

        if (styleItem.primary) {
            dialog.primaryColor = (typeof aegisubCore !== "undefined" && aegisubCore) ? aegisubCore.parseColor(styleItem.primary) : styleItem.primary;
        }
        if (styleItem.secondary) {
            dialog.secondaryColor = (typeof aegisubCore !== "undefined" && aegisubCore) ? aegisubCore.parseColor(styleItem.secondary) : styleItem.secondary;
        }
        var outCol = styleItem.outline || styleItem.outlineColor;
        if (outCol) {
            dialog.outlineColor = (typeof aegisubCore !== "undefined" && aegisubCore) ? aegisubCore.parseColor(outCol) : outCol;
        }
        var shCol = styleItem.shadow || styleItem.shadowColor;
        if (shCol) {
            dialog.shadowColor = (typeof aegisubCore !== "undefined" && aegisubCore) ? aegisubCore.parseColor(shCol) : shCol;
        }

        var mL = (styleItem.marginLeft !== undefined) ? styleItem.marginLeft : ((styleItem.marginL !== undefined) ? styleItem.marginL : 10);
        var mR = (styleItem.marginRight !== undefined) ? styleItem.marginRight : ((styleItem.marginR !== undefined) ? styleItem.marginR : 10);
        var mV = (styleItem.marginVert !== undefined) ? styleItem.marginVert : ((styleItem.marginV !== undefined) ? styleItem.marginV : 10);
        spinMarginL.value = mL;
        spinMarginR.value = mR;
        spinMarginV.value = mV;

        if (styleItem.alignment !== undefined) dialog.alignment = styleItem.alignment;
        if (styleItem.outlineWidth !== undefined) spinOutline.value = Math.round(styleItem.outlineWidth);
        var shDist = (styleItem.shadowDistance !== undefined) ? styleItem.shadowDistance : ((styleItem.shadowDepth !== undefined) ? styleItem.shadowDepth : 2.0);
        spinShadow.value = Math.round(shDist);

        if (styleItem.borderStyle === 3) radOpaqueBox.checked = true;
        else radOutline.checked = true;

        if (styleItem.scaleX !== undefined) spinScaleX.value = Math.round(styleItem.scaleX);
        if (styleItem.scaleY !== undefined) spinScaleY.value = Math.round(styleItem.scaleY);
        var rot = (styleItem.angle !== undefined) ? styleItem.angle : ((styleItem.rotation !== undefined) ? styleItem.rotation : 0);
        spinRotation.value = Math.round(rot);
        if (styleItem.spacing !== undefined) spinSpacing.value = Math.round(styleItem.spacing);
        if (styleItem.encoding !== undefined) cmbEncoding.currentIndex = Math.max(0, styleItem.encoding);
    }

    function saveStyle(closeAfter) {
        var primAss = (typeof aegisubCore !== "undefined" && aegisubCore) ? aegisubCore.formatAssStyleColor(primaryColor) : "&H00FFFFFF";
        var secAss = (typeof aegisubCore !== "undefined" && aegisubCore) ? aegisubCore.formatAssStyleColor(secondaryColor) : "&H000000FF";
        var outAss = (typeof aegisubCore !== "undefined" && aegisubCore) ? aegisubCore.formatAssStyleColor(outlineColor) : "&H00000000";
        var shaAss = (typeof aegisubCore !== "undefined" && aegisubCore) ? aegisubCore.formatAssStyleColor(shadowColor) : "&H00000000";

        var data = {
            name: txtStyleName.text || "Default",
            font: cmbFont.currentText || "Arial",
            size: spinFontSize.value,
            bold: chkBold.checked,
            italic: chkItalic.checked,
            underline: chkUnderline.checked,
            strikeout: chkStrikeout.checked,
            primary: primAss,
            secondary: secAss,
            outline: outAss,
            shadow: shaAss,
            outlineColor: outAss,
            shadowColor: shaAss,
            marginLeft: spinMarginL.value,
            marginRight: spinMarginR.value,
            marginVert: spinMarginV.value,
            marginL: spinMarginL.value,
            marginR: spinMarginR.value,
            marginV: spinMarginV.value,
            alignment: dialog.alignment,
            outlineWidth: spinOutline.value,
            shadowDistance: spinShadow.value,
            shadowDepth: spinShadow.value,
            borderStyle: radOutline.checked ? 1 : 3,
            scaleX: spinScaleX.value,
            scaleY: spinScaleY.value,
            angle: spinRotation.value,
            rotation: spinRotation.value,
            spacing: spinSpacing.value,
            encoding: cmbEncoding.currentIndex
        };
        dialog.styleSaved(data);
        if (closeAfter) dialog.close();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Style identifier and font typography
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            NativeGroupBox {
                title: qsTr("Style name")
                Layout.preferredWidth: 160
                implicitHeight: 52

                RowLayout {
                    anchors.fill: parent
                    NativeTextBox {
                        id: txtStyleName
                        text: dialog.styleName
                        Layout.fillWidth: true
                    }
                }
            }

            NativeGroupBox {
                title: qsTr("Font")
                Layout.fillWidth: true
                implicitHeight: 52

                RowLayout {
                    anchors.fill: parent
                    spacing: 6

                    NativeComboBox {
                        id: cmbFont
                        Layout.fillWidth: true
                        model: ["Microsoft YaHei UI", "Segoe UI", "Arial", "SimHei", "SimSun", "Times New Roman"]
                        currentIndex: 0
                    }

                    NativeSpinBox {
                        id: spinFontSize
                        Layout.preferredWidth: 60
                        from: 1; to: 500; value: dialog.fontSize
                    }

                    NativeCheckBox { id: chkBold; text: "B"; font.bold: true; checked: dialog.isBold }
                    NativeCheckBox { id: chkItalic; text: "I"; font.italic: true; checked: dialog.isItalic }
                    NativeCheckBox { id: chkUnderline; text: "U"; font.underline: true; checked: dialog.isUnderline }
                    NativeCheckBox { id: chkStrikeout; text: "S"; font.strikeout: true; checked: dialog.isStrikeout }
                }
            }
        }

        // Colors and layout margins
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            // ASS primary, secondary, outline, and shadow colors
            NativeGroupBox {
                title: qsTr("Colors")
                Layout.preferredWidth: 320
                implicitHeight: 85

                GridLayout {
                    anchors.fill: parent
                    columns: 4
                    columnSpacing: 6
                    rowSpacing: 4

                    Text { text: qsTr("Primary") + ":"; font.pixelSize: 11 }
                    Rectangle {
                        width: 48; height: 18; color: dialog.primaryColor; border.color: "#808080"; border.width: 1
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dialog.openColorPickerRequested("primary", dialog.primaryColor)
                        }
                    }

                    Text { text: qsTr("Secondary") + ":"; font.pixelSize: 11 }
                    Rectangle {
                        width: 48; height: 18; color: dialog.secondaryColor; border.color: "#808080"; border.width: 1
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dialog.openColorPickerRequested("secondary", dialog.secondaryColor)
                        }
                    }

                    Text { text: qsTr("Outline") + ":"; font.pixelSize: 11 }
                    Rectangle {
                        width: 48; height: 18; color: dialog.outlineColor; border.color: "#808080"; border.width: 1
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dialog.openColorPickerRequested("outline", dialog.outlineColor)
                        }
                    }

                    Text { text: qsTr("Shadow") + ":"; font.pixelSize: 11 }
                    Rectangle {
                        width: 48; height: 18; color: dialog.shadowColor; border.color: "#808080"; border.width: 1
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dialog.openColorPickerRequested("shadow", dialog.shadowColor)
                        }
                    }
                }
            }

            // Subtitle screen margins (left, right, vertical)
            NativeGroupBox {
                title: qsTr("Margins")
                Layout.fillWidth: true
                implicitHeight: 85

                RowLayout {
                    anchors.fill: parent
                    spacing: 6

                    Text { text: qsTr("Left:"); font.pixelSize: 11 }
                    NativeSpinBox { id: spinMarginL; Layout.fillWidth: true; from: 0; to: 1000; value: dialog.marginLeft }

                    Text { text: qsTr("Right:"); font.pixelSize: 11 }
                    NativeSpinBox { id: spinMarginR; Layout.fillWidth: true; from: 0; to: 1000; value: dialog.marginRight }

                    Text { text: qsTr("Vertical:"); font.pixelSize: 11 }
                    NativeSpinBox { id: spinMarginV; Layout.fillWidth: true; from: 0; to: 1000; value: dialog.marginVertical }
                }
            }
        }

        // Alignment, border styling, and coordinate transforms
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            // Screen alignment (numpad matrix 1-9)
            NativeGroupBox {
                title: qsTr("Alignment")
                Layout.preferredWidth: 120
                implicitWidth: 120
                implicitHeight: 110

                GridLayout {
                    anchors.centerIn: parent
                    columns: 3
                    rowSpacing: 2
                    columnSpacing: 2

                    Repeater {
                        model: [7, 8, 9, 4, 5, 6, 1, 2, 3]
                        NativeRadioButton {
                            text: modelData.toString()
                            checked: dialog.alignment === modelData
                            onClicked: dialog.alignment = modelData
                        }
                    }
                }
            }

            // Border style, outline width, and shadow offset
            NativeGroupBox {
                title: qsTr("Outline & Shadow")
                Layout.preferredWidth: 200
                implicitHeight: 110

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    RowLayout {
                        NativeRadioButton { id: radOutline; text: qsTr("Outline"); checked: dialog.borderStyle === 1 }
                        NativeRadioButton { id: radOpaqueBox; text: qsTr("Opaque box"); checked: dialog.borderStyle === 3 }
                    }

                    RowLayout {
                        Text { text: qsTr("Outline") + ":"; font.pixelSize: 11 }
                        NativeSpinBox { id: spinOutline; Layout.fillWidth: true; from: 0; to: 100; value: Math.round(dialog.outlineWidth) }
                    }

                    RowLayout {
                        Text { text: qsTr("Shadow") + ":"; font.pixelSize: 11 }
                        NativeSpinBox { id: spinShadow; Layout.fillWidth: true; from: 0; to: 100; value: Math.round(dialog.shadowDistance) }
                    }
                }
            }

            // Scale, rotation, letter spacing, and font encoding
            NativeGroupBox {
                title: qsTr("Miscellaneous")
                Layout.fillWidth: true
                implicitHeight: 110

                GridLayout {
                    anchors.fill: parent
                    columns: 4
                    columnSpacing: 4
                    rowSpacing: 4

                    Text { text: qsTr("Scale X %:"); font.pixelSize: 11 }
                    NativeSpinBox { id: spinScaleX; Layout.fillWidth: true; from: 0; to: 1000; value: Math.round(dialog.scaleX) }

                    Text { text: qsTr("Scale Y %:"); font.pixelSize: 11 }
                    NativeSpinBox { id: spinScaleY; Layout.fillWidth: true; from: 0; to: 1000; value: Math.round(dialog.scaleY) }

                    Text { text: qsTr("Rotation:"); font.pixelSize: 11 }
                    NativeSpinBox { id: spinRotation; Layout.fillWidth: true; from: -360; to: 360; value: Math.round(dialog.rotationAngle) }

                    Text { text: qsTr("Spacing:"); font.pixelSize: 11 }
                    NativeSpinBox { id: spinSpacing; Layout.fillWidth: true; from: -100; to: 100; value: Math.round(dialog.spacingPx) }

                    Text { text: qsTr("Encoding:"); font.pixelSize: 11 }
                    NativeComboBox {
                        id: cmbEncoding
                        Layout.columnSpan: 3
                        Layout.fillWidth: true
                        model: ["0 - ANSI", "1 - Default", "128 - Shift_JIS", "134 - GB2312", "136 - Big5"]
                        currentIndex: 1
                    }
                }
            }
        }

        // Live text rendering preview
        NativeGroupBox {
            title: qsTr("Preview")
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                anchors.fill: parent
                color: "#1e1e1e"
                border.color: "#7f9db9"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Aegisub Style Preview 123")
                    font.family: cmbFont.currentText
                    font.pixelSize: Math.min(32, spinFontSize.value)
                    font.bold: chkBold.checked
                    font.italic: chkItalic.checked
                    font.underline: chkUnderline.checked
                    font.strikeout: chkStrikeout.checked
                    color: dialog.primaryColor
                    style: Text.Outline
                    styleColor: dialog.outlineColor
                }
            }
        }

        // Dialog action buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Item { Layout.fillWidth: true }

            NativeButton {
                text: qsTr("Apply"); Layout.preferredWidth: 75
                onClicked: dialog.saveStyle(false)
            }

            NativeButton {
                text: qsTr("OK"); isDefault: true
                Layout.preferredWidth: 75
                onClicked: dialog.saveStyle(true)
            }

            NativeButton {
                text: qsTr("Cancel"); Layout.preferredWidth: 75
                onClicked: dialog.close()
            }

            NativeButton {
                text: qsTr("Help"); Layout.preferredWidth: 75
                onClicked: Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Styles/")
            }
        }
    }
}
