#ifndef FRACTAL2D_H
#define FRACTAL2D_H

#include <QWidget>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSlider>
#include <QKeyEvent>
#include <QTimer>
#include <QImage>
#include <QPainter>
#include <QColor>

#include "types.h"
#include "utils.h"

class FractalWidget2D : public QWidget
{
    Q_OBJECT

public:
    explicit FractalWidget2D(FractalState *sharedState, QWidget *parent = nullptr)
        : QWidget(parent), state(sharedState)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    FractalState *state;

    void renderFractal()
    {
        if (width() <= 0 || height() <= 0) return;

        image = QImage(width(), height(), QImage::Format_RGB32);

        int w = width();
        int h = height();
        double scale = state->scale;
        double posX = state->posX;
        double posY = state->posY;

        for (int py = 0; py < h; ++py)
        {
            for (int px = 0; px < w; ++px)
            {
                QRgb color = renderPixel(px, py, w, h, posX, posY, scale);
                image.setPixel(px, py, color);
            }
        }

        update();
    }

private:
    QRgb renderPixel(int px, int py, int w, int h, double posX, double posY, double scale)
    {
        // At scale 1, width shows range of 2 (-1 to 1)
        // Each pixel represents (2 / scale) / w units in world space
        double pixelSize = 2.0 / (scale * w);

        // Distance from center pixel (handles both odd and even dimensions)
        // For w=5: center is 2.0, pixels 0,1,2,3,4 have offsets -2,-1,0,1,2
        // For w=4: center is 1.5, pixels 0,1,2,3 have offsets -1.5,-0.5,0.5,1.5
        double centerX = (w - 1) / 2.0;
        double centerY = (h - 1) / 2.0;

        double offsetX = px - centerX;
        double offsetY = centerY - py;  // Y is inverted (screen Y goes down)

        // Map to world coordinates (this is c in the Mandelbrot formula)
        double cx = posX + offsetX * pixelSize;
        double cy = posY + offsetY * pixelSize;

        // Mandelbrot iteration: z = z^2 + c, starting with z = 0
        double zx = 0.0;
        double zy = 0.0;
        int iterations = 0;
        int maxIterations = state->maxIterations;

        while (iterations < maxIterations)
        {
            double zx2 = zx * zx;
            double zy2 = zy * zy;

            // Check escape condition: |z| > 2
            if (zx2 + zy2 > 4.0) break;

            // z = z^2 + c
            double newZx = zx2 - zy2 + cx;
            zy = 2.0 * zx * zy + cy;
            zx = newZx;

            iterations++;
        }

        // Black for points inside the set (reached max iterations)
        if (iterations == maxIterations)
        {
            return qRgb(0, 0, 0);
        }

        // HSV color based on iteration count
        int hue = (state->hueStart + iterations * state->hueRange / maxIterations) % 360;
        QColor color = QColor::fromHsv(hue, 255, 255);
        return color.rgb();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        if (!image.isNull())
        {
            painter.drawImage(0, 0, image);
        }
        else
        {
            painter.fillRect(rect(), Qt::black);
        }
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        renderFractal();
    }

private:
    QImage image;
};

class Sidebar2D : public QFrame
{
    Q_OBJECT

public:
    explicit Sidebar2D(FractalState *sharedState, QWidget *parent = nullptr)
        : QFrame(parent), state(sharedState)
    {
        setFrameStyle(QFrame::StyledPanel);
        setFixedWidth(250);

        auto *layout = new QVBoxLayout(this);
        layout->setAlignment(Qt::AlignTop);
        layout->setSpacing(10);

        // Back button
        btnBack = new QPushButton("Back to Menu");
        layout->addWidget(btnBack);

        // Formula section
        auto *formulaLabel = new QLabel("Formula:");
        formulaLabel->setStyleSheet("font-weight: bold;");
        layout->addWidget(formulaLabel);

        formulaInput = new QLineEdit();
        formulaInput->setPlaceholderText("e.g., z^2 + c");
        layout->addWidget(formulaInput);

        // Error label (hidden by default)
        errorLabel = new QLabel();
        errorLabel->setStyleSheet("color: #ff4444; font-size: 12px;");
        errorLabel->setWordWrap(true);
        errorLabel->setVisible(false);
        layout->addWidget(errorLabel);

        // Buttons
        auto *btnLayout = new QHBoxLayout();
        btnUpdate = new QPushButton("Update");
        btnClear = new QPushButton("Clear");
        btnLayout->addWidget(btnUpdate);
        btnLayout->addWidget(btnClear);
        layout->addLayout(btnLayout);

        layout->addSpacing(20);

        // Speed slider
        auto *speedLabel = new QLabel(QString("Speed: %1").arg(state->speed));
        speedLabel->setStyleSheet("font-weight: bold;");
        layout->addWidget(speedLabel);

        speedSlider = new QSlider(Qt::Horizontal);
        speedSlider->setRange(0, 100);
        speedSlider->setValue(state->speed);
        layout->addWidget(speedSlider);

        connect(speedSlider, &QSlider::valueChanged, this, [speedLabel](int value) {
            speedLabel->setText(QString("Speed: %1").arg(value));
        });

        // Scale speed slider
        auto *scaleSpeedLabel = new QLabel(QString("Scale Speed: %1").arg(state->scaleSpeed));
        scaleSpeedLabel->setStyleSheet("font-weight: bold;");
        layout->addWidget(scaleSpeedLabel);

        scaleSpeedSlider = new QSlider(Qt::Horizontal);
        scaleSpeedSlider->setRange(-1000, 1000);
        scaleSpeedSlider->setValue(state->scaleSpeed);
        layout->addWidget(scaleSpeedSlider);

        connect(scaleSpeedSlider, &QSlider::valueChanged, this, [scaleSpeedLabel](int value) {
            scaleSpeedLabel->setText(QString("Scale Speed: %1").arg(value));
        });

        layout->addSpacing(20);

        // Max iterations slider
        auto *maxIterLabel = new QLabel(QString("Max Iterations: %1").arg(state->maxIterations));
        maxIterLabel->setStyleSheet("font-weight: bold;");
        layout->addWidget(maxIterLabel);

        maxIterSlider = new QSlider(Qt::Horizontal);
        maxIterSlider->setRange(5, 1000);
        maxIterSlider->setValue(state->maxIterations);
        layout->addWidget(maxIterSlider);

        connect(maxIterSlider, &QSlider::valueChanged, this, [maxIterLabel](int value) {
            maxIterLabel->setText(QString("Max Iterations: %1").arg(value));
        });

        // Hue range slider
        auto *hueRangeLabel = new QLabel(QString("Hue Range: %1").arg(state->hueRange));
        hueRangeLabel->setStyleSheet("font-weight: bold;");
        layout->addWidget(hueRangeLabel);

        hueRangeSlider = new QSlider(Qt::Horizontal);
        hueRangeSlider->setRange(1, 360);
        hueRangeSlider->setValue(state->hueRange);
        layout->addWidget(hueRangeSlider);

        connect(hueRangeSlider, &QSlider::valueChanged, this, [hueRangeLabel](int value) {
            hueRangeLabel->setText(QString("Hue Range: %1").arg(value));
        });

        // Hue start slider
        auto *hueStartLabel = new QLabel(QString("Hue Start: %1").arg(state->hueStart));
        hueStartLabel->setStyleSheet("font-weight: bold;");
        layout->addWidget(hueStartLabel);

        hueStartSlider = new QSlider(Qt::Horizontal);
        hueStartSlider->setRange(0, 360);
        hueStartSlider->setValue(state->hueStart);
        layout->addWidget(hueStartSlider);

        connect(hueStartSlider, &QSlider::valueChanged, this, [hueStartLabel](int value) {
            hueStartLabel->setText(QString("Hue Start: %1").arg(value));
        });

        layout->addSpacing(20);

        // Info section
        auto *infoLabel = new QLabel("Info:");
        infoLabel->setStyleSheet("font-weight: bold;");
        layout->addWidget(infoLabel);

        frameInfoText = new QLabel();
        frameInfoText->setStyleSheet("font-size: 11px; color: #000000;");
        frameInfoText->setWordWrap(true);
        frameInfoText->setText(QString("X: %1\nY: %2\nScale: %3x")
            .arg(state->posX, 0, 'f', 2)
            .arg(state->posY, 0, 'f', 2)
            .arg(state->scale, 0, 'f', 2));
        layout->addWidget(frameInfoText);

        layout->addStretch();
    }

    void updateInfo(double x, double y, double scale)
    {
        frameInfoText->setText(QString("X: %1\nY: %2\nScale: %3x")
            .arg(x, 0, 'f', 2)
            .arg(y, 0, 'f', 2)
            .arg(scale, 0, 'f', scale < 10 ? 2 : 0));
    }

    void showError(const QString &message)
    {
        errorLabel->setText(message);
        errorLabel->setVisible(true);
    }

    void hideError()
    {
        errorLabel->setVisible(false);
    }

    void setFrameInfo(const QString &info)
    {
        frameInfoText->setText(info);
    }

    FractalState *state;
    QPushButton *btnBack;
    QPushButton *btnUpdate;
    QPushButton *btnClear;
    QLineEdit *formulaInput;
    QLabel *errorLabel;
    QLabel *frameInfoText;
    QSlider *speedSlider;
    QSlider *scaleSpeedSlider;
    QSlider *maxIterSlider;
    QSlider *hueRangeSlider;
    QSlider *hueStartSlider;
};

class FractalPage2D : public QWidget
{
    Q_OBJECT

public:
    explicit FractalPage2D(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        state = new FractalState();

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        sidebar = new Sidebar2D(state);
        layout->addWidget(sidebar);

        fractalWidget = new FractalWidget2D(state);
        layout->addWidget(fractalWidget, 1);

        // Connect Update button
        connect(sidebar->btnUpdate, &QPushButton::clicked, this, [this]() {
            state->formula = sidebar->formulaInput->text();
            sidebar->hideError();
            fractalWidget->renderFractal();
        });

        // Connect Clear button
        connect(sidebar->btnClear, &QPushButton::clicked, this, [this]() {
            resetState();
        });

        // Connect sliders
        connect(sidebar->speedSlider, &QSlider::valueChanged, this, [this](int value) {
            state->speed = value;
        });

        connect(sidebar->scaleSpeedSlider, &QSlider::valueChanged, this, [this](int value) {
            state->scaleSpeed = value;
        });

        connect(sidebar->maxIterSlider, &QSlider::valueChanged, this, [this](int value) {
            state->maxIterations = value;
            fractalWidget->renderFractal();
        });

        connect(sidebar->hueRangeSlider, &QSlider::valueChanged, this, [this](int value) {
            state->hueRange = value;
            fractalWidget->renderFractal();
        });

        connect(sidebar->hueStartSlider, &QSlider::valueChanged, this, [this](int value) {
            state->hueStart = value;
            fractalWidget->renderFractal();
        });

        // Game loop timer
        gameTimer = new QTimer(this);
        connect(gameTimer, &QTimer::timeout, this, &FractalPage2D::gameLoop);
        gameTimer->start(16);
    }

    ~FractalPage2D()
    {
        delete state;
    }

    void resetState()
    {
        state->clear();
        sidebar->formulaInput->clear();
        sidebar->speedSlider->setValue(state->speed);
        sidebar->scaleSpeedSlider->setValue(state->scaleSpeed);
        sidebar->maxIterSlider->setValue(state->maxIterations);
        sidebar->hueRangeSlider->setValue(state->hueRange);
        sidebar->hueStartSlider->setValue(state->hueStart);
        sidebar->hideError();
        sidebar->updateInfo(state->posX, state->posY, state->scale);
        fractalWidget->renderFractal();
    }

protected:
    void gameLoop()
    {
        double moveSpeed = state->speed * 0.001;
        bool changed = false;

        if (state->moveUp) {
            state->posY += moveSpeed;
            changed = true;
        }
        if (state->moveDown) {
            state->posY -= moveSpeed;
            changed = true;
        }
        if (state->moveLeft) {
            state->posX -= moveSpeed;
            changed = true;
        }
        if (state->moveRight) {
            state->posX += moveSpeed;
            changed = true;
        }

        // Update scale based on scaleSpeed
        if (state->scaleSpeed != 0) {
            double scaleMultiplier = 1.0 + state->scaleSpeed * 0.0001;
            state->scale *= scaleMultiplier;
            changed = true;
        }

        if (changed) {
            sidebar->updateInfo(state->posX, state->posY, state->scale);
            fractalWidget->renderFractal();
        }
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->isAutoRepeat()) return;

        switch (event->key())
        {
        case Qt::Key_W:
        case Qt::Key_Up:
            state->moveUp = true;
            break;
        case Qt::Key_S:
        case Qt::Key_Down:
            state->moveDown = true;
            break;
        case Qt::Key_A:
        case Qt::Key_Left:
            state->moveLeft = true;
            break;
        case Qt::Key_D:
        case Qt::Key_Right:
            state->moveRight = true;
            break;
        default:
            QWidget::keyPressEvent(event);
            return;
        }
    }

    void keyReleaseEvent(QKeyEvent *event) override
    {
        if (event->isAutoRepeat()) return;

        switch (event->key())
        {
        case Qt::Key_W:
        case Qt::Key_Up:
            state->moveUp = false;
            break;
        case Qt::Key_S:
        case Qt::Key_Down:
            state->moveDown = false;
            break;
        case Qt::Key_A:
        case Qt::Key_Left:
            state->moveLeft = false;
            break;
        case Qt::Key_D:
        case Qt::Key_Right:
            state->moveRight = false;
            break;
        default:
            QWidget::keyReleaseEvent(event);
            return;
        }
    }

public:
    Sidebar2D *sidebar;
    FractalWidget2D *fractalWidget;
    QTimer *gameTimer;
    FractalState *state;
};

#endif // FRACTAL2D_H
