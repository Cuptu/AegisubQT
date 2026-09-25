// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// SubtitleModel: High-performance QAbstractListModel backing subtitle table and grid views.
// Native C++ data storage, Copy-on-Write undo stacks, ASS file parser/serializer,
// and hardware-accelerated line/timing manipulation engines.

#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <QList>
#include <vector>
#include <memory>

/// Represents a single subtitle dialogue row in native memory.
struct SubtitleLine {
    int lineNumber = 1;
    int layer = 0;
    int startMs = 0;
    int endMs = 5000;
    QString startStr = QStringLiteral("0:00:00.00");
    QString endStr = QStringLiteral("0:00:05.00");
    QString cps = QStringLiteral("0");
    QString style = QStringLiteral("Default");
    QString actor;
    QString effect;
    int marginLeft = 0;
    int marginRight = 0;
    int marginVert = 0;
    QString text;
    bool isComment = false;

    void setStartMs(int ms);
    void setEndMs(int ms);
    void setStartStr(const QString &str);
    void setEndStr(const QString &str);
    void updateCps();

    QVariantMap toMap() const;
    static SubtitleLine fromMap(const QVariantMap &map, int fallbackLineNumber = 1);
    static QString calculateCps(const QString &text, int startMs, int endMs);
};

/// Undo snapshot capturing immutable line state and selection context.
struct UndoSnapshot {
    QString description;
    std::shared_ptr<const std::vector<SubtitleLine>> lines;
    int selectedIndex = 0;
    QList<int> selectedIndices;
    QVariantMap scriptInfo;
    QVariantList styles;
};

/// High-performance C++ model exposed to QML ListView and TableView.
class SubtitleModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStateChanged)
    Q_PROPERTY(QString undoDescription READ undoDescription NOTIFY undoStateChanged)
    Q_PROPERTY(QString redoDescription READ redoDescription NOTIFY undoStateChanged)
    Q_PROPERTY(QVariantMap scriptInfo READ scriptInfo WRITE setScriptInfo NOTIFY scriptInfoChanged)
    Q_PROPERTY(QVariantList styles READ styles WRITE setStyles NOTIFY stylesChanged)
    Q_PROPERTY(QStringList styleNames READ styleNames NOTIFY stylesChanged)
    Q_PROPERTY(QString fileName READ fileName WRITE setFileName NOTIFY fileNameChanged)
    // Reflects Aegisub's native SubsController::IsModified() state tracking.
    Q_PROPERTY(bool modified READ isModified NOTIFY modifiedChanged)

public:
    enum SubtitleRoles {
        LineNumberRole = Qt::UserRole + 1,
        LayerRole,
        StartRole,
        EndRole,
        CpsRole,
        StyleRole,
        ActorRole,
        EffectRole,
        MarginLeftRole,
        MarginRightRole,
        MarginVertRole,
        TextRole,
        IsCommentRole
    };
    Q_ENUM(SubtitleRoles)

    explicit SubtitleModel(QObject *parent = nullptr);
    ~SubtitleModel() override = default;

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // QML ListModel API compatibility methods
    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE bool setProperty(int index, const QString &propertyName, const QVariant &value);
    Q_INVOKABLE void append(const QVariantMap &item);
    Q_INVOKABLE void insert(int index, const QVariantMap &item);
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void renumberLines();

    // High-performance Copy-on-Write Undo / Redo engine
    Q_INVOKABLE void pushUndo(const QString &description, int selectedIndex = 0, const QVariantList &selectedIndices = QVariantList());
    void pushUndo(const QString &description, int selectedIndex, const QList<int> &selectedIndices);
    Q_INVOKABLE QVariantMap undo();
    Q_INVOKABLE QVariantMap redo();
    Q_INVOKABLE void clearUndo();

    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }
    QString undoDescription() const;
    QString redoDescription() const;

    // Document state & section metadata accessors
    QVariantMap scriptInfo() const { return m_scriptInfo; }
    void setScriptInfo(const QVariantMap &info);
    Q_INVOKABLE QVariant getScriptInfo(const QString &key, const QVariant &defaultValue = QVariant()) const;
    Q_INVOKABLE void setScriptInfoKey(const QString &key, const QVariant &value);

    QVariantList styles() const { return m_styles; }
    void setStyles(const QVariantList &stylesList);
    QStringList styleNames() const;

    Q_INVOKABLE QVariantMap getStyle(int index) const;
    Q_INVOKABLE QVariantMap getStyleByName(const QString &name) const;
    Q_INVOKABLE void setStyle(int index, const QVariantMap &style);
    Q_INVOKABLE void setStyleByName(const QString &name, const QVariantMap &style);
    Q_INVOKABLE void addStyle(const QVariantMap &style);
    Q_INVOKABLE void removeStyle(int index);
    Q_INVOKABLE void moveStyle(int from, int to);
    Q_INVOKABLE void copyStyle(int index);
    Q_INVOKABLE void sortStyles();

    QString fileName() const { return m_fileName; }
    void setFileName(const QString &name);

    /// Matches Aegisub's native SubsController::IsModified() semantics:
    /// commitId increments on each mutation, while savedCommitId synchronizes on successful disk save.
    bool isModified() const { return m_commitId != m_savedCommitId; }

    /// Fast-path document close query: returns 0 if clean (proceed), 1 if dirty (requires confirmation).
    /// Called before onClosing, newDocument, or loadFromFile to pause playback and prompt user.
    Q_INVOKABLE int tryToClose();

    /// Explicitly marks the document dirty for modifications that bypass model mutations (e.g. attachment edits).
    Q_INVOKABLE void markModified();

    // Native ASS file parser & serializer
    Q_INVOKABLE bool loadFromFile(const QString &filePath);
    Q_INVOKABLE bool saveToFile(const QString &filePath = QString());
    Q_INVOKABLE void newDocument();

    // Direct C++ model buffer access for automation and native processing engines
    const std::vector<SubtitleLine>& rawLines() const { return m_lines; }
    void setRawLines(std::vector<SubtitleLine> lines);

    // Bulk serialization interface for automation scripts & clipboard
    Q_INVOKABLE QVariantList getAllLines() const;
    Q_INVOKABLE void setAllLines(const QVariantList &lines);
    Q_INVOKABLE QVariantList getAllSubtitleLines() const { return getAllLines(); }
    Q_INVOKABLE void setAllSubtitleLines(const QVariantList &lines) { setAllLines(lines); }

    // Fast C++ query methods
    Q_INVOKABLE int getLineStartMs(int index) const;
    Q_INVOKABLE int getLineEndMs(int index) const;
    Q_INVOKABLE QList<int> getSnapPoints(int excludeIndex = -1) const;

    // High-performance native timing operations
    Q_INVOKABLE void shiftTimes(int amountMs, bool shiftStart, bool shiftEnd, int affectMode, const QVariantList &selectedIndices);
    Q_INVOKABLE void makeTimesContinuous(int curIndex, bool changeStart);
    Q_INVOKABLE void snapStartTime(int curIndex, int videoMs);
    Q_INVOKABLE void snapEndTime(int curIndex, int videoMs);
    Q_INVOKABLE void recombineSelectedLines(const QVariantList &selectedIndices);
    Q_INVOKABLE void shiftToCurrentFrame(int videoMs, const QVariantList &selectedIndices);
    Q_INVOKABLE int processTiming(int leadIn, int leadOut, int gapThresh, double bias, bool selectedOnly, const QVariantList &selectedIndices, const QStringList &allowedStyles);

    // High-performance native line operations
    Q_INVOKABLE int insertLine(int baseIndex, bool before, int startMs, int endMs, const QVariantMap &defaults = QVariantMap());
    Q_INVOKABLE QList<int> duplicateSelectedLines(const QVariantList &selectedIndices);
    Q_INVOKABLE int deleteSelectedLines(const QVariantList &selectedIndices);
    Q_INVOKABLE void swapSelectedLines(const QVariantList &selectedIndices);
    Q_INVOKABLE void joinSelectedLines(const QVariantList &selectedIndices, int mode);
    Q_INVOKABLE void splitLineAtFrame(int curIndex, int shift, int videoMs, double videoFps);
    Q_INVOKABLE void splitLineAtCursor(int curIndex, int pos, int mode, int videoMs);
    Q_INVOKABLE void sortLines(const QString &field, const QVariantList &selectedIndices, bool ascending);
    Q_INVOKABLE void sortByColumn(int col, bool ascending);
    Q_INVOKABLE int applyKanjiCopy(const QString &srcStyle, const QString &dstStyle);

    // High-performance native search and filter operations
    Q_INVOKABLE QList<int> selectLines(int action, int fieldIdx, int mode, bool invert, bool matchCase, bool comments, bool dialogues, const QString &query, const QVariantList &currentSelectedIndices);
    Q_INVOKABLE int findAndReplace(const QString &query, const QString &replaceWith, const QVariantMap &options, bool replaceAll, int currentIndex);
    Q_INVOKABLE int findNext(const QString &query, const QVariantMap &options, int startIndex);

    static QVariantMap defaultScriptInfo();
    static QVariantList defaultStyles();

signals:
    void countChanged();
    void undoStateChanged();
    void contentModified();
    void scriptInfoChanged();
    void stylesChanged();
    void fileNameChanged();
    void modifiedChanged();

private:
    SubtitleLine parseDialogueLine(const QString &rawLine, bool isComment, int lineNumber) const;
    QString formatDialogueLine(const SubtitleLine &line) const;
    /// Synchronizes savedCommitId with current commitId upon successful file save.
    void markSaved();
    /// Resets modification tracking counters when creating a new document or loading from disk.
    void resetModificationTracking();

    struct AssRawSection {
        QString header;
        QStringList lines;
    };

    std::vector<SubtitleLine> m_lines;
    std::vector<UndoSnapshot> m_undoStack;
    std::vector<UndoSnapshot> m_redoStack;
    QVariantMap m_scriptInfo;
    QVariantList m_styles;
    QList<AssRawSection> m_rawSections;
    QString m_fileName = QStringLiteral("Untitled");
    quint64 m_commitId = 0;
    quint64 m_savedCommitId = 0;

    static constexpr size_t kMaxUndoLevels = 128;
};
