// Copyright (c) 2005 - 2026, Aegisub Project & Contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

// DialogAbout: Upstream Aegisub About dialog with official splash banner, credits,
// and third-party software attribution.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../controls"

NativeDialogFrame {
    id: dialog
    isModal: true
    title: qsTr("About AegisubQT")
    iconSource: "../../assets/icons_native/icon.ico"
    implicitWidth: 422
    implicitHeight: 480

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Brand artwork banner
        Image {
            source: "../../assets/branding/banner.png"
            Layout.fillWidth: true
            Layout.preferredHeight: 240
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        // Separator line
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#d0d0d0"
        }

        // Readonly credits text view
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            clip: true

            Flickable {
                id: flickable
                anchors.fill: parent
                anchors.margins: 8
                contentWidth: width - 16
                contentHeight: creditText.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                ScrollBar.vertical: ScrollBar {
                    id: vbar
                    active: true
                }

                TextEdit {
                    id: creditText
                    width: flickable.width - (vbar.visible ? vbar.width + 4 : 0)
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WordWrap
                    font.pixelSize: 11
                    font.family: uiTheme.uiFont
                    color: "#000000"
                    text: "AegisubQT " + (Qt.application.version || "4.0.2") + " (Unofficial)\n" +
                          "A modern cross-platform Aegisub rewrite powered by Qt 6 Quick & C++20.\n\n" +
                          "Copyright \u00a9 2005-2026 Rodrigo Braz Monteiro, Niels Martin Hansen, Thomas Goyne, Cuptu et al.\n\n" +
                          "Qt Quick Modern Architecture:\n" +
                          "    Cuptu\n\n" +
                          "Programmers:\n" +
                          "    Alysson Souza e Silva\n" +
                          "    Amar Takhar\n" +
                          "    Charlie Jiang\n" +
                          "    Cuptu\n" +
                          "    Dan Donovan\n" +
                          "    Daniel Moscoviter\n" +
                          "    David Conrad\n" +
                          "    David Lamparter\n" +
                          "    Eric Batalitzky\n" +
                          "    Evgeniy Stepanov\n" +
                          "    Fredrik Mellbin\n" +
                          "    Grigori Goronzy\n" +
                          "    Karl Blomster\n" +
                          "    Mike Matsnev\n" +
                          "    Moritz Brunner\n" +
                          "    Muhammad Lukman Nasaruddin\n" +
                          "    Niels Martin Hansen\n" +
                          "    Patryk Pomykalski\n" +
                          "    Qirui Wang\n" +
                          "    Ravi Pinjala\n" +
                          "    Rodrigo Braz Monteiro\n" +
                          "    Simone Cociancich\n" +
                          "    Thomas Goyne\n\n" +
                          "User manual written by:\n" +
                          "    Karl Blomster\n" +
                          "    Niels Martin Hansen\n" +
                          "    Rodrigo Braz Monteiro\n\n" +
                          "Icons by:\n" +
                          "    Philip Cash\n\n" +
                          "Additional thanks to:\n" +
                          "    Mentar\n" +
                          "    Sigurd Tao Lyngse\n" +
                          "    Everyone in the Aegisub IRC channel\n" +
                          "    Everyone who ever reported a bug\n\n" +
                          "Aegisub includes portions from the following other projects:\n" +
                          "    wxWidgets - Copyright \u00a9 Julian Smart, Robert Roebling et al;\n" +
                          "    wxStyledTextCtrl - Copyright \u00a9 Robin Dunn, Neil Hodgson;\n" +
                          "    Scintilla - Copyright \u00a9 Neil Hodgson;\n" +
                          "    Boost - Copyright \u00a9 Beman Dawes, David Abrahams et al;\n" +
                          "    UniversalCharDet - Copyright \u00a9 Netscape Communications Corp.;\n" +
                          "    ICU - Copyright \u00a9 International Business Machines Corp.;\n" +
                          "    Lua - Copyright \u00a9 Lua.org, PUC-Rio;\n" +
                          "    LuaJIT - Copyright \u00a9 Mike Pall;\n" +
                          "    luabins - Copyright \u00a9 Alexander Gladysh;\n" +
                          "    Hunspell - Copyright \u00a9 Kevin Hendricks;\n" +
                          "    PortAudio - Copyright \u00a9 Ross Bencina, Phil Burk;\n" +
                          "    FFmpeg - Copyright \u00a9 Fabrice Bellard;\n" +
                          "    FFMS2 - Copyright \u00a9 Fredrik Mellbin;\n" +
                          "    dav1d - Copyright \u00a9 VideoLAN and dav1d authors;\n" +
                          "    Avisynth 2.5 - Copyright \u00a9 Ben Rudiak-Gould et al;\n" +
                          "    csri - Copyright \u00a9 David Lamparter;\n" +
                          "    vsfilter - Copyright \u00a9 Gabest et al;\n" +
                          "    libass - Copyright \u00a9 Evgeniy Stepanov, Grigori Goronzy;\n" +
                          "    Matroska Parser - Copyright \u00a9 Mike Matsnev;\n" +
                          "    Freetype - Copyright \u00a9 David Turner, Robert Wilhelm, Werner Lemberg;\n" +
                          "    Fontconfig - Copyright \u00a9 Keith Packard et al;\n" +
                          "    FFTW - Copyright \u00a9 Matteo Frigo, Massachusetts Institute of Technology;\n\n" +
                          qsTr("See the help file for full credits.")
                }
            }
        }

        // Separator line
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#d0d0d0"
        }

        // Dialog action button
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 6
            spacing: 0

            Item { Layout.fillWidth: true }

            NativeButton {
                text: qsTr("OK")
                isDefault: true
                Layout.preferredWidth: 75
                onClicked: dialog.close()
            }

            Item { Layout.fillWidth: true }
        }
    }
}
