[x] set up camera control
[ ] add cores
[ ] make drawing independent from sidebar
[ ] make drawing only on update, not every frame


You're right. There are several optimization techniques for deep Mandelbrot zooms:

1. Perturbation Theory (most important)

Instead of calculating each pixel independently, calculate ONE reference point with full precision (arbitrary precision math)
For other pixels, only calculate the difference (delta) from that reference
Deltas stay small, so you can use regular double for them
Reduces computation from O(pixels × iterations) with big floats to O(iterations) with big floats + O(pixels × iterations) with doubles
2. Series Approximation

Near the reference point, the first N iterations can be approximated by a polynomial
Skip those iterations entirely, jump straight to iteration N
Can skip thousands or millions of iterations this way
3. Period Detection

Points inside the set loop forever, but they often enter a cycle
Detect when z starts repeating → mark as inside immediately
Saves computing all maxIterations for interior points
4. Distance Estimation

Calculate how far a point is from the set boundary
If far outside → low iteration, skip detailed calculation
If far inside → mark as inside
5. Bounding/Subdivision

If a rectangular region is entirely one color, don't compute every pixel
Use quadtree subdivision
For 10^190 zoom, you'd typically use:

Arbitrary precision library (GMP, MPFR) for the reference orbit
Perturbation + series approximation for the bulk of computation
Still needs thousands of iterations but way fewer than naive approach
This is how programs like Kalles Fraktaler and Mandel Machine achieve deep zooms.
