#pragma once

#include <QDialog>
#include <QSize>
#include <QStringList>

class QComboBox;
class QLineEdit;
class QListWidget;
class QSpinBox;

class StartMenuDialog final : public QDialog
{
public:
    enum class Choice
    {
        None,
        NewProject,
        BrowseProject,
        RecentProject
    };

    explicit StartMenuDialog(const QStringList &recentProjects, QWidget *parent = nullptr);

    Choice choice() const;
    QString projectName() const;
    QSize canvasSize() const;
    QString selectedRecentProject() const;

private:
    void buildInterface(const QStringList &recentProjects);
    void applySelectedPreset(int index);
    void chooseNewProject();
    void chooseBrowseProject();
    void chooseRecentProject();

    Choice m_choice{Choice::None};
    QLineEdit *m_projectName{};
    QComboBox *m_canvasPreset{};
    QSpinBox *m_canvasWidth{};
    QSpinBox *m_canvasHeight{};
    QListWidget *m_recentList{};
    QString m_selectedRecentProject;
};
