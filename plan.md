[ ] set up camera control


1. color scheme
h = 360 * t;
s = 1.0;
v = 1.0;
rgb = hsv_to_rgb(h, s, v);

2. different zooms
| Zoom level     | Technique                           |
| -------------- | ----------------------------------- |
| ≤ 10¹²         | `float64`                           |
| 10¹² – 10⁵⁰    | Big floats                          |
| 10⁵⁰ – 10¹⁰⁰⁰⁰ | Perturbation + big float reference  |
| Extreme        | Series approximation + perturbation |
