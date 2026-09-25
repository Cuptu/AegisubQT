// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../controls"

// Preferences page: Video playback, viewport zoom levels, screenshot directory, and script resolution.
Flickable {
    id: root
    contentHeight: mainLayout.implicitHeight
    boundsBehavior: Flickable.StopAtBounds

    signal changed()

    function restoreDefaults() {
        chkVideoShowKeyframes.checked = true;
        chkVisualAutohide.checked = false;
        chkVideoSubtitleSync.checked = true;
        chkVideoOpenAudio.checked = true;
        cmbDefaultZoom.currentIndex = 7;
        spinFastJump.value = 10;
        cmbScreenshotPath.currentIndex = 0;
        chkAutoRes.checked = true;
        spinDefWidth.value = 1280;
        spinDefHeight.value = 720;
        cmbMatchRes.currentIndex = 0;
        root.changed();
    }

    function savePreferences() {
        // Persist video preferences
    }

    ColumnLayout {
        id: mainLayout
        width: root.width - 8
        spacing: 8

        NativeGroupBox {
            title: qsTr("Options")
            Layout.fillWidth: true
            implicitHeight: colOptions.implicitHeight + 28

            ColumnLayout {
                id: colOptions
                anchors.fill: parent
                spacing: 4

                NativeCheckBox {
                    id: chkVideoShowKeyframes
                    text: qsTr("Show keyframes in slider")
                    checked: true
                    onCheckedChanged: root.changed()
                }
                NativeCheckBox {
                    id: chkVisualAutohide
                    text: qsTr("Only show visual tools when mouse is over video")
                    checked: false
                    onCheckedChanged: root.changed()
                }
                NativeCheckBox {
                    id: chkVideoSubtitleSync
                    text: qsTr("Seek video to line start on selection change")
                    checked: true
                    onCheckedChanged: root.changed()
                }
                NativeCheckBox {
                    id: chkVideoOpenAudio
                    text: qsTr("Automatically open audio when opening video")
                    checked: true
                    onCheckedChanged: root.changed()
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Default Zoom")
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        Layout.fillWidth: true
                    }
                    NativeComboBox {
                        id: cmbDefaultZoom
                        Layout.preferredWidth: 160
                        model: [
                            "12.5%", "25%", "37.5%", "50%", "62.5%", "75%", "87.5%", "100%",
                            "112.5%", "125%", "137.5%", "150%", "162.5%", "175%", "187.5%", "200%",
                            "212.5%", "225%", "237.5%", "250%", "262.5%", "275%", "287.5%", "300%"
                        ]
                        currentIndex: (typeof videoDisplayController !== "undefined" && videoDisplayController)
                                      ? Math.max(0, model.indexOf(videoDisplayController.zoomText)) : 7
                        onCurrentIndexChanged: {
                            if (typeof videoDisplayController !== "undefined" && videoDisplayController)
                                videoDisplayController.setZoomText(currentText);
                            root.changed();
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Fast jump step in frames")
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        Layout.fillWidth: true
                    }
                    NativeSpinBox {
                        id: spinFastJump
                        Layout.preferredWidth: 160
                        from: 1; to: 1000; value: 10
                        onValueModified: root.changed()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Screenshot save path")
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        Layout.fillWidth: true
                    }
                    NativeComboBox {
                        id: cmbScreenshotPath
                        Layout.preferredWidth: 160
                        model: ["?video", "?script", "."]
                        currentIndex: 0
                        onCurrentIndexChanged: root.changed()
                    }
                }
            }
        }

        NativeGroupBox {
            title: qsTr("Script Resolution")
            Layout.fillWidth: true
            implicitHeight: colRes.implicitHeight + 28

            ColumnLayout {
                id: colRes
                anchors.fill: parent
                spacing: 4

                NativeCheckBox {
                    id: chkAutoRes
                    text: qsTr("Use resolution of first video opened")
                    checked: true
                    onCheckedChanged: root.changed()
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Default width")
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        Layout.fillWidth: true
                        color: chkAutoRes.checked ? "#6d6d6d" : "#000000"
                    }
                    NativeSpinBox {
                        id: spinDefWidth
                        Layout.preferredWidth: 160
                        enabled: !chkAutoRes.checked
                        from: 1; to: 10000; value: 1280
                        onValueModified: root.changed()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Default height")
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        Layout.fillWidth: true
                        color: chkAutoRes.checked ? "#6d6d6d" : "#000000"
                    }
                    NativeSpinBox {
                        id: spinDefHeight
                        Layout.preferredWidth: 160
                        enabled: !chkAutoRes.checked
                        from: 1; to: 10000; value: 720
                        onValueModified: root.changed()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Match video resolution on open")
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        Layout.fillWidth: true
                    }
                    NativeComboBox {
                        id: cmbMatchRes
                        Layout.preferredWidth: 160
                        model: [qsTr("Ask"), qsTr("Never"), qsTr("Always resample")]
                        currentIndex: 0
                        onCurrentIndexChanged: root.changed()
                    }
                }
            }
        }
    }
}
