// ASS dialogue blocks. Keep override/comment/drawing spans byte-for-byte while
// exposing only plain text spans to the translation assistant.
function blocks(text) {
    var result = [];
    if (!text.length) return [{start: 0, end: 0, text: "", plain: true}];
    var drawing = 0;
    var cur = 0;
    while (cur < text.length) {
        if (text[cur] === "{") {
            var close = text.indexOf("}", cur);
            if (close >= 0) {
                var contents = text.substring(cur + 1, close);
                if (contents.indexOf("\\") >= 0 || !contents.length) {
                    var tags = /\\p(-?\d+)/g;
                    var match;
                    while ((match = tags.exec(contents)) !== null)
                        drawing = Number(match[1]);
                }
                result.push({start: cur, end: close + 1,
                             text: text.substring(cur, close + 1), plain: false});
                cur = close + 1;
                continue;
            }
        }
        var next = text.indexOf("{", cur + 1);
        if (next < 0) next = text.length;
        result.push({start: cur, end: next, text: text.substring(cur, next),
                     plain: drawing === 0});
        cur = next;
    }
    return result;
}

function nextPlain(parts, from, direction) {
    for (var i = from + direction; i >= 0 && i < parts.length; i += direction) {
        if (parts[i].plain && parts[i].text.trim().length)
            return i;
    }
    return -1;
}

function firstPlain(parts) {
    var index = nextPlain(parts, -1, 1);
    if (index >= 0) return index;
    // Empty dialogue lines can be translated by inserting their first text block.
    return parts.length === 1 && parts[0].plain ? 0 : -1;
}

function assText(value) {
    return String(value).replace(/\r\n|\r|\n/g, "\\N");
}

function replaceBlock(text, index, translation) {
    var parts = blocks(text);
    if (index < 0 || index >= parts.length || !parts[index].plain)
        return null;
    return text.substring(0, parts[index].start) + assText(translation)
         + text.substring(parts[index].end);
}
