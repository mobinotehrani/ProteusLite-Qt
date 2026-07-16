#pragma once

#include "projectdocument.h"
#include "recentprojectsmanager.h"

#include <QMainWindow>

class QAction;
class CanvasView;
class ComponentLibraryPanel;
class QComboBox;
class QDockWidget;
class QLabel;
class QMenu;
class QStackedWidget;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void buildInterface();
    void buildWorkspaceActions();
    void buildMenus();
    void buildToolBar();
    void buildStatusBar();
    void buildComponentLibrary();
    void connectCanvasSignals();

    void showStartMenu();
    void createNewProject(const QString &name, const QSize &canvasSize);
    void browseAndOpenProject();
    void openProject(const QString &filePath);
    void openRecentProject();
    void refreshRecentMenu();
    void showProject(const ProjectMetadata &metadata);
    void showNoProject();
    void setWorkspaceActionsEnabled(bool enabled);
    void updateWindowTitle();
    void prepareComponentPlacement(const QString &componentId, const QString &displayName);

    RecentProjectsManager m_recentProjects;
    ProjectMetadata m_currentProject;

    QStackedWidget *m_workspaceStack{};
    QWidget *m_emptyPage{};
    QWidget *m_canvasPage{};
    QLabel *m_projectTitle{};
    QLabel *m_projectDetails{};
    CanvasView *m_canvasView{};
    QMenu *m_recentMenu{};

    QDockWidget *m_libraryDock{};
    ComponentLibraryPanel *m_libraryPanel{};

    QAction *m_gridAction{};
    QAction *m_snapAction{};
    QAction *m_zoomInAction{};
    QAction *m_zoomOutAction{};
    QAction *m_resetZoomAction{};
    QAction *m_fitSheetAction{};
    QComboBox *m_gridSpacingCombo{};

    QLabel *m_coordinateStatus{};
    QLabel *m_snapStatus{};
    QLabel *m_zoomStatus{};
    QLabel *m_panStatus{};
    QLabel *m_toolStatus{};

    QString m_pendingComponentId;
    QString m_pendingComponentName;
};
