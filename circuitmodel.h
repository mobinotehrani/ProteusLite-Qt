#pragma once

#include <QHash>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

enum class PinDirection
{
    Passive,
    Input,
    Output,
    Power
};

struct PinModel
{
    QString name;
    QPointF localPosition;
    PinDirection direction{PinDirection::Passive};
    qreal sensitivityRadius{11.0};
};

struct NodeModel
{
    QString id;
    QStringList endpoints;
    QStringList wireIds;
};

struct WireModel
{
    QString id;
    QString startEndpoint;
    QString endEndpoint;
    QVector<QPointF> points;
};

struct JunctionModel
{
    QString id;
    QPointF position;
};

class CircuitGraph final
{
  public:
    QString addComponent(const QString &type, const QVector<PinModel> &pins);
    void removeComponent(const QString &componentId);
    QStringList updateComponentPins(const QString &componentId, const QVector<PinModel> &pins);

    QString addJunction(const QPointF &position);
    void updateJunction(const QString &junctionId, const QPointF &position);
    void removeJunction(const QString &junctionId);

    QString addWire(const QString &startEndpoint, const QString &endEndpoint, const QVector<QPointF> &points);
    void updateWirePoints(const QString &wireId, const QVector<QPointF> &points);
    void removeWire(const QString &wireId);

    QStringList wiresForEndpoint(const QString &endpoint) const;
    QStringList wiresForComponent(const QString &componentId) const;
    QString nodeForEndpoint(const QString &endpoint) const;

    const QHash<QString, WireModel> &wires() const;
    const QHash<QString, JunctionModel> &junctions() const;
    const QHash<QString, NodeModel> &nodes() const;

    void clear();

    static QString pinEndpoint(const QString &componentId, int pinIndex);
    static bool parsePinEndpoint(const QString &endpoint, QString &componentId, int &pinIndex);

  private:
    struct ComponentRecord
    {
        QString type;
        QVector<PinModel> pins;
    };

    void rebuildNodes();

    QHash<QString, ComponentRecord> m_components;
    QHash<QString, WireModel> m_wires;
    QHash<QString, JunctionModel> m_junctions;
    QHash<QString, NodeModel> m_nodes;
    QHash<QString, QString> m_endpointToNode;

    int m_nextComponentId{1};
    int m_nextWireId{1};
    int m_nextJunctionId{1};
};
