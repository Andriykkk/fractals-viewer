#ifndef TYPES_H
#define TYPES_H

#include <QString>

struct FractalState
{
    QString formula;
    double posX = 0.0;
    double posY = 0.0;
    double posZ = 0.0;  // for 3D
    double scale = 1.0;        // actual scale for drawing
    int speed = 1;             // movement speed 0 to 100
    int scaleSpeed = 0;        // scale change speed -1000 to 1000

    // Movement flags
    bool moveUp = false;
    bool moveDown = false;
    bool moveLeft = false;
    bool moveRight = false;

    void clear()
    {
        formula.clear();
        posX = 0.0;
        posY = 0.0;
        posZ = 0.0;
        scale = 1.0;
        speed = 1;
        scaleSpeed = 0;
        moveUp = false;
        moveDown = false;
        moveLeft = false;
        moveRight = false;
    }
};

#endif // TYPES_H
