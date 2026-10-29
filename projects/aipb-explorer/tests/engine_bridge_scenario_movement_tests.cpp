#include <cmath>
#include <cstdio>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_scenario_movement.h>

using namespace aipb::explorer::scenario;

static void check(bool condition, const char *message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

static bool near(float a, float b, float epsilon = 0.02f) {
  return std::abs(a - b) <= epsilon;
}

static ScheduledActionFrame frameFor(const Action &action, int ms) {
  ScheduledActionFrame frame {};
  frame.action = action;
  frame.milliseconds = ms;
  frame.ticket = 1;
  return frame;
}

int main() {
  try {
    Action action {};
    action.durationMs = 100;
    action.timeoutMs = 200;
    action.forward = 1.0f;
    action.strafe = 1.0f;
    action.yawDelta = 90.0f;
    action.pitchDelta = 30.0f;
    action.jump = true;
    action.duck = true;
    auto frame = frameFor(action, 20);
    auto movement = planScenarioFrameMovement(frame, 0, 170.0f, 0.0f, 250.0f);
    check(movement.valid, "first frame valid");
    check(near(movement.yaw, -172.0f), "wrapped cumulative yaw");
    check(near(movement.pitch, 6.0f), "partial pitch");
    check(near(movement.forwardSpeed, 176.7767f), "normalized forward");
    check(near(movement.strafeSpeed, 176.7767f), "normalized strafe");
    check(movement.jump && movement.duck, "simultaneous held buttons");

    frame.milliseconds = 30;
    movement = planScenarioFrameMovement(frame, 20, 170.0f, 0.0f, 250.0f);
    check(movement.valid && near(movement.yaw, -145.0f), "cumulative yaw not repeated");
    check(near(movement.pitch, 15.0f), "cumulative pitch");

    frame.milliseconds = 50;
    movement = planScenarioFrameMovement(frame, 50, 170.0f, 0.0f, 250.0f);
    check(movement.valid && near(movement.yaw, -100.0f), "final exact yaw");
    check(near(movement.pitch, 30.0f), "final exact pitch");

    action.forward = -1.0f;
    action.strafe = 0.0f;
    action.yawDelta = -180.0f;
    action.pitchDelta = -180.0f;
    action.jump = false;
    action.duck = false;
    frame = frameFor(action, 100);
    movement = planScenarioFrameMovement(frame, 0, -160.0f, -70.0f, 200.0f);
    check(movement.valid && near(movement.yaw, 20.0f), "negative yaw wrap");
    check(near(movement.pitch, -89.0f), "pitch clamp");
    check(near(movement.forwardSpeed, -200.0f)
      && near(movement.strafeSpeed, 0.0f), "backward speed");
    check(!movement.jump && !movement.duck, "released buttons");

    action.pitchDelta = 180.0f;
    frame = frameFor(action, 100);
    movement = planScenarioFrameMovement(frame, 0, 0.0f, 70.0f, 250.0f);
    check(movement.valid && near(movement.pitch, 89.0f), "upper pitch clamp");

    action.attack = true;
    frame = frameFor(action, 100);
    check(!planScenarioFrameMovement(frame, 0, 0.0f, 0.0f, 250.0f).valid,
      "unsupported combat rejects");
    action.attack = false;
    action.select = "knife";
    frame = frameFor(action, 100);
    check(!planScenarioFrameMovement(frame, 0, 0.0f, 0.0f, 250.0f).valid,
      "unsupported selection rejects");
    action.select = "none";
    frame = frameFor(action, 100);

    check(!planScenarioFrameMovement(frame, -1, 0.0f, 0.0f, 250.0f).valid,
      "negative committed duration");
    check(!planScenarioFrameMovement(frame, 1, 0.0f, 0.0f, 250.0f).valid,
      "overshoot rejected");
    check(!planScenarioFrameMovement(frame, 0, 0.0f, 0.0f, 0.0f).valid,
      "invalid speed rejected");
    check(!planScenarioFrameMovement(frame, 0, NAN, 0.0f, 250.0f).valid,
      "invalid view rejected");
    frame.ticket = 0;
    check(!planScenarioFrameMovement(frame, 0, 0.0f, 0.0f, 250.0f).valid,
      "missing scheduler ticket");
    frame.ticket = 1;
    frame.milliseconds = 0;
    check(!planScenarioFrameMovement(frame, 0, 0.0f, 0.0f, 250.0f).valid,
      "empty frame rejected");
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "scenario movement: %s\n", e.what());
    return 1;
  }
}
