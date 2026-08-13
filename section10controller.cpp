#include "section10controller.h"

#include "canvasview.h"
#include "circuitmodel.h"
#include "componentitem.h"
#include "mainwindow.h"
#include "section05controller.h"
#include "wireitem.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSet>
#include <QStatusBar>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <optional>

namespace
{
QMenu *menuNamed(QMenuBar *menuBar, const QString &name)
{
    if (!menuBar)
        return nullptr;

    for (QAction *action : menuBar->actions())
    {
        if (!action || !action->menu())
            continue;
        QString text = action->text();
        text.remove(QLatin1Char('&'));
        if (text.compare(name, Qt::CaseInsensitive) == 0)
            return action->menu();
    }
    return nullptr;
}

QString ensuredJsonPath(QString path)
{
    if (!path.isEmpty() && QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".json");
    return path;
}

QString ensuredPngPath(QString path)
{
    if (!path.isEmpty() && QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".png");
    return path;
}

bool sourceType(const QString &type)
{
    return type == QStringLiteral("Voltage") || type == QStringLiteral("Battery") ||
           type == QStringLiteral("Clock");
}

std::optional<double> sourceNominalVoltage(const ComponentItem *component)
{
    if (!component || !component->componentModel())
        return std::nullopt;

    const QString type = component->componentType();
    if (type == QStringLiteral("Voltage") || type == QStringLiteral("Battery"))
        return component->componentModel()->property(QStringLiteral("voltage")).toDouble();
    if (type == QStringLiteral("Clock"))
        return component->componentModel()->property(QStringLiteral("highVoltage")).toDouble();
    return std::nullopt;
}
}

Section10Controller::Section10Controller(MainWindow *window,
                                         Section05Controller *section05,
                                         Section06Controller *simulation,
                                         QObject *parent)
    : QObject(parent),
      m_window(window),
      m_section05(section05),
      m_simulation(simulation),
      m_canvas(section05 ? section05->canvasView() : nullptr)
{
    buildMenus();
    buildLogPanel();
    connectProjectLifecycle();
    connectHistory();
    connectSimulation();
    resetHistory();
    updateActionState();

    appendLog(tr("Section 10 ready. Save, history, export and design checks are available."));

    if (m_window && m_window->hasActiveProject())
    {
        const QString path = m_window->currentProjectPath();
        if (!path.isEmpty())
            QTimer::singleShot(0, this, [this, path] { loadProjectCircuit(path); });
    }
}

void Section10Controller::buildMenus()
{
    if (!m_window || !m_window->menuBar())
        return;

    QMenuBar *mainMenuBar = m_window->menuBar();
    mainMenuBar->setNativeMenuBar(false);
    mainMenuBar->setMinimumHeight(32);
    mainMenuBar->setVisible(true);
    mainMenuBar->raise();

    QMenu *fileMenu = menuNamed(mainMenuBar, QStringLiteral("File"));
    if (!fileMenu)
        fileMenu = mainMenuBar->addMenu(tr("File"));

    m_saveAction = new QAction(tr("Save Project"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAsAction = new QAction(tr("Save Project As..."), this);
    m_saveAsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    m_exportAction = new QAction(tr("Export Circuit as PNG..."), this);
    m_exportAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+E")));

    QAction *exitAction = nullptr;
    for (QAction *action : fileMenu->actions())
    {
        if (!action)
            continue;
        QString actionText = action->text();
        actionText.remove(QLatin1Char('&'));
        if (actionText.compare(QStringLiteral("Exit"), Qt::CaseInsensitive) == 0)
        {
            exitAction = action;
            break;
        }
    }

    if (exitAction)
    {
        fileMenu->insertSeparator(exitAction);
        fileMenu->insertAction(exitAction, m_exportAction);
        fileMenu->insertAction(m_exportAction, m_saveAsAction);
        fileMenu->insertAction(m_saveAsAction, m_saveAction);
    }
    else
    {
        fileMenu->addSeparator();
        fileMenu->addAction(m_saveAction);
        fileMenu->addAction(m_saveAsAction);
        fileMenu->addAction(m_exportAction);
    }

    connect(m_saveAction, &QAction::triggered, this, &Section10Controller::saveProject);
    connect(m_saveAsAction, &QAction::triggered, this, &Section10Controller::saveProjectAs);
    connect(m_exportAction, &QAction::triggered, this, &Section10Controller::exportPng);

    QMenu *editMenu = menuNamed(mainMenuBar, QStringLiteral("Edit"));
    if (!editMenu)
    {
        auto *createdEditMenu = new QMenu(tr("Edit"), mainMenuBar);
        QMenu *viewMenu = menuNamed(mainMenuBar, QStringLiteral("View"));
        if (viewMenu)
            mainMenuBar->insertMenu(viewMenu->menuAction(), createdEditMenu);
        else
            mainMenuBar->addMenu(createdEditMenu);
        editMenu = createdEditMenu;
    }

    m_undoAction = editMenu->addAction(tr("Undo"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_redoAction = editMenu->addAction(tr("Redo"));
    m_redoAction->setShortcuts({QKeySequence::Redo, QKeySequence(QStringLiteral("Ctrl+Shift+Z"))});
    connect(m_undoAction, &QAction::triggered, this, &Section10Controller::undo);
    connect(m_redoAction, &QAction::triggered, this, &Section10Controller::redo);

    QMenu *toolsMenu = menuNamed(mainMenuBar, QStringLiteral("Tools"));
    if (!toolsMenu)
        toolsMenu = mainMenuBar->addMenu(tr("Tools"));
    m_drcAction = toolsMenu->addAction(tr("Run Design Check"));
    m_drcAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+D")));
    connect(m_drcAction, &QAction::triggered, this, [this] { runDesignCheck(true); });
}

void Section10Controller::buildLogPanel()
{
    if (!m_window)
        return;

    m_logDock = new QDockWidget(tr("Simulation Log / DRC"), m_window);
    m_logDock->setObjectName(QStringLiteral("Section10LogDock"));
    m_logDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea |
                               Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_logDock->setMinimumHeight(150);

    m_log = new QPlainTextEdit(m_logDock);
    m_log->setReadOnly(true);
    m_log->setPlaceholderText(tr("Simulation status, save/load messages and design-rule results appear here."));
    m_log->setStyleSheet(QStringLiteral(
        "QPlainTextEdit{background:#0f172a;color:#dbeafe;border:0;padding:9px;"
        "font-family:Consolas,monospace;font-size:9.5pt;}"));
    m_logDock->setWidget(m_log);
    m_window->addDockWidget(Qt::BottomDockWidgetArea, m_logDock);

    QMenu *viewMenu = menuNamed(m_window->menuBar(), QStringLiteral("View"));
    if (viewMenu)
    {
        viewMenu->addSeparator();
        viewMenu->addAction(m_logDock->toggleViewAction());
    }
}

void Section10Controller::connectProjectLifecycle()
{
    if (!m_window)
        return;

    connect(m_window,
            &MainWindow::projectCreated,
            this,
            [this]
            {
                if (m_simulation)
                    m_simulation->stopSimulation();
                if (m_section05)
                    m_section05->clearCircuit();
                resetHistory();
                replaceLog({tr("New project started."), tr("History has been reset.")});
                updateActionState();
            });

    connect(m_window,
            &MainWindow::projectOpened,
            this,
            [this](const QString &filePath) { loadProjectCircuit(filePath); });
}

void Section10Controller::connectHistory()
{
    if (!m_section05)
        return;

    m_historyTimer = new QTimer(this);
    m_historyTimer->setSingleShot(true);
    m_historyTimer->setInterval(280);
    connect(m_historyTimer, &QTimer::timeout, this, &Section10Controller::captureHistory);
    connect(m_section05, &Section05Controller::circuitChanged, this, &Section10Controller::scheduleHistoryCapture);
}

void Section10Controller::connectSimulation()
{
    if (m_section05)
    {
        connect(m_section05,
                &Section05Controller::simulationMessage,
                this,
                [this](const QString &message)
                {
                    if (message.isEmpty())
                        return;
                    const qint64 now = QDateTime::currentMSecsSinceEpoch();
                    if (message == m_lastSimulationWarning && now - m_lastSimulationWarningAt < 1500)
                        return;
                    m_lastSimulationWarning = message;
                    m_lastSimulationWarningAt = now;
                    appendLog(tr("WARNING: %1").arg(message));
                });
    }

    if (!m_simulation)
        return;

    m_simulation->setRunGuard([this]
    {
        bool blocking = false;
        const QStringList messages = designCheckMessages(blocking);

        if (!blocking)
        {
            for (const QString &message : messages)
            {
                if (message.startsWith(QStringLiteral("WARNING:")) ||
                    message.startsWith(QStringLiteral("INFO:")))
                {
                    appendLog(message);
                }
            }
            appendLog(tr("DRC OK: no blocking design errors were found before Run."));
            return true;
        }

        appendLog(tr("Simulation start blocked by design check."));
        for (const QString &message : messages)
            appendLog(message);
        showStatus(tr("Run blocked: fix the DRC errors first."), 5000);
        if (m_window)
            QMessageBox::warning(m_window,
                                 tr("Simulation blocked"),
                                 tr("The circuit has blocking design errors. See Simulation Log / DRC for details."));
        return false;
    });

    connect(m_simulation,
            &Section06Controller::simulationStateChanged,
            this,
            [this](Section06Controller::SimulationState state)
            {
                appendLog(tr("Simulation state: %1").arg(simulationStateName(state)));
            });

    connect(m_simulation,
            &Section06Controller::simulationAdvanced,
            this,
            [this](double, quint64)
            {
                if (stopForRuntimeConflict() && m_simulation)
                    m_simulation->stopSimulation();
            });
}

void Section10Controller::saveProject()
{
    if (!m_window || !m_window->hasActiveProject())
    {
        showStatus(tr("Create or open a project before saving."));
        return;
    }

    const QString currentPath = m_window->currentProjectPath();
    if (currentPath.isEmpty() || QFileInfo(currentPath).suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) != 0)
    {
        saveProjectAs();
        return;
    }

    writeProject(currentPath);
}

void Section10Controller::saveProjectAs()
{
    if (!m_window || !m_window->hasActiveProject())
    {
        showStatus(tr("Create or open a project before saving."));
        return;
    }

    QString initial = m_window->currentProjectPath();
    if (initial.isEmpty())
        initial = m_window->currentProjectName().isEmpty() ? QStringLiteral("project.json")
                                                            : m_window->currentProjectName() + QStringLiteral(".json");

    QString path = QFileDialog::getSaveFileName(m_window,
                                                tr("Save Project As"),
                                                initial,
                                                tr("ProteusPro JSON Project (*.json)"));
    path = ensuredJsonPath(path);
    if (path.isEmpty())
        return;

    if (QFileInfo::exists(path))
    {
        QMessageBox confirm(m_window);
        confirm.setIcon(QMessageBox::Warning);
        confirm.setWindowTitle(tr("Save Project As"));
        confirm.setText(tr("A project with this name already exists."));
        confirm.setInformativeText(tr("Do you want to overwrite the existing file?"));
        QPushButton *overwriteButton = confirm.addButton(tr("Overwrite"), QMessageBox::AcceptRole);
        confirm.addButton(QMessageBox::Cancel);
        confirm.exec();
        if (confirm.clickedButton() != overwriteButton)
            return;
    }

    writeProject(path);
}

bool Section10Controller::writeProject(const QString &filePath)
{
    if (!m_section05 || !m_window)
        return false;

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(m_window,
                             tr("Save Project"),
                             tr("The project could not be opened for writing:\n%1").arg(file.errorString()));
        return false;
    }

    const QJsonDocument document(projectDocument());
    if (file.write(document.toJson(QJsonDocument::Indented)) < 0 || !file.commit())
    {
        QMessageBox::warning(m_window,
                             tr("Save Project"),
                             tr("The project could not be saved:\n%1").arg(file.errorString()));
        return false;
    }

    const QJsonObject snapshot = m_section05->circuitSnapshot();
    const int componentCount = snapshot.value(QStringLiteral("components")).toArray().size();
    const int wireCount = snapshot.value(QStringLiteral("wires")).toArray().size();
    m_window->markProjectSaved(filePath, componentCount, wireCount);

    appendLog(tr("Saved project: %1").arg(QFileInfo(filePath).absoluteFilePath()));
    showStatus(tr("Project saved."), 3000);
    updateActionState();
    return true;
}

QJsonObject Section10Controller::projectDocument() const
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("ProteusPro"));
    root.insert(QStringLiteral("formatVersion"), 1);
    root.insert(QStringLiteral("name"), m_window ? m_window->currentProjectName() : QStringLiteral("Untitled Project"));

    const QSize canvasSize = m_window ? m_window->currentCanvasSize() : QSize(3000, 2000);
    QJsonObject canvas;
    canvas.insert(QStringLiteral("w"), canvasSize.width());
    canvas.insert(QStringLiteral("h"), canvasSize.height());
    root.insert(QStringLiteral("canvas"), canvas);

    if (m_section05)
    {
        const QJsonObject snapshot = m_section05->circuitSnapshot();
        root.insert(QStringLiteral("components"), snapshot.value(QStringLiteral("components")));
        root.insert(QStringLiteral("junctions"), snapshot.value(QStringLiteral("junctions")));
        root.insert(QStringLiteral("wires"), snapshot.value(QStringLiteral("wires")));
    }

    if (m_simulation)
    {
        QJsonObject simulation;
        simulation.insert(QStringLiteral("state"), simulationStateName(m_simulation->simulationState()));
        simulation.insert(QStringLiteral("timeSeconds"), m_simulation->simulationTime());
        simulation.insert(QStringLiteral("stepCount"), static_cast<double>(m_simulation->stepCount()));
        root.insert(QStringLiteral("simulation"), simulation);
    }

    return root;
}

void Section10Controller::loadProjectCircuit(const QString &filePath)
{
    if (!m_section05 || filePath.isEmpty() || m_loadingProject)
        return;

    const QFileInfo info(filePath);
    if (info.suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) != 0)
    {
        resetHistory();
        appendLog(tr("Opened %1. XML projects keep their metadata, but full circuit restoration is available for JSON files.")
                      .arg(info.fileName()));
        updateActionState();
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        appendLog(tr("Could not read project circuit: %1").arg(file.errorString()));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        appendLog(tr("Invalid JSON circuit data: %1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject root = document.object();
    const bool section10Format =
        root.value(QStringLiteral("format")).toString() == QStringLiteral("ProteusPro") &&
        root.value(QStringLiteral("formatVersion")).toInt() >= 1;
    if (!section10Format || !root.value(QStringLiteral("components")).isArray() ||
        !root.value(QStringLiteral("wires")).isArray())
    {
        resetHistory();
        appendLog(tr("Opened %1 as a metadata-only project. Save it with Section 10 to create a full circuit snapshot.")
                      .arg(info.fileName()));
        updateActionState();
        return;
    }

    QJsonObject snapshot;
    snapshot.insert(QStringLiteral("components"), root.value(QStringLiteral("components")));
    snapshot.insert(QStringLiteral("junctions"), root.value(QStringLiteral("junctions")));
    snapshot.insert(QStringLiteral("wires"), root.value(QStringLiteral("wires")));

    if (m_simulation)
        m_simulation->stopSimulation();

    m_loadingProject = true;
    QString errorMessage;
    const bool restored = m_section05->restoreCircuitSnapshot(snapshot, errorMessage);
    m_loadingProject = false;

    if (!restored)
    {
        QMessageBox::warning(m_window, tr("Open Project"), errorMessage);
        appendLog(tr("Circuit restoration failed: %1").arg(errorMessage));
        return;
    }

    resetHistory();
    appendLog(tr("Restored circuit from %1 (%2 components, %3 wires).")
                  .arg(info.fileName())
                  .arg(snapshot.value(QStringLiteral("components")).toArray().size())
                  .arg(snapshot.value(QStringLiteral("wires")).toArray().size()));

    const QJsonObject savedSimulation = root.value(QStringLiteral("simulation")).toObject();
    if (!savedSimulation.isEmpty())
    {
        appendLog(tr("Saved runtime marker: %1 at %2 s, step %3. The engine opens in STOPPED state for safe editing.")
                      .arg(savedSimulation.value(QStringLiteral("state")).toString(QStringLiteral("UNKNOWN")))
                      .arg(savedSimulation.value(QStringLiteral("timeSeconds")).toDouble(), 0, 'f', 6)
                      .arg(savedSimulation.value(QStringLiteral("stepCount")).toVariant().toULongLong()));
    }

    runDesignCheck(false);
    updateActionState();
}

void Section10Controller::exportPng()
{
    if (!m_window || !m_window->hasActiveProject() || !m_canvas || !m_canvas->scene())
    {
        showStatus(tr("Create or open a project before exporting."));
        return;
    }

    QString suggested = m_window->currentProjectName().isEmpty()
                            ? QStringLiteral("circuit.png")
                            : m_window->currentProjectName() + QStringLiteral(".png");
    QString filePath = QFileDialog::getSaveFileName(m_window,
                                                    tr("Export Circuit as PNG"),
                                                    suggested,
                                                    tr("PNG Image (*.png)"));
    filePath = ensuredPngPath(filePath);
    if (filePath.isEmpty())
        return;

    QRectF sourceRect = m_canvas->scene()->sceneRect();
    if (!sourceRect.isValid() || sourceRect.isEmpty())
        sourceRect = m_canvas->scene()->itemsBoundingRect().adjusted(-40.0, -40.0, 40.0, 40.0);
    if (!sourceRect.isValid() || sourceRect.isEmpty())
    {
        showStatus(tr("There is no circuit area to export."));
        return;
    }

    QSize imageSize = sourceRect.size().toSize();
    imageSize.setWidth(std::max(1, imageSize.width()));
    imageSize.setHeight(std::max(1, imageSize.height()));
    if (std::max(imageSize.width(), imageSize.height()) > 5000)
        imageSize.scale(5000, 5000, Qt::KeepAspectRatio);

    const QList<QGraphicsItem *> selected = m_canvas->scene()->selectedItems();
    for (QGraphicsItem *item : selected)
        item->setSelected(false);

    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    m_canvas->scene()->render(&painter,
                              QRectF(QPointF(0.0, 0.0), QSizeF(imageSize)),
                              sourceRect,
                              Qt::KeepAspectRatio);
    painter.end();

    for (QGraphicsItem *item : selected)
        item->setSelected(true);

    if (!image.save(filePath, "PNG"))
    {
        QMessageBox::warning(m_window, tr("Export Circuit"), tr("The PNG image could not be saved."));
        return;
    }

    appendLog(tr("Exported PNG: %1").arg(QFileInfo(filePath).absoluteFilePath()));
    showStatus(tr("Circuit image exported."), 3000);
}

void Section10Controller::scheduleHistoryCapture()
{
    if (m_applyingHistory || m_loadingProject || !m_historyTimer)
        return;
    m_historyTimer->start();
}

void Section10Controller::captureHistory()
{
    if (!m_section05 || m_applyingHistory || m_loadingProject)
        return;

    const QJsonObject snapshot = m_section05->circuitSnapshot();
    if (m_historyIndex >= 0 && m_historyIndex < m_history.size() &&
        compactSnapshot(snapshot) == compactSnapshot(m_history.at(m_historyIndex)))
    {
        updateActionState();
        return;
    }

    while (m_history.size() > m_historyIndex + 1)
        m_history.removeLast();

    m_history.append(snapshot);
    if (m_history.size() > 60)
        m_history.removeFirst();
    m_historyIndex = m_history.size() - 1;
    updateActionState();
}

void Section10Controller::resetHistory()
{
    if (m_historyTimer)
        m_historyTimer->stop();
    m_history.clear();
    m_historyIndex = -1;
    if (m_section05)
    {
        m_history.append(m_section05->circuitSnapshot());
        m_historyIndex = 0;
    }
    updateActionState();
}

void Section10Controller::undo()
{
    if (m_historyIndex <= 0)
        return;
    applyHistorySnapshot(m_historyIndex - 1);
}

void Section10Controller::redo()
{
    if (m_historyIndex < 0 || m_historyIndex + 1 >= m_history.size())
        return;
    applyHistorySnapshot(m_historyIndex + 1);
}

void Section10Controller::applyHistorySnapshot(int index)
{
    if (!m_section05 || index < 0 || index >= m_history.size())
        return;

    if (m_simulation)
        m_simulation->stopSimulation();
    if (m_historyTimer)
        m_historyTimer->stop();

    m_applyingHistory = true;
    QString errorMessage;
    const bool restored = m_section05->restoreCircuitSnapshot(m_history.at(index), errorMessage);
    m_applyingHistory = false;

    if (!restored)
    {
        QMessageBox::warning(m_window, tr("History"), errorMessage);
        return;
    }

    m_historyIndex = index;
    appendLog(index + 1 < m_history.size() ? tr("Undo/Redo restored circuit history step %1.").arg(index + 1)
                                            : tr("Restored latest circuit history state."));
    updateActionState();
}

void Section10Controller::updateActionState()
{
    const bool active = m_window && m_window->hasActiveProject();
    if (m_saveAction)
        m_saveAction->setEnabled(active);
    if (m_saveAsAction)
        m_saveAsAction->setEnabled(active);
    if (m_exportAction)
        m_exportAction->setEnabled(active);
    if (m_drcAction)
        m_drcAction->setEnabled(active);
    if (m_undoAction)
        m_undoAction->setEnabled(active && m_historyIndex > 0);
    if (m_redoAction)
        m_redoAction->setEnabled(active && m_historyIndex >= 0 && m_historyIndex + 1 < m_history.size());
}

void Section10Controller::runDesignCheck(bool showDialog)
{
    bool blocking = false;
    const QStringList messages = designCheckMessages(blocking);

    QStringList lines;
    lines.append(QStringLiteral("--- DRC %1 ---")
                     .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
    lines += messages;
    for (const QString &line : lines)
        appendLog(line);

    if (!showDialog || !m_window)
        return;

    if (blocking)
    {
        QMessageBox::warning(m_window,
                             tr("Design Check"),
                             tr("The circuit has blocking design errors. See Simulation Log / DRC for details."));
    }
    else
    {
        QMessageBox::information(m_window,
                                 tr("Design Check"),
                                 tr("Design check completed without blocking errors."));
    }
}

QStringList Section10Controller::designCheckMessages(bool &hasBlockingError) const
{
    hasBlockingError = false;
    QStringList result;
    if (!m_section05)
    {
        result.append(tr("ERROR: Circuit model is not available."));
        hasBlockingError = true;
        return result;
    }

    const QList<ComponentItem *> components = m_section05->componentItems();
    const CircuitGraph &graph = m_section05->circuitGraph();

    if (components.isEmpty())
    {
        result.append(tr("ERROR: The circuit is empty; there is nothing to simulate."));
        hasBlockingError = true;
        return result;
    }

    bool hasGround = false;
    QSet<QString> groundNodes;
    for (ComponentItem *component : components)
    {
        if (!component || component->componentType() != QStringLiteral("Ground"))
            continue;
        hasGround = true;
        const QVector<PinModel> pins = component->pins();
        for (int pinIndex = 0; pinIndex < pins.size(); ++pinIndex)
        {
            const QString node = graph.nodeForEndpoint(CircuitGraph::pinEndpoint(component->modelId(), pinIndex));
            if (!node.isEmpty())
                groundNodes.insert(node);
        }
    }

    if (!hasGround)
        result.append(tr("WARNING: No GND reference is placed in the circuit."));

    for (ComponentItem *component : components)
    {
        if (!component)
            continue;
        const QVector<PinModel> pins = component->pins();
        for (int pinIndex = 0; pinIndex < pins.size(); ++pinIndex)
        {
            if (pins.at(pinIndex).direction != PinDirection::Input)
                continue;
            const QString endpoint = CircuitGraph::pinEndpoint(component->modelId(), pinIndex);
            if (graph.nodeForEndpoint(endpoint).isEmpty())
            {
                result.append(tr("ERROR: Floating input detected at %1.%2.")
                                  .arg(component->reference(), pins.at(pinIndex).name));
                hasBlockingError = true;
            }
        }
    }

    for (ComponentItem *component : components)
    {
        if (!component || !sourceType(component->componentType()) || component->pins().size() < 2)
            continue;

        const QString firstNode =
            graph.nodeForEndpoint(CircuitGraph::pinEndpoint(component->modelId(), 0));
        const QString secondNode =
            graph.nodeForEndpoint(CircuitGraph::pinEndpoint(component->modelId(), 1));
        if (firstNode.isEmpty() || firstNode != secondNode)
            continue;

        const QString location = groundNodes.contains(firstNode)
                                     ? tr("the ground node")
                                     : tr("node %1").arg(firstNode);
        result.append(tr("ERROR: Both terminals of %1 are connected to %2 (short circuit).")
                          .arg(component->reference(), location));
        hasBlockingError = true;
    }

    struct DriverInfo
    {
        QString reference;
        QString type;
        std::optional<double> nominalVoltage;
    };
    QHash<QString, QList<DriverInfo>> sourceDrivers;

    for (ComponentItem *component : components)
    {
        if (!component || !sourceType(component->componentType()) || component->pins().isEmpty())
            continue;

        const QString outputNode =
            graph.nodeForEndpoint(CircuitGraph::pinEndpoint(component->modelId(), 0));
        if (outputNode.isEmpty())
            continue;

        const std::optional<double> nominal = sourceNominalVoltage(component);
        if (groundNodes.contains(outputNode) && nominal.has_value() && std::abs(*nominal) > 0.000001)
        {
            result.append(tr("ERROR: %1 output is wired directly to GND (short circuit).")
                              .arg(component->reference()));
            hasBlockingError = true;
        }

        sourceDrivers[outputNode].append(
            DriverInfo{component->reference(), component->componentType(), nominal});
    }

    for (auto nodeIt = sourceDrivers.cbegin(); nodeIt != sourceDrivers.cend(); ++nodeIt)
    {
        const QList<DriverInfo> drivers = nodeIt.value();
        if (drivers.size() < 2)
            continue;

        bool incompatible = false;
        for (int first = 0; first < drivers.size() && !incompatible; ++first)
        {
            for (int second = first + 1; second < drivers.size(); ++second)
            {
                const DriverInfo &a = drivers.at(first);
                const DriverInfo &b = drivers.at(second);
                if (a.type == QStringLiteral("Clock") || b.type == QStringLiteral("Clock"))
                {
                    incompatible = true;
                    break;
                }
                if (!a.nominalVoltage.has_value() || !b.nominalVoltage.has_value() ||
                    std::abs(*a.nominalVoltage - *b.nominalVoltage) > 0.000001)
                {
                    incompatible = true;
                    break;
                }
            }
        }

        if (incompatible)
        {
            QStringList names;
            for (const DriverInfo &driver : drivers)
                names.append(driver.reference);
            result.append(tr("ERROR: Conflicting voltage sources share %1: %2.")
                              .arg(nodeIt.key(), names.join(QStringLiteral(", "))));
            hasBlockingError = true;
        }
    }

    if (m_simulation && m_simulation->simulationState() != Section06Controller::SimulationState::Stopped)
    {
        QSet<QString> conflictNodes;
        for (WireItem *wire : m_section05->wireItems())
        {
            if (!wire)
                continue;
            const auto signal = m_simulation->signalForEndpoint(wire->startEndpoint());
            if (!signal.conflict)
                continue;
            QString node = graph.nodeForEndpoint(wire->startEndpoint());
            if (node.isEmpty())
                node = wire->startEndpoint();
            conflictNodes.insert(node);
        }
        for (const QString &node : conflictNodes)
        {
            result.append(tr("ERROR: Conflicting drivers / short circuit detected on %1.").arg(node));
            hasBlockingError = true;
        }
    }

    if (result.isEmpty())
        result.append(tr("OK: No floating inputs, source shorts or driver conflicts were found."));
    else if (!hasBlockingError)
        result.append(tr("OK: No blocking design error was found."));

    return result;
}

bool Section10Controller::stopForRuntimeConflict()
{
    if (!m_simulation || !m_section05 ||
        m_simulation->simulationState() == Section06Controller::SimulationState::Stopped)
        return false;

    const CircuitGraph &graph = m_section05->circuitGraph();
    for (WireItem *wire : m_section05->wireItems())
    {
        if (!wire)
            continue;
        const auto signal = m_simulation->signalForEndpoint(wire->startEndpoint());
        if (!signal.conflict)
            continue;

        QString node = graph.nodeForEndpoint(wire->startEndpoint());
        if (node.isEmpty())
            node = wire->startEndpoint();
        appendLog(tr("ERROR: Runtime conflict detected on %1. Simulation stopped.").arg(node));
        showStatus(tr("Simulation stopped: conflicting drivers detected."), 5000);
        return true;
    }
    return false;
}

void Section10Controller::appendLog(const QString &message)
{
    if (!m_log || message.isEmpty())
        return;
    m_log->appendPlainText(message);
}

void Section10Controller::replaceLog(const QStringList &lines)
{
    if (!m_log)
        return;
    m_log->setPlainText(lines.join(QLatin1Char('\n')));
}

void Section10Controller::showStatus(const QString &message, int timeout) const
{
    if (m_window && m_window->statusBar())
        m_window->statusBar()->showMessage(message, timeout);
}

QString Section10Controller::simulationStateName(Section06Controller::SimulationState state)
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

QByteArray Section10Controller::compactSnapshot(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

namespace
{
void attachSection10Controller()
{
    bool attached = false;
    const auto windows = QApplication::topLevelWidgets();
    for (QWidget *widget : windows)
    {
        auto *window = qobject_cast<MainWindow *>(widget);
        if (!window || window->property("section10ControllerAttached").toBool())
            continue;

        auto *section05 = window->findChild<Section05Controller *>();
        auto *section06 = window->findChild<Section06Controller *>();
        if (!section05 || !section06)
            continue;

        new Section10Controller(window, section05, section06, window);
        window->setProperty("section10ControllerAttached", true);
        attached = true;
    }

    if (!attached)
        QTimer::singleShot(140, [] { attachSection10Controller(); });
}

void installSection10Controller()
{
    QTimer::singleShot(0, [] { attachSection10Controller(); });
}
}

Q_COREAPP_STARTUP_FUNCTION(installSection10Controller)
