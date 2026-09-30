// Return the bounding audio interval for the selected subtitle lines.
function range(model, selectedRows) {
    if (!model || !selectedRows || !selectedRows.length) return null;
    var start = Infinity;
    var end = -Infinity;
    for (var i = 0; i < selectedRows.length; ++i) {
        var row = Number(selectedRows[i]);
        if (!Number.isInteger(row) || row < 0 || row >= model.count) continue;
        start = Math.min(start, model.getLineStartMs(row));
        end = Math.max(end, model.getLineEndMs(row));
    }
    if (!Number.isFinite(start) || !Number.isFinite(end) || end <= start)
        return null;
    return {startMs: start, endMs: end};
}
