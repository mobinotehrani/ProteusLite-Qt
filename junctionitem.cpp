#include "junctionitem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <algorithm>
#include <cmath>

JunctionItem::JunctionItem(const QString &modelId, int gridSpacing, QGraphicsItem *parent)
    : QGraphicsObject(parent), m_modelId(modelId), m_gridSpacing(std::max(5, gridSpacing))
{
    setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
    setCursor(Qt::SizeAllCursor);
    setToolTip(tr("Electrical junction %1").arg(modelId));
    setZValue(20.0);
}

int JunctionItem::type() const
{
    return Type;
}

QRectF JunctionItem::boundingRect() const
{
    return QRectF(-7.0, -7.0, 14.0, 14.0);
}

void JunctionItem::paint(QPainter *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(isSelected() ? QColor(220, 38, 38) : QColor(30, 64, 175), 1.2));
    painter->setBrush(isSelected() ? QColor(254, 202, 202) : QColor(37, 99, 235));
    painter->drawEllipse(QPointF(0.0, 0.0), isSelected() ? 5.5 : 4.5,
                         isSelected() ? 5.5 : 4.5);
}

QString JunctionItem::modelId() const
{
    return m_modelId;
}

QVariant JunctionItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && scene())
    {
        const QPointF proposed = value.toPointF();
        const qreal spacing = static_cast<qreal>(m_gridSpacing);
        return QPointF(std::round(proposed.x() / spacing) * spacing,
                       std::round(proposed.y() / spacing) * spacing);
    }

    const QVariant result = QGraphicsObject::itemChange(change, value);
    if (change == ItemPositionHasChanged)
        emit geometryChanged();
    return result;
}
