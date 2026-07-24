#include "section07controller.h"

#include "componentitem.h"
#include "section05controller.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTimer>

#include <algorithm>

Section07Controller::Section07Controller(QMainWindow *window,
                                         Section05Controller *section05,
                                         QObject *parent)
    : QObject(parent), m_window(window), m_section05(section05)
{
    m_monitorDock = new QDockWidget(tr("Advanced Devices"), m_window);
    m_monitorDock->setObjectName(QStringLiteral("Section07MonitorDock"));
    m_monitorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_monitorDock->setMinimumWidth(360);
    m_monitorDock->setStyleSheet(QStringLiteral(
        "QDockWidget{color:#0f172a;font-weight:700;}"
        "QDockWidget::title{background:#e8eef7;border:1px solid #cbd5e1;"
        "padding:8px 10px;text-align:left;}"));

    m_monitor = new QPlainTextEdit(m_monitorDock);
    m_monitor->setReadOnly(true);
    m_monitor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_monitor->setPlaceholderText(
        tr("ADC, DAC, MCU, memory, LCD and keypad activity appears here."));
    m_monitor->setStyleSheet(QStringLiteral(
        "QPlainTextEdit{background:#0b1220;color:#dbeafe;border:1px solid #1e293b;"
        "border-radius:8px;padding:10px;selection-background-color:#2563eb;"
        "font-family:'Cascadia Mono','Consolas',monospace;font-size:10pt;}"));
    m_monitorDock->setWidget(m_monitor);
    m_window->addDockWidget(Qt::RightDockWidgetArea, m_monitorDock);

    m_timer = new QTimer(this);
    m_timer->setInterval(140);
    connect(m_timer, &QTimer::timeout, this, &Section07Controller::refreshMonitor);
    m_timer->start();
    refreshMonitor();

    if (m_window && m_window->statusBar())
    {
        m_window->statusBar()->showMessage(
            tr("Advanced converters, firmware MCU, memory, LCD and keypad are ready."),
            5000);
    }
}

void Section07Controller::refreshMonitor()
{
    QStringList rows;
    if (m_section05)
    {
        for (ComponentItem *item : m_section05->componentItems())
        {
            if (!item || !isAdvancedDevice(item->componentType()))
                continue;
            rows.append(QStringLiteral("%1  [%2]  %3")
                            .arg(item->reference(),
                                 item->componentType(),
                                 shortened(item->runtimeText())));
        }
    }

    std::sort(rows.begin(),
              rows.end(),
              [](const QString &first, const QString &second)
              { return first.localeAwareCompare(second) < 0; });

    QStringList lines;
    lines.append(tr("ADVANCED DEVICES  |  active: %1").arg(rows.size()));
    lines.append(QString(46, QLatin1Char('-')));
    if (rows.isEmpty())
    {
        lines.append(tr("No advanced device is placed yet."));
        lines.append(QString());
        lines.append(tr("Place an ADC, DAC, MCU, memory, LCD or keypad to inspect it here."));
    }
    else
    {
        lines.append(rows);
    }

    const QString text = lines.join(QLatin1Char('\n'));
    if (text == m_previousText || !m_monitor)
        return;

    m_previousText = text;
    m_monitor->setPlainText(text);
}

bool Section07Controller::isAdvancedDevice(const QString &type)
{
    return type == QStringLiteral("ADC") || type == QStringLiteral("DAC") ||
           type == QStringLiteral("MCU") || type == QStringLiteral("EEPROM") ||
           type == QStringLiteral("LCD16x2") || type == QStringLiteral("Keypad4x4");
}

QString Section07Controller::shortened(const QString &text, int maximumLength)
{
    const QString compact = text.simplified();
    if (compact.size() <= maximumLength)
        return compact;
    return compact.left(std::max(0, maximumLength - 1)) + QChar(0x2026);
}

namespace
{
void attachSection07Controller()
{
    bool attached = false;
    const auto windows = QApplication::topLevelWidgets();
    for (QWidget *widget : windows)
    {
        auto *window = qobject_cast<QMainWindow *>(widget);
        if (!window || window->property("section07ControllerAttached").toBool())
            continue;

        auto *section05 = window->findChild<Section05Controller *>();
        if (!section05)
            continue;

        new Section07Controller(window, section05, window);
        window->setProperty("section07ControllerAttached", true);
        attached = true;
    }

    if (!attached)
        QTimer::singleShot(100, [] { attachSection07Controller(); });
}

void installSection07Controller()
{
    QTimer::singleShot(0, [] { attachSection07Controller(); });
}
}

Q_COREAPP_STARTUP_FUNCTION(installSection07Controller)
