#include "recentprojectsmanager.h"

#include <QFileInfo>
#include <QSettings>

namespace
{
QSettings createSettings()
{
    return QSettings(QStringLiteral("HomeworkMOT"), QStringLiteral("ProteusProFinal"));
}
}

QStringList RecentProjectsManager::projects() const
{
    QSettings settings = createSettings();
    const QStringList stored = settings.value(QStringLiteral("recent")).toStringList();
    QStringList existing;

    for (const QString &path : stored)
    {
        const QString normalized = normalizedPath(path);
        if (!normalized.isEmpty() && QFileInfo::exists(normalized) && !existing.contains(normalized))
            existing.append(normalized);
    }

    while (existing.size() > MaximumRecentProjects)
        existing.removeLast();

    if (existing != stored)
        save(existing);
    return existing;
}

void RecentProjectsManager::add(const QString &filePath) const
{
    const QString normalized = normalizedPath(filePath);
    if (normalized.isEmpty())
        return;

    QStringList recent = projects();
    recent.removeAll(normalized);
    recent.prepend(normalized);
    while (recent.size() > MaximumRecentProjects)
        recent.removeLast();
    save(recent);
}

void RecentProjectsManager::clear() const
{
    save({});
}

QString RecentProjectsManager::normalizedPath(const QString &filePath)
{
    if (filePath.trimmed().isEmpty())
        return {};
    return QFileInfo(filePath).absoluteFilePath();
}

void RecentProjectsManager::save(const QStringList &projects)
{
    QSettings settings = createSettings();
    settings.setValue(QStringLiteral("recent"), projects);
}
