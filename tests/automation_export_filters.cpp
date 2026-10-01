#include "LuaScript.h"
#include "AutomationManager.h"
#include "FramerateExport.h"
#include "SubtitleModel.h"
#include "AegisubCoreBridge.h"
#include "ScreenColorPicker.h"
#include <QScreen>
#include "audio_clip_ui_stubs.h"
#include "editor_timeline.h"
#include <libaegisub/color.h>
#include <libaegisub/vfr.h>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <cstdio>
#include <QQmlEngine>
#include <QJSEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <qqml.h>
#include <QQmlExpression>
#include <QJSValue>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QImage>
#include <QColor>
#include <QClipboard>
#include <QMimeData>
#include <QFontDatabase>
#include <QKeySequence>
#include <QUrl>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <memory>
#include <functional>

using namespace Automation;

static bool writeScript(const QString &path, const QByteArray &body) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(body) == body.size();
}
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main(int argc, char **argv) {
    QQuickStyle::setStyle("Aegisub");
    QQuickStyle::setFallbackStyle("Fusion");
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QGuiApplication app(argc, argv);
    {
        QImage pixels(8, 6, QImage::Format_RGB32);
        pixels.fill(Qt::black);
        pixels.setPixelColor(5, 3, QColor(19, 71, 203));
        pixels.setPixelColor(7, 5, QColor(201, 41, 13));
        QColor sampled;
        int completions = 0;
        ScreenPickWindow window(app.primaryScreen(), QRect(-20, -10, 4, 3), pixels,
            [&](QColor color) { sampled = color; ++completions; });
        CHECK(window.sample(QPointF(2.5, 1.5)) == QColor(19, 71, 203));
        CHECK(window.sample(QPointF(3.99, 2.99)) == QColor(201, 41, 13));
        CHECK(!window.sample(QPointF(-1, 0)).isValid());
        CHECK(!window.sample(QPointF(4, 0)).isValid());
        QMouseEvent click(QEvent::MouseButtonRelease, QPointF(2.5, 1.5), QPointF(-17.5, -8.5),
            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &click);
        CHECK(completions == 1 && sampled == QColor(19, 71, 203));
        QTest::keyClick(&window, Qt::Key_Escape);
        CHECK(completions == 2 && !sampled.isValid());
        QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier, QPoint(1, 1));
        CHECK(completions == 3 && !sampled.isValid());
        puts("PASS screen picker HiDPI pixels, negative desktop origin, bounds, click, Escape and right-click");
    }
    if (qEnvironmentVariableIsSet("AEGISUB_SCREEN_PICKER_ONLY")) {
        QQuickWindow source;
        source.setColor(QColor(17, 99, 201));
        source.setGeometry(QRect(app.primaryScreen()->availableGeometry().topLeft() + QPoint(60, 60), QSize(200, 160)));
        source.show();
        source.requestActivate();
        QTest::qWait(300);
        AegisubCoreBridge bridge;
        QSignalSpy picked(&bridge, &AegisubCoreBridge::screenColorPicked);
        QSignalSpy cancelled(&bridge, &AegisubCoreBridge::screenColorPickCancelled);
        CHECK(bridge.beginScreenColorPick());
        QTest::qWait(100);
        auto windows = bridge.findChildren<QWindow *>();
        const QPoint target = source.mapToGlobal(QPoint(100, 80));
        QWindow *overlay = nullptr;
        for (auto *window : windows) if (window->geometry().contains(target)) overlay = window;
        CHECK(overlay);
        QTest::mouseClick(overlay, Qt::LeftButton, Qt::NoModifier, overlay->mapFromGlobal(target));
        CHECK(picked.size() == 1 && picked[0][0].value<QColor>() == QColor(17, 99, 201));
        CHECK(cancelled.isEmpty());
        for (auto *window : windows) CHECK(!window->isVisible());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        CHECK(bridge.beginScreenColorPick());
        windows = bridge.findChildren<QWindow *>();
        CHECK(!windows.isEmpty());
        QTest::keyClick(windows.first(), Qt::Key_Escape);
        CHECK(cancelled.size() == 1 && picked.size() == 1);
        puts("PASS native desktop screen capture -> real overlay click -> sampled RGB and Escape cleanup");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QQmlEngine pickerEngine;
        const auto pickerRoot = QDir(QStringLiteral(AUTOMATION_SOURCE_DIR)).absoluteFilePath("../qml");
        pickerEngine.addImportPath(pickerRoot);
        pickerEngine.rootContext()->setContextProperty("aegisubCore", &bridge);
        QQmlComponent component(&pickerEngine, QUrl::fromLocalFile(pickerRoot + "/dialogs/DialogColorPicker.qml"));
        std::unique_ptr<QObject> picker(component.createWithInitialProperties({{"currentColor", QColor(33, 44, 55, 85)}}));
        if (!picker) for (const auto &error : component.errors()) fprintf(stderr, "%s\n", qPrintable(error.toString()));
        CHECK(picker && QMetaObject::invokeMethod(picker.get(), "open"));
        auto *dialogWindow = qobject_cast<QQuickWindow *>(picker.get());
        auto *dropper = picker->findChild<QQuickItem *>("color-picker-dropper");
        CHECK(dialogWindow && dropper);
        QSignalSpy accepted(picker.get(), SIGNAL(colorAccepted(QColor,QString,QString)));
        QTest::qWait(100);
        QTest::mouseClick(dialogWindow, Qt::LeftButton, Qt::NoModifier,
            dropper->mapToScene(QPointF(dropper->width() / 2, dropper->height() / 2)).toPoint());
        CHECK(picker->property("pickingScreen").toBool() && !picker->property("visible").toBool());
        QTest::qWait(200);
        windows = bridge.findChildren<QWindow *>();
        overlay = nullptr;
        for (auto *window : windows) if (window->geometry().contains(target)) overlay = window;
        CHECK(overlay);
        QTest::mouseClick(overlay, Qt::LeftButton, Qt::NoModifier, overlay->mapFromGlobal(target));
        CHECK(picker->property("visible").toBool() && !picker->property("pickingScreen").toBool());
        CHECK(picker->property("currentColor").value<QColor>() == QColor(17, 99, 201, 85));
        CHECK(picker->property("originalColor").value<QColor>() == QColor(33, 44, 55, 85));
        CHECK(picker->property("assAbgrCode") == "&HAAC96311");
        CHECK(accepted.isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QTest::qWait(80);
        const auto screenshot = qEnvironmentVariable("AEGISUB_SCREEN_PICKER_SCREENSHOT");
        if (!screenshot.isEmpty()) CHECK(dialogWindow->grabWindow().save(screenshot));
        QTest::mouseClick(dialogWindow, Qt::LeftButton, Qt::NoModifier,
            dropper->mapToScene(QPointF(dropper->width() / 2, dropper->height() / 2)).toPoint());
        QTest::qWait(200);
        windows = bridge.findChildren<QWindow *>();
        CHECK(!windows.isEmpty());
        QTest::mouseClick(windows.first(), Qt::RightButton, Qt::NoModifier, QPoint(10, 10));
        CHECK(picker->property("visible").toBool() && !picker->property("pickingScreen").toBool());
        CHECK(picker->property("currentColor").value<QColor>() == QColor(17, 99, 201, 85));
        CHECK(accepted.isEmpty());
        CHECK(QMetaObject::invokeMethod(picker.get(), "acceptColor"));
        CHECK(accepted.size() == 1 && accepted[0][0].value<QColor>() == QColor(17, 99, 201, 85));
        puts("PASS native production Dropper mouse button, capture timer, overlay click, RGBA/ASS update, preview, right-click cancellation and final acceptance");
        return 0;
    }
    qmlRegisterType<TestVideoSurface>("Aegisub", 1, 0, "VideoSurface");
    qmlRegisterType<TestVideoSurface>("Aegisub", 1, 0, "SubtitleSurface");
    qmlRegisterType<TestSpectrogramView>("Aegisub", 1, 0, "SpectrogramView");
#ifdef Q_OS_WIN
    // The offscreen plugin does not enumerate Windows system fonts. Supply the
    // fonts that the production Windows platform plugin normally discovers.
    // Include a bold family to exercise the font chooser's inherited style flags.
    const auto fontDirectory = qEnvironmentVariable("WINDIR") + "/Fonts/";
    QFontDatabase::addApplicationFont(fontDirectory + "arial.ttf");
    QFontDatabase::addApplicationFont(fontDirectory + "timesbd.ttf");
#endif
    const QString root = QStringLiteral(AUTOMATION_SOURCE_DIR);
    const QStringList includes{root + "/include"};
    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    {
        QTemporaryDir fixture;
        const auto path = fixture.filePath("multiline.srt");
        CHECK(writeScript(path, "1\n00:00:01,000 --> 00:00:02,000\n<i>one <i>nested</i></i>\ntwo\n\n"));
        SubtitleModel imported;
        CHECK(imported.loadFromFile(path));
        CHECK(imported.get(0).value("text") == "{\\i1}one nested{\\i}\\Ntwo");
        const auto style = imported.styles().first().toMap();
        CHECK(style.value("font") == "Arial" && style.value("size").toDouble() == 48);
        CHECK(style.value("alignment").toInt() == 2 && style.value("marginV").toInt() == 10);
        imported.setScriptInfo({});
        imported.initializeVideoResolution(1280,720);
        CHECK(imported.scriptInfo().value("PlayResX").toInt() == 1280);
        imported.initializeVideoResolution(1920,1080);
        CHECK(imported.scriptInfo().value("PlayResY").toInt() == 720);
        const auto before = imported.getAllLines();
        CHECK(imported.previewAss().contains("{\\i1}one nested{\\i}\\Ntwo"));
        CHECK(imported.getAllLines() == before);
        puts("PASS SRT nested formatting/multiline, original default style and unset-only video resolution initialization");
    }
    {
        SubtitleModel formatting;
        formatting.newDocument();
        for (const auto &tag : {"b", "i", "u", "s"}) {
            formatting.setProperty(0, "text", "word tail");
            formatting.clearUndo();
            auto result = formatting.toggleInlineFormatting({0}, 0, 0, 4, tag);
            CHECK(result.value("success").toBool() && result.value("changed").toBool());
            const QString expected = "{\\" + QString(tag) + "1}word{\\" + QString(tag) + "0} tail";
            CHECK(formatting.get(0).value("text") == expected);
            CHECK(result.value("selectionStart") == 5 && result.value("selectionEnd") == 9);
            result = formatting.toggleInlineFormatting({0}, 0, 5, 9, tag);
            CHECK(formatting.get(0).value("text") == "{\\" + QString(tag) + "0}word{\\" + QString(tag) + "0} tail");
            formatting.undo();
            CHECK(formatting.get(0).value("text") == expected);
            formatting.undo();
            CHECK(formatting.get(0).value("text") == "word tail" && !formatting.canUndo());
            formatting.redo();
            CHECK(formatting.get(0).value("text") == expected);
        }
        formatting.setProperty(0, "text", "{\\b1\\b700\\t(0,100,\\b0)\\bord3}word");
        CHECK(formatting.inlineFormattingState(0, 40).isEmpty());
        const auto prefixSize = formatting.get(0).value("text").toString().indexOf("word");
        CHECK(formatting.inlineFormattingState(0, prefixSize).value("b").toBool());
        auto result = formatting.toggleInlineFormatting({0}, 0, prefixSize, prefixSize, "b");
        CHECK(formatting.get(0).value("text") == "{\\t(0,100,\\b0)\\bord3\\b0}word");
        const int cursor = result.value("selectionStart").toInt();
        result = formatting.toggleInlineFormatting({0}, 0, cursor, cursor, "b");
        CHECK(formatting.get(0).value("text") == "{\\t(0,100,\\b0)\\bord3\\b1}word");
        auto style = formatting.getStyle(0);
        style["bold"] = true;
        formatting.setStyle(0, style);
        formatting.setProperty(0, "text", "{\\t(\\b0)}word");
        CHECK(formatting.inlineFormattingState(0, 10).value("b").toBool());
        auto resetStyle = style;
        resetStyle["name"] = "Reset";
        resetStyle["bold"] = false;
        resetStyle["font"] = "Comic Sans MS";
        resetStyle["size"] = 24.0;
        formatting.addStyle(resetStyle);
        formatting.setProperty(0, "text", "{\\b1}{\\rReset\\i1}word");
        auto state = formatting.inlineFormattingState(0, 20);
        CHECK(!state.value("b").toBool() && state.value("i").toBool() && state.value("font") == "Comic Sans MS" && state.value("size") == 24.0);
        formatting.setAllLines({QVariantMap{{"text",QString::fromUtf8("甲😀乙 tail")}},
            QVariantMap{{"text","{\\i1}four tail"}}, QVariantMap{{"text","untouched"}}});
        formatting.clearUndo();
        result = formatting.toggleInlineFormatting({0,1,1}, 0, 0, 4, "u");
        CHECK(formatting.get(0).value("text") == QString::fromUtf8("{\\u1}甲😀乙{\\u0} tail"));
        CHECK(formatting.get(1).value("text") == "{\\i1\\u1}four{\\u0} tail");
        CHECK(formatting.get(2).value("text") == "untouched");
        CHECK(result.value("selectionStart") == 5 && result.value("selectionEnd") == 9);
        formatting.undo();
        CHECK(!formatting.canUndo() && formatting.get(1).value("text") == "{\\i1}four tail");
        const auto before = formatting.getAllLines();
        CHECK(!formatting.toggleInlineFormatting({0,99}, 0, 0, 4, "b").value("success").toBool());
        CHECK(!formatting.toggleInlineFormatting({0}, 0, 4, 0, "b").value("success").toBool());
        CHECK(!formatting.toggleInlineFormatting({0}, 0, 0, 4, "bord").value("success").toBool());
        CHECK(formatting.getAllLines() == before && !formatting.canUndo());
        formatting.setProperty(0, "text", "{note}{\\p1}m 0 0 l 1 1{\\p0}text");
        result = formatting.toggleInlineFormatting({0}, 0, 15, 15, "i");
        CHECK(formatting.get(0).value("text") == "{note}{\\p1\\i1}m 0 0 l 1 1{\\p0}text");
        formatting.setProperty(0, "text", "word");
        formatting.clearUndo();
        QFont selected("Arial", 48);
        selected.setBold(true);
        CHECK(!formatting.applyInlineFont({0}, 0, 0, 0, selected).value("changed").toBool());
        selected.setFamily("Comic Sans MS");
        selected.setPointSizeF(30.5);
        selected.setUnderline(true);
        selected.setBold(false);
        result = formatting.applyInlineFont({0}, 0, 0, 0, selected);
        CHECK(result.value("success").toBool() && formatting.get(0).value("text") == "{\\fnComic Sans MS\\fs30.5\\b0\\u1}word");
        CHECK(formatting.inlineFormattingState(0, result.value("selectionStart").toInt()).value("u").toBool());
        formatting.undo();
        CHECK(formatting.get(0).value("text") == "word" && !formatting.canUndo());
        selected.setFamily("Bad{font}");
        CHECK(!formatting.applyInlineFont({0}, 0, 0, 0, selected).value("success").toBool() && !formatting.canUndo());
        puts("PASS inline formatting native selection restoration, repeated toggle, duplicate/static/reset tags, UTF16/multiline single undo, drawing preservation, font state, no-op and atomic invalid input");
    }
    {
        const QString folder = temporary.filePath(QString::fromUtf8("备份 50% 空格"));
        const QUrl folderUrl = QUrl::fromLocalFile(folder);
        AegisubCoreBridge bridge;
        CHECK(AegisubCoreBridge::localFilePath(folderUrl) == folder);
        CHECK(AegisubCoreBridge::resolveUserPath(folderUrl.toString()) == folder);
        CHECK(AegisubCoreBridge::resolveUserPath(folder) == folder);
        CHECK(AegisubCoreBridge::localFilePath(QUrl(QStringLiteral("https://example.invalid/backup"))).isEmpty());
        QQmlEngine folderEngine;
        folderEngine.rootContext()->setContextProperty("aegisubCore", &bridge);
        folderEngine.rootContext()->setContextProperty("selectedFolder", folderUrl);
        QQmlExpression folderBinding(folderEngine.rootContext(), nullptr,
            QStringLiteral("aegisubCore.localFilePath(selectedFolder)"));
        CHECK(folderBinding.evaluate().toString() == folder && !folderBinding.hasError());
        puts("PASS FolderDialog QUrl conversion and stored file URI preserve native paths, Unicode, spaces and percent signs");
    }
    {
        SubtitleModel recombine;
        recombine.setAllLines({QVariantMap{{"text","left"},{"start","0:00:01.00"},{"end","0:00:02.00"}},
            QVariantMap{{"text","left\\N right"},{"start","0:00:01.50"},{"end","0:00:02.50"}},
            QVariantMap{{"text","right"},{"start","0:00:02.00"},{"end","0:00:03.00"}},
            QVariantMap{{"text","untouched"},{"start","0:00:04.00"},{"end","0:00:05.00"}}});
        recombine.clearUndo();
        const auto original = recombine.getAllLines();
        CHECK(!recombine.recombineSelectedLines({0,3},0).value("changed").toBool() && !recombine.canUndo());
        CHECK(!recombine.recombineSelectedLines({0,99},0).value("success").toBool());
        CHECK(recombine.getAllLines() == original);
        const auto combined = recombine.recombineSelectedLines({0,1,2},1);
        CHECK(combined.value("success").toBool() && combined.value("changed").toBool());
        CHECK(combined.value("selectedIndices").toList() == QVariantList({0,1}));
        CHECK(combined.value("selectedIndex").toInt() == 0);
        CHECK(recombine.rowCount() == 3 && recombine.get(0).value("text") == "left");
        CHECK(recombine.get(1).value("text") == "right" && recombine.getLineStartMs(1) == 1000);
        CHECK(recombine.getLineEndMs(1) == 3000 && recombine.get(2).value("text") == "untouched");
        CHECK(recombine.undo().value("selectedIndices").toList() == QVariantList({0,1,2}));
        CHECK(recombine.getAllLines() == original);
        recombine.redo();
        CHECK(recombine.rowCount() == 3 && recombine.get(1).value("text") == "right");
        const std::vector<std::pair<QString, QString>> overlaps = {
            {"A", "A\\NB"}, {"A", "B\\NA"}, {"A\\NB", "B"}, {"B\\NA", "B"}};
        for (const auto &[first, second] : overlaps) {
            SubtitleModel pair;
            pair.setAllLines({QVariantMap{{"text",first},{"start","0:00:01.00"},{"end","0:00:02.00"}},
                QVariantMap{{"text",second},{"start","0:00:01.50"},{"end","0:00:03.00"}}});
            pair.clearUndo();
            CHECK(pair.recombineSelectedLines({0,1},0).value("changed").toBool());
            CHECK(pair.rowCount() == 2);
            CHECK(pair.get(first.size() > second.size() ? 0 : 1).value("text") == (first == "A" ? "B" : "A"));
            CHECK(pair.getLineStartMs(first.size() > second.size() ? 0 : 1) == 1000);
            CHECK(pair.getLineEndMs(first.size() > second.size() ? 0 : 1) == 3000);
        }
        puts("PASS recombine overlap text, duplicated fragment deletion, expanded times, unchanged unrelated rows, selection and one undo/redo");
    }
    {
        SubtitleModel timing;
        const auto first = QVariantMap{{"text","previous"},{"style","Other"},{"start","0:00:00.00"},{"end","0:00:01.00"}};
        const auto second = QVariantMap{{"text","target"},{"style","Default"},{"start","0:00:01.10"},{"end","0:00:02.00"}};
        timing.setAllLines({first, second});
        timing.clearUndo();
        const auto source = timing.getAllLines();
        QVariantMap options{{"leadIn",0},{"leadOut",0},{"adjacentEnabled",true},
            {"maxGap",200},{"maxOverlap",100},{"bias",0.5},{"selectedOnly",true},
            {"allowedStyles",QStringList{"Default","Other"}}};
        CHECK(!timing.processTiming(options,{1},1).value("changed").toBool());
        CHECK(timing.getAllLines() == source && !timing.canUndo());
        options["selectedOnly"] = false;
        options["allowedStyles"] = QStringList{"Default"};
        CHECK(!timing.processTiming(options,{},0).value("changed").toBool());
        options["allowedStyles"] = QStringList{};
        CHECK(!timing.processTiming(options,{},0).value("changed").toBool());
        CHECK(timing.getAllLines() == source && !timing.canUndo());
        options["allowedStyles"] = QStringList{"Default","Other"};
        const auto gapResult = timing.processTiming(options,{0,1},1);
        CHECK(gapResult.value("success").toBool() && gapResult.value("modifiedCount").toInt() == 2);
        CHECK(timing.getLineEndMs(0) == 1050 && timing.getLineStartMs(1) == 1050);
        CHECK(timing.undo().value("selectedIndex").toInt() == 1 && timing.getAllLines() == source);
        timing.setAllLines({QVariantMap{{"text","comment"},{"isComment",true},{"start","0:00:00.00"},{"end","0:00:01.00"}}, second});
        timing.clearUndo();
        const auto comments = timing.getAllLines();
        CHECK(!timing.processTiming(options,{0,1},1).value("changed").toBool());
        CHECK(timing.getAllLines() == comments && !timing.canUndo());
        timing.setAllLines({QVariantMap{{"text","one"},{"start","0:00:00.00"},{"end","0:00:01.00"}},
            QVariantMap{{"text","two"},{"start","0:00:00.90"},{"end","0:00:02.00"}}});
        timing.clearUndo();
        CHECK(timing.processTiming(options,{0,1},0).value("modifiedCount").toInt() == 2);
        CHECK(timing.getLineEndMs(0) == 950 && timing.getLineStartMs(1) == 950);
        timing.undo();
        timing.setAllLines({QVariantMap{{"text","vfr"},{"start","0:00:00.09"},{"end","0:00:00.50"}}});
        timing.clearUndo();
        const QVariantList timecodes{0,40,90,150,220,300,390,500};
        options = {{"leadIn",0},{"leadOut",0},{"adjacentEnabled",false},
            {"allowedStyles",QStringList{"Default"}},{"keyframesEnabled",true},
            {"keyframes",QVariantList{3}},{"frameCount",8},
            {"framerate",QVariantMap{{"available",true},{"isVfr",true},{"timecodes",timecodes}}},
            {"beforeStart",1000},{"afterStart",0},{"beforeEnd",0},{"afterEnd",0}};
        const agi::vfr::Framerate vfr(std::vector<int>{0,40,90,150,220,300,390,500});
        const auto keyResult = timing.processTiming(options,{0},0);
        CHECK(keyResult.value("changed").toBool());
        CHECK(timing.getLineStartMs(0) == vfr.TimeAtFrame(3,agi::vfr::START));
        timing.undo();
        CHECK(timing.getLineStartMs(0) == 90);
        timing.setAllLines({QVariantMap{{"text","vfr end"},{"start","0:00:00.00"},{"end","0:00:00.21"}}});
        timing.clearUndo();
        options["beforeStart"] = 0;
        options["beforeEnd"] = 1000;
        options["afterEnd"] = 1000;
        const int endFrame = vfr.FrameAtTime(210, agi::vfr::END);
        const int nearest = std::abs(3 - endFrame) <= std::abs(7 - endFrame) ? 3 : 7;
        const int expectedEnd = vfr.TimeAtFrame(nearest - 1, agi::vfr::END);
        CHECK(expectedEnd != 210);
        CHECK(timing.processTiming(options,{0},0).value("changed").toBool());
        CHECK(timing.getLineEndMs(0) == expectedEnd && timing.getLineStartMs(0) == 0);
        timing.undo();
        CHECK(timing.getLineEndMs(0) == 210);
        timing.setAllLines({QVariantMap{{"text","invalid"},{"start","0:00:02.00"},{"end","0:00:01.00"}}});
        timing.clearUndo();
        const auto invalidLine = timing.getAllLines();
        options = {{"leadIn",100},{"allowedStyles",QStringList{"Default"}}};
        CHECK(!timing.processTiming(options,{0},0).value("success").toBool());
        CHECK(timing.getAllLines() == invalidLine && !timing.canUndo());
        puts("PASS timing post-processor target filtering, no-op undo, gap/overlap, comments, selected scope and VFR keyframe snapping");
    }
    const auto path = temporary.filePath("filters.lua");
    CHECK(writeScript(path, R"lua(
script_name = 'Export test'
local function process(subs, config)
    assert(type(config) == 'table')
    assert(config.enabled == true and config.count == 2.5)
    assert(config.text == '中' .. string.char(0) .. '文')
    local retained_is_dead = true
    if saved then
        local result = saved[1]
        retained_is_dead = result == nil
    end
    assert(retained_is_dead, 'config retained live subtitle bridge')
    for i, line in ipairs(subs) do
        if line.class == 'dialogue' then
            if line.text ~= 'Original' then assert(line.text == config.text, 'subtitle read truncated NUL') end
            line.text = config.text; subs[i] = line
        end
    end
    saved = subs
end
local function configure(subs, stored)
    assert(type(stored) == 'table')
    assert(stored.enabled == true)
    local line = subs[1]
    assert(not pcall(function() subs[1] = line end), 'config can mutate document')
    saved = subs
    return {
        {class='edit', name='text', text=stored.text, x=0, y=0, width=2, height=1},
        {class='dropdown', name='choice', items={'甲','乙'}, value='乙', x=0,y=1},
        {class='checkbox',name='enabled',value=true,x=0,y=2},
        {class='floatedit',name='count',value=2.5,x=0,y=3}
    }
end
aegisub.register_filter('Configured', 'Description', 20, process, configure)
aegisub.register_filter('Fail', '', 5, function(subs)
    local line=subs[1]; line.text='MUST_NOT_LEAK';subs[1]=line
    error('intentional export failure')
end)
aegisub.register_filter('Cycle', '', 0, function() end, function() local t={};t[1]=t;return t end)
aegisub.register_filter('Sparse', '', 0, function() end, function() return {[2]={class='edit'}} end)
aegisub.register_filter('Bad descriptor', '', 0, function() end, function() return {function() end} end)
aegisub.register_filter('Config error', '', 0, function() end, function() error('intentional config failure') end)
local invalid = {
    function() aegisub.register_filter(false,'',0,function() end) end,
    function() aegisub.register_filter('x',{},0,function() end) end,
    function() aegisub.register_filter('x','',math.huge,function() end) end,
    function() aegisub.register_filter('x','',0.5,function() end) end,
    function() aegisub.register_filter('x','',2^40,function() end) end,
    function() aegisub.register_filter('x','','0',function() end) end,
    function() aegisub.register_filter('x','',0,false) end,
    function() aegisub.register_filter('x','',0,function() end,{}) end
}
for _, fn in ipairs(invalid) do assert(not pcall(fn), 'invalid registration accepted') end
)lua"));
    LuaScript script(path, includes);
    CHECK(script.load());
    CHECK(script.filters().size() == 6);
    const auto first = script.filters().front();
    CHECK(first.name == "Configured" && first.description == "Description" && first.priority == 20);
    CHECK(first.runRef != LUA_NOREF && first.configRef != LUA_NOREF);
    AssEntryData dialogue;
    dialogue.entryClass = AssEntryClass::Dialogue;
    dialogue.text = "Original";
    std::vector<AssEntryData> lines{dialogue};
    LuaAssFileBridge source(nullptr, lines);
    const auto binaryText = QString::fromUtf8("中") + QChar(0) + QString::fromUtf8("文");
    const QVariantMap settings{{"enabled", true}, {"count", 2.5}, {"text", binaryText}};
    QString error;
    QVariantList controls;
    CHECK(script.filterConfig(first.id, source, settings, controls, error));
    CHECK(error.isEmpty() && controls.size() == 4);
    CHECK(controls[0].toMap().value("text").toString() == binaryText);
    CHECK(controls[1].toMap().value("items").toList() == QVariantList({QStringLiteral("甲"), QStringLiteral("乙")}));
    CHECK(controls[2].toMap().value("value").typeId() == QMetaType::Bool);
    CHECK(source.getLines().front().text == "Original" && !source.isModified());
    for (int i = 0; i < 100; ++i) {
        const bool ran = script.runFilter(first.id, lines, 1920, 1080, settings, error);
        if (!ran) fprintf(stderr, "Export filter callback failed: %s\n", qPrintable(error));
        CHECK(ran);
        CHECK(lines.front().text == binaryText);
        CHECK(!script.runFilter(script.filters()[1].id, lines, 1920, 1080, {}, error));
        CHECK(error.contains("intentional export failure") && lines.front().text == binaryText);
        for (int index = 2; index < 6; ++index) {
            const auto old = controls;
            CHECK(!script.filterConfig(script.filters()[index].id, source, settings, controls, error));
            CHECK(!error.isEmpty() && controls == old);
        }
        CHECK(script.filterConfig(first.id, source, settings, controls, error));
    }
    CHECK(source.getLines().front().text == "Original");
    CHECK(!script.runFilter(first.id, lines, 1920, 1080, {{"unsupported", QVariantList{1}}}, error));
    CHECK(lines.front().text == binaryText && error.contains("Unsupported"));
    CHECK(!script.runFilter(-1, lines, 1920, 1080, settings, error));
    script.reload();
    CHECK(script.isLoaded() && script.filters().size() == 6 && script.filters().front().id != first.id);
    CHECK(!script.runFilter(first.id, lines, 1920, 1080, settings, error));
    CHECK(writeScript(path, "aegisub.register_filter('partial','',0,function() end)\nerror('load failed')"));
    CHECK(!script.load() && script.filters().empty());
    CHECK(!script.runFilter(first.id, lines, 1920, 1080, settings, error));

    // Execute the unchanged bundled upstream filter, not an equivalent test implementation.
    LuaScript clean(root + "/autoload/cleantags-autoload.lua", includes);
    CHECK(clean.load() && clean.filters().size() == 1 && clean.macros().size() == 1);
    dialogue.text = "{\\b1}{\\i1}hello";
    auto comment = dialogue;
    comment.comment = true;
    lines = {dialogue, comment};
    CHECK(clean.filterConfig(clean.filters().front().id, source, {}, controls, error) && controls.isEmpty());
    CHECK(clean.runFilter(clean.filters().front().id, lines, 1920, 1080, {}, error));
    CHECK(lines[0].text == "{\\b1\\i1}hello" && lines[1].text == comment.text);

    LuaScript karaoke(root + "/autoload/kara-templater.lua", includes);
    CHECK(karaoke.load() && karaoke.filters().size() == 1);
    CHECK(karaoke.filters().front().name == "Karaoke template" && karaoke.filters().front().priority == 2000);
    AssEntryData style;
    style.entryClass = AssEntryClass::Style;
    style.section = "[V4+ Styles]";
    AssEntryData templateLine = dialogue;
    templateLine.comment = true;
    templateLine.effect = "template line";
    templateLine.text = "{\\pos(10,20)}";
    dialogue.comment = false;
    dialogue.effect.clear();
    dialogue.text = "{\\k20}hello";
    dialogue.endTime = 1000;
    lines = {{AssEntryClass::Info, "[Script Info]", "PlayResX", "1920"},
             {AssEntryClass::Info, "[Script Info]", "PlayResY", "1080"}, style, templateLine, dialogue};
    CHECK(karaoke.runFilter(karaoke.filters().front().id, lines, 1920, 1080, {}, error));
    std::vector<AssEntryData> generatedDialogues;
    for (const auto &line : lines) if (line.entryClass == AssEntryClass::Dialogue) generatedDialogues.push_back(line);
    CHECK(generatedDialogues.size() == 3);
    CHECK(generatedDialogues[0].comment && generatedDialogues[0].effect == "template line");
    CHECK(generatedDialogues[1].comment && generatedDialogues[1].effect == "karaoke");
    CHECK(!generatedDialogues[2].comment && generatedDialogues[2].effect == "fx" && generatedDialogues[2].text == "{\\pos(10,20)}hello");

    AutomationManager manager;
    {
        SubtitleModel renamed;
        auto sourceStyle = renamed.getStyle(0);
        sourceStyle["name"] = "Old";
        auto otherStyle = sourceStyle;
        otherStyle["name"] = "Other";
        renamed.setStyles({renamed.getStyle(0), sourceStyle, otherStyle});
        renamed.setAllLines({QVariantMap{{"style","Old"},{"text","{\\rOld\\pos(10,20)}a{\\t(0,100,\\rOld\\fs20)}"}},
            QVariantMap{{"style","Other"},{"text","{\\rOld}b{comment}"},{"isComment",true}},
            QVariantMap{{"style","OLD"},{"text","{\\rOLD}c"}}});
        renamed.clearUndo();
        const auto beforeLines = renamed.getAllLines();
        const auto beforeStyles = renamed.styles();
        CHECK(renamed.styleEditPlan(1,sourceStyle).value("success").toBool());
        CHECK(!renamed.styleEditPlan(1,sourceStyle).value("changed").toBool());
        CHECK(!renamed.editStyle(1,sourceStyle,true).value("changed").toBool() && !renamed.canUndo());
        auto target = sourceStyle;
        target["name"] = "New";
        CHECK(renamed.styleEditPlan(1,target).value("needsConfirmation").toBool());
        for (const auto &bad : {QString("Other"),QString("other"),QString(""),QString("Bad,Name"),QString("Bad\nName")}) {
            target["name"] = bad;
            CHECK(!renamed.styleEditPlan(1,target).value("success").toBool());
            CHECK(!renamed.editStyle(1,target,true).value("success").toBool());
            CHECK(renamed.getAllLines() == beforeLines && renamed.styles() == beforeStyles && !renamed.canUndo());
        }
        target["name"] = "New";
        CHECK(renamed.editStyle(1,target,false,2,{0,2}).value("success").toBool());
        CHECK(renamed.get(0).value("style") == "Old" && renamed.get(0).value("text") == beforeLines[0].toMap().value("text"));
        CHECK(renamed.getStyle(1).value("name") == "New" && renamed.canUndo());
        CHECK(renamed.undo().value("selectedIndex").toInt() == 2);
        CHECK(renamed.getAllLines() == beforeLines && renamed.styles() == beforeStyles);
        CHECK(renamed.editStyle(1,target,true,2,{0,2}).value("success").toBool());
        CHECK(renamed.get(0).value("style") == "New" && renamed.get(0).value("text") == "{\\rNew\\pos(10,20)}a{\\t(0,100,\\rNew\\fs20)}");
        CHECK(renamed.get(1).value("style") == "Other" && renamed.get(1).value("text") == "{\\rNew}b{comment}");
        CHECK(renamed.get(2).value("style") == "New" && renamed.get(2).value("text") == "{\\rNew}c");
        CHECK(renamed.undo().value("selectedIndices").toList() == QVariantList({0,2}));
        CHECK(renamed.getAllLines() == beforeLines && renamed.styles() == beforeStyles);
        renamed.redo();
        CHECK(renamed.get(0).value("style") == "New" && renamed.getStyle(1).value("name") == "New");
        puts("PASS native style rename: case-folded conflict/reference matching, direct and nested reset references, comments, yes/no choice, one undo/redo and no-op/invalid isolation");
    }
    {
        const auto parserPath = temporary.filePath("karaoke-parser.lua");
        CHECK(writeScript(parserPath, R"lua(
aegisub.register_filter('Karaoke parser contract','',0,function(subs)
    for _, line in ipairs(subs) do
        if line.class == 'dialogue' then
            local unicode = '中' .. string.char(0) .. '尾'
            line.text = '{\\pos(100,100)\\k10}a{\\K10}' .. unicode
            line.start_time = 1000; line.end_time = 5000
            local k = aegisub.parse_karaoke_data(line)
            assert(k[0].text == '' and #k == 2)
            assert(k[1].text == '{\\pos(100,100)}a' and k[1].duration == 100)
            assert(k[2].text == unicode and k[2].text_stripped == unicode)
            assert(k[2].tag == '\\kf' and k[2].duration == 100 and k[2].start_time == 100 and k[2].end_time == 200)
            return
        end
    end
    error('missing dialogue')
end)
)lua"));
        LuaScript parserFixture(parserPath, includes);
        CHECK(parserFixture.load());
        auto parsedLines = lines;
        CHECK(parserFixture.runFilter(parserFixture.filters()[0].id, parsedLines,1920,1080,{},error));
        puts("PASS actual Lua parse_karaoke_data preserves non-k tags and Unicode/NUL, K alias and raw unnormalized durations relative to line start");
    }
    {
        const auto syllables = AegisubCoreBridge::parseKaraokeLine("{\\pos(100,100)\\k10}a{\\kf10\\i1}b{comment}{\\t(0,100,\\fs20)}", 1000, 1500, false);
        CHECK(syllables.size() == 2);
        CHECK(syllables[0].value("textWithTags") == "{\\pos(100,100)}a" && syllables[0].value("duration").toInt() == 100);
        CHECK(syllables[1].value("textWithTags") == "{\\i1}b{comment}{\\t(0,100,\\fs20)}" && syllables[1].value("duration").toInt() == 400);
        CHECK(syllables[1].value("startTime").toInt() == 1100 && syllables[1].value("endTime").toInt() == 1500);
        const auto unnormalized = AegisubCoreBridge::parseKaraokeLine("{\\k10}a{\\K10}b", 1000, 1500, false, false);
        CHECK(unnormalized[1].value("duration").toInt() == 100 && unnormalized[1].value("tagType") == "\\kf");
        const auto draws = AegisubCoreBridge::parseKaraokeLine("{\\k10\\p1}m 0 0 l 10 20{\\p0}字{\\k20}文", 0, 300, false);
        CHECK(draws.size() == 2 && draws[0].value("text") == QString::fromUtf8("字"));
        CHECK(draws[0].value("textWithTags") == QString::fromUtf8("{\\p1}m 0 0 l 10 20{\\p0}字"));
        const auto clipped = AegisubCoreBridge::parseKaraokeLine("{\\k10}a{\\k20}b{\\k30}c", 1000, 1150, false);
        CHECK(clipped.size() == 3 && clipped[1].value("duration").toInt() == 50 && clipped[2].value("duration").toInt() == 0 && clipped[2].value("startTime").toInt() == 1150);
        SubtitleModel split;
        split.setAllLines({QVariantMap{{"text", "{\\pos(100,100)\\k10}a{\\k10\\c&HFF&}b"}, {"start", "0:00:01.00"}, {"end", "0:00:01.50"}, {"style", "Custom"}, {"actor", "Singer"}, {"layer",3}, {"marginLeft",5}},
            QVariantMap{{"text","untouched"}}, QVariantMap{{"text","{\\ko10}x{\\K10}y"},{"isComment",true}}});
        const auto original = split.getAllLines();
        split.clearUndo();
        const auto result = split.splitSelectedByKaraoke({2,0,0,-1,999}, 0);
        CHECK(result.value("success").toBool() && result.value("changed").toBool());
        CHECK(result.value("selectedIndices").toList() == QVariantList({0,1,3,4}) && result.value("activeIndex").toInt() == 0);
        CHECK(split.rowCount() == 5 && split.get(0).value("text") == "{\\pos(100,100)}a" && split.get(1).value("text") == "{\\c&HFF&}b");
        CHECK(split.get(0).value("style") == "Custom" && split.get(1).value("actor") == "Singer" && split.get(1).value("layer").toInt() == 3 && split.get(1).value("marginLeft").toInt() == 5);
        CHECK(split.getLineStartMs(0) == 1000 && split.getLineEndMs(0) == 1100 && split.getLineStartMs(1) == 1100 && split.getLineEndMs(1) == 1500);
        CHECK(split.get(2).value("text") == "untouched" && split.get(3).value("isComment").toBool());
        CHECK(split.canUndo());
        split.undo();
        CHECK(split.getAllLines() == original && !split.canUndo());
        split.redo();
        CHECK(split.rowCount() == 5);
        split.undo();
        const auto noChange = split.splitSelectedByKaraoke({1},1);
        CHECK(noChange.value("success").toBool() && !noChange.value("changed").toBool() && !split.canUndo());
        split.setProperty(2,"text","{\\k2147483647}overflow");
        const auto beforeFailure = split.getAllLines();
        CHECK(!split.splitSelectedByKaraoke({0,2}).value("success").toBool());
        CHECK(split.getAllLines() == beforeFailure && !split.canUndo());
        puts("PASS karaoke parsing and split: non-k tags/comments/drawings preserved, normalized and Lua raw timings, case/K variants, multi-row selection, metadata/timing, single undo/redo, no-op and overflow atomicity");
    }
    {
        SubtitleModel resampled;
        auto resampleStyle = resampled.styles().first().toMap();
        resampleStyle["size"] = 20.0;
        resampled.setStyles({resampleStyle});
        resampled.setScriptInfo({{"PlayResX", 1920}, {"PlayResY", 1080}, {"LayoutResX", 1920}, {"LayoutResY", 1080}});
        auto row = resampled.get(0);
        row["text"] = "{\\pos(100,200)\\move(10,20,30,40,100,200)\\clip(0,0,100,200)\\t(100,200,\\fs30)\\bord2\\p1}m 0 0 l 100 200{\\p0}text";
        row["marginLeft"] = 20;
        row["marginRight"] = 0;
        row["marginVert"] = 10;
        auto templ = row;
        templ["isComment"] = true;
        templ["effect"] = "template line";
        resampled.setAllLines({row, templ});
        const auto beforeLines = resampled.getAllLines();
        const auto beforeStyles = resampled.styles();
        const auto beforeInfo = resampled.scriptInfo();
        resampled.clearUndo();
        const QVariantMap half{{"sourceX",1920},{"sourceY",1080},{"destX",960},{"destY",540}};
        CHECK(resampled.resampleResolution(half, 1, {0, 1}).value("success").toBool());
        CHECK(resampled.get(0).value("text") == "{\\pos(50,100)\\move(5,10,15,20,100,200)\\clip(0,0,50,100)\\t(100,200,\\fs15)\\bord1\\p1}m 0 0 l 50 100{\\p0}text");
        CHECK(resampled.get(0).value("marginLeft").toInt() == 10 && resampled.get(0).value("marginRight").toInt() == 0 && resampled.get(0).value("marginVert").toInt() == 5);
        CHECK(resampled.get(1) == beforeLines[1].toMap());
        CHECK(resampled.styles()[0].toMap().value("size").toInt() == 10);
        CHECK(resampled.styles()[0].toMap().value("outlineWidth").toDouble() == 1);
        CHECK(resampled.scriptInfo().value("LayoutResX").toInt() == 1920 && resampled.scriptInfo().value("PlayResX").toInt() == 960);
        CHECK(resampled.isModified() && resampled.canUndo());
        CHECK(resampled.undo().value("selectedIndex").toInt() == 1);
        CHECK(resampled.getAllLines() == beforeLines && resampled.styles() == beforeStyles && resampled.scriptInfo() == beforeInfo);
        resampled.redo();
        CHECK(resampled.get(0).value("marginLeft").toInt() == 10);
        resampled.undo();
        CHECK(resampled.resampleResolution({{"sourceX",1920},{"sourceY",1080},{"destX",1280},{"destY",720}}).value("success").toBool());
        CHECK(resampled.get(0).value("text").toString().startsWith("{\\pos(66.667,133.333)"));
        CHECK(resampled.getStyle(0).value("size").toInt() == 13 && resampled.get(0).value("marginLeft").toInt() == 13);
        CHECK(resampled.scriptInfo().value("PlayResX").toInt() == 1280 && resampled.scriptInfo().value("PlayResY").toInt() == 720);
        resampled.undo();
        resampled.setAllLines({QVariantMap{{"text", "{\\pos(100,200)\\fscx100\\clip(m 0 0 l 100 200)\\p1}m 0 0 l 100 200"}}});
        QVariantMap aspect{{"sourceX",640},{"sourceY",480},{"destX",1280},{"destY",720},{"mode",0}};
        CHECK(resampled.resampleResolution(aspect).value("success").toBool());
        CHECK(resampled.get(0).value("text") == "{\\pos(200,300)\\fscx133.333\\clip(m 0 0 l 200 300)\\p1}m 0 0 l 150 300");
        resampled.undo();
        aspect["mode"] = 1;
        CHECK(resampled.resampleResolution(aspect).value("success").toBool());
        CHECK(resampled.get(0).value("text").toString().startsWith("{\\pos(309.484,300)\\fscx100"));
        resampled.undo();
        aspect["mode"] = 2;
        CHECK(resampled.resampleResolution(aspect).value("success").toBool());
        CHECK(resampled.get(0).value("text").toString().startsWith("{\\pos(200,280)\\fscx100"));
        resampled.undo();
        const auto unchanged = resampled.getAllLines();
        aspect["mode"] = 3;
        aspect["left"] = -640;
        CHECK(!resampled.resampleResolution(aspect).value("success").toBool());
        CHECK(resampled.getAllLines() == unchanged);
        aspect["sourceX"] = 0;
        CHECK(!resampled.resampleResolution(aspect).value("success").toBool());
        CHECK(resampled.getAllLines() == unchanged);
        resampled.setProperty(0, "text", "{\\pos(1e999,0)}x");
        const auto overflowSource = resampled.getAllLines();
        CHECK(!resampled.resampleResolution(half).value("success").toBool());
        CHECK(resampled.getAllLines() == overflowSource);
        SubtitleModel manual;
        manual.setScriptInfo({{"PlayResX",640},{"PlayResY",480},{"YCbCr Matrix","TV.601"}});
        manual.setAllLines({QVariantMap{{"text", "{\\pos(100,200)\\1c&H0000FF&\\p1}m 0 0 l 100 200"}}});
        auto manualStyle = manual.styles()[0].toMap();
        manualStyle["primary"] = "&H800000FF";
        manual.setStyles({manualStyle});
        const QVariantMap manualSettings{{"sourceX",640},{"sourceY",480},{"destX",1280},{"destY",720},
            {"mode",3},{"left",80},{"right",80},{"sourceMatrix","TV.601"},{"destMatrix","TV.709"}};
        CHECK(manual.resampleResolution(manualSettings).value("success").toBool());
        // Independent BT.601 -> BT.709 equations send RGB(255,0,0) to
        // (277.031,24.65,-3.598), rounded/clamped to (255,25,0).
        CHECK(manual.get(0).value("text") == "{\\pos(288,300)\\1c&H0019FF&\\p1}m 0 0 l 150 300");
        CHECK(manual.styles()[0].toMap().value("primary") == "&H800019FF");
        CHECK(manual.styles()[0].toMap().value("marginL").toInt() == 144);
        CHECK(manual.scriptInfo().value("YCbCr Matrix") == "TV.709");
        manual.undo();
        auto invalidMatrix = manualSettings;
        invalidMatrix["destMatrix"] = "None";
        CHECK(!manual.resampleResolution(invalidMatrix).value("success").toBool());
        CHECK(manual.getStyle(0).value("primary") == "&H800000FF");
        SubtitleModel edges;
        edges.setScriptInfo({{"PlayResX",640},{"PlayResY",480},{"LayoutResX",640},{"LayoutResY",480}});
        edges.setAllLines({QVariantMap{{"text", "{\\org(-5,3)\\clip(-5,-3,5,3)\\xbord4\\ybord6\\xshad8\\yshad10\\fsp2.5\\fscy120\\fs+2\\blur2\\pbo5}x{comment}{unclosed"}}});
        auto edgeStyle = edges.styles()[0].toMap();
        edgeStyle["size"] = 23.5;
        edgeStyle["spacing"] = 2.5;
        edgeStyle["shadowDepth"] = 3.5;
        edgeStyle["scaleY"] = 120.0;
        edges.setStyles({edgeStyle});
        CHECK(edges.resampleResolution({{"sourceX",640},{"sourceY",480},{"destX",320},{"destY",240}}).value("success").toBool());
        CHECK(edges.get(0).value("text") == "{\\org(-2.5,1.5)\\clip(-2,-1,3,2)\\xbord2\\ybord3\\xshad4\\yshad5\\fsp1.25\\fscy120\\fs+2\\blur2\\pbo3}x{comment}{unclosed");
        CHECK(edges.getStyle(0).value("size").toInt() == 12);
        CHECK(edges.getStyle(0).value("spacing").toDouble() == 1.25 && edges.getStyle(0).value("shadowDepth").toDouble() == 1.75);
        CHECK(edges.getStyle(0).value("scaleY").toDouble() == 120);
        edges.undo();
        CHECK(edges.resampleResolution({{"sourceX",640},{"sourceY",480},{"destX",1280},{"destY",720},{"mode",3},{"top",80},{"bottom",80}}).value("success").toBool());
        CHECK(edges.scriptInfo().value("LayoutResY").toInt() == 600 && edges.scriptInfo().value("LayoutResX").toInt() == 1067);
        puts("PASS native resolution resampling: styles, margins, nested/rect/vector/drawing tags, template isolation, stretch/borders/crop, layout metadata, undo/redo and atomic invalid/overflow failure");
    }
    // Expected values are the upstream color parser fixtures, in Lua's ASS
    // transparency convention, plus existing short ASS/8-digit hex roundtrips.
    const std::pair<const char *, const char *> colorFixtures[] = {
        {"#FFF", "#FFFFFF00"}, {"#102030", "#10203000"},
        {"rgb(16, 32, 48)", "#10203000"}, {"rgba(16,32,48,64)", "#10203040"},
        {" rgba( 16 , 32 , 48 , 64 ) ", "#10203040"},
        {"&H102030&", "#30201000"}, {"&H01020304&", "#04030201"},
        {"h01020304", "#04030201"}, {"&102030", "#30201000"},
        {"1056816", "#30201000"}, {"+1056816", "#30201000"},
        {"16711807", "#7F00FF00"}, {"-1", "#FFFFFFFF"},
        {"&HFF&", "#FF000000"}, {"#11223380", "#11223380"}
    };
    for (const auto &[input, expected] : colorFixtures) {
        const auto result = manager.normalizeConfigColor(QString::fromLatin1(input), true);
        CHECK(result.value("success").toBool() && result.value("value") == QString::fromLatin1(expected));
        CHECK(manager.normalizeConfigColor(QString::fromLatin1(input), false).value("value") == QString::fromLatin1(expected).left(7));
    }
    for (const auto input : {"", "red", "#GGGGGG", "#1234", "rgb(256,0,0)", "rgb(-1,0,0)",
                             "rgb(1x,2,3)", "rgba(1,2,3)", "rgb(1,2,3,4)", "rgb(0000,0,0)",
                             "rgba(1,2,3,0.5)", "4294967296", "-2147483649", "&H123456789&"}) {
        agi::Color unchanged(11, 22, 33, 44);
        CHECK(!agi::Color::TryParse(unchanged, input));
        CHECK(unchanged == agi::Color(11, 22, 33, 44));
        CHECK(!manager.normalizeConfigColor(QString::fromLatin1(input), true).value("success").toBool());
    }
    CHECK(!manager.normalizeConfigColor(QString::fromLatin1("#102030\0tail", 12), true).value("success").toBool());
    while (!manager.scripts().isEmpty()) manager.removeScript(0);
    manager.addIncludePath(root + "/include");
    CHECK(writeScript(path, R"lua(
aegisub.register_filter('Low','low',0,function() end)
aegisub.register_filter('High','high',2000,function() end,function() return {} end)
aegisub.register_filter('Equal','equal',2000,function() end)
)lua"));
    CHECK(manager.addScript(path));
    auto filters = manager.filters();
    CHECK(filters.size() == 5);
    CHECK(filters[0].toMap().value("name") == "High" && filters[1].toMap().value("name") == "Equal" && filters[2].toMap().value("name") == "Transform Framerate" && filters[3].toMap().value("name") == "Low");
    CHECK(filters[0].toMap().value("hasConfig").toBool() && !filters[1].toMap().value("hasConfig").toBool());
    const auto oldId = filters[0].toMap().value("id");
    manager.reloadScript(0);
    CHECK(manager.filters().size() == 5 && manager.filters()[0].toMap().value("id") != oldId);
    manager.removeScript(0);
    CHECK(manager.filters().size() == 2 && manager.filters()[1].toMap().value("name") == "Fix Styles");

    SubtitleModel frameEditor;
    const auto rateConfig = manager.exportFilterConfig(-2, &frameEditor, {});
    CHECK(rateConfig.value("success").toBool() && rateConfig.value("controls").toList().size() == 7);
    CHECK(!rateConfig.value("controls").toList()[2].toMap().value("enabled").toBool());
    SubtitleLine timed;
    timed.setStartMs(1000); timed.setEndMs(2000);
    timed.text = QStringLiteral(R"({\fad(100,200)\fade(1,2,3,100,200,300,400)\move(1,2,3,4,100,200)\t(100,200,\fs20)\t(2,\bord3)\t(100,200,2,\blur1)\k10\K11\kf12\ko13}Text{comment}{\p1}m 0 0{\p0}{\unknown(1,2)})");
    auto timedComment = timed; timedComment.isComment = true;
    frameEditor.setRawLines({timed, timedComment});
    const auto frameBefore = frameEditor.getAllLines();
    const auto framePath = temporary.filePath("frame-export.ass");
    const QVariantMap speed{{"inputFps", 25.0}, {"outputFps", 50.0}, {"outputMode", "Constant"}};
    CHECK(manager.exportSubtitles(framePath, "UTF-8", {QVariantMap{{"id", -2}, {"settings", speed}}}, &frameEditor).value("success").toBool());
    SubtitleModel frameResult;
    CHECK(frameResult.loadFromFile(framePath));
    const QString transformedTags = QStringLiteral(R"({\fad(200,400)\fade(1,2,3,200,400,600,800)\move(1,2,3,4,200,400)\t(200,400,\fs20)\t(2,\bord3)\t(200,400,2,\blur1)\k20\K22\kf24\ko26}Text{comment}{\p1}m 0 0{\p0}{\unknown(1,2)})");
    CHECK(frameResult.getLineStartMs(0) == 2000 && frameResult.getLineEndMs(0) == 4000);
    CHECK(frameResult.get(0).value("text") == transformedTags && frameResult.get(1).value("text") == transformedTags);
    CHECK(frameResult.get(1).value("isComment").toBool());
    CHECK(frameEditor.getAllLines() == frameBefore);
    QObject timecodeContext;
    timecodeContext.setProperty("exportFramerateContext", QVariantMap{{"available", true}, {"inputFps", 25.0}, {"outputFps", 31.25}, {"isVfr", true}, {"timecodes", QVariantList{0,20,60,80,120,160}}});
    manager.setVideoContextSources(&timecodeContext, nullptr);
    timed.setStartMs(30); timed.setEndMs(70);
    timed.text = QStringLiteral(R"({\t(10,40,\fs20)\fad(10,10)\k1}a{\k1}b{\k1}c{\k1}d)");
    frameEditor.setRawLines({timed});
    CHECK(manager.exportSubtitles(framePath, "UTF-8", {QVariantMap{{"id", -2}, {"settings", QVariantMap{{"inputFps", 25.0}, {"outputMode", "Variable"}}}}}, &frameEditor).value("success").toBool());
    CHECK(frameResult.loadFromFile(framePath));
    CHECK(frameResult.getLineStartMs(0) == 50 && frameResult.getLineEndMs(0) == 100);
    CHECK(frameResult.get(0).value("text") == QStringLiteral(R"({\t(10,50,\fs20)\fad(10,20)\k1}a{\k1}b{\k1}c{\k2}d)"));
    CHECK(manager.exportSubtitles(temporary.filePath("frame-reverse.ass"), "UTF-8", {QVariantMap{{"id", -2}, {"settings", QVariantMap{{"inputFps", 25.0}, {"outputMode", "Variable"}, {"reverse", true}}}}}, &frameResult).value("success").toBool());
    SubtitleModel reverseResult;
    CHECK(reverseResult.loadFromFile(temporary.filePath("frame-reverse.ass")));
    CHECK(reverseResult.getLineStartMs(0) == 30 && reverseResult.getLineEndMs(0) == 70);
    CHECK(reverseResult.get(0).value("text") == timed.text);
    manager.setVideoContextSources(nullptr, nullptr);
    std::vector<AssEntryData> tiny(1);
    tiny[0].startTime = 0; tiny[0].endTime = 1000;
    tiny[0].text = QStringLiteral(R"({\move(1,2,3,4,0,1)\fad(1,1)}Tiny)");
    CHECK(FramerateExport::transform(tiny, agi::vfr::Framerate(1000.), agi::vfr::Framerate(1.), error));
    CHECK(tiny[0].endTime == 10 && tiny[0].text == QStringLiteral(R"({\move(1,2,3,4,0,1)\fad(1,10)}Tiny)"));
    tiny[0].startTime = 0; tiny[0].endTime = 1000;
    tiny[0].text = QStringLiteral(R"({\k1}a{\k1}b{\k1}c{\k1}d{\k1}e)");
    CHECK(FramerateExport::transform(tiny, agi::vfr::Framerate(30.), agi::vfr::Framerate(24.), error));
    CHECK(tiny[0].text == QStringLiteral(R"({\k0}a{\k1}b{\k1}c{\k1}d{\k0}e)"));
    tiny[0].text = QStringLiteral(R"({\fad(999999999999999999999,1)}Fail)");
    const auto overflowBefore = tiny[0].text;
    CHECK(!FramerateExport::transform(tiny, agi::vfr::Framerate(25.), agi::vfr::Framerate(50.), error));
    CHECK(tiny[0].text == overflowBefore && !error.isEmpty());
    const int overflowStart = tiny[0].startTime;
    CHECK(FramerateExport::transform(tiny, agi::vfr::Framerate(), agi::vfr::Framerate(50.), error));
    CHECK(tiny[0].text == overflowBefore && tiny[0].startTime == overflowStart);
    QFile goodFrame(framePath);
    CHECK(goodFrame.open(QIODevice::ReadOnly));
    const auto goodFrameBytes = goodFrame.readAll(); goodFrame.close();
    auto badTime = timed; badTime.text = overflowBefore;
    frameEditor.setRawLines({timed, badTime});
    const auto failedTransformRows = frameEditor.getAllLines();
    CHECK(!manager.exportSubtitles(framePath, "UTF-8", {QVariantMap{{"id", -2}, {"settings", speed}}}, &frameEditor).value("success").toBool());
    CHECK(frameEditor.getAllLines() == failedTransformRows);
    frameEditor.setRawLines({timed});
    for (const auto &badRate : {QVariantMap{{"inputFps", 0}}, QVariantMap{{"inputFps", "bad"}}, QVariantMap{{"outputMode", "Variable"}}, QVariantMap{{"reverse", "true"}}})
        CHECK(!manager.exportSubtitles(framePath, "UTF-8", {QVariantMap{{"id", -2}, {"settings", badRate}}}, &frameEditor).value("success").toBool());
    CHECK(goodFrame.open(QIODevice::ReadOnly) && goodFrame.readAll() == goodFrameBytes);
    puts("PASS native CFR/VFR and reverse export, comments, fade/move/nested transform timing, cumulative karaoke rounding, zero sentinel, checked overflow and failure preserves destination/source");

    // Upstream Fix Styles checks names case-insensitively, includes comments,
    // and never creates a Default style. Export omits editor project state.
    SubtitleModel styleEditor;
    const auto styleSource = temporary.filePath("styles-source.ass");
    CHECK(writeScript(styleSource, R"ass([Script Info]
Title: Keep title
ScriptType: v4.00+
[Aegisub Project Garbage]
Video File: private-video.mkv
Audio File: private-audio.wav
[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Unique,Arial,20,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1
[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
Dialogue: 0,0:00:01.00,0:00:02.00,uNiQuE,,0,0,0,,Valid
Dialogue: 0,0:00:02.00,0:00:03.00,Missing,,0,0,0,,Missing
Comment: 0,0:00:03.00,0:00:04.00,MissingComment,,0,0,0,,Comment
)ass"));
    CHECK(styleEditor.loadFromFile(styleSource));
    const auto styleRows = styleEditor.getAllLines();
    const auto styleHeaders = styleEditor.styles();
    const auto styleInfo = styleEditor.scriptInfo();
    const auto nativeConfig = manager.exportFilterConfig(-1, &styleEditor, {});
    CHECK(nativeConfig.value("success").toBool() && nativeConfig.value("controls").toList().isEmpty());
    const auto styleExport = temporary.filePath("styles-export.ass");
    CHECK(manager.exportSubtitles(styleExport, "UTF-8", {QVariantMap{{"id", -1}}}, &styleEditor).value("success").toBool());
    SubtitleModel styleResult;
    CHECK(styleResult.loadFromFile(styleExport));
    CHECK(styleResult.get(0).value("style") == "uNiQuE");
    CHECK(styleResult.get(1).value("style") == "Default" && styleResult.get(2).value("style") == "Default");
    CHECK(styleResult.styles().size() == 1 && styleResult.styles()[0].toMap().value("name") == "Unique");
    QFile styleBytes(styleExport);
    CHECK(styleBytes.open(QIODevice::ReadOnly));
    const auto nativeBytes = styleBytes.readAll();
    CHECK(nativeBytes.contains("Title: Keep title") && !nativeBytes.contains("Aegisub Project Garbage") && !nativeBytes.contains("private-video"));
    const auto normalSave = temporary.filePath("styles-save.ass");
    CHECK(styleEditor.saveToFile(normalSave));
    QFile normalBytes(normalSave);
    CHECK(normalBytes.open(QIODevice::ReadOnly) && normalBytes.readAll().contains("private-video.mkv"));
    CHECK(writeScript(path, R"lua(
aegisub.register_filter('Delete headers','',0,function(subs)
    for i=#subs,1,-1 do
        local line=subs[i]
        if line.class=='style' or (line.class=='info' and line.key=='Title') then subs.delete(i) end
    end
end)
)lua"));
    CHECK(manager.addScript(path));
    int deleteHeaders = 0;
    for (const auto &entry : manager.filters())
        if (entry.toMap().value("name") == "Delete headers") deleteHeaders = entry.toMap().value("id").toInt();
    CHECK(deleteHeaders > 0);
    const auto noHeaders = temporary.filePath("deleted-headers.ass");
    CHECK(manager.exportSubtitles(noHeaders, "UTF-8", {QVariantMap{{"id", deleteHeaders}}, QVariantMap{{"id", -1}}}, &styleEditor).value("success").toBool());
    QFile noHeaderBytes(noHeaders);
    CHECK(noHeaderBytes.open(QIODevice::ReadOnly));
    const auto deletedBytes = noHeaderBytes.readAll();
    CHECK(!deletedBytes.contains("\nStyle: ") && !deletedBytes.contains("\nTitle:"));
    CHECK(styleEditor.getAllLines() == styleRows && styleEditor.styles() == styleHeaders && styleEditor.scriptInfo() == styleInfo);
    // Opening an empty-style file intentionally supplies Default, as upstream does.
    CHECK(styleResult.loadFromFile(noHeaders) && styleResult.styles().size() == 1);
    manager.removeScript(0);

    // Independent expected SRT intervals/text, including unsorted input,
    // nested overlaps, equal starts, adjacent duplicates, comments and drawings.
    SubtitleModel srtEditor;
    std::vector<SubtitleLine> srtRows;
    auto cue = [&](int start, int end, QString text, bool comment = false) {
        SubtitleLine row;
        row.setStartMs(start); row.setEndMs(end); row.text = text; row.isComment = comment;
        srtRows.push_back(std::move(row));
    };
    cue(2500, 4500, "C"); cue(1000, 4000, "A"); cue(2000, 3000, "B");
    cue(5000, 5500, "Same"); cue(5500, 6000, "Same");
    cue(7000, 8000, "D"); cue(7000, 9000, "E");
    cue(0, 10000, "COMMENT", true); cue(0, 10000, "");
    cue(10000, 11000, QStringLiteral(R"({\b1}B{\b1}B{\i-1}I{\b0}x)"));
    cue(12000, 13000, QStringLiteral(R"({\s1\u1}SU)"));
    cue(14000, 15000, QStringLiteral(R"({\p1}m 0 0 l 10 10{\p0}Text\hX\NY\nZ)"));
    cue(16000, 17000, QStringLiteral(R"({\t(0,100,\b1)\iclip(0,0,1,1)\bord3\be1\blur1\shad2\pbo3\pos(1,2)}Plain)"));
    cue(18000, 19000, QStringLiteral(R"(Keep{comment}{\b1)"));
    cue(20000, 21000, QStringLiteral(R"({\b(1,2)}Paren{\b0\i0}End)"));
    cue(22000, 23000, QStringLiteral(R"(\\N literal)"));
    srtEditor.setRawLines(srtRows);
    const auto srtBefore = srtEditor.getAllLines();
    const auto srtFilename = srtEditor.fileName();
    const auto richSrt = temporary.filePath("rich-export.srt");
    CHECK(manager.exportSubtitles(richSrt, "UTF-8", {}, &srtEditor).value("success").toBool());
    QFile richFile(richSrt);
    CHECK(richFile.open(QIODevice::ReadOnly));
    auto richBytes = richFile.readAll();
#ifdef Q_OS_WIN
    CHECK(richBytes.contains("\r\n") && !QByteArray(richBytes).replace("\r\n", "").contains('\n'));
#endif
    if (richBytes.startsWith("\xef\xbb\xbf")) richBytes.remove(0, 3);
    richBytes.replace("\r\n", "\n");
    const QByteArray expectedSrt = R"srt(1
00:00:01,000 --> 00:00:02,000
A

2
00:00:02,000 --> 00:00:02,500
B
A

3
00:00:02,500 --> 00:00:03,000
C
B
A

4
00:00:03,000 --> 00:00:04,000
A
C

5
00:00:04,000 --> 00:00:04,500
C

6
00:00:05,000 --> 00:00:06,000
Same

7
00:00:07,000 --> 00:00:08,000
E
D

8
00:00:08,000 --> 00:00:09,000
E

9
00:00:10,000 --> 00:00:11,000
<b>BB<i>I</b>x</i>

10
00:00:12,000 --> 00:00:13,000
<s><u>SU</s></u>

11
00:00:14,000 --> 00:00:15,000
Text X
Y
Z

12
00:00:16,000 --> 00:00:17,000
Plain

13
00:00:18,000 --> 00:00:19,000
Keep{\b1

14
00:00:20,000 --> 00:00:21,000
<b>Paren</b>End

15
00:00:22,000 --> 00:00:23,000
\N literal

)srt";
    if (richBytes != expectedSrt) fprintf(stderr, "%s", richBytes.constData());
    CHECK(richBytes == expectedSrt);
    CHECK(srtEditor.getAllLines() == srtBefore && srtEditor.fileName() == srtFilename);
    srtEditor.setRawLines({});
    CHECK(manager.exportSubtitles(temporary.filePath("empty.srt"), "UTF-8", {}, &srtEditor).value("success").toBool());
    srtEditor.setRawLines({srtRows[7], srtRows[8]});
    CHECK(manager.exportSubtitles(temporary.filePath("comments-only.srt"), "UTF-8", {}, &srtEditor).value("success").toBool());
    for (const auto &name : {"empty.srt", "comments-only.srt"}) {
        QFile file(temporary.filePath(name));
        CHECK(file.open(QIODevice::ReadOnly));
        const auto bytes = file.readAll();
        CHECK(bytes.isEmpty() || bytes == QByteArray("\xef\xbb\xbf"));
    }
    puts("PASS full SRT expected bytes: sorted/nested/equal-start overlaps, upstream interval text ordering, adjacent merging, comments/empty/drawing removal, formatting tags, malformed braces and platform newlines; source unchanged");

    SubtitleModel editor;
    editor.newDocument();
    const auto editorPath = temporary.filePath("editor.ass");
    CHECK(editor.saveToFile(editorPath));
    const auto originalText = editor.get(0).value("text");
    editor.pushUndo("Before export", 0, QVariantList{0});
    editor.setProperty(0, "text", QStringLiteral("原字幕"));
    auto editorLines = editor.rawLines();
    editorLines.front().extra.insert("key", QByteArray("v\0x", 3));
    editor.setRawLines(std::move(editorLines));
    const auto attachmentPath = temporary.filePath("test.bin");
    CHECK(writeScript(attachmentPath, QByteArray("attachment\0bytes", 16)));
    CHECK(editor.addAttachmentFile(attachmentPath, false));
    const auto originalLines = editor.getAllLines();
    const auto originalInfo = editor.scriptInfo();
    const auto originalStyles = editor.styles();
    const auto originalAttachments = editor.attachments();
    const auto undoName = editor.undoDescription();
    const auto modified = editor.isModified();
    manager.setSubtitleModel(&editor);
    CHECK(writeScript(path, R"lua(
local function suffix(subs, config)
    for i,line in ipairs(subs) do
        if line.class == 'dialogue' then line.text=line.text..config.suffix;subs[i]=line end
        if line.class == 'style' then line.fontsize=37.5;subs[i]=line end
        if line.class == 'info' and line.key == 'Title' then line.value='Export title';subs[i]=line end
    end
end
aegisub.register_filter('A','',0,suffix,function(subs, stored)
    return {{class='edit',name='suffix',text=stored.suffix or 'A',x=0,y=0}}
end)
aegisub.register_filter('B','',2000,suffix)
aegisub.register_filter('Fail export','',0,function(subs)
    for i,line in ipairs(subs) do
        if line.class == 'dialogue' then line.text='SHOULD_NOT_LEAK';subs[i]=line end
    end
    error('export pipeline failure')
end)
)lua"));
    CHECK(manager.addScript(path));
    auto filterId = [&](const QString &name) {
        for (const auto &filter : manager.filters()) {
            const auto entry = filter.toMap();
            if (entry.value("name") == name) return entry.value("id").toInt();
        }
        return -1;
    };
    const int a = filterId("A"), b = filterId("B"), failure = filterId("Fail export");
    CHECK(a > 0 && b > 0 && failure > 0);
    const auto config = manager.exportFilterConfig(a, &editor, {{"suffix", "X"}});
    CHECK(config.value("success").toBool() && config.value("controls").toList()[0].toMap().value("text") == "X");
    const QVariantList pipeline{QVariantMap{{"id", b}, {"settings", QVariantMap{{"suffix", "B"}}}},
                                QVariantMap{{"id", a}, {"settings", QVariantMap{{"suffix", "A"}}}}};
    const auto exportPath = temporary.filePath("export.ass");
    CHECK(manager.exportSubtitles(exportPath, "UTF-16LE", pipeline, &editor).value("success").toBool());
    QFile exported(exportPath);
    CHECK(exported.open(QIODevice::ReadOnly));
    const auto savedExport = exported.readAll();
    exported.close();
    CHECK(savedExport.startsWith("\xff\xfe"));
    SubtitleModel reopened;
    CHECK(reopened.loadFromFile(exportPath));
    CHECK(reopened.get(0).value("text") == QStringLiteral("原字幕BA"));
    CHECK(reopened.styles().front().toMap().value("size").toDouble() == 37.5);
    CHECK(reopened.rawLines().front().extra == editor.rawLines().front().extra);
    CHECK(reopened.attachments() == originalAttachments);
    CHECK(QDir().mkpath(temporary.filePath("extracted")));
    CHECK(reopened.extractAttachment(0, temporary.filePath("extracted")));
    QFile extracted(temporary.filePath("extracted/test.bin"));
    CHECK(extracted.open(QIODevice::ReadOnly) && extracted.readAll() == QByteArray("attachment\0bytes", 16));
    CHECK(manager.exportSubtitles(temporary.filePath("export-gbk.ass"), "GBK", {}, &editor).value("success").toBool());
    CHECK(reopened.loadFromFileWithCharset(temporary.filePath("export-gbk.ass"), "GBK"));
    CHECK(reopened.get(0).value("text") == QStringLiteral("原字幕"));
    CHECK(manager.exportSubtitles(temporary.filePath("export.srt"), "UTF-8", pipeline, &editor).value("success").toBool());
    CHECK(reopened.loadFromFile(temporary.filePath("export.srt")) && reopened.get(0).value("text") == QStringLiteral("原字幕BA"));
    const QVariantList badPipelines[] = {
        {pipeline[0], QVariantMap{{"id", failure}}},
        {QVariantMap{{"id", -9999}}}, {QVariantMap{{"id", "1"}}},
        {QVariantMap{{"id", a}, {"settings", "invalid"}}}, {"invalid"}
    };
    for (const auto &bad : badPipelines)
        CHECK(!manager.exportSubtitles(exportPath, "UTF-8", bad, &editor).value("success").toBool());
    CHECK(!manager.exportSubtitles(exportPath, "US-ASCII", {}, &editor).value("success").toBool());
    CHECK(!manager.exportSubtitles(exportPath, "unknown-charset", {}, &editor).value("success").toBool());
    CHECK(!manager.exportSubtitles(temporary.filePath("missing/export.ass"), "UTF-8", {}, &editor).value("success").toBool());
    CHECK(!manager.exportSubtitles(temporary.filePath("export.vtt"), "UTF-8", {}, &editor).value("success").toBool());
    CHECK(exported.open(QIODevice::ReadOnly) && exported.readAll() == savedExport);
    exported.close();
    CHECK(editor.getAllLines() == originalLines && editor.scriptInfo() == originalInfo && editor.styles() == originalStyles);
    CHECK(editor.attachments() == originalAttachments && editor.fileName() == editorPath);
    CHECK(editor.isModified() == modified && editor.undoDescription() == undoName && !editor.canRedo());
    manager.reloadScript(0);
    CHECK(!manager.exportSubtitles(exportPath, "UTF-8", pipeline, &editor).value("success").toBool());
    while (editor.canUndo()) editor.undo();
    CHECK(editor.get(0).value("text") == originalText && !editor.isModified());
    CHECK(writeScript(path, R"lua(
aegisub.register_filter('UI configured filter','Real UI filter',0,function(subs,c)
    assert(c.text=='Edited' and c.multiline=='first\nsecond' and c.choice=='Beta')
    assert(c.enabled==true and c.integer==42 and c.number==1.25)
    assert(c.rgb=='#112233' and c.rgba=='#11223380' and c.alpha=='&H00&')
    for i,line in ipairs(subs) do
        if line.class=='dialogue' then line.text=c.text..' '..c.choice;subs[i]=line end
    end
end,function()
    return {
      {class='label',label='Settings',x=0,y=0},
      {class='edit',name='text',text='Initial',x=0,y=1},
      {class='textbox',name='multiline',text='',x=0,y=2,height=2},
      {class='dropdown',name='choice',items={'Alpha','Beta'},value='Alpha',x=0,y=4},
      {class='checkbox',name='enabled',label='Enabled',value=false,x=0,y=5},
      {class='intedit',name='integer',value=10,min=0,max=100,x=0,y=6},
      {class='floatedit',name='number',value=0.5,min=0,max=10,step=0.25,x=0,y=7},
      {class='color',name='rgb',value='&H332211&',x=0,y=8},
      {class='coloralpha',name='rgba',value='&H80332211&',x=0,y=9},
      {class='alpha',name='alpha',text='&H00&',x=0,y=10}
    }
end)
)lua"));
    manager.reloadScript(0);
    CHECK(manager.filters().size() == 3);
    manager.setVideoContextSources(&timecodeContext, nullptr);
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("automationManager", &manager);
    engine.rootContext()->setContextProperty("nativeSubtitleModel", &editor);
    QQmlComponent themeComponent(&engine);
    themeComponent.setData("import QtQml; QtObject { property string uiFont: \"Segoe UI\" }", QUrl());
    std::unique_ptr<QObject> theme(themeComponent.create());
    CHECK(theme);
    engine.rootContext()->setContextProperty("uiTheme", theme.get());
    const auto qmlRoot = QDir(root).absoluteFilePath("../qml");
    engine.addImportPath(qmlRoot);
    {
        SubtitleModel clipRows;
        clipRows.setAllLines({QVariantMap{{"start", "0:00:03.00"}, {"end", "0:00:04.00"}},
                              QVariantMap{{"start", "0:00:01.00"}, {"end", "0:00:09.00"}},
                              QVariantMap{{"start", "0:00:02.00"}, {"end", "0:00:06.00"}}});
        QQmlEngine::setObjectOwnership(&clipRows, QQmlEngine::CppOwnership);
        QFile selectionFile(qmlRoot + "/project/AudioClipSelection.js");
        CHECK(selectionFile.open(QIODevice::ReadOnly));
        QJSEngine selectionScript;
        CHECK(!selectionScript.evaluate(QString::fromUtf8(selectionFile.readAll())).isError());
        auto selectionRange = selectionScript.globalObject().property("range");
        CHECK(selectionRange.isCallable());
        auto picked = selectionRange.call({selectionScript.newQObject(&clipRows),
            selectionScript.toScriptValue(QVariantList{0, 2})});
        CHECK(!picked.isError() && picked.property("startMs").toInt() == 2000
              && picked.property("endMs").toInt() == 6000);
        auto empty = selectionRange.call({selectionScript.newQObject(&clipRows),
            selectionScript.toScriptValue(QVariantList{-1, 99})});
        CHECK(empty.isNull());
        puts("PASS audio clip selection bounds selected noncontiguous lines and rejects invalid selection");
    }
    QQmlComponent projectComponent(&engine, QUrl::fromLocalFile(qmlRoot + "/project/SubtitleProject.qml"));
    std::unique_ptr<QObject> project(projectComponent.create());
    if (!project) for (const auto &error : projectComponent.errors()) fprintf(stderr, "%s\n", qPrintable(error.toString()));
    CHECK(project);
    {
        SubtitleModel syncRows;
        syncRows.setAllLines({QVariantMap{{"start", "0:00:01.00"}, {"end", "0:00:02.00"}, {"text", "first"}},
                             QVariantMap{{"start", "0:00:03.00"}, {"end", "0:00:04.00"}, {"text", "second"}}});
        QQmlEngine::setObjectOwnership(&syncRows, QQmlEngine::CppOwnership);
        std::unique_ptr<QObject> syncProject(projectComponent.create());
        CHECK(syncProject && syncProject->setProperty("subtitleModel", QVariant::fromValue(&syncRows)));
        QQmlComponent mockComponent(&engine);
        mockComponent.setData(R"qml(import QtQml
QtObject {
    property bool hasVideo: true
    property bool autoScroll: true
    property bool playing: true
    readonly property bool isPlaying: playing
    property int seeks: 0
    property int pauses: 0
    property int audioScrolls: 0
    property real time: 8
    property string subtitle: ""
    function setActiveSubtitle(start, end, text) { subtitle = text; }
    function pause() { playing = false; pauses++; }
    function play() { playing = true; }
    function seekTime(value) { time = value; seeks++; }
    function scrollRangeInView(start, end) { audioScrolls++; }
})qml", QUrl());
        std::unique_ptr<QObject> media(mockComponent.create());
        CHECK(media);
        QQmlComponent syncComponent(&engine, QUrl::fromLocalFile(qmlRoot + "/project/SubtitleVideoSync.qml"));
        std::unique_ptr<QObject> sync(syncComponent.createWithInitialProperties({
            {"project", QVariant::fromValue(syncProject.get())},
            {"videoCtrl", QVariant::fromValue(media.get())}, {"audioCtrl", QVariant::fromValue(media.get())}}));
        CHECK(sync);
        auto select = [&](const QString &code) {
            QQmlExpression expression(engine.rootContext(), syncProject.get(), code + "; true");
            CHECK(expression.evaluate().toBool() && !expression.hasError());
            return 0;
        };
        CHECK(select("selectRow(1, false, false)") == 0);
        CHECK(media->property("time").toDouble() == 3 && media->property("seeks").toInt() == 1);
        CHECK(!media->property("playing").toBool() && media->property("subtitle").toString() == "second");
        CHECK(select("selectRow(1, false, false)") == 0);
        CHECK(media->property("seeks").toInt() == 1 && media->property("pauses").toInt() == 1);
        CHECK(media->setProperty("autoScroll", false) && media->setProperty("playing", true));
        CHECK(select("selectRow(0, false, false)") == 0);
        CHECK(media->property("time").toDouble() == 3 && media->property("playing").toBool());
        CHECK(media->property("subtitle").toString() == "first");
        CHECK(syncRows.setProperty(0, "text", "edited"));
        CHECK(media->property("subtitle").toString() == "edited" && media->property("seeks").toInt() == 1);
        QQmlExpression jump(engine.rootContext(), sync.get(), "jumpToLine(0); true");
        CHECK(jump.evaluate().toBool() && !jump.hasError());
        CHECK(media->property("time").toDouble() == 1 && media->property("seeks").toInt() == 2);
        CHECK(media->property("playing").toBool() && media->property("audioScrolls").toInt() == 1);
        CHECK(media->setProperty("autoScroll", true) && media->setProperty("hasVideo", false));
        CHECK(select("selectRow(1, false, false)") == 0);
        CHECK(media->property("subtitle").toString() == "second" && media->property("seeks").toInt() == 2);
        CHECK(media->setProperty("hasVideo", true) && media->setProperty("autoScroll", false));
        QQuickWindow selectionWindow;
        selectionWindow.resize(800, 180);
        QQmlComponent gridComponent(&engine, QUrl::fromLocalFile(qmlRoot + "/views/SubtitleGridArea.qml"));
        std::unique_ptr<QObject> gridObject(gridComponent.createWithInitialProperties({
            {"project", QVariant::fromValue(syncProject.get())}, {"width", 800}, {"height", 180}}));
        CHECK(gridObject);
        auto *grid = qobject_cast<QQuickItem *>(gridObject.get());
        CHECK(grid);
        grid->setParentItem(selectionWindow.contentItem());
        selectionWindow.show();
        QTest::qWait(40);
        auto *list = grid->findChild<QQuickItem *>("subtitle-grid-list");
        CHECK(list);
        const auto row0 = list->mapToScene(QPointF(120, 10)).toPoint();
        const auto row1 = list->mapToScene(QPointF(120, 30)).toPoint();
        QSignalSpy doubleClicks(grid, SIGNAL(lineDoubleClicked(int)));
        CHECK(doubleClicks.isValid());
        QTest::mouseClick(&selectionWindow, Qt::LeftButton, Qt::NoModifier, row0);
        CHECK(syncProject->property("currentSelectedIndex").toInt() == 0);
        CHECK(media->property("subtitle").toString() == "edited" && media->property("seeks").toInt() == 2);
        QTest::mouseDClick(&selectionWindow, Qt::LeftButton, Qt::NoModifier, row1);
        CHECK(doubleClicks.count() == 1 && doubleClicks.at(0).at(0).toInt() == 1);
        CHECK(syncProject->property("currentSelectedIndex").toInt() == 1);
        QTest::keyClick(&selectionWindow, Qt::Key_Up, Qt::AltModifier);
        CHECK(syncProject->property("currentSelectedIndex").toInt() == 0);
        CHECK(syncProject->property("selectedIndices").value<QJSValue>().toVariant().toList() == QVariantList{1});
        QTest::keyClick(&selectionWindow, Qt::Key_End);
        CHECK(syncProject->property("currentSelectedIndex").toInt() == 1);
        QTest::keyClick(&selectionWindow, Qt::Key_Up, Qt::ShiftModifier);
        CHECK(syncProject->property("selectedIndices").value<QJSValue>().toVariant().toList() == (QVariantList{0, 1}));
        QTest::keyClick(&selectionWindow, Qt::Key_Down, Qt::ShiftModifier);
        CHECK(syncProject->property("selectedIndices").value<QJSValue>().toVariant().toList() == QVariantList{1});
        CHECK(media->setProperty("autoScroll", true));
        const int seeksBeforeReload = media->property("seeks").toInt();
        syncRows.setAllLines(syncRows.getAllLines());
        CHECK(media->property("seeks").toInt() == seeksBeforeReload + 1);
        puts("PASS original selection/video sync, actual grid mouse/double-click, anchored keyboard selection and same-row document reload");
    }
    {
        auto *clipboard = QGuiApplication::clipboard();
        auto savedClipboard = std::unique_ptr<QMimeData, std::function<void(QMimeData *)>>(
            new QMimeData, [clipboard](QMimeData *data) { clipboard->setMimeData(data); });
        if (const auto *previous = clipboard->mimeData())
            for (const auto &format : previous->formats()) savedClipboard->setData(format, previous->data(format));
        SubtitleModel clipModel;
        clipModel.setAllLines({QVariantMap{{"text", "keep,尾 "}, {"isComment", true}, {"layer", 2},
            {"start", "0:00:01.00"}, {"end", "0:00:02.00"}, {"actor", "Actor"}, {"marginLeft", 12}},
            QVariantMap{{"text", "other"}}});
        clipModel.clearUndo();
        const auto originalRows = clipModel.getAllLines();
        AegisubCoreBridge clipboardBridge;
        QQmlEngine clipEngine;
        clipEngine.rootContext()->setContextProperty("aegisubCore", &clipboardBridge);
        QQmlComponent clipProjectComponent(&clipEngine, QUrl::fromLocalFile(qmlRoot + "/project/SubtitleProject.qml"));
        std::unique_ptr<QObject> clipProject(clipProjectComponent.create());
        CHECK(clipProject && clipProject->setProperty("subtitleModel", QVariant::fromValue(&clipModel)));
        CHECK(clipProject->setProperty("selectedIndices", QVariantList{0, 1}));
        QQmlExpression copy(clipEngine.rootContext(), clipProject.get(), QStringLiteral("copySelectedLines(); true"));
        CHECK(copy.evaluate().toBool() && !copy.hasError());
        QCoreApplication::processEvents();
        const auto clipboardCopy = clipboard->text().replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        CHECK(clipboardCopy.startsWith("Comment: 2,0:00:01.00,0:00:02.00,Default,Actor,0012,0000,0000,,keep,尾 \nDialogue:"));
        auto copied = clipModel.parseClipboardLines(clipboard->text());
        CHECK(copied.size() == 2 && copied[0].toMap().value("isComment").toBool());
        CHECK(copied[0].toMap().value("text") == QStringLiteral("keep,尾 "));
        clipboard->setText(QStringLiteral("Comment: 3,0:00:04.00,0:00:05.00,Alt,B,1,2,3,fx,external,尾 \r\nplain text"));
        QQmlExpression paste(clipEngine.rootContext(), clipProject.get(), QStringLiteral("pasteLines(false); true"));
        CHECK(paste.evaluate().toBool() && !paste.hasError());
        CHECK(clipModel.rowCount() == 4 && clipModel.get(0).value("isComment").toBool());
        CHECK(clipModel.get(0).value("style") == "Alt" && clipModel.get(0).value("text") == QStringLiteral("external,尾 "));
        CHECK(clipModel.getLineStartMs(0) == 4000 && clipModel.get(0).value("marginRight").toInt() == 2);
        CHECK(clipModel.get(1).value("text") == "plain text" && clipModel.getLineEndMs(1) == 0);
        CHECK(QMetaObject::invokeMethod(clipProject.get(), "undo"));
        CHECK(clipModel.getAllLines() == originalRows);
        clipboard->clear();
        CHECK(paste.evaluate().toBool() && !paste.hasError());
        CHECK(clipModel.getAllLines() == originalRows);
        clipboard->setText("Dialogue: 0,0:00:00.00,0:00:00.00,Default,,0,0,0,,replacement");
        QQmlExpression over(clipEngine.rootContext(), clipProject.get(), QStringLiteral("pasteLines(true, null, ['text','isComment']); true"));
        CHECK(over.evaluate().toBool() && !over.hasError());
        CHECK(clipModel.get(0).value("text") == "replacement" && !clipModel.get(0).value("isComment").toBool());
        CHECK(QMetaObject::invokeMethod(clipProject.get(), "undo"));
        CHECK(clipModel.getAllLines() == originalRows);
        CHECK(clipModel.parseClipboardLines("Dialogue: malformed")[0].toMap().value("text") == "Dialogue: malformed");
        QQmlComponent blockedClipboardComponent(&clipEngine);
        blockedClipboardComponent.setData("import QtQml; QtObject { function setClipboardText(text) { return false; } }", QUrl());
        std::unique_ptr<QObject> blockedClipboard(blockedClipboardComponent.create());
        CHECK(blockedClipboard);
        clipEngine.rootContext()->setContextProperty("aegisubCore", blockedClipboard.get());
        QQmlExpression failedCut(clipEngine.rootContext(), clipProject.get(), QStringLiteral("cutSelectedLines(); true"));
        CHECK(failedCut.evaluate().toBool() && !failedCut.hasError());
        CHECK(clipModel.getAllLines() == originalRows);
        clipEngine.rootContext()->setContextProperty("aegisubCore", &clipboardBridge);
        puts("PASS actual system clipboard copy/comment serialization, external ASS/plain paste, fields, trailing text, empty clipboard isolation, paste-over and undo");
        if (qEnvironmentVariableIsSet("AEGISUB_CLIPBOARD_ONLY")) return 0;
    }
    {
        SubtitleModel timingModel;
        timingModel.setAllLines({QVariantMap{{"text", "frame editor"}, {"start", "0:00:00.02"}, {"end", "0:00:00.16"}}});
        timingModel.clearUndo();
        TestEditorTimeline timeline;
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&timingModel)));
        CHECK(project->setProperty("currentSelectedIndex", 0));
        QQmlComponent editorComponent(&engine, QUrl::fromLocalFile(qmlRoot + "/views/SubtitleEditBox.qml"));
        std::unique_ptr<QObject> timeEditor(editorComponent.createWithInitialProperties(
            {{"project", QVariant::fromValue(project.get())}, {"videoCtrl", QVariant::fromValue(&timeline)}}));
        if (!timeEditor) for (const auto &error : editorComponent.errors()) fprintf(stderr, "%s\n", qPrintable(error.toString()));
        CHECK(timeEditor);
        auto *start = timeEditor->findChild<QObject *>("editor-start-time");
        auto *end = timeEditor->findChild<QObject *>("editor-end-time");
        auto *duration = timeEditor->findChild<QObject *>("editor-duration");
        auto *frames = timeEditor->findChild<QObject *>("editor-frame-mode");
        CHECK(start && end && duration && frames);
        CHECK(start->property("text") == "0:00:00.02");
        CHECK(QMetaObject::invokeMethod(frames, "clicked"));
        CHECK(start->property("text") == "1" && end->property("text") == "3" && duration->property("text") == "3");
        CHECK(!timingModel.canUndo());
        CHECK(start->setProperty("text", "2"));
        CHECK(QMetaObject::invokeMethod(start, "editingFinished"));
        CHECK(timingModel.getLineStartMs(0) == 70 && timingModel.getLineEndMs(0) == 160);
        CHECK(duration->property("text") == "2");
        CHECK(duration->setProperty("text", "1"));
        CHECK(QMetaObject::invokeMethod(duration, "editingFinished"));
        CHECK(timingModel.getLineEndMs(0) == 110);
        CHECK(end->property("text") == "2");
        CHECK(QMetaObject::invokeMethod(project.get(), "undo"));
        CHECK(timingModel.getLineEndMs(0) == 160 && end->property("text") == "3");
        CHECK(start->setProperty("text", "invalid"));
        CHECK(QMetaObject::invokeMethod(start, "editingFinished"));
        CHECK(timingModel.getLineStartMs(0) == 70 && start->property("text") == "2");
        timeline.close();
        CHECK(!timeEditor->property("frameMode").toBool() && !frames->property("enabled").toBool());
        CHECK(start->property("text") == "0:00:00.07" && end->property("text") == "0:00:00.16");
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&editor)));
        puts("PASS actual subtitle editor START/END VFR frames, inclusive duration, exact millisecond edits, mode no-op, undo, invalid input and timecodes close");
    }
    {
        SubtitleModel formatModel;
        formatModel.newDocument();
        formatModel.setProperty(0, "text", "word tail");
        formatModel.clearUndo();
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&formatModel)));
        CHECK(project->setProperty("currentSelectedIndex", 0) && project->setProperty("selectedIndices", QVariantList{0}));
        QQuickWindow editWindow;
        editWindow.resize(900, 320);
        QQmlComponent component(&engine, QUrl::fromLocalFile(qmlRoot + "/views/SubtitleEditBox.qml"));
        std::unique_ptr<QObject> formatEditor(component.createWithInitialProperties(
            {{"project", QVariant::fromValue(project.get())}, {"width",900}, {"height",320}}));
        CHECK(formatEditor);
        auto *item = qobject_cast<QQuickItem *>(formatEditor.get());
        CHECK(item);
        item->setParentItem(editWindow.contentItem());
        editWindow.show();
        QTest::qWait(50);
        auto *area = formatEditor->property("subtitleEditArea").value<QObject *>();
        auto *textItem = qobject_cast<QQuickItem *>(area);
        CHECK(textItem);
        for (const auto &entry : {QPair<const char *,const char *>("editor-bold","b"),
                                 {"editor-italic","i"}, {"editor-underline","u"}, {"editor-strikeout","s"}}) {
            CHECK(QMetaObject::invokeMethod(area, "select", Q_ARG(int,0), Q_ARG(int,4)));
            textItem->forceActiveFocus();
            auto *button = formatEditor->findChild<QQuickItem *>(entry.first);
            CHECK(button);
            QTest::mouseClick(&editWindow, Qt::LeftButton, Qt::NoModifier,
                button->mapToScene(QPointF(button->width()/2, button->height()/2)).toPoint());
            CHECK(formatModel.get(0).value("text") == "{\\" + QString(entry.second) + "1}word{\\" + QString(entry.second) + "0} tail");
            CHECK(area->property("selectedText") == "word");
            QTest::mouseClick(&editWindow, Qt::LeftButton, Qt::NoModifier,
                button->mapToScene(QPointF(button->width()/2, button->height()/2)).toPoint());
            CHECK(formatModel.get(0).value("text") == "{\\" + QString(entry.second) + "0}word{\\" + QString(entry.second) + "0} tail");
            CHECK(QMetaObject::invokeMethod(project.get(), "undo"));
            CHECK(QMetaObject::invokeMethod(project.get(), "undo"));
            CHECK(formatModel.get(0).value("text") == "word tail" && area->property("text") == "word tail" && !formatModel.canUndo());
        }
        CHECK(area->setProperty("cursorPosition", 0));
        textItem->forceActiveFocus();
        QTest::keyClick(&editWindow, Qt::Key_B, Qt::ControlModifier);
        CHECK(formatModel.get(0).value("text") == "{\\b1}word tail");
        QTest::keyClick(&editWindow, Qt::Key_B, Qt::ControlModifier);
        CHECK(formatModel.get(0).value("text") == "{\\b0}word tail");
        CHECK(QMetaObject::invokeMethod(project.get(), "undo") && QMetaObject::invokeMethod(project.get(), "undo"));
        const QString tagged = "{\\fnArial\\fs24}word tail";
        CHECK(formatModel.setProperty(0, "text", tagged));
        CHECK(QMetaObject::invokeMethod(formatEditor.get(), "syncFromProject"));
        formatModel.clearUndo();
        CHECK(area->setProperty("cursorPosition", tagged.indexOf("word")));
        auto *fontButton = formatEditor->findChild<QQuickItem *>("editor-font");
        CHECK(fontButton);
        QTest::mouseClick(&editWindow, Qt::LeftButton, Qt::NoModifier,
            fontButton->mapToScene(QPointF(fontButton->width()/2, fontButton->height()/2)).toPoint());
        auto *fontDialog = formatEditor->property("fontDialog").value<QObject *>();
        CHECK(fontDialog && fontDialog->property("visible").toBool());
        auto font = fontDialog->property("selectedFont").value<QFont>();
        CHECK(font.family() == "Arial" && font.pointSizeF() == 24);
        QTest::qWait(60);
        std::function<QQuickItem *(QQuickItem *, const QString &)> findVisual;
        findVisual = [&](QQuickItem *parent, const QString &name) -> QQuickItem * {
            if (parent->objectName() == name && parent->isVisible()) return parent;
            for (auto *child : parent->childItems()) if (auto *found = findVisual(child, name)) return found;
            return nullptr;
        };
        auto fontControl = [&](const QString &name) -> QQuickItem * {
            for (auto *surface : QGuiApplication::allWindows()) {
                auto *quick = qobject_cast<QQuickWindow *>(surface);
                if (quick && quick->isVisible()) if (auto *found = findVisual(quick->contentItem(), name)) return found;
            }
            return nullptr;
        };
        auto *familyList = fontControl("familyListView");
        CHECK(familyList && familyList->window());
        const auto fontScreenshot = qEnvironmentVariable("AEGISUB_INLINE_FONT_SCREENSHOT");
        if (!fontScreenshot.isEmpty()) {
            QTest::qWait(80);
            CHECK(familyList->window()->grabWindow().save(fontScreenshot));
        }
        CHECK(QMetaObject::invokeMethod(fontDialog, "reject"));
        CHECK(!formatModel.canUndo() && formatModel.get(0).value("text") == tagged);
        CHECK(QMetaObject::invokeMethod(formatEditor.get(), "chooseFont"));
        CHECK(QMetaObject::invokeMethod(fontDialog, "accept"));
        CHECK(!formatModel.canUndo() && formatModel.get(0).value("text") == tagged);
        CHECK(QMetaObject::invokeMethod(formatEditor.get(), "chooseFont"));
        QTest::qWait(60);
        familyList = fontControl("familyListView");
        auto *sizeInput = fontControl("sizeEdit");
        auto *underline = fontControl("underlineEffect");
        CHECK(familyList && sizeInput && underline);
        auto *fontWindow = familyList->window();
        const int familyCount = familyList->property("count").toInt();
        CHECK(familyCount > 1);
        const int initialFamilyIndex = familyList->property("currentIndex").toInt();
        const bool atLastFamily = initialFamilyIndex == familyCount - 1;
        const int expectedFamilyIndex = initialFamilyIndex + (atLastFamily ? -1 : 1);
        familyList->forceActiveFocus();
        QTest::keyClick(fontWindow, atLastFamily ? Qt::Key_Up : Qt::Key_Down);
        QTest::qWait(20);
        printf("Font chooser navigation: count=%d, initial=%d, expected=%d, actual=%d, focus=%d\n",
            familyCount, initialFamilyIndex, expectedFamilyIndex,
            familyList->property("currentIndex").toInt(), familyList->hasActiveFocus());
        CHECK(familyList->property("currentIndex").toInt() == expectedFamilyIndex);
        auto *familyDelegate = familyList->property("currentItem").value<QQuickItem *>();
        CHECK(familyDelegate);
        const auto chosenFamily = familyDelegate->property("text").toString();
        printf("Font chooser keyboard selected installed family: %s\n", qPrintable(chosenFamily));
        CHECK(!chosenFamily.isEmpty() && QFontDatabase::families().contains(chosenFamily));
        CHECK(fontDialog->property("selectedFont").value<QFont>().family() == chosenFamily);
        sizeInput->forceActiveFocus();
        QTest::keySequence(fontWindow, QKeySequence(QKeySequence::SelectAll));
        QTest::keyClick(fontWindow, Qt::Key_3);
        QTest::keyClick(fontWindow, Qt::Key_2);
        QTest::mouseClick(fontWindow, Qt::LeftButton, Qt::NoModifier,
            underline->mapToScene(QPointF(underline->width()/2, underline->height()/2)).toPoint());
        font = fontDialog->property("selectedFont").value<QFont>();
        CHECK(font.pointSizeF() == 32 && font.underline());
        if (!fontScreenshot.isEmpty()) { QTest::qWait(80); CHECK(fontWindow->grabWindow().save(fontScreenshot)); }
        std::function<QQuickItem *(QQuickItem *)> findOk;
        findOk = [&](QQuickItem *parent) -> QQuickItem * {
            if (parent->property("text") == "OK" && parent->property("pressed").isValid() && parent->isVisible()) return parent;
            for (auto *child : parent->childItems()) if (auto *found = findOk(child)) return found;
            return nullptr;
        };
        auto *okButton = findOk(fontWindow->contentItem());
        CHECK(okButton);
        QTest::mouseClick(fontWindow, Qt::LeftButton, Qt::NoModifier,
            okButton->mapToScene(QPointF(okButton->width()/2, okButton->height()/2)).toPoint());
        CHECK(!fontDialog->property("visible").toBool());
        // Installed families can select a bold or italic face by default (for
        // example Arial Black on macOS). Preserve those chosen font properties.
        QString expectedTags = QStringLiteral("\\fn%1\\fs32").arg(chosenFamily);
        if (font.bold()) expectedTags += QStringLiteral("\\b1");
        if (font.italic()) expectedTags += QStringLiteral("\\i1");
        expectedTags += QStringLiteral("\\u1");
        if (font.strikeOut()) expectedTags += QStringLiteral("\\s1");
        const auto expectedText = QStringLiteral("{%1}word tail").arg(expectedTags);
        printf("Font chooser saved ASS: %s\n", qPrintable(formatModel.get(0).value("text").toString()));
        CHECK(formatModel.get(0).value("text") == expectedText);
        CHECK(area->property("text") == formatModel.get(0).value("text"));
        CHECK(QMetaObject::invokeMethod(project.get(), "undo"));
        CHECK(formatModel.get(0).value("text") == tagged && !formatModel.canUndo());
        CHECK(QMetaObject::invokeMethod(formatEditor.get(), "chooseFont"));
        CHECK(formatModel.setProperty(0, "text", "external change"));
        formatModel.clearUndo();
        CHECK(fontDialog->setProperty("selectedFont", font) && QMetaObject::invokeMethod(fontDialog, "accept"));
        CHECK(formatModel.get(0).value("text") == "external change" && !formatModel.canUndo());
        const auto editorScreenshot = qEnvironmentVariable("AEGISUB_INLINE_FORMAT_SCREENSHOT");
        if (!editorScreenshot.isEmpty()) {
            CHECK(QMetaObject::invokeMethod(formatEditor.get(), "syncFromProject"));
            QTest::qWait(80);
            CHECK(editWindow.grabWindow().save(editorScreenshot));
        }
        formatEditor.reset();
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&editor)));
        puts("PASS actual inline format buttons preserve selection, repeated toggle, Ctrl+B, native document undo, real font chooser defaults/accept/no-op/cancel and stale result rejection");
    }
    {
        AegisubCoreBridge colorBridge;
        engine.rootContext()->setContextProperty("aegisubCore", &colorBridge);
        SubtitleModel colorModel;
        colorModel.newDocument();
        auto initialStyle = colorModel.getStyle(0);
        initialStyle["primary"] = "&HAA332211";
        colorModel.setStyle(0, initialStyle);
        colorModel.clearUndo();
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&colorModel)));
        QQmlComponent managerComponent(&engine, QUrl::fromLocalFile(qmlRoot + "/dialogs/DialogManager.qml"));
        std::unique_ptr<QObject> colorManager(managerComponent.createWithInitialProperties(
            {{"project", QVariant::fromValue(project.get())}}));
        CHECK(colorManager);
        QQmlExpression openStyle(engine.rootContext(), colorManager.get(),
            QStringLiteral("dlgStyleEditor.loadStyle(project.subtitleModel.getStyle(0), false, 0); dlgStyleEditor.open(); true"));
        CHECK(openStyle.evaluate().toBool() && !openStyle.hasError());
        auto *styleLoader = colorManager->property("dlgStyleEditor").value<QObject *>();
        CHECK(styleLoader);
        auto *styleDialog = styleLoader->property("item").value<QObject *>();
        CHECK(styleDialog);
        auto *loader = colorManager->property("dlgColorPicker").value<QObject *>();
        CHECK(loader && loader->setProperty("targetProp", "style_primary"));
        const QColor initial(0x11, 0x22, 0x33, 0x55);
        CHECK(loader->setProperty("currentColor", initial));
        CHECK(QMetaObject::invokeMethod(loader, "open"));
        auto *picker = loader->property("item").value<QObject *>();
        CHECK(picker && picker->property("visible").toBool());
        CHECK(picker->property("originalColor").value<QColor>() == initial);
        CHECK(picker->property("redVal").toInt() == 17 && picker->property("greenVal").toInt() == 34
              && picker->property("blueVal").toInt() == 51 && picker->property("alphaVal").toInt() == 170);
        auto *assField = picker->findChild<QObject *>("color-picker-ass");
        CHECK(assField && assField->property("text") == "&HAA332211");
        const auto screenshot = qEnvironmentVariable("AEGISUB_COLOR_PICKER_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            QTest::qWait(100);
            CHECK(qobject_cast<QQuickWindow *>(picker)->grabWindow().save(screenshot));
        }
        auto *red = picker->findChild<QObject *>("color-picker-red");
        CHECK(red && QMetaObject::invokeMethod(red, "stepUp"));
        auto changed = picker->property("currentColor").value<QColor>();
        CHECK(changed.red() == 18 && changed.green() == 34 && changed.blue() == 51 && changed.alpha() == 85);
        CHECK(assField->property("text") == "&HAA332212");
        CHECK(picker->setProperty("hueVal", 120.0) && picker->setProperty("satVal", 1.0)
              && picker->setProperty("valVal", 0.5));
        CHECK(QMetaObject::invokeMethod(picker, "updateFromHsv"));
        changed = picker->property("currentColor").value<QColor>();
        CHECK(changed.green() == 128 && changed.red() == 0 && changed.blue() == 0 && changed.alpha() == 85);
        CHECK(red->property("value").toInt() == 0);
        auto *alpha = picker->findChild<QObject *>("color-picker-alpha");
        CHECK(alpha && QMetaObject::invokeMethod(alpha, "stepUp"));
        changed = picker->property("currentColor").value<QColor>();
        CHECK(changed.alpha() == 84 && picker->property("alphaVal").toInt() == 171);
        QSignalSpy accepted(picker, SIGNAL(colorAccepted(QColor,QString,QString)));
        CHECK(accepted.isValid());
        CHECK(QMetaObject::invokeMethod(picker, "acceptColor"));
        CHECK(accepted.size() == 1 && accepted[0][0].value<QColor>() == changed);
        CHECK(accepted[0][1] == "&H008000&" && accepted[0][2] == "&HAB008000");
        CHECK(styleLoader && styleLoader->property("primaryColor").value<QColor>() == changed);
        CHECK(styleDialog->property("primaryColor").value<QColor>() == changed);
        QQmlExpression saveStyle(engine.rootContext(), styleDialog, QStringLiteral("saveStyle(false); true"));
        CHECK(saveStyle.evaluate().toBool() && !saveStyle.hasError());
        CHECK(colorModel.getStyle(0).value("primary") == "&HAB008000");
        const auto colorPath = temporary.filePath("color-alpha.ass");
        CHECK(colorModel.saveToFile(colorPath));
        SubtitleModel reopenedColor;
        CHECK(reopenedColor.loadFromFile(colorPath));
        CHECK(reopenedColor.getStyle(0).value("primary") == "&HAB008000");
        const QColor second(200, 100, 50);
        CHECK(loader->setProperty("currentColor", second));
        CHECK(QMetaObject::invokeMethod(loader, "open"));
        CHECK(picker->property("originalColor").value<QColor>() == second);
        CHECK(picker->property("redVal").toInt() == 200 && picker->property("alphaVal").toInt() == 0);
        CHECK(red->property("value").toInt() == 200 && alpha->property("value").toInt() == 0);
        CHECK(picker->setProperty("pickingScreen", true) && picker->setProperty("visible", false));
        colorBridge.screenColorPicked(QColor(31, 72, 191));
        CHECK(picker->property("visible").toBool() && !picker->property("pickingScreen").toBool());
        CHECK(picker->property("currentColor").value<QColor>() == QColor(31, 72, 191));
        CHECK(picker->property("originalColor").value<QColor>() == second);
        CHECK(picker->setProperty("pickingScreen", true) && picker->setProperty("visible", false));
        colorBridge.screenColorPickCancelled();
        CHECK(picker->property("visible").toBool());
        CHECK(picker->property("currentColor").value<QColor>() == QColor(31, 72, 191));
        CHECK(picker->setProperty("currentColor", initial) && picker->setProperty("pickingScreen", true));
        colorBridge.screenColorPicked(QColor(31, 72, 191));
        CHECK(picker->property("currentColor").value<QColor>() == QColor(31, 72, 191, 85));
        QSignalSpy dropperError(picker, SIGNAL(dropperError(QString)));
        CHECK(dropperError.isValid() && picker->setProperty("pickingScreen", true));
        colorBridge.screenColorPickFailed("capture unavailable");
        CHECK(dropperError.size() == 1 && dropperError[0][0] == "capture unavailable");
        CHECK(picker->property("visible").toBool() && !picker->property("pickingScreen").toBool());
        CHECK(picker->property("currentColor").value<QColor>() == QColor(31, 72, 191, 85));
        CHECK(QMetaObject::invokeMethod(picker, "close"));
        CHECK(accepted.size() == 1 && styleLoader->property("primaryColor").value<QColor>() == changed);
        CHECK(QMetaObject::invokeMethod(styleDialog, "close"));
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&editor)));
        colorManager.reset();
        engine.rootContext()->setContextProperty("aegisubCore", QVariant());
        puts("PASS actual Color Picker external RGBA/ASS synchronization, RGB/HSV preserves alpha, transparency controls, style result, reopen and cancel");
    }
    {
        QQmlComponent videoMockComponent(&engine);
        videoMockComponent.setData("import QtQml; QtObject { property var videoDetails: ({hasVideo:true, fileName:'first.mov', fps:25, width:720, height:480, aspectRatio:'3:2', frameCount:250, length:'0:00:09.960', colorMatrix:'BT.709', colorRange:'Unknown', decoder:'AstraCore / FFmpeg'}); signal videoInfoChanged(); signal timecodesChanged() }", QUrl());
        std::unique_ptr<QObject> videoMock(videoMockComponent.create());
        CHECK(videoMock);
        QQmlComponent managerComponent(&engine, QUrl::fromLocalFile(qmlRoot + "/dialogs/DialogManager.qml"));
        std::unique_ptr<QObject> manager(managerComponent.createWithInitialProperties({{"project", QVariant::fromValue(project.get())},
            {"videoCtrl", QVariant::fromValue(videoMock.get())}}));
        if (!manager) for (const auto &error : managerComponent.errors()) fprintf(stderr, "%s\n", qPrintable(error.toString()));
        CHECK(manager);
        QQmlExpression openDetails(engine.rootContext(), manager.get(), QStringLiteral("dlgVideoDetails.open(); true"));
        CHECK(openDetails.evaluate().toBool() && !openDetails.hasError());
        auto *loader = manager->property("dlgVideoDetails").value<QObject *>();
        CHECK(loader);
        auto *dialog = loader->property("item").value<QObject *>();
        CHECK(dialog && dialog->property("visible").toBool());
        auto *file = dialog->findChild<QObject *>("video-details-file");
        auto *resolution = dialog->findChild<QObject *>("video-details-resolution");
        auto *matrix = dialog->findChild<QObject *>("video-details-matrix");
        auto *scriptMatrix = dialog->findChild<QObject *>("video-details-script-matrix");
        CHECK(file && resolution && matrix && scriptMatrix);
        CHECK(file->property("text") == "first.mov" && resolution->property("text") == "720×480 (3:2)");
        CHECK(matrix->property("text") == "BT.709" && scriptMatrix->property("text") == "None");
        const auto detailsScreenshot = qEnvironmentVariable("AEGISUB_VIDEO_DETAILS_SCREENSHOT");
        if (!detailsScreenshot.isEmpty()) {
            auto *window = qobject_cast<QQuickWindow *>(dialog);
            CHECK(window);
            QTest::qWait(100);
            CHECK(window->grabWindow().save(detailsScreenshot));
        }
        auto next = videoMock->property("videoDetails").toMap();
        next["fileName"] = "second.mkv";
        next["width"] = 1920;
        next["height"] = 1080;
        next["aspectRatio"] = "16:9";
        next["colorMatrix"] = "BT.2020 NCL";
        CHECK(videoMock->setProperty("videoDetails", next));
        CHECK(QMetaObject::invokeMethod(videoMock.get(), "videoInfoChanged"));
        QCoreApplication::processEvents();
        CHECK(file->property("text") == "second.mkv" && resolution->property("text") == "1920×1080 (16:9)");
        CHECK(matrix->property("text") == "BT.2020 NCL");
        CHECK(videoMock->setProperty("videoDetails", QVariantMap{{"hasVideo",false}}));
        CHECK(QMetaObject::invokeMethod(videoMock.get(), "timecodesChanged"));
        QCoreApplication::processEvents();
        CHECK(file->property("text") == "No video open");
        CHECK(QMetaObject::invokeMethod(dialog, "close"));
        puts("PASS actual Video Details dialog binds live video metadata, aspect and script matrix and clears on close");
    }
    {
        SubtitleModel translationModel;
        translationModel.setAllLines({QVariantMap{{"text", "{\\i1}hello{\\i0} world"}},
                                      QVariantMap{{"text", "second line"}},
                                      QVariantMap{{"text", "{note}{\\p1}m 0 0 l 1 1{\\p0}visible"}}});
        translationModel.clearUndo();
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&translationModel)));
        CHECK(project->setProperty("currentSelectedIndex", 0));
        QQmlComponent managerComponent(&engine, QUrl::fromLocalFile(qmlRoot + "/dialogs/DialogManager.qml"));
        std::unique_ptr<QObject> translationManager(managerComponent.createWithInitialProperties(
            {{"project", QVariant::fromValue(project.get())}}));
        if (!translationManager) for (const auto &error : managerComponent.errors())
            fprintf(stderr, "%s\n", qPrintable(error.toString()));
        CHECK(translationManager);
        QQmlExpression openTranslation(engine.rootContext(), translationManager.get(),
                                      QStringLiteral("dlgTranslation.open(); true"));
        CHECK(openTranslation.evaluate().toBool() && !openTranslation.hasError());
        auto *translationLoader = translationManager->property("dlgTranslation").value<QObject *>();
        CHECK(translationLoader);
        auto *translationDialog = translationLoader->property("item").value<QObject *>();
        CHECK(translationDialog && translationDialog->property("visible").toBool());
        auto *translationInput = translationDialog->findChild<QObject *>("translation-input");
        CHECK(translationInput && translationDialog->property("blockIndex").toInt() == 1);
        CHECK(translationInput->setProperty("text", QStringLiteral("你好\n世界")));
        QTest::keyClick(qobject_cast<QQuickWindow *>(translationDialog), Qt::Key_Return);
        CHECK(translationModel.get(0).value("text") == QStringLiteral("{\\i1}你好\\N世界{\\i0} world"));
        CHECK(project->property("currentSelectedIndex").toInt() == 0);
        CHECK(translationDialog->property("blockIndex").toInt() == 3);
        CHECK(translationInput->property("text").toString().isEmpty());
        CHECK(translationInput->setProperty("text", QStringLiteral("朋友")));
        CHECK(QMetaObject::invokeMethod(translationDialog, "acceptCurrent"));
        CHECK(translationModel.get(0).value("text") == QStringLiteral("{\\i1}你好\\N世界{\\i0}朋友"));
        CHECK(project->property("currentSelectedIndex").toInt() == 1);
        CHECK(translationInput->property("text").toString().isEmpty());
        CHECK(QMetaObject::invokeMethod(translationDialog, "close"));
        CHECK(openTranslation.evaluate().toBool() && !openTranslation.hasError());
        CHECK(translationInput->property("text").toString().isEmpty());
        CHECK(translationInput->setProperty("text", QStringLiteral("第二行")));
        CHECK(QMetaObject::invokeMethod(translationDialog, "acceptCurrent"));
        CHECK(translationModel.get(1).value("text") == QStringLiteral("第二行"));
        CHECK(project->property("currentSelectedIndex").toInt() == 2);
        CHECK(translationInput->property("text").toString().isEmpty());
        CHECK(translationDialog->property("blockIndex").toInt() == 4);
        CHECK(translationInput->setProperty("text", QStringLiteral("可见")));
        CHECK(QMetaObject::invokeMethod(translationDialog, "acceptCurrent"));
        CHECK(translationModel.get(2).value("text") == QStringLiteral("{note}{\\p1}m 0 0 l 1 1{\\p0}可见"));
        CHECK(!translationDialog->property("visible").toBool());
        const auto translatedPath = temporary.filePath("translated.ass");
        CHECK(translationModel.saveToFile(translatedPath));
        SubtitleModel translatedReopen;
        CHECK(translatedReopen.loadFromFile(translatedPath));
        CHECK(translatedReopen.get(0).value("text") == QStringLiteral("{\\i1}你好\\N世界{\\i0}朋友"));
        CHECK(translatedReopen.get(1).value("text") == QStringLiteral("第二行"));
        CHECK(translatedReopen.get(2).value("text") == QStringLiteral("{note}{\\p1}m 0 0 l 1 1{\\p0}可见"));
        CHECK(QMetaObject::invokeMethod(project.get(), "undo"));
        CHECK(translationModel.get(2).value("text") == "{note}{\\p1}m 0 0 l 1 1{\\p0}visible");
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&editor)));
        puts("PASS actual Translation Assistant clears stale input, advances plain blocks, preserves override tags and saves multiline ASS text");
    }
    {
        SubtitleModel assistantModel;
        assistantModel.newDocument();
        auto secondStyle = assistantModel.getStyle(0);
        secondStyle["name"] = "Alternate";
        assistantModel.addStyle(secondStyle);
        QVariantList lines;
        for (int row = 0; row < 40; ++row)
            lines.append(QVariantMap{{"text", row == 1 ? "{\\p1}m 0 0 l 1 1" : "{\\i1}one{\\i0}two"}, {"style", "Default"}});
        assistantModel.setAllLines(lines);
        CHECK(assistantModel.setLineTimes(0, 1011, 1999));
        CHECK(assistantModel.setLineTimes(2, 3001, 4555));
        CHECK(assistantModel.setLineTimes(35, 10011, 11999));
        CHECK(assistantModel.setLineTimes(36, 12001, 15555));
        assistantModel.clearUndo();
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&assistantModel)));
        CHECK(project->setProperty("currentSelectedIndex", 0));
        QQmlComponent mediaComponent(&engine);
        mediaComponent.setData("import QtQml; QtObject { property var seeks: []; property var audio: []; property var active: []; property int plays: 0; function seekTime(time) { seeks = seeks.concat([time]); } function playRange(start,end) { audio = [start,end]; } function setActiveSubtitle(start,end,text) { active = [start,end,text]; } function playCurrentLine() { plays++; } }", QUrl());
        std::unique_ptr<QObject> media(mediaComponent.create());
        CHECK(media);
        QQmlComponent managerComponent(&engine, QUrl::fromLocalFile(qmlRoot + "/dialogs/DialogManager.qml"));
        std::unique_ptr<QObject> assistantManager(managerComponent.createWithInitialProperties(
            {{"project", QVariant::fromValue(project.get())}, {"videoCtrl", QVariant::fromValue(media.get())}, {"audioCtrl", QVariant::fromValue(media.get())}}));
        CHECK(assistantManager);
        QQmlExpression openTranslation(engine.rootContext(), assistantManager.get(), "dlgTranslation.open(); true");
        CHECK(openTranslation.evaluate().toBool() && !openTranslation.hasError());
        auto *translation = assistantManager->property("dlgTranslation").value<QObject *>()->property("item").value<QObject *>();
        auto *input = translation->findChild<QQuickItem *>("translation-input");
        auto *window = qobject_cast<QQuickWindow *>(translation);
        CHECK(input && window && media->property("seeks").value<QJSValue>().toVariant().toList().last().toDouble() == 1.011);
        input->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Return, Qt::AltModifier);
        CHECK(input->property("text") == "one" && !assistantModel.canUndo());
        CHECK(translation->property("blockIndex").toInt() == 1);
        QTest::keyClick(window, Qt::Key_PageUp);
        CHECK(project->property("currentSelectedIndex").toInt() == 0 && translation->property("blockIndex").toInt() == 1);
        CHECK(input->property("text") == "one");
        QTest::keyClick(window, Qt::Key_PageDown);
        CHECK(translation->property("blockIndex").toInt() == 3 && input->property("text").toString().isEmpty());
        QTest::keyClick(window, Qt::Key_PageDown);
        CHECK(project->property("currentSelectedIndex").toInt() == 2 && translation->property("blockIndex").toInt() == 1);
        QTest::keyClick(window, Qt::Key_PageUp);
        CHECK(project->property("currentSelectedIndex").toInt() == 0 && translation->property("blockIndex").toInt() == 3);
        QTest::keyClick(window, Qt::Key_PageUp);
        CHECK(translation->property("blockIndex").toInt() == 1);
        CHECK(input->setProperty("text", "draft"));
        QTest::keyClick(window, Qt::Key_Return, Qt::ShiftModifier);
        CHECK(input->property("text").toString().contains('\n') && !assistantModel.canUndo());
        CHECK(input->setProperty("text", "preview"));
        QTest::keyClick(window, Qt::Key_F8);
        CHECK(assistantModel.get(0).value("text") == "{\\i1}preview{\\i0}two");
        CHECK(project->property("currentSelectedIndex").toInt() == 0 && translation->property("blockIndex").toInt() == 1);
        CHECK(input->property("text").toString().isEmpty() && assistantModel.canUndo());
        QTest::keyClick(window, Qt::Key_F1);
        CHECK(media->property("audio").value<QJSValue>().toVariant().toList() == QVariantList({1011,1999}));
        QTest::keyClick(window, Qt::Key_F2);
        CHECK(media->property("plays").toInt() == 1);
        CHECK(media->property("active").value<QJSValue>().toVariant().toList() == QVariantList({1011,1999,"{\\i1}preview{\\i0}two"}));
        auto *preview = translation->findChild<QQuickItem *>("translation-preview");
        CHECK(preview);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            preview->mapToScene(QPointF(preview->width()/2, preview->height()/2)).toPoint());
        CHECK(!translation->property("enablePreview").toBool());
        const auto seekCount = media->property("seeks").value<QJSValue>().toVariant().toList().size();
        QTest::keyClick(window, Qt::Key_PageDown);
        QTest::keyClick(window, Qt::Key_PageDown);
        CHECK(project->property("currentSelectedIndex").toInt() == 2);
        CHECK(media->property("seeks").value<QJSValue>().toVariant().toList().size() == seekCount);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            preview->mapToScene(QPointF(preview->width()/2, preview->height()/2)).toPoint());
        CHECK(translation->property("enablePreview").toBool());
        CHECK(media->property("seeks").value<QJSValue>().toVariant().toList().last().toDouble() == 3.001);
        CHECK(project->setProperty("currentSelectedIndex", 39));
        input->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_PageDown);
        QTest::keyClick(window, Qt::Key_PageDown);
        CHECK(project->property("currentSelectedIndex").toInt() == 39 && translation->property("blockIndex").toInt() == 3);
        CHECK(translation->property("visible").toBool());
        const auto translationScreenshot = qEnvironmentVariable("AEGISUB_TRANSLATION_KEYS_SCREENSHOT");
        if (!translationScreenshot.isEmpty()) {
            QTest::qWait(80);
            CHECK(window->grabWindow().save(translationScreenshot));
        }
        CHECK(QMetaObject::invokeMethod(translation, "close"));

        QQuickWindow gridWindow;
        gridWindow.resize(800, 180);
        QQmlComponent gridComponent(&engine, QUrl::fromLocalFile(qmlRoot + "/views/SubtitleGridArea.qml"));
        std::unique_ptr<QObject> gridObject(gridComponent.createWithInitialProperties({{"project", QVariant::fromValue(project.get())}, {"width",800}, {"height",180}}));
        if (!gridObject) for (const auto &error : gridComponent.errors()) fprintf(stderr, "%s\n", qPrintable(error.toString()));
        CHECK(gridObject);
        auto *grid = qobject_cast<QQuickItem *>(gridObject.get());
        CHECK(grid);
        grid->setParentItem(gridWindow.contentItem());
        gridWindow.show();
        QTest::qWait(50);
        auto *list = grid->findChild<QObject *>("subtitle-grid-list");
        CHECK(list);
        CHECK(project->setProperty("currentSelectedIndex", 35));
        QQmlExpression openStyling(engine.rootContext(), assistantManager.get(), "dlgStylingAssistant.open(); true");
        CHECK(openStyling.evaluate().toBool() && !openStyling.hasError());
        auto *styling = assistantManager->property("dlgStylingAssistant").value<QObject *>()->property("item").value<QObject *>();
        auto *styleInput = styling->findChild<QQuickItem *>("styling-input");
        auto *styleWindow = qobject_cast<QQuickWindow *>(styling);
        CHECK(styleInput && styleWindow);
        QTest::qWait(50);
        CHECK(list->property("contentY").toDouble() > 500);
        CHECK(styleInput->setProperty("text", "Alternate"));
        styleInput->forceActiveFocus();
        QTest::keyClick(styleWindow, Qt::Key_F8);
        CHECK(assistantModel.get(35).value("style") == "Alternate" && project->property("currentSelectedIndex").toInt() == 35);
        QTest::keyClick(styleWindow, Qt::Key_PageDown);
        CHECK(project->property("currentSelectedIndex").toInt() == 36 && styleInput->property("text") == "Default");
        QTest::keyClick(styleWindow, Qt::Key_PageUp);
        CHECK(project->property("currentSelectedIndex").toInt() == 35 && styleInput->property("text") == "Alternate");
        QTest::keyClick(styleWindow, Qt::Key_F1);
        QTest::keyClick(styleWindow, Qt::Key_F2);
        CHECK(media->property("plays").toInt() == 2);
        CHECK(media->property("audio").value<QJSValue>().toVariant().toList() == QVariantList({10011,11999}));
        CHECK(media->property("active").value<QJSValue>().toVariant().toList() == QVariantList({10011,11999,"{\\i1}one{\\i0}two"}));
        for (const auto &name : {"styling-scroll", "styling-seek"}) {
            auto *control = styling->findChild<QQuickItem *>(name);
            CHECK(control);
            QTest::mouseClick(styleWindow, Qt::LeftButton, Qt::NoModifier,
                control->mapToScene(QPointF(control->width()/2, control->height()/2)).toPoint());
        }
        CHECK(!styling->property("scrollToCurrent").toBool() && !styling->property("seekVideo").toBool());
        const auto beforeSeek = media->property("seeks").value<QJSValue>().toVariant().toList().size();
        CHECK(list->setProperty("contentY", 0));
        QTest::keyClick(styleWindow, Qt::Key_PageDown);
        QTest::qWait(30);
        CHECK(project->property("currentSelectedIndex").toInt() == 36 && list->property("contentY").toDouble() == 0);
        CHECK(media->property("seeks").value<QJSValue>().toVariant().toList().size() == beforeSeek);
        CHECK(styleInput->setProperty("text", "Alternate"));
        QTest::keyClick(styleWindow, Qt::Key_Return);
        CHECK(assistantModel.get(36).value("style") == "Alternate" && project->property("currentSelectedIndex").toInt() == 37);
        CHECK(styleInput->setProperty("text", "Missing style"));
        QTest::keyClick(styleWindow, Qt::Key_Return);
        CHECK(project->property("currentSelectedIndex").toInt() == 37 && assistantModel.get(37).value("style") == "Default");
        auto *scroll = styling->findChild<QQuickItem *>("styling-scroll");
        CHECK(scroll);
        QTest::mouseClick(styleWindow, Qt::LeftButton, Qt::NoModifier,
            scroll->mapToScene(QPointF(scroll->width()/2, scroll->height()/2)).toPoint());
        CHECK(styling->property("scrollToCurrent").toBool() && list->property("contentY").toDouble() > 500);
        const auto stylingScreenshot = qEnvironmentVariable("AEGISUB_STYLING_KEYS_SCREENSHOT");
        if (!stylingScreenshot.isEmpty()) {
            QTest::qWait(80);
            CHECK(styleWindow->grabWindow().save(stylingScreenshot));
        }
        CHECK(QMetaObject::invokeMethod(styling, "close"));
        assistantManager.reset();
        gridObject.reset();
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&editor)));
        puts("PASS production assistants actual Alt/Shift/Enter, Page Up/Down block/row navigation, F1/F2/F8, precise playback times, preview checkbox and real grid scroll opt-out");
    }
    {
        SubtitleModel sortModel;
        sortModel.setAllLines({QVariantMap{{"text","A"},{"layer",3}},
            QVariantMap{{"text","B"},{"layer",0}},
            QVariantMap{{"text","C"},{"layer",1}},
            QVariantMap{{"text","D"},{"layer",0}},
            QVariantMap{{"text","E"},{"layer",2}}});
        sortModel.clearUndo();
        const auto originalRows = sortModel.getAllLines();
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&sortModel)));
        CHECK(project->setProperty("selectedIndices", QVariantList{0,2,4}));
        CHECK(project->setProperty("currentSelectedIndex", 2));
        QQmlExpression selectedSort(engine.rootContext(), project.get(),
            QStringLiteral("sortLines('layer', true); true"));
        CHECK(selectedSort.evaluate().toBool() && !selectedSort.hasError());
        CHECK(sortModel.get(0).value("text") == "C" && sortModel.get(1).value("text") == "B");
        CHECK(sortModel.get(2).value("text") == "E" && sortModel.get(3).value("text") == "D");
        CHECK(sortModel.get(4).value("text") == "A");
        CHECK(sortModel.canUndo());
        QQmlExpression undoSort(engine.rootContext(), project.get(), QStringLiteral("undo(); true"));
        CHECK(undoSort.evaluate().toBool() && !undoSort.hasError());
        CHECK(sortModel.getAllLines() == originalRows);
        CHECK(project->property("selectedIndices").toList() == QVariantList({0,2,4}));
        QQmlExpression allSort(engine.rootContext(), project.get(),
            QStringLiteral("sortLines('layer', false); true"));
        CHECK(allSort.evaluate().toBool() && !allSort.hasError());
        CHECK(sortModel.get(0).value("text") == "B" && sortModel.get(1).value("text") == "D");
        CHECK(sortModel.get(2).value("text") == "C" && sortModel.get(3).value("text") == "E");
        CHECK(sortModel.get(4).value("text") == "A");
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&editor)));
        puts("PASS actual SubtitleProject selected layer sort preserves unselected rows and undo selection; all-lines layer sort remains global");
    }
    {
        SubtitleModel recombine;
        recombine.setAllLines({QVariantMap{{"text","A"},{"start","0:00:01.00"},{"end","0:00:02.00"}},
            QVariantMap{{"text","A"},{"start","0:00:01.50"},{"end","0:00:03.00"}},
            QVariantMap{{"text","keep"},{"start","0:00:03.00"},{"end","0:00:04.00"}}});
        recombine.clearUndo();
        const auto originalRows = recombine.getAllLines();
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&recombine)));
        CHECK(project->setProperty("selectedIndices", QVariantList{0,1}));
        CHECK(project->setProperty("currentSelectedIndex", 0));
        QQmlExpression combineCall(engine.rootContext(), project.get(),
            QStringLiteral("recombineSelectedLines(); true"));
        CHECK(combineCall.evaluate().toBool() && !combineCall.hasError());
        CHECK(recombine.rowCount() == 2 && recombine.get(0).value("text") == "A");
        CHECK(recombine.getLineStartMs(0) == 1000 && recombine.getLineEndMs(0) == 3000);
        CHECK(recombine.get(1).value("text") == "keep");
        QQmlExpression selectedAfterCombine(engine.rootContext(), project.get(),
            QStringLiteral("selectedIndices.length === 1 && selectedIndices[0] === 0"));
        CHECK(selectedAfterCombine.evaluate().toBool() && !selectedAfterCombine.hasError());
        CHECK(project->property("currentSelectedIndex").toInt() == 0);
        QQmlExpression undoCombine(engine.rootContext(), project.get(), QStringLiteral("undo(); true"));
        CHECK(undoCombine.evaluate().toBool() && !undoCombine.hasError());
        CHECK(recombine.getAllLines() == originalRows);
        CHECK(project->property("selectedIndices").toList() == QVariantList({0,1}));
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&editor)));
        puts("PASS actual SubtitleProject recombine deletes duplicate, expands time and restores selection on undo");
    }
    {
        SubtitleModel timingEditor;
        timingEditor.setAllLines({QVariantMap{{"text","outside"},{"start","0:00:00.00"},{"end","0:00:01.00"}},
            QVariantMap{{"text","selected"},{"start","0:00:01.10"},{"end","0:00:02.00"}}});
        timingEditor.clearUndo();
        const auto original = timingEditor.getAllLines();
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&timingEditor)));
        CHECK(project->setProperty("selectedIndices", QVariantList{1}));
        CHECK(project->setProperty("currentSelectedIndex", 1));
        QQmlComponent managerComponent(&engine, QUrl::fromLocalFile(qmlRoot + "/dialogs/DialogManager.qml"));
        std::unique_ptr<QObject> manager(managerComponent.createWithInitialProperties({{"project", QVariant::fromValue(project.get())}}));
        if (!manager) for (const auto &error : managerComponent.errors()) fprintf(stderr, "%s\n", qPrintable(error.toString()));
        CHECK(manager);
        QQmlExpression openTiming(engine.rootContext(), manager.get(), QStringLiteral("dlgTimingProcessor.open(); true"));
        CHECK(openTiming.evaluate().toBool() && !openTiming.hasError());
        auto *loader = manager->property("dlgTimingProcessor").value<QObject *>();
        CHECK(loader);
        auto *timingDialog = loader->property("item").value<QObject *>();
        CHECK(timingDialog && timingDialog->property("visible").toBool());
        CHECK(timingDialog->setProperty("stylesModel", QVariantList{QVariantMap{{"name","Default"},{"checked",false}}}));
        QQmlExpression applyTiming(engine.rootContext(), timingDialog, QStringLiteral("doProcess(false); true"));
        CHECK(applyTiming.evaluate().toBool() && !applyTiming.hasError());
        CHECK(!timingDialog->property("errorMessage").toString().isEmpty());
        CHECK(timingEditor.getAllLines() == original && !timingEditor.canUndo());
        CHECK(timingDialog->setProperty("stylesModel", QVariantList{QVariantMap{{"name","Default"},{"checked",true}}}));
        auto *scope = timingDialog->findChild<QObject *>("timing-selection-only");
        CHECK(scope && scope->setProperty("checked", true));
        CHECK(applyTiming.evaluate().toBool() && !applyTiming.hasError());
        CHECK(timingDialog->property("errorMessage").toString().isEmpty());
        CHECK(timingEditor.get(0) == original[0].toMap());
        CHECK(timingEditor.getLineStartMs(1) != 1100 && timingEditor.canUndo());
        QQmlExpression undoTiming(engine.rootContext(), project.get(), QStringLiteral("undo(); true"));
        CHECK(undoTiming.evaluate().toBool() && !undoTiming.hasError());
        CHECK(timingEditor.getAllLines() == original);
        QQmlExpression timingUndoSelection(engine.rootContext(), project.get(),
            QStringLiteral("selectedIndices.length === 1 && selectedIndices[0] === 1"));
        CHECK(timingUndoSelection.evaluate().toBool() && !timingUndoSelection.hasError());
        CHECK(QMetaObject::invokeMethod(timingDialog, "close"));
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&editor)));
        puts("PASS actual DialogManager/TimingProcessor style None, selected-only isolation, successful Apply and one undo");
    }
    {
        SubtitleModel styleEditorModel;
        auto oldStyle = styleEditorModel.getStyle(0);
        oldStyle["name"] = "Old";
        auto otherStyle = oldStyle;
        otherStyle["name"] = "Other";
        styleEditorModel.setStyles({styleEditorModel.getStyle(0), oldStyle, otherStyle});
        styleEditorModel.setAllLines({QVariantMap{{"style","Old"},{"text","{\\rOld}a"}}, QVariantMap{{"style","Other"},{"text","{\\t(0,100,\\rOld)}b"}}});
        const auto originalRows = styleEditorModel.getAllLines();
        const auto originalStyles = styleEditorModel.styles();
        styleEditorModel.clearUndo();
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&styleEditorModel)));
        CHECK(project->setProperty("selectedIndices", QVariantList{0,1}));
        CHECK(project->setProperty("currentSelectedIndex", 1));
        QQmlComponent managerComponent(&engine, QUrl::fromLocalFile(qmlRoot + "/dialogs/DialogManager.qml"));
        std::unique_ptr<QObject> styleManager(managerComponent.createWithInitialProperties({{"project", QVariant::fromValue(project.get())}}));
        if (!styleManager) for (const auto &error : managerComponent.errors()) fprintf(stderr, "%s\n", qPrintable(error.toString()));
        CHECK(styleManager);
        auto *styleLoader = styleManager->property("dlgStyleEditor").value<QObject *>();
        CHECK(styleLoader);
        auto loadEditor = [&] {
            QQmlExpression code(engine.rootContext(), styleManager.get(), "dlgStyleEditor.loadStyle(project.subtitleModel.getStyle(1), false, 1); dlgStyleEditor.open(); true");
            const auto value = code.evaluate();
            if (code.hasError()) fprintf(stderr, "%s\n", qPrintable(code.error().toString()));
            return !code.hasError() && value.toBool();
        };
        CHECK(loadEditor());
        auto *styleDialog = styleLoader->property("item").value<QObject *>();
        auto *styleWindow = qobject_cast<QQuickWindow *>(styleDialog);
        CHECK(styleWindow && styleDialog->property("visible").toBool());
        std::function<QQuickItem *(QQuickItem *, const QString &)> findStyle = [&](QQuickItem *parent, const QString &name) -> QQuickItem * {
            if (parent->objectName() == name) return parent;
            for (auto *child : parent->childItems()) if (auto *found = findStyle(child, name)) return found;
            return nullptr;
        };
        auto control = [&](const char *name) { return findStyle(styleWindow->contentItem(), QString("style-") + name); };
        auto click = [&](const char *name) {
            auto *item = control(name);
            if (!item || !item->isVisible() || !item->isEnabled()) return false;
            const auto point = item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
            QTest::mouseClick(styleWindow, Qt::LeftButton, Qt::NoModifier, point);
            QCoreApplication::processEvents();
            return true;
        };
        auto typeName = [&](const QByteArray &name) {
            auto *field = control("editor-name");
            if (!field) return false;
            styleWindow->requestActivate();
            field->forceActiveFocus();
            if (!field->hasActiveFocus()) return false;
            QTest::keySequence(styleWindow, QKeySequence(QKeySequence::SelectAll));
            QTest::keyClick(styleWindow, Qt::Key_Backspace);
            for (char c : name) QTest::keyClick(styleWindow, c);
            return true;
        };
        CHECK(click("editor-apply"));
        CHECK(styleEditorModel.getAllLines() == originalRows && styleEditorModel.styles() == originalStyles && !styleEditorModel.canUndo());
        CHECK(typeName("Other") && click("editor-apply"));
        CHECK(!styleDialog->property("errorMessage").toString().isEmpty() && styleEditorModel.styles() == originalStyles && !styleEditorModel.canUndo());
        const auto styleScreenshot = qEnvironmentVariable("AEGISUB_STYLE_TEST_SCREENSHOT");
        if (!styleScreenshot.isEmpty()) { QTest::qWait(100); CHECK(styleWindow->grabWindow().save(styleScreenshot + ".error.png")); }
        CHECK(typeName("New") && click("editor-apply"));
        auto *prompt = styleDialog->property("renamePrompt").value<QObject *>();
        CHECK(prompt && prompt->property("visible").toBool());
        if (!styleScreenshot.isEmpty()) { QTest::qWait(100); CHECK(styleWindow->grabWindow().save(styleScreenshot)); }
        CHECK(click("rename-cancel"));
        CHECK(styleEditorModel.styles() == originalStyles && styleEditorModel.getAllLines() == originalRows && !styleEditorModel.canUndo());
        CHECK(click("editor-apply") && prompt->property("visible").toBool());
        CHECK(click("rename-no"));
        CHECK(styleEditorModel.getStyle(1).value("name") == "New" && styleEditorModel.getAllLines() == originalRows);
        CHECK(styleEditorModel.canUndo() && QMetaObject::invokeMethod(project.get(), "undo"));
        CHECK(styleEditorModel.styles() == originalStyles && styleEditorModel.getAllLines() == originalRows);
        CHECK(loadEditor());
        CHECK(typeName("Renamed") && click("editor-apply"));
        CHECK(prompt->property("visible").toBool() && click("rename-yes"));
        CHECK(styleEditorModel.getStyle(1).value("name") == "Renamed");
        CHECK(styleEditorModel.get(0).value("style") == "Renamed" && styleEditorModel.get(0).value("text") == "{\\rRenamed}a");
        CHECK(styleEditorModel.get(1).value("text") == "{\\t(0,100,\\rRenamed)}b");
        CHECK(QMetaObject::invokeMethod(project.get(), "undo"));
        CHECK(styleEditorModel.getAllLines() == originalRows && styleEditorModel.styles() == originalStyles);
        CHECK(QMetaObject::invokeMethod(styleDialog, "close"));
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&editor)));
        CHECK(project->setProperty("selectedIndices", QVariantList{0}));
        CHECK(project->setProperty("currentSelectedIndex", 0));
        puts("PASS actual Style Editor/DialogManager conflict, rename Yes/No/Cancel, nested/direct reference, single undo and unchanged failures");
    }
    {
        SubtitleModel karaokeEditor;
        karaokeEditor.setAllLines({QVariantMap{{"text", "{\\pos(100,100)\\k10}a{\\k10\\i1}b"}, {"start", "0:00:01.00"}, {"end", "0:00:01.50"}},
            QVariantMap{{"text", "plain"}}, QVariantMap{{"text", "{\\ko10}x{\\K10}y"}}});
        const auto original = karaokeEditor.getAllLines();
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&karaokeEditor)));
        CHECK(project->setProperty("selectedIndices", QVariantList{0,2}));
        CHECK(project->setProperty("currentSelectedIndex", 2));
        CHECK(QMetaObject::invokeMethod(project.get(), "splitSelectedByKaraoke"));
        auto selection = project->property("selectedIndices");
        if (selection.typeId() == qMetaTypeId<QJSValue>()) selection = selection.value<QJSValue>().toVariant();
        CHECK(selection.toList() == QVariantList({0,1,3,4}));
        CHECK(karaokeEditor.rowCount() == 5 && karaokeEditor.get(0).value("text") == "{\\pos(100,100)}a" && karaokeEditor.get(1).value("text") == "{\\i1}b");
        CHECK(project->property("currentSelectedIndex").toInt() == 0);
        CHECK(QMetaObject::invokeMethod(project.get(), "undo"));
        CHECK(karaokeEditor.getAllLines() == original && !karaokeEditor.canUndo());
        CHECK(project->property("currentSelectedIndex").toInt() == 2);
        CHECK(project->setProperty("selectedIndices", QVariantList{1}));
        CHECK(QMetaObject::invokeMethod(project.get(), "splitSelectedByKaraoke"));
        CHECK(karaokeEditor.getAllLines() == original && !karaokeEditor.canUndo());
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&editor)));
        CHECK(project->setProperty("selectedIndices", QVariantList{0}));
        CHECK(project->setProperty("currentSelectedIndex", 0));
        puts("PASS actual SubtitleProject karaoke split preserves tags, complete multi-row selection, one undo with active selection restore and no-op isolation");
    }
    {
        SubtitleModel resampleEditor;
        resampleEditor.setScriptInfo({{"PlayResX",640},{"PlayResY",480},{"YCbCr Matrix","TV.601"}});
        resampleEditor.setAllLines({QVariantMap{{"text", "{\\pos(100,200)\\1c&H0000FF&}ui"}}});
        const auto originalRows = resampleEditor.getAllLines();
        const auto originalInfo = resampleEditor.scriptInfo();
        const auto originalStyles = resampleEditor.styles();
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&resampleEditor)));
        QQmlComponent resampleComponent(&engine, QUrl::fromLocalFile(qmlRoot + "/dialogs/DialogManager.qml"));
        std::unique_ptr<QObject> dialogManager(resampleComponent.createWithInitialProperties({{"project", QVariant::fromValue(project.get())}}));
        if (!dialogManager) for (const auto &error : resampleComponent.errors()) fprintf(stderr, "%s\n", qPrintable(error.toString()));
        CHECK(dialogManager);
        auto *loader = dialogManager->property("dlgResample").value<QObject *>();
        CHECK(loader && QMetaObject::invokeMethod(loader, "open"));
        auto *resampleDialog = loader->property("item").value<QObject *>();
        CHECK(resampleDialog);
        QCoreApplication::processEvents();
        CHECK(resampleDialog->property("visible").toBool());
        auto *resampleWindow = qobject_cast<QQuickWindow *>(resampleDialog);
        CHECK(resampleWindow);
        std::function<QQuickItem *(QQuickItem *, const QString &)> find = [&](QQuickItem *parent, const QString &name) -> QQuickItem * {
            if (parent->objectName() == name) return parent;
            for (auto *child : parent->childItems()) if (auto *found = find(child, name)) return found;
            return nullptr;
        };
        auto control = [&](const char *name) { return find(resampleWindow->contentItem(), QString("resample-") + name); };
        auto type = [&](const char *name, const QByteArray &text) {
            auto *item = control(name);
            if (!item || !item->isEnabled() || !item->isVisible()) return false;
            item->forceActiveFocus();
            if (!item->hasActiveFocus()) return false;
            QTest::keySequence(resampleWindow, QKeySequence(QKeySequence::SelectAll));
            QTest::keyClick(resampleWindow, Qt::Key_Backspace);
            for (char c : text) QTest::keyClick(resampleWindow, c);
            return true;
        };
        auto click = [&](const char *name) {
            auto *item = control(name);
            if (!item || !item->isEnabled() || !item->isVisible()) return false;
            const auto point = item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
            QTest::mouseClick(resampleWindow, Qt::LeftButton, Qt::NoModifier, point);
            QCoreApplication::processEvents();
            return true;
        };
        resampleWindow->requestActivate();
        CHECK(control("source-width")->property("text").toString() == "640");
        CHECK(control("source-height")->property("text").toString() == "480");
        CHECK(type("dest-width", "1280") && type("dest-height", "720"));
        CHECK(control("preset")->property("currentIndex").toInt() == 2);
        CHECK(click("manual"));
        CHECK(control("manual")->property("checked").toBool());
        CHECK(type("left", "80"));
        CHECK(control("right")->property("text").toString() == "80");
        CHECK(click("symmetrical"));
        CHECK(control("right")->property("text").toString() == "80");
        CHECK(type("right", "40"));
        CHECK(click("symmetrical"));
        CHECK(control("right")->property("text").toString() == "80");
        for (const auto &[name, steps] : {std::pair{"source-matrix", 1}, std::pair{"dest-matrix", 3}}) {
            auto *item = control(name);
            CHECK(item);
            item->forceActiveFocus();
            for (int i = 0; i < steps; ++i) QTest::keyClick(resampleWindow, Qt::Key_Down);
        }
        CHECK(control("source-matrix")->property("currentText").toString() == "TV.601");
        CHECK(control("dest-matrix")->property("currentText").toString() == "TV.709");
        const auto screenshot = qEnvironmentVariable("AEGISUB_RESAMPLE_TEST_SCREENSHOT");
        if (!screenshot.isEmpty()) { QTest::qWait(100); CHECK(resampleWindow->grabWindow().save(screenshot)); }
        CHECK(type("dest-width", ""));
        CHECK(click("ok"));
        CHECK(resampleDialog->property("visible").toBool() && !resampleDialog->property("errorMessage").toString().isEmpty());
        if (!screenshot.isEmpty()) { QTest::qWait(100); CHECK(resampleWindow->grabWindow().save(screenshot + ".error.png")); }
        CHECK(resampleEditor.getAllLines() == originalRows && resampleEditor.scriptInfo() == originalInfo);
        CHECK(type("dest-width", "1280"));
        CHECK(type("left", "-640"));
        CHECK(click("ok"));
        CHECK(resampleDialog->property("visible").toBool() && !resampleDialog->property("errorMessage").toString().isEmpty());
        CHECK(resampleEditor.getAllLines() == originalRows && resampleEditor.scriptInfo() == originalInfo && !resampleEditor.canUndo());
        CHECK(type("left", "80"));
        CHECK(click("ok"));
        CHECK(!resampleDialog->property("visible").toBool());
        CHECK(resampleEditor.get(0).value("text") == "{\\pos(288,300)\\1c&H0019FF&}ui");
        CHECK(resampleEditor.scriptInfo().value("YCbCr Matrix") == "TV.709");
        CHECK(resampleEditor.canUndo());
        CHECK(QMetaObject::invokeMethod(project.get(), "undo"));
        CHECK(resampleEditor.getAllLines() == originalRows && resampleEditor.styles() == originalStyles && resampleEditor.scriptInfo() == originalInfo);
        CHECK(QMetaObject::invokeMethod(project.get(), "redo"));
        CHECK(resampleEditor.get(0).value("text") == "{\\pos(288,300)\\1c&H0019FF&}ui");
        CHECK(QMetaObject::invokeMethod(loader, "open"));
        CHECK(control("source-width")->property("text").toString() == "1280");
        CHECK(click("from-script"));
        CHECK(control("source-matrix")->property("currentText").toString() == "TV.709");
        CHECK(click("cancel"));
        CHECK(!resampleDialog->property("visible").toBool());
        CHECK(project->setProperty("subtitleModel", QVariant::fromValue(&editor)));
        QCoreApplication::processEvents();
        puts("PASS actual DialogManager/Resample field typing, manual mode, symmetric/asymmetric offsets, matrix selection, invalid input keeps dialog/document, real model resample, project undo/redo, reopen and cancel");
    }
    QQmlComponent exportComponent(&engine, QUrl::fromLocalFile(qmlRoot + "/dialogs/DialogExport.qml"));
    std::unique_ptr<QObject> dialog(exportComponent.createWithInitialProperties({{"project", QVariant::fromValue(project.get())}}));
    if (!dialog) for (const auto &error : exportComponent.errors()) fprintf(stderr, "%s\n", qPrintable(error.toString()));
    CHECK(dialog);
    auto evaluate = [&](const QString &expression) {
        QQmlExpression code(engine.rootContext(), dialog.get(), expression);
        auto value = code.evaluate();
        if (code.hasError()) fprintf(stderr, "%s\n", qPrintable(code.error().toString()));
        if (value.typeId() == qMetaTypeId<QJSValue>()) value = value.value<QJSValue>().toVariant();
        return value;
    };
    CHECK(QMetaObject::invokeMethod(dialog.get(), "open"));
    QCoreApplication::processEvents();
    CHECK(evaluate("filtersModel.length").toInt() == 3);
    CHECK(evaluate("selectFilter(filtersModel.findIndex(f => f.name === 'UI configured filter')); filtersModel[selectedFilterIndex].name").toString() == "UI configured filter");
    auto *window = qobject_cast<QQuickWindow *>(dialog.get());
    CHECK(window);
    std::function<QQuickItem *(QQuickItem *, const QString &)> findVisual = [&](QQuickItem *parent, const QString &name) -> QQuickItem * {
        if (parent->objectName() == name) return parent;
        for (auto *child : parent->childItems()) if (auto *found = findVisual(child, name)) return found;
        return nullptr;
    };
    auto findField = [&](const QString &name) { return findVisual(window->contentItem(), "automation-config-" + name); };
    auto typeText = [&](const QString &name, const QByteArray &text) {
        auto *item = findField(name);
        if (!item) return false;
        item->forceActiveFocus();
        if (!item->hasActiveFocus()) return false;
        QTest::keySequence(window, QKeySequence(QKeySequence::SelectAll));
        for (char character : text) {
            if (character == '\n') QTest::keyClick(window, Qt::Key_Return);
            else QTest::keyClick(window, character);
        }
        return true;
    };
    CHECK(typeText("text", "Edited"));
    CHECK(typeText("multiline", "first\nsecond"));
    CHECK(typeText("integer", "42"));
    CHECK(typeText("number", "1.25"));
    auto *numericField = findField("integer");
    CHECK(numericField);
    numericField->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Up);
    CHECK(evaluate("configForm.readback().settings.integer").toDouble() == 43);
    QTest::keyClick(window, Qt::Key_Down);
    CHECK(evaluate("configForm.readback().settings.integer").toDouble() == 42);
    auto *scroll = findVisual(window->contentItem(), "automation-config-scroll");
    CHECK(scroll);
    auto *flickable = scroll->property("contentItem").value<QObject *>();
    CHECK(flickable && flickable->setProperty("contentY", 190));
    QCoreApplication::processEvents();
    auto clickControl = [&](const QString &name) {
        auto *item = findVisual(window->contentItem(), name);
        if (!item) return false;
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        QCoreApplication::processEvents();
        return true;
    };
    CHECK(evaluate("selectFilter(filtersModel.findIndex(f => f.name === 'Transform Framerate')); configForm.readback().settings.inputFps").toDouble() == 25);
    CHECK(clickControl("automation-config-button-inputFps"));
    CHECK(evaluate("configForm.readback().settings.inputFps").toDouble() == 31.25);
    CHECK(typeText("inputFps", "25"));
    auto *modeField = findField("outputMode");
    auto *outputFpsField = findField("outputFps");
    CHECK(modeField && outputFpsField && !outputFpsField->isEnabled());
    modeField->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Up);
    CHECK(outputFpsField->isEnabled());
    CHECK(typeText("outputFps", "50"));
    CHECK(evaluate("configForm.readback().settings.outputFps").toDouble() == 50);
    CHECK(typeText("outputFps", "invalid"));
    CHECK(!evaluate("configForm.readback().success").toBool());
    modeField->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Down);
    CHECK(!outputFpsField->isEnabled() && evaluate("configForm.readback().success").toBool());
    CHECK(evaluate("selectFilter(filtersModel.findIndex(f => f.name === 'UI configured filter')); configForm.readback().settings.integer").toDouble() == 42);
    CHECK(flickable->setProperty("contentY", 190));
    QCoreApplication::processEvents();
    CHECK(clickControl("automation-step-up-number"));
    CHECK(evaluate("configForm.readback().settings.number").toDouble() == 1.5);
    CHECK(clickControl("automation-step-down-number"));
    CHECK(evaluate("configForm.readback().settings.number").toDouble() == 1.25);
    CHECK(typeText("integer", "100"));
    numericField->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Up);
    CHECK(evaluate("configForm.readback().settings.integer").toDouble() == 100);
    CHECK(typeText("integer", "42"));
    CHECK(flickable->setProperty("contentY", 270));
    QCoreApplication::processEvents();
    CHECK(clickControl("automation-color-button-rgba"));
    CHECK(evaluate("configForm.colorDialog.visible").toBool());
    CHECK(evaluate("configForm.colorDialog.withAlpha").toBool());
    auto *formObject = dialog->property("configForm").value<QObject *>();
    CHECK(formObject);
    auto *colorPicker = formObject->property("colorDialog").value<QObject *>();
    CHECK(colorPicker);
    const auto initialPickerColor = colorPicker->property("selectedColor").value<QColor>();
    CHECK(initialPickerColor.red() == 0x11 && initialPickerColor.green() == 0x22 && initialPickerColor.blue() == 0x33 && initialPickerColor.alpha() == 0x7f);
    CHECK(colorPicker->setProperty("selectedColor", QColor(0x12, 0x34, 0x56, 0x55)));
    CHECK(QMetaObject::invokeMethod(colorPicker, "accept"));
    CHECK(evaluate("configForm.readback().settings.rgba").toString() == "#123456AA");
    CHECK(clickControl("automation-color-button-rgba"));
    CHECK(colorPicker->setProperty("selectedColor", QColor(Qt::red)));
    CHECK(QMetaObject::invokeMethod(colorPicker, "reject"));
    CHECK(evaluate("configForm.readback().settings.rgba").toString() == "#123456AA");
    CHECK(clickControl("automation-color-button-rgb"));
    CHECK(!evaluate("configForm.colorDialog.withAlpha").toBool());
    CHECK(colorPicker->setProperty("selectedColor", QColor(0x11, 0x22, 0x33, 0x55)));
    CHECK(QMetaObject::invokeMethod(colorPicker, "accept"));
    CHECK(evaluate("configForm.readback().settings.rgb").toString() == "#112233");
    CHECK(flickable->setProperty("contentY", 0));
    QTest::qWait(250); // Allow the Qt Quick color popup's exit transition to finish.
    window->requestActivate();
    QCoreApplication::processEvents();
    for (const auto &[input, expected] : colorFixtures) {
        CHECK(typeText("rgb", QByteArray(input)));
        CHECK(typeText("rgba", QByteArray(input)));
        const auto colors = evaluate("configForm.readback()").toMap();
        CHECK(colors.value("success").toBool());
        CHECK(colors.value("settings").toMap().value("rgb") == QString::fromLatin1(expected).left(7));
        CHECK(colors.value("settings").toMap().value("rgba") == QString::fromLatin1(expected));
    }
    CHECK(typeText("rgba", "rgba(1,2,3,999)"));
    CHECK(!evaluate("configForm.readback().success").toBool());
    CHECK(typeText("rgb", "#112233"));
    CHECK(typeText("rgba", "#11223380"));
    auto *checkbox = findField("enabled");
    auto *dropdown = findField("choice");
    CHECK(checkbox && dropdown);
    checkbox->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Space);
    dropdown->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Down);
    const auto readback = evaluate("configForm.readback()").toMap();
    CHECK(readback.value("success").toBool());
    const auto settingsFromUI = readback.value("settings").toMap();
    CHECK(settingsFromUI.value("text") == "Edited" && settingsFromUI.value("multiline") == "first\nsecond");
    CHECK(settingsFromUI.value("enabled").toBool() && settingsFromUI.value("choice") == "Beta");
    CHECK(settingsFromUI.value("integer").toDouble() == 42 && settingsFromUI.value("number").toDouble() == 1.25);
    CHECK(settingsFromUI.value("rgb") == "#112233" && settingsFromUI.value("rgba") == "#11223380");
    CHECK(evaluate("storeCurrentSettings(); configForm.configure([{class:'color',name:'rgb',value:'rgb(16,32,48)',y:0}, {class:'coloralpha',name:'rgba',value:'1056816',y:1}, {class:'coloralpha',name:'invalid',value:'red',y:2}], {}); configForm.readback().success").toBool());
    CHECK(evaluate("configForm.readback().settings.rgb").toString() == "#102030");
    CHECK(evaluate("configForm.readback().settings.rgba").toString() == "#30201000");
    CHECK(evaluate("configForm.readback().settings.invalid").toString() == "#00000000");
    CHECK(evaluate("showCurrentConfig(); configForm.readback().settings.rgba").toString() == "#11223380");
    CHECK(evaluate("storeCurrentSettings(); configForm.configure([{class:'floatedit',name:'bounded',value:99.9,step:0.25,x:0,y:0}, {class:'floatedit',name:'plain',value:-1,x:0,y:1}, {class:'intedit',name:'wide',value:-1,min:10,max:5,x:0,y:2}], {}); configForm.readback().success").toBool());
    CHECK(evaluate("configForm.numericLimits(configForm.controls[0]).max").toDouble() == 100);
    CHECK(evaluate("configForm.numericLimits(configForm.controls[2]).min").toDouble() == -2147483648.0);
    CHECK(!findVisual(window->contentItem(), "automation-step-up-plain"));
    CHECK(clickControl("automation-step-up-bounded"));
    CHECK(evaluate("configForm.readback().settings.bounded").toDouble() == 100);
    CHECK(clickControl("automation-step-down-bounded"));
    CHECK(evaluate("configForm.readback().settings.bounded").toDouble() == 99.75);
    CHECK(evaluate("showCurrentConfig(); configForm.readback().settings.integer").toDouble() == 42);
    CHECK(flickable->setProperty("contentY", 270));
    QCoreApplication::processEvents();
    CHECK(clickControl("automation-color-button-rgb"));
    CHECK(evaluate("configForm.colorDialog.visible").toBool());
    CHECK(evaluate("configForm.configure([], {}); !configForm.colorDialog.visible && configForm.colorDialog.activeField === null").toBool());
    CHECK(QMetaObject::invokeMethod(colorPicker, "accepted")); // A stale completion cannot target the replaced controls.
    CHECK(evaluate("showCurrentConfig(); configForm.readback().settings.rgb").toString() == "#112233");
    CHECK(flickable->setProperty("contentY", 0));
    QTest::qWait(250);
    window->requestActivate();
    QCoreApplication::processEvents();
    const auto screenshotPath = qEnvironmentVariable("AEGISUB_EXPORT_TEST_SCREENSHOT");
    if (!screenshotPath.isEmpty()) {
        CHECK(window->grabWindow().save(screenshotPath));
        CHECK(flickable->setProperty("contentY", 230));
        QTest::qWait(50);
        CHECK(window->grabWindow().save(screenshotPath + ".controls.png"));
        CHECK(flickable->setProperty("contentY", 0));
    }
    CHECK(evaluate("setAll(true); storeCurrentSettings(); filtersModel[selectedFilterIndex].settings.text").toString() == "Edited");
    CHECK(typeText("integer", "invalid"));
    CHECK(!evaluate("configForm.readback().success").toBool());
    CHECK(typeText("integer", "42"));
    const auto beforeGUI = editor.getAllLines();
    const auto guiModified = editor.isModified();
    const auto guiUndo = editor.undoDescription();
    engine.rootContext()->setContextProperty("testOutputPath", temporary.filePath("gui-export.ass"));
    CHECK(evaluate("doExport(); outputDialog.visible").toBool());
    auto *outputDialog = dialog->property("outputDialog").value<QObject *>();
    CHECK(outputDialog);
    CHECK(outputDialog->setProperty("currentFolder", QUrl::fromLocalFile(temporary.path())));
    CHECK(QMetaObject::invokeMethod(outputDialog, "reject"));
    CHECK(dialog->property("visible").toBool() && !QFile::exists(temporary.filePath("gui-export.ass")));
    CHECK(evaluate("doExport(); outputDialog.visible").toBool());
    CHECK(evaluate("pendingPipeline.length").toInt() == 3);
    CHECK(outputDialog->setProperty("selectedFile", QUrl::fromLocalFile(temporary.filePath("gui-export.ass"))));
    // The native/platform helper emits accepted after supplying selectedFile.
    // Exercise that actual handler; accept() on the fallback helper reads its
    // private filename editor instead and would replace a programmatic URL.
    CHECK(QMetaObject::invokeMethod(outputDialog, "accepted"));
    QCoreApplication::processEvents();
    CHECK(!dialog->property("visible").toBool());
    CHECK(reopened.loadFromFile(temporary.filePath("gui-export.ass")));
    if (reopened.get(0).value("text") != "Edited Beta") fprintf(stderr, "Unexpected export text: %s\n", qPrintable(reopened.get(0).value("text").toString()));
    CHECK(reopened.get(0).value("text") == "Edited Beta");
    CHECK(editor.getAllLines() == beforeGUI && editor.isModified() == guiModified && editor.undoDescription() == guiUndo);
    CHECK(QMetaObject::invokeMethod(dialog.get(), "open"));
    CHECK(evaluate("doExport(); outputDialog.visible").toBool());
    CHECK(outputDialog->setProperty("selectedFile", QUrl::fromLocalFile(temporary.filePath("missing/ui-failed.ass"))));
    CHECK(QMetaObject::invokeMethod(outputDialog, "accepted"));
    CHECK(dialog->property("visible").toBool() && !dialog->property("errorMessage").toString().isEmpty());
    CHECK(!QFile::exists(temporary.filePath("missing/ui-failed.ass")));
    CHECK(editor.getAllLines() == beforeGUI && editor.isModified() == guiModified);
    CHECK(QMetaObject::invokeMethod(outputDialog, "close"));
    CHECK(QMetaObject::invokeMethod(dialog.get(), "close"));
    puts("PASS actual DialogExport/SubtitleProject QML, mouse numeric step buttons, keyboard arrows and bounds/defaults, color popup open/accept/cancel and ASS-alpha conversion, stale picker disposal, typed settings, real file write/reopen, unchanged editor");
    {
        SubtitleModel uiClipRows;
        uiClipRows.setAllLines({QVariantMap{{"start", "0:00:03.00"}, {"end", "0:00:04.00"}},
                                QVariantMap{{"start", "0:00:01.00"}, {"end", "0:00:09.00"}},
                                QVariantMap{{"start", "0:00:02.00"}, {"end", "0:00:06.00"}}});
        QQmlEngine::setObjectOwnership(&uiClipRows, QQmlEngine::CppOwnership);
        QQmlComponent audioMockComponent(&engine);
        audioMockComponent.setData(R"qml(import QtQml
QtObject {
    property bool hasAudio: false
    property int savedStart: -1
    property int savedEnd: -1
    function saveAudioClip(url, start, end) {
        savedStart = start; savedEnd = end
        return {success: true, message: "Saved test clip"}
    }
})qml", QUrl());
        std::unique_ptr<QObject> audioMock(audioMockComponent.create());
        CHECK(audioMock);
        engine.rootContext()->setContextProperty("nativeSubtitleModel", &uiClipRows);
        engine.rootContext()->setContextProperty("audioController", audioMock.get());
        QQmlComponent mainComponent(&engine, QUrl::fromLocalFile(qmlRoot + "/Main.qml"));
        std::unique_ptr<QObject> mainWindow(mainComponent.create());
        if (!mainWindow) for (const auto &error : mainComponent.errors())
            fprintf(stderr, "%s\n", qPrintable(error.toString()));
        CHECK(mainWindow);
        CHECK(mainWindow->setProperty("viewMode", "subs"));
        auto *mainProject = mainWindow->property("project").value<QObject *>();
        CHECK(mainProject);
        auto *inlinePicker = mainWindow->property("dlgColorPicker").value<QObject *>();
        CHECK(inlinePicker && inlinePicker->setProperty("targetProp", "primary"));
        CHECK(QMetaObject::invokeMethod(inlinePicker, "colorAccepted", Q_ARG(QColor, QColor(17,34,51,85)),
            Q_ARG(QString, QStringLiteral("&H332211&")), Q_ARG(QString, QStringLiteral("&HAA332211"))));
        CHECK(uiClipRows.get(0).value("text") == QStringLiteral("{\\c&H332211&\\1a&HAA&}"));
        CHECK(mainProject->setProperty("selectedIndices", QVariantList{0, 2}));
        auto *clipPicker = mainWindow->findChild<QObject *>("audio-clip-save-dialog");
        CHECK(clipPicker);
        CHECK(QMetaObject::invokeMethod(mainWindow.get(), "createAudioClip"));
        CHECK(!clipPicker->property("visible").toBool());
        CHECK(audioMock->setProperty("hasAudio", true));
        CHECK(QMetaObject::invokeMethod(mainWindow.get(), "createAudioClip"));
        CHECK(clipPicker->property("visible").toBool());
        auto pending = mainWindow->property("pendingAudioClipRange").toMap();
        CHECK(pending.value("startMs").toInt() == 2000 && pending.value("endMs").toInt() == 6000);
        CHECK(clipPicker->setProperty("selectedFile", QUrl::fromLocalFile(temporary.filePath("selected.wav"))));
        CHECK(QMetaObject::invokeMethod(clipPicker, "accepted"));
        CHECK(audioMock->property("savedStart").toInt() == 2000
              && audioMock->property("savedEnd").toInt() == 6000);
        CHECK(mainWindow->property("pendingAudioClipRange").isNull());
        CHECK(QMetaObject::invokeMethod(clipPicker, "close"));
        CHECK(QMetaObject::invokeMethod(mainWindow.get(), "createAudioClip"));
        CHECK(QMetaObject::invokeMethod(clipPicker, "rejected"));
        CHECK(mainWindow->property("pendingAudioClipRange").isNull());
        CHECK(QMetaObject::invokeMethod(clipPicker, "close"));
        puts("PASS actual Main Create Audio Clip selection, no-audio guard, save picker acceptance and cancellation");
        mainWindow.reset();
        engine.rootContext()->setContextProperty("audioController", QVariant());
        engine.rootContext()->setContextProperty("nativeSubtitleModel", QVariant());
    }
    puts("PASS native export chain ordering/config, actual UTF-16/GBK/ASS/SRT write/reopen, attachment/extradata fidelity, failure preserves destination, unchanged editor state/undo, stale selection rejection");
    puts("PASS real Clean Tags and Karaoke Templater generation; registration validation; config descriptors/settings; readonly config; retained userdata expiry; atomic failed filter; callback recovery; reload/remove/failed-load cleanup; stable priority ordering");
    return 0;
}
