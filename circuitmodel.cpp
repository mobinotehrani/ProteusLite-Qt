#include "circuitmodel.h"

#include <QRegularExpression>

#include <algorithm>

namespace
{
QString findRoot(QHash<QString, QString> &parents, const QString &value)
{
    const QString parent = parents.value(value, value);
    if (parent == value)
        return value;

    const QString root = findRoot(parents, parent);
    parents[value] = root;
    return root;
}

void unite(QHash<QString, QString> &parents, const QString &first, const QString &second)
{
    const QString firstRoot = findRoot(parents, first);
    const QString secondRoot = findRoot(parents, second);
    if (firstRoot != secondRoot)
        parents[secondRoot] = firstRoot;
}
}

QString CircuitGraph::addComponent(const QString &type, const QVector<PinModel> &pins)
{
    const QString id = QStringLiteral("C%1").arg(m_nextComponentId++);
    m_components.insert(id, ComponentRecord{type, pins});
    return id;
}

void CircuitGraph::removeComponent(const QString &componentId)
{
    const QString prefix = componentId + QLatin1Char('#');
    QStringList wireIds;
    for (auto it = m_wires.cbegin(); it != m_wires.cend(); ++it)
    {
        if (it->startEndpoint.startsWith(prefix) || it->endEndpoint.startsWith(prefix))
            wireIds.append(it.key());
    }

    for (const QString &wireId : wireIds)
        m_wires.remove(wireId);

    m_components.remove(componentId);
    rebuildNodes();
}

QString CircuitGraph::addJunction(const QPointF &position)
{
    const QString id = QStringLiteral("J%1").arg(m_nextJunctionId++);
    m_junctions.insert(id, JunctionModel{id, position});
    return id;
}

void CircuitGraph::updateJunction(const QString &junctionId, const QPointF &position)
{
    auto it = m_junctions.find(junctionId);
    if (it != m_junctions.end())
        it->position = position;
}

void CircuitGraph::removeJunction(const QString &junctionId)
{
    QStringList wireIds;
    for (auto it = m_wires.cbegin(); it != m_wires.cend(); ++it)
    {
        if (it->startEndpoint == junctionId || it->endEndpoint == junctionId)
            wireIds.append(it.key());
    }

    for (const QString &wireId : wireIds)
        m_wires.remove(wireId);

    m_junctions.remove(junctionId);
    rebuildNodes();
}

QString CircuitGraph::addWire(const QString &startEndpoint,
                              const QString &endEndpoint,
                              const QVector<QPointF> &points)
{
    if (startEndpoint.isEmpty() || endEndpoint.isEmpty() || startEndpoint == endEndpoint)
        return {};

    const QString id = QStringLiteral("W%1").arg(m_nextWireId++);
    m_wires.insert(id, WireModel{id, startEndpoint, endEndpoint, points});
    rebuildNodes();
    return id;
}

void CircuitGraph::updateWirePoints(const QString &wireId, const QVector<QPointF> &points)
{
    auto it = m_wires.find(wireId);
    if (it != m_wires.end())
        it->points = points;
}

void CircuitGraph::removeWire(const QString &wireId)
{
    if (m_wires.remove(wireId) > 0)
        rebuildNodes();
}

QStringList CircuitGraph::wiresForEndpoint(const QString &endpoint) const
{
    QStringList result;
    for (auto it = m_wires.cbegin(); it != m_wires.cend(); ++it)
    {
        if (it->startEndpoint == endpoint || it->endEndpoint == endpoint)
            result.append(it.key());
    }
    return result;
}

QStringList CircuitGraph::wiresForComponent(const QString &componentId) const
{
    QStringList result;
    const QString prefix = componentId + QLatin1Char('#');
    for (auto it = m_wires.cbegin(); it != m_wires.cend(); ++it)
    {
        if (it->startEndpoint.startsWith(prefix) || it->endEndpoint.startsWith(prefix))
            result.append(it.key());
    }
    return result;
}

QString CircuitGraph::nodeForEndpoint(const QString &endpoint) const
{
    return m_endpointToNode.value(endpoint);
}

const QHash<QString, WireModel> &CircuitGraph::wires() const
{
    return m_wires;
}

const QHash<QString, JunctionModel> &CircuitGraph::junctions() const
{
    return m_junctions;
}

const QHash<QString, NodeModel> &CircuitGraph::nodes() const
{
    return m_nodes;
}

void CircuitGraph::clear()
{
    m_components.clear();
    m_wires.clear();
    m_junctions.clear();
    m_nodes.clear();
    m_endpointToNode.clear();
    m_nextComponentId = 1;
    m_nextWireId = 1;
    m_nextJunctionId = 1;
}

QString CircuitGraph::pinEndpoint(const QString &componentId, int pinIndex)
{
    return componentId + QLatin1Char('#') + QString::number(pinIndex);
}

bool CircuitGraph::parsePinEndpoint(const QString &endpoint, QString &componentId, int &pinIndex)
{
    static const QRegularExpression expression(QStringLiteral("^(C\\d+)#(\\d+)$"));
    const QRegularExpressionMatch match = expression.match(endpoint);
    if (!match.hasMatch())
        return false;

    bool ok = false;
    const int parsedIndex = match.captured(2).toInt(&ok);
    if (!ok)
        return false;

    componentId = match.captured(1);
    pinIndex = parsedIndex;
    return true;
}

void CircuitGraph::rebuildNodes()
{
    m_nodes.clear();
    m_endpointToNode.clear();

    QHash<QString, QString> parents;
    for (const WireModel &wire : m_wires)
    {
        if (!parents.contains(wire.startEndpoint))
            parents.insert(wire.startEndpoint, wire.startEndpoint);
        if (!parents.contains(wire.endEndpoint))
            parents.insert(wire.endEndpoint, wire.endEndpoint);
        unite(parents, wire.startEndpoint, wire.endEndpoint);
    }

    QHash<QString, QStringList> groups;
    QStringList endpoints = parents.keys();
    std::sort(endpoints.begin(), endpoints.end());
    for (const QString &endpoint : endpoints)
        groups[findRoot(parents, endpoint)].append(endpoint);

    QStringList roots = groups.keys();
    std::sort(roots.begin(), roots.end());

    int nextNode = 1;
    for (const QString &root : roots)
    {
        NodeModel node;
        node.id = QStringLiteral("N%1").arg(nextNode++);
        node.endpoints = groups.value(root);

        for (const WireModel &wire : m_wires)
        {
            if (node.endpoints.contains(wire.startEndpoint) ||
                node.endpoints.contains(wire.endEndpoint))
            {
                node.wireIds.append(wire.id);
            }
        }
        node.wireIds.removeDuplicates();

        for (const QString &endpoint : node.endpoints)
            m_endpointToNode.insert(endpoint, node.id);
        m_nodes.insert(node.id, node);
    }
}
