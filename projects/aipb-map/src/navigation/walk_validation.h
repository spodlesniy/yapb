#pragma once

#include <aipb/map/navigation/connectivity.h>

#include "brush_policy.h"

#include <cstddef>
#include <vector>

namespace aipb::map::navigation::detail {

enum class WalkFailure {
  None,
  Collision,
  Support,
  Slope,
  Step,
  Endpoint
};

struct WalkResult {
  WalkFailure failure { WalkFailure::None };
  std::size_t samples {};
  bool passed() const {
    return failure == WalkFailure::None;
  }
};

// Bounded, conservative ground-following hull probe; not a GoldSrc movement simulation.
WalkResult validateWalk(
  const collision::CollisionQueries &queries,
  ObjectId world,
  const std::vector<Brush> &brushes,
  const Candidate &from,
  const Candidate &to,
  std::size_t hull,
  const ConnectivityConfig &config
);

} // namespace aipb::map::navigation::detail
