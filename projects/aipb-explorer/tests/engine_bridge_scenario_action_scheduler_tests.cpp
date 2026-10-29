#include <cstdio>
#include <stdexcept>
#include <string_view>

#include <aipb/explorer/engine_bridge_scenario_action_scheduler.h>

using namespace aipb::explorer::scenario;

static void check(bool ok, const char *description) {
  if (!ok) {
    throw std::runtime_error(description);
  }
}

static Scenario makeScenario(int firstMs = 120, int secondMs = 80) {
  Scenario scenario {};
  scenario.name = "scheduler_test";
  scenario.map = "de_dust2";
  scenario.botIndex = 1;
  scenario.team = 2;
  scenario.timeoutMs = 2000;
  scenario.actions.push_back({});
  scenario.actions.back().durationMs = firstMs;
  scenario.actions.back().timeoutMs = firstMs + 200;
  scenario.actions.back().forward = 1.0f;
  scenario.actions.push_back({});
  scenario.actions.back().durationMs = secondMs;
  scenario.actions.back().timeoutMs = secondMs + 200;
  scenario.actions.back().strafe = 1.0f;
  return scenario;
}

static RunStartDecision admitted() {
  RunStartDecision result {};
  result.status = RunStartStatus::Ready;
  return result;
}

int main() {
  try {
    ActionScheduler scheduler {};
    check(scheduler.status() == ActionScheduleStatus::Idle, "initially idle");
    check(scheduler.prepareFrame(0, 10).milliseconds == 0, "idle cannot prepare");
    check(!scheduler.cancel(), "idle cancel");
    check(!scheduler.begin(makeScenario(), {}, 0), "no admission");
    check(scheduler.status() == ActionScheduleStatus::InvalidScenario, "admission rejection");
    check(!scheduler.begin(makeScenario(), admitted(), -1), "invalid clock");
    check(scheduler.begin(makeScenario(), admitted(), 1000), "start after rejection");
    check(scheduler.botIndex() == 1 && scheduler.running(), "owns bot and scenario");
    check(!scheduler.begin(makeScenario(), admitted(), 1000), "no concurrent start");

    check(scheduler.prepareFrame(1000, 0).milliseconds == 0, "zero frame rejected");
    check(scheduler.prepareFrame(1000, 256).milliseconds == 0, "large frame rejected");
    check(scheduler.frames() == 0, "invalid frame leaves state");
    auto first = scheduler.prepareFrame(1000, 70);
    check(first.milliseconds == 70 && first.actionIndex == 0, "first frame");
    check(first.action.forward == 1.0f, "first control");
    check(scheduler.pending(), "frame pending");
    check(scheduler.prepareFrame(1000, 5).milliseconds == 0, "no double dispatch");
    auto wrong = first;
    wrong.ticket++;
    check(!scheduler.commitFrame(wrong, 1010), "ticket mismatch");
    check(scheduler.pending() && scheduler.frames() == 0, "wrong ticket has no effect");
    wrong = first;
    wrong.milliseconds++;
    check(!scheduler.commitFrame(wrong, 1010), "msec mismatch");
    check(scheduler.commitFrame(first, 1010), "first commit");
    check(!scheduler.commitFrame(first, 1010), "no duplicate commit");
    check(scheduler.frames() == 1 && scheduler.totalMilliseconds() == 70, "one frame counted");
    check(scheduler.actionMilliseconds() == 70, "partial action");

    auto finishFirst = scheduler.prepareFrame(1010, 100);
    check(finishFirst.milliseconds == 50, "clamp to remaining action");
    check(scheduler.commitFrame(finishFirst, 1011), "first action end");
    check(scheduler.actionIndex() == 1 && scheduler.actionMilliseconds() == 0,
      "advance action without idle");
    auto second = scheduler.prepareFrame(1011, 70);
    check(second.actionIndex == 1 && second.action.strafe == 1.0f,
      "next frame uses second action");
    check(scheduler.commitFrame(second, 1012), "second action partial");
    auto last = scheduler.prepareFrame(1012, 100);
    check(last.milliseconds == 10, "second action remainder");
    check(scheduler.commitFrame(last, 1013), "last commit");
    check(scheduler.status() == ActionScheduleStatus::Completed, "completed");
    check(scheduler.frames() == 4 && scheduler.totalMilliseconds() == 200,
      "exact commanded duration");
    check(scheduler.actionIndex() == 2, "no more actions");
    check(scheduler.prepareFrame(1013, 5).milliseconds == 0, "completed cannot dispatch");
    check(!scheduler.cancel(), "completed cannot cancel");

    // A failed adapter frame is not consumed; cancel discards its outstanding token.
    check(scheduler.begin(makeScenario(), admitted(), 2000), "restart completed");
    auto pending = scheduler.prepareFrame(2000, 40);
    check(pending.milliseconds == 40, "new frame");
    check(scheduler.cancel(), "cancel");
    check(scheduler.status() == ActionScheduleStatus::Cancelled, "cancelled status");
    check(!scheduler.pending() && !scheduler.commitFrame(pending, 2001),
      "cancelled frame never commits");
    check(scheduler.frames() == 0, "no cancelled progress");

    // Remaining scenario budget bounds a proposed frame.
    auto budget = makeScenario(100, 100);
    budget.timeoutMs = 200;
    budget.actions[0].timeoutMs = 100;
    budget.actions[1].timeoutMs = 100;
    check(scheduler.begin(budget, admitted(), 0), "exact global budget start");
    first = scheduler.prepareFrame(0, 255);
    check(first.milliseconds == 100, "first action cap");
    check(scheduler.commitFrame(first, 100), "action exact deadline");
    second = scheduler.prepareFrame(100, 255);
    check(second.milliseconds == 100, "second action cap");
    check(scheduler.commitFrame(second, 200), "global exact deadline");
    check(scheduler.status() == ActionScheduleStatus::Completed, "exact deadline completes");

    // Timeout is measured in wall-clock ms even without any simulated progress.
    check(scheduler.begin(makeScenario(), admitted(), 0), "wall timeout start");
    check(scheduler.prepareFrame(321, 10).milliseconds == 0, "first action timed out");
    check(scheduler.status() == ActionScheduleStatus::ActionTimeout, "action timeout reason");
    check(scheduler.frames() == 0, "timeout before frame");

    auto global = makeScenario();
    global.timeoutMs = 650;
    check(scheduler.begin(global, admitted(), 0), "global timeout start");
    check(scheduler.prepareFrame(650, 10).milliseconds == 0, "global time expired");
    check(scheduler.status() == ActionScheduleStatus::ScenarioTimeout, "global timeout reason");

    check(scheduler.begin(makeScenario(), admitted(), 200), "clock regression start");
    check(scheduler.prepareFrame(199, 10).milliseconds == 0, "clock backward");
    check(scheduler.status() == ActionScheduleStatus::InvalidClock, "clock regression reason");

    check(scheduler.begin(makeScenario(), admitted(), 0), "pending clock start");
    first = scheduler.prepareFrame(10, 10);
    check(!scheduler.commitFrame(first, 9), "commit clock regression");
    check(scheduler.status() == ActionScheduleStatus::InvalidClock, "pending clock reason");

    // An applied frame exceeding a wall-clock deadline remains recorded, then fails.
    check(scheduler.begin(makeScenario(), admitted(), 0), "late frame start");
    first = scheduler.prepareFrame(0, 10);
    check(scheduler.commitFrame(first, 321), "late applied frame accounted");
    check(scheduler.status() == ActionScheduleStatus::ActionTimeout
      && scheduler.totalMilliseconds() == 10, "late frame abort");

    // The frame limit is independent of the simulated time budget.
    auto many = makeScenario(5000, 1);
    many.timeoutMs = 6000;
    many.actions[0].timeoutMs = 5000;
    many.actions[1].timeoutMs = 1;
    check(scheduler.begin(many, admitted(), 0), "many frames start");
    for (int i = 0; i < ActionScheduler::kMaxFrames; ++i) {
      const auto frame = scheduler.prepareFrame(0, 1);
      check(frame.milliseconds == 1, "frame before cap");
      check(scheduler.commitFrame(frame, 0), "frame before cap commit");
    }
    check(scheduler.frames() == ActionScheduler::kMaxFrames, "frame counter");
    check(scheduler.prepareFrame(0, 1).milliseconds == 0, "maximum frame reached");
    check(scheduler.status() == ActionScheduleStatus::FrameLimit, "frame limit reason");

    // Malformed or unsupported plans never schedule actions.
    auto invalid = makeScenario();
    invalid.actions[0].durationMs = 0;
    check(!scheduler.begin(invalid, admitted(), 0), "invalid scenario");
    invalid = makeScenario();
    invalid.actions[1].attack = true;
    check(!scheduler.begin(invalid, admitted(), 0), "unsupported action");
    check(scheduler.status() == ActionScheduleStatus::InvalidScenario, "invalid plan status");
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "scenario scheduler: %s\n", e.what());
    return 1;
  }
}
