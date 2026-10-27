#pragma once

#include <optional>

namespace aipb::map::navigation {

struct JumpPhysics {
  double gravity { 800.0 };
  double jumpApex { 45.0 };
  double maximumHorizontalSpeed { 250.0 };
  double groundAcceleration { 10.0 };
  double standingHalfHeight { 36.0 };
  double crouchingHalfHeight { 18.0 };
  double airborneDuckDelay { 0.04 };
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

// A candidate landing plan, using the unchanged origin during an airborne duck.
struct JumpLandingPlan {
  double flightSeconds {};
  double requiredHorizontalSpeed {};
  double targetOriginRise {};
};

// Crouched landing shifts the feet-to-origin relation, not airborne origin.
std::optional<JumpLandingPlan> planJumpLanding(
  const JumpPhysics &physics,
  double floorRise,
  double planarDistance,
  bool crouched
);

// Optimistic upper bound from rest over a supported straight approach.
// Ignores ground friction, input frame quantization and prior momentum.
double takeoffSpeedUpperBound(
  const JumpPhysics &physics,
  double approachDistance
);

// Vertical movement relative to the standing jump origin at elapsed time.
double jumpDisplacement(
  const JumpPhysics &physics,
  double seconds
);

} // namespace aipb::map::navigation
