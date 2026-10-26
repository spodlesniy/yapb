#include <aipb/map/navigation/jump_physics.h>

#include <cmath>
#include <stdexcept>

namespace aipb::map::navigation {
namespace {

void requirePhysics(const JumpPhysics &physics) {
  if (!std::isfinite(physics.gravity) || physics.gravity <= 0 ||
      !std::isfinite(physics.jumpApex) || physics.jumpApex <= 0 ||
      !std::isfinite(physics.maximumHorizontalSpeed) || physics.maximumHorizontalSpeed <= 0 ||
      !std::isfinite(physics.standingHalfHeight) || physics.standingHalfHeight <= 0 ||
      !std::isfinite(physics.crouchingHalfHeight) || physics.crouchingHalfHeight <= 0 ||
      physics.crouchingHalfHeight >= physics.standingHalfHeight ||
      !std::isfinite(physics.airborneDuckDelay) || physics.airborneDuckDelay < 0) {
    throw std::invalid_argument("invalid GoldSrc jump physics profile");
  }
}

} // namespace

double jumpImpulse(const JumpPhysics &physics) {
  requirePhysics(physics);

  return std::sqrt(2.0 * physics.gravity * physics.jumpApex);
}

double theoreticalJumpRise(
  const JumpPhysics &physics,
  bool crouched
) {
  requirePhysics(physics);

  return physics.jumpApex +
         (crouched ? physics.standingHalfHeight - physics.crouchingHalfHeight : 0.0);
}

std::optional<double> descendingJumpTime(
  const JumpPhysics &physics,
  double targetOriginRise
) {
  requirePhysics(physics);

  if (!std::isfinite(targetOriginRise)) {
    throw std::invalid_argument("nonfinite jump landing height");
  }
  const double speed = jumpImpulse(physics);
  const double discriminant = speed * speed - 2.0 * physics.gravity * targetOriginRise;

  if (discriminant < 0) {
    return std::nullopt;
  }

  return (speed + std::sqrt(discriminant)) / physics.gravity;
}

std::optional<JumpLandingPlan> planJumpLanding(
  const JumpPhysics &physics,
  double floorRise,
  double planarDistance,
  bool crouched
) {
  requirePhysics(physics);

  if (!std::isfinite(floorRise) || !std::isfinite(planarDistance) ||
      planarDistance <= 0) {
    throw std::invalid_argument("invalid jump landing displacement");
  }

  if (floorRise > theoreticalJumpRise(physics, crouched)) {
    return std::nullopt;
  }

  const double duckHeight = crouched ?
    physics.standingHalfHeight - physics.crouchingHalfHeight : 0.0;
  const double originRise = floorRise - duckHeight;
  const auto descending = descendingJumpTime(physics, originRise);

  if (!descending || *descending <= physics.airborneDuckDelay * static_cast<double>(crouched)) {
    return std::nullopt;
  }
  const double speed = planarDistance / *descending;

  if (!std::isfinite(speed) || speed > physics.maximumHorizontalSpeed) {
    return std::nullopt;
  }

  return JumpLandingPlan {
    .flightSeconds = *descending,
    .requiredHorizontalSpeed = speed,
    .targetOriginRise = originRise
  };
}

double jumpDisplacement(
  const JumpPhysics &physics,
  double seconds
) {
  requirePhysics(physics);

  if (!std::isfinite(seconds) || seconds < 0) {
    throw std::invalid_argument("invalid jump flight time");
  }

  return jumpImpulse(physics) * seconds - 0.5 * physics.gravity * seconds * seconds;
}

} // namespace aipb::map::navigation
