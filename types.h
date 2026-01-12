#ifndef TYPES_H
#define TYPES_H

#include <QString>

struct FractalState
{
    QString formula;
    double posX = 0.0;
    double posY = 0.0;
    double posZ = 0.0;  // for 3D
    double scale = 1.0;
    int speed = 1;      // 0 to 100
    int scaleSlider = 0; // -1000 to 1000

    // Movement flags
    bool moveUp = false;
    bool moveDown = false;
    bool moveLeft = false;
    bool moveRight = false;
};

#endif // TYPES_H
