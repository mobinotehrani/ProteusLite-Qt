#include "section09controller.h"

#include "canvasview.h"
#include "circuitmodel.h"
#include "componentitem.h"
#include "junctionitem.h"
#include "oscilloscopewidget.h"
#include "section05controller.h"
#include "wireitem.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QDockWidget>
#include <QEvent>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace
{
QPoint eventPosition(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

void setInstrumentText(ComponentItem *item, const QString &text)
{
    if (!item || item->runtimeText() == text)
        return;
    item->setComponentProperty(QStringLiteral("value"), text);
}

QString panelStyle()
{
    return QStringLiteral(
        "QWidget#MeasurementPanel{background:#f8fafc;}"
        "QLabel{color:#334155;}"
        "QLabel#MeasurementHeading{color:#0f172a;font-weight:700;font-size:11pt;}"
        "QLabel#MeasurementStatus{color:#64748b;}"
        "QPushButton{min-height:36px;border-radius:8px;padding:0 14px;font-weight:700;}"
        "QPushButton#VoltageProbeButton{background:#e2e8f0;color:#0f172a;border:1px solid #cbd5e1;}"
        "QPushButton#VoltageProbeButton:checked{background:#2563eb;color:white;border-color:#1d4ed8;}"
        "QPushButton#ScopeButton{background:#0f172a;color:white;border:1px solid #0f172a;}"
        "QPushButton#ScopeButton:hover{background:#1e293b;}"
        "QPushButton:disabled{background:#e2e8f0;color:#94a3b8;border-color:#cbd5e1;}"
    );
}
}

Section09Controller::Section09Controller(QMainWindow *window,
                                         Section05Controller *section05,
                                         Section06Controller *simulation,
                                         QObject *parent)
    : QObject(parent),
      m_window(window),
      m_section05(section05),
      m_simulation(simulation),
      m_canvas(section05 ? section05->canvasView() : nullptr)
{
    buildMeasurementPanel();
    buildOscilloscopePanel();
    connectSimulation();
    connectScene();
    resetMeasurementDisplays();
    refreshSummary();

    if (m_window && m_window->statusBar())
        m_window->statusBar()->showMessage(tr("Measurement tools are ready."), 3500);
}

bool Section09Controller::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_canvas || !m_probeEnabled || watched != m_canvas->viewport())
        return QObject::eventFilter(watched, event);

    if (event->type() == QEvent::MouseMove)
    {
        auto *mouse = static_cast<QMouseEvent *>(event);
        updateProbeAt(eventPosition(mouse));
    }
    else if (event->type() == QEvent::Leave)
    {
        QToolTip::hideText();
        if (m_probeStatus)
            m_probeStatus->setText(tr("Move over a wire, junction or pin to inspect its voltage."));
    }

    return QObject::eventFilter(watched, event);
}

void Section09Controller::buildMeasurementPanel()
{
    m_measurementDock = new QDockWidget(tr("Measurements"), m_window);
    m_measurementDock->setObjectName(QStringLiteral("Section09MeasurementDock"));
    m_measurementDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_measurementDock->setMinimumWidth(330);

    auto *panel = new QWidget(m_measurementDock);
    panel->setObjectName(QStringLiteral("MeasurementPanel"));
    panel->setStyleSheet(panelStyle());

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(9);

    auto *heading = new QLabel(tr("Live measurement tools"), panel);
    heading->setObjectName(QStringLiteral("MeasurementHeading"));
    layout->addWidget(heading);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(8);
    m_probeButton = new QPushButton(tr("Voltage Probe"), panel);
    m_probeButton->setObjectName(QStringLiteral("VoltageProbeButton"));
    m_probeButton->setCheckable(true);
    m_scopeButton = new QPushButton(tr("Oscilloscope"), panel);
    m_scopeButton->setObjectName(QStringLiteral("ScopeButton"));
    buttonRow->addWidget(m_probeButton, 1);
    buttonRow->addWidget(m_scopeButton, 1);
    layout->addLayout(buttonRow);

    m_probeStatus = new QLabel(panel);
    m_probeStatus->setObjectName(QStringLiteral("MeasurementStatus"));
    m_probeStatus->setWordWrap(true);
    layout->addWidget(m_probeStatus);

    m_instrumentStatus = new QLabel(panel);
    m_instrumentStatus->setObjectName(QStringLiteral("MeasurementStatus"));
    m_instrumentStatus->setWordWrap(true);
    layout->addWidget(m_instrumentStatus);
    layout->addStretch(1);

    connect(m_probeButton,
            &QPushButton::toggled,
            this,
            &Section09Controller::setProbeEnabled);
    connect(m_scopeButton,
            &QPushButton::clicked,
            this,
            &Section09Controller::showOscilloscope);

    m_measurementDock->setWidget(panel);
    m_window->addDockWidget(Qt::RightDockWidgetArea, m_measurementDock);
}

void Section09Controller::buildOscilloscopePanel()
{
    m_scopeDock = new QDockWidget(tr("Oscilloscope"), m_window);
    m_scopeDock->setObjectName(QStringLiteral("Section09OscilloscopeDock"));
    m_scopeDock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea |
                                 Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_scopeDock->setFeatures(QDockWidget::DockWidgetMovable |
                             QDockWidget::DockWidgetFloatable |
                             QDockWidget::DockWidgetClosable);
    m_scopeDock->setMinimumSize(680, 390);

    m_scopeWidget = new OscilloscopeWidget(m_scopeDock);
    m_scopeDock->setWidget(m_scopeWidget);
    m_window->addDockWidget(Qt::BottomDockWidgetArea, m_scopeDock);
    m_scopeDock->hide();
}

void Section09Controller::connectSimulation()
{
    if (!m_simulation)
        return;

    connect(m_simulation,
            &Section06Controller::simulationAdvanced,
            this,
            [this](double timeSeconds, quint64)
            {
                updateMeasurements();
                updateOscilloscope(timeSeconds);
                refreshSummary();
            });

    connect(m_simulation,
            &Section06Controller::simulationStateChanged,
            this,
            [this](Section06Controller::SimulationState state)
            {
                if (m_scopeWidget)
                    m_scopeWidget->setStateText(stateText(state));

                if (state == Section06Controller::SimulationState::Stopped)
                {
                    resetMeasurementDisplays();
                    closeOscilloscope();
                    QToolTip::hideText();
                }
                else
                {
                    updateMeasurements();
                    if (firstOscilloscope())
                        showOscilloscope();
                }
                refreshSummary();
            });
}

void Section09Controller::connectScene()
{
    if (!m_canvas || !m_canvas->viewport())
        return;

    m_canvas->viewport()->setMouseTracking(true);
    m_canvas->viewport()->installEventFilter(this);

    m_sceneRefreshTimer = new QTimer(this);
    m_sceneRefreshTimer->setSingleShot(true);
    m_sceneRefreshTimer->setInterval(100);
    connect(m_sceneRefreshTimer,
            &QTimer::timeout,
            this,
            [this]
            {
                updateMeasurements();
                refreshSummary();
            });

    if (m_canvas->scene())
    {
        connect(m_canvas->scene(),
                &QGraphicsScene::changed,
                this,
                [this](const QList<QRectF> &)
                {
                    if (m_sceneRefreshTimer)
                        m_sceneRefreshTimer->start();
                });
    }
}

void Section09Controller::setProbeEnabled(bool enabled)
{
    m_probeEnabled = enabled;
    QToolTip::hideText();

    if (!m_probeStatus)
        return;

    if (!enabled)
    {
        m_probeStatus->setText(tr("Voltage probe is off."));
        return;
    }

    if (!m_simulation ||
        m_simulation->simulationState() == Section06Controller::SimulationState::Stopped)
    {
        m_probeStatus->setText(tr("Start or pause a simulation before using the voltage probe."));
        return;
    }

    m_probeStatus->setText(tr("Move over a wire, junction or pin to inspect its voltage."));
}

void Section09Controller::updateMeasurements()
{
    if (!m_section05 || !m_simulation)
        return;

    const bool stopped = m_simulation->simulationState() ==
                         Section06Controller::SimulationState::Stopped;
    const bool groundAvailable = hasGroundReference();

    for (ComponentItem *item : m_section05->componentItems())
    {
        if (!item)
            continue;

        const QString type = item->componentType();
        if (type == QStringLiteral("VoltageProbe"))
        {
            QString text = QStringLiteral("--");
            if (!stopped)
                text = signalText(m_simulation->signalForComponentPin(item->modelId(), 0));
            setInstrumentText(item, text);
        }
        else if (type == QStringLiteral("Voltmeter"))
        {
            QString text = QStringLiteral("0.000 V");
            if (!stopped)
            {
                if (!groundAvailable)
                {
                    text = QStringLiteral("ERR");
                }
                else
                {
                    const auto positive = numericPinVoltage(item, 0);
                    const auto negative = numericPinVoltage(item, 1);
                    text = positive.has_value() && negative.has_value()
                               ? voltageText(*positive - *negative)
                               : QStringLiteral("FLOAT");
                }
            }
            setInstrumentText(item, text);
        }
        else if (type == QStringLiteral("Ammeter"))
        {
            QString text = QStringLiteral("0.000 A");
            if (!stopped)
            {
                const std::optional<double> current =
                    m_simulation->branchCurrentForComponent(item->modelId());
                text = current.has_value() ? currentText(current) : QStringLiteral("FLOAT");
            }
            setInstrumentText(item, text);
        }
        else if (type == QStringLiteral("Oscilloscope"))
        {
            if (stopped)
            {
                setInstrumentText(item, QStringLiteral("CH1, CH2"));
                continue;
            }

            const auto reference = numericPinVoltage(item, 2);
            const auto channel1 = numericPinVoltage(item, 0);
            const auto channel2 = numericPinVoltage(item, 1);
            if (!reference.has_value())
            {
                setInstrumentText(item, QStringLiteral("GND?"));
                continue;
            }

            const std::optional<double> first = channel1.has_value()
                                                    ? std::optional<double>(*channel1 - *reference)
                                                    : std::nullopt;
            const std::optional<double> second = channel2.has_value()
                                                     ? std::optional<double>(*channel2 - *reference)
                                                     : std::nullopt;
            setInstrumentText(
                item,
                QStringLiteral("CH1 %1 | CH2 %2")
                    .arg(voltageText(first), voltageText(second)));
        }
    }
}

void Section09Controller::resetMeasurementDisplays()
{
    if (!m_section05)
        return;

    for (ComponentItem *item : m_section05->componentItems())
    {
        if (!item)
            continue;

        const QString type = item->componentType();
        if (type == QStringLiteral("VoltageProbe"))
            setInstrumentText(item, QStringLiteral("--"));
        else if (type == QStringLiteral("Voltmeter"))
            setInstrumentText(item, QStringLiteral("0.000 V"));
        else if (type == QStringLiteral("Ammeter"))
            setInstrumentText(item, QStringLiteral("0.000 A"));
        else if (type == QStringLiteral("Oscilloscope"))
            setInstrumentText(item, QStringLiteral("CH1, CH2"));
    }

    if (m_scopeWidget)
    {
        m_scopeWidget->clearSamples();
        m_scopeWidget->setSourceText(tr("No active waveform"));
        m_scopeWidget->setStateText(QStringLiteral("STOPPED"));
    }
}

void Section09Controller::updateOscilloscope(double timeSeconds)
{
    if (!m_scopeWidget || !m_simulation ||
        m_simulation->simulationState() == Section06Controller::SimulationState::Stopped)
        return;

    ComponentItem *scope = firstOscilloscope();
    if (!scope)
        return;

    const auto reference = numericPinVoltage(scope, 2);
    const auto firstInput = numericPinVoltage(scope, 0);
    const auto secondInput = numericPinVoltage(scope, 1);

    std::optional<double> channel1;
    std::optional<double> channel2;
    if (reference.has_value())
    {
        if (firstInput.has_value())
            channel1 = *firstInput - *reference;
        if (secondInput.has_value())
            channel2 = *secondInput - *reference;
    }

    m_scopeWidget->setSourceText(
        tr("%1  |  CH1, CH2 referenced to scope GND").arg(scope->reference()));
    m_scopeWidget->setStateText(stateText(m_simulation->simulationState()));
    m_scopeWidget->appendSample(timeSeconds, channel1, channel2);

}

void Section09Controller::closeOscilloscope()
{
    if (!m_scopeDock || !m_scopeWidget)
        return;
    m_scopeWidget->clearSamples();
    m_scopeDock->close();
}

void Section09Controller::showOscilloscope()
{
    if (!m_scopeDock || !m_scopeWidget)
        return;

    ComponentItem *scope = firstOscilloscope();
    if (!scope)
    {
        if (m_window && m_window->statusBar())
            m_window->statusBar()->showMessage(
                tr("Place an Oscilloscope from Instruments before opening the waveform panel."),
                4500);
        return;
    }

    m_scopeWidget->setSourceText(tr("%1  |  wire CH1, CH2 and GND").arg(scope->reference()));
    m_scopeWidget->setStateText(stateText(m_simulation->simulationState()));
    m_scopeDock->show();
    m_scopeDock->raise();
}

void Section09Controller::refreshSummary()
{
    if (!m_section05 || !m_instrumentStatus || !m_scopeButton)
        return;

    int probes = 0;
    int voltmeters = 0;
    int ammeters = 0;
    int scopes = 0;
    for (ComponentItem *item : m_section05->componentItems())
    {
        if (!item)
            continue;
        const QString type = item->componentType();
        if (type == QStringLiteral("VoltageProbe"))
            ++probes;
        else if (type == QStringLiteral("Voltmeter"))
            ++voltmeters;
        else if (type == QStringLiteral("Ammeter"))
            ++ammeters;
        else if (type == QStringLiteral("Oscilloscope"))
            ++scopes;
    }

    m_instrumentStatus->setText(
        tr("Placed instruments: %1 probe, %2 voltmeter, %3 ammeter, %4 oscilloscope.")
            .arg(probes)
            .arg(voltmeters)
            .arg(ammeters)
            .arg(scopes));
    m_scopeButton->setEnabled(scopes > 0);

    if (m_probeStatus && !m_probeEnabled)
    {
        m_probeStatus->setText(tr("Voltage probe is off."));
    }
    else if (m_probeStatus &&
             m_simulation->simulationState() == Section06Controller::SimulationState::Stopped)
    {
        m_probeStatus->setText(tr("Start or pause a simulation before using the voltage probe."));
    }
    else if (m_probeStatus)
    {
        m_probeStatus->setText(tr("Move over a wire, junction or pin to inspect its voltage."));
    }
}

void Section09Controller::updateProbeAt(const QPoint &viewportPosition)
{
    if (!m_probeEnabled || !m_canvas || !m_simulation)
        return;

    const auto state = m_simulation->simulationState();
    if (state == Section06Controller::SimulationState::Stopped)
    {
        QToolTip::hideText();
        if (m_probeStatus)
            m_probeStatus->setText(tr("Start or pause a simulation before using the voltage probe."));
        return;
    }

    const QPointF scenePosition = m_canvas->mapToScene(viewportPosition);
    const ProbeHit hit = probeHitAt(scenePosition);
    if (!hit.isValid())
    {
        QToolTip::hideText();
        if (m_probeStatus)
            m_probeStatus->setText(tr("No measurable wire, junction or pin under the pointer."));
        return;
    }

    const auto signal = m_simulation->signalForEndpoint(hit.endpoint);
    const QString reading = signalText(signal);
    const QString text = QStringLiteral("%1\n%2").arg(hit.label, reading);
    QToolTip::showText(QCursor::pos(), text, m_canvas->viewport());
    if (m_probeStatus)
        m_probeStatus->setText(QStringLiteral("%1: %2").arg(hit.label, reading));
}

Section09Controller::ProbeHit
Section09Controller::probeHitAt(const QPointF &scenePosition) const
{
    if (!m_section05 || !m_canvas || !m_canvas->scene())
        return {};

    for (ComponentItem *item : m_section05->componentItems())
    {
        if (!item)
            continue;
        const int pinIndex = item->pinAtScenePosition(scenePosition, 13.0);
        if (pinIndex < 0)
            continue;
        const QVector<PinModel> pins = item->pins();
        const QString pinName = pinIndex < pins.size() ? pins.at(pinIndex).name
                                                       : QString::number(pinIndex + 1);
        return {CircuitGraph::pinEndpoint(item->modelId(), pinIndex),
                QStringLiteral("%1.%2").arg(item->reference(), pinName)};
    }

    const QList<QGraphicsItem *> sceneItems = m_canvas->scene()->items(scenePosition);
    for (QGraphicsItem *graphicsItem : sceneItems)
    {
        if (!graphicsItem)
            continue;
        if (graphicsItem->type() == JunctionItem::Type)
        {
            auto *junction = static_cast<JunctionItem *>(graphicsItem);
            return {junction->modelId(), tr("Junction %1").arg(junction->modelId())};
        }
        if (graphicsItem->type() == WireItem::Type)
        {
            auto *wire = static_cast<WireItem *>(graphicsItem);
            return {wire->startEndpoint(), tr("Wire %1").arg(wire->modelId())};
        }
    }

    for (WireItem *wire : m_section05->wireItems())
    {
        if (!wire)
            continue;
        if (wire->shape().contains(wire->mapFromScene(scenePosition)))
            return {wire->startEndpoint(), tr("Wire %1").arg(wire->modelId())};
    }

    return {};
}

ComponentItem *Section09Controller::firstOscilloscope() const
{
    if (!m_section05)
        return nullptr;

    QList<ComponentItem *> scopes;
    for (ComponentItem *item : m_section05->componentItems())
    {
        if (item && item->componentType() == QStringLiteral("Oscilloscope"))
            scopes.append(item);
    }
    if (scopes.isEmpty())
        return nullptr;

    std::sort(scopes.begin(),
              scopes.end(),
              [](ComponentItem *first, ComponentItem *second)
              {
                  return first->reference().localeAwareCompare(second->reference()) < 0;
              });
    return scopes.first();
}

bool Section09Controller::hasGroundReference() const
{
    if (!m_section05)
        return false;
    const QList<ComponentItem *> items = m_section05->componentItems();
    return std::any_of(items.cbegin(),
                       items.cend(),
                       [](ComponentItem *item)
                       {
                           return item && item->componentType() == QStringLiteral("Ground");
                       });
}

std::optional<double>
Section09Controller::numericPinVoltage(const ComponentItem *item, int pinIndex) const
{
    if (!item || !m_simulation || pinIndex < 0 || pinIndex >= item->pins().size())
        return std::nullopt;

    const auto signal = m_simulation->signalForComponentPin(item->modelId(), pinIndex);
    if (signal.conflict || signal.undefined || !signal.voltage.has_value() ||
        !std::isfinite(*signal.voltage))
        return std::nullopt;
    return signal.voltage;
}

QString Section09Controller::signalText(const Section06Controller::WireSignal &signal)
{
    if (signal.conflict)
        return QStringLiteral("CONFLICT");
    if (signal.undefined || !signal.voltage.has_value())
        return QStringLiteral("FLOAT / UNDEFINED");
    return voltageText(signal.voltage);
}

QString Section09Controller::voltageText(const std::optional<double> &voltage)
{
    if (!voltage.has_value() || !std::isfinite(*voltage))
        return QStringLiteral("--");
    return QStringLiteral("%1 V").arg(*voltage, 0, 'f', 3);
}

QString Section09Controller::currentText(const std::optional<double> &current)
{
    if (!current.has_value() || !std::isfinite(*current))
        return QStringLiteral("--");

    const double absolute = std::abs(*current);
    if (absolute < 0.001)
        return QStringLiteral("%1 uA").arg(*current * 1000000.0, 0, 'f', 2);
    if (absolute < 1.0)
        return QStringLiteral("%1 mA").arg(*current * 1000.0, 0, 'f', 3);
    return QStringLiteral("%1 A").arg(*current, 0, 'f', 4);
}

QString Section09Controller::stateText(Section06Controller::SimulationState state)
{
    switch (state)
    {
    case Section06Controller::SimulationState::Running:
        return QStringLiteral("RUNNING");
    case Section06Controller::SimulationState::Paused:
        return QStringLiteral("PAUSED");
    case Section06Controller::SimulationState::Stopped:
        return QStringLiteral("STOPPED");
    }
    return QStringLiteral("UNKNOWN");
}

namespace
{
void attachSection09Controller()
{
    bool attached = false;
    const auto windows = QApplication::topLevelWidgets();
    for (QWidget *widget : windows)
    {
        auto *window = qobject_cast<QMainWindow *>(widget);
        if (!window || window->property("section09ControllerAttached").toBool())
            continue;

        auto *section05 = window->findChild<Section05Controller *>();
        auto *section06 = window->findChild<Section06Controller *>();
        if (!section05 || !section06)
            continue;

        new Section09Controller(window, section05, section06, window);
        window->setProperty("section09ControllerAttached", true);
        attached = true;
    }

    if (!attached)
        QTimer::singleShot(120, [] { attachSection09Controller(); });
}

void installSection09Controller()
{
    QTimer::singleShot(0, [] { attachSection09Controller(); });
}
}

Q_COREAPP_STARTUP_FUNCTION(installSection09Controller)
