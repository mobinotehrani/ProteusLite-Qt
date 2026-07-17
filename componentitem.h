#pragma once

#include "circuitmodel.h"
#include "componentdefinition.h"

#include <QGraphicsObject>
#include <QVector>

class QGraphicsSceneContextMenuEvent;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;

class ComponentItem final : public QGraphicsObject
{
    Q_OBJECT

public:
    enum
    {
        Type = QGraphicsItem::UserType + 501
    };

    ComponentItem(const QString &modelId,
                  const ComponentDefinition &definition,
                  int gridSpacing,
                  QGraphicsItem *parent = nullptr);

    int type() const override;
    QRectF boundingRect() const override;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

    QString modelId() const;
    QString componentType() const;
    QString reference() const;
    QString value() const;

    QVector<PinModel> pins() const;
    QPointF scenePinPosition(int pinIndex) const;
    int pinAtScenePosition(const QPointF &scenePosition, qreal tolerance) const;
    void setHighlightedPin(int pinIndex);

    void rotateClockwise();
    void mirrorHorizontal();
    void mirrorVertical();
    void openProperties();

    static QVector<PinModel> pinsForType(const QString &type);
    static void resetReferenceCounters();

signals:
    void geometryChanged();
    void edited();
    void deleteRequested(ComponentItem *item);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    static QVector<PinModel> createPins(const QString &type);
    static QString referencePrefix(const QString &type);
    static QString nextReference(const QString &type);

    void applyVisualTransform();
    void drawSymbol(QPainter *painter) const;
    void drawPinLabels(QPainter *painter) const;

    QString m_modelId;
    ComponentDefinition m_definition;
    QString m_reference;
    QString m_value;
    QVector<PinModel> m_pins;
    int m_gridSpacing{20};
    int m_rotationSteps{0};
    bool m_mirrorHorizontal{false};
    bool m_mirrorVertical{false};
    int m_highlightedPin{-1};
};
