#pragma once

#include <algorithm>
#include <cmath>

#include <aipb/explorer/engine_bridge_math.h>
#include <aipb/explorer/engine_bridge_scenario_action_scheduler.h>

namespace aipb::explorer::scenario {

struct ScenarioFrameMovement {
  bool valid {};
  float yaw {};
  float pitch {};
  float forwardSpeed {};
  float strafeSpeed {};
  bool jump {};
  bool duck {};
};

// A GoldSrc-independent movement plan for one already prepared engine frame.
// Angular deltas are totals per action, not deltas applied every frame.
inline ScenarioFrameMovement planScenarioFrameMovement(
  const ScheduledActionFrame &frame,
  int committedActionMs,
  float startYaw,
  float startPitch,
  float maxSpeed
) {
  if (frame.ticket == 0 || frame.milliseconds < 1
    || frame.milliseconds > ActionScheduler::kMaxFrameMs
    || !validAction(frame.action)
    || frame.action.attack || frame.action.attack2 || frame.action.use
    || frame.action.select != "none"
    || committedActionMs < 0
    || committedActionMs + frame.milliseconds > frame.action.durationMs
    || !std::isfinite(startYaw) || !std::isfinite(startPitch)
    || !std::isfinite(maxSpeed) || maxSpeed < 10.0f) {
    return {};
  }

  const float progress = static_cast<float>(
    committedActionMs + frame.milliseconds) / static_cast<float>(frame.action.durationMs);
  ScenarioFrameMovement output {};
  output.yaw = bridge::yaw(startYaw, frame.action.yawDelta * progress);
  output.pitch = std::clamp(startPitch + frame.action.pitchDelta * progress, -89.0f, 89.0f);

  AipbExplorerBridgeCommand command {};
  command.forward = frame.action.forward;
  command.strafe = frame.action.strafe;
  bridge::translateAxes(command, maxSpeed, output.forwardSpeed, output.strafeSpeed);
  output.jump = frame.action.jump;
  output.duck = frame.action.duck;
  output.valid = std::isfinite(output.yaw) && std::isfinite(output.pitch)
    && std::isfinite(output.forwardSpeed) && std::isfinite(output.strafeSpeed);
  return output;
}

} // namespace aipb::explorer::scenario
