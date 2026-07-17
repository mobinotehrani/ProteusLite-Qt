#pragma once

#include <QGraphicsObject>

class QPainter;
class QStyleOptionGraphicsItem;

class JunctionItem final : public QGraphicsObject
{
    Q_OBJECT

public:
    enum
    {
        Type = QGraphicsItem::UserType + 503
    };

    explicit JunctionItem(const QString &modelId, int gridSpacing, QGraphicsItem *parent = nullptr);

    int type() const override;
    QRectF boundingRect() const override;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

    QString modelId() const;

signals:
    void geometryChanged();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QString m_modelId;
    int m_gridSpacing{20};
};
