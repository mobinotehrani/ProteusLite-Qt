#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class SchematicPreviewWidget;

class ComponentLibraryPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ComponentLibraryPanel(QWidget *parent = nullptr);

    void setPlacementEnabled(bool enabled);
    QString selectedComponentId() const;

signals:
    void placementRequested(const QString &componentId, const QString &displayName);
    void statusMessageRequested(const QString &message);

private:
    void buildInterface();
    void rebuildTree();
    void selectTreeItem(QTreeWidgetItem *item);
    void updateSelection(const QString &componentId);
    void addSelectedToActive();
    void removeCurrentActive();
    void placeSelectedComponent();
    void placeActiveComponent(QListWidgetItem *item);
    bool activeListContains(const QString &componentId) const;

    QLineEdit *m_searchEdit{};
    QLabel *m_resultLabel{};
    QTreeWidget *m_componentTree{};
    SchematicPreviewWidget *m_preview{};
    QLabel *m_selectedInfo{};
    QPushButton *m_addActiveButton{};
    QPushButton *m_placeButton{};
    QListWidget *m_activeList{};
    QPushButton *m_placeActiveButton{};
    QPushButton *m_removeActiveButton{};
    QString m_selectedComponentId;
    bool m_placementEnabled{false};
};
