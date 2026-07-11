#pragma once

#include <QStringList>

class RecentProjectsManager final
{
public:
    QStringList projects() const;
    void add(const QString &filePath) const;
    void clear() const;

private:
    static constexpr int MaximumRecentProjects = 5;
    static QString normalizedPath(const QString &filePath);
    static void save(const QStringList &projects);
};
