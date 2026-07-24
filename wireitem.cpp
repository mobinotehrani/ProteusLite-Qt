#include "wireitem.h"

#include <QColor>
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
    setAcceptHoverEvents(true);
    setZValue(2.0);
    updateAppearance();
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

WireItem::SignalState WireItem::signalState() const
{
    return m_signalState;
}

std::optional<double> WireItem::signalVoltage() const
{
    return m_signalVoltage;
}

void WireItem::setPoints(const QVector<QPointF> &points)
{
    m_points = points;
    QPainterPath newPath;
    if (!m_points.isEmpty())
    {
        newPath.moveTo(m_points.first());
        for (int i = 1; i < m_points.size(); ++i)
            newPath.lineTo(m_points.at(i));
    }
    setPath(newPath);
}

void WireItem::setSignalState(SignalState state,
                              std::optional<double> voltage)
{
    if (m_signalState == state && m_signalVoltage == voltage)
        return;
    m_signalState = state;
    m_signalVoltage = voltage;
    updateAppearance();
}

QPainterPath WireItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(14.0);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    return stroker.createStroke(path());
}

void WireItem::updateAppearance()
{
    QColor color(51, 65, 85);
    Qt::PenStyle style = Qt::SolidLine;
    QString stateText = QStringLiteral("Idle");

    switch (m_signalState)
    {
    case SignalState::Idle:
        break;
    case SignalState::Low:
        color = QColor(37, 99, 235);
        stateText = QStringLiteral("LOW");
        break;
    case SignalState::High:
        color = QColor(220, 38, 38);
        stateText = QStringLiteral("HIGH");
        break;
    case SignalState::Analog:
        color = QColor(217, 119, 6);
        stateText = QStringLiteral("ANALOG");
        break;
    case SignalState::Undefined:
        color = QColor(100, 116, 139);
        style = Qt::DashLine;
        stateText = QStringLiteral("FLOATING");
        break;
    case SignalState::Conflict:
        color = QColor(192, 38, 211);
        style = Qt::DashDotLine;
        stateText = QStringLiteral("CONFLICT");
        break;
    }

    setPen(QPen(color, 3.0, style, Qt::RoundCap, Qt::RoundJoin));

    QString tooltip = QStringLiteral("Signal: %1").arg(stateText);
    if (m_signalVoltage.has_value())
        tooltip += QStringLiteral("\nVoltage: %1 V").arg(*m_signalVoltage, 0, 'f', 4);
    setToolTip(tooltip);
    update();
}
