#pragma once

#include "projectdocument.h"
#include "recentprojectsmanager.h"

#include <QMainWindow>

class QAction;
class QLabel;
class QMenu;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void buildInterface();
    void buildMenus();
    void showStartMenu();
    void createNewProject(const QString &name, const QSize &canvasSize);
    void browseAndOpenProject();
    void openProject(const QString &filePath);
    void openRecentProject();
    void refreshRecentMenu();
    void showProject(const ProjectMetadata &metadata);
    void showNoProject();

    RecentProjectsManager m_recentProjects;
    ProjectMetadata m_currentProject;
    QWidget *m_workspace{};
    QLabel *m_projectTitle{};
    QLabel *m_projectDetails{};
    QLabel *m_canvasPlaceholder{};
    QMenu *m_recentMenu{};
};
