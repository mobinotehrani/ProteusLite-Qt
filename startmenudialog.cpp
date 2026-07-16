#include "startmenudialog.h"

#include <QFont>
#include <QListWidgetItem>
#include <QColor>
#include <QComboBox>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

StartMenuDialog::StartMenuDialog(const QStringList &recentProjects, QWidget *parent)
    : QDialog(parent)
{
    buildInterface(recentProjects);
}

StartMenuDialog::Choice StartMenuDialog::choice() const
{
    return m_choice;
}

QString StartMenuDialog::projectName() const
{
    return m_projectName->text().trimmed();
}

QSize StartMenuDialog::canvasSize() const
{
    return QSize(m_canvasWidth->value(), m_canvasHeight->value());
}

QString StartMenuDialog::selectedRecentProject() const
{
    return m_selectedRecentProject;
}

void StartMenuDialog::buildInterface(const QStringList &recentProjects)
{
    setWindowTitle(tr("ProteusPro - Start Menu"));
    setModal(true);
    resize(920, 590);
    setMinimumSize(820, 520);
    setStyleSheet(
        "QDialog{background:#f8fafc;}"
        "QFrame#Hero{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #1d4ed8,stop:1 #0f172a);"
        "border-radius:18px;}"
        "QFrame#Card{background:white;border:1px solid #dbe3ef;border-radius:16px;}"
        "QLabel{color:#0f172a;}"
        "QLabel#Muted{color:#64748b;}"
        "QPushButton{background:#2563eb;color:white;border:0;border-radius:10px;padding:10px 16px;"
        "font-weight:700;}"
        "QPushButton:hover{background:#1d4ed8;}"
        "QPushButton#Secondary{background:#e2e8f0;color:#0f172a;}"
        "QPushButton#Secondary:hover{background:#cbd5e1;}"
        "QLineEdit,QComboBox,QSpinBox,QListWidget{background:white;color:#0f172a;border:1px solid #cbd5e1;"
        "border-radius:8px;padding:7px;}"
        "QListWidget::item{padding:7px;}"
        "QListWidget::item:selected{background:#dbeafe;color:#1e3a8a;}");

    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(16);

    auto *hero = new QFrame;
    hero->setObjectName(QStringLiteral("Hero"));
    hero->setMinimumWidth(300);
    auto *heroLayout = new QVBoxLayout(hero);
    heroLayout->setContentsMargins(26, 26, 26, 26);
    heroLayout->setSpacing(16);

    auto *brand = new QLabel(
        QStringLiteral("<div style='font-size:36px;font-weight:900;color:white;'>ProteusPro</div>"
                       "<div style='font-size:14px;color:#bfdbfe;'>Object-Oriented Circuit Designer</div>"));
    brand->setTextFormat(Qt::RichText);
    heroLayout->addWidget(brand);

    auto *summary = new QLabel(
        tr("Create a blank circuit project, select the design-sheet size, open a saved project, or continue "
           "from the recent-project list."));
    summary->setWordWrap(true);
    summary->setStyleSheet(QStringLiteral("color:#e0f2fe;font-size:14px;"));
    heroLayout->addWidget(summary);

    auto *features = new QLabel(
        tr("New project and canvas presets\n"
           "JSON/XML project metadata loading\n"
           "Interactive grid canvas with zoom and pan\n"
           "Searchable component library and schematic preview"));
    features->setStyleSheet(QStringLiteral("color:#bfdbfe;font-size:13px;"));
    heroLayout->addWidget(features);
    heroLayout->addStretch();

    auto *tip = new QLabel(
        tr("Sections 1-4 provide the application foundation, project flow, design workspace, and component library."));
    tip->setWordWrap(true);
    tip->setStyleSheet(QStringLiteral("color:#93c5fd;"));
    heroLayout->addWidget(tip);
    rootLayout->addWidget(hero, 1);

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("Card"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(24, 22, 24, 22);
    cardLayout->setSpacing(11);

    auto *title = new QLabel(tr("Start Menu"));
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);
    cardLayout->addWidget(title);

    auto *subtitle = new QLabel(tr("Start a new design or open a previously saved ProteusPro project."));
    subtitle->setObjectName(QStringLiteral("Muted"));
    subtitle->setWordWrap(true);
    cardLayout->addWidget(subtitle);

    cardLayout->addWidget(new QLabel(tr("Project name")));
    m_projectName = new QLineEdit;
    m_projectName->setPlaceholderText(tr("Untitled Project"));
    cardLayout->addWidget(m_projectName);

    auto *sizeRow = new QHBoxLayout;
    m_canvasPreset = new QComboBox;
    m_canvasPreset->addItem(tr("Custom"), QSize(3000, 2000));
    m_canvasPreset->addItem(tr("A4 landscape"), QSize(2338, 1654));
    m_canvasPreset->addItem(tr("A3 landscape"), QSize(3307, 2338));
    m_canvasPreset->addItem(tr("HD sheet"), QSize(1920, 1080));

    m_canvasWidth = new QSpinBox;
    m_canvasWidth->setRange(800, 8000);
    m_canvasWidth->setValue(3000);
    m_canvasWidth->setSuffix(QStringLiteral(" px"));

    m_canvasHeight = new QSpinBox;
    m_canvasHeight->setRange(600, 6000);
    m_canvasHeight->setValue(2000);
    m_canvasHeight->setSuffix(QStringLiteral(" px"));

    sizeRow->addWidget(new QLabel(tr("Canvas")));
    sizeRow->addWidget(m_canvasPreset, 1);
    sizeRow->addWidget(m_canvasWidth);
    sizeRow->addWidget(m_canvasHeight);
    cardLayout->addLayout(sizeRow);

    connect(m_canvasPreset, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) { applySelectedPreset(index); });

    cardLayout->addWidget(new QLabel(tr("Recent projects")));
    m_recentList = new QListWidget;
    m_recentList->setAlternatingRowColors(true);
    m_recentList->setMinimumHeight(190);

    if (recentProjects.isEmpty())
    {
        auto *item = new QListWidgetItem(tr("No recent project yet"));
        item->setFlags(Qt::NoItemFlags);
        item->setForeground(QColor(100, 116, 139));
        m_recentList->addItem(item);
    }
    else
    {
        for (const QString &path : recentProjects)
        {
            const QFileInfo fileInfo(path);
            auto *item = new QListWidgetItem(fileInfo.fileName() + QStringLiteral("\n") + path);
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
            m_recentList->addItem(item);
        }
    }
    cardLayout->addWidget(m_recentList, 1);

    auto *buttonRow = new QHBoxLayout;
    auto *newButton = new QPushButton(tr("New Project"));
    auto *browseButton = new QPushButton(tr("Open File..."));
    browseButton->setObjectName(QStringLiteral("Secondary"));
    auto *recentButton = new QPushButton(tr("Open Selected"));
    recentButton->setObjectName(QStringLiteral("Secondary"));
    auto *closeButton = new QPushButton(tr("Close"));
    closeButton->setObjectName(QStringLiteral("Secondary"));

    buttonRow->addWidget(newButton);
    buttonRow->addWidget(browseButton);
    buttonRow->addWidget(recentButton);
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);
    cardLayout->addLayout(buttonRow);

    connect(newButton, &QPushButton::clicked, this, [this] { chooseNewProject(); });
    connect(browseButton, &QPushButton::clicked, this, [this] { chooseBrowseProject(); });
    connect(recentButton, &QPushButton::clicked, this, [this] { chooseRecentProject(); });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_recentList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *) { chooseRecentProject(); });

    rootLayout->addWidget(card, 2);
}

void StartMenuDialog::applySelectedPreset(int index)
{
    const QSize size = m_canvasPreset->itemData(index).toSize();
    if (!size.isValid())
        return;
    m_canvasWidth->setValue(size.width());
    m_canvasHeight->setValue(size.height());
}

void StartMenuDialog::chooseNewProject()
{
    m_choice = Choice::NewProject;
    accept();
}

void StartMenuDialog::chooseBrowseProject()
{
    m_choice = Choice::BrowseProject;
    accept();
}

void StartMenuDialog::chooseRecentProject()
{
    QListWidgetItem *item = m_recentList->currentItem();
    if (!item)
        return;

    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty())
        return;

    m_selectedRecentProject = path;
    m_choice = Choice::RecentProject;
    accept();
}
