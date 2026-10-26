#pragma once

#include <aipb/map/model.h>

namespace aipb::map {

// Match the Inspector's exact cross-product test without an epsilon threshold.
inline bool zeroAreaTriangle(
  const Vec3 &a,
  const Vec3 &b,
  const Vec3 &c
) {
  const auto x = static_cast<long double>(b.x) - a.x;
  const auto y = static_cast<long double>(b.y) - a.y;
  const auto z = static_cast<long double>(b.z) - a.z;
  const auto u = static_cast<long double>(c.x) - a.x;
  const auto v = static_cast<long double>(c.y) - a.y;
  const auto w = static_cast<long double>(c.z) - a.z;

  return (y * w - z * v) == 0 && (z * u - x * w) == 0 && (x * v - y * u) == 0;
}

} // namespace aipb::map
