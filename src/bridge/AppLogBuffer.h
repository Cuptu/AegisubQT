#pragma once
#include <QObject>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QStringListModel>

// Thread-safe in-memory log ring buffer powering the in-app Log Window
// (upstream Help > Log Window). Qt log messages are mirrored here in addition
// to stderr so users can inspect runtime diagnostics without a console.
class AppLogBuffer : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList lines READ lines NOTIFY linesChanged)
public:
    explicit AppLogBuffer(QObject *parent = nullptr) : QObject(parent) {}

    void append(QtMsgType type, const QString &msg) {
        static constexpr int kMaxLines = 1000;
        const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
        const QString prefix = (type == QtWarningMsg) ? QStringLiteral("W")
                               : (type == QtCriticalMsg) ? QStringLiteral("C")
                               : (type == QtFatalMsg) ? QStringLiteral("F")
                               : QStringLiteral("I");
        const QString line = QStringLiteral("[%1][%2] %3").arg(stamp, prefix, msg.left(4096));
        {
            QMutexLocker lock(&m_mutex);
            m_lines.append(line);
            while (m_lines.size() > kMaxLines) m_lines.removeFirst();
            scheduleRefresh();
        }
    }

    QStringList lines() const {
        QMutexLocker lock(&m_mutex);
        return m_lines;
    }

    Q_INVOKABLE void clear() {
        {
            QMutexLocker lock(&m_mutex);
            m_lines.clear();
            scheduleRefresh();
        }
    }

    QAbstractListModel *model() { return &m_model; }

signals:
    void linesChanged();

private:
    // Called with m_mutex held. A stalled GUI retains one refresh event.
    void scheduleRefresh() {
        if (m_refreshPending) return;
        m_refreshPending = true;
        QMetaObject::invokeMethod(this, [this]() {
            QStringList snapshot;
            {
                QMutexLocker lock(&m_mutex);
                snapshot = m_lines;
                m_refreshPending = false;
            }
            m_model.setStringList(snapshot);
            Q_EMIT linesChanged();
        }, Qt::QueuedConnection);
    }
    bool m_refreshPending = false;
    mutable QMutex m_mutex;
    QStringList m_lines;
    QStringListModel m_model;
};
