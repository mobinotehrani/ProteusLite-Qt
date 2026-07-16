#pragma once

#include <QList>
#include <QString>

struct ComponentDefinition
{
    QString id;
    QString displayName;
    QString category;
    QString description;
    QString defaultValue;
};

class ComponentCatalog final
{
public:
    static const QList<ComponentDefinition> &all();
    static QList<ComponentDefinition> filtered(const QString &query);
    static const ComponentDefinition *find(const QString &id);
};
