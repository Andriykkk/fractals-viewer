#ifndef FRACTAL3D_H
#define FRACTAL3D_H

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

class GLWidget3D : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GLWidget3D(FractalState *sharedState, QWidget *parent = nullptr)
        : QOpenGLWidget(parent), state(sharedState) {}

    FractalState *state;

protected:
    void initializeGL() override
    {
        initializeOpenGLFunctions();
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glEnable(GL_DEPTH_TEST);
    }

    void resizeGL(int w, int h) override
    {
        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        float aspect = (h > 0) ? static_cast<float>(w) / h : 1.0f;
        // Perspective projection for 3D
        float fov = 45.0f;
        float nearPlane = 0.1f;
        float farPlane = 100.0f;
        float top = nearPlane * tanf(fov * 3.14159f / 360.0f);
        float bottom = -top;
        float right = top * aspect;
        float left = -right;
        glFrustum(left, right, bottom, top, nearPlane, farPlane);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    void paintGL() override
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();

        // Move back to see the scene
        glTranslatef(0.0f, 0.0f, -3.0f);

        // Apply position from state
        glTranslatef(static_cast<float>(state->posX),
                     static_cast<float>(state->posY),
                     static_cast<float>(state->posZ));

        // Draw a simple 3D cube placeholder
        drawCube();
    }

private:
    void drawCube()
    {
        float s = 0.5f;

        glBegin(GL_QUADS);
        // Front face (red)
        glColor3f(1.0f, 0.0f, 0.0f);
        glVertex3f(-s, -s, s);
        glVertex3f(s, -s, s);
        glVertex3f(s, s, s);
        glVertex3f(-s, s, s);

        // Back face (green)
        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex3f(-s, -s, -s);
        glVertex3f(-s, s, -s);
        glVertex3f(s, s, -s);
        glVertex3f(s, -s, -s);

        // Top face (blue)
        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex3f(-s, s, -s);
        glVertex3f(-s, s, s);
        glVertex3f(s, s, s);
        glVertex3f(s, s, -s);

        // Bottom face (yellow)
        glColor3f(1.0f, 1.0f, 0.0f);
        glVertex3f(-s, -s, -s);
        glVertex3f(s, -s, -s);
        glVertex3f(s, -s, s);
        glVertex3f(-s, -s, s);

        // Right face (magenta)
        glColor3f(1.0f, 0.0f, 1.0f);
        glVertex3f(s, -s, -s);
        glVertex3f(s, s, -s);
        glVertex3f(s, s, s);
        glVertex3f(s, -s, s);

        // Left face (cyan)
        glColor3f(0.0f, 1.0f, 1.0f);
        glVertex3f(-s, -s, -s);
        glVertex3f(-s, -s, s);
        glVertex3f(-s, s, s);
        glVertex3f(-s, s, -s);
        glEnd();
    }
};

class Sidebar3D : public QFrame
{
    Q_OBJECT

public:
    explicit Sidebar3D(QWidget *parent = nullptr) : QFrame(parent)
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
        formulaInput->setPlaceholderText("e.g., mandelbulb");
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

class FractalPage3D : public QWidget
{
    Q_OBJECT

public:
    explicit FractalPage3D(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        state = new FractalState();

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        sidebar = new Sidebar3D();
        layout->addWidget(sidebar);

        glWidget = new GLWidget3D(state);
        layout->addWidget(glWidget, 1);

        // Connect Update button
        connect(sidebar->btnUpdate, &QPushButton::clicked, this, [this]() {
            state->formula = sidebar->formulaInput->text();
            sidebar->hideError();
            glWidget->update();
        });

        // Connect Clear button
        connect(sidebar->btnClear, &QPushButton::clicked, this, [this]() {
            resetState();
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
        connect(gameTimer, &QTimer::timeout, this, &FractalPage3D::gameLoop);
        gameTimer->start(16);
    }

    ~FractalPage3D()
    {
        delete state;
    }

    void resetState()
    {
        state->clear();
        sidebar->formulaInput->clear();
        sidebar->speedSlider->setValue(1);
        sidebar->scaleSlider->setValue(0);
        sidebar->hideError();
        glWidget->update();
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
    Sidebar3D *sidebar;
    GLWidget3D *glWidget;
    QTimer *gameTimer;
    FractalState *state;
};

#endif // FRACTAL3D_H
