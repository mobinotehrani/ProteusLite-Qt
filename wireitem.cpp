#include "wireitem.h"

#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPen>

WireItem::WireItem(const QString &modelId,
                   const QString &startEndpoint,
                   const QString &endEndpoint,
                   QGraphicsItem *parent)
    : QGraphicsPathItem(parent),
      m_modelId(modelId),
      m_startEndpoint(startEndpoint),
      m_endEndpoint(endEndpoint)
{
    setFlags(ItemIsSelectable);
    setPen(QPen(QColor(51, 65, 85), 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    setZValue(2.0);
}

int WireItem::type() const
{
    return Type;
}

QString WireItem::modelId() const
{
    return m_modelId;
}

QString WireItem::startEndpoint() const
{
    return m_startEndpoint;
}

QString WireItem::endEndpoint() const
{
    return m_endEndpoint;
}

QVector<QPointF> WireItem::points() const
{
    return m_points;
}

void WireItem::setPoints(const QVector<QPointF> &points)
{
    m_points = points;

    QPainterPath newPath;
    if (!m_points.isEmpty())
    {
        newPath.moveTo(m_points.first());
        for (int i = 1; i < m_points.size(); ++i)
            newPath.lineTo(m_points[i]);
    }
    setPath(newPath);
}

QPainterPath WireItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(14.0);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    return stroker.createStroke(path());
}
