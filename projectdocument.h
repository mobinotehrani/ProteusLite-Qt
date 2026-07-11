#pragma once

#include <QSize>
#include <QString>

struct ProjectMetadata
{
    QString name;
    QSize canvasSize{3000, 2000};
    QString filePath;
    int componentCount{0};
    int wireCount{0};
    bool openedFromFile{false};
};

class ProjectDocument final
{
public:
    static ProjectMetadata createNew(const QString &name, const QSize &canvasSize);
    static bool loadMetadata(const QString &filePath, ProjectMetadata &metadata, QString &errorMessage);

private:
    static bool loadJson(const QString &filePath, ProjectMetadata &metadata, QString &errorMessage);
    static bool loadXml(const QString &filePath, ProjectMetadata &metadata, QString &errorMessage);
    static bool isValidCanvas(const QSize &canvasSize);
};
