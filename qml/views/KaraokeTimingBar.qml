import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: bar
    objectName: "karaoke-timing-bar"
    property var subtitleModel: null
    property var audioCtrl: null
    property int currentIndex: -1
    property bool active: false
    property var snapshot: null
    property var syllables: []
    property int selectedSyllable: 0
    property bool applying: false
    property string amendCommitId: ""
    signal statusMessage(string message)
    implicitHeight: active ? 42 : 0
    visible: active
    color: "#f7f9fa"
    border.color: "#d0d0d0"

    function reload(preserveSelection) {
        amendCommitId = ""
        var previous = preserveSelection ? selectedSyllable : 0
        snapshot = null
        syllables = []
        selectedSyllable = 0
        updatePrimaryRange()
        if (!active || !subtitleModel) return
        var result = subtitleModel.karaokeTiming(currentIndex)
        if (!result.success) return
        snapshot = result
        var copy = []
        for (var i = 0; i < result.syllables.length; ++i) {
            var s = result.syllables[i]
            copy.push({text: s.text, duration: s.duration})
        }
        syllables = copy
        selectedSyllable = Math.min(previous, Math.max(0, syllables.length - 1))
        updatePrimaryRange()
        Qt.callLater(updatePrimaryRange)
    }
    function updatePrimaryRange() {
        if (!audioCtrl || typeof audioCtrl.setKaraokePlaybackRange !== "function") return
        if (!active || !snapshot || !syllables.length) {
            audioCtrl.setKaraokePlaybackRange(-1, -1)
            return
        }
        var start = snapshot.startMs
        for (var i = 0; i < selectedSyllable; ++i) start += syllables[i].duration
        audioCtrl.setKaraokePlaybackRange(start, start + syllables[selectedSyllable].duration)
    }
    function playSelected() {
        updatePrimaryRange()
        if (audioCtrl && typeof audioCtrl.playSelection === "function") audioCtrl.playSelection()
    }
    function navigate(direction) {
        var next = selectedSyllable + direction
        if (!active || next < 0 || next >= syllables.length) return false
        selectedSyllable = next
        playSelected()
        return true
    }
    onSelectedSyllableChanged: updatePrimaryRange()
    onAudioCtrlChanged: updatePrimaryRange()
    function adjustDuration(index, duration) {
        if (syllables.length < 2 || index < 0 || index >= syllables.length) return
        if (!Number.isFinite(duration)) return
        var neighbor = index + 1 < syllables.length ? index + 1 : index - 1
        var total = syllables[index].duration + syllables[neighbor].duration
        var value = Math.max(0, Math.min(total, Math.round(duration)))
        var copy = syllables.slice()
        copy[index] = {text: copy[index].text, duration: value}
        copy[neighbor] = {text: copy[neighbor].text, duration: total - value}
        syllables = copy
        updatePrimaryRange()
        if (audioCtrl && audioCtrl.autoCommit) commitTiming(true)
    }
    function apply() { return commitTiming(false) }
    function commitTiming(automatic) {
        if (!snapshot || !subtitleModel) return false
        var durations = syllables.map(function(s) { return s.duration })
        applying = true
        var result = subtitleModel.applyKaraokeTiming(currentIndex, snapshot, durations, automatic ? amendCommitId : "")
        applying = false
        if (!result.success) {
            statusMessage(result.message)
            reload()
            return false
        }
        snapshot = subtitleModel.karaokeTiming(currentIndex)
        amendCommitId = automatic ? result.commitId : ""
        updatePrimaryRange()
        if (result.changed) statusMessage(qsTr("Applied karaoke timing"))
        return true
    }
    onActiveChanged: reload()
    onCurrentIndexChanged: reload()
    onSubtitleModelChanged: reload()
    Component.onCompleted: reload()
    Connections {
        target: bar.subtitleModel
        function onContentModified() { if (!bar.applying) bar.reload(true) }
        function onCountChanged() { bar.reload() }
    }
    Connections {
        target: bar.audioCtrl
        ignoreUnknownSignals: true
        function onCommitted() {
            if (bar.active && !bar.apply() && typeof bar.audioCtrl.rejectCommit === "function")
                bar.audioCtrl.rejectCommit()
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 6
        Label { text: qsTr("Karaoke syllables:") }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            Row {
                spacing: 4
                Repeater {
                    model: bar.syllables.length
                    delegate: Rectangle {
                        required property int index
                        property var modelData: bar.syllables[index]
                        objectName: "karaoke-syllable-" + index
                        width: Math.max(46, label.implicitWidth + 12)
                        height: 30
                        color: bar.selectedSyllable === index ? "#cce8ff" : "white"
                        border.color: "#0078d4"
                        Label {
                            id: label
                            anchors.centerIn: parent
                            text: modelData.text + "\n" + (modelData.duration / 10) + " cs"
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: 11
                        }
                        MouseArea {
                            anchors.fill: parent
                            property real initialX: 0
                            property int initialDuration: 0
                            hoverEnabled: true
                            cursorShape: Qt.SizeHorCursor
                            ToolTip.visible: containsMouse
                            ToolTip.text: qsTr("Drag to adjust the syllable boundary")
                            onPressed: function(mouse) {
                                bar.amendCommitId = ""
                                bar.selectedSyllable = index
                                initialX = mapToItem(bar, mouse.x, mouse.y).x
                                initialDuration = modelData.duration
                            }
                            onPositionChanged: function(mouse) {
                                if (pressed) bar.adjustDuration(index, initialDuration +
                                    Math.round(mapToItem(bar, mouse.x, mouse.y).x - initialX) * 10)
                            }
                        }
                    }
                }
            }
        }
        SpinBox {
            objectName: "karaoke-duration"
            enabled: bar.syllables.length > 1
            from: 0
            to: {
                var index = bar.selectedSyllable
                if (!enabled) return 0
                var next = index + 1 < bar.syllables.length ? index + 1 : index - 1
                return bar.syllables[index].duration + bar.syllables[next].duration
            }
            value: bar.syllables.length ? bar.syllables[bar.selectedSyllable].duration : 0
            stepSize: 10
            editable: true
            textFromValue: function(value) { return (value / 10) + " cs" }
            valueFromText: function(text) { return Math.round(parseFloat(text) * 10) }
            onValueModified: bar.adjustDuration(bar.selectedSyllable, value)
            Layout.preferredWidth: 110
            Layout.fillHeight: true
        }
        Button {
            objectName: "karaoke-apply"
            text: qsTr("Apply")
            enabled: bar.snapshot !== null
            Layout.fillHeight: true
            onClicked: bar.apply()
        }
    }
}
