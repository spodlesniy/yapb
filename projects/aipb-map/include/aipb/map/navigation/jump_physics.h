#pragma once

#include <optional>

namespace aipb::map::navigation {

struct JumpPhysics {
  double gravity { 800.0 };
  double jumpApex { 45.0 };
  double maximumHorizontalSpeed { 250.0 };
  double standingHalfHeight { 36.0 };
  double crouchingHalfHeight { 18.0 };
};

// GoldSrc's default jump impulse; game-specific movevars must be verified in-engine.
double jumpImpulse(const JumpPhysics &physics);

// Maximum height of a ledge reachable by the specified collision hull,
// based only on free-flight geometry, not collision or acceleration.
double theoreticalJumpRise(
  const JumpPhysics &physics,
  bool crouched
);

// Time when descending to the required player origin height.
// No timing is possible above the ballistic apex.
std::optional<double> descendingJumpTime(
  const JumpPhysics &physics,
  double targetOriginRise
);

// Vertical movement relative to the standing jump origin at elapsed time.
double jumpDisplacement(
  const JumpPhysics &physics,
  double seconds
);

} // namespace aipb::map::navigation
