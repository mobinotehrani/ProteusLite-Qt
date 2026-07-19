#pragma once

#include "basiccomponent.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

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
    Section06Controller(QMainWindow *window, Section05Controller *section05, QObject *parent = nullptr);

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

    void synchronizeComponents();
    void runStep();
    NetworkSnapshot resolveNetwork(const QHash<QString, ComponentStepResult> &results) const;
    QHash<QString, ComponentStepResult>
    evaluateComponents(const NetworkSnapshot &snapshot, double nowSeconds, QStringList &warnings);
    void
    evaluateElectricalModels(const NetworkSnapshot &snapshot, double deltaSeconds, QStringList &warnings);
    void appendReferenceWarnings(const NetworkSnapshot &snapshot, QStringList &warnings) const;
    void updateMonitor(const NetworkSnapshot &snapshot, const QStringList &warnings);
    bool hasGround() const;
    bool resultsEqual(const QHash<QString, ComponentStepResult> &first,
                      const QHash<QString, ComponentStepResult> &second) const;
    QString endpointFor(const ComponentItem *item, int pinIndex) const;
    QString baseNodeForEndpoint(const QString &endpoint) const;
    static void appendUnique(QStringList &values, const QString &value);

    QMainWindow *m_window{};
    Section05Controller *m_section05{};
    QTimer *m_timer{};
    QDockWidget *m_monitorDock{};
    QPlainTextEdit *m_monitor{};
    QElapsedTimer m_clock;
    qint64 m_lastStepMilliseconds{0};
    QHash<QString, ComponentItem *> m_components;
    QHash<QString, ComponentStepResult> m_lastResults;
    QString m_lastMonitorText;
};
