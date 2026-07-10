#include <QtWidgets>

class MainWindow final : public QMainWindow
{
  public:
    MainWindow()
    {
        setWindowTitle("ProteusPro - Circuit Designer");
        resize(1200, 800);
        setMinimumSize(900, 600);

        auto *workspace = new QWidget(this);
        auto *layout = new QVBoxLayout(workspace);
        auto *title = new QLabel("ProteusPro workspace", workspace);

        title->setAlignment(Qt::AlignCenter);
        layout->addWidget(title);
        setCentralWidget(workspace);

        statusBar()->showMessage("Ready");
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("ProteusPro_Final");
    QApplication::setOrganizationName("HomeworkMOT");

    MainWindow window;
    window.show();

    return app.exec();
}
