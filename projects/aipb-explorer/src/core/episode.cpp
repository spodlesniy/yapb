#include <stdexcept>

#include <aipb/explorer/episode.h>

namespace aipb::explorer {

ExplorationEpisode::ExplorationEpisode(
  const MovementState &initial,
  std::size_t maxTransitions
)
  : initial_(initial), current_(initial), maxTransitions_(maxTransitions) {
  if (!isValid(initial) || maxTransitions == 0) {
    throw std::invalid_argument("ExplorationEpisode requires a valid initial state and positive capacity");
  }
}

const MovementState &ExplorationEpisode::initialState() const noexcept {
  return initial_;
}

const MovementState &ExplorationEpisode::currentState() const noexcept {
  return current_;
}

std::span<const ExplorationTransition> ExplorationEpisode::transitions() const noexcept {
  return transitions_;
}

bool ExplorationEpisode::isTerminal() const noexcept {
  return terminal_;
}

bool ExplorationEpisode::append(
  const MovementCommand &command,
  const MovementResult &result
) {
  if (terminal_ || transitions_.size() >= maxTransitions_ || !isValid(current_, result)) {
    return false;
  }

  if (!isValid(command) && result.status != StepStatus::Invalid) {
    return false;
  }

  transitions_.push_back(ExplorationTransition {
    .before = current_,
    .command = command,
    .result = result
  });
  current_ = result.state;
  terminal_ = result.status == StepStatus::Terminal;

  return true;
}

}
