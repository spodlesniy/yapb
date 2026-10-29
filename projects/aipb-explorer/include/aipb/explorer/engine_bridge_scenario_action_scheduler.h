#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include <aipb/explorer/engine_bridge_scenario_run_gate.h>

namespace aipb::explorer::scenario {

enum class ActionScheduleStatus {
  Idle,
  Running,
  Completed,
  Cancelled,
  InvalidScenario,
  InvalidClock,
  ActionTimeout,
  ScenarioTimeout,
  FrameLimit
};

inline const char *actionScheduleStatusName(ActionScheduleStatus status) {
  switch (status) {
  case ActionScheduleStatus::Idle: return "Idle";
  case ActionScheduleStatus::Running: return "Running";
  case ActionScheduleStatus::Completed: return "Completed";
  case ActionScheduleStatus::Cancelled: return "Cancelled";
  case ActionScheduleStatus::InvalidScenario: return "InvalidScenario";
  case ActionScheduleStatus::InvalidClock: return "InvalidClock";
  case ActionScheduleStatus::ActionTimeout: return "ActionTimeout";
  case ActionScheduleStatus::ScenarioTimeout: return "ScenarioTimeout";
  case ActionScheduleStatus::FrameLimit: return "FrameLimit";
  }

  return "InvalidScenario";
}

struct ScheduledActionFrame {
  Action action {};
  std::size_t actionIndex {};
  int milliseconds {};
  int frameIndex {};
  std::uint64_t ticket {};
};

// A pure scheduler: frames are never executed here. An adapter must submit a
// prepared frame to its engine before confirming it exactly once.
class ActionScheduler {
public:
  static constexpr int kMaxFrames = 4096;
  static constexpr int kMaxFrameMs = 255;

  bool begin(const Scenario &scenario, const RunStartDecision &admission,
    std::int64_t nowMs) {
    if (running()) {
      return false;
    }

    clear();

    if (!admission.ready() || !validScenario(scenario) || nowMs < 0) {
      status_ = ActionScheduleStatus::InvalidScenario;
      return false;
    }

    for (const auto &action : scenario.actions) {
      if (action.attack || action.attack2 || action.use || action.select != "none") {
        status_ = ActionScheduleStatus::InvalidScenario;
        return false;
      }
    }

    scenario_ = scenario;
    startedAtMs_ = nowMs;
    actionStartedAtMs_ = nowMs;
    lastSeenMs_ = nowMs;
    status_ = ActionScheduleStatus::Running;
    return true;
  }

  // Plan at most one GoldSrc frame. The remaining action and scenario budgets
  // constrain msec; switching actions never inserts a synthetic idle frame.
  ScheduledActionFrame prepareFrame(std::int64_t nowMs, int requestedMs) {
    if (!running() || pending_ || requestedMs < 1 || requestedMs > kMaxFrameMs) {
      return {};
    }

    if (!validateClock(nowMs)) {
      return {};
    }

    if (frames_ >= kMaxFrames) {
      fail(ActionScheduleStatus::FrameLimit);
      return {};
    }

    if (nowMs - startedAtMs_ >= scenario_.timeoutMs) {
      fail(ActionScheduleStatus::ScenarioTimeout);
      return {};
    }

    const auto &action = scenario_.actions[actionIndex_];

    if (nowMs - actionStartedAtMs_ >= action.timeoutMs) {
      fail(ActionScheduleStatus::ActionTimeout);
      return {};
    }

    const int milliseconds = std::min({
      requestedMs,
      action.durationMs - actionMs_,
      scenario_.timeoutMs - totalMs_
    });

    if (milliseconds <= 0) {
      fail(ActionScheduleStatus::ScenarioTimeout);
      return {};
    }

    ++nextTicket_;
    pendingFrame_ = { action, actionIndex_, milliseconds, frames_, nextTicket_ };
    pendingAtMs_ = nowMs;
    pending_ = true;
    return pendingFrame_;
  }

  // A successfully applied engine frame consumes its exact prepared msec.
  // Deadline failures after application are terminal but retain frame counts.
  bool commitFrame(const ScheduledActionFrame &frame, std::int64_t nowMs) {
    if (!running() || !pending_
      || frame.ticket == 0 || frame.ticket != pendingFrame_.ticket
      || frame.actionIndex != pendingFrame_.actionIndex
      || frame.frameIndex != pendingFrame_.frameIndex
      || frame.milliseconds != pendingFrame_.milliseconds) {
      return false;
    }

    if (nowMs < pendingAtMs_ || !validateClock(nowMs)) {
      fail(ActionScheduleStatus::InvalidClock);
      return false;
    }

    pending_ = false;
    ++frames_;
    actionMs_ += pendingFrame_.milliseconds;
    totalMs_ += pendingFrame_.milliseconds;

    if (nowMs - startedAtMs_ > scenario_.timeoutMs) {
      fail(ActionScheduleStatus::ScenarioTimeout);
      return true;
    }

    if (nowMs - actionStartedAtMs_ > scenario_.actions[actionIndex_].timeoutMs) {
      fail(ActionScheduleStatus::ActionTimeout);
      return true;
    }

    if (actionMs_ == scenario_.actions[actionIndex_].durationMs) {
      ++actionIndex_;
      actionMs_ = 0;

      if (actionIndex_ == scenario_.actions.size()) {
        status_ = ActionScheduleStatus::Completed;
      }
      else {
        actionStartedAtMs_ = nowMs;
      }
    }
    else if (nowMs - startedAtMs_ >= scenario_.timeoutMs) {
      fail(ActionScheduleStatus::ScenarioTimeout);
    }
    else if (nowMs - actionStartedAtMs_ >= scenario_.actions[actionIndex_].timeoutMs) {
      fail(ActionScheduleStatus::ActionTimeout);
    }

    return true;
  }

  bool cancel() {
    if (!running()) {
      return false;
    }

    fail(ActionScheduleStatus::Cancelled);
    return true;
  }

  bool running() const {
    return status_ == ActionScheduleStatus::Running;
  }

  bool pending() const {
    return pending_;
  }

  ActionScheduleStatus status() const {
    return status_;
  }

  std::size_t actionIndex() const {
    return actionIndex_;
  }

  int actionMilliseconds() const {
    return actionMs_;
  }

  int totalMilliseconds() const {
    return totalMs_;
  }

  int frames() const {
    return frames_;
  }

  int botIndex() const {
    return scenario_.botIndex;
  }

private:
  void clear() {
    scenario_ = {};
    status_ = ActionScheduleStatus::Idle;
    pending_ = false;
    pendingFrame_ = {};
    actionIndex_ = 0;
    actionMs_ = 0;
    totalMs_ = 0;
    frames_ = 0;
    startedAtMs_ = 0;
    actionStartedAtMs_ = 0;
    lastSeenMs_ = 0;
    pendingAtMs_ = 0;
    nextTicket_ = 0;
  }

  void fail(ActionScheduleStatus status) {
    pending_ = false;
    status_ = status;
  }

  bool validateClock(std::int64_t nowMs) {
    if (nowMs < lastSeenMs_) {
      fail(ActionScheduleStatus::InvalidClock);
      return false;
    }

    lastSeenMs_ = nowMs;
    return true;
  }

  Scenario scenario_ {};
  ActionScheduleStatus status_ { ActionScheduleStatus::Idle };
  ScheduledActionFrame pendingFrame_ {};
  bool pending_ {};
  std::size_t actionIndex_ {};
  int actionMs_ {};
  int totalMs_ {};
  int frames_ {};
  std::int64_t startedAtMs_ {};
  std::int64_t actionStartedAtMs_ {};
  std::int64_t lastSeenMs_ {};
  std::int64_t pendingAtMs_ {};
  std::uint64_t nextTicket_ {};
};

} // namespace aipb::explorer::scenario
