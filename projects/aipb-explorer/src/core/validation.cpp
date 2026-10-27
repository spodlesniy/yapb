#include <cmath>
#include <cstdint>
#include <limits>

#include <aipb/explorer/movement_command.h>
#include <aipb/explorer/movement_result.h>
#include <aipb/explorer/movement_state.h>

namespace aipb::explorer {

namespace {

[[nodiscard]] bool finite(const Vec3 &value) noexcept {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool normalizedAxis(float value) noexcept {
  return std::isfinite(value) && value >= -1.0F && value <= 1.0F;
}

} // namespace

bool isValid(const MovementState &state) noexcept {
  const bool validStance = state.stance == Stance::Standing || state.stance == Stance::Crouching;

  return finite(state.position) && finite(state.velocity) && std::isfinite(state.yaw) && state.yaw >= -180.0F
    && state.yaw < 180.0F && std::isfinite(state.pitch) && state.pitch >= -90.0F && state.pitch <= 90.0F
    && validStance;
}

bool isValid(const MovementCommand &command) noexcept {
  return normalizedAxis(command.forward) && normalizedAxis(command.strafe) && std::isfinite(command.yawDelta)
    && command.yawDelta >= -180.0F && command.yawDelta <= 180.0F;
}

bool isValid(
  const MovementState &previous,
  const MovementResult &result
) noexcept {
  if (!isValid(previous) || !isValid(result.state) || (result.startedSolid && !result.collided)) {
    return false;
  }

  switch (result.status) {
  case StepStatus::Invalid:
    return result.state == previous && !result.collided && !result.startedSolid;
  case StepStatus::Blocked:
    if (!result.collided) {
      return false;
    }
    break;
  case StepStatus::Applied:
  case StepStatus::Terminal:
    break;
  default:
    return false;
  }

  return previous.tick < std::numeric_limits<std::uint64_t>::max() && result.state.tick == previous.tick + 1;
}

}
