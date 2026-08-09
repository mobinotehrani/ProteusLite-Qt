#pragma once

#include "basiccomponent.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

class ComponentItem;
class QDockWidget;
class QMainWindow;
class QPlainTextEdit;
class QTimer;
class Section05Controller;

class Section06Controller final : public QObject
{
    Q_OBJECT

  public:
    enum class SimulationState
    {
        Stopped,
        Running,
        Paused
    };
    Q_ENUM(SimulationState)

    struct WireSignal
    {
        bool conflict{false};
        bool undefined{true};
        std::optional<double> voltage;
    };

    Section06Controller(QMainWindow *window,
                        Section05Controller *section05,
                        QObject *parent = nullptr);

    SimulationState simulationState() const;
    double simulationTime() const;
    quint64 stepCount() const;
    WireSignal signalForEndpoint(const QString &endpoint) const;
    WireSignal signalForComponentPin(const QString &componentId, int pinIndex) const;
    std::optional<double> branchCurrentForComponent(const QString &componentId) const;

  public slots:
    void runSimulation();
    void pauseSimulation();
    void stopSimulation();
    void stepSimulation();

  signals:
    void simulationStateChanged(Section06Controller::SimulationState state);
    void simulationAdvanced(double timeSeconds, quint64 stepCount);

  private:
    struct NetState
    {
        bool driven{false};
        bool conflict{false};
        bool undefined{false};
        std::optional<double> voltage;
    };

    struct NetworkSnapshot
    {
        QHash<QString, QString> endpointRoots;
        QHash<QString, NetState> nets;
        QHash<QString, QVector<std::optional<double>>> componentInputs;
        QStringList warnings;
    };

    enum class PassiveKind
    {
        Resistor,
        Led
    };

    struct PassiveBranch
    {
        ComponentItem *item{};
        QString firstRoot;
        QString secondRoot;
        PassiveKind kind{PassiveKind::Resistor};
        double resistance{1.0};
        double forwardVoltage{0.0};
    };

    void synchronizeComponents();
    void timerTick();
    void advanceSimulation(double deltaSeconds);
    double manualStepDuration() const;
    NetworkSnapshot resolveNetwork(const QHash<QString, ComponentStepResult> &results) const;
    void solvePassiveNetwork(NetworkSnapshot &snapshot,
                             double deltaSeconds,
                             QStringList &warnings);
    QVector<PassiveBranch> passiveBranches(const NetworkSnapshot &snapshot) const;
    void rebuildComponentInputs(NetworkSnapshot &snapshot) const;
    bool solveLinearSystem(QVector<QVector<double>> matrix,
                           QVector<double> values,
                           QVector<double> &solution) const;
    QHash<QString, ComponentStepResult>
    evaluateComponents(const NetworkSnapshot &snapshot,
                       double nowSeconds,
                       QStringList &warnings);
    void evaluateElectricalModels(const NetworkSnapshot &snapshot,
                                  double deltaSeconds,
                                  QStringList &warnings);
    void appendReferenceWarnings(const NetworkSnapshot &snapshot,
                                 QStringList &warnings) const;
    void updateMonitor(const NetworkSnapshot &snapshot,
                       const QStringList &warnings);
    void resetComponents();
    bool hasGround() const;
    bool resultsEqual(const QHash<QString, ComponentStepResult> &first,
                      const QHash<QString, ComponentStepResult> &second) const;
    QString endpointFor(const ComponentItem *item, int pinIndex) const;
    QString baseNodeForEndpoint(const QString &endpoint) const;
    static QString stateText(SimulationState state);
    static void appendUnique(QStringList &values, const QString &value);

    QMainWindow *m_window{};
    Section05Controller *m_section05{};
    QTimer *m_timer{};
    QDockWidget *m_monitorDock{};
    QPlainTextEdit *m_monitor{};
    QElapsedTimer m_wallClock;
    qint64 m_lastWallMilliseconds{0};
    SimulationState m_state{SimulationState::Stopped};
    double m_simulationTime{0.0};
    quint64 m_stepCount{0};
    QHash<QString, ComponentItem *> m_components;
    QHash<QString, ComponentStepResult> m_lastResults;
    QHash<QString, double> m_branchCurrents;
    NetworkSnapshot m_lastSnapshot;
    QString m_lastMonitorText;
};
