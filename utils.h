#ifndef UTILS_H
#define UTILS_H

#include <cmath>

// Converts linear slider value (-1000 to 1000) to exponential scale
// Slow around 0, fast at extremes
inline double sliderToScale(int sliderValue)
{
    // Normalize to -1 to 1
    double normalized = sliderValue / 1000.0;
    // Apply cubic curve: preserves sign, slow near 0, fast at edges
    double curved = normalized * normalized * normalized;
    // Map to useful scale range (e.g., 0.001 to 1000)
    // At slider 0: scale = 1.0
    // At slider +1000: scale = 1000.0
    // At slider -1000: scale = 0.001
    return std::pow(10.0, curved * 3.0);
}

#endif // UTILS_H
