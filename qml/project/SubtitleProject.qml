// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// SubtitleProject: Central coordinator for subtitle document state, selection, and undo context.
// Subtitle dialogue storage, document snapshot undo stacks, ASS file parser/serializer,
// timing engines, line manipulation algorithms, and query engines are backed by native C++ SubtitleModel.

import QtQuick
import "AssUtils.js" as AssUtils

QtObject {
    id: project

    // Primary subtitle document model backed by native C++ SubtitleModel
    property var subtitleModel: (typeof nativeSubtitleModel !== "undefined") ? nativeSubtitleModel : null

    // Modification tracking delegated to C++ SubtitleModel (commitId/savedCommitId, aligning with IsModified())
    readonly property bool isModified: subtitleModel ? subtitleModel.modified : false
    property string currentFileName: subtitleModel ? subtitleModel.fileName : qsTr("Untitled")
    property int currentSelectedIndex: 0
    property var selectedIndices: [0]
    property var internalSubClipboard: []
    property string initialSelectedText: ""
    property int tagHidingMode: 2
    property int sortColumn: -1
    property bool sortAscending: true

    // Last search parameters backing Edit > Find Next (upstream edit/find_next).
    property string lastFindQuery: ""
    property var lastFindOptions: null
    property int lastFindRow: -1
    property int lastFindEnd: 0

    // Undo / Redo history delegated directly to native C++ SubtitleModel
    readonly property bool canUndo: subtitleModel ? subtitleModel.canUndo : false
    readonly property bool canRedo: subtitleModel ? subtitleModel.canRedo : false
    readonly property string undoDescription: subtitleModel ? subtitleModel.undoDescription : ""
    readonly property string redoDescription: subtitleModel ? subtitleModel.redoDescription : ""

    function pushUndo(description) {
        if (subtitleModel) {
            subtitleModel.pushUndo(description || "edit", currentSelectedIndex, selectedIndices || [0]);
        }
        // Modification state is tracked automatically by the C++ model upon commit; QML does not set it manually
    }

    function undo() {
        if (!subtitleModel || !canUndo) return;
        var desc = undoDescription;
        var sel = subtitleModel.undo();
        if (sel && sel.selectedIndex !== undefined) {
            currentSelectedIndex = Math.min(sel.selectedIndex, subtitleModel.count - 1);
            if (sel.selectedIndices && sel.selectedIndices.length > 0) {
                selectedIndices = sel.selectedIndices;
            }
        }
        statusMessage(qsTr("Undone: ") + desc);
        dataModified();
    }

    function redo() {
        if (!subtitleModel || !canRedo) return;
        var desc = redoDescription;
        var sel = subtitleModel.redo();
        if (sel && sel.selectedIndex !== undefined) {
            currentSelectedIndex = Math.min(sel.selectedIndex, subtitleModel.count - 1);
            if (sel.selectedIndices && sel.selectedIndices.length > 0) {
                selectedIndices = sel.selectedIndices;
            }
        }
        statusMessage(qsTr("Redone: ") + desc);
        dataModified();
    }

    // [Script Info] ASS script header key-value map and canvas resolution settings
    property var scriptInfo: subtitleModel ? subtitleModel.scriptInfo : ({
        "Title": qsTr("Untitled"),
        "Original Script": "",
        "Original Translation": "",
        "Original Editing": "",
        "Original Timing": "",
        "Synch Point": "",
        "Script Updated By": "",
        "Update Details": "",
        "PlayResX": 1920,
        "PlayResY": 1080,
        "LayoutResX": 0,
        "LayoutResY": 0,
        "WrapStyle": 0,
        "ScaledBorderAndShadow": "yes",
        "YCbCr Matrix": "None"
    })

    function getScriptInfo(key, defVal) {
        if (subtitleModel) return subtitleModel.getScriptInfo(key, defVal !== undefined ? defVal : "");
        if (scriptInfo && scriptInfo[key] !== undefined) return scriptInfo[key];
        return defVal !== undefined ? defVal : "";
    }

    function setScriptInfo(key, value) {
        if (subtitleModel) {
            subtitleModel.setScriptInfoKey(key, value);
            scriptInfo = subtitleModel.scriptInfo;
        } else {
            var info = Object.assign({}, scriptInfo);
            info[key] = value;
            scriptInfo = info;
        }
        dataModified();
    }

    property ListModel attachmentModel: ListModel {}

    function refreshAttachments() {
        attachmentModel.clear();
        if (!subtitleModel) return;
        var items = subtitleModel.attachments();
        for (var i = 0; i < items.length; ++i) attachmentModel.append(items[i]);
    }

    function addAttachment(path, isFont) {
        return subtitleModel ? subtitleModel.addAttachmentFile(path, isFont) : false;
    }

    function removeAttachment(index) {
        return subtitleModel ? subtitleModel.removeAttachment(index) : false;
    }

    function extractAttachment(index, folder) {
        return subtitleModel ? subtitleModel.extractAttachment(index, folder) : false;
    }

    Component.onCompleted: refreshAttachments()

    // Boundary timestamps of non-selected dialogue lines used as waveform snap markers
    readonly property var snapPoints: subtitleModel ? subtitleModel.getSnapPoints(currentSelectedIndex) : []

    // Project mutation and selection notifications
    signal statusMessage(string msg)
    signal lineSelected(int index, var item)
    signal ensureRowVisible(int index)
    signal searchMatchFound(int row, int start, int length)
    signal dataModified()

    property Connections automationConnections: Connections {
        target: (typeof automationManager !== "undefined") ? automationManager : null
        function onMacroSelectionChanged(targetProject, activeIndex, indices) {
            if (targetProject !== project && targetProject !== subtitleModel) return;
            if (activeIndex < 0) {
                currentSelectedIndex = -1;
                selectedIndices = [];
                return;
            }
            project.selectRow(activeIndex, false, false);
            project.selectedIndices = indices.slice();
        }
    }

    // QtObject has no default child property; Connections must be declared as a named property
    property Connections modelConnections: Connections {
        target: subtitleModel
        function onContentModified() {
            // C++ model tracks modified status internally via commitId; QML only triggers UI refresh
            project.dataModified();
        }
        function onAttachmentsChanged() { project.refreshAttachments(); }
    }

    function isRowSelected(idx) {
        return selectedIndices.indexOf(idx) !== -1;
    }

    function renumberLines() {
        if (subtitleModel) {
            subtitleModel.renumberLines();
        }
    }

    function selectRow(idx, ctrlPressed, shiftPressed) {
        if (!subtitleModel || idx < 0 || idx >= subtitleModel.count) return;
        if (shiftPressed) {
            var startIdx = currentSelectedIndex;
            var minIdx = Math.min(startIdx, idx);
            var maxIdx = Math.max(startIdx, idx);
            var newSel = [];
            for (var i = minIdx; i <= maxIdx; ++i) {
                newSel.push(i);
            }
            selectedIndices = newSel;
            currentSelectedIndex = idx;
        } else if (ctrlPressed) {
            var pos = selectedIndices.indexOf(idx);
            var copy = selectedIndices.slice();
            if (pos !== -1) {
                if (copy.length > 1) {
                    copy.splice(pos, 1);
                    selectedIndices = copy;
                }
            } else {
                copy.push(idx);
                copy.sort(function(a, b) { return a - b; });
                selectedIndices = copy;
                currentSelectedIndex = idx;
            }
        } else {
            selectedIndices = [idx];
            currentSelectedIndex = idx;
        }
        var it = subtitleModel.get(currentSelectedIndex);
        if (it) {
            initialSelectedText = it.text;
            lineSelected(currentSelectedIndex, it);
        }
    }

    function selectAllRows() {
        if (!subtitleModel) return;
        var all = [];
        for (var i = 0; i < subtitleModel.count; ++i) all.push(i);
        selectedIndices = all;
        if (subtitleModel.count > 0) currentSelectedIndex = 0;
        statusMessage(qsTr("Selected all ") + subtitleModel.count + qsTr(" lines"));
    }

    function selectVisibleLines(vTime) {
        if (!subtitleModel) return;
        var vMs = Math.round((vTime || 0) * 1000.0);
        var visible = [];
        for (var i = 0; i < subtitleModel.count; ++i) {
            var s = subtitleModel.getLineStartMs(i);
            var e = subtitleModel.getLineEndMs(i);
            if (vMs >= s && vMs <= e) visible.push(i);
        }
        if (visible.length > 0) {
            selectRow(visible[0], false, false);
            selectedIndices = visible;
            statusMessage(qsTr("Selected %1 lines visible in the current video frame").arg(visible.length));
        } else {
            statusMessage(qsTr("No lines visible in the current video frame"));
        }
    }

    // Line operations delegated directly to native C++ SubtitleModel
    function insertLine(before, atVideoTime, videoCurrentTime) {
        if (!subtitleModel) return;
        pushUndo(qsTr("insert line"));
        var baseIdx = currentSelectedIndex >= 0 ? currentSelectedIndex : 0;
        var startMs = 0;
        var endMs = 5000;
        var defDuration = 5000;

        if (atVideoTime && typeof videoCurrentTime === "number") {
            startMs = Math.round(videoCurrentTime * 1000.0);
            endMs = startMs + defDuration;
        } else if (before) {
            var baseStart = subtitleModel.getLineStartMs(baseIdx);
            endMs = baseStart;
            if (baseIdx > 0) {
                var prevEnd = subtitleModel.getLineEndMs(baseIdx - 1);
                startMs = (baseStart - prevEnd > 100) ? prevEnd : Math.max(0, baseStart - defDuration);
            } else {
                startMs = Math.max(0, baseStart - defDuration);
            }
        } else {
            var baseEnd = subtitleModel.getLineEndMs(baseIdx);
            startMs = baseEnd;
            if (baseIdx < subtitleModel.count - 1) {
                var nextStart = subtitleModel.getLineStartMs(baseIdx + 1);
                endMs = (nextStart - baseEnd > 100) ? Math.min(startMs + defDuration, nextStart) : (startMs + defDuration);
            } else {
                endMs = startMs + defDuration;
            }
        }

        var newIdx = subtitleModel.insertLine(baseIdx, before, startMs, endMs, {});
        selectRow(newIdx, false, false);
        statusMessage(qsTr("Inserted new line"));
        dataModified();
    }

    function duplicateSelectedLines() {
        if (!subtitleModel || selectedIndices.length === 0) return;
        pushUndo(qsTr("duplicate lines"));
        var newSel = subtitleModel.duplicateSelectedLines(selectedIndices);
        if (newSel.length > 0) {
            selectRow(newSel[0], false, false);
            selectedIndices = newSel;
            statusMessage(qsTr("Duplicated ") + newSel.length + qsTr(" lines"));
            dataModified();
        }
    }

    function splitLineAtFrame(shift, videoCtrl) {
        if (!subtitleModel || subtitleModel.count === 0 || !videoCtrl || !videoCtrl.hasVideo ||
                selectedIndices.length === 0) return;
        var frame = videoCtrl.currentFrame;
        var firstEnd = videoCtrl.timeAtFrameMs(frame + (shift < 0 ? -1 : 0), 2);
        var secondStart = videoCtrl.timeAtFrameMs(frame + (shift < 0 ? 0 : 1), 1);
        pushUndo(qsTr("split line"));
        var newRows = subtitleModel.splitLinesAtFrame(selectedIndices, firstEnd, secondStart,
            videoCtrl.timeAtFrameMs(frame, 1), videoCtrl.timeAtFrameMs(frame, 2), videoCtrl.timeAtFrameMs(frame));
        if (newRows.length === 0) return;
        selectRow(newRows[0], false, false);
        selectedIndices = newRows;
        statusMessage(qsTr("Split line at current video frame"));
        dataModified();
    }

    function splitLineAtCursor(pos, mode, videoTime) {
        if (!subtitleModel || subtitleModel.count === 0 || pos <= 0) return;
        pushUndo(qsTr("split line"));
        var vMs = Math.round((videoTime || 0) * 1000.0);
        subtitleModel.splitLineAtCursor(currentSelectedIndex, pos, mode, vMs);
        selectRow(currentSelectedIndex + 1, false, false);
        statusMessage(qsTr("Split line at cursor"));
        dataModified();
    }

    function swapSelectedLines() {
        if (!subtitleModel || selectedIndices.length !== 2) return;
        pushUndo(qsTr("swap lines"));
        subtitleModel.swapSelectedLines(selectedIndices);
        selectRow(currentSelectedIndex, false, false);
        statusMessage(qsTr("Swapped selected lines"));
        dataModified();
    }

    function joinSelectedLines(mode) {
        if (!subtitleModel || selectedIndices.length < 2) return;
        pushUndo(qsTr("join lines"));
        var count = selectedIndices.length;
        subtitleModel.joinSelectedLines(selectedIndices, mode);
        selectRow(currentSelectedIndex, false, false);
        statusMessage(qsTr("Merged ") + count + qsTr(" lines"));
        dataModified();
    }

    function deleteSelectedLines() {
        if (!subtitleModel || selectedIndices.length === 0) return;
        pushUndo(qsTr("delete lines"));
        var nextIdx = subtitleModel.deleteSelectedLines(selectedIndices);
        selectRow(nextIdx, false, false);
        statusMessage(qsTr("Deleted lines"));
        dataModified();
    }

    function cutSelectedLines(copyHelper) {
        if (copySelectedLines(copyHelper)) deleteSelectedLines();
    }

    function copySelectedLines(copyHelper) {
        if (!subtitleModel || selectedIndices.length === 0) return false;
        var sorted = selectedIndices.slice().sort(function(a, b) { return a - b; });
        var copied = [];
        for (var i = 0; i < sorted.length; ++i) {
            var it = subtitleModel.get(sorted[i]);
            if (!it || it.lineNumber === undefined) continue;
            copied.push(it);
        }
        if (!copied.length) return false;
        internalSubClipboard = copied;
        var serialized = subtitleModel.serializeClipboardLines(sorted);
        if (copyHelper && typeof copyHelper.copyText === "function") {
            copyHelper.copyText(serialized);
        } else if (typeof aegisubCore !== "undefined" && aegisubCore) {
            if (!aegisubCore.setClipboardText(serialized)) {
                statusMessage(qsTr("Unable to write to the system clipboard"));
                return false;
            }
        }
        statusMessage(qsTr("Copied ") + copied.length + qsTr(" lines"));
        return true;
    }

    function pasteLines(pasteOver, pastedData, allowedFields) {
        if (!subtitleModel) return;
        var toPaste;
        if (pastedData && pastedData.length > 0) toPaste = pastedData;
        else if (typeof aegisubCore !== "undefined" && aegisubCore)
            toPaste = subtitleModel.parseClipboardLines(aegisubCore.clipboardText());
        else toPaste = internalSubClipboard;
        if (!toPaste || toPaste.length === 0 || !subtitleModel) return;
        pushUndo(pasteOver ? qsTr("paste over") : qsTr("paste lines"));

        if (pasteOver) {
            var count = Math.min(toPaste.length, selectedIndices.length);
            var sorted = selectedIndices.slice().sort(function(a, b) { return a - b; });
            var fields = [];
            if (Array.isArray(allowedFields) && allowedFields.length > 0) {
                fields = allowedFields;
            } else if (allowedFields && typeof allowedFields === "object") {
                for (var key in allowedFields) {
                    if (allowedFields[key]) fields.push(key);
                }
            }
            if (fields.length === 0) fields = ["text"];

            for (var k = 0; k < count; ++k) {
                var targetIdx = sorted[k];
                var src = toPaste[k];
                for (var f = 0; f < fields.length; ++f) {
                    var fName = fields[f];
                    if (src[fName] !== undefined) {
                        subtitleModel.setProperty(targetIdx, fName, src[fName]);
                    }
                }
            }
            selectRow(sorted[0], false, false);
            statusMessage(qsTr("Pasted over %1 lines").arg(count));
            dataModified();
            return;
        }

        var insertPos = currentSelectedIndex >= 0 ? currentSelectedIndex : subtitleModel.count;
        var newSel = [];
        for (var i = 0; i < toPaste.length; ++i) {
            subtitleModel.insert(insertPos + i, toPaste[i]);
            newSel.push(insertPos + i);
        }
        subtitleModel.renumberLines();
        selectedIndices = newSel;
        currentSelectedIndex = newSel[0];
        statusMessage(qsTr("Pasted ") + toPaste.length + qsTr(" lines"));
        dataModified();
    }

    function sortLines(field, selectedOnly) {
        if (!subtitleModel || subtitleModel.count === 0) return;
        pushUndo(qsTr("sort lines"));
        subtitleModel.sortLines(field, selectedOnly ? selectedIndices : [], true);
        selectRow(currentSelectedIndex, false, false);
        statusMessage(qsTr("Sorted by %1").arg(field));
        dataModified();
    }

    function sortByColumn(col) {
        if (!subtitleModel || subtitleModel.count === 0) return;
        pushUndo(qsTr("sort column"));
        if (sortColumn === col) {
            sortAscending = !sortAscending;
        } else {
            sortColumn = col;
            sortAscending = true;
        }
        subtitleModel.sortByColumn(col, sortAscending);
        selectRow(0, false, false);
        statusMessage(qsTr("Sorted by column (") + (sortAscending ? qsTr("ascending") : qsTr("descending")) + ")");
        dataModified();
    }

    function applyKanjiCopy(srcStyle, dstStyle) {
        if (!subtitleModel || subtitleModel.count === 0) return 0;
        pushUndo(qsTr("kanji timer"));
        var count = subtitleModel.applyKanjiCopy(srcStyle, dstStyle);
        statusMessage(qsTr("Karaoke timing copy complete: synced ") + count + qsTr(" lines"));
        dataModified();
        return count;
    }

    // Timing operations delegated directly to native C++ SubtitleModel
    function makeTimesContinuous(changeStart) {
        if (!subtitleModel || subtitleModel.count === 0) return;
        pushUndo(qsTr("continuous times"));
        subtitleModel.makeTimesContinuous(currentSelectedIndex, changeStart);
        selectRow(currentSelectedIndex, false, false);
        statusMessage(changeStart ? qsTr("Made start times continuous") : qsTr("Made end times continuous"));
        dataModified();
    }

    function snapStartTimeToVideo(vTime) {
        if (!subtitleModel || subtitleModel.count === 0) return;
        pushUndo(qsTr("snap start"));
        var vMs = Math.round((vTime || 0) * 1000.0);
        subtitleModel.snapStartTime(currentSelectedIndex, vMs);
        selectRow(currentSelectedIndex, false, false);
        statusMessage(qsTr("Snapped start time to the current video frame"));
        dataModified();
    }

    function snapEndTimeToVideo(vTime) {
        if (!subtitleModel || subtitleModel.count === 0) return;
        pushUndo(qsTr("snap end"));
        var vMs = Math.round((vTime || 0) * 1000.0);
        subtitleModel.snapEndTime(currentSelectedIndex, vMs);
        selectRow(currentSelectedIndex, false, false);
        statusMessage(qsTr("Snapped end time to the current video frame"));
        dataModified();
    }

    function recombineSelectedLines() {
        if (!subtitleModel || selectedIndices.length < 2) return;
        var result = subtitleModel.recombineSelectedLines(selectedIndices, currentSelectedIndex);
        if (!result.success) { statusMessage(result.message); return; }
        if (!result.changed) return;
        selectedIndices = result.selectedIndices;
        currentSelectedIndex = result.selectedIndex;
        statusMessage(qsTr("Recombined selected lines"));
        dataModified();
    }

    function shiftToCurrentFrame(vTime) {
        if (!subtitleModel || subtitleModel.count === 0 || selectedIndices.length === 0) return;
        pushUndo(qsTr("shift to current frame"));
        var vMs = Math.round((vTime || 0) * 1000.0);
        subtitleModel.shiftToCurrentFrame(vMs, selectedIndices);
        selectRow(currentSelectedIndex, false, false);
        statusMessage(qsTr("Shifted ") + selectedIndices.length + qsTr(" lines to the current frame"));
        dataModified();
    }

    function processTiming(options) {
        if (!subtitleModel || subtitleModel.count === 0) return {success: true, changed: false, modifiedCount: 0};
        var result = subtitleModel.processTiming(options, selectedIndices, currentSelectedIndex);
        if (!result.success) { statusMessage(result.message); return result; }
        if (result.changed) dataModified();
        return result;
    }

    function shiftTimes(amountMs, shiftStart, shiftEnd, affectMode) {
        if (!subtitleModel || subtitleModel.count === 0) return;
        pushUndo(qsTr("shift times"));
        subtitleModel.shiftTimes(amountMs, shiftStart, shiftEnd, affectMode, selectedIndices);
        var selection = selectedIndices.slice();
        selectRow(currentSelectedIndex, false, false);
        selectedIndices = selection;
        dataModified();
    }

    function shiftTimesByFrames(amount, shiftStart, shiftEnd, affectMode, videoCtrl) {
        if (!subtitleModel || !videoCtrl || !videoCtrl.hasVideo || !amount) return;
        var changes = [];
        var firstSelected = selectedIndices.length ? Math.min.apply(null, selectedIndices) : subtitleModel.count;
        for (var row = 0; row < subtitleModel.count; ++row) {
            if (affectMode === 1 && selectedIndices.indexOf(row) === -1) continue;
            if (affectMode === 2 && row < firstSelected) continue;
            var start = subtitleModel.getLineStartMs(row);
            var end = subtitleModel.getLineEndMs(row);
            var newStart = shiftStart ? Math.max(0, videoCtrl.timeAtFrameMs(videoCtrl.frameAtTimeMs(start, 1) + amount, 1)) : start;
            var newEnd = shiftEnd ? Math.max(0, videoCtrl.timeAtFrameMs(videoCtrl.frameAtTimeMs(end, 2) + amount, 2)) : end;
            if (newStart !== start || newEnd !== end) changes.push({row: row, start: newStart, end: newEnd});
        }
        if (!changes.length) return;
        pushUndo(qsTr("shift times by frames"));
        for (var i = 0; i < changes.length; ++i) {
            var change = changes[i];
            if (shiftStart) subtitleModel.setProperty(change.row, "start", AssUtils.msToAss(change.start));
            if (shiftEnd) subtitleModel.setProperty(change.row, "end", AssUtils.msToAss(change.end));
        }
        var selection = selectedIndices.slice();
        selectRow(currentSelectedIndex, false, false);
        selectedIndices = selection;
        dataModified();
    }

    // Search operations delegated directly to native C++ SubtitleModel
    function selectLines(action, fldIdx, mode, invert, matchCase, comments, dialogues, query) {
        if (!subtitleModel || subtitleModel.count === 0) return;
        var finalSel = subtitleModel.selectLines(action, fldIdx, mode, invert, matchCase, comments, dialogues, query, selectedIndices);
        if (finalSel.length > 0) {
            selectRow(finalSel[0], false, false);
            selectedIndices = finalSel;
            statusMessage(qsTr("Selected ") + finalSel.length + qsTr(" lines"));
        } else {
            selectedIndices = [];
            statusMessage(qsTr("No matching lines found"));
        }
        dataModified();
    }

    function findAndReplace(query, replaceWith, options, replaceAll) {
        if (!subtitleModel || subtitleModel.count === 0 || !query) return 0;
        options = Object.assign({}, options || {});
        options.selectedIndices = selectedIndices.slice();
        // Record search parameters so Edit > Find Next can repeat them (upstream edit/find_next).
        lastFindQuery = query;
        lastFindOptions = options;
        lastFindRow = -1;
        lastFindEnd = 0;
        pushUndo(qsTr("replace"));
        var count = subtitleModel.findAndReplace(query, replaceWith, options, replaceAll, currentSelectedIndex);
        if (count > 0) {
            statusMessage(qsTr("Replaced ") + count + qsTr(" matches"));
            dataModified();
        } else if (count === 0) {
            statusMessage(qsTr("No matches found"));
        } else {
            statusMessage(qsTr("Invalid regular expression"));
        }
        return count;
    }

    // Upstream Subtitle > Split by Karaoke: one line per karaoke syllable.
    function splitSelectedByKaraoke() {
        if (!subtitleModel || selectedIndices.length === 0) return;
        var result = subtitleModel.splitSelectedByKaraoke(selectedIndices, currentSelectedIndex);
        if (!result.success) { statusMessage(result.message); return; }
        if (!result.changed) return;
        selectRow(result.activeIndex, false, false);
        selectedIndices = result.selectedIndices;
        statusMessage(qsTr("Split selected lines by karaoke syllables"));
        dataModified();
    }

    function exportFiltered(path, filters, charset) {
        if (typeof automationManager === "undefined")
            return {success: false, message: qsTr("Automation manager is unavailable")};
        var result = automationManager.exportSubtitles(path, charset, filters, project);
        statusMessage(result.message);
        return result;
    }

    // Serialization interface for C++ backend controllers and automation macro scripts
    function getAllSubtitleLines() {
        return subtitleModel ? subtitleModel.getAllLines() : [];
    }

    function setAllSubtitleLines(lines) {
        if (!subtitleModel) return;
        subtitleModel.setAllLines(lines);
        selectRow(0, false, false);
        dataModified();
    }

    // Set selected lines to the scene containing the current video frame.
    function snapToKeyframes(bounds) {
        if (!subtitleModel || !bounds || bounds.start === undefined || bounds.end === undefined ||
                bounds.end <= bounds.start) return;
        var changed = false;
        var indices = selectedIndices && selectedIndices.length > 0 ? selectedIndices : [currentSelectedIndex];
        for (var i = 0; i < indices.length; ++i) {
            var row = indices[i];
            if (row < 0 || row >= subtitleModel.count) continue;
            if (subtitleModel.getLineStartMs(row) === bounds.start &&
                    subtitleModel.getLineEndMs(row) === bounds.end) continue;
            if (!changed) pushUndo(qsTr("Snap to scene"));
            subtitleModel.setProperty(row, "start", AssUtils.msToAss(bounds.start));
            subtitleModel.setProperty(row, "end", AssUtils.msToAss(bounds.end));
            changed = true;
        }
        if (changed) {
            dataModified();
            statusMessage(qsTr("Snapped to scene"));
        }
    }

    // Searches for the next occurrence of query in lines without altering content.
    // When called without arguments (Edit > Find Next) it repeats the last search.
    function findNext(query, options) {
        if (!subtitleModel) return -1;
        var newSearch = false;
        if (!query) {
            query = lastFindQuery;
            options = lastFindOptions || {};
        } else {
            // Dialog buttons pass the query every time. Only changed search
            // criteria start over; repeated clicks continue the same search.
            var requested = options || {};
            var previous = lastFindOptions || {};
            newSearch = query !== lastFindQuery;
            var defaults = {matchCase: false, useRegex: false, skipComments: true,
                            skipTags: false, selectedOnly: false, field: "text"};
            for (var key in defaults) {
                var nextValue = requested[key] === undefined ? defaults[key] : requested[key];
                var oldValue = previous[key] === undefined ? defaults[key] : previous[key];
                if (nextValue !== oldValue) newSearch = true;
            }
            lastFindQuery = query;
            options = Object.assign({}, requested);
            if (newSearch) {
                lastFindRow = -1;
                lastFindEnd = 0;
            } else {
                options.selectedIndices = previous.selectedIndices;
            }
        }
        if (!query) {
            statusMessage(qsTr("Not found: %1").arg(""));
            return -1;
        }
        options = Object.assign({}, options || {});
        if (newSearch || !options.selectedIndices) options.selectedIndices = selectedIndices.slice();
        lastFindOptions = options;
        var startRow = lastFindRow >= 0 ? lastFindRow : Math.max(0, currentSelectedIndex);
        var startOffset = lastFindRow >= 0 ? lastFindEnd : 0;
        var match = subtitleModel.findNextMatch(query, options, startRow, startOffset);
        if (match.row !== undefined) {
            lastFindRow = match.row;
            lastFindEnd = match.start + Math.max(1, match.length);
            selectRow(match.row, false, false);
            searchMatchFound(match.row, match.start, match.length);
            statusMessage(qsTr("Found on line %1").arg(match.row + 1));
            return match.row;
        } else {
            statusMessage(qsTr("Not found: %1").arg(query));
            return -1;
        }
    }

    // Creates a new blank subtitle project document
    function fileNew() {
        lastFindRow = -1;
        lastFindEnd = 0;
        if (subtitleModel) {
            subtitleModel.newDocument();
        }
        currentFileName = qsTr("Untitled");
        selectRow(0, false, false);
        statusMessage(qsTr("New subtitle project created"));
        dataModified();
        // Modification tracking has been reset in C++ by newDocument()
    }

    // Opens and parses an ASS subtitle file from disk
    function openSubtitles(filePath, charset) {
        if (!filePath || !subtitleModel) return false;
        var cleanPath = filePath;
        var ok = charset ? subtitleModel.loadFromFileWithCharset(cleanPath, charset)
                         : subtitleModel.loadFromFile(cleanPath);
        if (!ok) {
            statusMessage(qsTr("Failed to open file: ") + cleanPath);
            return false;
        }
        lastFindRow = -1;
        lastFindEnd = 0;
        currentFileName = subtitleModel.fileName;
        selectRow(0, false, false);
        statusMessage(qsTr("Opened subtitles: ") + currentFileName);
        dataModified();
        // Modification tracking has been reset in C++ by loadFromFile()
        return true;
    }

    // Serializes and writes subtitle project to disk
    function saveSubtitles(filePath) {
        if (!subtitleModel) return false;
        var target = filePath || currentFileName;
        if (!target || target === qsTr("Untitled") || target === "Untitled") {
            return false;
        }
        var ok = subtitleModel.saveToFile(target);
        if (ok) {
            currentFileName = subtitleModel.fileName;
            statusMessage(qsTr("File saved: ") + currentFileName);
            return true;
        } else {
            statusMessage(qsTr("Failed to save file: ") + target);
            return false;
        }
    }
}
