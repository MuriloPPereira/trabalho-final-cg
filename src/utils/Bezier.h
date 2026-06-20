#ifndef UTILS_BEZIER_H
#define UTILS_BEZIER_H

template <typename Vector>
inline Vector EvaluateCubicBezier(float t, const Vector &p0, const Vector &p1,
                                  const Vector &p2, const Vector &p3) {
  const float u = 1.0f - t;
  return (u * u * u) * p0 + 3.0f * (u * u) * t * p1 +
         3.0f * u * (t * t) * p2 + (t * t * t) * p3;
}

template <typename Vector>
inline Vector EvaluateCubicBezierDerivative(float t, const Vector &p0,
                                            const Vector &p1,
                                            const Vector &p2,
                                            const Vector &p3) {
  const float u = 1.0f - t;
  return 3.0f * u * u * (p1 - p0) + 6.0f * u * t * (p2 - p1) +
         3.0f * t * t * (p3 - p2);
}

#endif
