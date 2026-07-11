#include "mainwindow.h"

#include "startmenudialog.h"

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("ProteusPro - Circuit Designer"));
    resize(1200, 800);
    setMinimumSize(900, 620);
    buildInterface();
    buildMenus();
    showNoProject();
    QTimer::singleShot(0, this, [this] { showStartMenu(); });
}

void MainWindow::buildInterface()
{
    setStyleSheet(
        "QMainWindow{background:#eef2f7;}"
        "QFrame#WorkspaceCard{background:white;border:1px solid #d8e0eb;border-radius:16px;}"
        "QLabel#ProjectTitle{color:#0f172a;}"
        "QLabel#ProjectDetails{color:#64748b;}"
        "QLabel#CanvasPlaceholder{background:#f8fafc;border:2px dashed #94a3b8;border-radius:12px;"
        "color:#475569;padding:30px;}"
        "QMenuBar{background:white;border-bottom:1px solid #dbe3ef;}"
        "QMenuBar::item:selected{background:#dbeafe;}"
        "QMenu{background:white;border:1px solid #cbd5e1;}"
        "QMenu::item:selected{background:#dbeafe;color:#1e3a8a;}");

    auto *root = new QWidget;
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(28, 24, 28, 28);

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("WorkspaceCard"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 24, 28, 28);
    cardLayout->setSpacing(10);

    m_projectTitle = new QLabel;
    m_projectTitle->setObjectName(QStringLiteral("ProjectTitle"));
    QFont titleFont = m_projectTitle->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    m_projectTitle->setFont(titleFont);
    cardLayout->addWidget(m_projectTitle);

    m_projectDetails = new QLabel;
    m_projectDetails->setObjectName(QStringLiteral("ProjectDetails"));
    m_projectDetails->setWordWrap(true);
    cardLayout->addWidget(m_projectDetails);

    m_canvasPlaceholder = new QLabel;
    m_canvasPlaceholder->setObjectName(QStringLiteral("CanvasPlaceholder"));
    m_canvasPlaceholder->setAlignment(Qt::AlignCenter);
    m_canvasPlaceholder->setMinimumHeight(390);
    m_canvasPlaceholder->setWordWrap(true);
    cardLayout->addWidget(m_canvasPlaceholder, 1);

    rootLayout->addWidget(card, 1);
    m_workspace = root;
    setCentralWidget(m_workspace);
    statusBar()->showMessage(tr("Ready"));
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
        details += tr("   |   Components: %1   |   Wires: %2")
                       .arg(metadata.componentCount)
                       .arg(metadata.wireCount);
        details += QStringLiteral("\n") + metadata.filePath;
    }
    else
    {
        details += tr("   |   Unsaved project");
    }
    m_projectDetails->setText(details);

    m_canvasPlaceholder->setText(
        tr("Project workspace is ready.\n\n"
           "The selected sheet size and project metadata are now available to the application.\n"
           "Grid, zoom, panning, coordinates, and circuit editing are implemented in Section 3 and later sections."));
}

void MainWindow::showNoProject()
{
    m_projectTitle->setText(tr("No project selected"));
    m_projectDetails->setText(tr("Use File > New Project or File > Open Project to begin."));
    m_canvasPlaceholder->setText(
        tr("ProteusPro workspace\n\n"
           "Section 2 adds the start menu, new-project flow, canvas-size selection, project opening, and recent files."));
}
