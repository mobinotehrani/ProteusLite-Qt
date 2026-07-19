#include "section05controller.h"

#include "canvasview.h"
#include "componentdefinition.h"
#include "componentitem.h"
#include "componentlibrarypanel.h"
#include "junctionitem.h"
#include "wireitem.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QLineF>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPen>
#include <QStatusBar>
#include <QTimer>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <limits>

Section05Controller::Section05Controller(QMainWindow *window,
                                         CanvasView *canvas,
                                         ComponentLibraryPanel *library,
                                         QObject *parent)
    : QObject(parent), m_window(window), m_canvas(canvas), m_library(library)
{
    m_canvas->installEventFilter(this);
    m_canvas->viewport()->installEventFilter(this);

    connect(m_library,
            &ComponentLibraryPanel::placementRequested,
            this,
            [this](const QString &componentId, const QString &displayName)
            {
                m_pendingComponentId = componentId;
                m_pendingComponentName = displayName;
            });

    connect(m_canvas,
            &CanvasView::placementPointChosen,
            this,
            [this](const QPointF &position)
            {
                createComponent(position);
                m_pendingComponentId.clear();
                m_pendingComponentName.clear();
            });

    connect(m_window,
            &QWidget::windowTitleChanged,
            this,
            [this](const QString &title)
            {
                if (m_lastWindowTitle.isEmpty())
                {
                    m_lastWindowTitle = title;
                    return;
                }
                if (title != m_lastWindowTitle)
                {
                    m_lastWindowTitle = title;
                    clearCircuit();
                }
            });
    m_lastWindowTitle = m_window->windowTitle();

    setStatus(tr("Section 5 ready: place a component, then click one pin and "
                 "another pin to draw a 90-degree wire."),
              6500);
}

const CircuitGraph &Section05Controller::circuitGraph() const
{
    return m_graph;
}

QList<ComponentItem *> Section05Controller::componentItems() const
{
    return m_components.values();
}

ComponentItem *Section05Controller::componentItem(const QString &modelId) const
{
    return m_components.value(modelId, nullptr);
}

CanvasView *Section05Controller::canvasView() const
{
    return m_canvas;
}

bool Section05Controller::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_canvas || (watched != m_canvas && watched != m_canvas->viewport()))
        return QObject::eventFilter(watched, event);

    if (event->type() == QEvent::MouseMove && watched == m_canvas->viewport())
    {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const QPointF scenePosition = m_canvas->mapToScene(mousePosition(mouseEvent));
        if (!m_wireStartEndpoint.isEmpty())
            updateWirePreview(scenePosition);
        return false;
    }

    if (event->type() == QEvent::MouseButtonPress && watched == m_canvas->viewport())
    {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const QPoint viewPosition = mousePosition(mouseEvent);
        const QPointF scenePosition = m_canvas->mapToScene(viewPosition);

        if (mouseEvent->button() == Qt::RightButton)
        {
            if (!m_wireStartEndpoint.isEmpty())
            {
                cancelWire();
                return true;
            }

            if (WireItem *wire = wireAt(scenePosition))
            {
                showWireMenu(wire, m_canvas->viewport()->mapToGlobal(viewPosition), scenePosition);
                return true;
            }

            const EndpointHit endpoint = endpointAt(scenePosition);
            if (endpoint.junction)
            {
                showJunctionMenu(endpoint.junction, m_canvas->viewport()->mapToGlobal(viewPosition));
                return true;
            }
            return false;
        }

        if (mouseEvent->button() != Qt::LeftButton || m_canvas->hasPreparedComponent())
            return false;

        const EndpointHit endpoint = endpointAt(scenePosition);
        if (m_wireStartEndpoint.isEmpty())
        {
            if (endpoint.isValid())
            {
                beginWire(endpoint);
                return true;
            }
            return false;
        }

        if (endpoint.isValid())
        {
            completeWire(endpoint);
            return true;
        }

        if (WireItem *wire = wireAt(scenePosition))
        {
            JunctionItem *junction = splitWireAt(wire, scenePosition);
            if (junction)
            {
                EndpointHit junctionHit;
                junctionHit.key = junction->modelId();
                junctionHit.junction = junction;
                completeWire(junctionHit);
            }
            return true;
        }

        setStatus(tr("Finish the wire on another pin, a junction, or an existing "
                     "wire. Right-click or Esc cancels."));
        return true;
    }

    if (event->type() == QEvent::KeyPress && watched == m_canvas)
    {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape && !m_wireStartEndpoint.isEmpty())
        {
            cancelWire();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace)
        {
            handleDeleteSelection();
            return true;
        }
        if (keyEvent->key() == Qt::Key_R || keyEvent->key() == Qt::Key_H || keyEvent->key() == Qt::Key_V)
        {
            handleTransformShortcut(keyEvent->key());
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

void Section05Controller::createComponent(const QPointF &position)
{
    if (!m_canvas || m_pendingComponentId.isEmpty())
        return;

    const ComponentDefinition *definition = ComponentCatalog::find(m_pendingComponentId);
    if (!definition)
    {
        setStatus(tr("The selected component definition was not found."));
        return;
    }

    const QVector<PinModel> pins = ComponentItem::pinsForType(definition->id);
    const QString modelId = m_graph.addComponent(definition->id, pins);
    auto *component = new ComponentItem(modelId, *definition, m_canvas->gridSpacing());
    component->setPos(m_canvas->snapToGrid(position));
    m_canvas->scene()->addItem(component);
    m_components.insert(modelId, component);

    connect(
        component, &ComponentItem::geometryChanged, this, [this, modelId] { updateConnectedWires(modelId); });
    connect(component,
            &ComponentItem::pinsChanged,
            this,
            [this, component] { handleComponentPinsChanged(component); });
    connect(component,
            &ComponentItem::deleteRequested,
            this,
            [this](ComponentItem *item) { removeComponent(item); });
    connect(component,
            &ComponentItem::edited,
            this,
            [this, component]
            {
                if (component)
                    setStatus(tr("Updated %1.").arg(component->reference()), 2200);
            });

    component->setSelected(true);
    component->setFocus();
    setStatus(tr("Placed %1. Click a pin to begin wiring; R rotates, H/V "
                 "mirrors, Delete removes.")
                  .arg(component->reference()),
              5200);
}

void Section05Controller::removeComponent(ComponentItem *component)
{
    if (!component)
        return;

    const QString componentId = component->modelId();
    if (m_wireStartEndpoint.startsWith(componentId + QLatin1Char('#')))
        cancelWire(false);

    const QStringList wireIds = m_graph.wiresForComponent(componentId);
    for (const QString &wireId : wireIds)
        removeWire(m_wires.value(wireId));

    m_graph.removeComponent(componentId);
    m_components.remove(componentId);
    if (component->scene())
        component->scene()->removeItem(component);
    component->deleteLater();
    setStatus(tr("Component deleted."), 2200);
}

void Section05Controller::removeWire(WireItem *wire)
{
    if (!wire)
        return;

    const QString wireId = wire->modelId();
    m_graph.removeWire(wireId);
    m_wires.remove(wireId);
    if (wire->scene())
        wire->scene()->removeItem(wire);
    delete wire;
}

void Section05Controller::removeJunction(JunctionItem *junction)
{
    if (!junction)
        return;

    const QString junctionId = junction->modelId();
    if (m_wireStartEndpoint == junctionId)
        cancelWire(false);

    const QStringList wireIds = m_graph.wiresForEndpoint(junctionId);
    for (const QString &wireId : wireIds)
        removeWire(m_wires.value(wireId));

    m_graph.removeJunction(junctionId);
    m_junctions.remove(junctionId);
    if (junction->scene())
        junction->scene()->removeItem(junction);
    junction->deleteLater();
    setStatus(tr("Junction deleted with its connected wires."), 2600);
}

void Section05Controller::clearCircuit()
{
    cancelWire(false);

    const QList<WireItem *> wires = m_wires.values();
    for (WireItem *wire : wires)
    {
        if (wire && wire->scene())
            wire->scene()->removeItem(wire);
        delete wire;
    }

    for (ComponentItem *component : m_components)
    {
        if (component)
        {
            if (component->scene())
                component->scene()->removeItem(component);
            component->deleteLater();
        }
    }

    for (JunctionItem *junction : m_junctions)
    {
        if (junction)
        {
            if (junction->scene())
                junction->scene()->removeItem(junction);
            junction->deleteLater();
        }
    }

    m_wires.clear();
    m_components.clear();
    m_junctions.clear();
    m_graph.clear();
    ComponentItem::resetReferenceCounters();
}

Section05Controller::EndpointHit Section05Controller::endpointAt(const QPointF &scenePosition) const
{
    EndpointHit best;
    if (!m_canvas)
        return best;

    const qreal zoom = std::max(0.15, m_canvas->zoomFactor());
    const qreal tolerance = 13.0 / zoom;
    qreal bestDistance = tolerance;

    for (auto it = m_junctions.cbegin(); it != m_junctions.cend(); ++it)
    {
        JunctionItem *junction = it.value();
        if (!junction)
            continue;
        const qreal distance = QLineF(junction->scenePos(), scenePosition).length();
        if (distance <= bestDistance)
        {
            bestDistance = distance;
            best.key = junction->modelId();
            best.junction = junction;
            best.component = nullptr;
            best.pinIndex = -1;
        }
    }

    for (auto it = m_components.cbegin(); it != m_components.cend(); ++it)
    {
        ComponentItem *component = it.value();
        if (!component)
            continue;
        const int pinIndex = component->pinAtScenePosition(scenePosition, bestDistance);
        if (pinIndex < 0)
            continue;

        const qreal distance = QLineF(component->scenePinPosition(pinIndex), scenePosition).length();
        if (distance <= bestDistance)
        {
            bestDistance = distance;
            best.key = CircuitGraph::pinEndpoint(component->modelId(), pinIndex);
            best.component = component;
            best.junction = nullptr;
            best.pinIndex = pinIndex;
        }
    }

    return best;
}

WireItem *Section05Controller::wireAt(const QPointF &scenePosition) const
{
    if (!m_canvas || !m_canvas->scene())
        return nullptr;

    const QList<QGraphicsItem *> items = m_canvas->scene()->items(
        scenePosition, Qt::IntersectsItemShape, Qt::DescendingOrder, m_canvas->transform());
    for (QGraphicsItem *item : items)
    {
        if (item && item->type() == WireItem::Type)
            return static_cast<WireItem *>(item);
    }
    return nullptr;
}

QPointF Section05Controller::endpointPosition(const QString &endpoint) const
{
    QString componentId;
    int pinIndex = -1;
    if (CircuitGraph::parsePinEndpoint(endpoint, componentId, pinIndex))
    {
        ComponentItem *component = m_components.value(componentId);
        return component ? component->scenePinPosition(pinIndex) : QPointF();
    }

    JunctionItem *junction = m_junctions.value(endpoint);
    return junction ? junction->scenePos() : QPointF();
}

QPointF Section05Controller::nearestPointOnWire(const WireItem *wire, const QPointF &position) const
{
    if (!wire)
        return position;

    const QVector<QPointF> points = wire->points();
    if (points.size() < 2)
        return position;

    QPointF closest = points.first();
    qreal closestSquared = std::numeric_limits<qreal>::max();

    for (int i = 1; i < points.size(); ++i)
    {
        const QPointF first = points[i - 1];
        const QPointF second = points[i];
        const QPointF delta = second - first;
        const qreal lengthSquared = delta.x() * delta.x() + delta.y() * delta.y();
        qreal t = 0.0;
        if (lengthSquared > 0.0)
        {
            const QPointF relative = position - first;
            t = (relative.x() * delta.x() + relative.y() * delta.y()) / lengthSquared;
            t = std::clamp(t, 0.0, 1.0);
        }
        const QPointF candidate = first + delta * t;
        const QPointF difference = position - candidate;
        const qreal squared = difference.x() * difference.x() + difference.y() * difference.y();
        if (squared < closestSquared)
        {
            closestSquared = squared;
            closest = candidate;
        }
    }

    return m_canvas ? m_canvas->snapToGrid(closest) : closest;
}

void Section05Controller::beginWire(const EndpointHit &endpoint)
{
    if (!m_canvas || !endpoint.isValid())
        return;

    cancelWire(false);
    m_wireStartEndpoint = endpoint.key;
    m_wirePreview = new QGraphicsPathItem;
    m_wirePreview->setPen(QPen(QColor(37, 99, 235), 2.2, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
    m_wirePreview->setZValue(1.0);
    m_canvas->scene()->addItem(m_wirePreview);
    updateWirePreview(endpointPosition(endpoint.key));
    setStatus(tr("Wire started. Click another pin, a junction, or an existing "
                 "wire to finish."),
              5000);
}

void Section05Controller::updateWirePreview(const QPointF &position)
{
    if (!m_wirePreview || m_wireStartEndpoint.isEmpty())
        return;

    const QPointF start = endpointPosition(m_wireStartEndpoint);
    const QPointF end = m_canvas ? m_canvas->snapToGrid(position) : position;
    const QVector<QPointF> points = orthogonalRoute(start, end);

    QPainterPath path;
    if (!points.isEmpty())
    {
        path.moveTo(points.first());
        for (int i = 1; i < points.size(); ++i)
            path.lineTo(points[i]);
    }
    m_wirePreview->setPath(path);
}

void Section05Controller::completeWire(const EndpointHit &endpoint)
{
    if (!endpoint.isValid() || m_wireStartEndpoint.isEmpty())
        return;

    if (endpoint.key == m_wireStartEndpoint)
    {
        setStatus(tr("A wire needs two different endpoints."), 2400);
        return;
    }

    WireItem *wire = createWire(m_wireStartEndpoint, endpoint.key);
    cancelWire(false);
    if (wire)
    {
        wire->setSelected(true);
        setStatus(tr("90-degree wire created. Drag either component to see the "
                     "wire update dynamically."),
                  4200);
    }
}

JunctionItem *Section05Controller::splitWireAt(WireItem *wire, const QPointF &position)
{
    if (!wire)
        return nullptr;

    const QString startEndpoint = wire->startEndpoint();
    const QString endEndpoint = wire->endEndpoint();
    const QPointF junctionPosition = nearestPointOnWire(wire, position);

    removeWire(wire);
    JunctionItem *junction = createJunction(junctionPosition);
    if (!junction)
        return nullptr;

    createWire(startEndpoint, junction->modelId());
    createWire(junction->modelId(), endEndpoint);
    return junction;
}

JunctionItem *Section05Controller::createJunction(const QPointF &position)
{
    if (!m_canvas)
        return nullptr;

    const QPointF snapped = m_canvas->snapToGrid(position);
    const QString junctionId = m_graph.addJunction(snapped);
    auto *junction = new JunctionItem(junctionId, m_canvas->gridSpacing());
    junction->setPos(snapped);
    m_canvas->scene()->addItem(junction);
    m_junctions.insert(junctionId, junction);

    connect(junction,
            &JunctionItem::geometryChanged,
            this,
            [this, junctionId, junction]
            {
                if (!junction)
                    return;
                m_graph.updateJunction(junctionId, junction->scenePos());
                updateConnectedWires(junctionId);
            });
    return junction;
}

WireItem *Section05Controller::createWire(const QString &startEndpoint, const QString &endEndpoint)
{
    if (!m_canvas || startEndpoint.isEmpty() || endEndpoint.isEmpty() || startEndpoint == endEndpoint)
    {
        return nullptr;
    }

    const QVector<QPointF> points =
        orthogonalRoute(endpointPosition(startEndpoint), endpointPosition(endEndpoint));
    const QString wireId = m_graph.addWire(startEndpoint, endEndpoint, points);
    if (wireId.isEmpty())
        return nullptr;

    auto *wire = new WireItem(wireId, startEndpoint, endEndpoint);
    wire->setPoints(points);
    m_canvas->scene()->addItem(wire);
    m_wires.insert(wireId, wire);
    return wire;
}

void Section05Controller::cancelWire(bool showMessage)
{
    if (m_wirePreview)
    {
        if (m_wirePreview->scene())
            m_wirePreview->scene()->removeItem(m_wirePreview);
        delete m_wirePreview;
        m_wirePreview = nullptr;
    }

    const bool hadWire = !m_wireStartEndpoint.isEmpty();
    m_wireStartEndpoint.clear();
    if (showMessage && hadWire)
        setStatus(tr("Wire creation canceled."), 2200);
}

void Section05Controller::handleComponentPinsChanged(ComponentItem *component)
{
    if (!component)
        return;

    const QStringList removedWireIds = m_graph.updateComponentPins(component->modelId(), component->pins());
    for (const QString &wireId : removedWireIds)
    {
        WireItem *wire = m_wires.take(wireId);
        if (!wire)
            continue;
        if (wire->scene())
            wire->scene()->removeItem(wire);
        delete wire;
    }

    updateConnectedWires(component->modelId());
    setStatus(tr("Updated pins for %1.").arg(component->reference()), 2400);
}

void Section05Controller::updateConnectedWires(const QString &endpointPrefix)
{
    for (WireItem *wire : m_wires)
    {
        if (!wire)
            continue;
        const bool connected = wire->startEndpoint() == endpointPrefix ||
                               wire->endEndpoint() == endpointPrefix ||
                               wire->startEndpoint().startsWith(endpointPrefix + QLatin1Char('#')) ||
                               wire->endEndpoint().startsWith(endpointPrefix + QLatin1Char('#'));
        if (connected)
            updateWireGeometry(wire);
    }

    if (!m_wireStartEndpoint.isEmpty() && (m_wireStartEndpoint == endpointPrefix ||
                                           m_wireStartEndpoint.startsWith(endpointPrefix + QLatin1Char('#'))))
    {
        updateWirePreview(endpointPosition(m_wireStartEndpoint));
    }
}

void Section05Controller::updateWireGeometry(WireItem *wire)
{
    if (!wire)
        return;

    const QVector<QPointF> points =
        orthogonalRoute(endpointPosition(wire->startEndpoint()), endpointPosition(wire->endEndpoint()));
    wire->setPoints(points);
    m_graph.updateWirePoints(wire->modelId(), points);
}

QVector<QPointF> Section05Controller::orthogonalRoute(const QPointF &start, const QPointF &end) const
{
    QVector<QPointF> points;
    points.append(start);
    if (qFuzzyCompare(start.x() + 1.0, end.x() + 1.0) || qFuzzyCompare(start.y() + 1.0, end.y() + 1.0))
    {
        points.append(end);
        return points;
    }

    const qreal horizontalDistance = std::abs(end.x() - start.x());
    const qreal verticalDistance = std::abs(end.y() - start.y());
    const QPointF bend =
        horizontalDistance >= verticalDistance ? QPointF(end.x(), start.y()) : QPointF(start.x(), end.y());
    points.append(bend);
    points.append(end);
    return points;
}

void Section05Controller::handleDeleteSelection()
{
    if (!m_canvas || !m_canvas->scene())
        return;

    const QList<QGraphicsItem *> selected = m_canvas->scene()->selectedItems();
    QList<WireItem *> wires;
    QList<JunctionItem *> junctions;
    QList<ComponentItem *> components;

    for (QGraphicsItem *item : selected)
    {
        if (!item)
            continue;
        if (item->type() == WireItem::Type)
            wires.append(static_cast<WireItem *>(item));
        else if (item->type() == JunctionItem::Type)
            junctions.append(static_cast<JunctionItem *>(item));
        else if (item->type() == ComponentItem::Type)
            components.append(static_cast<ComponentItem *>(item));
    }

    for (WireItem *wire : wires)
        removeWire(wire);
    for (JunctionItem *junction : junctions)
        removeJunction(junction);
    for (ComponentItem *component : components)
        removeComponent(component);

    if (!selected.isEmpty())
        setStatus(tr("Selected circuit items deleted."), 2300);
}

void Section05Controller::handleTransformShortcut(int key)
{
    if (!m_canvas || !m_canvas->scene())
        return;

    int changed = 0;
    const QList<QGraphicsItem *> selected = m_canvas->scene()->selectedItems();
    for (QGraphicsItem *item : selected)
    {
        if (!item || item->type() != ComponentItem::Type)
            continue;
        auto *component = static_cast<ComponentItem *>(item);
        if (key == Qt::Key_R)
            component->rotateClockwise();
        else if (key == Qt::Key_H)
            component->mirrorHorizontal();
        else if (key == Qt::Key_V)
            component->mirrorVertical();
        ++changed;
    }

    if (changed > 0)
        setStatus(tr("Updated %1 selected component(s).").arg(changed), 2300);
}

void Section05Controller::showWireMenu(WireItem *wire,
                                       const QPoint &globalPosition,
                                       const QPointF &scenePosition)
{
    if (!wire)
        return;

    QMenu menu;
    QAction *junctionAction = menu.addAction(tr("Add junction here"));
    QAction *deleteAction = menu.addAction(tr("Delete wire"));
    QAction *chosen = menu.exec(globalPosition);
    if (chosen == junctionAction)
    {
        JunctionItem *junction = splitWireAt(wire, scenePosition);
        if (junction)
        {
            junction->setSelected(true);
            setStatus(tr("Junction added. Click it to start another wire."), 3200);
        }
    }
    else if (chosen == deleteAction)
    {
        removeWire(wire);
        setStatus(tr("Wire deleted."), 2200);
    }
}

void Section05Controller::showJunctionMenu(JunctionItem *junction, const QPoint &globalPosition)
{
    if (!junction)
        return;

    QMenu menu;
    QAction *deleteAction = menu.addAction(tr("Delete junction and connected wires"));
    if (menu.exec(globalPosition) == deleteAction)
        removeJunction(junction);
}

void Section05Controller::setStatus(const QString &message, int timeout) const
{
    if (m_window && m_window->statusBar())
        m_window->statusBar()->showMessage(message, timeout);
}

QPoint Section05Controller::mousePosition(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

namespace
{
void attachSection05Controller()
{
    bool attached = false;
    const auto topLevelWidgets = QApplication::topLevelWidgets();
    for (QWidget *widget : topLevelWidgets)
    {
        auto *window = qobject_cast<QMainWindow *>(widget);
        if (!window || window->property("section05ControllerAttached").toBool())
            continue;

        CanvasView *canvas = window->findChild<CanvasView *>();
        ComponentLibraryPanel *library = window->findChild<ComponentLibraryPanel *>();
        if (!canvas || !library)
            continue;

        new Section05Controller(window, canvas, library, window);
        window->setProperty("section05ControllerAttached", true);
        attached = true;
    }

    if (!attached)
        QTimer::singleShot(100, [] { attachSection05Controller(); });
}

void installSection05Controller()
{
    QTimer::singleShot(0, [] { attachSection05Controller(); });
}
}

Q_COREAPP_STARTUP_FUNCTION(installSection05Controller)
