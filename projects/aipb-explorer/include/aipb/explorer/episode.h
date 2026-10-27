#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <aipb/explorer/movement_command.h>
#include <aipb/explorer/movement_result.h>

namespace aipb::explorer {

struct ExplorationTransition {
  MovementState before {};
  MovementCommand command {};
  MovementResult result {};
};

// Bounded in-memory evidence of executed and rejected commands.
class ExplorationEpisode {
public:
  explicit ExplorationEpisode(
    const MovementState &initial,
    std::size_t maxTransitions = 100'000
  );

  [[nodiscard]] const MovementState &initialState() const noexcept;
  [[nodiscard]] const MovementState &currentState() const noexcept;
  [[nodiscard]] std::span<const ExplorationTransition> transitions() const noexcept;
  [[nodiscard]] bool isTerminal() const noexcept;

  // False means invalid ordering, inconsistent result or exhausted capacity.
  [[nodiscard]] bool append(
    const MovementCommand &command,
    const MovementResult &result
  );

private:
  MovementState initial_ {};
  MovementState current_ {};
  std::size_t maxTransitions_ {};
  std::vector<ExplorationTransition> transitions_ {};
  bool terminal_ {};
};

}
