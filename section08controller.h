#pragma once

#include "section06controller.h"

#include <QObject>
#include <QString>

class QAction;
class QLabel;
class QDockWidget;
class QMainWindow;
class QPlainTextEdit;
class QPushButton;
class QTimer;
class Section05Controller;

class Section08Controller final : public QObject
{
    Q_OBJECT

  public:
    Section08Controller(QMainWindow *window,
                        Section05Controller *section05,
                        Section06Controller *simulation,
                        QObject *parent = nullptr);

  private:
    void buildPanel();
    void buildShortcuts();
    void configureMonitors();
    void connectSceneRefresh();
    void scheduleSceneRefresh();
    void refreshStoppedCircuitMonitor();
    void updateControls(Section06Controller::SimulationState state);
    void updateCounters(double timeSeconds, quint64 stepCount);
    void refreshWireColors();
    void resetWireColors();
    void showStatus(const QString &message) const;
    bool hasCircuitComponents() const;
    bool hasGroundReference() const;
    static QString shortened(const QString &text, int maximumLength = 92);

    QMainWindow *m_window{};
    Section05Controller *m_section05{};
    Section06Controller *m_simulation{};
    QDockWidget *m_controlDock{};
    QDockWidget *m_circuitDock{};
    QDockWidget *m_deviceDock{};
    QPlainTextEdit *m_circuitMonitor{};
    QPushButton *m_runButton{};
    QPushButton *m_pauseButton{};
    QPushButton *m_stopButton{};
    QPushButton *m_stepButton{};
    QLabel *m_stateLabel{};
    QLabel *m_timeLabel{};
    QLabel *m_stepLabel{};
    QLabel *m_inventoryLabel{};
    QLabel *m_hintLabel{};
    QAction *m_runAction{};
    QAction *m_pauseAction{};
    QAction *m_stopAction{};
    QAction *m_stepAction{};
    QTimer *m_sceneRefreshTimer{};
};
