#pragma once

#include "section06controller.h"

#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <optional>

class CanvasView;
class ComponentItem;
class QLabel;
class QDockWidget;
class QEvent;
class QMainWindow;
class QPushButton;
class QTimer;
class Section05Controller;
class OscilloscopeWidget;

class Section09Controller final : public QObject
{
    Q_OBJECT

  public:
    Section09Controller(QMainWindow *window,
                        Section05Controller *section05,
                        Section06Controller *simulation,
                        QObject *parent = nullptr);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    struct ProbeHit
    {
        QString endpoint;
        QString label;

        bool isValid() const
        {
            return !endpoint.isEmpty();
        }
    };

    void buildMeasurementPanel();
    void buildOscilloscopePanel();
    void connectSimulation();
    void connectScene();
    void setProbeEnabled(bool enabled);
    void updateMeasurements();
    void resetMeasurementDisplays();
    void updateOscilloscope(double timeSeconds);
    void closeOscilloscope();
    void showOscilloscope();
    void refreshSummary();
    void updateProbeAt(const QPoint &viewportPosition);
    ProbeHit probeHitAt(const QPointF &scenePosition) const;
    ComponentItem *firstOscilloscope() const;
    bool hasGroundReference() const;
    std::optional<double> numericPinVoltage(const ComponentItem *item, int pinIndex) const;
    static QString signalText(const Section06Controller::WireSignal &signal);
    static QString voltageText(const std::optional<double> &voltage);
    static QString currentText(const std::optional<double> &current);
    static QString stateText(Section06Controller::SimulationState state);

    QMainWindow *m_window{};
    Section05Controller *m_section05{};
    Section06Controller *m_simulation{};
    CanvasView *m_canvas{};
    QDockWidget *m_measurementDock{};
    QDockWidget *m_scopeDock{};
    QPushButton *m_probeButton{};
    QPushButton *m_scopeButton{};
    QLabel *m_probeStatus{};
    QLabel *m_instrumentStatus{};
    OscilloscopeWidget *m_scopeWidget{};
    QTimer *m_sceneRefreshTimer{};
    bool m_probeEnabled{false};
};
