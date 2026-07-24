#include "section08controller.h"

#include "basiccomponent.h"
#include "canvasview.h"
#include "componentitem.h"
#include "section05controller.h"
#include "wireitem.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDockWidget>
#include <QFrame>
#include <QGraphicsScene>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace
{
QDockWidget *findDock(QMainWindow *window,
                      const QString &objectName,
                      const QString &oldTitle)
{
    if (!window)
        return nullptr;

    if (QDockWidget *dock = window->findChild<QDockWidget *>(objectName))
        return dock;

    const QList<QDockWidget *> docks = window->findChildren<QDockWidget *>();
    for (QDockWidget *dock : docks)
    {
        if (dock && dock->windowTitle() == oldTitle)
            return dock;
    }
    return nullptr;
}

QString monitorStyle()
{
    return QStringLiteral(
        "QPlainTextEdit{"
        "background:#0b1220;"
        "color:#dbeafe;"
        "border:1px solid #1e293b;"
        "border-radius:8px;"
        "padding:10px;"
        "selection-background-color:#2563eb;"
        "font-family:'Cascadia Mono','Consolas',monospace;"
        "font-size:10pt;"
        "}"
        "QScrollBar:vertical{background:#0f172a;width:10px;margin:0;}"
        "QScrollBar::handle:vertical{background:#475569;border-radius:5px;min-height:28px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
        "QScrollBar:horizontal{background:#0f172a;height:10px;margin:0;}"
        "QScrollBar::handle:horizontal{background:#475569;border-radius:5px;min-width:28px;}"
        "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}");
}

QString dockStyle()
{
    return QStringLiteral(
        "QDockWidget{color:#0f172a;font-weight:700;}"
        "QDockWidget::title{"
        "background:#e8eef7;"
        "border:1px solid #cbd5e1;"
        "padding:8px 10px;"
        "text-align:left;"
        "}"
        "QDockWidget::close-button,QDockWidget::float-button{"
        "background:transparent;border:0;padding:2px;"
        "}"
        "QDockWidget::close-button:hover,QDockWidget::float-button:hover{"
        "background:#cbd5e1;border-radius:4px;"
        "}");
}
}

Section08Controller::Section08Controller(QMainWindow *window,
                                         Section05Controller *section05,
                                         Section06Controller *simulation,
                                         QObject *parent)
    : QObject(parent), m_window(window), m_section05(section05), m_simulation(simulation)
{
    buildPanel();
    buildShortcuts();
    configureMonitors();
    connectSceneRefresh();

    connect(m_simulation,
            &Section06Controller::simulationStateChanged,
            this,
            [this](Section06Controller::SimulationState state)
            {
                updateControls(state);
                if (state == Section06Controller::SimulationState::Stopped)
                {
                    resetWireColors();
                    scheduleSceneRefresh();
                }
                else
                {
                    refreshWireColors();
                }
            });

    connect(m_simulation,
            &Section06Controller::simulationAdvanced,
            this,
            [this](double timeSeconds, quint64 stepCount)
            {
                updateCounters(timeSeconds, stepCount);
                updateControls(m_simulation->simulationState());
                refreshWireColors();
            });

    updateControls(m_simulation->simulationState());
    updateCounters(m_simulation->simulationTime(), m_simulation->stepCount());
    resetWireColors();
    refreshStoppedCircuitMonitor();

    QTimer::singleShot(250, this, [this] { configureMonitors(); });
    QTimer::singleShot(750, this, [this] { configureMonitors(); });

    showStatus(tr("Simulation controls are ready. Use Run or Step to begin."));
}

void Section08Controller::buildPanel()
{
    m_controlDock = new QDockWidget(tr("Simulation Console"), m_window);
    m_controlDock->setObjectName(QStringLiteral("Section08SimulationControlDock"));
    m_controlDock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    m_controlDock->setFeatures(QDockWidget::DockWidgetMovable |
                               QDockWidget::DockWidgetFloatable |
                               QDockWidget::DockWidgetClosable);
    m_controlDock->setMinimumHeight(184);
    m_controlDock->setMaximumHeight(236);
    m_controlDock->setStyleSheet(dockStyle());

    auto *panel = new QWidget(m_controlDock);
    panel->setObjectName(QStringLiteral("SimulationConsolePanel"));
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    panel->setStyleSheet(QStringLiteral(
        "QWidget#SimulationConsolePanel{background:#f8fafc;}"
        "QLabel{color:#0f172a;}"
        "QPushButton{"
        "min-height:38px;"
        "border:0;"
        "border-radius:8px;"
        "padding:0 18px;"
        "color:white;"
        "font-weight:700;"
        "font-size:10pt;"
        "}"
        "QPushButton:disabled{background:#dbe3ee;color:#64748b;}"
        "QPushButton#RunSimulationButton{background:#16a34a;}"
        "QPushButton#RunSimulationButton:hover{background:#15803d;}"
        "QPushButton#PauseSimulationButton{background:#d97706;}"
        "QPushButton#PauseSimulationButton:hover{background:#b45309;}"
        "QPushButton#StopSimulationButton{background:#dc2626;}"
        "QPushButton#StopSimulationButton:hover{background:#b91c1c;}"
        "QPushButton#StepSimulationButton{background:#2563eb;}"
        "QPushButton#StepSimulationButton:hover{background:#1d4ed8;}"
        "QPushButton#RunSimulationButton:disabled,"
        "QPushButton#PauseSimulationButton:disabled,"
        "QPushButton#StopSimulationButton:disabled,"
        "QPushButton#StepSimulationButton:disabled{"
        "background:#dbe3ee;color:#64748b;"
        "}"
        "QFrame#SimulationSummary{"
        "background:white;"
        "border:1px solid #d7e0ec;"
        "border-radius:10px;"
        "}"
        "QLabel#MetricCaption{color:#64748b;font-size:9pt;}"
        "QLabel#MetricValue{color:#0f172a;font-weight:700;font-size:10pt;}"
        "QLabel#InventoryLabel{color:#475569;font-weight:600;}"
        "QLabel#SimulationHint{color:#64748b;}"));

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(8);

    m_runButton = new QPushButton(tr("Run  F5"), panel);
    m_pauseButton = new QPushButton(tr("Pause  F6"), panel);
    m_stopButton = new QPushButton(tr("Stop  Shift+F5"), panel);
    m_stepButton = new QPushButton(tr("Step  F10"), panel);
    m_runButton->setObjectName(QStringLiteral("RunSimulationButton"));
    m_pauseButton->setObjectName(QStringLiteral("PauseSimulationButton"));
    m_stopButton->setObjectName(QStringLiteral("StopSimulationButton"));
    m_stepButton->setObjectName(QStringLiteral("StepSimulationButton"));

    topRow->addWidget(m_runButton, 1);
    topRow->addWidget(m_pauseButton, 1);
    topRow->addWidget(m_stopButton, 1);
    topRow->addWidget(m_stepButton, 1);
    layout->addLayout(topRow);

    auto *summary = new QFrame(panel);
    summary->setObjectName(QStringLiteral("SimulationSummary"));
    auto *summaryLayout = new QGridLayout(summary);
    summaryLayout->setContentsMargins(12, 7, 12, 7);
    summaryLayout->setHorizontalSpacing(10);
    summaryLayout->setVerticalSpacing(2);

    auto makeCaption = [summary](const QString &text)
    {
        auto *label = new QLabel(text, summary);
        label->setObjectName(QStringLiteral("MetricCaption"));
        return label;
    };

    m_stateLabel = new QLabel(summary);
    m_timeLabel = new QLabel(summary);
    m_stepLabel = new QLabel(summary);
    m_inventoryLabel = new QLabel(summary);
    m_timeLabel->setObjectName(QStringLiteral("MetricValue"));
    m_stepLabel->setObjectName(QStringLiteral("MetricValue"));
    m_inventoryLabel->setObjectName(QStringLiteral("InventoryLabel"));

    summaryLayout->addWidget(makeCaption(tr("State")), 0, 0);
    summaryLayout->addWidget(m_stateLabel, 1, 0);
    summaryLayout->addWidget(makeCaption(tr("Simulation time")), 0, 1);
    summaryLayout->addWidget(m_timeLabel, 1, 1);
    summaryLayout->addWidget(makeCaption(tr("Steps")), 0, 2);
    summaryLayout->addWidget(m_stepLabel, 1, 2);
    summaryLayout->addWidget(makeCaption(tr("Circuit")), 0, 3);
    summaryLayout->addWidget(m_inventoryLabel, 1, 3);
    summaryLayout->setColumnStretch(3, 1);
    layout->addWidget(summary);

    m_hintLabel = new QLabel(panel);
    m_hintLabel->setObjectName(QStringLiteral("SimulationHint"));
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    connect(m_runButton,
            &QPushButton::clicked,
            m_simulation,
            &Section06Controller::runSimulation);
    connect(m_pauseButton,
            &QPushButton::clicked,
            m_simulation,
            &Section06Controller::pauseSimulation);
    connect(m_stopButton,
            &QPushButton::clicked,
            m_simulation,
            &Section06Controller::stopSimulation);
    connect(m_stepButton,
            &QPushButton::clicked,
            m_simulation,
            &Section06Controller::stepSimulation);

    m_controlDock->setWidget(panel);
    m_window->addDockWidget(Qt::BottomDockWidgetArea, m_controlDock);
    m_window->resizeDocks({m_controlDock}, {205}, Qt::Vertical);
}

void Section08Controller::buildShortcuts()
{
    m_runAction = new QAction(tr("Run simulation"), this);
    m_runAction->setShortcut(QKeySequence(QStringLiteral("F5")));
    m_runAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_runAction, &QAction::triggered, m_simulation, &Section06Controller::runSimulation);
    m_window->addAction(m_runAction);

    m_pauseAction = new QAction(tr("Pause simulation"), this);
    m_pauseAction->setShortcut(QKeySequence(QStringLiteral("F6")));
    m_pauseAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_pauseAction,
            &QAction::triggered,
            m_simulation,
            &Section06Controller::pauseSimulation);
    m_window->addAction(m_pauseAction);

    m_stopAction = new QAction(tr("Stop simulation"), this);
    m_stopAction->setShortcut(QKeySequence(QStringLiteral("Shift+F5")));
    m_stopAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_stopAction, &QAction::triggered, m_simulation, &Section06Controller::stopSimulation);
    m_window->addAction(m_stopAction);

    m_stepAction = new QAction(tr("Single simulation step"), this);
    m_stepAction->setShortcut(QKeySequence(QStringLiteral("F10")));
    m_stepAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_stepAction, &QAction::triggered, m_simulation, &Section06Controller::stepSimulation);
    m_window->addAction(m_stepAction);
}

void Section08Controller::configureMonitors()
{
    if (!m_window)
        return;

    m_window->setDockNestingEnabled(true);
    m_window->setDockOptions(m_window->dockOptions() |
                             QMainWindow::AllowNestedDocks |
                             QMainWindow::AllowTabbedDocks |
                             QMainWindow::AnimatedDocks);
    m_window->setTabPosition(Qt::RightDockWidgetArea, QTabWidget::North);

    m_circuitDock = findDock(m_window,
                             QStringLiteral("Section06MonitorDock"),
                             QStringLiteral("Section 6 Monitor"));
    m_deviceDock = findDock(m_window,
                            QStringLiteral("Section07MonitorDock"),
                            QStringLiteral("Section 7 Monitor"));

    if (m_circuitDock)
    {
        m_circuitDock->setWindowTitle(tr("Circuit Monitor"));
        m_circuitDock->setMinimumWidth(360);
        m_circuitDock->setStyleSheet(dockStyle());
        m_circuitMonitor = qobject_cast<QPlainTextEdit *>(m_circuitDock->widget());
        if (m_circuitMonitor)
        {
            m_circuitMonitor->setLineWrapMode(QPlainTextEdit::NoWrap);
            m_circuitMonitor->setPlaceholderText(
                tr("Place components on the canvas to inspect the circuit."));
            m_circuitMonitor->setStyleSheet(monitorStyle());
        }
    }

    if (m_deviceDock)
    {
        m_deviceDock->setWindowTitle(tr("Advanced Devices"));
        m_deviceDock->setMinimumWidth(360);
        m_deviceDock->setStyleSheet(dockStyle());
        if (auto *monitor = qobject_cast<QPlainTextEdit *>(m_deviceDock->widget()))
        {
            monitor->setLineWrapMode(QPlainTextEdit::NoWrap);
            monitor->setPlaceholderText(
                tr("ADC, DAC, MCU, memory, LCD and keypad activity appears here."));
            monitor->setStyleSheet(monitorStyle());
        }
    }

    if (m_circuitDock && m_deviceDock &&
        !m_deviceDock->property("professionalMonitorTabbed").toBool())
    {
        m_window->tabifyDockWidget(m_circuitDock, m_deviceDock);
        m_circuitDock->raise();
        m_deviceDock->setProperty("professionalMonitorTabbed", true);
    }

    refreshStoppedCircuitMonitor();
}

void Section08Controller::connectSceneRefresh()
{
    m_sceneRefreshTimer = new QTimer(this);
    m_sceneRefreshTimer->setSingleShot(true);
    m_sceneRefreshTimer->setInterval(90);
    connect(m_sceneRefreshTimer,
            &QTimer::timeout,
            this,
            [this]
            {
                updateControls(m_simulation->simulationState());
                if (m_simulation->simulationState() ==
                    Section06Controller::SimulationState::Stopped)
                {
                    refreshStoppedCircuitMonitor();
                    resetWireColors();
                }
            });

    CanvasView *canvas = m_section05 ? m_section05->canvasView() : nullptr;
    if (!canvas || !canvas->scene())
        return;

    connect(canvas->scene(),
            &QGraphicsScene::changed,
            this,
            [this](const QList<QRectF> &)
            {
                if (m_simulation->simulationState() ==
                    Section06Controller::SimulationState::Stopped)
                {
                    scheduleSceneRefresh();
                }
            });
}

void Section08Controller::scheduleSceneRefresh()
{
    if (m_sceneRefreshTimer)
        m_sceneRefreshTimer->start();
}

void Section08Controller::refreshStoppedCircuitMonitor()
{
    if (!m_circuitMonitor || !m_section05 || !m_simulation)
        return;
    if (m_simulation->simulationState() != Section06Controller::SimulationState::Stopped)
        return;

    QList<ComponentItem *> components = m_section05->componentItems();
    components.erase(std::remove(components.begin(), components.end(), nullptr), components.end());
    std::sort(components.begin(),
              components.end(),
              [](ComponentItem *first, ComponentItem *second)
              {
                  return first->reference().localeAwareCompare(second->reference()) < 0;
              });

    QStringList lines;
    lines.append(tr("CIRCUIT READY  |  STOPPED"));
    lines.append(tr("Components: %1  |  Wires: %2")
                     .arg(components.size())
                     .arg(m_section05->wireItems().size()));
    lines.append(QString(46, QLatin1Char('-')));

    if (components.isEmpty())
    {
        lines.append(tr("No circuit component is placed yet."));
        lines.append(QString());
        lines.append(tr("Choose a component from the library and place it on the canvas."));
    }
    else
    {
        for (ComponentItem *item : components)
        {
            lines.append(QStringLiteral("%1  [%2]  %3")
                             .arg(item->reference(),
                                  item->componentType(),
                                  shortened(item->runtimeText())));
        }

        lines.append(QString());
        if (!hasGroundReference())
            lines.append(tr("Warning: add at least one GND before running."));
        else
            lines.append(tr("Ready. Press Run for continuous execution or Step for one cycle."));
    }

    const QString text = lines.join(QLatin1Char('\n'));
    if (m_circuitMonitor->toPlainText() != text)
        m_circuitMonitor->setPlainText(text);
}

void Section08Controller::updateControls(Section06Controller::SimulationState state)
{
    const bool running = state == Section06Controller::SimulationState::Running;
    const bool stopped = state == Section06Controller::SimulationState::Stopped;
    const bool hasComponents = hasCircuitComponents();

    m_runButton->setEnabled(!running && hasComponents);
    m_pauseButton->setEnabled(running);
    m_stopButton->setEnabled(!stopped);
    m_stepButton->setEnabled(!running && hasComponents);

    if (m_runAction)
        m_runAction->setEnabled(!running && hasComponents);
    if (m_pauseAction)
        m_pauseAction->setEnabled(running);
    if (m_stopAction)
        m_stopAction->setEnabled(!stopped);
    if (m_stepAction)
        m_stepAction->setEnabled(!running && hasComponents);

    const int componentCount = m_section05 ? m_section05->componentItems().size() : 0;
    const int wireCount = m_section05 ? m_section05->wireItems().size() : 0;
    m_inventoryLabel->setText(tr("%1 components, %2 wires")
                                  .arg(componentCount)
                                  .arg(wireCount));

    if (running)
    {
        m_stateLabel->setText(tr("RUNNING"));
        m_stateLabel->setStyleSheet(
            QStringLiteral("background:#dcfce7;color:#166534;border:1px solid #86efac;"
                           "border-radius:7px;padding:4px 10px;font-weight:800;"));
        m_hintLabel->setText(
            tr("The circuit is running. Switches, push buttons, keypad keys and the potentiometer remain interactive."));
    }
    else if (stopped)
    {
        m_stateLabel->setText(tr("STOPPED"));
        m_stateLabel->setStyleSheet(
            QStringLiteral("background:#e2e8f0;color:#334155;border:1px solid #cbd5e1;"
                           "border-radius:7px;padding:4px 10px;font-weight:800;"));
        if (!hasComponents)
            m_hintLabel->setText(tr("Place at least one component to enable Run and Step."));
        else if (!hasGroundReference())
            m_hintLabel->setText(tr("The circuit is ready, but a GND reference is still required for a valid simulation."));
        else
            m_hintLabel->setText(
                tr("Ready. Red wires are HIGH, blue wires are LOW, orange wires are analog, and gray dashed wires are floating."));
    }
    else
    {
        m_stateLabel->setText(tr("PAUSED"));
        m_stateLabel->setStyleSheet(
            QStringLiteral("background:#fef3c7;color:#92400e;border:1px solid #fcd34d;"
                           "border-radius:7px;padding:4px 10px;font-weight:800;"));
        m_hintLabel->setText(
            tr("Simulation time is frozen. Use Step for a controlled cycle or Run to continue."));
    }
}

void Section08Controller::updateCounters(double timeSeconds, quint64 stepCount)
{
    m_timeLabel->setText(QStringLiteral("%1 s").arg(timeSeconds, 0, 'f', 6));
    m_stepLabel->setText(QString::number(stepCount));
}

void Section08Controller::refreshWireColors()
{
    if (!m_section05 || !m_simulation)
        return;
    if (m_simulation->simulationState() == Section06Controller::SimulationState::Stopped)
    {
        resetWireColors();
        return;
    }

    for (WireItem *wire : m_section05->wireItems())
    {
        if (!wire)
            continue;

        const Section06Controller::WireSignal signal =
            m_simulation->signalForEndpoint(wire->startEndpoint());

        if (signal.conflict)
        {
            wire->setSignalState(WireItem::SignalState::Conflict, signal.voltage);
            continue;
        }
        if (signal.undefined || !signal.voltage.has_value())
        {
            wire->setSignalState(WireItem::SignalState::Undefined);
            continue;
        }

        const LogicLevel logic = LogicThresholds::fromVoltage(signal.voltage);
        if (logic == LogicLevel::High)
            wire->setSignalState(WireItem::SignalState::High, signal.voltage);
        else if (logic == LogicLevel::Low)
            wire->setSignalState(WireItem::SignalState::Low, signal.voltage);
        else
            wire->setSignalState(WireItem::SignalState::Analog, signal.voltage);
    }
}

void Section08Controller::resetWireColors()
{
    if (!m_section05)
        return;
    for (WireItem *wire : m_section05->wireItems())
    {
        if (wire)
            wire->setSignalState(WireItem::SignalState::Idle);
    }
}

void Section08Controller::showStatus(const QString &message) const
{
    if (m_window && m_window->statusBar())
        m_window->statusBar()->showMessage(message, 4500);
}

bool Section08Controller::hasCircuitComponents() const
{
    return m_section05 && !m_section05->componentItems().isEmpty();
}

bool Section08Controller::hasGroundReference() const
{
    if (!m_section05)
        return false;

    for (ComponentItem *item : m_section05->componentItems())
    {
        if (!item)
            continue;
        const QString type = item->componentType();
        if (type == QStringLiteral("Ground") || type == QStringLiteral("GND"))
            return true;
    }
    return false;
}

QString Section08Controller::shortened(const QString &text, int maximumLength)
{
    const QString compact = text.simplified();
    if (compact.size() <= maximumLength)
        return compact;
    return compact.left(std::max(0, maximumLength - 1)) + QChar(0x2026);
}

namespace
{
void attachSection08Controller()
{
    bool attached = false;
    const auto windows = QApplication::topLevelWidgets();
    for (QWidget *widget : windows)
    {
        auto *window = qobject_cast<QMainWindow *>(widget);
        if (!window || window->property("section08ControllerAttached").toBool())
            continue;

        auto *section05 = window->findChild<Section05Controller *>();
        auto *section06 = window->findChild<Section06Controller *>();
        if (!section05 || !section06)
            continue;

        new Section08Controller(window, section05, section06, window);
        window->setProperty("section08ControllerAttached", true);
        attached = true;
    }

    if (!attached)
        QTimer::singleShot(100, [] { attachSection08Controller(); });
}

void installSection08Controller()
{
    QTimer::singleShot(0, [] { attachSection08Controller(); });
}
}

Q_COREAPP_STARTUP_FUNCTION(installSection08Controller)
