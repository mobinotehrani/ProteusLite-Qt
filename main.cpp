#include "mainwindow.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("ProteusPro_Final"));
    application.setOrganizationName(QStringLiteral("HomeworkMOT"));

    QFont font = application.font();
    font.setPointSize(10);
    application.setFont(font);

    MainWindow mainWindow;
    mainWindow.show();

    return application.exec();
}
