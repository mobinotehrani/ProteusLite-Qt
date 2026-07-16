#include "projectdocument.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QXmlStreamReader>

ProjectMetadata ProjectDocument::createNew(const QString &name, const QSize &canvasSize)
{
    ProjectMetadata metadata;
    metadata.name = name.trimmed().isEmpty() ? QStringLiteral("Untitled Project") : name.trimmed();
    metadata.canvasSize = isValidCanvas(canvasSize) ? canvasSize : QSize(3000, 2000);
    return metadata;
}

bool ProjectDocument::loadMetadata(const QString &filePath,
                                   ProjectMetadata &metadata,
                                   QString &errorMessage)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile())
    {
        errorMessage = QStringLiteral("The selected project file does not exist.");
        return false;
    }

    const QString suffix = fileInfo.suffix().toLower();
    bool loaded = false;

    if (suffix == QStringLiteral("json"))
        loaded = loadJson(fileInfo.absoluteFilePath(), metadata, errorMessage);
    else if (suffix == QStringLiteral("xml"))
        loaded = loadXml(fileInfo.absoluteFilePath(), metadata, errorMessage);
    else
    {
        errorMessage = QStringLiteral("Only JSON and XML project files are supported.");
        return false;
    }

    if (!loaded)
        return false;

    metadata.filePath = fileInfo.absoluteFilePath();
    metadata.openedFromFile = true;
    if (metadata.name.trimmed().isEmpty())
        metadata.name = fileInfo.completeBaseName();
    return true;
}

bool ProjectDocument::loadJson(const QString &filePath,
                               ProjectMetadata &metadata,
                               QString &errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        errorMessage = QStringLiteral("The project file could not be opened for reading.");
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        errorMessage = QStringLiteral("Invalid JSON project: %1").arg(parseError.errorString());
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonObject canvas = root.value(QStringLiteral("canvas")).toObject();
    const QSize canvasSize(canvas.value(QStringLiteral("w")).toInt(),
                           canvas.value(QStringLiteral("h")).toInt());

    if (!isValidCanvas(canvasSize))
    {
        errorMessage = QStringLiteral("The project does not contain a valid canvas size.");
        return false;
    }

    metadata.name = root.value(QStringLiteral("name")).toString();
    if (metadata.name.isEmpty())
        metadata.name = root.value(QStringLiteral("projectName")).toString();
    metadata.canvasSize = canvasSize;
    metadata.componentCount = root.value(QStringLiteral("components")).toArray().size();
    metadata.wireCount = root.value(QStringLiteral("wires")).toArray().size();
    return true;
}

bool ProjectDocument::loadXml(const QString &filePath,
                              ProjectMetadata &metadata,
                              QString &errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        errorMessage = QStringLiteral("The project file could not be opened for reading.");
        return false;
    }

    QXmlStreamReader xml(&file);
    QSize canvasSize;
    int componentCount = 0;
    int wireCount = 0;

    while (!xml.atEnd())
    {
        xml.readNext();
        if (!xml.isStartElement())
            continue;

        if (xml.name() == QStringLiteral("project"))
        {
            metadata.name = xml.attributes().value(QStringLiteral("name")).toString();
        }
        else if (xml.name() == QStringLiteral("canvas"))
        {
            canvasSize.setWidth(xml.attributes().value(QStringLiteral("w")).toInt());
            canvasSize.setHeight(xml.attributes().value(QStringLiteral("h")).toInt());
        }
        else if (xml.name() == QStringLiteral("component"))
        {
            ++componentCount;
        }
        else if (xml.name() == QStringLiteral("wire"))
        {
            ++wireCount;
        }
    }

    if (xml.hasError())
    {
        errorMessage = QStringLiteral("Invalid XML project: %1").arg(xml.errorString());
        return false;
    }

    if (!isValidCanvas(canvasSize))
    {
        errorMessage = QStringLiteral("The project does not contain a valid canvas size.");
        return false;
    }

    metadata.canvasSize = canvasSize;
    metadata.componentCount = componentCount;
    metadata.wireCount = wireCount;
    return true;
}

bool ProjectDocument::isValidCanvas(const QSize &canvasSize)
{
    return canvasSize.width() >= 100 && canvasSize.height() >= 100 &&
           canvasSize.width() <= 20000 && canvasSize.height() <= 20000;
}
