#pragma once
#include <QObject>
#include <libaegisub/vfr.h>

class TestEditorTimeline : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasTimecodes READ hasTimecodes NOTIFY timecodesChanged)
public:
    bool hasTimecodes() const { return loaded; }
    Q_INVOKABLE int frameAtTimeMs(int ms, int type) const {
        return fps.FrameAtTime(ms, type == 1 ? agi::vfr::START : agi::vfr::END);
    }
    Q_INVOKABLE int timeAtFrameMs(int frame, int type) const {
        return fps.TimeAtFrame(frame, type == 1 ? agi::vfr::START : agi::vfr::END);
    }
    Q_INVOKABLE void parseAndSetActiveSubtitle(const QString &start, const QString &end, const QString &text) {
        activeStart = start;
        activeEnd = end;
        activeText = text;
    }
    QString activeStart, activeEnd, activeText;
    void close() { loaded = false; emit timecodesChanged(); }
signals:
    void timecodesChanged();
private:
    bool loaded = true;
    agi::vfr::Framerate fps{0, 40, 100, 120, 200, 300};
};
