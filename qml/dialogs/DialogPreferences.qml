// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogPreferences: Modular configuration dialog managing application preferences,
// playback parameters, visual rendering styles, and automation options.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"
import "preferences"

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Preferences")
    iconSource: "../../assets/icons_native/options_button_16.png"
    width: 710
    height: 600
    implicitWidth: 710
    implicitHeight: 600

    property int currentPageIndex: 3 // Active preference category index
    property bool hasPendingChanges: false

    signal preferencesSaved()

    function markChanged() {
        hasPendingChanges = true;
    }

    function savePreferences() {
        pGeneral.savePreferences();
        pDefaultStyles.savePreferences();
        pAudio.savePreferences();
        pVideo.savePreferences();
        pInterface.savePreferences();
        pColours.savePreferences();
        pHotkeys.savePreferences();
        pBackup.savePreferences();
        pAutomation.savePreferences();
        pAdvanced.savePreferences();
        pAdvancedAudio.savePreferences();
        pAdvancedVideo.savePreferences();
        dialog.preferencesSaved();
        hasPendingChanges = false;
    }

    function restoreDefaults() {
        pGeneral.restoreDefaults();
        pDefaultStyles.restoreDefaults();
        pAudio.restoreDefaults();
        pVideo.restoreDefaults();
        pInterface.restoreDefaults();
        pColours.restoreDefaults();
        pHotkeys.restoreDefaults();
        pBackup.restoreDefaults();
        pAutomation.restoreDefaults();
        pAdvanced.restoreDefaults();
        pAdvancedAudio.restoreDefaults();
        pAdvancedVideo.restoreDefaults();
        hasPendingChanges = true;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        // Main layout: navigation tree and settings panel stack
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // Category navigation tree
            NativeTreeBook {
                id: navTree
                Layout.preferredWidth: 135
                Layout.fillHeight: true
                currentIndex: dialog.currentPageIndex
                onItemSelected: (idx) => dialog.currentPageIndex = idx
            }

            // Category settings panel container
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                // Page 0: General
                PrefGeneral {
                    id: pGeneral
                    anchors.fill: parent
                    visible: dialog.currentPageIndex === 0
                    onChanged: dialog.markChanged()
                }

                // Page 1: Default Styles
                PrefDefaultStyles {
                    id: pDefaultStyles
                    anchors.fill: parent
                    visible: dialog.currentPageIndex === 1
                    onChanged: dialog.markChanged()
                }

                // Page 2: Audio
                PrefAudio {
                    id: pAudio
                    anchors.fill: parent
                    visible: dialog.currentPageIndex === 2
                    onChanged: dialog.markChanged()
                }

                // Page 3: Video
                PrefVideo {
                    id: pVideo
                    anchors.fill: parent
                    visible: dialog.currentPageIndex === 3
                    onChanged: dialog.markChanged()
                }

                // Page 4: Interface
                PrefInterface {
                    id: pInterface
                    anchors.fill: parent
                    visible: dialog.currentPageIndex === 4
                    onChanged: dialog.markChanged()
                }

                // Page 5: Colours
                PrefColours {
                    id: pColours
                    anchors.fill: parent
                    visible: dialog.currentPageIndex === 5
                    onChanged: dialog.markChanged()
                }

                // Page 6: Hotkeys
                PrefHotkeys {
                    id: pHotkeys
                    anchors.fill: parent
                    visible: dialog.currentPageIndex === 6
                    onChanged: dialog.markChanged()
                }

                // Page 7: Backup
                PrefBackup {
                    id: pBackup
                    anchors.fill: parent
                    visible: dialog.currentPageIndex === 7
                    onChanged: dialog.markChanged()
                }

                // Page 8: Automation
                PrefAutomation {
                    id: pAutomation
                    anchors.fill: parent
                    visible: dialog.currentPageIndex === 8
                    onChanged: dialog.markChanged()
                }

                // Page 9: Advanced
                PrefAdvanced {
                    id: pAdvanced
                    anchors.fill: parent
                    visible: dialog.currentPageIndex === 9
                    onChanged: dialog.markChanged()
                }

                // Page 10: Advanced Audio
                PrefAdvancedAudio {
                    id: pAdvancedAudio
                    anchors.fill: parent
                    visible: dialog.currentPageIndex === 10
                    onChanged: dialog.markChanged()
                }

                // Page 11: Advanced Video
                PrefAdvancedVideo {
                    id: pAdvancedVideo
                    anchors.fill: parent
                    visible: dialog.currentPageIndex === 11
                    onChanged: dialog.markChanged()
                }
            }
        }

        // Dialog action controls
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            NativeButton {
                text: qsTr("Reset to Defaults")
                Layout.preferredWidth: 95
                onClicked: dialog.restoreDefaults()
            }

            Item { Layout.fillWidth: true }

            NativeButton {
                text: qsTr("OK")
                isDefault: true
                Layout.preferredWidth: 75
                onClicked: {
                    dialog.savePreferences();
                    dialog.close();
                }
            }

            NativeButton {
                text: qsTr("Cancel")
                Layout.preferredWidth: 75
                onClicked: dialog.close()
            }

            NativeButton {
                id: btnApply
                text: qsTr("Apply")
                enabled: dialog.hasPendingChanges
                Layout.preferredWidth: 75
                onClicked: {
                    dialog.savePreferences();
                    dialog.hasPendingChanges = false;
                }
            }

            NativeButton {
                text: qsTr("Help")
                Layout.preferredWidth: 75
                onClicked: Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Options/")
            }
        }
    }
}
