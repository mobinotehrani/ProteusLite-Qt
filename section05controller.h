#pragma once

#include "circuitmodel.h"

#include <QObject>
#include <QHash>
#include <QPoint>

class CanvasView;
class QEvent;
class ComponentItem;
class ComponentLibraryPanel;
class JunctionItem;
class QGraphicsPathItem;
class QMainWindow;
class QMouseEvent;
class WireItem;

class Section05Controller final : public QObject
{
    Q_OBJECT

public:
    Section05Controller(QMainWindow *window,
                        CanvasView *canvas,
                        ComponentLibraryPanel *library,
                        QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct EndpointHit
    {
        QString key;
        ComponentItem *component{};
        JunctionItem *junction{};
        int pinIndex{-1};

        bool isValid() const { return !key.isEmpty(); }
    };

    void createComponent(const QPointF &position);
    void removeComponent(ComponentItem *component);
    void removeWire(WireItem *wire);
    void removeJunction(JunctionItem *junction);
    void clearCircuit();

    EndpointHit endpointAt(const QPointF &scenePosition) const;
    WireItem *wireAt(const QPointF &scenePosition) const;
    QPointF endpointPosition(const QString &endpoint) const;
    QPointF nearestPointOnWire(const WireItem *wire, const QPointF &position) const;

    void beginWire(const EndpointHit &endpoint);
    void updateWirePreview(const QPointF &position);
    void completeWire(const EndpointHit &endpoint);
    JunctionItem *splitWireAt(WireItem *wire, const QPointF &position);
    JunctionItem *createJunction(const QPointF &position);
    WireItem *createWire(const QString &startEndpoint, const QString &endEndpoint);
    void cancelWire(bool showMessage = true);

    void updateConnectedWires(const QString &endpointPrefix);
    void updateWireGeometry(WireItem *wire);
    QVector<QPointF> orthogonalRoute(const QPointF &start, const QPointF &end) const;

    void handleDeleteSelection();
    void handleTransformShortcut(int key);
    void showWireMenu(WireItem *wire, const QPoint &globalPosition, const QPointF &scenePosition);
    void showJunctionMenu(JunctionItem *junction, const QPoint &globalPosition);
    void setStatus(const QString &message, int timeout = 3500) const;

    static QPoint mousePosition(const QMouseEvent *event);

    QMainWindow *m_window{};
    CanvasView *m_canvas{};
    ComponentLibraryPanel *m_library{};

    CircuitGraph m_graph;
    QHash<QString, ComponentItem *> m_components;
    QHash<QString, JunctionItem *> m_junctions;
    QHash<QString, WireItem *> m_wires;

    QString m_pendingComponentId;
    QString m_pendingComponentName;
    QString m_wireStartEndpoint;
    QGraphicsPathItem *m_wirePreview{};
    QString m_lastWindowTitle;
};
