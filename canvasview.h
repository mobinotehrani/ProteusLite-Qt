#pragma once

#include <QGraphicsView>
#include <QPointF>
#include <QSize>

class CanvasView final : public QGraphicsView
{
    Q_OBJECT

public:
    explicit CanvasView(QWidget *parent = nullptr);

    void setCanvasSize(const QSize &size);
    QSize canvasSize() const;

    void setGridVisible(bool visible);
    bool isGridVisible() const;

    void setSnapEnabled(bool enabled);
    bool isSnapEnabled() const;

    void setGridSpacing(int spacing);
    int gridSpacing() const;

    QPointF snapToGrid(const QPointF &point) const;
    double zoomFactor() const;

public slots:
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitSheet();

signals:
    void cursorPositionChanged(const QPointF &rawPosition, const QPointF &snappedPosition);
    void cursorLeftCanvas();
    void zoomChanged(double factor);
    void panningChanged(bool active);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    static QPoint mousePosition(const QMouseEvent *event);
    static QPoint wheelPosition(const QWheelEvent *event);

    void zoomBy(double multiplier);
    void updateCursorPosition(const QPoint &viewPosition);
    bool shouldStartPanning(const QMouseEvent *event) const;
    void beginPanning(const QPoint &viewPosition);
    void endPanning();

    int m_gridSpacing{20};
    bool m_gridVisible{true};
    bool m_snapEnabled{true};
    bool m_cursorInsideCanvas{false};
    bool m_spacePressed{false};
    bool m_panning{false};
    QPoint m_lastPanPosition;
    QPointF m_rawCursorPosition;
    QPointF m_snappedCursorPosition;
    double m_zoomFactor{1.0};
};
