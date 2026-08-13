#pragma once

#include "section06controller.h"

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

class QAction;
class CanvasView;
class MainWindow;
class QDockWidget;
class QPlainTextEdit;
class QTimer;
class Section05Controller;

class Section10Controller final : public QObject
{
    Q_OBJECT

  public:
    Section10Controller(MainWindow *window,
                        Section05Controller *section05,
                        Section06Controller *simulation,
                        QObject *parent = nullptr);

  private:
    void buildMenus();
    void buildLogPanel();
    void connectProjectLifecycle();
    void connectHistory();
    void connectSimulation();

    void saveProject();
    void saveProjectAs();
    bool writeProject(const QString &filePath);
    void loadProjectCircuit(const QString &filePath);
    QJsonObject projectDocument() const;

    void exportPng();

    void scheduleHistoryCapture();
    void captureHistory();
    void resetHistory();
    void undo();
    void redo();
    void applyHistorySnapshot(int index);
    void updateActionState();

    void runDesignCheck(bool showDialog = true);
    QStringList designCheckMessages(bool &hasBlockingError) const;
    bool stopForRuntimeConflict();

    void appendLog(const QString &message);
    void replaceLog(const QStringList &lines);
    void showStatus(const QString &message, int timeout = 3500) const;

    static QString simulationStateName(Section06Controller::SimulationState state);
    static QByteArray compactSnapshot(const QJsonObject &object);

    MainWindow *m_window{};
    Section05Controller *m_section05{};
    Section06Controller *m_simulation{};
    CanvasView *m_canvas{};

    QDockWidget *m_logDock{};
    QPlainTextEdit *m_log{};

    QAction *m_saveAction{};
    QAction *m_saveAsAction{};
    QAction *m_exportAction{};
    QAction *m_undoAction{};
    QAction *m_redoAction{};
    QAction *m_drcAction{};

    QTimer *m_historyTimer{};
    QVector<QJsonObject> m_history;
    int m_historyIndex{-1};
    bool m_applyingHistory{false};
    bool m_loadingProject{false};
    QString m_lastSimulationWarning;
    qint64 m_lastSimulationWarningAt{0};
};
