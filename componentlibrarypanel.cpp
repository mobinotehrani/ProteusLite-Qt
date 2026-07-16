#include "componentlibrarypanel.h"

#include "componentdefinition.h"
#include "schematicpreviewwidget.h"

#include <QColor>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMap>
#include <QPushButton>
#include <QSizePolicy>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

ComponentLibraryPanel::ComponentLibraryPanel(QWidget *parent)
    : QWidget(parent)
{
    buildInterface();
    rebuildTree();
    setPlacementEnabled(false);
}

void ComponentLibraryPanel::setPlacementEnabled(bool enabled)
{
    m_placementEnabled = enabled;
    m_placeButton->setEnabled(enabled && !m_selectedComponentId.isEmpty());
    m_placeActiveButton->setEnabled(enabled && m_activeList->currentItem() != nullptr);
}

QString ComponentLibraryPanel::selectedComponentId() const
{
    return m_selectedComponentId;
}

void ComponentLibraryPanel::buildInterface()
{
    setMinimumWidth(300);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setStyleSheet(
        "QWidget{color:#0f172a;}"
        "QLineEdit,QTreeWidget,QListWidget{background:white;border:1px solid #cbd5e1;border-radius:8px;"
        "selection-background-color:#dbeafe;selection-color:#1e3a8a;}"
        "QLineEdit{padding:7px;}"
        "QTreeWidget::item,QListWidget::item{padding:4px;}"
        "QPushButton{background:#2563eb;color:white;border:0;border-radius:8px;padding:7px 9px;font-weight:700;}"
        "QPushButton:hover{background:#1d4ed8;}"
        "QPushButton:disabled{background:#cbd5e1;color:#64748b;}"
        "QPushButton#Secondary{background:#e2e8f0;color:#0f172a;}"
        "QPushButton#Secondary:hover{background:#cbd5e1;}");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(tr("Search by name or category..."));
    layout->addWidget(m_searchEdit);

    m_resultLabel = new QLabel;
    m_resultLabel->setStyleSheet(QStringLiteral("color:#64748b;font-size:11px;"));
    layout->addWidget(m_resultLabel);

    m_componentTree = new QTreeWidget;
    m_componentTree->setHeaderHidden(true);
    m_componentTree->setAlternatingRowColors(true);
    m_componentTree->setMinimumHeight(145);
    m_componentTree->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_componentTree, 3);

    auto *libraryButtons = new QHBoxLayout;
    libraryButtons->setSpacing(6);
    m_placeButton = new QPushButton(tr("Place"));
    m_addActiveButton = new QPushButton(tr("Add Active"));
    m_addActiveButton->setObjectName(QStringLiteral("Secondary"));
    libraryButtons->addWidget(m_placeButton);
    libraryButtons->addWidget(m_addActiveButton);
    layout->addLayout(libraryButtons);

    m_preview = new SchematicPreviewWidget;
    layout->addWidget(m_preview);

    m_selectedInfo = new QLabel(tr("No component selected"));
    m_selectedInfo->setWordWrap(true);
    m_selectedInfo->setMinimumHeight(30);
    m_selectedInfo->setStyleSheet(
        QStringLiteral("background:#f8fafc;border:1px solid #e2e8f0;border-radius:7px;"
                       "padding:5px;color:#475569;font-size:11px;"));
    layout->addWidget(m_selectedInfo);

    auto *activeTitle = new QLabel(tr("Active project parts"));
    QFont activeFont = activeTitle->font();
    activeFont.setBold(true);
    activeTitle->setFont(activeFont);
    layout->addWidget(activeTitle);

    m_activeList = new QListWidget;
    m_activeList->setAlternatingRowColors(true);
    m_activeList->setMinimumHeight(85);
    m_activeList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_activeList, 2);

    auto *activeButtons = new QHBoxLayout;
    activeButtons->setSpacing(6);
    m_placeActiveButton = new QPushButton(tr("Place Active"));
    m_removeActiveButton = new QPushButton(tr("Remove Active"));
    m_removeActiveButton->setObjectName(QStringLiteral("Secondary"));
    m_placeActiveButton->setEnabled(false);
    m_removeActiveButton->setEnabled(false);
    activeButtons->addWidget(m_placeActiveButton);
    activeButtons->addWidget(m_removeActiveButton);
    layout->addLayout(activeButtons);

    connect(m_searchEdit, &QLineEdit::textChanged, this,
            [this] { rebuildTree(); });
    connect(m_componentTree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *current, QTreeWidgetItem *) { selectTreeItem(current); });
    connect(m_componentTree, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem *item, int)
            {
                selectTreeItem(item);
                placeSelectedComponent();
            });
    connect(m_addActiveButton, &QPushButton::clicked, this,
            [this] { addSelectedToActive(); });
    connect(m_placeButton, &QPushButton::clicked, this,
            [this] { placeSelectedComponent(); });
    connect(m_activeList, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item)
            {
                const bool hasItem = item != nullptr;
                m_removeActiveButton->setEnabled(hasItem);
                m_placeActiveButton->setEnabled(m_placementEnabled && hasItem);
            });
    connect(m_activeList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *item) { placeActiveComponent(item); });
    connect(m_placeActiveButton, &QPushButton::clicked, this,
            [this] { placeActiveComponent(m_activeList->currentItem()); });
    connect(m_removeActiveButton, &QPushButton::clicked, this,
            [this] { removeCurrentActive(); });
}

void ComponentLibraryPanel::rebuildTree()
{
    const QList<ComponentDefinition> matches = ComponentCatalog::filtered(m_searchEdit->text());

    m_componentTree->clear();
    updateSelection({});

    QMap<QString, QTreeWidgetItem *> categoryItems;
    for (const ComponentDefinition &definition : matches)
    {
        QTreeWidgetItem *categoryItem = categoryItems.value(definition.category, nullptr);
        if (!categoryItem)
        {
            categoryItem = new QTreeWidgetItem(QStringList(definition.category));
            categoryItem->setData(0, Qt::UserRole, QString());
            QFont categoryFont = categoryItem->font(0);
            categoryFont.setBold(true);
            categoryItem->setFont(0, categoryFont);
            categoryItem->setForeground(0, QColor(30, 64, 175));
            m_componentTree->addTopLevelItem(categoryItem);
            categoryItems.insert(definition.category, categoryItem);
        }

        auto *componentItem = new QTreeWidgetItem(QStringList(definition.displayName));
        componentItem->setData(0, Qt::UserRole, definition.id);
        componentItem->setToolTip(0, definition.description);
        categoryItem->addChild(componentItem);
    }

    if (matches.isEmpty())
    {
        m_resultLabel->setText(tr("No component matches the current search."));
        return;
    }

    m_resultLabel->setText(
        tr("%1 component(s) in %2 category group(s)").arg(matches.size()).arg(categoryItems.size()));
    m_componentTree->expandAll();

    QTreeWidgetItem *firstCategory = m_componentTree->topLevelItem(0);
    if (firstCategory && firstCategory->childCount() > 0)
        m_componentTree->setCurrentItem(firstCategory->child(0));
}

void ComponentLibraryPanel::selectTreeItem(QTreeWidgetItem *item)
{
    if (!item)
    {
        updateSelection({});
        return;
    }

    updateSelection(item->data(0, Qt::UserRole).toString());
}

void ComponentLibraryPanel::updateSelection(const QString &componentId)
{
    const ComponentDefinition *definition = ComponentCatalog::find(componentId);
    if (!definition)
    {
        m_selectedComponentId.clear();
        m_preview->setComponent(nullptr);
        m_selectedInfo->setText(tr("No component selected"));
        m_selectedInfo->setToolTip(QString());
        m_addActiveButton->setEnabled(false);
        m_placeButton->setEnabled(false);
        return;
    }

    m_selectedComponentId = definition->id;
    m_preview->setComponent(definition);
    m_selectedInfo->setText(
        tr("%1  |  %2\nDefault: %3")
            .arg(definition->displayName)
            .arg(definition->category)
            .arg(definition->defaultValue));
    m_selectedInfo->setToolTip(definition->description);
    m_addActiveButton->setEnabled(true);
    m_placeButton->setEnabled(m_placementEnabled);
}

void ComponentLibraryPanel::addSelectedToActive()
{
    const ComponentDefinition *definition = ComponentCatalog::find(m_selectedComponentId);
    if (!definition)
        return;

    if (activeListContains(definition->id))
    {
        emit statusMessageRequested(
            tr("%1 is already in the active-parts list.").arg(definition->displayName));
        return;
    }

    auto *item = new QListWidgetItem(definition->displayName);
    item->setData(Qt::UserRole, definition->id);
    item->setToolTip(definition->description);
    m_activeList->addItem(item);
    m_activeList->setCurrentItem(item);
    m_removeActiveButton->setEnabled(true);
    m_placeActiveButton->setEnabled(m_placementEnabled);
    emit statusMessageRequested(
        tr("Added %1 to active project parts.").arg(definition->displayName));
}

void ComponentLibraryPanel::removeCurrentActive()
{
    const int row = m_activeList->currentRow();
    if (row < 0)
        return;

    QListWidgetItem *item = m_activeList->takeItem(row);
    const QString name = item ? item->text() : QString();
    delete item;

    const bool hasSelection = m_activeList->currentItem() != nullptr;
    m_removeActiveButton->setEnabled(hasSelection);
    m_placeActiveButton->setEnabled(m_placementEnabled && hasSelection);

    if (!name.isEmpty())
        emit statusMessageRequested(tr("Removed %1 from active project parts.").arg(name));
}

void ComponentLibraryPanel::placeSelectedComponent()
{
    const ComponentDefinition *definition = ComponentCatalog::find(m_selectedComponentId);
    if (!definition)
        return;

    if (!m_placementEnabled)
    {
        emit statusMessageRequested(
            tr("Create or open a project before preparing a component for placement."));
        return;
    }

    emit placementRequested(definition->id, definition->displayName);
}

void ComponentLibraryPanel::placeActiveComponent(QListWidgetItem *item)
{
    if (!item)
        return;

    const ComponentDefinition *definition =
        ComponentCatalog::find(item->data(Qt::UserRole).toString());
    if (!definition)
        return;

    updateSelection(definition->id);

    if (!m_placementEnabled)
    {
        emit statusMessageRequested(
            tr("Create or open a project before preparing a component for placement."));
        return;
    }

    emit placementRequested(definition->id, definition->displayName);
}

bool ComponentLibraryPanel::activeListContains(const QString &componentId) const
{
    for (int row = 0; row < m_activeList->count(); ++row)
    {
        QListWidgetItem *item = m_activeList->item(row);
        if (item && item->data(Qt::UserRole).toString() == componentId)
            return true;
    }
    return false;
}
