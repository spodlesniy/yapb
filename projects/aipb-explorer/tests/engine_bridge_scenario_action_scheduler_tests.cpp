#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <stdexcept>
#include <string_view>

#include <aipb/explorer/engine_bridge_scenario_action_scheduler.h>
#include <aipb/explorer/engine_bridge_scenario_result.h>

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

    // A short jump action must not inherit the callback delay from admission.
    // The default 60-ms action allows only 6 ms of wall-clock headroom.
    auto jump = makeScenario(60, 1440);
    jump.timeoutMs = 2500;
    jump.actions[0].timeoutMs = 66;
    jump.actions[1].timeoutMs = 1584;
    check(scheduler.begin(jump, admitted(), 1000), "short jump start");
    first = scheduler.prepareFrame(1017, 1);
    check(first.ticket != 0 && first.milliseconds == 1, "delayed first jump frame");
    check(scheduler.commitFrame(first, 1018), "first jump frame committed");
    auto jumpPart = scheduler.prepareFrame(1030, 15);
    check(jumpPart.ticket != 0 && jumpPart.actionIndex == 0, "short action progresses");
    check(scheduler.commitFrame(jumpPart, 1031), "short action partial committed");
    jumpPart = scheduler.prepareFrame(1045, 15);
    check(scheduler.commitFrame(jumpPart, 1046), "second jump slice");
    jumpPart = scheduler.prepareFrame(1060, 15);
    check(scheduler.commitFrame(jumpPart, 1061), "third jump slice");
    jumpPart = scheduler.prepareFrame(1077, 255);
    check(jumpPart.milliseconds == 14 && jumpPart.actionIndex == 0,
      "last jump slice clamped to duration");
    check(scheduler.commitFrame(jumpPart, 1078), "short action boundary committed");
    check(scheduler.running() && scheduler.actionIndex() == 1
      && scheduler.totalMilliseconds() == 60, "short action advances without timeout");

    // The next action also starts at its first real engine frame.
    jumpPart = scheduler.prepareFrame(1095, 20);
    check(jumpPart.actionIndex == 1 && jumpPart.milliseconds == 20,
      "second action starts after callback gap");
    check(scheduler.commitFrame(jumpPart, 1096), "second action first commit");
    check(scheduler.totalMilliseconds() == 80 && scheduler.running(),
      "second action retains exact accounting");

    // An action still times out when no callback arrives before its first frame.
    check(scheduler.begin(jump, admitted(), 2000), "wait for first jump callback");
    check(scheduler.prepareFrame(2066, 10).ticket == 0, "missing first callback deadline");
    check(scheduler.status() == ActionScheduleStatus::ActionTimeout,
      "first callback remains bounded");

    // A callback gap after the preceding action is also bounded.
    check(scheduler.begin(makeScenario(10, 60), admitted(), 3000),
      "next action wait start");
    first = scheduler.prepareFrame(3000, 10);
    check(scheduler.commitFrame(first, 3000) && scheduler.actionIndex() == 1,
      "first action completes");
    check(scheduler.prepareFrame(3260, 10).ticket == 0,
      "missing callback for next action");
    check(scheduler.status() == ActionScheduleStatus::ActionTimeout,
      "next action waits only until its wall deadline");

    // After the action begins, a stalled or late engine frame still times out.
    check(scheduler.begin(jump, admitted(), 4000), "started action deadline");
    first = scheduler.prepareFrame(4015, 1);
    check(scheduler.commitFrame(first, 4016), "started action first frame");
    check(scheduler.prepareFrame(4081, 10).ticket == 0,
      "started action timeout unaffected by admission anchoring");
    check(scheduler.status() == ActionScheduleStatus::ActionTimeout,
      "started action deadline retained");

    // An applied late frame is still counted and can terminally time out.
    check(scheduler.begin(jump, admitted(), 5000), "late jump frame");
    first = scheduler.prepareFrame(5015, 10);
    check(scheduler.commitFrame(first, 5082), "late jump frame committed");
    check(scheduler.status() == ActionScheduleStatus::ActionTimeout
      && scheduler.totalMilliseconds() == 10, "late frame still fails");

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

    // Console admission and the first bot callback may observe different
    // engine clock snapshots. Only pre-execution re-anchoring is permitted.
    check(scheduler.begin(makeScenario(), admitted(), 400), "rebase start");
    check(!scheduler.rebaseBeforeFirstFrame(400), "equal clock needs no rebase");
    check(scheduler.rebaseBeforeFirstFrame(350), "first frame clock rebase");
    first = scheduler.prepareFrame(350, 10);
    check(first.ticket != 0 && first.frameIndex == 0, "rebased frame prepared");
    check(!scheduler.rebaseBeforeFirstFrame(340), "pending frame prevents rebase");
    check(scheduler.commitFrame(first, 351), "rebased first frame committed");
    check(scheduler.frames() == 1, "rebased run counted frame");
    check(!scheduler.rebaseBeforeFirstFrame(340), "cannot rebase after execution");
    check(scheduler.prepareFrame(350, 10).ticket == 0, "late clock rollback rejected");
    check(scheduler.status() == ActionScheduleStatus::InvalidClock,
      "strict clock semantics retained after first frame");

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

    // Terminal summaries never equate scheduler completion with physical success.
    ScenarioResult result {};
    result.name = "ground_forward_02";
    result.map = "de_dust2";
    result.botIndex = 1;
    result.team = 2;
    result.roundId = 4;
    result.status = ActionScheduleStatus::Completed;
    result.reason = "finished";
    result.frames = 380;
    result.commandMs = 750;
    result.startGameMs = 1000;
    result.endGameMs = 1750;
    result.endSource = ScenarioEndSource::Live;
    result.start.position[0] = 448.0;
    result.start.position[1] = 2368.0;
    result.start.position[2] = -92.0;
    result.start.onGround = true;
    result.end = result.start;
    result.end.position[0] += 40.0;
    char record[1024] {};
    check(formatScenarioResult(record, sizeof(record), result), "format summary");
    check(std::strstr(record, "aipb-scenario-result-v1,ground_forward_02,de_dust2,1,2,4,Completed,finished,380,750,1000,1750,live,") == record,
      "result provenance");
    check(std::strstr(record, ",488,2368,-92,") != nullptr, "actual ending coordinates");
    check(!formatScenarioResult(record, 16, result), "truncation rejected");
    result.status = ActionScheduleStatus::Running;
    check(!formatScenarioResult(record, sizeof(record), result), "running not terminal");
    result.status = ActionScheduleStatus::Cancelled;
    result.reason = "bot disconnected";
    result.endSource = ScenarioEndSource::LastCommitted;
    check(formatScenarioResult(record, sizeof(record), result), "disconnected last observation");
    check(std::strstr(record, ",Cancelled,bot disconnected,") != nullptr, "cancellation reason");
    result.endSource = ScenarioEndSource::Missing;
    result.end.position[0] = std::numeric_limits<double>::quiet_NaN();
    check(formatScenarioResult(record, sizeof(record), result), "unavailable endpoint marked");
    check(std::strstr(record, ",missing,") != nullptr, "missing source");
    result.endSource = ScenarioEndSource::Live;
    check(!formatScenarioResult(record, sizeof(record), result), "invalid live endpoint rejected");
    result.end = result.start;
    result.reason = "comma,invalid";
    check(!formatScenarioResult(record, sizeof(record), result), "unsafe CSV reason rejected");

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
