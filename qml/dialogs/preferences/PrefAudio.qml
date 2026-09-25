// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../controls"

// Preferences page: Audio playback behavior, grab/snap timing parameters, and display overlays.
Flickable {
    id: root
    contentHeight: mainLayout.implicitHeight
    boundsBehavior: Flickable.StopAtBounds

    signal changed()

    function restoreDefaults() {
        chkWheelZoom.checked = false;
        chkLockScroll.checked = false;
        chkSnapMarkers.checked = true;
        chkAutoFocus.checked = false;
        chkPlayOnStep.checked = false;
        chkLeftDragMovesEnd.checked = true;
        spinTimingLength.value = 2000;
        // Default values (100 / 350 / 3) matching native default_config.json
        spinLeadIn.value = 100;
        spinLeadOut.value = 350;
        spinBoundaryThickness.value = 2;
        cmbInactiveLines.currentIndex = 3;
        chkKeyframesDialogue.checked = true;
        chkKeyframesKaraoke.checked = true;
        chkCursorTime.checked = true;
        chkVideoPosition.checked = true;
        chkSeconds.checked = true;
        root.changed();
    }

    function savePreferences() {
        // Persist audio preferences
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
                    id: chkWheelZoom
                    text: qsTr("Default mouse wheel to zoom")
                    checked: true
                    onCheckedChanged: root.changed()
                }
                NativeCheckBox {
                    id: chkLockScroll
                    text: qsTr("Lock scroll on cursor")
                    checked: false
                    onCheckedChanged: root.changed()
                }
                NativeCheckBox {
                    id: chkSnapMarkers
                    text: qsTr("Snap markers by default")
                    checked: true
                    onCheckedChanged: root.changed()
                }
                NativeCheckBox {
                    id: chkAutoFocus
                    text: qsTr("Auto-focus on mouse over")
                    checked: false
                    onCheckedChanged: root.changed()
                }
                NativeCheckBox {
                    id: chkPlayOnStep
                    text: qsTr("Play audio when stepping in video")
                    checked: false
                    onCheckedChanged: root.changed()
                }
                NativeCheckBox {
                    id: chkLeftDragMovesEnd
                    text: qsTr("Left-click-drag moves end marker")
                    checked: true
                    onCheckedChanged: root.changed()
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Default timing length (ms)") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.fillWidth: true }
                    NativeSpinBox { id: spinTimingLength; from: 0; to: 36000; value: 2000; Layout.preferredWidth: 80; onValueModified: root.changed() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Default lead-in length (ms)") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.fillWidth: true }
                    NativeSpinBox {
                        id: spinLeadIn
                        from: 0; to: 36000; value: audioController ? audioController.leadInMs : 100
                        Layout.preferredWidth: 80
                        onValueModified: { if (audioController) audioController.leadInMs = value; root.changed(); }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Default lead-out length (ms)") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.fillWidth: true }
                    NativeSpinBox {
                        id: spinLeadOut
                        from: 0; to: 36000; value: audioController ? audioController.leadOutMs : 350
                        Layout.preferredWidth: 80
                        onValueModified: { if (audioController) audioController.leadOutMs = value; root.changed(); }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Show inactive lines") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.fillWidth: true }
                    NativeComboBox {
                        id: cmbInactiveLines
                        model: [qsTr("Don't show"), qsTr("Show previous"), qsTr("Show previous and next"), qsTr("Show all")]
                        currentIndex: audioController ? audioController.inactiveLinesMode : 3
                        Layout.preferredWidth: 160
                        onCurrentIndexChanged: { if (audioController) audioController.inactiveLinesMode = currentIndex; root.changed(); }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Line boundary thickness (px)") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.fillWidth: true }
                    NativeSpinBox {
                        id: spinBoundaryThickness
                        from: 1; to: 5; value: audioController ? audioController.lineBoundaryThickness : 2
                        Layout.preferredWidth: 80
                        onValueModified: { if (audioController) audioController.lineBoundaryThickness = value; root.changed(); }
                    }
                }
            }
        }

        NativeGroupBox {
            title: qsTr("Display Visual Options")
            Layout.fillWidth: true
            implicitHeight: colVisual.implicitHeight + 28

            ColumnLayout {
                id: colVisual
                anchors.fill: parent
                spacing: 4

                NativeCheckBox {
                    id: chkKeyframesDialogue
                    text: qsTr("Keyframes in dialogue mode")
                    checked: audioController ? audioController.drawKeyframesEnabled : true
                    onCheckedChanged: { if (audioController) audioController.drawKeyframesEnabled = checked; root.changed(); }
                }
                NativeCheckBox { id: chkKeyframesKaraoke; text: qsTr("Keyframes in karaoke mode"); checked: true; onCheckedChanged: root.changed() }
                NativeCheckBox {
                    id: chkCursorTime
                    text: qsTr("Cursor time")
                    checked: audioController ? audioController.drawCursorTime : true
                    onCheckedChanged: { if (audioController) audioController.drawCursorTime = checked; root.changed(); }
                }
                NativeCheckBox {
                    id: chkVideoPosition
                    text: qsTr("Video position")
                    checked: audioController ? audioController.drawVideoPosition : true
                    onCheckedChanged: { if (audioController) audioController.drawVideoPosition = checked; root.changed(); }
                }
                NativeCheckBox {
                    id: chkSeconds
                    text: qsTr("Seconds boundaries")
                    checked: audioController ? audioController.drawSeconds : true
                    onCheckedChanged: { if (audioController) audioController.drawSeconds = checked; root.changed(); }
                }
            }
        }
    }
}
