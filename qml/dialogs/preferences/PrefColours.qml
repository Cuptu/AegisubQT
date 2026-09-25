// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../../controls"

// Preferences page: Complete 1:1 color scheme and syntax highlighting configuration.
Flickable {
    id: root
    contentWidth: width
    contentHeight: contentRow.implicitHeight + 16
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    signal changed()

    ColorDialog {
        id: prefColorDialog
        title: qsTr("Select Color")
        property var activeTarget: null
        onAccepted: {
            if (activeTarget) {
                activeTarget.color = selectedColor;
                root.changed();
            }
        }
    }

    component ColorOptionRow: RowLayout {
        id: cRow
        property string labelText: ""
        property alias color: swatch.color
        Layout.fillWidth: true
        spacing: 4

        Text {
            text: cRow.labelText
            font.pixelSize: 12
            font.family: "Segoe UI, Microsoft YaHei UI, Tahoma, sans-serif"
            renderType: Text.NativeRendering
            color: "#000000"
            Layout.fillWidth: true
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Rectangle {
            id: swatch
            Layout.preferredWidth: 38
            Layout.preferredHeight: 16
            radius: 2
            border.color: cMouse.containsMouse ? "#0078d4" : "#7a7a7a"
            border.width: 1

            MouseArea {
                id: cMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    prefColorDialog.selectedColor = swatch.color;
                    prefColorDialog.activeTarget = swatch;
                    prefColorDialog.open();
                }
            }
        }
    }

    function restoreDefaults() {
        // Reset colours to built-in Aegisub defaults
        colPlayCursor.color = "#ffffff";
        colLineBoundaryStart.color = "#d80000";
        colLineBoundaryEnd.color = "#0000d8";
        colLineBoundaryInactive.color = "#bebebe";
        colSyllableBoundaries.color = "#ffff00";
        colSecondsBoundaries.color = "#0064ff";

        colSyntaxBg.color = "#ffffff";
        colSyntaxNormal.color = "#000000";
        colSyntaxComment.color = "#000000";
        colSyntaxDrawing.color = "#000000";
        colSyntaxBrackets.color = "#1432ff";
        colSyntaxSlashes.color = "#ff00c8";
        colSyntaxTags.color = "#5a5a5a";
        colSyntaxParams.color = "#285a28";
        colSyntaxError.color = "#c80000";
        colSyntaxErrorBg.color = "#ffc8c8";
        colSyntaxLineBreak.color = "#a0a0a0";
        colSyntaxKaraokeTpl.color = "#8000c0";
        colSyntaxKaraokeVar.color = "#8000c0";

        cmbSpectrumScheme.currentIndex = 0;
        cmbWaveformScheme.currentIndex = 0;

        colGridStandardFg.color = "#000000";
        colGridStandardBg.color = "#ffffff";
        colGridSelectionFg.color = "#000000";
        colGridSelectionBg.color = "#ceffea";
        colGridCollisionFg.color = "#ff0000";
        colGridInframeBg.color = "#fffdea";
        colGridCommentBg.color = "#d8def5";
        colGridSelectedCommentBg.color = "#d3eeee";
        colGridHeaderBg.color = "#a5cfe7";
        colGridLeftCol.color = "#c4ecc9";
        colGridActiveBorder.color = "#ff5bef";
        colGridLines.color = "#bebebe";
        colGridCpsError.color = "#ff0000";

        colVisualLinesPrimary.color = "#bb0000";
        colVisualLinesSecondary.color = "#6a2013";
        colVisualHighlightPrimary.color = "#ffa928";
        colVisualHighlightSecondary.color = "#fffdb9";
        spinShadedAreaAlpha.value = 5;

        root.changed();
    }

    function savePreferences() {
        // Persist color preferences
    }

    ScrollBar.vertical: ScrollBar {
        id: scrollBar
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        policy: (root.contentHeight > root.height) ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
    }

    RowLayout {
        id: contentRow
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.rightMargin: scrollBar.visible ? 16 : 6
        anchors.topMargin: 4
        spacing: 8

        // Column 1 (Left): Audio Display + Syntax Highlighting
        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 250
            Layout.alignment: Qt.AlignTop
            spacing: 8

            NativeGroupBox {
                title: qsTr("Audio Display")
                Layout.fillWidth: true
                implicitHeight: colAudioDisp.implicitHeight + 26

                ColumnLayout {
                    id: colAudioDisp
                    anchors.fill: parent
                    spacing: 3

                    ColorOptionRow { id: colPlayCursor; labelText: qsTr("Play cursor"); color: "#ffffff" }
                    ColorOptionRow { id: colLineBoundaryStart; labelText: qsTr("Line boundary start"); color: "#d80000" }
                    ColorOptionRow { id: colLineBoundaryEnd; labelText: qsTr("Line boundary end"); color: "#0000d8" }
                    ColorOptionRow { id: colLineBoundaryInactive; labelText: qsTr("Line boundary inactive line"); color: "#bebebe" }
                    ColorOptionRow { id: colSyllableBoundaries; labelText: qsTr("Syllable boundaries"); color: "#ffff00" }
                    ColorOptionRow { id: colSecondsBoundaries; labelText: qsTr("Seconds boundaries"); color: "#0064ff" }
                }
            }

            NativeGroupBox {
                title: qsTr("Syntax Highlighting")
                Layout.fillWidth: true
                implicitHeight: colSyntaxHigh.implicitHeight + 26

                ColumnLayout {
                    id: colSyntaxHigh
                    anchors.fill: parent
                    spacing: 3

                    ColorOptionRow { id: colSyntaxBg; labelText: qsTr("Background"); color: "#ffffff" }
                    ColorOptionRow { id: colSyntaxNormal; labelText: qsTr("Normal"); color: "#000000" }
                    ColorOptionRow { id: colSyntaxComment; labelText: qsTr("Comments"); color: "#000000" }
                    ColorOptionRow { id: colSyntaxDrawing; labelText: qsTr("Drawing Commands"); color: "#000000" }
                    ColorOptionRow { id: colSyntaxBrackets; labelText: qsTr("Brackets"); color: "#1432ff" }
                    ColorOptionRow { id: colSyntaxSlashes; labelText: qsTr("Slashes and Parentheses"); color: "#ff00c8" }
                    ColorOptionRow { id: colSyntaxTags; labelText: qsTr("Tags"); color: "#5a5a5a" }
                    ColorOptionRow { id: colSyntaxParams; labelText: qsTr("Parameters"); color: "#285a28" }
                    ColorOptionRow { id: colSyntaxError; labelText: qsTr("Error"); color: "#c80000" }
                    ColorOptionRow { id: colSyntaxErrorBg; labelText: qsTr("Error Background"); color: "#ffc8c8" }
                    ColorOptionRow { id: colSyntaxLineBreak; labelText: qsTr("Line Break"); color: "#a0a0a0" }
                    ColorOptionRow { id: colSyntaxKaraokeTpl; labelText: qsTr("Karaoke templates"); color: "#8000c0" }
                    ColorOptionRow { id: colSyntaxKaraokeVar; labelText: qsTr("Karaoke variables"); color: "#8000c0" }
                }
            }
        }

        // Column 2 (Right): Audio Color Schemes + Subtitle Grid + Visual Typesetting Tools
        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 260
            Layout.alignment: Qt.AlignTop
            spacing: 8

            NativeGroupBox {
                title: qsTr("Audio Color Schemes")
                Layout.fillWidth: true
                implicitHeight: colAudioSchemes.implicitHeight + 26

                ColumnLayout {
                    id: colAudioSchemes
                    anchors.fill: parent
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Spectrum")
                            font.pixelSize: 12
                            font.family: "Segoe UI, Microsoft YaHei UI, Tahoma, sans-serif"
                            renderType: Text.NativeRendering
                            color: "#000000"
                            Layout.fillWidth: true
                        }
                        NativeComboBox {
                            id: cmbSpectrumScheme
                            Layout.preferredWidth: 100
                            model: ["Icy Blue", "Classic Wave", "Fire", "Rainbow"]
                            currentIndex: 0
                            onCurrentIndexChanged: root.changed()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Waveform")
                            font.pixelSize: 12
                            font.family: "Segoe UI, Microsoft YaHei UI, Tahoma, sans-serif"
                            renderType: Text.NativeRendering
                            color: "#000000"
                            Layout.fillWidth: true
                        }
                        NativeComboBox {
                            id: cmbWaveformScheme
                            Layout.preferredWidth: 100
                            model: ["Green", "Blue", "Charcoal", "Mono"]
                            currentIndex: 0
                            onCurrentIndexChanged: root.changed()
                        }
                    }
                }
            }

            NativeGroupBox {
                title: qsTr("Subtitle Grid")
                Layout.fillWidth: true
                implicitHeight: colSubGrid.implicitHeight + 26

                ColumnLayout {
                    id: colSubGrid
                    anchors.fill: parent
                    spacing: 3

                    ColorOptionRow { id: colGridStandardFg; labelText: qsTr("Standard foreground"); color: "#000000" }
                    ColorOptionRow { id: colGridStandardBg; labelText: qsTr("Standard background"); color: "#ffffff" }
                    ColorOptionRow { id: colGridSelectionFg; labelText: qsTr("Selection foreground"); color: "#000000" }
                    ColorOptionRow { id: colGridSelectionBg; labelText: qsTr("Selection background"); color: "#ceffea" }
                    ColorOptionRow { id: colGridCollisionFg; labelText: qsTr("Collision foreground"); color: "#ff0000" }
                    ColorOptionRow { id: colGridInframeBg; labelText: qsTr("In frame background"); color: "#fffdea" }
                    ColorOptionRow { id: colGridCommentBg; labelText: qsTr("Comment background"); color: "#d8def5" }
                    ColorOptionRow { id: colGridSelectedCommentBg; labelText: qsTr("Selected comment background"); color: "#d3eeee" }
                    ColorOptionRow { id: colGridHeaderBg; labelText: qsTr("Header background"); color: "#a5cfe7" }
                    ColorOptionRow { id: colGridLeftCol; labelText: qsTr("Left Column"); color: "#c4ecc9" }
                    ColorOptionRow { id: colGridActiveBorder; labelText: qsTr("Active Line Border"); color: "#ff5bef" }
                    ColorOptionRow { id: colGridLines; labelText: qsTr("Lines"); color: "#bebebe" }
                    ColorOptionRow { id: colGridCpsError; labelText: qsTr("CPS Error"); color: "#ff0000" }
                }
            }

            NativeGroupBox {
                title: "Visual Typesetting Tools"
                Layout.fillWidth: true
                implicitHeight: colVisualTools.implicitHeight + 26

                ColumnLayout {
                    id: colVisualTools
                    anchors.fill: parent
                    spacing: 3

                    ColorOptionRow { id: colVisualLinesPrimary; labelText: "Primary Lines"; color: "#bb0000" }
                    ColorOptionRow { id: colVisualLinesSecondary; labelText: "Secondary Lines"; color: "#6a2013" }
                    ColorOptionRow { id: colVisualHighlightPrimary; labelText: "Primary Highlight"; color: "#ffa928" }
                    ColorOptionRow { id: colVisualHighlightSecondary; labelText: "Secondary Highlight"; color: "#fffdb9" }
                }
            }

            NativeGroupBox {
                title: "Visual Typesetting Tools Alpha"
                Layout.fillWidth: true
                implicitHeight: colVisualAlpha.implicitHeight + 26

                ColumnLayout {
                    id: colVisualAlpha
                    anchors.fill: parent
                    spacing: 3

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "Shaded Area"
                            font.pixelSize: 12
                            font.family: "Segoe UI, Microsoft YaHei UI, Tahoma, sans-serif"
                            renderType: Text.NativeRendering
                            color: "#000000"
                            Layout.fillWidth: true
                        }
                        SpinBox {
                            id: spinShadedAreaAlpha
                            Layout.preferredWidth: 65
                            from: 0
                            to: 10
                            value: 5
                            stepSize: 1
                            textFromValue: function(v) { return (v / 10.0).toFixed(1); }
                            valueFromText: function(t) { return Math.round(parseFloat(t) * 10); }
                            onValueModified: root.changed()
                        }
                    }
                }
            }
        }
    }
}
