// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

.pragma library

// Converts an ASS timestamp string (h:mm:ss.cc) to milliseconds.
function assToMs(t) {
    if (!t) return 0;
    var parts = String(t).split(":");
    if (parts.length !== 3) return 0;
    var sec = parts[2].split(".");
    return (parseInt(parts[0], 10) * 3600 + parseInt(parts[1], 10) * 60 + parseInt(sec[0], 10)) * 1000
         + (sec.length > 1 ? parseInt(sec[1], 10) * 10 : 0);
}

// Formats a millisecond duration into an ASS timestamp string (h:mm:ss.cc).
function msToAss(ms) {
    if (ms < 0 || isNaN(ms)) ms = 0;
    var totalSec = Math.floor(ms / 1000);
    var centisec = Math.floor((ms % 1000) / 10);
    var h = Math.floor(totalSec / 3600);
    var m = Math.floor((totalSec % 3600) / 60);
    var s = totalSec % 60;
    return h + ":" + (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s + "." + (centisec < 10 ? "0" : "") + centisec;
}
