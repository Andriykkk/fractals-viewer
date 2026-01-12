#ifndef LANDINGPAGE_H
#define LANDINGPAGE_H

#include <QWidget>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>

class OptionCard : public QFrame
{
    Q_OBJECT

public:
    explicit OptionCard(const QString &title, const QString &description, QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setFixedSize(300, 300);
        setCursor(Qt::PointingHandCursor);

        setStyleSheet(R"(
            OptionCard {
                background-color: #2d2d2d;
                border: 2px solid #404040;
                border-radius: 20px;
            }
            OptionCard:hover {
                background-color: #3d3d3d;
                border: 2px solid #606060;
            }
        )");

        auto *shadow = new QGraphicsDropShadowEffect();
        shadow->setBlurRadius(20);
        shadow->setColor(QColor(0, 0, 0, 80));
        shadow->setOffset(0, 5);
        setGraphicsEffect(shadow);

        auto *layout = new QVBoxLayout(this);
        layout->setAlignment(Qt::AlignCenter);
        layout->setSpacing(15);

        auto *titleLabel = new QLabel(title);
        titleLabel->setStyleSheet(R"(
            font-size: 48px;
            font-weight: bold;
            color: #ffffff;
        )");
        titleLabel->setAlignment(Qt::AlignCenter);

        auto *descLabel = new QLabel(description);
        descLabel->setStyleSheet(R"(
            font-size: 14px;
            color: #aaaaaa;
        )");
        descLabel->setAlignment(Qt::AlignCenter);
        descLabel->setWordWrap(true);

        layout->addWidget(titleLabel);
        layout->addWidget(descLabel);
    }

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        emit clicked();
        QFrame::mousePressEvent(event);
    }
};

class LandingPage : public QWidget
{
    Q_OBJECT

public:
    explicit LandingPage(QWidget *parent = nullptr) : QWidget(parent)
    {
        setStyleSheet("background-color: #1a1a1a;");

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setAlignment(Qt::AlignCenter);
        mainLayout->setSpacing(40);

        // Title
        auto *title = new QLabel("Fractal Viewer");
        title->setStyleSheet(R"(
            font-size: 36px;
            font-weight: bold;
            color: #ffffff;
        )");
        title->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(title);

        // Subtitle
        auto *subtitle = new QLabel("Choose your dimension");
        subtitle->setStyleSheet(R"(
            font-size: 16px;
            color: #888888;
        )");
        subtitle->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(subtitle);

        // Cards container
        auto *cardsLayout = new QHBoxLayout();
        cardsLayout->setSpacing(60);
        cardsLayout->setAlignment(Qt::AlignCenter);

        card2D = new OptionCard("2D", "Explore classic fractals like\nMandelbrot and Julia sets");
        card3D = new OptionCard("3D", "Dive into 3D fractals like\nMandelbox and Mandelbulb");

        cardsLayout->addWidget(card2D);
        cardsLayout->addWidget(card3D);

        mainLayout->addLayout(cardsLayout);
    }

    OptionCard *card2D;
    OptionCard *card3D;
};

#endif // LANDINGPAGE_H
