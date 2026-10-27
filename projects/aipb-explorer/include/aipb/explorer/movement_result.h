#pragma once

#include <aipb/explorer/movement_state.h>

namespace aipb::explorer {

enum class StepStatus {
  Applied,
  Blocked,
  Invalid,
  Terminal
};

struct MovementResult {
  MovementState state {};
  StepStatus status { StepStatus::Applied };
  bool collided {};
  bool startedSolid {};
};

// Validates tick accounting and collision flags relative to the previous state.
[[nodiscard]] bool isValid(
  const MovementState &previous,
  const MovementResult &result
) noexcept;

}
