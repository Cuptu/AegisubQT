// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogSelectLines: Query-based subtitle line selection dialog supporting text/style/actor/effect
// pattern matching, regex evaluation, set operations, and line type filtering.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    title: qsTr("Select")
    iconSource: "../../assets/icons_native/select_lines_button_16.png"
    width: 480
    height: 460
    implicitWidth: 480
    implicitHeight: 460

    // Emitted when a selection action is confirmed:
    // action: 0 = set, 1 = add, 2 = subtract, 3 = intersect
    // fieldIndex: 0 = text, 1 = style, 2 = actor, 3 = effect
    // mode: 0 = exact, 1 = contains, 2 = regex
    // invert: true to match non-matching lines
    // matchCase: case-sensitive flag
    // comments: include comment lines
    // dialogues: include dialogue lines
    // query: match pattern string
    signal selectLinesRequested(int action, int fieldIndex, int mode, bool invert, bool matchCase, bool comments, bool dialogues, string query)

    function doProcess(isOk) {
        var action = radActionSet.checked ? 0 : (radActionAdd.checked ? 1 : (radActionSub.checked ? 2 : 3));
        var field = radFieldText.checked ? 0 : (radFieldStyle.checked ? 1 : (radFieldActor.checked ? 2 : 3));
        var mode = radModeExact.checked ? 0 : (radModeContains.checked ? 1 : 2);
        var invert = radDoesntMatch.checked;
        var matchCase = chkCase.checked;
        var comments = chkComments.checked;
        var dialogues = chkDialogues.checked;
        var query = txtMatch.text;

        dialog.selectLinesRequested(action, field, mode, invert, matchCase, comments, dialogues, query);
        if (isOk) dialog.close();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Match condition: pattern, inversion, and case sensitivity
        NativeGroupBox {
            title: qsTr("Match")
            Layout.fillWidth: true
            implicitHeight: 76

            ColumnLayout {
                anchors.fill: parent
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    NativeRadioButton {
                        id: radMatches
                        text: qsTr("&Matches"); checked: true
                    }
                    NativeRadioButton {
                        id: radDoesntMatch
                        text: qsTr("&Doesn't Match")
                    }
                    NativeCheckBox {
                        id: chkCase
                        text: qsTr("Match c&ase"); checked: false
                    }
                }

                NativeTextBox {
                    id: txtMatch
                    Layout.fillWidth: true
                }
            }
        }

        // Match mode: exact, substring, or regular expression
        NativeGroupBox {
            title: qsTr("Match")
            Layout.fillWidth: true
            implicitHeight: 52

            RowLayout {
                anchors.fill: parent
                spacing: 12

                NativeRadioButton {
                    id: radModeExact
                    text: qsTr("&Exact match")
                }
                NativeRadioButton {
                    id: radModeContains
                    text: qsTr("&Contains"); checked: true
                }
                NativeRadioButton {
                    id: radModeRegex
                    text: qsTr("&Regular Expression matches")
                }
            }
        }

        // Target attribute field
        NativeGroupBox {
            title: qsTr("In Field")
            Layout.fillWidth: true
            implicitHeight: 52

            RowLayout {
                anchors.fill: parent
                spacing: 12

                NativeRadioButton {
                    id: radFieldText
                    text: qsTr("&Text")
                    checked: true
                }
                NativeRadioButton {
                    id: radFieldStyle
                    text: qsTr("&Style")
                }
                NativeRadioButton {
                    id: radFieldActor
                    text: qsTr("Act&or")
                }
                NativeRadioButton {
                    id: radFieldEffect
                    text: qsTr("E&ffect")
                }
            }
        }

        // Row type filter: dialogue or comment lines
        NativeGroupBox {
            title: qsTr("Match dialogues/comments")
            Layout.fillWidth: true
            implicitHeight: 52

            RowLayout {
                anchors.fill: parent
                spacing: 16

                NativeCheckBox {
                    id: chkDialogues
                    text: qsTr("&Dialogue")
                    checked: true
                    onCheckedChanged: {
                        if (!checked && !chkComments.checked) {
                            chkComments.checked = true;
                        }
                    }
                }
                NativeCheckBox {
                    id: chkComments
                    text: qsTr("Comme&nts")
                    checked: true
                    onCheckedChanged: {
                        if (!checked && !chkDialogues.checked) {
                            chkDialogues.checked = true;
                        }
                    }
                }
            }
        }

        // Selection set operation: replace, union, subtract, or intersect
        NativeGroupBox {
            title: qsTr("Action")
            Layout.fillWidth: true
            implicitHeight: 52

            RowLayout {
                anchors.fill: parent
                spacing: 8

                NativeRadioButton {
                    id: radActionSet
                    text: qsTr("Set &Selection"); checked: true
                }
                NativeRadioButton {
                    id: radActionAdd
                    text: qsTr("&Add to Selection")
                }
                NativeRadioButton {
                    id: radActionSub
                    text: qsTr("S&ubtract from Selection")
                }
                NativeRadioButton {
                    id: radActionIntersect
                    text: qsTr("Intersect &with Selection")
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Dialog action buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

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
                text: qsTr("Apply"); Layout.preferredWidth: 75
                onClicked: dialog.doProcess(false)
            }

            NativeButton {
                text: qsTr("Help"); Layout.preferredWidth: 75
                onClicked: Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Select_Lines/")
            }
        }
    }
}
