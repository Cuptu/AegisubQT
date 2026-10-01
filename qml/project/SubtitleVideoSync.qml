// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT
import QtQuick

// Selection changes seek; document edits only refresh subtitle state.
Item {
    id: sync
    required property var project
    property var videoCtrl: null
    property var audioCtrl: null

    function refresh(activeChanged) {
        var model = project.subtitleModel;
        var row = project.currentSelectedIndex;
        if (!videoCtrl || !model || row < 0 || row >= model.count) return;
        var item = model.get(row);
        videoCtrl.setActiveSubtitle(model.getLineStartMs(row), model.getLineEndMs(row), item.text);
        if (activeChanged && videoCtrl.hasVideo && videoCtrl.autoScroll) {
            videoCtrl.pause();
            videoCtrl.seekTime(model.getLineStartMs(row) / 1000.0);
        }
    }

    function jumpToLine(row) {
        var model = project.subtitleModel;
        if (!model || row < 0 || row >= model.count) return;
        if (audioCtrl)
            audioCtrl.scrollRangeInView(model.getLineStartMs(row), model.getLineEndMs(row));
        if (videoCtrl && videoCtrl.hasVideo) {
            var wasPlaying = videoCtrl.isPlaying;
            if (wasPlaying) videoCtrl.pause();
            videoCtrl.seekTime(model.getLineStartMs(row) / 1000.0);
            if (wasPlaying) videoCtrl.play();
        }
    }

    Connections {
        target: sync.project
        function onCurrentSelectedIndexChanged() { sync.refresh(true); }
        function onLineSelected() { sync.refresh(false); }
        function onDataModified() { sync.refresh(false); }
    }
    Connections {
        target: sync.project.subtitleModel
        function onDataChanged() { sync.refresh(false); }
        function onModelReset() { sync.refresh(false); }
    }
    Component.onCompleted: {
        if (videoCtrl && typeof aegisubCore !== "undefined" && aegisubCore)
            videoCtrl.setAutoScroll(aegisubCore.getSetting("Video/Subtitle Sync", true));
        refresh(false);
    }
}
