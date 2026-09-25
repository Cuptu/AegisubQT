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

#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>
#include <memory>
#include <vector>

class SubtitleModel;

namespace Automation {

class LuaScript;

class AutomationManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList scripts READ scripts NOTIFY scriptsChanged)
    Q_PROPERTY(QVariantList macros READ macros NOTIFY macrosChanged)

public:
    explicit AutomationManager(QObject *parent = nullptr);
    ~AutomationManager();

    static AutomationManager *instance();

    QVariantList scripts() const;
    QVariantList macros() const;

    Q_INVOKABLE void scanAutoloadFolder();
    Q_INVOKABLE bool addScript(const QString &filepath, bool isGlobal = true);
    Q_INVOKABLE void removeScript(int index);
    Q_INVOKABLE void reloadScript(int index);
    Q_INVOKABLE void reloadAll();

    Q_INVOKABLE bool runMacro(int macroId, QObject *subtitleProject = nullptr, int activeIndex = 0, const QVariantList &selectedIndices = {});

    QStringList includePaths() const { return m_includePaths; }
    void addIncludePath(const QString &path);

    SubtitleModel *subtitleModel() const { return m_subtitleModel; }
    void setSubtitleModel(SubtitleModel *model) { m_subtitleModel = model; }

signals:
    void scriptsChanged();
    void macrosChanged();
    void macroExecuted(const QString &name, bool success, const QString &message);
    void statusMessage(const QString &msg);

private:
    void updateMacroList();

    struct ScriptEntry {
        std::unique_ptr<LuaScript> script;
        bool isGlobal = true;
    };

    std::vector<ScriptEntry> m_entries;
    QStringList m_includePaths;
    QVariantList m_cachedScripts;
    QVariantList m_cachedMacros;
    SubtitleModel *m_subtitleModel = nullptr;
};

} // namespace Automation
