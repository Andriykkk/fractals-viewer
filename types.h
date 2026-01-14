#ifndef TYPES_H
#define TYPES_H

#include <QString>
#include <mpfr.h>
#include <cmath>
#include <algorithm>

// RAII wrapper for MPFR high-precision float
class MPFRFloat
{
public:
    mpfr_t value;

    explicit MPFRFloat(mpfr_prec_t precision = 128)
    {
        mpfr_init2(value, precision);
        mpfr_set_d(value, 0.0, MPFR_RNDN);
    }

    MPFRFloat(double d, mpfr_prec_t precision = 128)
    {
        mpfr_init2(value, precision);
        mpfr_set_d(value, d, MPFR_RNDN);
    }

    ~MPFRFloat()
    {
        mpfr_clear(value);
    }

    // Copy constructor
    MPFRFloat(const MPFRFloat& other)
    {
        mpfr_init2(value, mpfr_get_prec(other.value));
        mpfr_set(value, other.value, MPFR_RNDN);
    }

    // Copy assignment
    MPFRFloat& operator=(const MPFRFloat& other)
    {
        if (this != &other)
        {
            mpfr_set_prec(value, mpfr_get_prec(other.value));
            mpfr_set(value, other.value, MPFR_RNDN);
        }
        return *this;
    }

    // Move constructor
    MPFRFloat(MPFRFloat&& other) noexcept
    {
        mpfr_init2(value, mpfr_get_prec(other.value));
        mpfr_swap(value, other.value);
    }

    // Move assignment
    MPFRFloat& operator=(MPFRFloat&& other) noexcept
    {
        if (this != &other)
        {
            mpfr_swap(value, other.value);
        }
        return *this;
    }

    void set(double d) { mpfr_set_d(value, d, MPFR_RNDN); }
    void set(const MPFRFloat& other) { mpfr_set(value, other.value, MPFR_RNDN); }
    double toDouble() const { return mpfr_get_d(value, MPFR_RNDN); }

    mpfr_prec_t getPrecision() const { return mpfr_get_prec(value); }
    void setPrecision(mpfr_prec_t prec) { mpfr_prec_round(value, prec, MPFR_RNDN); }

    // Arithmetic: result = a op b
    void add(const MPFRFloat& a, const MPFRFloat& b) { mpfr_add(value, a.value, b.value, MPFR_RNDN); }
    void sub(const MPFRFloat& a, const MPFRFloat& b) { mpfr_sub(value, a.value, b.value, MPFR_RNDN); }
    void mul(const MPFRFloat& a, const MPFRFloat& b) { mpfr_mul(value, a.value, b.value, MPFR_RNDN); }
    void div(const MPFRFloat& a, const MPFRFloat& b) { mpfr_div(value, a.value, b.value, MPFR_RNDN); }

    // Arithmetic with double
    void mul_d(const MPFRFloat& a, double d) { mpfr_mul_d(value, a.value, d, MPFR_RNDN); }
    void add_d(const MPFRFloat& a, double d) { mpfr_add_d(value, a.value, d, MPFR_RNDN); }
    void div_d(const MPFRFloat& a, double d) { mpfr_div_d(value, a.value, d, MPFR_RNDN); }

    // In-place operations
    void add_inplace(double d) { mpfr_add_d(value, value, d, MPFR_RNDN); }
    void mul_inplace(double d) { mpfr_mul_d(value, value, d, MPFR_RNDN); }

    // Increase precision if needed (preserves value)
    void ensurePrecision(mpfr_prec_t minPrec)
    {
        if (mpfr_get_prec(value) < minPrec)
        {
            mpfr_prec_round(value, minPrec, MPFR_RNDN);
        }
    }
};

struct FractalState
{
    QString formula;
    MPFRFloat posX{0.0};
    MPFRFloat posY{0.0};
    MPFRFloat posZ{0.0};  // for 3D
    MPFRFloat scale{1.0};
    int speed = 1;             // movement speed 0 to 100
    int scaleSpeed = 0;        // scale change speed -1000 to 1000

    // Color settings
    int hueRange = 240;        // how much of HSV to use (0-360)
    int hueStart = 0;          // starting hue position (0-360)

    int maxIterations = 100;   // 5 to 1000

    // Movement flags
    bool moveUp = false;
    bool moveDown = false;
    bool moveLeft = false;
    bool moveRight = false;

    void clear()
    {
        formula.clear();
        posX.set(0.0);
        posY.set(0.0);
        posZ.set(0.0);
        scale.set(1.0);
        speed = 1;
        scaleSpeed = 0;
        moveUp = false;
        moveDown = false;
        moveLeft = false;
        moveRight = false;
    }

    // Calculate required precision based on scale (without losing precision)
    mpfr_prec_t getRequiredPrecision() const
    {
        // Get exponent directly from MPFR (log2 of scale, roughly)
        mpfr_exp_t exp = mpfr_get_exp(scale.value);
        if (exp <= 0) return 128;
        // Need exp + 64 bits, round up to next 64-bit boundary
        mpfr_prec_t prec = ((exp + 64 + 63) / 64) * 64;
        return std::max(prec, (mpfr_prec_t)128);
    }

    // Update precision of position values when scale increases
    void updatePrecision()
    {
        mpfr_prec_t needed = getRequiredPrecision();
        mpfr_prec_t current = scale.getPrecision();
        if (needed > current)
        {
            posX.ensurePrecision(needed);
            posY.ensurePrecision(needed);
            posZ.ensurePrecision(needed);
            scale.ensurePrecision(needed);
        }
    }
};

#endif // TYPES_H
