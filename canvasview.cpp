#include "canvasview.h"

#include <QColor>
#include <QEvent>
#include <QFrame>
#include <QPen>
#include <QRectF>
#include <QString>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace
{
constexpr double MinimumZoom = 0.15;
constexpr double MaximumZoom = 4.0;
constexpr double ZoomStep = 1.15;
constexpr int MajorGridEvery = 5;
}

CanvasView::CanvasView(QWidget *parent)
    : QGraphicsView(parent)
{
    auto *canvasScene = new QGraphicsScene(this);
    setScene(canvasScene);
    setCanvasSize(QSize(3000, 2000));

    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                   QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setDragMode(QGraphicsView::RubberBandDrag);
    setRubberBandSelectionMode(Qt::IntersectsItemShape);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setBackgroundBrush(QColor(220, 227, 236));
}

void CanvasView::setCanvasSize(const QSize &size)
{
    const QSize safeSize(std::max(100, size.width()), std::max(100, size.height()));
    scene()->setSceneRect(QRectF(QPointF(0.0, 0.0), QSizeF(safeSize)));
    m_cursorInsideCanvas = false;
    viewport()->update();
}

QSize CanvasView::canvasSize() const
{
    return scene()->sceneRect().size().toSize();
}

void CanvasView::setGridVisible(bool visible)
{
    if (m_gridVisible == visible)
        return;
    m_gridVisible = visible;
    viewport()->update();
}

bool CanvasView::isGridVisible() const
{
    return m_gridVisible;
}

void CanvasView::setSnapEnabled(bool enabled)
{
    if (m_snapEnabled == enabled)
        return;

    m_snapEnabled = enabled;
    m_snappedCursorPosition = snapToGrid(m_rawCursorPosition);
    viewport()->update();
    if (m_cursorInsideCanvas)
        emit cursorPositionChanged(m_rawCursorPosition, m_snappedCursorPosition);
}

bool CanvasView::isSnapEnabled() const
{
    return m_snapEnabled;
}

void CanvasView::setGridSpacing(int spacing)
{
    const int safeSpacing = std::clamp(spacing, 5, 200);
    if (m_gridSpacing == safeSpacing)
        return;

    m_gridSpacing = safeSpacing;
    m_snappedCursorPosition = snapToGrid(m_rawCursorPosition);
    viewport()->update();
}

int CanvasView::gridSpacing() const
{
    return m_gridSpacing;
}

QPointF CanvasView::snapToGrid(const QPointF &point) const
{
    if (!m_snapEnabled || m_gridSpacing <= 0)
        return point;

    const double spacing = static_cast<double>(m_gridSpacing);
    return QPointF(std::round(point.x() / spacing) * spacing,
                   std::round(point.y() / spacing) * spacing);
}

double CanvasView::zoomFactor() const
{
    return m_zoomFactor;
}

void CanvasView::prepareComponentPlacement(const QString &componentName)
{
    m_preparedComponentName = componentName.trimmed();
    updateInteractionCursor();
    viewport()->update();
}

void CanvasView::cancelComponentPlacement()
{
    if (m_preparedComponentName.isEmpty())
        return;

    m_preparedComponentName.clear();
    updateInteractionCursor();
    viewport()->update();
    emit placementCanceled();
}

bool CanvasView::hasPreparedComponent() const
{
    return !m_preparedComponentName.isEmpty();
}

void CanvasView::zoomIn()
{
    zoomBy(ZoomStep);
}

void CanvasView::zoomOut()
{
    zoomBy(1.0 / ZoomStep);
}

void CanvasView::resetZoom()
{
    const QPointF currentCenter = mapToScene(viewport()->rect().center());
    resetTransform();
    m_zoomFactor = 1.0;
    centerOn(currentCenter);
    emit zoomChanged(m_zoomFactor);
    viewport()->update();
}

void CanvasView::fitSheet()
{
    if (!scene() || scene()->sceneRect().isEmpty() || viewport()->size().isEmpty())
        return;

    const QRectF sheetWithMargin = scene()->sceneRect().adjusted(-40.0, -40.0, 40.0, 40.0);
    fitInView(sheetWithMargin, Qt::KeepAspectRatio);
    m_zoomFactor = transform().m11();
    emit zoomChanged(m_zoomFactor);
    viewport()->update();
}

void CanvasView::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->save();
    painter->fillRect(rect, QColor(220, 227, 236));

    const QRectF sheet = scene()->sceneRect();
    const QRectF visibleSheet = rect.intersected(sheet);
    if (visibleSheet.isEmpty())
    {
        painter->restore();
        return;
    }

    painter->fillRect(visibleSheet, QColor(250, 252, 255));

    if (m_gridVisible)
    {
        const int spacing = m_gridSpacing;
        const int firstX = std::max(0, static_cast<int>(std::floor(visibleSheet.left() / spacing)) * spacing);
        const int firstY = std::max(0, static_cast<int>(std::floor(visibleSheet.top() / spacing)) * spacing);

        const QPen minorPen(QColor(226, 232, 240), 0.0);
        const QPen majorPen(QColor(203, 213, 225), 0.0);

        for (int x = firstX; x <= visibleSheet.right(); x += spacing)
        {
            painter->setPen((x / spacing) % MajorGridEvery == 0 ? majorPen : minorPen);
            painter->drawLine(QPointF(x, visibleSheet.top()), QPointF(x, visibleSheet.bottom()));
        }

        for (int y = firstY; y <= visibleSheet.bottom(); y += spacing)
        {
            painter->setPen((y / spacing) % MajorGridEvery == 0 ? majorPen : minorPen);
            painter->drawLine(QPointF(visibleSheet.left(), y), QPointF(visibleSheet.right(), y));
        }
    }

    painter->setPen(QPen(QColor(71, 85, 105), 0.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(sheet);

    painter->setPen(QPen(QColor(37, 99, 235), 0.0));
    painter->drawLine(QPointF(0.0, 0.0), QPointF(40.0, 0.0));
    painter->drawLine(QPointF(0.0, 0.0), QPointF(0.0, 40.0));
    painter->setBrush(QColor(37, 99, 235));
    painter->drawEllipse(QPointF(0.0, 0.0), 3.0, 3.0);

    painter->restore();
}

void CanvasView::drawForeground(QPainter *painter, const QRectF &)
{
    if (!m_cursorInsideCanvas)
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (m_snapEnabled)
    {
        const QPointF point = m_snappedCursorPosition;
        painter->setPen(QPen(QColor(37, 99, 235, 210), 0.0, Qt::DashLine));
        painter->drawLine(point + QPointF(-10.0, 0.0), point + QPointF(10.0, 0.0));
        painter->drawLine(point + QPointF(0.0, -10.0), point + QPointF(0.0, 10.0));
        painter->setPen(QPen(QColor(29, 78, 216), 0.0));
        painter->setBrush(QColor(147, 197, 253, 110));
        painter->drawEllipse(point, 4.0, 4.0);
    }

    if (!m_preparedComponentName.isEmpty())
    {
        const QPointF point = m_snapEnabled ? m_snappedCursorPosition : m_rawCursorPosition;
        const QRectF labelRect(point.x() + 14.0, point.y() - 36.0, 190.0, 28.0);
        painter->setPen(QPen(QColor(37, 99, 235), 0.0));
        painter->setBrush(QColor(239, 246, 255, 235));
        painter->drawRoundedRect(labelRect, 6.0, 6.0);
        painter->setPen(QColor(30, 64, 175));
        painter->drawText(labelRect.adjusted(8.0, 0.0, -8.0, 0.0),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          tr("Ready: %1").arg(m_preparedComponentName));
    }

    painter->restore();
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() == 0)
    {
        QGraphicsView::wheelEvent(event);
        return;
    }

    updateCursorPosition(wheelPosition(event));
    zoomBy(event->angleDelta().y() > 0 ? ZoomStep : 1.0 / ZoomStep);
    event->accept();
}

void CanvasView::mousePressEvent(QMouseEvent *event)
{
    const QPoint position = mousePosition(event);
    updateCursorPosition(position);

    if (event->button() == Qt::RightButton && hasPreparedComponent())
    {
        cancelComponentPlacement();
        event->accept();
        return;
    }

    if (shouldStartPanning(event))
    {
        beginPanning(position);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && hasPreparedComponent() && m_cursorInsideCanvas)
    {
        emit placementPointChosen(m_snapEnabled ? m_snappedCursorPosition : m_rawCursorPosition);
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint position = mousePosition(event);
    updateCursorPosition(position);

    if (m_panning)
    {
        const QPoint delta = position - m_lastPanPosition;
        m_lastPanPosition = position;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_panning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton))
    {
        endPanning();
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && hasPreparedComponent())
    {
        cancelComponentPlacement();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Space && !event->isAutoRepeat())
    {
        m_spacePressed = true;
        updateInteractionCursor();
        event->accept();
        return;
    }

    QGraphicsView::keyPressEvent(event);
}

void CanvasView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat())
    {
        m_spacePressed = false;
        updateInteractionCursor();
        event->accept();
        return;
    }

    QGraphicsView::keyReleaseEvent(event);
}

void CanvasView::leaveEvent(QEvent *event)
{
    m_cursorInsideCanvas = false;
    emit cursorLeftCanvas();
    viewport()->update();
    QGraphicsView::leaveEvent(event);
}

QPoint CanvasView::mousePosition(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

QPoint CanvasView::wheelPosition(const QWheelEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

void CanvasView::zoomBy(double multiplier)
{
    const double target = std::clamp(m_zoomFactor * multiplier, MinimumZoom, MaximumZoom);
    const double actualMultiplier = target / m_zoomFactor;
    if (qFuzzyCompare(actualMultiplier, 1.0))
        return;

    scale(actualMultiplier, actualMultiplier);
    m_zoomFactor = target;
    emit zoomChanged(m_zoomFactor);
    viewport()->update();
}

void CanvasView::updateCursorPosition(const QPoint &viewPosition)
{
    m_rawCursorPosition = mapToScene(viewPosition);
    m_cursorInsideCanvas = scene()->sceneRect().contains(m_rawCursorPosition);

    if (!m_cursorInsideCanvas)
    {
        emit cursorLeftCanvas();
        viewport()->update();
        return;
    }

    m_snappedCursorPosition = snapToGrid(m_rawCursorPosition);
    emit cursorPositionChanged(m_rawCursorPosition, m_snappedCursorPosition);
    viewport()->update();
}

bool CanvasView::shouldStartPanning(const QMouseEvent *event) const
{
    return event->button() == Qt::MiddleButton ||
           (event->button() == Qt::LeftButton && m_spacePressed);
}

void CanvasView::beginPanning(const QPoint &viewPosition)
{
    m_panning = true;
    m_lastPanPosition = viewPosition;
    setCursor(Qt::ClosedHandCursor);
    emit panningChanged(true);
}

void CanvasView::endPanning()
{
    m_panning = false;
    updateInteractionCursor();
    emit panningChanged(false);
}

void CanvasView::updateInteractionCursor()
{
    if (m_panning)
        setCursor(Qt::ClosedHandCursor);
    else if (m_spacePressed)
        setCursor(Qt::OpenHandCursor);
    else if (hasPreparedComponent())
        setCursor(Qt::CrossCursor);
    else
        unsetCursor();
}
