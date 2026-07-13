#include "mainwindow.h"

#include "canvasview.h"
#include "startmenudialog.h"

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1280, 820);
    setMinimumSize(960, 640);

    buildInterface();
    buildWorkspaceActions();
    buildMenus();
    buildToolBar();
    buildStatusBar();
    connectCanvasSignals();
    showNoProject();

    QTimer::singleShot(0, this, [this] { showStartMenu(); });
}

void MainWindow::buildInterface()
{
    setStyleSheet(
        "QMainWindow{background:#e9eef5;}"
        "QFrame#WorkspaceCard{background:white;border:1px solid #d5deea;border-radius:14px;}"
        "QFrame#CanvasFrame{background:#dce3ec;border:1px solid #cbd5e1;border-radius:10px;}"
        "QLabel#ProjectTitle{color:#0f172a;}"
        "QLabel#ProjectDetails{color:#64748b;}"
        "QLabel#EmptyTitle{color:#0f172a;}"
        "QLabel#EmptyHint{color:#64748b;}"
        "QMenuBar{background:white;border-bottom:1px solid #dbe3ef;}"
        "QMenuBar::item:selected{background:#dbeafe;}"
        "QMenu{background:white;border:1px solid #cbd5e1;}"
        "QMenu::item:selected{background:#dbeafe;color:#1e3a8a;}"
        "QToolBar{background:white;border:0;border-bottom:1px solid #dbe3ef;padding:5px;spacing:4px;}"
        "QToolButton{background:#f8fafc;color:#0f172a;border:1px solid #dbe3ef;border-radius:7px;padding:6px 10px;}"
        "QToolButton:hover{background:#e0ecff;border-color:#93c5fd;}"
        "QToolButton:checked{background:#dbeafe;color:#1d4ed8;border-color:#60a5fa;}"
        "QStatusBar{background:white;border-top:1px solid #dbe3ef;color:#475569;}"
        "QStatusBar QLabel{padding:0 7px;color:#475569;}");

    auto *root = new QWidget;
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(18, 16, 18, 18);

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("WorkspaceCard"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(18, 16, 18, 18);
    cardLayout->setSpacing(8);

    m_projectTitle = new QLabel;
    m_projectTitle->setObjectName(QStringLiteral("ProjectTitle"));
    QFont titleFont = m_projectTitle->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    m_projectTitle->setFont(titleFont);
    cardLayout->addWidget(m_projectTitle);

    m_projectDetails = new QLabel;
    m_projectDetails->setObjectName(QStringLiteral("ProjectDetails"));
    m_projectDetails->setWordWrap(true);
    cardLayout->addWidget(m_projectDetails);

    m_workspaceStack = new QStackedWidget;

    m_emptyPage = new QWidget;
    auto *emptyLayout = new QVBoxLayout(m_emptyPage);
    emptyLayout->setContentsMargins(30, 30, 30, 30);
    emptyLayout->addStretch();

    auto *emptyTitle = new QLabel(tr("Create or open a project to activate the design canvas"));
    emptyTitle->setObjectName(QStringLiteral("EmptyTitle"));
    emptyTitle->setAlignment(Qt::AlignCenter);
    QFont emptyTitleFont = emptyTitle->font();
    emptyTitleFont.setPointSize(17);
    emptyTitleFont.setBold(true);
    emptyTitle->setFont(emptyTitleFont);
    emptyLayout->addWidget(emptyTitle);

    auto *emptyHint = new QLabel(
        tr("Section 3 provides a grid-based sheet, live coordinates, snap preview, mouse-wheel zoom, "
           "middle-button/Space panning, and workspace view controls."));
    emptyHint->setObjectName(QStringLiteral("EmptyHint"));
    emptyHint->setAlignment(Qt::AlignCenter);
    emptyHint->setWordWrap(true);
    emptyLayout->addWidget(emptyHint);
    emptyLayout->addStretch();

    m_canvasPage = new QFrame;
    m_canvasPage->setObjectName(QStringLiteral("CanvasFrame"));
    auto *canvasLayout = new QVBoxLayout(m_canvasPage);
    canvasLayout->setContentsMargins(1, 1, 1, 1);
    m_canvasView = new CanvasView;
    canvasLayout->addWidget(m_canvasView);

    m_workspaceStack->addWidget(m_emptyPage);
    m_workspaceStack->addWidget(m_canvasPage);
    cardLayout->addWidget(m_workspaceStack, 1);

    rootLayout->addWidget(card, 1);
    setCentralWidget(root);
}

void MainWindow::buildWorkspaceActions()
{
    m_gridAction = new QAction(tr("Grid"), this);
    m_gridAction->setCheckable(true);
    m_gridAction->setChecked(true);
    m_gridAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
    connect(m_gridAction, &QAction::toggled, m_canvasView, &CanvasView::setGridVisible);

    m_snapAction = new QAction(tr("Snap"), this);
    m_snapAction->setCheckable(true);
    m_snapAction->setChecked(true);
    m_snapAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+G")));
    connect(m_snapAction, &QAction::toggled, m_canvasView, &CanvasView::setSnapEnabled);

    m_zoomInAction = new QAction(tr("Zoom In"), this);
    m_zoomInAction->setShortcuts(QList<QKeySequence>{QKeySequence(QKeySequence::ZoomIn),
                                                     QKeySequence(QStringLiteral("Ctrl+="))});
    connect(m_zoomInAction, &QAction::triggered, m_canvasView, &CanvasView::zoomIn);

    m_zoomOutAction = new QAction(tr("Zoom Out"), this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, m_canvasView, &CanvasView::zoomOut);

    m_resetZoomAction = new QAction(tr("100%"), this);
    m_resetZoomAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    connect(m_resetZoomAction, &QAction::triggered, m_canvasView, &CanvasView::resetZoom);

    m_fitSheetAction = new QAction(tr("Fit Sheet"), this);
    m_fitSheetAction->setShortcut(QKeySequence(QStringLiteral("F")));
    connect(m_fitSheetAction, &QAction::triggered, m_canvasView, &CanvasView::fitSheet);
}

void MainWindow::buildMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("File"));

    QAction *newAction = fileMenu->addAction(tr("New Project..."));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this] { showStartMenu(); });

    QAction *openAction = fileMenu->addAction(tr("Open Project..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this] { browseAndOpenProject(); });

    m_recentMenu = fileMenu->addMenu(tr("Recent Projects"));
    refreshRecentMenu();

    QAction *startMenuAction = fileMenu->addAction(tr("Show Start Menu"));
    connect(startMenuAction, &QAction::triggered, this, [this] { showStartMenu(); });

    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction(tr("Exit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *viewMenu = menuBar()->addMenu(tr("View"));
    viewMenu->addAction(m_gridAction);
    viewMenu->addAction(m_snapAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_resetZoomAction);
    viewMenu->addAction(m_fitSheetAction);
}

void MainWindow::buildToolBar()
{
    auto *toolBar = addToolBar(tr("Canvas Tools"));
    toolBar->setObjectName(QStringLiteral("CanvasTools"));
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    toolBar->addAction(m_gridAction);
    toolBar->addAction(m_snapAction);
    toolBar->addSeparator();
    toolBar->addAction(m_zoomInAction);
    toolBar->addAction(m_zoomOutAction);
    toolBar->addAction(m_resetZoomAction);
    toolBar->addAction(m_fitSheetAction);
}

void MainWindow::buildStatusBar()
{
    statusBar()->showMessage(tr("Ready"));

    m_panStatus = new QLabel(tr("Pan: middle mouse or Space + drag"));
    m_coordinateStatus = new QLabel(tr("X: —   Y: —"));
    m_snapStatus = new QLabel(tr("Snap: 20 px"));
    m_zoomStatus = new QLabel(tr("Zoom: 100%"));

    statusBar()->addPermanentWidget(m_panStatus, 1);
    statusBar()->addPermanentWidget(m_coordinateStatus);
    statusBar()->addPermanentWidget(m_snapStatus);
    statusBar()->addPermanentWidget(m_zoomStatus);
}

void MainWindow::connectCanvasSignals()
{
    connect(m_canvasView, &CanvasView::cursorPositionChanged, this,
            [this](const QPointF &raw, const QPointF &snapped)
            {
                m_coordinateStatus->setText(
                    tr("X: %1   Y: %2").arg(qRound(raw.x())).arg(qRound(raw.y())));

                if (m_canvasView->isSnapEnabled())
                {
                    m_snapStatus->setText(
                        tr("Snap: %1, %2").arg(qRound(snapped.x())).arg(qRound(snapped.y())));
                }
                else
                {
                    m_snapStatus->setText(tr("Snap: Off"));
                }
            });

    connect(m_canvasView, &CanvasView::cursorLeftCanvas, this,
            [this]
            {
                m_coordinateStatus->setText(tr("X: —   Y: —"));
                m_snapStatus->setText(m_canvasView->isSnapEnabled() ? tr("Snap: 20 px") : tr("Snap: Off"));
            });

    connect(m_canvasView, &CanvasView::zoomChanged, this,
            [this](double factor)
            {
                m_zoomStatus->setText(tr("Zoom: %1%").arg(qRound(factor * 100.0)));
            });

    connect(m_canvasView, &CanvasView::panningChanged, this,
            [this](bool active)
            {
                m_panStatus->setText(active ? tr("Panning canvas...")
                                            : tr("Pan: middle mouse or Space + drag"));
            });

    connect(m_snapAction, &QAction::toggled, this,
            [this](bool enabled)
            {
                m_snapStatus->setText(enabled ? tr("Snap: 20 px") : tr("Snap: Off"));
            });
}

void MainWindow::showStartMenu()
{
    StartMenuDialog dialog(m_recentProjects.projects(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    switch (dialog.choice())
    {
    case StartMenuDialog::Choice::NewProject:
        createNewProject(dialog.projectName(), dialog.canvasSize());
        break;
    case StartMenuDialog::Choice::BrowseProject:
        browseAndOpenProject();
        break;
    case StartMenuDialog::Choice::RecentProject:
        openProject(dialog.selectedRecentProject());
        break;
    case StartMenuDialog::Choice::None:
        break;
    }
}

void MainWindow::createNewProject(const QString &name, const QSize &canvasSize)
{
    m_currentProject = ProjectDocument::createNew(name, canvasSize);
    showProject(m_currentProject);
    statusBar()->showMessage(
        tr("New project created: %1 x %2").arg(canvasSize.width()).arg(canvasSize.height()), 3500);
}

void MainWindow::browseAndOpenProject()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open Project"),
        {},
        tr("ProteusPro Project (*.json *.xml);;JSON Project (*.json);;XML Project (*.xml)"));

    if (!filePath.isEmpty())
        openProject(filePath);
}

void MainWindow::openProject(const QString &filePath)
{
    ProjectMetadata metadata;
    QString errorMessage;
    if (!ProjectDocument::loadMetadata(filePath, metadata, errorMessage))
    {
        QMessageBox::warning(this, tr("Open Project"), errorMessage);
        return;
    }

    m_currentProject = metadata;
    m_recentProjects.add(filePath);
    refreshRecentMenu();
    showProject(metadata);
    statusBar()->showMessage(tr("Opened %1").arg(QFileInfo(filePath).fileName()), 3500);
}

void MainWindow::openRecentProject()
{
    const QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;

    const QString filePath = action->data().toString();
    if (!filePath.isEmpty())
        openProject(filePath);
}

void MainWindow::refreshRecentMenu()
{
    if (!m_recentMenu)
        return;

    m_recentMenu->clear();
    const QStringList projects = m_recentProjects.projects();
    if (projects.isEmpty())
    {
        QAction *emptyAction = m_recentMenu->addAction(tr("No recent projects"));
        emptyAction->setEnabled(false);
        return;
    }

    for (const QString &filePath : projects)
    {
        QAction *action = m_recentMenu->addAction(QFileInfo(filePath).fileName());
        action->setData(filePath);
        action->setToolTip(filePath);
        connect(action, &QAction::triggered, this, &MainWindow::openRecentProject);
    }

    m_recentMenu->addSeparator();
    QAction *clearAction = m_recentMenu->addAction(tr("Clear Recent Projects"));
    connect(clearAction, &QAction::triggered, this,
            [this]
            {
                m_recentProjects.clear();
                refreshRecentMenu();
            });
}

void MainWindow::showProject(const ProjectMetadata &metadata)
{
    m_projectTitle->setText(metadata.name);

    QString details = tr("Canvas: %1 x %2 px")
                          .arg(metadata.canvasSize.width())
                          .arg(metadata.canvasSize.height());

    if (metadata.openedFromFile)
    {
        details += tr("   |   Components in file: %1   |   Wires in file: %2")
                       .arg(metadata.componentCount)
                       .arg(metadata.wireCount);
        details += QStringLiteral("\n") + metadata.filePath;
    }
    else
    {
        details += tr("   |   Unsaved project");
    }

    m_projectDetails->setText(details);
    m_canvasView->setCanvasSize(metadata.canvasSize);
    m_workspaceStack->setCurrentWidget(m_canvasPage);
    setWorkspaceActionsEnabled(true);
    updateWindowTitle();

    QTimer::singleShot(0, m_canvasView, &CanvasView::fitSheet);
}

void MainWindow::showNoProject()
{
    m_currentProject = ProjectMetadata{};
    m_projectTitle->setText(tr("No project selected"));
    m_projectDetails->setText(tr("Use File > New Project or File > Open Project to begin."));
    m_workspaceStack->setCurrentWidget(m_emptyPage);
    setWorkspaceActionsEnabled(false);
    updateWindowTitle();
}

void MainWindow::setWorkspaceActionsEnabled(bool enabled)
{
    m_gridAction->setEnabled(enabled);
    m_snapAction->setEnabled(enabled);
    m_zoomInAction->setEnabled(enabled);
    m_zoomOutAction->setEnabled(enabled);
    m_resetZoomAction->setEnabled(enabled);
    m_fitSheetAction->setEnabled(enabled);

    if (!enabled)
    {
        m_coordinateStatus->setText(tr("X: —   Y: —"));
        m_snapStatus->setText(tr("Snap: —"));
        m_zoomStatus->setText(tr("Zoom: —"));
    }
}

void MainWindow::updateWindowTitle()
{
    if (m_workspaceStack->currentWidget() == m_canvasPage && !m_currentProject.name.isEmpty())
        setWindowTitle(tr("%1 - ProteusPro").arg(m_currentProject.name));
    else
        setWindowTitle(tr("ProteusPro - Circuit Designer"));
}
