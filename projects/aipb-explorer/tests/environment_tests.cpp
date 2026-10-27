#include <cstdio>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include <aipb/explorer/environment.h>
#include <aipb/explorer/episode.h>

using namespace aipb::explorer;

static void require(bool value) {
  if (!value) {
    throw std::runtime_error("environment assertion failed");
  }
}

class MockEnvironment final : public IExplorationEnvironment {
public:
  [[nodiscard]] double tickSeconds() const noexcept override {
    return 0.01;
  }

  MovementState reset(const MovementState &initial) override {
    if (!isValid(initial)) {
      throw std::invalid_argument("invalid initial");
    }
    state_ = initial;
    terminal_ = false;
    return state_;
  }

  MovementResult step(const MovementCommand &command) override {
    if (!isValid(command) || terminal_ || state_.tick == std::numeric_limits<std::uint64_t>::max()) {
      return MovementResult { .state = state_, .status = StepStatus::Invalid };
    }
    ++state_.tick;
    const bool blocked = state_.position.x + command.forward > 2.0;
    if (!blocked) {
      state_.position.x += command.forward;
    }
    terminal_ = state_.position.x <= -1.0;
    return MovementResult {
      .state = state_,
      .status = terminal_ ? StepStatus::Terminal : (blocked ? StepStatus::Blocked : StepStatus::Applied),
      .collided = blocked
    };
  }

private:
  MovementState state_ {};
  bool terminal_ {};
};

int runTests() {
  MockEnvironment mock;
  IExplorationEnvironment &environment = mock;
  require(environment.tickSeconds() == 0.01);
  const MovementState initial = environment.reset(MovementState {});
  ExplorationEpisode episode(initial, 5);
  require(episode.transitions().empty());
  const MovementCommand forward { .forward = 1 };
  const auto first = environment.step(forward);
  require(isValid(initial, first));
  require(episode.append(forward, first));
  require(episode.currentState().tick == 1);
  const MovementCommand invalid { .forward = 2 };
  const auto rejected = environment.step(invalid);
  require(rejected.status == StepStatus::Invalid);
  require(episode.append(invalid, rejected));
  require(episode.currentState().tick == 1);
  // A valid command can also be rejected by the environment without consuming a tick.
  const MovementResult denied {
    .state = episode.currentState(),
    .status = StepStatus::Invalid
  };
  require(episode.append(forward, denied));
  require(episode.currentState().tick == 1);
  const MovementResult malformed {
    .state = episode.currentState(),
    .status = StepStatus::Applied
  };
  require(!episode.append(forward, malformed));
  require(episode.transitions().size() == 3);
  const auto second = environment.step(forward);
  require(episode.append(forward, second));
  const auto blocked = environment.step(forward);
  require(blocked.status == StepStatus::Blocked);
  require(episode.append(forward, blocked));
  require(episode.transitions().size() == 5);
  require(!episode.append(forward, environment.step(forward)));
  const MovementState reset = environment.reset(initial);
  ExplorationEpisode terminal(reset);
  const MovementCommand backwards { .forward = -1 };
  require(terminal.append(backwards, environment.step(backwards)));
  require(terminal.isTerminal());
  require(!terminal.append(backwards, environment.step(backwards)));
  bool thrown = false;
  try {
    ExplorationEpisode bad(initial, 0);
    (void)bad;
  } catch (const std::invalid_argument &) {
    thrown = true;
  }
  require(thrown);
}

int main() {
  try {
    return runTests();
  } catch (const std::exception &error) {
    std::fprintf(stderr, "aipb_explorer_environment: %s\n", error.what());
    return 1;
  }
}
