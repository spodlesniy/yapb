#pragma once

namespace aipb::explorer {

// Distances and velocities use GoldSrc units and units per second.
struct Vec3 {
  double x {};
  double y {};
  double z {};

  bool operator==(const Vec3 &) const = default;
};

}
