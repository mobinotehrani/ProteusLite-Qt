#pragma once

#include "basiccomponent.h"
#include "circuitmodel.h"
#include "componentdefinition.h"

#include <QGraphicsObject>
#include <QPointF>
#include <QVector>

#include <memory>
#include <optional>

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
    ~ComponentItem() override;

    int type() const override;
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

    QString modelId() const;
    QString componentType() const;
    QString reference() const;
    QString value() const;
    QString runtimeText() const;

    QVector<PinModel> pins() const;
    QPointF scenePinPosition(int pinIndex) const;
    int pinAtScenePosition(const QPointF &scenePosition, qreal tolerance) const;
    void setHighlightedPin(int pinIndex);

    void rotateClockwise();
    void mirrorHorizontal();
    void mirrorVertical();
    void openProperties();

    Component *componentModel();
    const Component *componentModel() const;
    ComponentStepResult updateSimulation(const QVector<std::optional<double>> &pinVoltages,
                                         double timeSeconds);
    QVariantMap componentState() const;
    void restoreComponentState(const QVariantMap &state);

    static QVector<PinModel> pinsForType(const QString &type);
    static void resetReferenceCounters();

  signals:
    void geometryChanged();
    void edited();
    void pinsChanged();
    void stateChanged();
    void simulationWarning(const QString &message);
    void deleteRequested(ComponentItem *item);

  protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

  private:
    static QVector<PinModel> createPins(const QString &type);
    static QString referencePrefix(const QString &type);
    static QString nextReference(const QString &type);
    static QVector<QPointF> pinPositionsForType(const QString &type, int count);

    QVector<PinModel> pinsForModel() const;
    void refreshPins(bool notify);
    void applyVisualTransform();
    void syncValueText();
    void drawSymbol(QPainter *painter) const;
    void drawSource(QPainter *painter, const QString &label) const;
    void drawSwitch(QPainter *painter, bool closed) const;
    void drawPushButton(QPainter *painter, bool pressed) const;
    void drawLed(QPainter *painter) const;
    void drawSevenSegment(QPainter *painter) const;
    void drawLogicGate(QPainter *painter) const;
    void drawGenericBody(QPainter *painter) const;
    void drawPinLabels(QPainter *painter) const;

    QString m_modelId;
    ComponentDefinition m_definition;
    QString m_reference;
    QString m_value;
    QVector<PinModel> m_pins;
    std::unique_ptr<Component> m_component;
    int m_gridSpacing{20};
    int m_rotationSteps{0};
    bool m_mirrorHorizontal{false};
    bool m_mirrorVertical{false};
    int m_highlightedPin{-1};
    QPointF m_pressScenePosition;
    bool m_interactionPressed{false};
};
