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
    m_monitorDock = new QDockWidget(tr("Section 7 Monitor"), m_window);
    m_monitorDock->setObjectName(QStringLiteral("Section07MonitorDock"));
    m_monitorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_monitorDock->setMinimumWidth(350);

    m_monitor = new QPlainTextEdit(m_monitorDock);
    m_monitor->setReadOnly(true);
    m_monitor->setPlaceholderText(
        tr("Place an ADC, DAC, MCU, memory, LCD or keypad to view its live state."));
    m_monitor->setStyleSheet(
        QStringLiteral("QPlainTextEdit{background:#111827;color:#e5e7eb;border:0;padding:8px;"
                       "font-family:Consolas,monospace;font-size:10pt;}"));
    m_monitorDock->setWidget(m_monitor);
    m_window->addDockWidget(Qt::RightDockWidgetArea, m_monitorDock);

    m_timer = new QTimer(this);
    m_timer->setInterval(120);
    connect(m_timer, &QTimer::timeout, this, &Section07Controller::refreshMonitor);
    m_timer->start();
    refreshMonitor();

    if (m_window && m_window->statusBar())
    {
        m_window->statusBar()->showMessage(
            tr("Section 7 ready: converters, firmware MCU, external memory, LCD and keypad are available."),
            6500);
    }
}

void Section07Controller::refreshMonitor()
{
    QStringList rows;
    if (m_section05)
    {
        for (ComponentItem *item : m_section05->componentItems())
        {
            if (!item || !isSection07Component(item->componentType()))
                continue;

            rows.append(QStringLiteral("%1 [%2]  %3")
                            .arg(item->reference(),
                                 item->componentType(),
                                 shortened(item->runtimeText())));
        }
    }

    std::sort(rows.begin(), rows.end(), [](const QString &first, const QString &second)
              { return first.localeAwareCompare(second) < 0; });

    if (rows.isEmpty())
        rows.append(tr("No Section 7 component is placed yet."));

    const QString text = rows.join(QLatin1Char('\n'));
    if (text == m_previousText || !m_monitor)
        return;

    m_previousText = text;
    m_monitor->setPlainText(text);
}

bool Section07Controller::isSection07Component(const QString &type)
{
    return type == QStringLiteral("ADC") || type == QStringLiteral("DAC") ||
           type == QStringLiteral("MCU") || type == QStringLiteral("EEPROM") ||
           type == QStringLiteral("LCD16x2") || type == QStringLiteral("Keypad4x4");
}

QString Section07Controller::shortened(const QString &text, int maximumLength)
{
    if (text.size() <= maximumLength)
        return text;
    return text.left(std::max(0, maximumLength - 1)) + QChar(0x2026);
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
