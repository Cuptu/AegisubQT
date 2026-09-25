// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../controls"

// Preferences page: General application behavior and recent history limits.
Flickable {
    id: root
    contentHeight: mainLayout.implicitHeight
    boundsBehavior: Flickable.StopAtBounds

    signal changed()

    function restoreDefaults() {
        chkCheckUpdates.checked = true;
        chkShowToolbar.checked = true;
        chkSaveUIState.checked = true;
        cmbToolbarSize.currentIndex = 1;
        cmbAutoloadLinked.currentIndex = 2;
        spinUndoLevels.value = 128;
        spinMRULimit.value = 16;
        spinFindReplaceLimit.value = 16;
        root.changed();
    }

    function savePreferences() {
        // Persist general settings
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
                    id: chkCheckUpdates
                    text: qsTr("Automatically check for new versions on start")
                    checked: true
                    onCheckedChanged: root.changed()
                }

                NativeCheckBox {
                    id: chkShowToolbar
                    text: qsTr("Show main toolbar")
                    checked: true
                    onCheckedChanged: root.changed()
                }

                NativeCheckBox {
                    id: chkSaveUIState
                    text: qsTr("Save UI state")
                    checked: true
                    onCheckedChanged: root.changed()
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Language:")
                        font.pixelSize: 12
                        font.family: uiTheme.uiFont
                        Layout.fillWidth: true
                    }
                    NativeComboBox {
                        id: cmbLanguage
                        Layout.preferredWidth: 160
                        model: [
                            "简体中文 (Simplified Chinese)",
                            "繁體中文 (Traditional Chinese)",
                            "English",
                            "日本語 (Japanese)",
                            "한국어 (Korean)",
                            "Français (French)",
                            "Deutsch (German)",
                            "Русский (Russian)",
                            "Español (Spanish)"
                        ]
                        property var langCodes: ["zh_CN", "zh_TW", "en_US", "ja", "ko", "fr_FR", "de", "ru", "es"]
                        Component.onCompleted: syncLang()
                        function syncLang() {
                            if (typeof languageManager !== "undefined" && languageManager) {
                                var cur = languageManager.currentLanguage;
                                for (var i = 0; i < langCodes.length; ++i) {
                                    if (langCodes[i] === cur) {
                                        currentIndex = i;
                                        return;
                                    }
                                }
                            }
                        }
                        Connections {
                            target: typeof languageManager !== "undefined" ? languageManager : null
                            function onLanguageChanged(code) { cmbLanguage.syncLang(); }
                        }
                        onActivated: (idx) => {
                            if (idx >= 0 && idx < langCodes.length && typeof languageManager !== "undefined" && languageManager) {
                                var code = langCodes[idx];
                                if (code !== languageManager.currentLanguage) {
                                    languageManager.setLanguage(code);
                                    root.changed();
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Toolbar Icon Size") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.fillWidth: true }
                    NativeComboBox {
                        id: cmbToolbarSize
                        Layout.preferredWidth: 160
                        model: [qsTr("Standard (16x16)"), qsTr("Standard (24x24)"), qsTr("Large (32x32)"), qsTr("Extra Large (48x48)")]
                        currentIndex: 1
                        onCurrentIndexChanged: root.changed()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Automatically load linked files") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.fillWidth: true }
                    NativeComboBox {
                        id: cmbAutoloadLinked
                        Layout.preferredWidth: 160
                        model: [qsTr("Never"), qsTr("Always"), qsTr("Ask")]
                        currentIndex: 2
                        onCurrentIndexChanged: root.changed()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Undo Levels") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.fillWidth: true }
                    NativeSpinBox {
                        id: spinUndoLevels
                        Layout.preferredWidth: 80
                        from: 2; to: 10000; value: 128
                        onValueModified: root.changed()
                    }
                }
            }
        }

        NativeGroupBox {
            title: qsTr("Recently Used Lists")
            Layout.fillWidth: true
            implicitHeight: 85

            ColumnLayout {
                anchors.fill: parent
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Files") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.fillWidth: true }
                    NativeSpinBox {
                        id: spinMRULimit
                        Layout.preferredWidth: 80
                        from: 0; to: 16; value: 16
                        onValueModified: root.changed()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Find/Replace") + ":"; font.pixelSize: 12; font.family: uiTheme.uiFont; Layout.fillWidth: true }
                    NativeSpinBox {
                        id: spinFindReplaceLimit
                        Layout.preferredWidth: 80
                        from: 0; to: 50; value: 16
                        onValueModified: root.changed()
                    }
                }
            }
        }
    }
}
