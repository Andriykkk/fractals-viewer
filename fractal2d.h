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
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

#include <map>
#include <thread>
#include <atomic>
#include <cmath>
#include <cstring>

#include "types.h"
#include "utils.h"

// Pixel dimensions of each chunk (100x100 pixels)
const int CHUNK_SIZE = 100;

// Fast HSV to RGB (hue 0-359, returns 0xRRGGBB)
inline uint32_t hsvToRgb(int hue)
{
    int h = hue % 360;
    int hi = h / 60;
    int f = h % 60;
    int q = 255 - (255 * f / 60);
    int t = 255 * f / 60;

    switch (hi) {
        case 0: return 0xFF0000 | (t << 8);           // R=255, G=t, B=0
        case 1: return (q << 16) | 0x00FF00;          // R=q, G=255, B=0
        case 2: return 0x00FF00 | t;                  // R=0, G=255, B=t
        case 3: return (q << 8) | 0x0000FF;           // R=0, G=q, B=255
        case 4: return (t << 16) | 0x0000FF;          // R=t, G=0, B=255
        default: return 0xFF0000 | (q << 8);          // R=255, G=0, B=q
    }
}

// Chunk structure: stores raw pixel data
struct Chunk
{
    uint32_t data[CHUNK_SIZE * CHUNK_SIZE];
    bool rendered = false;
    bool processing = false;
};

class FractalWidget2D : public QWidget
{
    Q_OBJECT

public:
    explicit FractalWidget2D(FractalState *sharedState, QWidget *parent = nullptr)
        : QWidget(parent), state(sharedState)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        numCores = std::thread::hardware_concurrency();
        if (numCores == 0) numCores = 4;  // fallback
    }

    FractalState *state;
    int numCores = 4;
    std::atomic<int> activeThreads{0};
    bool hasUnrenderedChunks = false;

    void renderFractal()
    {
        if (width() <= 0 || height() <= 0) return;

        int w = width();
        int h = height();
        double scale = state->scale;
        double posX = state->posX;
        double posY = state->posY;
        int maxIter = state->maxIterations;
        int hueStart = state->hueStart;
        int hueRange = state->hueRange;

        // Invalidate chunks if scale or width changed
        if (scale != lastScale || w != lastWidth)
        {
            invalidateChunks();
            lastScale = scale;
            lastWidth = w;
        }

        // Resize image if needed
        if (image.width() != w || image.height() != h)
        {
            image = QImage(w, h, QImage::Format_RGB32);
        }

        double pixelSize = 2.0 / (scale * w);
        double chunkWorldSize = CHUNK_SIZE * pixelSize;
        double centerX = (w - 1) / 2.0;
        double centerY = (h - 1) / 2.0;

        hasUnrenderedChunks = false;

        // Go through each pixel
        for (int py = 0; py < h; ++py)
        {
            uint32_t *line = reinterpret_cast<uint32_t*>(image.scanLine(py));
            double worldY = posY + (centerY - py) * pixelSize;

            for (int px = 0; px < w; ++px)
            {
                double worldX = posX + (px - centerX) * pixelSize;

                int chunkX = (int)floor(worldX / chunkWorldSize);
                int chunkY = (int)floor(worldY / chunkWorldSize);

                auto key = std::make_pair(chunkX, chunkY);
                Chunk &chunk = chunks[key];

                if (chunk.rendered)
                {
                    double chunkOriginX = chunkX * chunkWorldSize;
                    double chunkOriginY = chunkY * chunkWorldSize;
                    int localX = (int)((worldX - chunkOriginX) / pixelSize);
                    int localY = (int)((worldY - chunkOriginY) / pixelSize);
                    if (localX < 0) localX = 0;
                    if (localX >= CHUNK_SIZE) localX = CHUNK_SIZE - 1;
                    if (localY < 0) localY = 0;
                    if (localY >= CHUNK_SIZE) localY = CHUNK_SIZE - 1;

                    line[px] = chunk.data[localY * CHUNK_SIZE + localX];
                }
                else
                {
                    line[px] = 0x000000;
                    hasUnrenderedChunks = true;

                    if (!chunk.processing && activeThreads < numCores)
                    {
                        chunk.processing = true;
                        activeThreads++;
                        int gen = generation;

                        std::thread([this, chunkX, chunkY, scale, w, maxIter, hueStart, hueRange, gen]() {
                            renderChunk(chunkX, chunkY, scale, w, maxIter, hueStart, hueRange, gen);
                            activeThreads--;
                        }).detach();
                    }
                }
            }
        }

        update();
    }

private:

    QImage image;
    std::map<std::pair<int, int>, Chunk> chunks;
    double lastScale = 0.0;
    int lastWidth = 0;
    std::atomic<int> generation{0};

    void invalidateChunks()
    {
        generation++;
        chunks.clear();
    }

    // Calculate Mandelbrot color for world position (static for thread safety)
    static uint32_t calcMandelbrot(double cx, double cy, int maxIterations, int hueStart, int hueRange)
    {
        double zx = 0.0;
        double zy = 0.0;
        int iterations = 0;

        while (iterations < maxIterations)
        {
            double zx2 = zx * zx;
            double zy2 = zy * zy;

            if (zx2 + zy2 > 4.0) break;

            double newZx = zx2 - zy2 + cx;
            zy = 2.0 * zx * zy + cy;
            zx = newZx;

            iterations++;
        }

        if (iterations == maxIterations)
        {
            return 0x000000;
        }

        int hue = (hueStart + iterations * hueRange / maxIterations) % 360;
        return hsvToRgb(hue);
    }

    // Render entire chunk (all 100x100 pixels)
    void renderChunk(int chunkX, int chunkY, double scale, int screenWidth, int maxIter, int hueStart, int hueRange, int gen)
    {
        // Render to local buffer first
        uint32_t localData[CHUNK_SIZE * CHUNK_SIZE];

        double pixelSize = 2.0 / (scale * screenWidth);
        double chunkWorldSize = CHUNK_SIZE * pixelSize;

        double chunkOriginX = chunkX * chunkWorldSize;
        double chunkOriginY = chunkY * chunkWorldSize;

        for (int ly = 0; ly < CHUNK_SIZE; ++ly)
        {
            double cy = chunkOriginY + ly * pixelSize;
            uint32_t *row = localData + ly * CHUNK_SIZE;
            for (int lx = 0; lx < CHUNK_SIZE; ++lx)
            {
                double cx = chunkOriginX + lx * pixelSize;
                row[lx] = calcMandelbrot(cx, cy, maxIter, hueStart, hueRange);
            }
        }

        // Only store result if generation hasn't changed
        if (generation == gen)
        {
            auto key = std::make_pair(chunkX, chunkY);
            Chunk &chunk = chunks[key];
            memcpy(chunk.data, localData, sizeof(localData));
            chunk.rendered = true;
            chunk.processing = false;
        }
    }

    // Calculate world position and chunk info for a screen pixel
    void pixelToWorld(int px, int py, int w, int h, double posX, double posY, double scale,
                      double &worldX, double &worldY, int &chunkX, int &chunkY, int &localX, int &localY)
    {
        double pixelSize = 2.0 / (scale * w);
        double chunkWorldSize = CHUNK_SIZE * pixelSize;

        double centerX = (w - 1) / 2.0;
        double centerY = (h - 1) / 2.0;

        worldX = posX + (px - centerX) * pixelSize;
        worldY = posY + (centerY - py) * pixelSize;

        chunkX = (int)floor(worldX / chunkWorldSize);
        chunkY = (int)floor(worldY / chunkWorldSize);

        double chunkOriginX = chunkX * chunkWorldSize;
        double chunkOriginY = chunkY * chunkWorldSize;

        localX = (int)floor((worldX - chunkOriginX) / pixelSize);
        localY = (int)floor((worldY - chunkOriginY) / pixelSize);

        // Clamp
        if (localX < 0) localX = 0;
        if (localX >= CHUNK_SIZE) localX = CHUNK_SIZE - 1;
        if (localY < 0) localY = 0;
        if (localY >= CHUNK_SIZE) localY = CHUNK_SIZE - 1;
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        if (!image.isNull())
        {
            painter.drawImage(0, 0, image);
        }
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        renderFractal();
    }

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

        connect(scaleSpeedSlider, &QSlider::valueChanged, this, [this, scaleSpeedLabel](int value) {
            // Snap to zero when close to center
            if (value > -50 && value < 50 && value != 0) {
                scaleSpeedSlider->setValue(0);
                return;
            }
            state->scaleSpeed = value;
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

        // Update scale based on scaleSpeed (cubic curve for slow center, fast extremes)
        if (state->scaleSpeed != 0) {
            double normalized = state->scaleSpeed / 1000.0;  // -1 to 1
            double curved = normalized * normalized * normalized;  // cubic: preserves sign
            double scaleMultiplier = 1.0 + curved * 0.1;
            state->scale *= scaleMultiplier;
            changed = true;
        }

        if (changed) {
            sidebar->updateInfo(state->posX, state->posY, state->scale);
        }

        // Render if something changed or there are unrendered chunks
        if (changed || fractalWidget->hasUnrenderedChunks) {
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
