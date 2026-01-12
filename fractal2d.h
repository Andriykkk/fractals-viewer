#ifndef FRACTAL2D_H
#define FRACTAL2D_H

#include <QWidget>
#include <QFrame>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSlider>
#include <QKeyEvent>
#include <QTimer>

#include "types.h"
#include "utils.h"

class GLWidget2D : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GLWidget2D(FractalState *sharedState, QWidget *parent = nullptr)
        : QOpenGLWidget(parent), state(sharedState) {}

    FractalState *state;

protected:
    void initializeGL() override
    {
        initializeOpenGLFunctions();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    }

    void resizeGL(int w, int h) override
    {
        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        float aspect = (h > 0) ? static_cast<float>(w) / h : 1.0f;
        if (aspect >= 1.0f)
        {
            glOrtho(-aspect, aspect, -1.0, 1.0, -1.0, 1.0);
        }
        else
        {
            glOrtho(-1.0, 1.0, -1.0 / aspect, 1.0 / aspect, -1.0, 1.0);
        }
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    void paintGL() override
    {
        glClear(GL_COLOR_BUFFER_BIT);
        glLoadIdentity();

        // Get position from state
        float px = static_cast<float>(state->posX);
        float py = static_cast<float>(state->posY);

        // Draw a colored triangle at current position
        glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 0.0f);
        glVertex2f(px + 0.0f, py + 0.5f);
        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex2f(px - 0.5f, py - 0.5f);
        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex2f(px + 0.5f, py - 0.5f);
        glEnd();
    }
};

class Sidebar2D : public QFrame
{
    Q_OBJECT

public:
    explicit Sidebar2D(QWidget *parent = nullptr) : QFrame(parent)
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
        auto *speedLabel = new QLabel("Speed: 1");
        speedLabel->setStyleSheet("font-weight: bold;");
        layout->addWidget(speedLabel);

        speedSlider = new QSlider(Qt::Horizontal);
        speedSlider->setRange(0, 100);
        speedSlider->setValue(1);
        layout->addWidget(speedSlider);

        connect(speedSlider, &QSlider::valueChanged, this, [speedLabel](int value) {
            speedLabel->setText(QString("Speed: %1").arg(value));
        });

        // Scale slider
        auto *scaleSliderLabel = new QLabel("Scale: 1.00x");
        scaleSliderLabel->setStyleSheet("font-weight: bold;");
        layout->addWidget(scaleSliderLabel);

        scaleSlider = new QSlider(Qt::Horizontal);
        scaleSlider->setRange(-1000, 1000);
        scaleSlider->setValue(0);
        layout->addWidget(scaleSlider);

        connect(scaleSlider, &QSlider::valueChanged, this, [scaleSliderLabel](int value) {
            double scale = sliderToScale(value);
            scaleSliderLabel->setText(QString("Scale: %1x").arg(scale, 0, 'f', scale < 10 ? 2 : 0));
        });

        layout->addSpacing(20);

        // Frame info section
        auto *infoLabel = new QLabel("Frame Info:");
        infoLabel->setStyleSheet("font-weight: bold;");
        layout->addWidget(infoLabel);

        frameInfoText = new QLabel();
        frameInfoText->setStyleSheet("font-size: 11px; color: #000000;");
        frameInfoText->setWordWrap(true);
        frameInfoText->setText("FPS: --\nZoom: --\nPosition: --");
        layout->addWidget(frameInfoText);

        layout->addStretch();
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

    QPushButton *btnBack;
    QPushButton *btnUpdate;
    QPushButton *btnClear;
    QLineEdit *formulaInput;
    QLabel *errorLabel;
    QLabel *frameInfoText;
    QSlider *speedSlider;
    QSlider *scaleSlider;
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

        sidebar = new Sidebar2D();
        layout->addWidget(sidebar);

        glWidget = new GLWidget2D(state);
        layout->addWidget(glWidget, 1);

        // Connect Update button
        connect(sidebar->btnUpdate, &QPushButton::clicked, this, [this]() {
            state->formula = sidebar->formulaInput->text();
            sidebar->hideError();
            glWidget->update();
        });

        // Connect Clear button
        connect(sidebar->btnClear, &QPushButton::clicked, this, [this]() {
            state->formula.clear();
            state->posX = 0.0;
            state->posY = 0.0;
            state->scale = 1.0;
            state->speed = 1;
            state->scaleSlider = 0;
            sidebar->formulaInput->clear();
            sidebar->speedSlider->setValue(1);
            sidebar->scaleSlider->setValue(0);
            sidebar->hideError();
            glWidget->update();
        });

        // Connect sliders
        connect(sidebar->speedSlider, &QSlider::valueChanged, this, [this](int value) {
            state->speed = value;
        });

        connect(sidebar->scaleSlider, &QSlider::valueChanged, this, [this](int value) {
            state->scaleSlider = value;
            state->scale = sliderToScale(value);
            glWidget->update();
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

protected:
    void gameLoop()
    {
        double moveSpeed = state->speed * 0.001;
        bool moved = false;

        if (state->moveUp) {
            state->posY += moveSpeed;
            moved = true;
        }
        if (state->moveDown) {
            state->posY -= moveSpeed;
            moved = true;
        }
        if (state->moveLeft) {
            state->posX -= moveSpeed;
            moved = true;
        }
        if (state->moveRight) {
            state->posX += moveSpeed;
            moved = true;
        }

        if (moved) {
            glWidget->update();
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
    GLWidget2D *glWidget;
    QTimer *gameTimer;
    FractalState *state;
};

#endif // FRACTAL2D_H
