#pragma once

#include <QQuickItem>

// Lightweight visual stand-ins so the production Main.qml can be loaded in
// the export/UI test without pulling the media renderer into that test binary.
class TestVideoSurface : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QObject *controller MEMBER controller)
    Q_PROPERTY(QObject *model MEMBER model)
public:
    using QQuickItem::QQuickItem;
    QObject *controller = nullptr;
    QObject *model = nullptr;
};

class TestSpectrogramView : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QObject *audioController MEMBER audioController)
public:
    using QQuickItem::QQuickItem;
    QObject *audioController = nullptr;
};
