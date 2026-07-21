#pragma once

#include <QObject>
#include <QString>

class QDockWidget;
class QMainWindow;
class QPlainTextEdit;
class QTimer;
class Section05Controller;

class Section07Controller final : public QObject
{
    Q_OBJECT

  public:
    Section07Controller(QMainWindow *window, Section05Controller *section05, QObject *parent = nullptr);

  private:
    void refreshMonitor();
    static bool isSection07Component(const QString &type);
    static QString shortened(const QString &text, int maximumLength = 110);

    QMainWindow *m_window{};
    Section05Controller *m_section05{};
    QDockWidget *m_monitorDock{};
    QPlainTextEdit *m_monitor{};
    QTimer *m_timer{};
    QString m_previousText;
};
