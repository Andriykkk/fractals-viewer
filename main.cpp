#include <QApplication>
#include <QMainWindow>
#include <QStackedWidget>

#include "types.h"
#include "landingpage.h"
#include "fractal2d.h"
#include "fractal3d.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr) : QMainWindow(parent)
    {
        setWindowTitle("Fractal Viewer");
        setMinimumSize(900, 700);

        stackedWidget = new QStackedWidget();
        setCentralWidget(stackedWidget);

        // Create pages
        landingPage = new LandingPage();
        fractal2DPage = new FractalPage2D();
        fractal3DPage = new FractalPage3D();

        stackedWidget->addWidget(landingPage);
        stackedWidget->addWidget(fractal2DPage);
        stackedWidget->addWidget(fractal3DPage);

        // Connect signals
        connect(landingPage->card2D, &OptionCard::clicked, this, [this]() {
            stackedWidget->setCurrentWidget(fractal2DPage);
            fractal2DPage->setFocus();
        });

        connect(landingPage->card3D, &OptionCard::clicked, this, [this]() {
            stackedWidget->setCurrentWidget(fractal3DPage);
            fractal3DPage->setFocus();
        });

        connect(fractal2DPage->sidebar->btnBack, &QPushButton::clicked, this, [this]() {
            stackedWidget->setCurrentWidget(landingPage);
        });

        connect(fractal3DPage->sidebar->btnBack, &QPushButton::clicked, this, [this]() {
            stackedWidget->setCurrentWidget(landingPage);
        });
    }

private:
    QStackedWidget *stackedWidget;
    LandingPage *landingPage;
    FractalPage2D *fractal2DPage;
    FractalPage3D *fractal3DPage;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}

#include "main.moc"
