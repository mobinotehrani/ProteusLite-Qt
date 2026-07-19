#include "section06controller.h"

#include "componentitem.h"
#include "section05controller.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QSet>
#include <QStatusBar>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace
{
class DisjointSet final
{
  public:
    void add(const QString &value)
    {
        if (!value.isEmpty() && !m_parent.contains(value))
            m_parent.insert(value, value);
    }

    QString find(const QString &value)
    {
        add(value);
        QString root = value;
        while (m_parent.value(root) != root)
            root = m_parent.value(root);

        QString cursor = value;
        while (m_parent.value(cursor) != cursor)
        {
            const QString next = m_parent.value(cursor);
            m_parent[cursor] = root;
            cursor = next;
        }
        return root;
    }

    void unite(const QString &first, const QString &second)
    {
        if (first.isEmpty() || second.isEmpty())
            return;
        const QString firstRoot = find(first);
        const QString secondRoot = find(second);
        if (firstRoot != secondRoot)
            m_parent[secondRoot] = firstRoot;
    }

  private:
    QHash<QString, QString> m_parent;
};

struct NetAccumulator
{
    bool driven{false};
    bool hasNumericVoltage{false};
    bool unknown{false};
    bool conflict{false};
    double voltage{0.0};
};

bool optionalEqual(const std::optional<double> &first, const std::optional<double> &second)
{
    if (first.has_value() != second.has_value())
        return false;
    if (!first.has_value())
        return true;
    return std::abs(*first - *second) <= 0.000001;
}

QString shortText(const QString &text, int maximum = 80)
{
    return text.size() <= maximum ? text : text.left(maximum - 1) + QChar(0x2026);
}
}

Section06Controller::Section06Controller(QMainWindow *window, Section05Controller *section05, QObject *parent)
    : QObject(parent), m_window(window), m_section05(section05)
{
    m_monitorDock = new QDockWidget(tr("Section 6 Monitor"), m_window);
    m_monitorDock->setObjectName(QStringLiteral("Section06MonitorDock"));
    m_monitorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_monitorDock->setMinimumWidth(330);

    m_monitor = new QPlainTextEdit(m_monitorDock);
    m_monitor->setReadOnly(true);
    m_monitor->setPlaceholderText(tr("Place basic components to view their live state."));
    m_monitor->setStyleSheet(
        QStringLiteral("QPlainTextEdit{background:#0f172a;color:#dbeafe;border:0;padding:8px;"
                       "font-family:Consolas,monospace;font-size:10pt;}"));
    m_monitorDock->setWidget(m_monitor);
    m_window->addDockWidget(Qt::RightDockWidgetArea, m_monitorDock);

    m_clock.start();
    m_timer = new QTimer(this);
    m_timer->setInterval(20);
    connect(m_timer, &QTimer::timeout, this, &Section06Controller::runStep);
    m_timer->start();

    if (m_window && m_window->statusBar())
    {
        m_window->statusBar()->showMessage(
            tr("Section 6 ready: sources, passive components, interactive parts, "
               "displays, gates and D flip-flop are active."),
            6500);
    }
}

void Section06Controller::synchronizeComponents()
{
    QHash<QString, ComponentItem *> current;
    if (m_section05)
    {
        for (ComponentItem *item : m_section05->componentItems())
        {
            if (item)
                current.insert(item->modelId(), item);
        }
    }

    const QStringList knownIds = m_components.keys();
    for (const QString &id : knownIds)
    {
        if (!current.contains(id))
            m_lastResults.remove(id);
    }
    m_components = current;
}

void Section06Controller::runStep()
{
    synchronizeComponents();

    const qint64 nowMilliseconds = m_clock.elapsed();
    const double nowSeconds = static_cast<double>(nowMilliseconds) / 1000.0;
    double deltaSeconds = m_lastStepMilliseconds == 0
                              ? 0.02
                              : static_cast<double>(nowMilliseconds - m_lastStepMilliseconds) / 1000.0;
    m_lastStepMilliseconds = nowMilliseconds;
    deltaSeconds = std::clamp(deltaSeconds, 0.000001, 0.25);

    QStringList warnings;
    if (!m_components.isEmpty() && !hasGround())
    {
        appendUnique(warnings, QStringLiteral("At least one GND is required before simulation."));
        NetworkSnapshot snapshot = resolveNetwork({});
        updateMonitor(snapshot, warnings);
        return;
    }

    QHash<QString, ComponentStepResult> results = m_lastResults;
    NetworkSnapshot snapshot;
    for (int pass = 0; pass < 16; ++pass)
    {
        snapshot = resolveNetwork(results);
        for (const QString &warning : snapshot.warnings)
            appendUnique(warnings, warning);

        QHash<QString, ComponentStepResult> next = evaluateComponents(snapshot, nowSeconds, warnings);
        if (resultsEqual(results, next))
        {
            results = next;
            break;
        }
        results = next;
    }

    m_lastResults = results;
    snapshot = resolveNetwork(m_lastResults);
    for (const QString &warning : snapshot.warnings)
        appendUnique(warnings, warning);

    appendReferenceWarnings(snapshot, warnings);
    evaluateElectricalModels(snapshot, deltaSeconds, warnings);
    updateMonitor(snapshot, warnings);
}

Section06Controller::NetworkSnapshot
Section06Controller::resolveNetwork(const QHash<QString, ComponentStepResult> &results) const
{
    NetworkSnapshot snapshot;
    if (!m_section05)
        return snapshot;

    DisjointSet groups;
    for (ComponentItem *item : m_components)
    {
        if (!item)
            continue;
        const QVector<PinModel> pins = item->pins();
        for (int pinIndex = 0; pinIndex < pins.size(); ++pinIndex)
        {
            const QString endpoint = endpointFor(item, pinIndex);
            groups.add(baseNodeForEndpoint(endpoint));
        }
    }

    for (ComponentItem *item : m_components)
    {
        if (!item || !item->componentModel())
            continue;
        const QVector<QPair<int, int>> pairs = item->componentModel()->conductivePinPairs();
        for (const QPair<int, int> &pair : pairs)
        {
            const QString first = baseNodeForEndpoint(endpointFor(item, pair.first));
            const QString second = baseNodeForEndpoint(endpointFor(item, pair.second));
            groups.unite(first, second);
        }
    }

    for (ComponentItem *item : m_components)
    {
        if (!item)
            continue;
        const QVector<PinModel> pins = item->pins();
        for (int pinIndex = 0; pinIndex < pins.size(); ++pinIndex)
        {
            const QString endpoint = endpointFor(item, pinIndex);
            snapshot.endpointRoots.insert(endpoint, groups.find(baseNodeForEndpoint(endpoint)));
        }
    }

    QHash<QString, NetAccumulator> accumulators;
    for (ComponentItem *item : m_components)
    {
        if (!item)
            continue;
        const ComponentStepResult result = results.value(item->modelId());
        const QVector<PinModel> pins = item->pins();
        const int count = std::min(pins.size(), result.drivenPins.size());
        for (int pinIndex = 0; pinIndex < count; ++pinIndex)
        {
            if (!result.drivenPins.value(pinIndex))
                continue;

            const QString endpoint = endpointFor(item, pinIndex);
            const QString root = snapshot.endpointRoots.value(endpoint, baseNodeForEndpoint(endpoint));
            NetAccumulator &accumulator = accumulators[root];
            const std::optional<double> voltage = result.pinVoltages.value(pinIndex);
            accumulator.driven = true;
            if (!voltage.has_value() || !std::isfinite(*voltage))
            {
                accumulator.unknown = true;
                continue;
            }

            if (!accumulator.hasNumericVoltage)
            {
                accumulator.hasNumericVoltage = true;
                accumulator.voltage = *voltage;
                continue;
            }

            if (std::abs(accumulator.voltage - *voltage) > 0.000001)
                accumulator.conflict = true;
        }
    }

    QSet<QString> roots;
    for (auto root = snapshot.endpointRoots.cbegin(); root != snapshot.endpointRoots.cend(); ++root)
        roots.insert(root.value());

    for (const QString &root : roots)
    {
        const NetAccumulator accumulator = accumulators.value(root);
        NetState state;
        state.driven = accumulator.driven;
        state.conflict = accumulator.conflict;
        state.undefined = accumulator.unknown || accumulator.conflict;
        if (state.driven && !state.conflict && !state.undefined)
            state.voltage = accumulator.voltage;
        snapshot.nets.insert(root, state);
        if (state.conflict)
            appendUnique(snapshot.warnings, QStringLiteral("Conflicting drivers detected."));
    }

    for (ComponentItem *item : m_components)
    {
        if (!item)
            continue;
        QVector<std::optional<double>> inputs;
        const QVector<PinModel> pins = item->pins();
        inputs.resize(pins.size());
        for (int pinIndex = 0; pinIndex < pins.size(); ++pinIndex)
        {
            const QString endpoint = endpointFor(item, pinIndex);
            const QString root = snapshot.endpointRoots.value(endpoint, baseNodeForEndpoint(endpoint));
            inputs[pinIndex] = snapshot.nets.value(root).voltage;
        }
        snapshot.componentInputs.insert(item->modelId(), inputs);
    }

    return snapshot;
}

QHash<QString, ComponentStepResult> Section06Controller::evaluateComponents(const NetworkSnapshot &snapshot,
                                                                            double nowSeconds,
                                                                            QStringList &warnings)
{
    QHash<QString, ComponentStepResult> results;
    for (ComponentItem *item : m_components)
    {
        if (!item || !item->componentModel())
            continue;

        const QVector<std::optional<double>> inputs = snapshot.componentInputs.value(item->modelId());
        const ComponentStepResult result = item->updateSimulation(inputs, nowSeconds);
        results.insert(item->modelId(), result);
        for (const QString &warning : result.warnings)
            appendUnique(warnings, QStringLiteral("%1: %2").arg(item->reference(), warning));
    }
    return results;
}

void Section06Controller::evaluateElectricalModels(const NetworkSnapshot &snapshot,
                                                   double deltaSeconds,
                                                   QStringList &warnings)
{
    for (ComponentItem *item : m_components)
    {
        if (!item || !item->componentModel())
            continue;

        const QVector<std::optional<double>> inputs = snapshot.componentInputs.value(item->modelId());
        if (inputs.size() < 2 || !inputs.at(0).has_value() || !inputs.at(1).has_value())
            continue;

        ComponentElectricalInput electricalInput;
        electricalInput.voltageAcross = *inputs.at(0) - *inputs.at(1);
        electricalInput.deltaSeconds = deltaSeconds;
        const ComponentElectricalResult result = item->componentModel()->evaluateElectrical(electricalInput);
        for (const QString &warning : result.warnings)
            appendUnique(warnings, QStringLiteral("%1: %2").arg(item->reference(), warning));
        item->update();
    }
}

void Section06Controller::appendReferenceWarnings(const NetworkSnapshot &snapshot,
                                                  QStringList &warnings) const
{
    QSet<QString> groundedRoots;
    for (ComponentItem *item : m_components)
    {
        if (!item || item->componentType() != QStringLiteral("Ground"))
            continue;
        const QString endpoint = endpointFor(item, 0);
        groundedRoots.insert(snapshot.endpointRoots.value(endpoint, baseNodeForEndpoint(endpoint)));
    }

    for (ComponentItem *item : m_components)
    {
        if (!item)
            continue;
        const QString type = item->componentType();
        if (type != QStringLiteral("Voltage") && type != QStringLiteral("Battery") &&
            type != QStringLiteral("Clock"))
            continue;

        const QVector<PinModel> pins = item->pins();
        int referenceIndex = -1;
        for (int i = 0; i < pins.size(); ++i)
        {
            if (pins.at(i).name == QStringLiteral("RET"))
            {
                referenceIndex = i;
                break;
            }
        }
        if (referenceIndex < 0)
            continue;

        const QString endpoint = endpointFor(item, referenceIndex);
        const QString root = snapshot.endpointRoots.value(endpoint, baseNodeForEndpoint(endpoint));
        if (!groundedRoots.contains(root))
        {
            appendUnique(warnings,
                         QStringLiteral("%1: source return is not connected to GND.").arg(item->reference()));
        }
    }
}

void Section06Controller::updateMonitor(const NetworkSnapshot &snapshot, const QStringList &warnings)
{
    Q_UNUSED(snapshot);
    QStringList lines;
    QList<QPair<QString, QString>> sorted;
    for (ComponentItem *item : m_components)
    {
        if (!item)
            continue;
        sorted.append(
            qMakePair(item->reference(),
                      QStringLiteral("%1 [%2]  %3")
                          .arg(item->reference(), item->componentType(), shortText(item->runtimeText()))));
    }

    std::sort(sorted.begin(),
              sorted.end(),
              [](const QPair<QString, QString> &first, const QPair<QString, QString> &second)
              { return first.first.localeAwareCompare(second.first) < 0; });
    for (const QPair<QString, QString> &entry : sorted)
        lines.append(entry.second);

    if (lines.isEmpty())
        lines.append(tr("No basic component is placed yet."));

    if (!warnings.isEmpty())
    {
        lines.append(QString());
        lines.append(tr("Warnings:"));
        for (const QString &warning : warnings)
            lines.append(QStringLiteral("- ") + warning);
    }

    const QString text = lines.join(QLatin1Char('\n'));
    if (text != m_lastMonitorText && m_monitor)
    {
        m_lastMonitorText = text;
        m_monitor->setPlainText(text);
    }

    if (!warnings.isEmpty() && m_window && m_window->statusBar())
        m_window->statusBar()->showMessage(warnings.first(), 3000);
}

bool Section06Controller::hasGround() const
{
    return std::any_of(m_components.cbegin(),
                       m_components.cend(),
                       [](ComponentItem *item)
                       { return item && item->componentType() == QStringLiteral("Ground"); });
}

bool Section06Controller::resultsEqual(const QHash<QString, ComponentStepResult> &first,
                                       const QHash<QString, ComponentStepResult> &second) const
{
    if (first.size() != second.size())
        return false;

    for (auto item = second.cbegin(); item != second.cend(); ++item)
    {
        if (!first.contains(item.key()))
            return false;
        const ComponentStepResult previous = first.value(item.key());
        const ComponentStepResult current = item.value();
        if (previous.drivenPins != current.drivenPins ||
            previous.pinVoltages.size() != current.pinVoltages.size())
            return false;
        for (int i = 0; i < current.pinVoltages.size(); ++i)
        {
            if (!optionalEqual(previous.pinVoltages.at(i), current.pinVoltages.at(i)))
                return false;
        }
    }
    return true;
}

QString Section06Controller::endpointFor(const ComponentItem *item, int pinIndex) const
{
    return item ? CircuitGraph::pinEndpoint(item->modelId(), pinIndex) : QString();
}

QString Section06Controller::baseNodeForEndpoint(const QString &endpoint) const
{
    if (endpoint.isEmpty() || !m_section05)
        return {};
    const QString node = m_section05->circuitGraph().nodeForEndpoint(endpoint);
    return node.isEmpty() ? QStringLiteral("endpoint:") + endpoint : QStringLiteral("node:") + node;
}

void Section06Controller::appendUnique(QStringList &values, const QString &value)
{
    if (!value.isEmpty() && !values.contains(value))
        values.append(value);
}

namespace
{
void attachSection06Controller()
{
    bool attached = false;
    const auto windows = QApplication::topLevelWidgets();
    for (QWidget *widget : windows)
    {
        auto *window = qobject_cast<QMainWindow *>(widget);
        if (!window || window->property("section06ControllerAttached").toBool())
            continue;

        auto *section05 = window->findChild<Section05Controller *>();
        if (!section05)
            continue;

        new Section06Controller(window, section05, window);
        window->setProperty("section06ControllerAttached", true);
        attached = true;
    }

    if (!attached)
        QTimer::singleShot(100, [] { attachSection06Controller(); });
}

void installSection06Controller()
{
    QTimer::singleShot(0, [] { attachSection06Controller(); });
}
}

Q_COREAPP_STARTUP_FUNCTION(installSection06Controller)
