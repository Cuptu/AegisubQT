// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs as PlatformDialogs
import "../controls"

Item {
    id: form
    property var controls: []
    property var values: ({})
    property string errorMessage: ""
    property int columns: 1
    property int rows: 0
    property real cellWidth: Math.max(100, (width - (columns - 1) * 6) / columns)
    property alias colorDialog: colorDialog
    property var colorNormalizer: typeof automationManager !== "undefined" ? automationManager : null
    implicitHeight: rows * 38
    implicitWidth: columns * 106

    function defaultValue(c) {
        if (c.class === "checkbox") return !!c.value;
        if (c.class === "intedit" || c.class === "floatedit") return Number(c.value || 0);
        if (c.class === "edit" || c.class === "textbox" || c.class === "alpha")
            return String(c.text !== undefined ? c.text : (c.value !== undefined ? c.value : ""));
        if (c.class === "color" || c.class === "coloralpha") {
            var color = readColor(c, String(c.value !== undefined ? c.value : ""));
            return color.success ? color.value : (c.class === "coloralpha" ? "#00000000" : "#000000");
        }
        return String(c.value !== undefined ? c.value : "");
    }
    function configure(descriptors, stored, nativeControls) {
        colorDialog.close();
        colorDialog.activeField = null;
        controls = [];
        errorMessage = "";
        columns = 1;
        rows = 0;
        var next = Object.assign({}, stored || {});
        var normalized = [];
        var classes = ["label", "edit", "textbox", "intedit", "floatedit", "dropdown", "checkbox", "color", "coloralpha", "alpha"];
        if (nativeControls) classes.push("button");
        for (var i = 0; i < descriptors.length; ++i) {
            var c = Object.assign({}, descriptors[i]);
            c.nativeControl = !!nativeControls;
            if (classes.indexOf(c.class) < 0) {
                errorMessage = qsTr("Unsupported configuration control: ") + c.class;
                break;
            }
            c.name = String(c.name || "");
            c.x = c.x !== undefined ? Number(c.x) : 0;
            c.y = c.y !== undefined ? Number(c.y) : 0;
            c.width = c.width !== undefined ? Number(c.width) : 1;
            c.height = c.height !== undefined ? Number(c.height) : 1;
            if (![c.x, c.y, c.width, c.height].every(function(n) { return isFinite(n) && Math.floor(n) === n; }) ||
                c.x < 0 || c.y < 0 || c.width < 1 || c.height < 1) {
                errorMessage = qsTr("Invalid configuration control position");
                break;
            }
            columns = Math.max(columns, c.x + c.width);
            rows = Math.max(rows, c.y + c.height);
            if (c.class !== "label" && c.class !== "button" && next[c.name] === undefined) next[c.name] = defaultValue(c);
            normalized.push(c);
        }
        values = next;
        if (!errorMessage) controls = normalized;
    }
    function setValue(name, value) {
        var next = Object.assign({}, values);
        next[name] = value;
        values = next;
    }
    function writeValue(name, value) {
        for (var i = 0; i < fields.count; ++i) {
            if (controls[i].name !== name) continue;
            var field = fields.itemAt(i).item;
            if (field && field.writeValue) field.writeValue(value);
        }
        setValue(name, value);
    }
    function numericLimits(c) {
        var integer = c.class === "intedit";
        var minimum = c.min !== undefined ? Number(c.min) : (integer ? -2147483648 : -Number.MAX_VALUE);
        var maximum = c.max !== undefined ? Number(c.max) : (integer ? 2147483647 : Number.MAX_VALUE);
        if (!isFinite(minimum) || !isFinite(maximum) || minimum >= maximum) {
            minimum = integer ? -2147483648 : -Number.MAX_VALUE;
            maximum = integer ? 2147483647 : Number.MAX_VALUE;
        }
        // Upstream FloatEdit supplies 0..100 for an otherwise unbounded stepper.
        if (!integer && Number(c.step || 0) !== 0) {
            if (minimum === -Number.MAX_VALUE) minimum = 0;
            if (maximum === Number.MAX_VALUE) maximum = 100;
        }
        return {min: minimum, max: maximum};
    }
    function readNumber(c, text) {
        var number = Number(text);
        var limits = numericLimits(c);
        if (!text.trim() || !isFinite(number) || number < limits.min || number > limits.max ||
                (c.class === "intedit" && Math.floor(number) !== number))
            return {success: false, message: qsTr("Invalid number: ") + c.name};
        return {success: true, value: number};
    }
    function canStep(c) { return c.class === "intedit" || (c.class === "floatedit" && Number(c.step) > 0); }
    function stepField(field, c, direction) {
        var read = readNumber(c, field.text);
        if (!read.success) return;
        var limits = numericLimits(c);
        var step = c.class === "intedit" ? 1 : Number(c.step);
        var number = Math.max(limits.min, Math.min(limits.max, read.value + direction * step));
        if (!isFinite(number)) return;
        field.text = String(number);
        setValue(c.name, number);
    }
    function readColor(c, text) {
        if (!colorNormalizer) return {success: false, message: qsTr("Color parser is unavailable")};
        return colorNormalizer.normalizeConfigColor(String(text), c.class === "coloralpha");
    }
    function displayColor(c, text) {
        var read = readColor(c, text);
        if (!read.success) return Qt.rgba(0, 0, 0, 1);
        var hex = read.value;
        // Lua uses ASS transparency (0 opaque), Qt uses opacity (1 opaque).
        var opacity = c.class === "coloralpha" ? 1 - parseInt(hex.slice(7, 9), 16) / 255 : 1;
        return Qt.rgba(parseInt(hex.slice(1, 3), 16) / 255, parseInt(hex.slice(3, 5), 16) / 255,
                       parseInt(hex.slice(5, 7), 16) / 255, opacity);
    }
    function channelHex(value) { return Math.round(value).toString(16).padStart(2, "0").toUpperCase(); }
    function openColor(field, c) {
        colorDialog.activeField = field;
        colorDialog.withAlpha = c.class === "coloralpha";
        colorDialog.selectedColor = displayColor(c, field.text);
        colorDialog.open();
    }
    PlatformDialogs.ColorDialog {
        id: colorDialog
        parentWindow: form.Window.window
        title: qsTr("Select Color")
        property var activeField: null
        property bool withAlpha: false
        options: withAlpha ? PlatformDialogs.ColorDialog.ShowAlphaChannel : 0
        onAccepted: {
            if (activeField) {
                var color = selectedColor;
                var value = "#" + form.channelHex(color.r * 255) + form.channelHex(color.g * 255) + form.channelHex(color.b * 255);
                if (withAlpha) value += form.channelHex(255 - color.a * 255);
                activeField.text = value;
                form.setValue(activeField.controlName, value);
            }
            activeField = null;
        }
        onRejected: activeField = null
    }
    Connections {
        target: form.Window.window
        function onVisibleChanged() {
            if (!target.visible) { colorDialog.close(); colorDialog.activeField = null; }
        }
    }
    function readback() {
        if (errorMessage) return {success: false, message: errorMessage};
        var result = {};
        for (var i = 0; i < fields.count; ++i) {
            var field = fields.itemAt(i).item;
            if (!field) return {success: false, message: qsTr("Configuration is still loading")};
            var c = controls[i];
            if (c.class === "label" || c.class === "button") continue;
            var read = field.readValue();
            if (!read.success) return read;
            result[c.name] = read.value;
        }
        return {success: true, settings: result};
    }

    Repeater {
        id: fields
        model: form.controls
        delegate: Loader {
            required property var modelData
            required property int index
            property var descriptor: modelData
            x: descriptor.x * (form.cellWidth + 6)
            y: descriptor.y * 38
            width: descriptor.width * (form.cellWidth + 6) - 6
            height: descriptor.height * 38 - 6
            sourceComponent: descriptor.class === "label" ? labelControl :
                             descriptor.class === "button" ? buttonControl :
                             descriptor.class === "checkbox" ? checkControl :
                             descriptor.class === "dropdown" ? dropdownControl :
                             descriptor.class === "textbox" ? multilineControl :
                             descriptor.class === "color" || descriptor.class === "coloralpha" ? colorControl :
                             form.canStep(descriptor) ? numericControl : textControl
        }
    }
    Component {
        id: buttonControl
        NativeButton {
            objectName: "automation-config-button-" + descriptor.target
            text: descriptor.label || ""
            enabled: descriptor.enabled !== false
            onClicked: form.writeValue(descriptor.target, descriptor.value)
            function readValue() { return {success: true}; }
        }
    }
    Component {
        id: labelControl
        Label {
            text: descriptor.label || ""
            wrapMode: Text.WordWrap
            function readValue() { return {success: true}; }
        }
    }
    Component {
        id: checkControl
        NativeCheckBox {
            objectName: "automation-config-" + descriptor.name
            text: descriptor.label || ""
            checked: !!form.values[descriptor.name]
            onToggled: form.setValue(descriptor.name, checked)
            ToolTip.visible: hovered && !!descriptor.hint
            ToolTip.text: descriptor.hint || ""
            function readValue() { return {success: true, value: checked}; }
        }
    }
    Component {
        id: dropdownControl
        NativeComboBox {
            objectName: "automation-config-" + descriptor.name
            model: descriptor.items || []
            currentIndex: model.indexOf(String(form.values[descriptor.name]))
            displayText: currentIndex < 0 ? String(form.values[descriptor.name]) : currentText
            onActivated: form.setValue(descriptor.name, currentText)
            ToolTip.visible: hovered && !!descriptor.hint
            ToolTip.text: descriptor.hint || ""
            function readValue() { return {success: true, value: displayText}; }
        }
    }
    Component {
        id: multilineControl
        ScrollView {
            TextArea {
                id: multiline
                objectName: "automation-config-" + descriptor.name
                Component.onCompleted: text = String(form.values[descriptor.name])
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                onTextChanged: form.setValue(descriptor.name, text)
                ToolTip.visible: hovered && !!descriptor.hint
                ToolTip.text: descriptor.hint || ""
            }
            function readValue() { return {success: true, value: multiline.text}; }
        }
    }
    Component {
        id: numericControl
        Item {
            property var controlDescriptor: descriptor
            Loader { id: numericInput; property var descriptor: parent.controlDescriptor; anchors.fill: parent; anchors.rightMargin: 26; sourceComponent: textControl }
            Column {
                anchors.right: parent.right; width: 24; height: parent.height
                Button {
                    objectName: "automation-step-up-" + descriptor.name
                    width: parent.width; height: parent.height / 2; text: "+"
                    onClicked: form.stepField(numericInput.item, descriptor, 1)
                    Accessible.name: qsTr("Increase ") + descriptor.name
                }
                Button {
                    objectName: "automation-step-down-" + descriptor.name
                    width: parent.width; height: parent.height / 2; text: "−"
                    onClicked: form.stepField(numericInput.item, descriptor, -1)
                    Accessible.name: qsTr("Decrease ") + descriptor.name
                }
            }
            function readValue() { return numericInput.item.readValue(); }
            function writeValue(value) { numericInput.item.writeValue(value); }
        }
    }
    Component {
        id: colorControl
        Item {
            property var controlDescriptor: descriptor
            Loader { id: colorInput; property var descriptor: parent.controlDescriptor; anchors.fill: parent; anchors.rightMargin: 38; sourceComponent: textControl }
            Button {
                objectName: "automation-color-button-" + descriptor.name
                anchors.right: parent.right; width: 34; height: parent.height
                contentItem: Item {
                    Rectangle { anchors.fill: parent; color: "white" }
                    Rectangle { width: parent.width / 2; height: parent.height / 2; color: "#999999" }
                    Rectangle { x: parent.width / 2; y: parent.height / 2; width: parent.width / 2; height: parent.height / 2; color: "#999999" }
                    Rectangle { anchors.fill: parent; color: colorInput.item ? form.displayColor(descriptor, colorInput.item.text) : "black"; border.color: "#777777" }
                }
                onClicked: form.openColor(colorInput.item, descriptor)
                Accessible.name: qsTr("Choose color: ") + descriptor.name
                ToolTip.visible: hovered && !!descriptor.hint
                ToolTip.text: descriptor.hint || ""
            }
            function readValue() { return colorInput.item.readValue(); }
            function writeValue(value) { colorInput.item.writeValue(value); }
        }
    }
    Component {
        id: textControl
        TextField {
            property string controlName: descriptor.name
            objectName: "automation-config-" + descriptor.name
            Component.onCompleted: text = String(form.values[descriptor.name])
            enabled: !descriptor.nativeControl || !descriptor.nativeEnabledWhen ||
                     form.values[descriptor.nativeEnabledWhen.field] === descriptor.nativeEnabledWhen.value
            selectByMouse: true
            ToolTip.visible: hovered && !!descriptor.hint
            ToolTip.text: descriptor.hint || ""
            onTextEdited: {
                if (descriptor.class !== "intedit" && descriptor.class !== "floatedit")
                    form.setValue(descriptor.name, text);
            }
            Keys.onUpPressed: (event) => {
                event.accepted = form.canStep(descriptor);
                if (event.accepted) form.stepField(this, descriptor, 1);
            }
            Keys.onDownPressed: (event) => {
                event.accepted = form.canStep(descriptor);
                if (event.accepted) form.stepField(this, descriptor, -1);
            }
            function readValue() {
                var kind = descriptor.class;
                if (kind === "intedit" || kind === "floatedit") {
                    if (descriptor.nativeControl && !enabled)
                        return {success: true, value: Number(form.values[descriptor.name])};
                    return form.readNumber(descriptor, text);
                }
                if (kind === "color" || kind === "coloralpha") {
                    return form.readColor(descriptor, text);
                }
                return {success: true, value: text};
            }
            function writeValue(value) { text = String(value); }
        }
    }
}
