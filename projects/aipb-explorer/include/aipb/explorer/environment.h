#pragma once

#include <aipb/explorer/movement_command.h>
#include <aipb/explorer/movement_result.h>

namespace aipb::explorer {

// Implementations own movement timing and may use different execution backends.
class IExplorationEnvironment {
public:
  virtual ~IExplorationEnvironment() = default;

  // Fixed, positive duration in seconds; must not change during an episode.
  [[nodiscard]] virtual double tickSeconds() const noexcept = 0;

  // Reset to a valid tick-boundary state or report an unsupported reset.
  virtual MovementState reset(const MovementState &initial) = 0;

  // Apply one tick. Invalid commands must not advance the tick or state.
  virtual MovementResult step(const MovementCommand &command) = 0;
};

}
