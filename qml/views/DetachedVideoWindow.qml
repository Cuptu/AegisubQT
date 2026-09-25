// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// DetachedVideoWindow: independent floating video playback window
// (upstream "Detach Video" / DialogDetachedVideo). Hosts its own MediaPlayer
// mirroring the shared VideoController timeline: while this window is open the
// main video box suspends its decoder so exactly one pipeline plays.
import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtMultimedia

Window {
    id: detachedWindow
    title: qsTr("AegisubQT - Detached Video")
    width: 640
    height: 360
    minimumWidth: 240
    minimumHeight: 160
    color: "#000000"

    // Local mirror player: decodes the active video and reports its timeline
    // position back into VideoController (same protocol as the main VideoBox).
    MediaPlayer {
        id: detachedPlayer
        source: {
            if (typeof videoController === "undefined" || !videoController || !videoController.hasVideo || videoController.isDummy) return "";
            var p = videoController.videoPath;
            if (!p) return "";
            if (p.startsWith("file:///")) return p;
            if (p.startsWith("file://")) return p;
            if (p.length >= 2 && p.charAt(1) === ':') {
                return "file:///" + p.replace(/\\/g, "/");
            }
            return "file://" + p.replace(/\\/g, "/");
        }
        videoOutput: detachedOutput
        audioOutput: AudioOutput {
            muted: (typeof audioController !== "undefined" && audioController && audioController.hasAudio && audioController.isPlaying)
        }

        onPositionChanged: {
            if (detachedPlayer.playbackState === MediaPlayer.PlayingState) {
                if (typeof audioController === "undefined" || !audioController || !audioController.hasAudio || !audioController.isPlaying) {
                    var sec = detachedPlayer.position / 1000.0;
                    if (Math.abs(videoController.currentTime - sec) > 0.02) {
                        videoController.seekTime(sec);
                    }
                }
            }
        }
    }

    VideoOutput {
        id: detachedOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
    }

    Connections {
        target: typeof videoController !== "undefined" ? videoController : null
        property real lastResyncMs: 0

        function onIsPlayingChanged() {
            if (!detachedPlayer.hasVideo) return;
            if (videoController.isPlaying) {
                detachedPlayer.play();
            } else {
                detachedPlayer.pause();
            }
        }

        function onPositionChanged() {
            if (!detachedPlayer.hasVideo) return;
            var targetMs = Math.round(videoController.currentTime * 1000.0);
            if (detachedPlayer.playbackState !== MediaPlayer.PlayingState) {
                // Paused or stopped: follow the timeline exactly (scrubbing mirror).
                if (Math.abs(detachedPlayer.position - targetMs) > 15) {
                    detachedPlayer.position = targetMs;
                }
            } else {
                // Playing: resync only on significant drift (audio stays the clock master).
                var now = Date.now();
                if (Math.abs(detachedPlayer.position - targetMs) > 500 && (now - lastResyncMs > 600)) {
                    lastResyncMs = now;
                    detachedPlayer.position = targetMs;
                }
            }
        }
    }

    onVisibleChanged: {
        if (visible && detachedPlayer.hasVideo) {
            detachedPlayer.position = Math.round(videoController.currentTime * 1000.0);
            if (videoController.isPlaying) {
                detachedPlayer.play();
            }
        }
    }

    onClosing: (close) => {
        // Re-attach to the main window instead of destroying the shared state.
        detachedPlayer.pause();
        detachedWindow.visible = false;
        if (typeof root !== "undefined" && root) {
            root.videoDetached = false;
        }
        close.accepted = false;
    }
}
