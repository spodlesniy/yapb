#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include <aipb/explorer/movement_command.h>
#include <aipb/explorer/movement_result.h>
#include <aipb/explorer/movement_state.h>

using namespace aipb::explorer;

static void require(bool value) {
  if (!value) {
    throw std::runtime_error("contract assertion failed");
  }
}

int main() {
  MovementState state {};
  require(isValid(state));
  state.position.x = std::numeric_limits<double>::quiet_NaN();
  require(!isValid(state));
  state.position.x = 0;
  state.velocity.y = std::numeric_limits<double>::infinity();
  require(!isValid(state));
  state.velocity.y = 0;
  state.yaw = 180;
  require(!isValid(state));
  state.yaw = -180;
  require(isValid(state));
  state.pitch = 91;
  require(!isValid(state));
  state.pitch = -90;
  require(isValid(state));
  state.stance = static_cast<Stance>(99);
  require(!isValid(state));
  state.stance = Stance::Crouching;

  MovementCommand command {};
  require(isValid(command));
  command.forward = 1;
  command.strafe = -1;
  command.yawDelta = 180;
  require(isValid(command));
  command.forward = 2;
  require(!isValid(command));
  command.forward = 0;
  command.strafe = -2;
  require(!isValid(command));
  command.strafe = 0;
  command.yawDelta = std::numeric_limits<float>::quiet_NaN();
  require(!isValid(command));

  MovementResult result { .state = state, .status = StepStatus::Invalid };
  require(isValid(state, result));
  result.state.position.x = 2;
  require(!isValid(state, result));
  result.state = state;
  result.status = StepStatus::Applied;
  require(!isValid(state, result));
  result.state.tick = 1;
  require(isValid(state, result));
  result.status = StepStatus::Blocked;
  require(!isValid(state, result));
  result.collided = true;
  require(isValid(state, result));
  result.startedSolid = true;
  result.collided = false;
  require(!isValid(state, result));
  result.startedSolid = false;
  result.status = static_cast<StepStatus>(44);
  require(!isValid(state, result));
  state.tick = std::numeric_limits<std::uint64_t>::max();
  result.state.tick = 0;
  result.status = StepStatus::Applied;
  require(!isValid(state, result));
}
