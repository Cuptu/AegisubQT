// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Video display box with visual typesetting tools, preview surface, scrubber slider, and transport controls.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import Aegisub 1.0
import "../controls"

Item {
    id: videoBox
    implicitWidth: 414
    implicitHeight: 282

    property color winBg: "#f0f0f0"
    property color winBorder: "#bebebe"
    property color winSunkenBorder: "#ababab" // Standard 1px neutral gray sunken border

    property int currentTool: typeof videoDisplayController !== "undefined" ? videoDisplayController.currentTool : 0

    signal zoomApplied(string zoomStr)

    // Video pane layout hierarchy:
    // 1. Top row (visual toolbar + video display viewport)
    // 2. Horizontal divider
    // 3. Frame scrubber slider
    // 4. Video transport bar (playback controls, timecodes, zoom)
    ColumnLayout {
        anchors.fill: parent
        spacing: 2

        // ==========================================
        // Top row: visual toolbar (left) + video display viewport (right)
        // ==========================================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Left vertical visual typesetting toolbar (22px wide)
            Rectangle {
                id: visualToolBar
                Layout.fillHeight: true
                Layout.preferredWidth: 22
                Layout.minimumWidth: 22
                Layout.maximumWidth: 22
                color: videoBox.winBg

                Column {
                    anchors.top: parent.top
                    anchors.topMargin: 2
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 1

                    // Visual tool button item (20x20 px)
                    component ToolBtn: ToolButton {
                        id: tBtn
                        property string iconSrc: ""
                        property string tipText: ""
                        property int toolIdx: 0
                        property bool isHelp: false

                        width: 20
                        height: 20
                        anchors.horizontalCenter: parent.horizontalCenter
                        padding: 0

                        background: Rectangle {
                            anchors.fill: parent
                            radius: 0
                            color: videoBox.currentTool === tBtn.toolIdx && !tBtn.isHelp ?
                                   "#cce8ff" : (tBtn.pressed ? "#c4e0fc" : (tBtn.hovered ? "#0000000f" : "transparent"))
                            border.color: videoBox.currentTool === tBtn.toolIdx && !tBtn.isHelp ?
                                          "#99d1ff" : "transparent"
                            border.width: 1
                        }

                        contentItem: Image {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            source: tBtn.iconSrc
                            fillMode: Image.Pad
                            smooth: false
                        }

                        onClicked: {
                            if (!isHelp) {
                                videoBox.currentTool = toolIdx
                                if (typeof videoDisplayController !== "undefined") {
                                    videoDisplayController.setCurrentTool(toolIdx);
                                }
                            }
                        }

                        ToolTip.visible: hovered
                        ToolTip.text: tipText
                        ToolTip.delay: 700
                    }

                    ToolBtn {
                        iconSrc: "../../assets/icons_native/visual_standard_16.png"
                        tipText: qsTr("Standard mode, double click sets position")
                        toolIdx: 0
                    }
                    ToolBtn {
                        iconSrc: "../../assets/icons_native/visual_move_16.png"
                        tipText: qsTr("Drag subtitles")
                        toolIdx: 1
                    }
                    ToolBtn {
                        iconSrc: "../../assets/icons_native/visual_rotatez_16.png"
                        tipText: qsTr("Rotate subtitles on their Z axis")
                        toolIdx: 2
                    }
                    ToolBtn {
                        iconSrc: "../../assets/icons_native/visual_rotatexy_16.png"
                        tipText: qsTr("Rotate subtitles on their X and Y axes")
                        toolIdx: 3
                    }
                    ToolBtn {
                        iconSrc: "../../assets/icons_native/visual_scale_16.png"
                        tipText: qsTr("Scale subtitles on X and Y axes")
                        toolIdx: 4
                    }
                    ToolBtn {
                        iconSrc: "../../assets/icons_native/visual_clip_16.png"
                        tipText: qsTr("Clip subtitles to a rectangle")
                        toolIdx: 5
                    }
                    ToolBtn {
                        iconSrc: "../../assets/icons_native/visual_vector_clip_16.png"
                        tipText: qsTr("Clip subtitles to a vectorial area")
                        toolIdx: 6
                    }

                    // Divider separator (1px shadow + 1px highlight)
                    Item {
                        width: 16
                        height: 4
                        anchors.horizontalCenter: parent.horizontalCenter
                        Rectangle {
                            anchors.top: parent.top
                            anchors.topMargin: 1
                            width: parent.width
                            height: 1
                            color: "#b0b0b0"
                        }
                        Rectangle {
                            anchors.top: parent.top
                            anchors.topMargin: 2
                            width: parent.width
                            height: 1
                            color: "#ffffff"
                        }
                    }

                    ToolBtn {
                        iconSrc: "../../assets/icons_native/visual_help_16.png"
                        tipText: qsTr("Open the manual page for Visual Typesetting")
                        toolIdx: 7
                        isHelp: true
                    }
                }
            }

            // Video display viewport
            Rectangle {
                id: videoDisplayArea
                Layout.fillWidth: true
                Layout.fillHeight: true
                // 原版视频显示窗口的 OpenGL 清屏色是透明黑（video_out_gl.cpp glClearColor(0,0,0,0)），
                // 视频框以外的留白露出的是面板底色，不是黑边。
                color: videoBox.winBg
                clip: true

                // Sunken viewport border
                Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: "#707070"; z: 50 }
                Rectangle { anchors.left: parent.left; height: parent.height; width: 1; color: "#707070"; z: 50 }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#ffffff"; z: 50 }
                Rectangle { anchors.right: parent.right; height: parent.height; width: 1; color: "#ffffff"; z: 50 }

                // Video rendering surface with pan and cursor-centered zoom
                Rectangle {
                    id: videoScreen
                    x: typeof videoDisplayController !== "undefined" ? Math.round(videoDisplayController.videoLeft) : 0
                    y: typeof videoDisplayController !== "undefined" ? Math.round(videoDisplayController.videoTop) : 0
                    width: typeof videoDisplayController !== "undefined" ? Math.round(videoDisplayController.videoWidth) : parent.width
                    height: typeof videoDisplayController !== "undefined" ? Math.round(videoDisplayController.videoHeight) : parent.height
                    color: (videoController && videoController.isDummy) ? videoController.dummyColor : "#000000"
                    visible: videoController && videoController.hasVideo
                    clip: true

                    // 直接以场景图纹理渲染当前帧：帧到达由渲染线程上传纹理，
                    // 不再经过 QQuickImageProvider + Image 的每帧 URL 变更同步加载。
                    VideoSurface {
                        anchors.fill: parent
                        controller: typeof videoController !== "undefined" ? videoController : null
                        // 与原 Image 的条件保持一致：MediaPlayer 未接管画面时由纹理直连渲染
                        visible: !videoController.isDummy && videoController.hasVideo
                                 && (!mediaVideoPlayer.hasVideo || mediaVideoPlayer.playbackState === MediaPlayer.StoppedState)
                    }

                    MediaPlayer {
                        id: mediaVideoPlayer
                        source: {
                            if (!videoController || !videoController.hasVideo || videoController.isDummy) return "";
                            var p = videoController.videoPath;
                            if (!p) return "";
                            if (p.startsWith("file:///")) return p;
                            if (p.startsWith("file://")) return p;
                            if (p.length >= 2 && p.charAt(1) === ':') {
                                return "file:///" + p.replace(/\\/g, "/");
                            }
                            return "file://" + p.replace(/\\/g, "/");
                        }
                        videoOutput: mediaVideoOutput
                        audioOutput: AudioOutput {
                            id: mediaAudioOutput
                            muted: (typeof audioController !== "undefined" && audioController && audioController.hasAudio && audioController.isPlaying)
                        }

                        onPositionChanged: {
                            if (mediaVideoPlayer.playbackState === MediaPlayer.PlayingState) {
                                if (typeof audioController === "undefined" || !audioController || !audioController.hasAudio || !audioController.isPlaying) {
                                    var sec = mediaVideoPlayer.position / 1000.0;
                                    if (Math.abs(videoController.currentTime - sec) > 0.02) {
                                        videoController.seekTime(sec);
                                    }
                                }
                            }
                        }

                        onPlaybackStateChanged: {
                            if (mediaVideoPlayer.playbackState === MediaPlayer.StoppedState) {
                                if (videoController.isPlaying) {
                                    videoController.pause();
                                    videoController.seekFrame(0);
                                }
                            }
                        }
                    }

                    VideoOutput {
                        id: mediaVideoOutput
                        anchors.fill: parent
                        fillMode: VideoOutput.Stretch
                        visible: !videoController.isDummy && videoController.hasVideo && mediaVideoPlayer.hasVideo
                    }

                    Connections {
                        target: typeof videoController !== "undefined" ? videoController : null
                        property real lastResyncMs: 0

                        function onPositionChanged() {
                            if (!mediaVideoPlayer.hasVideo) return;
                            var targetMs = Math.round(videoController.currentTime * 1000.0);
                            if (mediaVideoPlayer.playbackState !== MediaPlayer.PlayingState) {
                                // When paused or stopped, seek immediately to keep current frame in sync
                                if (Math.abs(mediaVideoPlayer.position - targetMs) > 15) {
                                    mediaVideoPlayer.position = targetMs;
                                }
                            } else {
                                var audioMaster = (typeof audioController !== "undefined" && audioController && audioController.hasAudio && audioController.isPlaying);
                                var now = Date.now();
                                if (audioMaster) {
                                    // AudioController is the clock master (e.g. playing audio timing selection); only resync if drift exceeds 800ms
                                    if (Math.abs(mediaVideoPlayer.position - targetMs) > 800 && (now - lastResyncMs > 600)) {
                                        lastResyncMs = now;
                                        mediaVideoPlayer.position = targetMs;
                                    }
                                } else {
                                    // MediaPlayer is the clock master; only seek if an external jump occurred (e.g. subtitle jump or slider jump)
                                    if (Math.abs(mediaVideoPlayer.position - targetMs) > 1000 && (now - lastResyncMs > 600)) {
                                        lastResyncMs = now;
                                        mediaVideoPlayer.position = targetMs;
                                    }
                                }
                            }
                        }
                        function onPlaybackStateChanged() {
                            if (!mediaVideoPlayer.hasVideo) return;
                            if (videoController.isPlaying) {
                                var targetMs = Math.round(videoController.currentTime * 1000.0);
                                if (Math.abs(mediaVideoPlayer.position - targetMs) > 15) {
                                    mediaVideoPlayer.position = targetMs;
                                }
                                mediaVideoPlayer.play();
                            } else {
                                mediaVideoPlayer.pause();
                            }
                        }
                    }

                    // Subtitle preview layer with real-time \pos coordinates and \frz rotation
                    Item {
                        id: subItem
                        property var posMatch: videoController.activeSubText.match(/\\pos\s*\(\s*([-\d\.]+)\s*,\s*([-\d\.]+)\s*\)/)
                        property var frzMatch: videoController.activeSubText.match(/\\frz([-\d\.]+)/)
                        property var fscxMatch: videoController.activeSubText.match(/\\fscx([-\d\.]+)/)
                        property var fscyMatch: videoController.activeSubText.match(/\\fscy([-\d\.]+)/)

                        property real scriptX: posMatch ? parseFloat(posMatch[1]) : (videoController.videoWidth * 0.5)
                        property real scriptY: posMatch ? parseFloat(posMatch[2]) : (videoController.videoHeight - 30)
                        property real frzAngle: frzMatch ? parseFloat(frzMatch[1]) : 0.0
                        property real fscxScale: fscxMatch ? (parseFloat(fscxMatch[1]) / 100.0) : 1.0
                        property real fscyScale: fscyMatch ? (parseFloat(fscyMatch[1]) / 100.0) : 1.0

                        property real screenX: (videoController.videoWidth > 0) ? (scriptX / videoController.videoWidth) * videoScreen.width : videoScreen.width * 0.5
                        property real screenY: (videoController.videoHeight > 0) ? (scriptY / videoController.videoHeight) * videoScreen.height : (videoScreen.height - 20)

                        x: Math.round(screenX)
                        y: Math.round(screenY)
                        rotation: frzAngle
                        transform: Scale {
                            xScale: subItem.fscxScale
                            yScale: subItem.fscyScale
                        }

                        Text {
                            anchors.centerIn: parent
                            text: videoController.activeSubText.replace(/\{[^\}]*\}/g, "")
                            visible: text.length > 0
                            font.pixelSize: Math.max(12, Math.round(videoScreen.height * 0.08))
                            font.family: uiTheme.uiFont
                            font.bold: true
                            color: "#ffffff"
                            style: Text.Outline
                            styleColor: "#000000"
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                // ═══════════════════════════════════════════════════════════════
                // Visual tools overlay (Mode 0: Crosshair, Mode 1: Drag pins, Mode 2: Protractor)
                // ═══════════════════════════════════════════════════════════════

                // Mode 0: Crosshair cursor
                Item {
                    id: crosshairOverlay
                    anchors.fill: parent
                    visible: typeof videoDisplayController !== "undefined" && videoDisplayController.crosshairVisible
                    z: 20

                    Rectangle {
                        x: 0
                        y: Math.round(videoDisplayController.crosshairY)
                        width: parent.width
                        height: 1
                        color: "#ffffff"
                        opacity: 0.85
                    }
                    Rectangle {
                        x: Math.round(videoDisplayController.crosshairX)
                        y: 0
                        width: 1
                        height: parent.height
                        color: "#ffffff"
                        opacity: 0.85
                    }

                    // Coordinate tooltip near cursor
                    Item {
                        x: Math.min(parent.width - 85, Math.max(4, Math.round(videoDisplayController.crosshairX) + 6))
                        y: Math.min(parent.height - 22, Math.max(4, Math.round(videoDisplayController.crosshairY) + 6))
                        width: coordText.width + 10
                        height: 18
                        Rectangle {
                            anchors.fill: parent
                            color: "#d0000000"
                            radius: 0
                            border.color: "#80ffffff"
                            border.width: 1
                        }
                        Text {
                            id: coordText
                            anchors.centerIn: parent
                            text: videoDisplayController.scriptCoordText
                            color: "#ffffff"
                            font.family: uiTheme.uiFont
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }

                // Mode 1: Drag pins and \move vector
                Item {
                    id: dragOverlay
                    anchors.fill: parent
                    visible: videoBox.currentTool === 1
                    z: 21

                    // Movement trajectory vector line
                    Repeater {
                        model: typeof videoDisplayController !== "undefined" ? videoDisplayController.connectingLines : []
                        Item {
                            anchors.fill: parent
                            Canvas {
                                id: arrowCanvas
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.clearRect(0, 0, width, height);
                                    var line = modelData;
                                    ctx.beginPath();
                                    ctx.strokeStyle = "#ffff00";
                                    ctx.lineWidth = 2;
                                    ctx.moveTo(line.x1, line.y1);
                                    ctx.lineTo(line.x2, line.y2);
                                    ctx.stroke();

                                    var angle = Math.atan2(line.y2 - line.y1, line.x2 - line.x1);
                                    ctx.beginPath();
                                    ctx.fillStyle = "#ffff00";
                                    ctx.moveTo(line.x2, line.y2);
                                    ctx.lineTo(line.x2 - 10 * Math.cos(angle - Math.PI / 6), line.y2 - 10 * Math.sin(angle - Math.PI / 6));
                                    ctx.lineTo(line.x2 - 10 * Math.cos(angle + Math.PI / 6), line.y2 - 10 * Math.sin(angle + Math.PI / 6));
                                    ctx.closePath();
                                    ctx.fill();
                                }
                                Connections {
                                    target: videoDisplayController
                                    function onVisualDataChanged() { arrowCanvas.requestPaint(); }
                                }
                            }
                        }
                    }

                    // Position control pins
                    Repeater {
                        model: typeof videoDisplayController !== "undefined" ? videoDisplayController.dragPins : []
                        Item {
                            x: Math.round(modelData.x - width / 2)
                            y: Math.round(modelData.y - height / 2)
                            width: 12
                            height: 12

                            // 0: Start (\pos) - square pin
                            Rectangle {
                                anchors.fill: parent
                                visible: modelData.type === 0
                                color: modelData.isHovered ? "#ffcc00" : "#5991dc"
                                border.color: "#ffffff"
                                border.width: 1.5
                            }

                            // 1: End (\move) - circle pin
                            Rectangle {
                                anchors.fill: parent
                                radius: width / 2
                                visible: modelData.type === 1
                                color: modelData.isHovered ? "#ffcc00" : "#32cd32"
                                border.color: "#ffffff"
                                border.width: 1.5
                            }

                            // 2: Origin (\org) - triangle pin
                            Canvas {
                                anchors.fill: parent
                                visible: modelData.type === 2
                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.clearRect(0, 0, width, height);
                                    ctx.beginPath();
                                    ctx.moveTo(width / 2, 0);
                                    ctx.lineTo(width, height);
                                    ctx.lineTo(0, height);
                                    ctx.closePath();
                                    ctx.fillStyle = modelData.isHovered ? "#ffcc00" : "#ff4500";
                                    ctx.fill();
                                    ctx.strokeStyle = "#ffffff";
                                    ctx.lineWidth = 1.5;
                                    ctx.stroke();
                                }
                            }

                            // Hover coordinate badge
                            Rectangle {
                                anchors.bottom: parent.top
                                anchors.bottomMargin: 3
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: pinHintText.width + 6
                                height: 16
                                color: "#e0000000"
                                radius: 0
                                visible: modelData.isHovered
                                Text {
                                    id: pinHintText
                                    anchors.centerIn: parent
                                    text: "(" + modelData.scriptX + "," + modelData.scriptY + ")"
                                    color: "#ffffff"
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }
                }

                // Mode 2: Z-axis rotation protractor
                Item {
                    id: rotateOverlay
                    anchors.fill: parent
                    visible: videoBox.currentTool === 2
                    z: 22

                    Canvas {
                        id: rotCanvas
                        anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);
                            if (typeof videoDisplayController === "undefined") return;
                            var rot = videoDisplayController.rotationData;
                            if (!rot || typeof rot.centerX === "undefined") return;

                            var cx = rot.centerX;
                            var cy = rot.centerY;
                            var r = rot.radius;
                            var angleDeg = rot.angle;
                            var angleRad = -angleDeg * (Math.PI / 180.0);

                            // 1. Protractor outer dashed circle
                            ctx.beginPath();
                            ctx.strokeStyle = "rgba(255, 255, 255, 0.6)";
                            ctx.lineWidth = 1.5;
                            ctx.setLineDash([4, 4]);
                            ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                            ctx.stroke();
                            ctx.setLineDash([]);

                            // 2. Origin crosshair
                            ctx.beginPath();
                            ctx.strokeStyle = "rgba(255, 255, 255, 0.8)";
                            ctx.lineWidth = 1;
                            ctx.moveTo(cx - 6, cy);
                            ctx.lineTo(cx + 6, cy);
                            ctx.moveTo(cx, cy - 6);
                            ctx.lineTo(cx, cy + 6);
                            ctx.stroke();

                            // 3. Angle baseline solid line
                            ctx.beginPath();
                            ctx.strokeStyle = "#5991dc";
                            ctx.lineWidth = 2;
                            ctx.moveTo(cx - r * Math.cos(angleRad), cy - r * Math.sin(angleRad));
                            ctx.lineTo(cx + r * Math.cos(angleRad), cy + r * Math.sin(angleRad));
                            ctx.stroke();

                            // 4. Drag pointer vector line
                            if (rot.isHolding) {
                                ctx.beginPath();
                                ctx.strokeStyle = "#ffcc00";
                                ctx.lineWidth = 1.5;
                                ctx.moveTo(cx, cy);
                                ctx.lineTo(rot.mouseX, rot.mouseY);
                                ctx.stroke();
                            }
                        }
                        Connections {
                            target: videoDisplayController
                            function onVisualDataChanged() { rotCanvas.requestPaint(); }
                        }
                    }

                    // Rotation angle badge
                    Rectangle {
                        x: Math.min(parent.width - 70, Math.max(10, ((videoDisplayController.rotationData.centerX || 50) + 15)))
                        y: Math.min(parent.height - 24, Math.max(10, ((videoDisplayController.rotationData.centerY || 50) - 25)))
                        width: angleText.width + 8
                        height: 18
                        color: "#c0000000"
                        radius: 0
                        border.color: "#5991dc"
                        border.width: 1
                        Text {
                            id: angleText
                            anchors.centerIn: parent
                            text: (videoDisplayController.rotationData.angle !== undefined) ?
                                  (Math.round(videoDisplayController.rotationData.angle * 10) / 10 + "°") : "0°"
                            color: "#ffff00"
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }
                }

                // Mode 4: 2D scale tool (\fscx, \fscy)
                Item {
                    id: scaleOverlay
                    anchors.fill: parent
                    visible: videoBox.currentTool === 4
                    z: 23

                    Canvas {
                        id: scaleCanvas
                        anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);
                            if (typeof videoDisplayController === "undefined") return;
                            var sc = videoDisplayController.scaleData;
                            if (!sc || typeof sc.x === "undefined") return;

                            var cx = sc.x;
                            var cy = sc.y;
                            var sx = sc.scaleX;
                            var sy = sc.scaleY;

                            // 1. Center anchor crosshair
                            ctx.beginPath();
                            ctx.strokeStyle = "#ffffff";
                            ctx.lineWidth = 1.5;
                            ctx.moveTo(cx - 8, cy); ctx.lineTo(cx + 8, cy);
                            ctx.moveTo(cx, cy - 8); ctx.lineTo(cx, cy + 8);
                            ctx.stroke();

                            // 2. X-axis scale indicator (horizontal arrow)
                            var armX = Math.max(30, Math.min(200, (sx / 100.0) * 50));
                            ctx.beginPath();
                            ctx.strokeStyle = "#ff4d4d";
                            ctx.lineWidth = 2;
                            ctx.moveTo(cx - armX, cy); ctx.lineTo(cx + armX, cy);
                            ctx.moveTo(cx + armX, cy); ctx.lineTo(cx + armX - 6, cy - 4);
                            ctx.moveTo(cx + armX, cy); ctx.lineTo(cx + armX - 6, cy + 4);
                            ctx.moveTo(cx - armX, cy); ctx.lineTo(cx - armX + 6, cy - 4);
                            ctx.moveTo(cx - armX, cy); ctx.lineTo(cx - armX + 6, cy + 4);
                            ctx.stroke();

                            // 3. Y-axis scale indicator (vertical arrow)
                            var armY = Math.max(30, Math.min(200, (sy / 100.0) * 50));
                            ctx.beginPath();
                            ctx.strokeStyle = "#33cc33";
                            ctx.lineWidth = 2;
                            ctx.moveTo(cx, cy - armY); ctx.lineTo(cx, cy + armY);
                            ctx.moveTo(cx, cy - armY); ctx.lineTo(cx - 4, cy - armY + 6);
                            ctx.moveTo(cx, cy - armY); ctx.lineTo(cx + 4, cy - armY + 6);
                            ctx.moveTo(cx, cy + armY); ctx.lineTo(cx - 4, cy + armY - 6);
                            ctx.moveTo(cx, cy + armY); ctx.lineTo(cx + 4, cy + armY - 6);
                            ctx.stroke();

                            // 4. Bounding box frame (dashed)
                            ctx.beginPath();
                            ctx.strokeStyle = "rgba(255, 255, 255, 0.4)";
                            ctx.setLineDash([3, 3]);
                            ctx.strokeRect(cx - armX, cy - armY, armX * 2, armY * 2);
                            ctx.setLineDash([]);
                        }
                        Connections {
                            target: videoDisplayController
                            function onVisualDataChanged() { scaleCanvas.requestPaint(); }
                        }
                    }

                    // Scale percentage badge
                    Rectangle {
                        x: Math.min(parent.width - 95, Math.max(10, ((videoDisplayController.scaleData.x || 50) + 15)))
                        y: Math.min(parent.height - 24, Math.max(10, ((videoDisplayController.scaleData.y || 50) - 25)))
                        width: scaleText.width + 8
                        height: 18
                        color: "#c0000000"
                        radius: 0
                        border.color: "#32cd32"
                        border.width: 1
                        Text {
                            id: scaleText
                            anchors.centerIn: parent
                            text: (videoDisplayController.scaleData.scaleX !== undefined) ?
                                  (Math.round(videoDisplayController.scaleData.scaleX) + "% × " + Math.round(videoDisplayController.scaleData.scaleY) + "%") : "100% × 100%"
                            color: "#ffffff"
                            font.bold: true
                            font.pixelSize: 10
                        }
                    }
                }

                // Mode 5: Rectangular clip tool (\clip, \iclip)
                Item {
                    id: clipOverlay
                    anchors.fill: parent
                    visible: videoBox.currentTool === 5
                    z: 24

                    property var cData: typeof videoDisplayController !== "undefined" ? videoDisplayController.clipData : ({})
                    property bool hasClip: !!(cData && cData.active)
                    property real clX: hasClip ? cData.x1 : 0
                    property real clY: hasClip ? cData.y1 : 0
                    property real clW: hasClip ? Math.max(0, cData.x2 - cData.x1) : 0
                    property real clH: hasClip ? Math.max(0, cData.y2 - cData.y1) : 0

                    // Dimmed translucent mask outside clip boundary
                    Rectangle {
                        x: 0; y: 0; width: parent.width; height: clipOverlay.clY
                        color: "#60000000"; visible: clipOverlay.hasClip
                    }
                    Rectangle {
                        x: 0; y: clipOverlay.clY + clipOverlay.clH; width: parent.width; height: Math.max(0, parent.height - (clipOverlay.clY + clipOverlay.clH))
                        color: "#60000000"; visible: clipOverlay.hasClip
                    }
                    Rectangle {
                        x: 0; y: clipOverlay.clY; width: clipOverlay.clX; height: clipOverlay.clH
                        color: "#60000000"; visible: clipOverlay.hasClip
                    }
                    Rectangle {
                        x: clipOverlay.clX + clipOverlay.clW; y: clipOverlay.clY; width: Math.max(0, parent.width - (clipOverlay.clX + clipOverlay.clW)); height: clipOverlay.clH
                        color: "#60000000"; visible: clipOverlay.hasClip
                    }

                    // Clip boundary dashed frame
                    Rectangle {
                        x: Math.round(clipOverlay.clX)
                        y: Math.round(clipOverlay.clY)
                        width: Math.round(clipOverlay.clW)
                        height: Math.round(clipOverlay.clH)
                        color: "transparent"
                        border.color: "#ffff00"
                        border.width: 1.5
                        visible: clipOverlay.hasClip

                        // Corner drag handle pins
                        Rectangle { width: 8; height: 8; anchors.centerIn: parent.topLeft; color: "#ffff00"; border.color: "#000000"; border.width: 1 }
                        Rectangle { width: 8; height: 8; anchors.centerIn: parent.topRight; color: "#ffff00"; border.color: "#000000"; border.width: 1 }
                        Rectangle { width: 8; height: 8; anchors.centerIn: parent.bottomLeft; color: "#ffff00"; border.color: "#000000"; border.width: 1 }
                        Rectangle { width: 8; height: 8; anchors.centerIn: parent.bottomRight; color: "#ffff00"; border.color: "#000000"; border.width: 1 }

                        // Clip dimensions badge
                        Rectangle {
                            anchors.bottom: parent.top
                            anchors.bottomMargin: 3
                            anchors.left: parent.left
                            width: clipDimText.width + 6
                            height: 16
                            color: "#d0000000"
                            radius: 0
                            Text {
                                id: clipDimText
                                anchors.centerIn: parent
                                text: Math.round(clipOverlay.clW) + " × " + Math.round(clipOverlay.clH)
                                color: "#ffff00"
                                font.pixelSize: 10
                            }
                        }
                    }
                }

                // Mode 3: 3D XY rotation tool (\frx, \fry)
                Item {
                    id: rotateXYOverlay
                    anchors.fill: parent
                    visible: videoBox.currentTool === 3
                    z: 25

                    property var pData: typeof videoDisplayController !== "undefined" ? videoDisplayController.posData : ({})
                    property real centerX: pData && pData.hasPos ? pData.x : parent.width / 2
                    property real centerY: pData && pData.hasPos ? pData.y : parent.height / 2
                    property real frxAngle: 0
                    property real fryAngle: 0

                    Canvas {
                        id: rotXYCanvas
                        anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);

                            var cx = rotateXYOverlay.centerX;
                            var cy = rotateXYOverlay.centerY;

                            // 1. Horizontal guide ring for \fry rotation
                            ctx.strokeStyle = "#40c0ff";
                            ctx.lineWidth = 1.5;
                            ctx.beginPath();
                            ctx.ellipse(cx - 50, cy - 18, 100, 36);
                            ctx.stroke();

                            // 2. Vertical guide ring for \frx rotation
                            ctx.strokeStyle = "#ff6060";
                            ctx.beginPath();
                            ctx.ellipse(cx - 18, cy - 50, 36, 100);
                            ctx.stroke();

                            // Center anchor
                            ctx.fillStyle = "#ffffff";
                            ctx.strokeStyle = "#000000";
                            ctx.lineWidth = 1;
                            ctx.beginPath();
                            ctx.arc(cx, cy, 4, 0, 2 * Math.PI);
                            ctx.fill();
                            ctx.stroke();
                        }
                    }

                    // 3D rotation angles badge
                    Rectangle {
                        x: Math.min(parent.width - width - 8, Math.max(8, rotateXYOverlay.centerX + 20))
                        y: Math.min(parent.height - height - 8, Math.max(8, rotateXYOverlay.centerY - 30))
                        width: rotXYText.width + 10
                        height: 18
                        radius: 0
                        color: "#d0000000"
                        border.color: "#ffffff"
                        border.width: 1

                        Text {
                            id: rotXYText
                            anchors.centerIn: parent
                            text: "\\frx: " + Math.round(rotateXYOverlay.frxAngle) + "°  \\fry: " + Math.round(rotateXYOverlay.fryAngle) + "°"
                            font.pixelSize: 10
                            font.family: uiTheme.monoFont
                            color: "#ffffff"
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        property int dragAxis: 0
                        property real startMouseX: 0
                        property real startMouseY: 0
                        property real startFrx: 0
                        property real startFry: 0

                        onPressed: (mouse) => {
                            startMouseX = mouse.x;
                            startMouseY = mouse.y;
                            startFrx = rotateXYOverlay.frxAngle;
                            startFry = rotateXYOverlay.fryAngle;
                            var dx = mouse.x - rotateXYOverlay.centerX;
                            var dy = mouse.y - rotateXYOverlay.centerY;
                            if (Math.abs(dx) > Math.abs(dy)) {
                                dragAxis = 2;
                            } else {
                                dragAxis = 1;
                            }
                        }
                        onPositionChanged: (mouse) => {
                            if (pressed) {
                                if (dragAxis === 2) {
                                    var diffY = (mouse.x - startMouseX) * 0.8;
                                    rotateXYOverlay.fryAngle = (startFry + diffY) % 360;
                                } else {
                                    var diffX = (mouse.y - startMouseY) * 0.8;
                                    rotateXYOverlay.frxAngle = (startFrx + diffX) % 360;
                                }
                                rotXYCanvas.requestPaint();
                            }
                        }
                    }
                }

                // Mode 6: Vector clip tool (\clip(m ...))
                Item {
                    id: vectorClipOverlay
                    anchors.fill: parent
                    visible: videoBox.currentTool === 6
                    z: 26

                    property int subTool: 0
                    property var points: [
                        { x: 60, y: 60, type: "line" },
                        { x: 220, y: 60, type: "line" },
                        { x: 220, y: 180, type: "bezier", cx1: 220, cy1: 120, cx2: 140, cy2: 180 },
                        { x: 60, y: 180, type: "line" }
                    ]
                    property bool isClosed: true
                    property int selectedPointIndex: -1

                    Rectangle {
                        anchors.top: parent.top
                        anchors.topMargin: 8
                        anchors.horizontalCenter: parent.horizontalCenter
                        height: 24
                        width: subToolRow.width + 10
                        radius: 0
                        color: "#ffffff"
                        border.color: "#c8c8c8"
                        border.width: 1
                        z: 30

                        Row {
                            id: subToolRow
                            anchors.centerIn: parent
                            spacing: 2

                            component VecToolBtn: ToolButton {
                                id: vtBtn
                                property string tip: ""
                                property int idx: 0
                                property string btnText: ""
                                implicitWidth: 22
                                implicitHeight: 20
                                padding: 0

                                background: Rectangle {
                                    radius: 0
                                    color: vectorClipOverlay.subTool === vtBtn.idx ? "#cce8ff" : (vtBtn.hovered ? "#f0f0f0" : "transparent")
                                    border.color: vectorClipOverlay.subTool === vtBtn.idx ? "#99d1ff" : "transparent"
                                    border.width: 1
                                }
                                contentItem: Text {
                                    text: vtBtn.btnText
                                    font.pixelSize: 11
                                    font.bold: true
                                    font.family: uiTheme.uiFont
                                    color: vectorClipOverlay.subTool === vtBtn.idx ? "#0066cc" : "#333333"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: vectorClipOverlay.subTool = vtBtn.idx
                                ToolTip.visible: hovered
                                ToolTip.text: tip
                            }

                            VecToolBtn { idx: 0; btnText: "✛"; tip: qsTr("Drag control points") }
                            VecToolBtn { idx: 1; btnText: "L"; tip: qsTr("Append a line") }
                            VecToolBtn { idx: 2; btnText: "B"; tip: qsTr("Append a bezier bicubic curve") }
                            VecToolBtn { idx: 3; btnText: "C"; tip: qsTr("Close contour") }
                            VecToolBtn { idx: 4; btnText: "✕"; tip: qsTr("Remove a control point") }
                        }
                    }

                    Canvas {
                        id: vecCanvas
                        anchors.fill: parent

                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);
                            if (vectorClipOverlay.points.length < 2) return;

                            ctx.strokeStyle = "#ffff00";
                            ctx.lineWidth = 1.5;
                            ctx.beginPath();
                            var pts = vectorClipOverlay.points;
                            ctx.moveTo(pts[0].x, pts[0].y);

                            for (var i = 1; i < pts.length; i++) {
                                var p = pts[i];
                                if (p.type === "bezier" && typeof p.cx1 !== "undefined") {
                                    ctx.bezierCurveTo(p.cx1, p.cy1, p.cx2, p.cy2, p.x, p.y);
                                } else {
                                    ctx.lineTo(p.x, p.y);
                                }
                            }
                            if (vectorClipOverlay.isClosed) {
                                ctx.closePath();
                            }
                            ctx.stroke();

                            for (var j = 0; j < pts.length; j++) {
                                var pt = pts[j];
                                ctx.fillStyle = (j === vectorClipOverlay.selectedPointIndex) ? "#ff4040" : "#ffff00";
                                ctx.strokeStyle = "#000000";
                                ctx.lineWidth = 1;
                                ctx.fillRect(pt.x - 4, pt.y - 4, 8, 8);
                                ctx.strokeRect(pt.x - 4, pt.y - 4, 8, 8);

                                if (pt.type === "bezier" && typeof pt.cx1 !== "undefined") {
                                    ctx.strokeStyle = "#80ffffff";
                                    ctx.beginPath();
                                    ctx.moveTo(pt.cx1, pt.cy1);
                                    ctx.lineTo(pt.cx2, pt.cy2);
                                    ctx.stroke();

                                    ctx.fillStyle = "#ffffff";
                                    ctx.beginPath();
                                    ctx.arc(pt.cx1, pt.cy1, 3, 0, 2*Math.PI);
                                    ctx.arc(pt.cx2, pt.cy2, 3, 0, 2*Math.PI);
                                    ctx.fill();
                                }
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        property int activeDragIdx: -1

                        onPressed: (mouse) => {
                            var pts = vectorClipOverlay.points.slice();
                            for (var i = 0; i < pts.length; i++) {
                                var dx = Math.abs(mouse.x - pts[i].x);
                                var dy = Math.abs(mouse.y - pts[i].y);
                                if (dx <= 6 && dy <= 6) {
                                    activeDragIdx = i;
                                    vectorClipOverlay.selectedPointIndex = i;
                                    if (vectorClipOverlay.subTool === 4) {
                                        pts.splice(i, 1);
                                        vectorClipOverlay.points = pts;
                                        vecCanvas.requestPaint();
                                    }
                                    return;
                                }
                            }
                            if (vectorClipOverlay.subTool === 1) {
                                pts.push({ x: mouse.x, y: mouse.y, type: "line" });
                                vectorClipOverlay.points = pts;
                                vecCanvas.requestPaint();
                            } else if (vectorClipOverlay.subTool === 2) {
                                var last = pts[pts.length - 1];
                                pts.push({
                                    x: mouse.x, y: mouse.y, type: "bezier",
                                    cx1: (last.x + mouse.x) / 2, cy1: last.y,
                                    cx2: (last.x + mouse.x) / 2, cy2: mouse.y
                                });
                                vectorClipOverlay.points = pts;
                                vecCanvas.requestPaint();
                            } else if (vectorClipOverlay.subTool === 3) {
                                vectorClipOverlay.isClosed = !vectorClipOverlay.isClosed;
                                vecCanvas.requestPaint();
                            }
                        }
                        onPositionChanged: (mouse) => {
                            if (pressed && activeDragIdx >= 0 && activeDragIdx < vectorClipOverlay.points.length) {
                                var pts = vectorClipOverlay.points.slice();
                                pts[activeDragIdx].x = mouse.x;
                                pts[activeDragIdx].y = mouse.y;
                                vectorClipOverlay.points = pts;
                                vecCanvas.requestPaint();
                            }
                        }
                        onReleased: {
                            activeDragIdx = -1;
                        }
                    }
                }

                // Input routing surface: forwards clicks, drags, wheel scrolls, and double-clicks to C++ controller
                MouseArea {
                    id: videoInteractiveArea
                    anchors.fill: parent
                    anchors.margins: 1
                    hoverEnabled: true
                    preventStealing: true
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                    cursorShape: typeof videoDisplayController !== "undefined" ? videoDisplayController.cursorShape : Qt.ArrowCursor

                    onPressed: (mouse) => {
                        forceActiveFocus();
                        if (mouse.button === Qt.RightButton) {
                            var p = mapToItem(videoBox, mouse.x, mouse.y);
                            videoContextMenu.x = Math.max(0, Math.min(videoBox.width - videoContextMenu.implicitWidth - 5, p.x));
                            videoContextMenu.y = Math.max(0, Math.min(videoBox.height - 240, p.y));
                            videoContextMenu.targetX = mouse.x;
                            videoContextMenu.targetY = mouse.y;
                            videoContextMenu.open();
                        } else {
                            if (typeof videoDisplayController !== "undefined")
                                videoDisplayController.mousePressed(mouse.x, mouse.y, mouse.button, mouse.modifiers);
                        }
                    }
                    onPositionChanged: (mouse) => {
                        if (typeof videoDisplayController !== "undefined")
                            videoDisplayController.mouseMoved(mouse.x, mouse.y, mouse.buttons, mouse.modifiers);
                    }
                    onReleased: (mouse) => {
                        if (typeof videoDisplayController !== "undefined")
                            videoDisplayController.mouseReleased(mouse.x, mouse.y, mouse.button, mouse.modifiers);
                    }
                    onDoubleClicked: (mouse) => {
                        if (typeof videoDisplayController !== "undefined")
                            videoDisplayController.mouseDoubleClicked(mouse.x, mouse.y, mouse.button, mouse.modifiers);
                    }
                    onExited: () => {
                        if (typeof videoDisplayController !== "undefined")
                            videoDisplayController.mouseLeft();
                    }
                    onWheel: (wheel) => {
                        if (typeof videoDisplayController !== "undefined")
                            videoDisplayController.wheel(wheel.angleDelta.y, wheel.x, wheel.y, wheel.modifiers);
                    }

                    onWidthChanged: {
                        if (typeof videoDisplayController !== "undefined")
                            videoDisplayController.setViewportSize(width, height);
                    }
                    onHeightChanged: {
                        if (typeof videoDisplayController !== "undefined")
                            videoDisplayController.setViewportSize(width, height);
                    }
                    Component.onCompleted: {
                        if (typeof videoDisplayController !== "undefined")
                            videoDisplayController.setViewportSize(width, height);
                    }
                }
            }
        }

        // ==========================================
        // Divider separator
        // ==========================================
        Item {
            Layout.fillWidth: true
            implicitHeight: 2
            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: "#b0b0b0"
            }
            Rectangle {
                anchors.top: parent.top
                anchors.topMargin: 1
                width: parent.width
                height: 1
                color: "#ffffff"
            }
        }

        // ==========================================
        // Video timeline frame slider
        // ==========================================
        Item {
            id: videoSlider
            Layout.fillWidth: true
            // 原版 VideoSlider 的客户区尺寸是 20x25（video_slider.cpp:60）
            implicitHeight: 25

            property int val: videoController.currentFrame
            property int max: Math.max(1, videoController.totalFrames - 1)
            property bool hasFocus: false

            function getXAtValue(v) {
                if (max <= 0) return 5;
                return Math.floor(v * (width - 10) / max) + 5;
            }

            function getValueAtX(x) {
                if (width <= 10) return 0;
                var v = Math.round((x - 5) * max / (width - 10));
                return Math.max(0, Math.min(max, v));
            }

            // 1. Static track & keyframe markers canvas (repainted only on geometry or keyframe changes)
            Canvas {
                id: sliderTrackCanvas
                anchors.fill: parent

                Connections {
                    target: videoController
                    function onKeyframesChanged() {
                        sliderTrackCanvas.requestPaint();
                    }
                    function onVideoInfoChanged() {
                        sliderTrackCanvas.requestPaint();
                    }
                }

                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    var w = width;
                    var h = height;

                    // 系统色：wxSYS_COLOUR_3DFACE / 3DDKSHADOW / 3DLIGHT
                    var face = videoBox.winBg;
                    var shad = "#696969";
                    var high = "#e3e3e3";

                    // 背景：整个客户区填 3DFACE
                    ctx.fillStyle = face;
                    ctx.fillRect(0, 0, w, h);

                    // 获得焦点时整圈 1px 点线（原版 wxPENSTYLE_DOT + 3DDKSHADOW）
                    if (videoSlider.hasFocus) {
                        ctx.strokeStyle = shad;
                        ctx.lineWidth = 1;
                        ctx.setLineDash([1, 1]);
                        ctx.strokeRect(0.5, 0.5, w - 1, h - 1);
                        ctx.setLineDash([]);
                    }

                    var x1 = 5;
                    var x2 = w - 5;
                    var y1 = 8;
                    var y2 = h - 8;

                    // 1. 关键帧刻度：3DDKSHADOW，位于滑轨上方 y=2..8（原版 video_slider.cpp:221-227）
                    if (videoController && videoController.keyframeList && videoController.keyframeList.length > 0) {
                        ctx.fillStyle = shad;
                        for (var i = 0; i < videoController.keyframeList.length; i++) {
                            var kx = videoSlider.getXAtValue(videoController.keyframeList[i]);
                            ctx.fillRect(kx, 2, 1, 6);
                        }
                    }

                    // 2. 滑轨：1px 3D 凹陷方框，只描边不填充（上/左暗，下/右亮）
                    ctx.fillStyle = shad;
                    ctx.fillRect(x1, y1, x2 - x1 + 1, 1);
                    ctx.fillRect(x1, y1, 1, y2 - y1 + 1);
                    ctx.fillStyle = high;
                    ctx.fillRect(x1, y2, x2 - x1 + 1, 1);
                    ctx.fillRect(x2, y1, 1, y2 - y1 + 1);
                }
            }

            // 2. Scene-graph positioned thumb cursor (zero Canvas repaints on frame ticks)
            //    几何与配色严格对齐原版 OnPaint：先铺 3DFACE 底，再画 highlights / shades / 黑色描边
            Item {
                id: sliderThumb
                width: 9
                height: 18
                y: 5
                x: videoSlider.getXAtValue(videoSlider.val) - 4
                visible: videoController && videoController.hasVideo

                Canvas {
                    id: thumbCanvas
                    anchors.fill: parent

                    Connections {
                        target: videoSlider
                        function onHasFocusChanged() {
                            thumbCanvas.requestPaint();
                        }
                    }

                    Component.onCompleted: requestPaint()

                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);

                        var face = videoBox.winBg;
                        var shad = "#696969";
                        var high = "#e3e3e3";
                        var sel = "#7bfbe8";       // 焦点高亮青
                        var notSel = "#31645c";    // 非焦点：sel 的 2/5 暗色
                        var bord = "#000000";

                        // 游标底：rect(curX-2, y1-1, 4, y2-y1+5)
                        ctx.fillStyle = face;
                        ctx.fillRect(2, 2, 4, 14);

                        var line = (ax, ay, bx, by) => {
                            ctx.beginPath();
                            ctx.moveTo(ax + 0.5, ay + 0.5);
                            ctx.lineTo(bx + 0.5, by + 0.5);
                            ctx.stroke();
                        };

                        // highlights（3DLIGHT）
                        ctx.strokeStyle = high;
                        ctx.lineWidth = 1;
                        line(4, 1, 0, 5);
                        ctx.fillRect(1, 5, 1, 13);

                        // shades（3DDKSHADOW）
                        ctx.strokeStyle = shad;
                        line(5, 2, 8, 5);
                        ctx.fillRect(7, 5, 1, 13);
                        ctx.fillRect(1, 16, 7, 1);

                        // outline（黑）
                        ctx.strokeStyle = bord;
                        line(4, 0, 0, 4);
                        line(4, 0, 8, 4);
                        ctx.fillRect(0, 4, 1, 14);
                        ctx.fillRect(8, 4, 1, 14);
                        ctx.fillRect(1, 17, 8, 1);
                        ctx.fillRect(1, 12, 8, 1);

                        // 底部选区块：rect(curX-3, y2+1, 7, 4)
                        ctx.fillStyle = videoSlider.hasFocus ? sel : notSel;
                        ctx.fillRect(1, 13, 7, 4);
                    }
                }
            }

            MouseArea {
                id: sliderMouse
                anchors.fill: parent
                preventStealing: true
                hoverEnabled: true

                // 原版：Shift 点击吸附到最近关键帧（video_slider.cpp:126-137）
                function snapToKeyframe(frame) {
                    if (!videoController.keyframeList || videoController.keyframeList.length === 0)
                        return frame;
                    var kfs = videoController.keyframeList;
                    // 关键帧表按帧号升序，找第一个 >= frame 的项，再和它的前一项比谁更近
                    var lo = 0, hi = kfs.length - 1, pos = kfs.length;
                    while (lo <= hi) {
                        var mid = (lo + hi) >> 1;
                        if (kfs[mid] < frame) lo = mid + 1;
                        else { pos = mid; hi = mid - 1; }
                    }
                    if (pos === kfs.length) return kfs[kfs.length - 1];
                    if (pos + 1 < kfs.length && (frame - kfs[pos]) > (kfs[pos + 1] - frame))
                        return kfs[pos + 1];
                    return kfs[pos];
                }

                // 原版：Shift 滚轮按关键帧步进（video_slider.cpp:148-155）
                function stepKeyframe(dir) {
                    var kfs = videoController.keyframeList;
                    if (!kfs || kfs.length === 0) return;
                    var cur = videoController.currentFrame;
                    if (dir > 0) {
                        for (var i = 0; i < kfs.length; i++) {
                            if (kfs[i] > cur) { videoController.seekFrame(kfs[i]); return; }
                        }
                    } else {
                        for (var j = kfs.length - 1; j >= 0; j--) {
                            if (kfs[j] < cur) { videoController.seekFrame(kfs[j]); return; }
                        }
                    }
                }

                onPressed: (mouse) => {
                    if (videoController.isPlaying) {
                        videoController.pause();
                    }
                    videoSlider.hasFocus = true;
                    // 拖动期间按低分辨率解码，松手补全尺寸（见 setScrubbing）
                    videoController.setScrubbing(true);
                    var target = videoSlider.getValueAtX(mouse.x);
                    if (mouse.modifiers & Qt.ShiftModifier) target = snapToKeyframe(target);
                    videoController.seekFrame(target);
                }
                onReleased: {
                    videoController.setScrubbing(false);
                }
                onPositionChanged: (mouse) => {
                    if (pressed) {
                        var target = videoSlider.getValueAtX(mouse.x);
                        if (mouse.modifiers & Qt.ShiftModifier) target = snapToKeyframe(target);
                        videoController.seekFrame(target);
                    }
                }
                onWheel: (wheel) => {
                    if (videoController.isPlaying) {
                        videoController.pause();
                    }
                    var delta = wheel.angleDelta.y > 0 ? -1 : 1;
                    if (wheel.modifiers & Qt.ShiftModifier) stepKeyframe(-delta);
                    else videoController.stepFrame(delta);
                }
            }
        }

        // ==========================================
        // Video transport bar: controls, timecodes, and zoom dropdown
        // ==========================================
        RowLayout {
            Layout.fillWidth: true
            implicitHeight: 22
            spacing: 3

            // Transport button base item
            component VBtn: ToolButton {
                id: vtb
                property string iconSrc: ""
                property string tipText: ""
                implicitWidth: 20
                implicitHeight: 20
                padding: 1

                contentItem: Image {
                    source: vtb.iconSrc
                    width: 16
                    height: 16
                    fillMode: Image.Pad
                    smooth: false
                    anchors.centerIn: parent
                }

                background: Rectangle {
                    radius: 0
                    color: vtb.pressed ? "#0000001a" : (vtb.hovered ? "#0000000f" : "transparent")
                    border.color: "transparent"
                }

                ToolTip.visible: hovered
                ToolTip.text: tipText
                ToolTip.delay: 700
            }

            // Play / Pause toggle
            VBtn {
                iconSrc: (typeof videoController !== "undefined" && videoController.isPlaying) ? "../../assets/icons_native/button_pause_16.png" : "../../assets/icons_native/button_play_16.png"
                tipText: (typeof videoController !== "undefined" && videoController.isPlaying) ? qsTr("Pause video playback") : qsTr("Play the video starting on this position")
                onClicked: {
                    if (videoController.isPlaying) {
                        videoController.pause();
                        if (typeof audioController !== "undefined" && audioController && audioController.isPlaying) {
                            audioController.stop();
                        }
                    } else {
                        videoController.play();
                    }
                }
            }

            // Play current line
            VBtn {
                iconSrc: "../../assets/icons_native/button_playline_16.png"
                tipText: qsTr("Play the video for the current line")
                onClicked: {
                    if (typeof audioController !== "undefined" && audioController && audioController.hasAudio) {
                        audioController.playCurrentLine();
                    } else {
                        videoController.playCurrentLine();
                    }
                }
            }

            // Pause / Stop
            VBtn {
                iconSrc: "../../assets/icons_native/button_pause_16.png"
                tipText: qsTr("Stop video playback")
                onClicked: {
                    videoController.pause();
                    if (typeof audioController !== "undefined" && audioController && audioController.isPlaying) {
                        audioController.stop();
                    }
                }
            }

            // Toggle video autoscroll
            ToolButton {
                id: btnAutoScroll
                implicitWidth: 20
                implicitHeight: 20
                checkable: true
                checked: videoController.autoScroll
                padding: 1

                contentItem: Image {
                    source: "../../assets/icons_native/toggle_video_autoscroll_16.png"
                    width: 16
                    height: 16
                    fillMode: Image.Pad
                    smooth: false
                    anchors.centerIn: parent
                }

                background: Rectangle {
                    radius: 0
                    color: btnAutoScroll.checked ? "#cce8ff" : (btnAutoScroll.hovered ? "#0000000f" : "transparent")
                    border.color: btnAutoScroll.checked ? "#99d1ff" : "transparent"
                    border.width: 1
                }

                onClicked: videoController.setAutoScroll(!videoController.autoScroll)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Toggle automatically seeking video to the start time of selected lines"); ToolTip.delay: 700
            }

            // Current timecode and frame index
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 20
                color: videoController.isKeyframe ? "#d8faeb" : "#ffffff"
                radius: 0
                border.color: "#cecece"
                border.width: 1

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    text: videoController.timeAndFrameString
                    font.pixelSize: 12
                    font.family: uiTheme.uiFont
                    renderType: Text.NativeRendering
                    color: "#1e1e1e"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignLeft
                }

                ToolTip.visible: posMouse.containsMouse
                ToolTip.text: qsTr("Current frame time and number")
                ToolTip.delay: 700

                MouseArea {
                    id: posMouse
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }

            // Relative timecode offset from active line
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 20
                color: "#f5f5f5"
                radius: 0
                border.color: "#cecece"
                border.width: 1

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    text: videoController.relativeTimeString
                    font.pixelSize: 12
                    font.family: uiTheme.uiFont
                    renderType: Text.NativeRendering
                    color: "#505050"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignLeft
                }

                ToolTip.visible: subsPosMouse.containsMouse
                ToolTip.text: qsTr("Time of this frame relative to start and end of current subs")
                ToolTip.delay: 700

                MouseArea {
                    id: subsPosMouse
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }

            // Video zoom level dropdown
            NativeComboBox {
                id: zoomCombo
                implicitWidth: 68
                implicitHeight: 20
                model: ["Fit", "12.5%", "25%", "37.5%", "50%", "62.5%", "75%", "87.5%", "100%",
                        "112.5%", "125%", "137.5%", "150%", "162.5%", "175%", "187.5%", "200%",
                        "212.5%", "225%", "237.5%", "250%", "262.5%", "275%", "287.5%", "300%"]
                // 初值跟随控制器；Fit = 自适应视口（默认档）
                currentIndex: (typeof videoDisplayController !== "undefined" && videoDisplayController)
                              ? Math.max(0, model.indexOf(videoDisplayController.zoomText)) : 0

                onActivated: (index) => {
                    var z = model[index];
                    if (typeof videoDisplayController !== "undefined") {
                        videoDisplayController.setZoomText(z);
                    }
                    videoController.setZoom(z);
                    videoBox.zoomApplied(z);
                }

                // 只在用户主动选档（onActivated）时回写控制器；程序侧的同步
                // （Fit 比例更新、窗口缩放变化）不能反过来推给控制器，
                // 否则会把 Fit 状态顶成显式档位。

                ToolTip.visible: hovered
                ToolTip.text: qsTr("视频缩放比例")
                ToolTip.delay: 700
            }

            Connections {
                target: typeof videoDisplayController !== "undefined" ? videoDisplayController : null
                function onWindowZoomChanged() {
                    if (zoomCombo.currentText !== videoDisplayController.zoomText) {
                        var idx = zoomCombo.model.indexOf(videoDisplayController.zoomText);
                        if (idx >= 0) {
                            zoomCombo.currentIndex = idx;
                        } else if (zoomCombo.editable) {
                            zoomCombo.editText = videoDisplayController.zoomText;
                        }
                    }
                }
            }
        }
    }

    // Video viewport context menu
    Popup {
        id: videoContextMenu
        implicitWidth: 200
        padding: 0
        topPadding: 3
        bottomPadding: 3
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        property int targetX: 0
        property int targetY: 0

        background: Rectangle {
            radius: 4
            color: "#ffffff"
            border.color: "#dcdcdc"
            border.width: 1
        }

        contentItem: Column {
            spacing: 0
            width: parent.width

            component VContextMenuItem: Rectangle {
                id: vcMi
                property string itemText: ""
                property string iconSrc: ""
                signal triggered()

                width: parent.width
                height: 20
                color: vcMiArea.containsMouse ? "#e8e8e8" : "transparent"
                radius: 4

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    Item {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        Image {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            fillMode: Image.Pad
                            smooth: false
                            source: vcMi.iconSrc
                            visible: vcMi.iconSrc !== ""
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: vcMi.itemText
                        font.pixelSize: 11
                        font.family: uiTheme.uiFont
                        renderType: Text.NativeRendering
                        color: "#1e1e1e"
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                MouseArea {
                    id: vcMiArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        vcMi.triggered();
                        videoContextMenu.close();
                    }
                }
            }

            VContextMenuItem {
                itemText: qsTr("Save PNG snapshot")
                iconSrc: "../../assets/icons_native/save_toolbutton_16.png"
                onTriggered: videoController.saveFrame(false, false)
            }
            VContextMenuItem {
                itemText: qsTr("Copy image to Clipboard")
                iconSrc: "../../assets/icons_native/copy_button_16.png"
                onTriggered: videoController.copyFrame(false, false)
            }

            Rectangle { width: parent.width - 12; height: 1; color: "#e5e5e5"; anchors.horizontalCenter: parent.horizontalCenter }

            VContextMenuItem {
                itemText: qsTr("Save PNG snapshot (no subtitles)")
                onTriggered: videoController.saveFrame(true, false)
            }
            VContextMenuItem {
                itemText: qsTr("Copy image to Clipboard (no subtitles)")
                onTriggered: videoController.copyFrame(true, false)
            }

            Rectangle { width: parent.width - 12; height: 1; color: "#e5e5e5"; anchors.horizontalCenter: parent.horizontalCenter }

            VContextMenuItem {
                itemText: qsTr("Save PNG snapshot (only subtitles)")
                onTriggered: videoController.saveFrame(false, true)
            }
            VContextMenuItem {
                itemText: qsTr("Copy image to Clipboard (only subtitles)")
                onTriggered: videoController.copyFrame(false, true)
            }

            Rectangle { width: parent.width - 12; height: 1; color: "#e5e5e5"; anchors.horizontalCenter: parent.horizontalCenter }

            VContextMenuItem {
                itemText: qsTr("Copy coordinates to Clipboard")
                onTriggered: videoController.copyCoordinates(videoContextMenu.targetX, videoContextMenu.targetY)
            }
            VContextMenuItem {
                itemText: qsTr("Reset Video &Pan")
                onTriggered: {
                    if (typeof videoDisplayController !== "undefined") {
                        videoDisplayController.resetPanZoom();
                    }
                    videoController.setZoom("100%");
                }
            }
        }
    }
}

