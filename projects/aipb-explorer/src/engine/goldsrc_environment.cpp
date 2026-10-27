#include <cmath>
#include <stdexcept>

#include <aipb/explorer/goldsrc_environment.h>

namespace aipb::explorer {
namespace {

AipbExplorerBridgeState encode(const MovementState &state) {
  return {
    state.position.x, state.position.y, state.position.z,
    state.velocity.x, state.velocity.y, state.velocity.z,
    state.yaw, state.pitch, state.tick,
    static_cast<uint8_t>(state.onGround),
    static_cast<uint8_t>(state.stance == Stance::Crouching)
  };
}

MovementState decode(const AipbExplorerBridgeState &state) {
  return {
    .position = { state.x, state.y, state.z },
    .velocity = { state.vx, state.vy, state.vz },
    .yaw = state.yaw,
    .pitch = state.pitch,
    .stance = state.crouching != 0 ? Stance::Crouching : Stance::Standing,
    .onGround = state.on_ground != 0,
    .tick = state.tick
  };
}

} // namespace

GoldSrcEnvironment::GoldSrcEnvironment(AipbExplorerBridgeApi bridge) : bridge_(bridge) {
  if (!std::isfinite(bridge_.tick_seconds) || bridge_.tick_seconds <= 0.0 || bridge_.step == nullptr) {
    throw std::invalid_argument("GoldSrc bridge needs a positive fixed tick and step callback");
  }
}

double GoldSrcEnvironment::tickSeconds() const noexcept {
  return bridge_.tick_seconds;
}

MovementState GoldSrcEnvironment::reset(const MovementState &initial) {
  if (!isValid(initial)) {
    throw std::invalid_argument("invalid GoldSrc initial state");
  }
  if (bridge_.reset == nullptr) {
    throw std::runtime_error("GoldSrc bridge cannot reset a physical player");
  }
  const auto requested = encode(initial);
  AipbExplorerBridgeState observed {};
  if (!bridge_.reset(bridge_.context, &requested, &observed)) {
    throw std::runtime_error("GoldSrc bridge rejected reset");
  }
  const auto result = decode(observed);
  if (!isValid(result)) {
    throw std::runtime_error("GoldSrc bridge returned invalid initial state");
  }
  current_ = result;
  initialized_ = true;
  terminal_ = false;
  return current_;
}

MovementResult GoldSrcEnvironment::step(const MovementCommand &command) {
  if (!initialized_ || terminal_ || !isValid(command)) {
    return { .state = current_, .status = StepStatus::Invalid };
  }
  const AipbExplorerBridgeCommand request {
    command.forward, command.strafe, command.yawDelta,
    static_cast<uint8_t>(command.jump), static_cast<uint8_t>(command.duck)
  };
  AipbExplorerBridgeStep response {};
  if (!bridge_.step(bridge_.context, &request, &response)) {
    return { .state = current_, .status = StepStatus::Invalid };
  }
  if (response.status > static_cast<uint8_t>(StepStatus::Terminal)
      || response.collided > 1 || response.started_solid > 1
      || response.state.on_ground > 1 || response.state.crouching > 1) {
    throw std::runtime_error("GoldSrc bridge returned malformed flags");
  }
  const MovementResult result {
    .state = decode(response.state),
    .status = static_cast<StepStatus>(response.status),
    .collided = response.collided != 0,
    .startedSolid = response.started_solid != 0
  };
  if (!isValid(current_, result)) {
    throw std::runtime_error("GoldSrc bridge violated the movement tick contract");
  }
  current_ = result.state;
  terminal_ = result.status == StepStatus::Terminal;
  return result;
}

}
