#pragma once

#include <QGraphicsPathItem>
#include <QString>
#include <QVector>

class WireItem final : public QGraphicsPathItem
{
public:
    enum
    {
        Type = QGraphicsItem::UserType + 502
    };

    WireItem(const QString &modelId,
             const QString &startEndpoint,
             const QString &endEndpoint,
             QGraphicsItem *parent = nullptr);

    int type() const override;

    QString modelId() const;
    QString startEndpoint() const;
    QString endEndpoint() const;
    QVector<QPointF> points() const;

    void setPoints(const QVector<QPointF> &points);
    QPainterPath shape() const override;

private:
    QString m_modelId;
    QString m_startEndpoint;
    QString m_endEndpoint;
    QVector<QPointF> m_points;
};
