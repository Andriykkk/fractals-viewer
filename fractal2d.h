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
#include <QWheelEvent>
#include <QTimer>
#include <QPainter>

#include <map>
#include <thread>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>
#include <complex>
#include <mpfr.h>

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

// Reference orbit point (Z values during iteration, stored as doubles since |Z| <= 2)
using ComplexD = std::complex<double>;

// Chunk structure: stores raw pixel data and reference orbit
struct Chunk
{
    uint32_t data[CHUNK_SIZE * CHUNK_SIZE];
    bool rendered = false;
    bool processing = false;

    // Reference orbit for perturbation theory
    // Stores Z values at each iteration for the chunk center point
    // Multiple reference points can be used for better accuracy
    std::vector<std::vector<ComplexD>> referenceOrbits;

    // Escape iteration for each reference point (-1 if doesn't escape)
    std::vector<int> referenceEscapeIter;
};

// Compute reference orbit using high-precision arithmetic
// Returns the orbit (Z values at each iteration) and escape iteration (-1 if doesn't escape)
inline std::pair<std::vector<ComplexD>, int> computeReferenceOrbit(
    const MPFRFloat& cReal, const MPFRFloat& cImag,
    int maxIter, mpfr_prec_t precision)
{
    std::vector<ComplexD> orbit;
    orbit.reserve(maxIter);

    MPFRFloat zReal(precision), zImag(precision);
    MPFRFloat zReal2(precision), zImag2(precision);
    MPFRFloat temp(precision);

    zReal.set(0.0);
    zImag.set(0.0);

    for (int i = 0; i < maxIter; ++i)
    {
        // Store current Z as double (fits because |Z| <= 2)
        orbit.push_back(ComplexD(zReal.toDouble(), zImag.toDouble()));

        // z = z^2 + c
        // zReal2 = zReal^2, zImag2 = zImag^2
        zReal2.mul(zReal, zReal);
        zImag2.mul(zImag, zImag);

        // Check escape: |z|^2 > 4
        temp.add(zReal2, zImag2);
        if (temp.toDouble() > 4.0)
        {
            return {orbit, i};
        }

        // newZImag = 2 * zReal * zImag + cImag
        temp.mul(zReal, zImag);
        temp.mul_d(temp, 2.0);
        temp.add(temp, cImag);

        // newZReal = zReal2 - zImag2 + cReal
        zReal.sub(zReal2, zImag2);
        zReal.add(zReal, cReal);

        zImag.set(temp);
    }

    return {orbit, -1};  // Doesn't escape
}

// Perturbation iteration for a single pixel
// pixelC is the actual C value for this pixel (computed with high precision outside)
// Returns iteration count where it escapes, or -1 if doesn't escape
inline int perturbationIterate(
    const std::vector<ComplexD>& refOrbit,
    int refEscapeIter,
    ComplexD deltaC,
    ComplexD pixelC,  // For fallback when reference escapes
    int maxIter,
    bool& glitched)
{
    ComplexD delta(0.0, 0.0);
    glitched = false;

    int orbitLen = (int)refOrbit.size();
    int limit = (refEscapeIter >= 0) ? std::min(refEscapeIter, maxIter) : maxIter;
    limit = std::min(limit, orbitLen);

    for (int i = 0; i < limit; ++i)
    {
        ComplexD Z = refOrbit[i];

        // Full value: W = Z + delta
        ComplexD W = Z + delta;

        // Check escape
        if (std::norm(W) > 4.0)
        {
            return i;
        }

        // Glitch detection: if |delta| becomes too large relative to Z
        // This means the reference point is too far and we need a closer one
        double Znorm = std::norm(Z);
        double deltaNorm = std::norm(delta);
        if (Znorm > 1e-30 && deltaNorm > Znorm * 1e6)
        {
            glitched = true;
            return i;
        }

        // Perturbation formula: delta_next = 2*Z*delta + delta^2 + deltaC
        delta = 2.0 * Z * delta + delta * delta + deltaC;
    }

    // If reference escaped but we didn't, continue with direct iteration
    // using the pixel's actual C value (loses precision at extreme zooms)
    if (refEscapeIter >= 0 && refEscapeIter < maxIter)
    {
        ComplexD W = refOrbit.back() + delta;
        for (int i = refEscapeIter; i < maxIter; ++i)
        {
            if (std::norm(W) > 4.0)
            {
                return i;
            }
            W = W * W + pixelC;
        }
    }

    return -1;  // Doesn't escape
}

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
        double scale = state->scale.toDouble();
        double posX = state->posX.toDouble();
        double posY = state->posY.toDouble();
        // Scale iterations with zoom - more detail at higher zoom
        int maxIter = (int)(state->maxIterations * log2(scale + 1) + state->maxIterations);
        int hueStart = state->hueStart;
        int hueRange = state->hueRange;

        // Invalidate chunks if scale, width, or hue settings changed
        if (scale != lastScale || w != lastWidth || hueStart != lastHueStart || hueRange != lastHueRange)
        {
            invalidateChunks();
            state->updatePrecision();  // Increase precision if needed for new scale
            lastScale = scale;
            lastWidth = w;
            lastHueStart = hueStart;
            lastHueRange = hueRange;
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

        // Calculate which chunks are visible
        double worldLeft = posX - centerX * pixelSize;
        double worldRight = posX + (w - 1 - centerX) * pixelSize;
        double worldTop = posY + centerY * pixelSize;
        double worldBottom = posY - (h - 1 - centerY) * pixelSize;

        int chunkMinX = (int)floor(worldLeft / chunkWorldSize);
        int chunkMaxX = (int)floor(worldRight / chunkWorldSize);
        int chunkMinY = (int)floor(worldBottom / chunkWorldSize);
        int chunkMaxY = (int)floor(worldTop / chunkWorldSize);

        // Process each visible chunk
        for (int cy = chunkMinY; cy <= chunkMaxY; ++cy)
        {
            for (int cx = chunkMinX; cx <= chunkMaxX; ++cx)
            {
                auto key = std::make_pair(cx, cy);
                Chunk &chunk = chunks[key];

                // Calculate screen region for this chunk
                double chunkWorldLeft = cx * chunkWorldSize;
                double chunkWorldBottom = cy * chunkWorldSize;

                // Screen coords where chunk starts
                int screenStartX = (int)((chunkWorldLeft - posX) / pixelSize + centerX);
                int screenStartY = (int)(centerY - (chunkWorldBottom + chunkWorldSize - posY) / pixelSize);

                if (chunk.rendered)
                {
                    // Copy chunk data to image
                    for (int ly = 0; ly < CHUNK_SIZE; ++ly)
                    {
                        int screenY = screenStartY + ly;
                        if (screenY < 0 || screenY >= h) continue;

                        uint32_t *line = reinterpret_cast<uint32_t*>(image.scanLine(screenY));
                        int chunkRow = CHUNK_SIZE - 1 - ly;  // flip Y

                        for (int lx = 0; lx < CHUNK_SIZE; ++lx)
                        {
                            int screenX = screenStartX + lx;
                            if (screenX < 0 || screenX >= w) continue;

                            line[screenX] = chunk.data[chunkRow * CHUNK_SIZE + lx];
                        }
                    }
                }
                else
                {
                    // Fill with black and start rendering
                    for (int ly = 0; ly < CHUNK_SIZE; ++ly)
                    {
                        int screenY = screenStartY + ly;
                        if (screenY < 0 || screenY >= h) continue;

                        uint32_t *line = reinterpret_cast<uint32_t*>(image.scanLine(screenY));
                        for (int lx = 0; lx < CHUNK_SIZE; ++lx)
                        {
                            int screenX = screenStartX + lx;
                            if (screenX < 0 || screenX >= w) continue;
                            line[screenX] = 0x000000;
                        }
                    }

                    hasUnrenderedChunks = true;

                    if (!chunk.processing && activeThreads < numCores)
                    {
                        chunk.processing = true;
                        activeThreads++;
                        int gen = generation;
                        mpfr_prec_t precision = state->getRequiredPrecision();

                        // Compute chunk origin in high precision
                        MPFRFloat chunkOriginX(precision);
                        MPFRFloat chunkOriginY(precision);
                        chunkOriginX.set(state->posX);
                        chunkOriginY.set(state->posY);
                        // Offset from camera to chunk origin
                        double offsetX = chunkWorldLeft - posX;
                        double offsetY = chunkWorldBottom - posY;
                        chunkOriginX.add_inplace(offsetX);
                        chunkOriginY.add_inplace(offsetY);

                        std::thread([this, cx, cy, chunkOriginX = std::move(chunkOriginX),
                                     chunkOriginY = std::move(chunkOriginY), pixelSize, maxIter,
                                     hueStart, hueRange, precision, gen]() mutable {
                            renderChunk(cx, cy, chunkOriginX, chunkOriginY, pixelSize, maxIter,
                                       hueStart, hueRange, precision, gen);
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
    int lastHueStart = -1;
    int lastHueRange = -1;
    std::atomic<int> generation{0};

    void invalidateChunks()
    {
        generation++;
        chunks.clear();
    }

    // Render entire chunk using perturbation theory
    void renderChunk(int chunkX, int chunkY, const MPFRFloat& centerX, const MPFRFloat& centerY,
                     double pixelSize, int maxIter, int hueStart, int hueRange,
                     mpfr_prec_t precision, int gen)
    {
        // Render to local buffer first
        uint32_t localData[CHUNK_SIZE * CHUNK_SIZE];
        int localIterations[CHUNK_SIZE * CHUNK_SIZE];
        bool needsRecompute[CHUNK_SIZE * CHUNK_SIZE];

        // Compute chunk center in world coordinates (high precision)
        MPFRFloat chunkCenterX(precision);
        MPFRFloat chunkCenterY(precision);
        double halfChunk = (CHUNK_SIZE - 1) / 2.0 * pixelSize;
        chunkCenterX.set(centerX);
        chunkCenterX.add_inplace(halfChunk);
        chunkCenterY.set(centerY);
        chunkCenterY.add_inplace(halfChunk);

        // Compute reference orbit at chunk center
        auto [refOrbit, refEscapeIter] = computeReferenceOrbit(chunkCenterX, chunkCenterY, maxIter, precision);

        // Store reference orbits for potential glitch recovery
        std::vector<std::vector<ComplexD>> refOrbits = {refOrbit};
        std::vector<int> refEscapeIters = {refEscapeIter};
        std::vector<std::pair<double, double>> refOffsets = {{halfChunk, halfChunk}};  // offset from chunk origin

        // Chunk origin as double for computing pixel C values
        double originX = centerX.toDouble();
        double originY = centerY.toDouble();

        // First pass: render all pixels using primary reference
        int glitchCount = 0;
        for (int ly = 0; ly < CHUNK_SIZE; ++ly)
        {
            for (int lx = 0; lx < CHUNK_SIZE; ++lx)
            {
                int idx = ly * CHUNK_SIZE + lx;

                // Delta from reference point (chunk center)
                double deltaReal = (lx - (CHUNK_SIZE - 1) / 2.0) * pixelSize;
                double deltaImag = (ly - (CHUNK_SIZE - 1) / 2.0) * pixelSize;
                ComplexD deltaC(deltaReal, deltaImag);

                // Pixel's actual C value (for fallback iteration)
                ComplexD pixelC(originX + lx * pixelSize, originY + ly * pixelSize);

                bool glitched = false;
                int iter = perturbationIterate(refOrbit, refEscapeIter, deltaC, pixelC, maxIter, glitched);

                localIterations[idx] = iter;
                needsRecompute[idx] = glitched;
                if (glitched) glitchCount++;
            }
        }

        // Handle glitched pixels by computing additional reference points
        while (glitchCount > 0)
        {
            // Find a glitched pixel to use as new reference
            int newRefLx = -1, newRefLy = -1;
            for (int ly = 0; ly < CHUNK_SIZE && newRefLx < 0; ++ly)
            {
                for (int lx = 0; lx < CHUNK_SIZE; ++lx)
                {
                    if (needsRecompute[ly * CHUNK_SIZE + lx])
                    {
                        newRefLx = lx;
                        newRefLy = ly;
                        break;
                    }
                }
            }

            if (newRefLx < 0) break;

            // Compute new reference orbit at this pixel
            MPFRFloat newRefX(precision);
            MPFRFloat newRefY(precision);
            newRefX.set(centerX);
            newRefX.add_inplace(newRefLx * pixelSize);
            newRefY.set(centerY);
            newRefY.add_inplace(newRefLy * pixelSize);

            auto [newOrbit, newEscapeIter] = computeReferenceOrbit(newRefX, newRefY, maxIter, precision);

            refOrbits.push_back(newOrbit);
            refEscapeIters.push_back(newEscapeIter);
            refOffsets.push_back({newRefLx * pixelSize, newRefLy * pixelSize});

            // Re-render glitched pixels using new reference
            int fixedCount = 0;
            for (int ly = 0; ly < CHUNK_SIZE; ++ly)
            {
                for (int lx = 0; lx < CHUNK_SIZE; ++lx)
                {
                    int idx = ly * CHUNK_SIZE + lx;
                    if (!needsRecompute[idx]) continue;

                    // Delta from new reference point
                    double deltaReal = (lx * pixelSize) - refOffsets.back().first;
                    double deltaImag = (ly * pixelSize) - refOffsets.back().second;
                    ComplexD deltaC(deltaReal, deltaImag);

                    // Pixel's actual C value
                    ComplexD pixelC(originX + lx * pixelSize, originY + ly * pixelSize);

                    bool glitched = false;
                    int iter = perturbationIterate(newOrbit, newEscapeIter, deltaC, pixelC, maxIter, glitched);

                    if (!glitched)
                    {
                        localIterations[idx] = iter;
                        needsRecompute[idx] = false;
                        fixedCount++;
                    }
                }
            }

            glitchCount -= fixedCount;

            // Safety: prevent infinite loop if we can't fix glitches
            if (fixedCount == 0) break;
        }

        // Convert iterations to colors
        for (int i = 0; i < CHUNK_SIZE * CHUNK_SIZE; ++i)
        {
            int iter = localIterations[i];
            if (iter < 0)
            {
                localData[i] = 0x000000;  // In set
            }
            else
            {
                int hue = (hueStart + iter * hueRange / maxIter) % 360;
                localData[i] = hsvToRgb(hue);
            }
        }

        // Only store result if generation hasn't changed
        if (generation == gen)
        {
            auto key = std::make_pair(chunkX, chunkY);
            Chunk &chunk = chunks[key];
            memcpy(chunk.data, localData, sizeof(localData));
            chunk.referenceOrbits = std::move(refOrbits);
            chunk.referenceEscapeIter = std::move(refEscapeIters);
            chunk.rendered = true;
            chunk.processing = false;
        }
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

        // Scale buttons
        auto *scaleButtonLayout = new QHBoxLayout();
        btnHalfScale = new QPushButton("0.5x");
        btnDoubleScale = new QPushButton("2x");
        scaleButtonLayout->addWidget(btnHalfScale);
        scaleButtonLayout->addWidget(btnDoubleScale);
        layout->addLayout(scaleButtonLayout);

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
            .arg(state->posX.toDouble(), 0, 'f', 2)
            .arg(state->posY.toDouble(), 0, 'f', 2)
            .arg(state->scale.toDouble(), 0, 'f', 2));
        layout->addWidget(frameInfoText);

        layout->addStretch();
    }

    void updateInfo(double x, double y, double scale)
    {
        frameInfoText->setText(QString("X: %1\nY: %2\nScale: %3x")
            .arg(x, 0, 'g', 4)
            .arg(y, 0, 'g', 4)
            .arg(scale, 0, 'g', 4));
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
    QPushButton *btnHalfScale;
    QPushButton *btnDoubleScale;
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
            setFocus();
        });

        // Connect Clear button
        connect(sidebar->btnClear, &QPushButton::clicked, this, [this]() {
            resetState();
            setFocus();
        });

        // Connect scale buttons
        connect(sidebar->btnHalfScale, &QPushButton::clicked, this, [this]() {
            state->scale.mul_inplace(0.5);
            sidebar->updateInfo(state->posX.toDouble(), state->posY.toDouble(), state->scale.toDouble());
            fractalWidget->renderFractal();
            setFocus();
        });

        connect(sidebar->btnDoubleScale, &QPushButton::clicked, this, [this]() {
            state->scale.mul_inplace(2.0);
            sidebar->updateInfo(state->posX.toDouble(), state->posY.toDouble(), state->scale.toDouble());
            fractalWidget->renderFractal();
            setFocus();
        });

        // Connect sliders
        connect(sidebar->speedSlider, &QSlider::valueChanged, this, [this](int value) {
            state->speed = value;
        });
        connect(sidebar->speedSlider, &QSlider::sliderReleased, this, [this]() {
            setFocus();
        });

        connect(sidebar->maxIterSlider, &QSlider::valueChanged, this, [this](int value) {
            state->maxIterations = value;
            fractalWidget->renderFractal();
        });
        connect(sidebar->maxIterSlider, &QSlider::sliderReleased, this, [this]() {
            setFocus();
        });

        connect(sidebar->hueRangeSlider, &QSlider::valueChanged, this, [this](int value) {
            state->hueRange = value;
            fractalWidget->renderFractal();
        });
        connect(sidebar->hueRangeSlider, &QSlider::sliderReleased, this, [this]() {
            setFocus();
        });

        connect(sidebar->hueStartSlider, &QSlider::valueChanged, this, [this](int value) {
            state->hueStart = value;
            fractalWidget->renderFractal();
        });
        connect(sidebar->hueStartSlider, &QSlider::sliderReleased, this, [this]() {
            setFocus();
        });

        connect(sidebar->scaleSpeedSlider, &QSlider::sliderReleased, this, [this]() {
            setFocus();
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
        sidebar->updateInfo(state->posX.toDouble(), state->posY.toDouble(), state->scale.toDouble());
        fractalWidget->renderFractal();
    }

protected:
    void gameLoop()
    {
        // Movement speed scales with zoom - move same visual distance regardless of scale
        double moveSpeed = state->speed * 0.001 / state->scale.toDouble();
        bool changed = false;

        if (state->moveUp) {
            state->posY.add_inplace(moveSpeed);
            changed = true;
        }
        if (state->moveDown) {
            state->posY.add_inplace(-moveSpeed);
            changed = true;
        }
        if (state->moveLeft) {
            state->posX.add_inplace(-moveSpeed);
            changed = true;
        }
        if (state->moveRight) {
            state->posX.add_inplace(moveSpeed);
            changed = true;
        }

        // Update scale based on scaleSpeed
        // At max speed (1000), scale changes 3x per second (60 FPS)
        // 3^(1/60) ≈ 1.0186, so max multiplier per frame is ~0.0186
        if (state->scaleSpeed != 0) {
            double normalized = state->scaleSpeed / 1000.0;  // -1 to 1
            double curved = normalized * normalized * normalized;  // cubic: preserves sign
            double scaleMultiplier = pow(3.0, curved / 60.0);  // 3x per second at max
            state->scale.mul_inplace(scaleMultiplier);
            changed = true;
        }

        if (changed) {
            sidebar->updateInfo(state->posX.toDouble(), state->posY.toDouble(), state->scale.toDouble());
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

    void wheelEvent(QWheelEvent *event) override
    {
        // Scroll up = zoom in (double), scroll down = zoom out (half)
        if (event->angleDelta().y() > 0) {
            state->scale.mul_inplace(2.0);
        } else if (event->angleDelta().y() < 0) {
            state->scale.mul_inplace(0.5);
        }
        sidebar->updateInfo(state->posX.toDouble(), state->posY.toDouble(), state->scale.toDouble());
        fractalWidget->renderFractal();
    }

public:
    Sidebar2D *sidebar;
    FractalWidget2D *fractalWidget;
    QTimer *gameTimer;
    FractalState *state;
};

#endif // FRACTAL2D_H
