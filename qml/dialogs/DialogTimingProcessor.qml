// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogTimingProcessor: Batch subtitle timing processor providing lead-in/lead-out
// padding, adjacent line gap snapping with bias control, and keyframe alignment.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Timing Post-Processor")
    iconSource: "../../assets/icons_native/timing_processor_toolbutton_16.png"
    width: implicitWidth
    height: implicitHeight
    implicitWidth: 560
    implicitHeight: 480

    property var project: null
    property var videoCtrl: null
    property var processHandler: null
    property string errorMessage: ""
    signal timingProcessRequested(var options)

    property var stylesModel: [
        { name: "Default", checked: true },
        { name: "Alt", checked: true },
        { name: "Sign", checked: true },
        { name: "Title", checked: true }
    ]

    function refreshStyles() {
        var model = project ? project.subtitleModel :
                    ((typeof nativeSubtitleModel !== "undefined") ? nativeSubtitleModel : null);
        if (model && model.styleNames.length > 0) {
            var list = [];
            for (var i = 0; i < model.styleNames.length; ++i) {
                list.push({ name: model.styleNames[i], checked: true });
            }
            dialog.stylesModel = list;
        } else dialog.stylesModel = [];
    }
    onAboutToShow: {
        refreshStyles();
        if (!chkKeysEnable.enabled) chkKeysEnable.checked = false;
        errorMessage = "";
    }

    function checkAll(chk) {
        var items = dialog.stylesModel;
        for (var i = 0; i < items.length; ++i) {
            items[i].checked = chk;
        }
        dialog.stylesModel = [].concat(items);
    }

    function doProcess(isOk) {
        var allowed = [];
        for (var i = 0; i < stylesModel.length; ++i) {
            if (stylesModel[i].checked) allowed.push(stylesModel[i].name);
        }

        var options = {
            leadIn: chkLeadIn.checked ? spinLeadIn.value : 0,
            leadOut: chkLeadOut.checked ? spinLeadOut.value : 0,
            adjacentEnabled: chkAdjEnable.checked,
            maxGap: spinAdjGap.value,
            maxOverlap: spinAdjOverlap.value,
            bias: sliderBias.value,
            selectedOnly: chkSelectionOnly.checked,
            allowedStyles: allowed,
            keyframesEnabled: chkKeysEnable.checked && chkKeysEnable.enabled,
            beforeStart: spinBeforeStart.value,
            afterStart: spinAfterStart.value,
            beforeEnd: spinBeforeEnd.value,
            afterEnd: spinAfterEnd.value
        };
        if (allowed.length === 0 || !(options.leadIn > 0 || options.leadOut > 0 || options.adjacentEnabled || options.keyframesEnabled)) {
            errorMessage = qsTr("Select at least one style and timing operation");
            return;
        }
        var result = processHandler ? processHandler(options) : {success: true};
        if (!result.success) { errorMessage = result.message || qsTr("Timing processing failed"); return; }
        errorMessage = "";
        dialog.timingProcessRequested(options);
        if (isOk) dialog.close();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Top section: style filter on left, timing parameters on right
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // Target styles selection
            NativeGroupBox {
                title: qsTr("Apply to styles")
                Layout.preferredWidth: 155
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
                            id: lvStyles
                            anchors.fill: parent
                            anchors.margins: 2
                            clip: true
                            model: dialog.stylesModel

                            delegate: Item {
                                width: lvStyles.width
                                height: 22

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 4
                                    anchors.rightMargin: 4
                                    spacing: 4

                                    NativeCheckBox {
                                        checked: modelData.checked
                                        text: modelData.name
                                        onCheckedChanged: {
                                            var items = dialog.stylesModel;
                                            items[index].checked = checked;
                                            dialog.stylesModel = items;
                                        }
                                    }
                                }
                            }
                        }
                    }

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
                    }
                }
            }

            // Timing adjustment configuration panels
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 6

                // Row selection scope
                NativeGroupBox {
                    title: qsTr("Options")
                    Layout.fillWidth: true
                    implicitHeight: 46

                    RowLayout {
                        anchors.fill: parent
                        NativeCheckBox {
                            id: chkSelectionOnly
                            objectName: "timing-selection-only"
                            text: qsTr("Only affect selected lines"); checked: false
                        }
                    }
                }

                // Lead-in and lead-out offsets
                NativeGroupBox {
                    title: qsTr("Lead-in/Lead-out")
                    Layout.fillWidth: true
                    implicitHeight: 64

                    GridLayout {
                        anchors.fill: parent
                        columns: 4
                        columnSpacing: 6
                        rowSpacing: 4

                        NativeCheckBox {
                            id: chkLeadIn
                            text: qsTr("Add lead in:"); checked: true
                        }
                        NativeSpinBox {
                            id: spinLeadIn
                            Layout.preferredWidth: 65
                            from: 0; to: 36000; value: 200
                            enabled: chkLeadIn.checked
                        }

                        NativeCheckBox {
                            id: chkLeadOut
                            text: qsTr("Add lead out:"); checked: true
                        }
                        NativeSpinBox {
                            id: spinLeadOut
                            Layout.preferredWidth: 65
                            from: 0; to: 36000; value: 300
                            enabled: chkLeadOut.checked
                        }
                    }
                }

                // Adjacent subtitle continuity and gap snapping
                NativeGroupBox {
                    title: qsTr("Make adjacent subtitles continuous")
                    Layout.fillWidth: true
                    implicitHeight: 100

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 4

                        NativeCheckBox {
                            id: chkAdjEnable
                            text: qsTr("Enable"); checked: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text { text: qsTr("Max gap:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                            NativeSpinBox {
                                id: spinAdjGap
                                Layout.preferredWidth: 60
                                from: 0; to: 10000; value: 200
                                enabled: chkAdjEnable.checked
                            }

                            Text { text: qsTr("Max overlap:"); font.pixelSize: 12; font.family: uiTheme.uiFont }
                            NativeSpinBox {
                                id: spinAdjOverlap
                                Layout.preferredWidth: 60
                                from: 0; to: 10000; value: 100
                                enabled: chkAdjEnable.checked
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text { text: qsTr("Bias: Start <- "); font.pixelSize: 11; font.family: uiTheme.uiFont }
                            Slider {
                                id: sliderBias
                                Layout.fillWidth: true
                                from: 0.0; to: 1.0; value: 0.5
                                enabled: chkAdjEnable.checked
                            }
                            Text { text: qsTr(" -> End"); font.pixelSize: 11; font.family: uiTheme.uiFont }
                        }
                    }
                }

                // Video keyframe threshold snapping
                NativeGroupBox {
                    title: qsTr("Keyframe snapping")
                    Layout.fillWidth: true
                    implicitHeight: 100

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 4

                        NativeCheckBox {
                            id: chkKeysEnable
                            text: qsTr("Enable"); checked: false
                            enabled: !!(dialog.videoCtrl && dialog.videoCtrl.hasKeyframes &&
                                        dialog.videoCtrl.exportFramerateContext.available)
                        }

                        GridLayout {
                            columns: 4
                            columnSpacing: 6
                            rowSpacing: 4
                            Layout.fillWidth: true

                            Text { text: qsTr("Threshold:"); font.pixelSize: 11 }
                            NativeSpinBox { id: spinBeforeStart; Layout.preferredWidth: 60; from: 0; to: 5000; value: 200; enabled: chkKeysEnable.checked }

                            Text { text: qsTr("Starts after thres.:"); font.pixelSize: 11 }
                            NativeSpinBox { id: spinAfterStart; Layout.preferredWidth: 60; from: 0; to: 5000; value: 200; enabled: chkKeysEnable.checked }

                            Text { text: qsTr("Ends before thres.:"); font.pixelSize: 11 }
                            NativeSpinBox { id: spinBeforeEnd; Layout.preferredWidth: 60; from: 0; to: 5000; value: 200; enabled: chkKeysEnable.checked }

                            Text { text: qsTr("Ends after thres.:"); font.pixelSize: 11 }
                            NativeSpinBox { id: spinAfterEnd; Layout.preferredWidth: 60; from: 0; to: 5000; value: 200; enabled: chkKeysEnable.checked }
                        }
                    }
                }
            }
        }

        // Dialog action buttons
        Text {
            text: dialog.errorMessage
            visible: text.length > 0
            color: "#b00020"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            NativeButton {
                text: qsTr("Apply"); Layout.preferredWidth: 75
                onClicked: dialog.doProcess(false)
            }

            Item { Layout.fillWidth: true }

            NativeButton {
                text: qsTr("OK"); isDefault: true
                Layout.preferredWidth: 75
                onClicked: dialog.doProcess(true)
            }

            NativeButton {
                text: qsTr("Cancel"); Layout.preferredWidth: 75
                onClicked: dialog.close()
            }

            NativeButton {
                text: qsTr("Help"); Layout.preferredWidth: 75
                onClicked: {
                    Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Timing_Post-Processor/")
                }
            }
        }
    }
}
