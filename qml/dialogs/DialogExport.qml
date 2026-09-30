// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DialogExport: Subtitle export dialog allowing pipeline filter configuration
// (framerate transforms, style fixes, metadata cleanup) and output text encoding selection.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as PlatformDialogs
import "../controls"

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("Export")
    iconSource: "../../assets/icons_native/export_menu_16.png"
    implicitWidth: 580
    implicitHeight: 620

    required property var project
    property var filterManager: typeof automationManager !== "undefined" ? automationManager : null
    property string errorMessage: ""
    property var pendingPipeline: []
    property string pendingCharset: "UTF-8"

    // Emitted to broadcast informational status notifications
    signal statusMessage(string msg)

    property var filtersModel: []

    property int selectedFilterIndex: 0
    property alias configForm: configForm
    property alias outputDialog: outputDialog
    property alias charsetChoice: cmbCharset

    function storeCurrentSettings() {
        var entry = filtersModel[selectedFilterIndex];
        if (!entry || entry.configError) return;
        var result = configForm.readback();
        entry.inputError = result.success ? "" : result.message;
        if (result.success) entry.settings = result.settings;
    }
    function showCurrentConfig() {
        var entry = filtersModel[selectedFilterIndex];
        configForm.configure(entry ? entry.controls : [], entry ? entry.settings : {}, entry ? entry.isBuiltin : false);
    }
    function selectFilter(index) {
        storeCurrentSettings();
        selectedFilterIndex = index;
        showCurrentConfig();
    }
    function refreshFilters() {
        storeCurrentSettings();
        var old = {};
        for (var i = 0; i < filtersModel.length; ++i) old[filtersModel[i].id] = filtersModel[i];
        var registered = filterManager ? filterManager.filters : [];
        var next = [];
        for (var j = 0; j < registered.length; ++j) {
            var item = Object.assign({}, registered[j]);
            var previous = old[item.id];
            item.checked = previous ? previous.checked : false;
            item.settings = previous ? previous.settings : {};
            item.controls = [];
            item.configError = "";
            if (item.hasConfig) {
                var result = filterManager.exportFilterConfig(item.id, project, item.settings);
                if (result.success) item.controls = result.controls;
                else item.configError = result.message;
            }
            configForm.configure(item.controls, item.settings, item.isBuiltin);
            item.configError = item.configError || configForm.errorMessage;
            var defaults = configForm.readback();
            item.inputError = defaults.success ? "" : defaults.message;
            item.settings = defaults.success ? defaults.settings : configForm.values;
            next.push(item);
        }
        // Keep the user's order for surviving filters; append newly registered ones.
        next.sort(function(a, b) {
            var ai = filtersModel.findIndex(function(f) { return f.id === a.id; });
            var bi = filtersModel.findIndex(function(f) { return f.id === b.id; });
            if (ai < 0) ai = filtersModel.length + registered.findIndex(function(f) { return f.id === a.id; });
            if (bi < 0) bi = filtersModel.length + registered.findIndex(function(f) { return f.id === b.id; });
            return ai - bi;
        });
        filtersModel = next;
        selectedFilterIndex = Math.max(0, Math.min(selectedFilterIndex, next.length - 1));
        showCurrentConfig();
        errorMessage = "";
    }
    onAboutToShow: refreshFilters()
    Connections {
        target: dialog.filterManager
        function onFiltersChanged() { if (dialog.visible) dialog.refreshFilters(); }
    }

    function setAll(chk) {
        var items = dialog.filtersModel;
        for (var i = 0; i < items.length; ++i) items[i].checked = chk;
        dialog.filtersModel = [].concat(items);
    }

    function doExport() {
        storeCurrentSettings();
        var chosen = [];
        for (var i = 0; i < filtersModel.length; ++i) {
            if (!filtersModel[i].checked) continue;
            var failure = filtersModel[i].configError || filtersModel[i].inputError;
            if (i === selectedFilterIndex) failure = failure || configForm.errorMessage;
            if (failure) { errorMessage = failure; return; }
            chosen.push({id: filtersModel[i].id, settings: filtersModel[i].settings});
        }
        pendingPipeline = chosen;
        pendingCharset = cmbCharset.currentText;
        errorMessage = "";
        outputDialog.open();
    }
    function completeExport(path) {
        var result = project.exportFiltered(path, pendingPipeline, pendingCharset);
        if (result.success) dialog.close();
        else errorMessage = result.message;
        pendingPipeline = [];
        return result;
    }
    PlatformDialogs.FileDialog {
        id: outputDialog
        title: qsTr("Export Subtitles")
        parentWindow: dialog
        fileMode: PlatformDialogs.FileDialog.SaveFile
        defaultSuffix: selectedNameFilter.index === 1 ? "srt" : "ass"
        nameFilters: ["Advanced SubStation Alpha (*.ass)", "SubRip (*.srt)"]
        onAccepted: dialog.completeExport(selectedFile.toString())
        onRejected: dialog.pendingPipeline = []
    }
    onClosed: outputDialog.close()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Export filter pipeline
        NativeGroupBox {
            title: qsTr("Filters")
            Layout.fillWidth: true
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
                        id: lvFilters
                        anchors.fill: parent
                        anchors.margins: 2
                        clip: true
                        model: dialog.filtersModel

                        delegate: Rectangle {
                            width: lvFilters.width
                            height: 22
                            color: dialog.selectedFilterIndex === index ? "#e5f1fb" : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 4
                                anchors.rightMargin: 4
                                spacing: 4

                                NativeCheckBox {
                                    text: modelData.name
                                    checked: modelData.checked
                                    onToggled: {
                                        var items = dialog.filtersModel;
                                        items[index].checked = checked;
                                        dialog.filtersModel = items;
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                propagateComposedEvents: true
                                onClicked: (mouse) => {
                                    dialog.selectFilter(index);
                                    mouse.accepted = false;
                                }
                            }
                        }
                    }
                }

                // Filter selection and reordering actions
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    NativeButton {
                        text: qsTr("Move &Up")
                        Layout.fillWidth: true
                        onClicked: {
                            if (dialog.selectedFilterIndex > 0) {
                                dialog.storeCurrentSettings();
                                var arr = dialog.filtersModel.slice();
                                var tmp = arr[dialog.selectedFilterIndex];
                                arr[dialog.selectedFilterIndex] = arr[dialog.selectedFilterIndex - 1];
                                arr[dialog.selectedFilterIndex - 1] = tmp;
                                dialog.selectedFilterIndex--;
                                dialog.filtersModel = arr;
                                dialog.showCurrentConfig();
                            }
                        }
                    }
                    NativeButton {
                        text: qsTr("Move &Down")
                        Layout.fillWidth: true
                        onClicked: {
                            if (dialog.selectedFilterIndex < dialog.filtersModel.length - 1) {
                                dialog.storeCurrentSettings();
                                var arr = dialog.filtersModel.slice();
                                var tmp = arr[dialog.selectedFilterIndex];
                                arr[dialog.selectedFilterIndex] = arr[dialog.selectedFilterIndex + 1];
                                arr[dialog.selectedFilterIndex + 1] = tmp;
                                dialog.selectedFilterIndex++;
                                dialog.filtersModel = arr;
                                dialog.showCurrentConfig();
                            }
                        }
                    }
                    NativeButton { text: qsTr("Select &All"); Layout.fillWidth: true; onClicked: dialog.setAll(true) }
                    NativeButton { text: qsTr("Select &None"); Layout.fillWidth: true; onClicked: dialog.setAll(false) }
                }

                // Active filter description preview
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 48
                    color: "#f8f9fa"
                    border.color: "#d0d0d0"
                    border.width: 1

                    Text {
                        anchors.fill: parent
                        anchors.margins: 4
                        text: dialog.filtersModel[dialog.selectedFilterIndex] ? dialog.filtersModel[dialog.selectedFilterIndex].description : ""
                        font.pixelSize: 11
                        font.family: uiTheme.uiFont
                        color: "#444444"
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        ScrollView {
            id: configScroll
            objectName: "automation-config-scroll"
            implicitWidth: 300
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(180, Math.max(32, configForm.implicitHeight))
            clip: true
            contentWidth: Math.max(availableWidth, configForm.implicitWidth)
            contentHeight: configForm.implicitHeight
            ScrollBar.vertical: ScrollBar {
                id: configBar
                policy: configForm.implicitHeight > configScroll.availableHeight ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            }
            AutomationConfigForm {
                id: configForm
                width: Math.max(configScroll.availableWidth - (configForm.implicitHeight > configScroll.availableHeight ? configBar.width + 4 : 0), implicitWidth)
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#b00020"
            text: dialog.errorMessage || (dialog.filtersModel[dialog.selectedFilterIndex] ? dialog.filtersModel[dialog.selectedFilterIndex].configError : "") || configForm.errorMessage
            visible: text.length > 0
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("No export filters are registered. Export will keep the current subtitle content.")
            wrapMode: Text.WordWrap
            visible: dialog.filtersModel.length === 0
        }

        // Text encoding / charset selection
        NativeGroupBox {
            title: qsTr("Text encoding:")
            Layout.fillWidth: true
            implicitHeight: 52

            RowLayout {
                anchors.fill: parent
                spacing: 6

                NativeComboBox {
                    id: cmbCharset
                    Layout.fillWidth: true
                    model: ["UTF-8", "UTF-16LE", "UTF-16BE", "GB2312", "GBK", "Big5", "Shift_JIS", "EUC-KR", "ISO-8859-1"]
                    currentIndex: 0
                }
            }
        }

        // Dialog action buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Item { Layout.fillWidth: true }

            NativeButton {
                text: qsTr("&Export...")
                isDefault: true
                Layout.preferredWidth: 80
                onClicked: dialog.doExport()
            }

            NativeButton {
                text: qsTr("Cancel"); Layout.preferredWidth: 75
                onClicked: dialog.close()
            }

            NativeButton {
                text: qsTr("Help"); Layout.preferredWidth: 75
                onClicked: Qt.openUrlExternally("http://www.aegisub.org/docs/3.2/Exporting/")
            }
        }
    }
}
