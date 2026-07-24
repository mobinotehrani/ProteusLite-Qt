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
#include <limits>

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

bool optionalEqual(const std::optional<double> &first,
                   const std::optional<double> &second)
{
    if (first.has_value() != second.has_value())
        return false;
    if (!first.has_value())
        return true;
    return std::abs(*first - *second) <= 0.000001;
}

QString shortText(const QString &text, int maximum = 88)
{
    return text.size() <= maximum ? text : text.left(maximum - 1) + QChar(0x2026);
}

bool isLedType(const QString &type)
{
    return type == QStringLiteral("LEDRed") ||
           type == QStringLiteral("LEDGreen") ||
           type == QStringLiteral("LEDBlue");
}

void addConductance(QVector<QVector<double>> &matrix,
                    QVector<double> &values,
                    const QHash<QString, int> &indices,
                    const QHash<QString, double> &known,
                    const QString &first,
                    const QString &second,
                    double conductance,
                    double sourceOffset)
{
    const bool firstUnknown = indices.contains(first);
    const bool secondUnknown = indices.contains(second);

    if (firstUnknown)
    {
        const int row = indices.value(first);
        matrix[row][row] += conductance;
        if (secondUnknown)
            matrix[row][indices.value(second)] -= conductance;
        else if (known.contains(second))
            values[row] += conductance * known.value(second);
        values[row] += sourceOffset;
    }

    if (secondUnknown)
    {
        const int row = indices.value(second);
        matrix[row][row] += conductance;
        if (firstUnknown)
            matrix[row][indices.value(first)] -= conductance;
        else if (known.contains(first))
            values[row] += conductance * known.value(first);
        values[row] -= sourceOffset;
    }
}
}

Section06Controller::Section06Controller(QMainWindow *window,
                                         Section05Controller *section05,
                                         QObject *parent)
    : QObject(parent), m_window(window), m_section05(section05)
{
    m_monitorDock = new QDockWidget(tr("Circuit Monitor"), m_window);
    m_monitorDock->setObjectName(QStringLiteral("Section06MonitorDock"));
    m_monitorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_monitorDock->setMinimumWidth(360);

    m_monitor = new QPlainTextEdit(m_monitorDock);
    m_monitor->setReadOnly(true);
    m_monitor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_monitor->setPlaceholderText(tr("Place circuit components to inspect their live state."));
    m_monitor->setStyleSheet(
        QStringLiteral("QPlainTextEdit{background:#0b1220;color:#dbeafe;border:0;padding:10px;"
                       "font-family:'Cascadia Mono','Consolas',monospace;font-size:10pt;}"));
    m_monitorDock->setWidget(m_monitor);
    m_window->addDockWidget(Qt::RightDockWidgetArea, m_monitorDock);

    m_wallClock.start();
    m_timer = new QTimer(this);
    m_timer->setInterval(20);
    connect(m_timer, &QTimer::timeout, this, &Section06Controller::timerTick);

    synchronizeComponents();
    m_lastSnapshot = resolveNetwork({});
    updateMonitor(m_lastSnapshot, {});

    if (m_window && m_window->statusBar())
        m_window->statusBar()->showMessage(tr("Simulation engine ready."), 4000);
}

Section06Controller::SimulationState Section06Controller::simulationState() const
{
    return m_state;
}

double Section06Controller::simulationTime() const
{
    return m_simulationTime;
}

quint64 Section06Controller::stepCount() const
{
    return m_stepCount;
}

Section06Controller::WireSignal
Section06Controller::signalForEndpoint(const QString &endpoint) const
{
    WireSignal signal;
    const QString root = m_lastSnapshot.endpointRoots.value(endpoint,
                                                            baseNodeForEndpoint(endpoint));
    if (root.isEmpty() || !m_lastSnapshot.nets.contains(root))
        return signal;

    const NetState net = m_lastSnapshot.nets.value(root);
    signal.conflict = net.conflict;
    signal.undefined = net.undefined || !net.voltage.has_value();
    signal.voltage = net.voltage;
    return signal;
}

void Section06Controller::runSimulation()
{
    if (m_state == SimulationState::Running)
        return;

    synchronizeComponents();
    if (m_components.isEmpty())
        return;

    m_state = SimulationState::Running;
    m_lastWallMilliseconds = m_wallClock.elapsed();
    m_timer->start();
    emit simulationStateChanged(m_state);
    updateMonitor(m_lastSnapshot, {});
}

void Section06Controller::pauseSimulation()
{
    if (m_state != SimulationState::Running)
        return;

    m_timer->stop();
    m_state = SimulationState::Paused;
    emit simulationStateChanged(m_state);
    updateMonitor(m_lastSnapshot, {});
}

void Section06Controller::stopSimulation()
{
    m_timer->stop();
    m_state = SimulationState::Stopped;
    m_simulationTime = 0.0;
    m_stepCount = 0;
    m_lastWallMilliseconds = m_wallClock.elapsed();
    m_lastResults.clear();
    m_branchCurrents.clear();
    synchronizeComponents();
    resetComponents();
    m_lastSnapshot = resolveNetwork({});
    emit simulationStateChanged(m_state);
    emit simulationAdvanced(m_simulationTime, m_stepCount);
    updateMonitor(m_lastSnapshot, {});
}

void Section06Controller::stepSimulation()
{
    if (m_state == SimulationState::Running)
        pauseSimulation();
    if (m_state == SimulationState::Stopped)
    {
        m_state = SimulationState::Paused;
        emit simulationStateChanged(m_state);
    }

    synchronizeComponents();
    for (ComponentItem *item : m_components)
    {
        if (item && item->componentModel())
            item->componentModel()->prepareManualStep();
    }
    advanceSimulation(manualStepDuration());
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
        {
            m_lastResults.remove(id);
            m_branchCurrents.remove(id);
        }
    }
    m_components = current;
}

void Section06Controller::timerTick()
{
    if (m_state != SimulationState::Running)
        return;

    const qint64 nowMilliseconds = m_wallClock.elapsed();
    double deltaSeconds = m_lastWallMilliseconds == 0
                              ? 0.02
                              : static_cast<double>(nowMilliseconds - m_lastWallMilliseconds) / 1000.0;
    m_lastWallMilliseconds = nowMilliseconds;
    deltaSeconds = std::clamp(deltaSeconds, 0.000001, 0.25);
    advanceSimulation(deltaSeconds);
}

void Section06Controller::advanceSimulation(double deltaSeconds)
{
    synchronizeComponents();
    deltaSeconds = std::clamp(deltaSeconds, 0.000001, 0.25);
    m_simulationTime += deltaSeconds;
    ++m_stepCount;

    QStringList warnings;
    if (!m_components.isEmpty() && !hasGround())
    {
        appendUnique(warnings, QStringLiteral("At least one GND is required before simulation."));
        m_lastSnapshot = resolveNetwork({});
        updateMonitor(m_lastSnapshot, warnings);
        emit simulationAdvanced(m_simulationTime, m_stepCount);
        return;
    }

    QHash<QString, ComponentStepResult> results = m_lastResults;
    NetworkSnapshot snapshot;

    for (int pass = 0; pass < 24; ++pass)
    {
        snapshot = resolveNetwork(results);
        for (const QString &warning : snapshot.warnings)
            appendUnique(warnings, warning);

        solvePassiveNetwork(snapshot, deltaSeconds, warnings);
        const QHash<QString, ComponentStepResult> next =
            evaluateComponents(snapshot, m_simulationTime, warnings);

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

    solvePassiveNetwork(snapshot, deltaSeconds, warnings);
    appendReferenceWarnings(snapshot, warnings);
    evaluateElectricalModels(snapshot, deltaSeconds, warnings);
    m_lastSnapshot = snapshot;
    updateMonitor(m_lastSnapshot, warnings);
    emit simulationAdvanced(m_simulationTime, m_stepCount);
}

double Section06Controller::manualStepDuration() const
{
    double duration = 0.02;
    bool clockFound = false;

    for (ComponentItem *item : m_components)
    {
        if (!item || !item->componentModel() ||
            item->componentType() != QStringLiteral("Clock"))
            continue;

        const double frequency = item->componentModel()->property(QStringLiteral("frequency")).toDouble();
        if (frequency <= 0.0 || !std::isfinite(frequency))
            continue;

        const double halfPeriod = 0.5 / frequency;
        if (!clockFound || halfPeriod < duration)
            duration = halfPeriod;
        clockFound = true;
    }

    return std::clamp(duration, 0.000001, 0.25);
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
            groups.add(baseNodeForEndpoint(endpointFor(item, pinIndex)));
    }

    for (ComponentItem *item : m_components)
    {
        if (!item || !item->componentModel())
            continue;
        const QVector<QPair<int, int>> pairs = item->componentModel()->conductivePinPairs();
        for (const QPair<int, int> &pair : pairs)
        {
            groups.unite(baseNodeForEndpoint(endpointFor(item, pair.first)),
                         baseNodeForEndpoint(endpointFor(item, pair.second)));
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
            snapshot.endpointRoots.insert(endpoint,
                                          groups.find(baseNodeForEndpoint(endpoint)));
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
            const QString root = snapshot.endpointRoots.value(endpoint,
                                                              baseNodeForEndpoint(endpoint));
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
    for (auto endpoint = snapshot.endpointRoots.cbegin();
         endpoint != snapshot.endpointRoots.cend();
         ++endpoint)
    {
        roots.insert(endpoint.value());
    }

    for (const QString &root : roots)
    {
        const NetAccumulator accumulator = accumulators.value(root);
        NetState state;
        state.driven = accumulator.driven;
        state.conflict = accumulator.conflict;
        state.undefined = accumulator.unknown || accumulator.conflict;
        if (state.driven && accumulator.hasNumericVoltage &&
            !state.conflict && !state.undefined)
        {
            state.voltage = accumulator.voltage;
        }
        snapshot.nets.insert(root, state);
        if (state.conflict)
            appendUnique(snapshot.warnings, QStringLiteral("Conflicting drivers detected."));
    }

    rebuildComponentInputs(snapshot);
    return snapshot;
}

QVector<Section06Controller::PassiveBranch>
Section06Controller::passiveBranches(const NetworkSnapshot &snapshot) const
{
    QVector<PassiveBranch> branches;

    for (ComponentItem *item : m_components)
    {
        if (!item || !item->componentModel() || item->pins().size() < 2)
            continue;

        const QString type = item->componentType();
        if (type != QStringLiteral("Resistor") && !isLedType(type))
            continue;

        PassiveBranch branch;
        branch.item = item;
        branch.firstRoot = snapshot.endpointRoots.value(endpointFor(item, 0));
        branch.secondRoot = snapshot.endpointRoots.value(endpointFor(item, 1));
        if (branch.firstRoot.isEmpty() || branch.secondRoot.isEmpty() ||
            branch.firstRoot == branch.secondRoot)
            continue;

        if (type == QStringLiteral("Resistor"))
        {
            branch.kind = PassiveKind::Resistor;
            branch.resistance = std::max(0.000001,
                                         item->componentModel()
                                             ->property(QStringLiteral("resistance"))
                                             .toDouble());
        }
        else
        {
            branch.kind = PassiveKind::Led;
            branch.resistance = 10.0;
            branch.forwardVoltage = std::max(0.0,
                                             item->componentModel()
                                                 ->property(QStringLiteral("forwardVoltage"))
                                                 .toDouble());
        }
        branches.append(branch);
    }

    return branches;
}

void Section06Controller::solvePassiveNetwork(NetworkSnapshot &snapshot,
                                               double deltaSeconds,
                                               QStringList &warnings)
{
    Q_UNUSED(deltaSeconds);
    m_branchCurrents.clear();

    const QVector<PassiveBranch> branches = passiveBranches(snapshot);
    if (branches.isEmpty())
    {
        rebuildComponentInputs(snapshot);
        return;
    }

    QHash<QString, double> known;
    QSet<QString> blocked;
    for (auto net = snapshot.nets.cbegin(); net != snapshot.nets.cend(); ++net)
    {
        if (net.value().conflict || (net.value().driven && net.value().undefined))
        {
            blocked.insert(net.key());
            continue;
        }
        if (net.value().voltage.has_value())
            known.insert(net.key(), *net.value().voltage);
    }

    QHash<QString, QSet<QString>> adjacency;
    for (const PassiveBranch &branch : branches)
    {
        if (blocked.contains(branch.firstRoot) || blocked.contains(branch.secondRoot))
            continue;
        adjacency[branch.firstRoot].insert(branch.secondRoot);
        adjacency[branch.secondRoot].insert(branch.firstRoot);
    }

    QSet<QString> reachable;
    QList<QString> queue = known.keys();
    for (const QString &root : queue)
        reachable.insert(root);

    for (int cursor = 0; cursor < queue.size(); ++cursor)
    {
        const QString root = queue.at(cursor);
        for (const QString &next : adjacency.value(root))
        {
            if (!reachable.contains(next))
            {
                reachable.insert(next);
                queue.append(next);
            }
        }
    }

    QStringList unknownRoots;
    for (const QString &root : reachable)
    {
        if (!known.contains(root))
            unknownRoots.append(root);
    }
    std::sort(unknownRoots.begin(), unknownRoots.end());

    if (unknownRoots.isEmpty())
    {
        for (const PassiveBranch &branch : branches)
        {
            if (!known.contains(branch.firstRoot) || !known.contains(branch.secondRoot))
                continue;
            const double voltage = known.value(branch.firstRoot) - known.value(branch.secondRoot);
            const double current = branch.kind == PassiveKind::Resistor
                                       ? voltage / branch.resistance
                                       : std::max(0.0,
                                                  (voltage - branch.forwardVoltage) /
                                                      branch.resistance);
            m_branchCurrents.insert(branch.item->modelId(), current);
        }
        rebuildComponentInputs(snapshot);
        return;
    }

    QHash<QString, int> indices;
    for (int i = 0; i < unknownRoots.size(); ++i)
        indices.insert(unknownRoots.at(i), i);

    QHash<QString, double> estimates = known;
    double averageKnown = 0.0;
    for (double value : known)
        averageKnown += value;
    if (!known.isEmpty())
        averageKnown /= static_cast<double>(known.size());
    for (const QString &root : unknownRoots)
        estimates.insert(root, averageKnown);

    QSet<QString> previousActiveLeds;
    bool solved = false;

    for (int iteration = 0; iteration < 12; ++iteration)
    {
        QVector<QVector<double>> matrix(unknownRoots.size(),
                                        QVector<double>(unknownRoots.size(), 0.0));
        QVector<double> values(unknownRoots.size(), 0.0);
        QSet<QString> activeLeds;

        for (const PassiveBranch &branch : branches)
        {
            if (blocked.contains(branch.firstRoot) ||
                blocked.contains(branch.secondRoot) ||
                !reachable.contains(branch.firstRoot) ||
                !reachable.contains(branch.secondRoot))
                continue;

            double sourceOffset = 0.0;
            double conductance = 0.0;
            if (branch.kind == PassiveKind::Resistor)
            {
                conductance = 1.0 / branch.resistance;
            }
            else
            {
                const double firstVoltage = estimates.value(branch.firstRoot, averageKnown);
                const double secondVoltage = estimates.value(branch.secondRoot, averageKnown);
                if (firstVoltage - secondVoltage <= branch.forwardVoltage + 0.000001)
                    continue;
                conductance = 1.0 / branch.resistance;
                sourceOffset = conductance * branch.forwardVoltage;
                activeLeds.insert(branch.item->modelId());
            }

            addConductance(matrix,
                           values,
                           indices,
                           known,
                           branch.firstRoot,
                           branch.secondRoot,
                           conductance,
                           sourceOffset);
        }

        QVector<double> solution;
        if (!solveLinearSystem(matrix, values, solution))
            break;

        solved = true;
        double maximumChange = 0.0;
        for (int i = 0; i < unknownRoots.size(); ++i)
        {
            maximumChange = std::max(maximumChange,
                                     std::abs(estimates.value(unknownRoots.at(i)) - solution.at(i)));
            estimates[unknownRoots.at(i)] = solution.at(i);
        }

        if (maximumChange <= 0.000001 && activeLeds == previousActiveLeds)
            break;
        previousActiveLeds = activeLeds;
    }

    if (!solved)
    {
        appendUnique(warnings,
                     QStringLiteral("Passive network could not be resolved because it is floating or singular."));
        rebuildComponentInputs(snapshot);
        return;
    }

    for (const QString &root : unknownRoots)
    {
        NetState state = snapshot.nets.value(root);
        state.driven = false;
        state.conflict = false;
        state.undefined = false;
        state.voltage = estimates.value(root);
        snapshot.nets.insert(root, state);
    }

    for (const PassiveBranch &branch : branches)
    {
        if (!estimates.contains(branch.firstRoot) || !estimates.contains(branch.secondRoot))
            continue;

        const double voltage = estimates.value(branch.firstRoot) - estimates.value(branch.secondRoot);
        const double current = branch.kind == PassiveKind::Resistor
                                   ? voltage / branch.resistance
                                   : std::max(0.0,
                                              (voltage - branch.forwardVoltage) /
                                                  branch.resistance);
        m_branchCurrents.insert(branch.item->modelId(), current);
    }

    rebuildComponentInputs(snapshot);
}

void Section06Controller::rebuildComponentInputs(NetworkSnapshot &snapshot) const
{
    snapshot.componentInputs.clear();
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
            const QString root = snapshot.endpointRoots.value(endpoint,
                                                              baseNodeForEndpoint(endpoint));
            inputs[pinIndex] = snapshot.nets.value(root).voltage;
        }
        snapshot.componentInputs.insert(item->modelId(), inputs);
    }
}

bool Section06Controller::solveLinearSystem(QVector<QVector<double>> matrix,
                                            QVector<double> values,
                                            QVector<double> &solution) const
{
    const int count = matrix.size();
    if (count == 0 || values.size() != count)
        return false;

    for (int column = 0; column < count; ++column)
    {
        int pivot = column;
        for (int row = column + 1; row < count; ++row)
        {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column]))
                pivot = row;
        }

        if (std::abs(matrix[pivot][column]) <= 1e-12)
            return false;

        if (pivot != column)
        {
            matrix.swapItemsAt(pivot, column);
            values.swapItemsAt(pivot, column);
        }

        const double divisor = matrix[column][column];
        for (int entry = column; entry < count; ++entry)
            matrix[column][entry] /= divisor;
        values[column] /= divisor;

        for (int row = 0; row < count; ++row)
        {
            if (row == column)
                continue;
            const double factor = matrix[row][column];
            if (std::abs(factor) <= 1e-15)
                continue;
            for (int entry = column; entry < count; ++entry)
                matrix[row][entry] -= factor * matrix[column][entry];
            values[row] -= factor * values[column];
        }
    }

    solution = values;
    for (double value : solution)
    {
        if (!std::isfinite(value))
            return false;
    }
    return true;
}

QHash<QString, ComponentStepResult>
Section06Controller::evaluateComponents(const NetworkSnapshot &snapshot,
                                        double nowSeconds,
                                        QStringList &warnings)
{
    QHash<QString, ComponentStepResult> results;
    for (ComponentItem *item : m_components)
    {
        if (!item || !item->componentModel())
            continue;

        const QVector<std::optional<double>> inputs =
            snapshot.componentInputs.value(item->modelId());
        const ComponentStepResult result = item->updateSimulation(inputs, nowSeconds);
        results.insert(item->modelId(), result);
        for (const QString &warning : result.warnings)
            appendUnique(warnings,
                         QStringLiteral("%1: %2").arg(item->reference(), warning));
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

        const QVector<std::optional<double>> inputs =
            snapshot.componentInputs.value(item->modelId());
        ComponentElectricalInput electricalInput;
        electricalInput.deltaSeconds = deltaSeconds;

        if (m_branchCurrents.contains(item->modelId()))
            electricalInput.branchCurrent = m_branchCurrents.value(item->modelId());
        else if (inputs.size() >= 2 && inputs.at(0).has_value() && inputs.at(1).has_value())
            electricalInput.voltageAcross = *inputs.at(0) - *inputs.at(1);
        else
            continue;

        const ComponentElectricalResult result =
            item->componentModel()->evaluateElectrical(electricalInput);
        for (const QString &warning : result.warnings)
            appendUnique(warnings,
                         QStringLiteral("%1: %2").arg(item->reference(), warning));
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
        groundedRoots.insert(snapshot.endpointRoots.value(endpoint,
                                                          baseNodeForEndpoint(endpoint)));
    }

    for (ComponentItem *item : m_components)
    {
        if (!item)
            continue;

        const QString type = item->componentType();
        if (type != QStringLiteral("Voltage") &&
            type != QStringLiteral("Battery") &&
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
        const QString root = snapshot.endpointRoots.value(endpoint,
                                                          baseNodeForEndpoint(endpoint));
        if (!groundedRoots.contains(root))
        {
            appendUnique(warnings,
                         QStringLiteral("%1: source return is not connected to GND.")
                             .arg(item->reference()));
        }
    }
}

void Section06Controller::updateMonitor(const NetworkSnapshot &snapshot,
                                        const QStringList &warnings)
{
    Q_UNUSED(snapshot);
    QStringList lines;
    lines.append(QStringLiteral("SIMULATION: %1  |  t=%2 s  |  step=%3")
                     .arg(stateText(m_state))
                     .arg(m_simulationTime, 0, 'f', 6)
                     .arg(m_stepCount));
    lines.append(QStringLiteral("Components: %1").arg(m_components.size()));
    lines.append(QString(52, QLatin1Char('-')));

    QList<QPair<QString, QString>> sorted;
    for (ComponentItem *item : m_components)
    {
        if (!item)
            continue;
        sorted.append(qMakePair(item->reference(),
                                QStringLiteral("%1  [%2]  %3")
                                    .arg(item->reference(),
                                         item->componentType(),
                                         shortText(item->runtimeText()))));
    }

    std::sort(sorted.begin(),
              sorted.end(),
              [](const QPair<QString, QString> &first,
                 const QPair<QString, QString> &second)
              {
                  return first.first.localeAwareCompare(second.first) < 0;
              });

    for (const QPair<QString, QString> &entry : sorted)
        lines.append(entry.second);

    if (sorted.isEmpty())
        lines.append(tr("No circuit component is placed yet."));

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
        m_window->statusBar()->showMessage(warnings.first(), 3500);
}

void Section06Controller::resetComponents()
{
    for (ComponentItem *item : m_components)
    {
        if (!item || !item->componentModel())
            continue;

        Component *model = item->componentModel();
        const QVector<ComponentProperty> properties = model->editableProperties();
        model->loadState({});
        for (const ComponentProperty &property : properties)
            model->setProperty(property.key, property.value);

        QVector<std::optional<double>> emptyInputs(item->pins().size());
        item->updateSimulation(emptyInputs, 0.0);
        item->update();
    }
}

bool Section06Controller::hasGround() const
{
    return std::any_of(m_components.cbegin(),
                       m_components.cend(),
                       [](ComponentItem *item)
                       {
                           return item &&
                                  item->componentType() == QStringLiteral("Ground");
                       });
}

bool Section06Controller::resultsEqual(
    const QHash<QString, ComponentStepResult> &first,
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
            if (!optionalEqual(previous.pinVoltages.at(i),
                               current.pinVoltages.at(i)))
                return false;
        }
    }
    return true;
}

QString Section06Controller::endpointFor(const ComponentItem *item,
                                         int pinIndex) const
{
    return item ? CircuitGraph::pinEndpoint(item->modelId(), pinIndex) : QString();
}

QString Section06Controller::baseNodeForEndpoint(const QString &endpoint) const
{
    if (endpoint.isEmpty() || !m_section05)
        return {};
    const QString node = m_section05->circuitGraph().nodeForEndpoint(endpoint);
    return node.isEmpty() ? QStringLiteral("endpoint:") + endpoint
                          : QStringLiteral("node:") + node;
}

QString Section06Controller::stateText(SimulationState state)
{
    switch (state)
    {
    case SimulationState::Running:
        return QStringLiteral("RUNNING");
    case SimulationState::Paused:
        return QStringLiteral("PAUSED");
    case SimulationState::Stopped:
        return QStringLiteral("STOPPED");
    }
    return QStringLiteral("UNKNOWN");
}

void Section06Controller::appendUnique(QStringList &values,
                                       const QString &value)
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
