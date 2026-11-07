//
// AiPB - explicit one-shot mock action trial on live GoldSrc.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

#include <ai/ai_mock_trial.h>
#include <ai/ai_model_io_trial.h>
#include <ai/ai_exploration_probe.h>
#include <ai/ai_server_state_adapter.h>
#include <ai/ai_world_query_adapter.h>
#include <ai/ai_yapb_action_execution_context.h>
#include <ai/ai_bot_adapter.h>
#include <ai/ai_observation_builder.h>
#include <ai/ai_mock_motion.h>
#include <ai/ai_mock_telemetry.h>
#include <ai/ai_execution_ownership_adapter.h>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cstdint>

namespace ai::mock {
namespace {

constexpr float kTrialDistance = 96.0f;
constexpr float kTrialTimeout = 5.0f;
int selectedBot = -1;
bool pending {};
bool executing {};
float started {};
float lastFrameTime {};
ai::Vec3 origin {};
ai::Vec3 target {};
uint64_t expectedSession {};
uint64_t nextRequestId {};
ai::mock::MockTrace g_trace {};

float displacement(const ai::Vec3 &a, const ai::Vec3 &b) {
  const float x = b.x - a.x;
  const float y = b.y - a.y;
  const float z = b.z - a.z;
  return std::sqrt(x * x + y * y + z * z);
}

const char *resultName(ai::ActionResultType status) {
  switch (status) {
  case ai::ActionResultType::None: return "None";
  case ai::ActionResultType::Accepted: return "Accepted";
  case ai::ActionResultType::Completed: return "Completed";
  case ai::ActionResultType::Rejected: return "Rejected";
  case ai::ActionResultType::Invalid: return "Invalid";
  case ai::ActionResultType::Failed: return "Failed";
  case ai::ActionResultType::Interrupted: return "Interrupted";
  }
  return "Unknown";
}

void clearSelection() {
  selectedBot = -1;
  pending = false;
  executing = false;
  expectedSession = 0;
}

void abortTrial(Bot *bot, const char *reason) {
  if (bot != nullptr && executing) {
    ai::YaPBActionExecutionContext::cancelMockRuntimeMove(*bot);
  }
  if (executing) {
    g_trace.cancel();
  }
  game.print("AiPB mock: cancelled (%s).", reason);
  clearSelection();
}

} // namespace

bool active() {
  return selectedBot >= 0;
}

uint64_t nextWorldQueryRequestId() {
  return ++nextRequestId;
}

bool handleServer() {
  if (engfuncs.pfnCmd_Argc() < 2 || std::strcmp(engfuncs.pfnCmd_Argv(1), "mock") != 0) {
    return false;
  }

  const int argc = engfuncs.pfnCmd_Argc();
  const char *operation = argc >= 3 ? engfuncs.pfnCmd_Argv(2) : "";
  if (argc >= 4 && std::strcmp(operation, "trace") == 0) {
    const char *traceCommand = engfuncs.pfnCmd_Argv(3);
    if (argc == 5 && std::strcmp(traceCommand, "interval") == 0) {
      const char *value = engfuncs.pfnCmd_Argv(4);
      char *end {};
      const long milliseconds = std::strtol(value, &end, 10);
      if (value == end || *end != '\0' || milliseconds < 20 || milliseconds > 50 ||
          pending || executing ||
          !g_trace.setSampleIntervalSeconds(static_cast<float>(milliseconds) * 0.001f)) {
        game.print("AiPB mock trace: invalid interval or active trial (allowed: 20-50 ms).");
      }
      else {
        game.print("AiPB mock trace: interval=%d ms.", static_cast<int>(milliseconds));
      }
      return true;
    }
    if (argc == 4 && (std::strcmp(traceCommand, "status") == 0 ||
                      std::strcmp(traceCommand, "replay") == 0)) {
      const auto report = g_trace.replay();
      game.print("AiPB mock trace: %s; result=%s; frames=%d; observed_frames=%llu; saved_frames=%d; skipped_frames=%d; capacity_skipped=%d; interval_ms=%.0f; decision_frame=%llu; terminal_frame=%llu; elapsed=%.3f; moved=%.2f; remaining=%.2f.",
                 replayStatusName(report.status), resultName(report.terminalResult), static_cast<int>(report.frames),
                 static_cast<unsigned long long>(report.observedFrames),
                 static_cast<int>(report.frames), static_cast<int>(report.skippedFrames),
                 static_cast<int>(report.capacitySkipped), report.sampleIntervalSeconds * 1000.0f,
                 static_cast<unsigned long long>(report.decisionFrameId),
                 static_cast<unsigned long long>(report.terminalFrameId),
                 report.elapsedSeconds, report.displacement, report.remainingDistance);
      if (std::strcmp(traceCommand, "replay") == 0) {
        game.print("AiPB mock trace: replay is read-only; no GoldSrc actions executed.");
      }
      return true;
    }
    if ((argc == 4 || argc == 5) && std::strcmp(traceCommand, "dump") == 0) {
      long first = 0;
      if (argc == 5) {
        const char *argument = engfuncs.pfnCmd_Argv(4);
        char *end {};
        first = std::strtol(argument, &end, 10);
        if (argument == end || *end != '\0' || first < 0 ||
            static_cast<size_t>(first) > g_trace.size()) {
          game.print("AiPB mock trace: invalid starting record.");
          return true;
        }
      }
      const auto goal = g_trace.target();
      game.print("AiPB mock trace: rows=%d; start=%d; target=(%.2f,%.2f,%.2f); preflight=%d.",
                 static_cast<int>(g_trace.size()), static_cast<int>(first),
                 goal.x, goal.y, goal.z, static_cast<int>(g_trace.preflight()));
      const size_t end = cr::min(g_trace.size(), static_cast<size_t>(first) + 16u);
      for (size_t i = static_cast<size_t>(first); i < end; ++i) {
        const auto *record = g_trace.record(i);
        if (record == nullptr) {
          break;
        }
        const auto &stamp = record->observed.stamp;
        const auto &self = record->observed.self;
        game.print("AiPB mock trace[%d]: session=%llu frame=%llu time=%.3f pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f) action=%d result=%s feedback=%d decision_frame=%llu.",
                   static_cast<int>(i), static_cast<unsigned long long>(stamp.sessionId),
                   static_cast<unsigned long long>(stamp.frameId), stamp.gameTime,
                   self.origin.x, self.origin.y, self.origin.z,
                   self.velocity.x, self.velocity.y, self.velocity.z,
                   static_cast<int>(record->result.action), resultName(record->result.type),
                   record->hasFeedback ? 1 : 0,
                   static_cast<unsigned long long>(record->feedback.decisionFrameId));
      }
      return true;
    }
    game.print("Usage: aipb mock trace status | replay | dump [start] | interval <20-50 ms>");
    return true;
  }
  if (argc == 3 && std::strcmp(operation, "status") == 0) {
    game.print("AiPB mock: %s; selected_bot=%d.", pending ? "Pending" :
       executing ? "Executing" : "Idle", selectedBot);
    return true;
  }
  if (argc == 3 && std::strcmp(operation, "stop") == 0) {
    auto *bot = selectedBot >= 0 ? bots.findBotByIndex(selectedBot) : nullptr;
    abortTrial(bot, "server request");
    return true;
  }
  if (argc != 4 || std::strcmp(operation, "run") != 0) {
    game.print("Usage: aipb mock run <bot-index> | status | stop | trace status/replay/dump/interval");
    return true;
  }
  const char *argument = engfuncs.pfnCmd_Argv(3);
  char *end {};
  const long index = std::strtol(argument, &end, 10);
  if (argument == end || *end != '\0' || index < 0 || index >= ai::runtime_io::kServerBotSlots ||
      selectedBot >= 0 || ai::model_io_trial::active() || ai::explore::active() || gameState.isRoundOver()) {
    game.print("AiPB mock: rejected (InvalidBotOrBusy).");
    return true;
  }
  auto *bot = bots.findBotByIndex(static_cast<int>(index));
  if (bot == nullptr || bot->pev == nullptr || !game.isAliveEntity(bot->ent()) ||
      !ai::mockTrialModeAllowed(ai::YaPBActionExecutionContext::runtimeMode(*bot))) {
    game.print("AiPB mock: rejected (BotUnavailableOrBusy).");
    return true;
  }
  selectedBot = static_cast<int>(index);
  pending = true;
  game.print("AiPB mock: requested for bot #%d.", selectedBot);
  return true;
}

bool frame(Bot &bot) {
  if (selectedBot != bot.index()) {
    return false;
  }
  ai::runtime_io::ServerBotSnapshot state {};
  if (!ai::runtime_io::readServerBotSnapshot(selectedBot, state) ||
      !state.self.alive || !std::isfinite(state.self.maxSpeed) || state.self.maxSpeed < 10.0f ||
      ai::explore::active() || gameState.isRoundOver() ||
      !ai::mockTrialModeAllowed(ai::YaPBActionExecutionContext::runtimeMode(bot)) ||
      (executing && state.stamp.sessionId != expectedSession)) {
    abortTrial(&bot, "stale bot, round or competing owner");
    return true;
  }

  if (pending) {
    if (!state.self.onGround) {
      abortTrial(&bot, "bot not grounded");
      return true;
    }
    const float yaw = state.self.viewAngles.y * 0.017453292519943295f;
    ai::runtime_io::WorldQueryRequest request {};
    request.requestId = nextWorldQueryRequestId();
    request.sessionId = state.stamp.sessionId;
    request.observationFrameId = state.stamp.frameId;
    request.kind = ai::runtime_io::QueryKind::TraceHull;
    request.hull = state.self.crouching ? ai::runtime_io::kWorldQueryCrouchingHull :
                                          ai::runtime_io::kWorldQueryStandingHull;
    request.start = state.self.origin;
    request.end = { request.start.x + std::cos(yaw) * kTrialDistance,
                    request.start.y + std::sin(yaw) * kTrialDistance,
                    request.start.z };
    const auto reply = ai::runtime_io::queryBotWorld(selectedBot, request);
    if (!ai::runtime_io::isCompleted(request, reply) || reply.startSolid ||
        reply.fraction < 0.999f ||
        !ai::YaPBActionExecutionContext::armMockRuntimeMove(bot, request.end)) {
      game.print("AiPB mock: rejected (UnsafeTraceOrAction), query_status=%d.",
                 static_cast<int>(reply.status));
      clearSelection();
      return true;
    }
    if (!g_trace.begin(state, request.end, reply.status)) {
      ai::YaPBActionExecutionContext::cancelMockRuntimeMove(bot);
      game.print("AiPB mock: rejected (TelemetryInitialization).");
      clearSelection();
      return true;
    }
    origin = state.self.origin;
    target = request.end;
    expectedSession = state.stamp.sessionId;
    started = state.stamp.gameTime;
    lastFrameTime = started;
    pending = false;
    executing = true;
    game.print("AiPB mock: bot #%d armed MoveToPosition; from=(%.1f,%.1f,%.1f) to=(%.1f,%.1f,%.1f).",
               selectedBot, origin.x, origin.y, origin.z, request.end.x, request.end.y, request.end.z);
    return true;
  }

  // Execute the same Neural ActionRuntime policy/executor without Bot::logic.
  const auto observation = ai::buildObservation(ai::buildObservationInput(bot));
  const auto runtimeResult = ai::YaPBActionExecutionContext::stepMockRuntime(bot, observation);
  ai::runtime_io::ControlFrame ownership {};
  const bool hasOwnership = ai::runtime_io::readControlFrame(selectedBot, ownership);
  const auto *feedback = hasOwnership && ownership.hasFeedback &&
                         ownership.feedback.sessionId == state.stamp.sessionId &&
                         ownership.feedback.observedFrameId == state.stamp.frameId
      ? &ownership.feedback : nullptr;
  if (!g_trace.append(state, runtimeResult, feedback)) {
    abortTrial(&bot, "telemetry discontinuity or capacity exceeded");
    return true;
  }
  const auto outcome = ai::YaPBActionExecutionContext::mockRuntimeResult(bot);
  if (outcome.type != ai::ActionResultType::None && outcome.isTerminal()) {
    const float moved = displacement(origin, state.self.origin);
    const float remaining = ai::mockDistanceToTarget(state.self.origin, target);
    game.print("AiPB mock: result=%s; physical_displacement=%.2f; target_distance_remaining=%.2f; action_elapsed=%.3f; session=%llu; frame=%llu.",
               resultName(outcome.type), moved, remaining, state.stamp.gameTime - started,
               static_cast<unsigned long long>(state.stamp.sessionId),
               static_cast<unsigned long long>(state.stamp.frameId));
    clearSelection();
    return true;
  }
  if (state.stamp.gameTime - started > kTrialTimeout) {
    abortTrial(&bot, "action timeout");
    return true;
  }

  // No tactical Legacy logic runs while the selected Mock policy owns this bot.
  // PlayerMove's buttons are scoped to this frame, just like Explorer scenarios.
  const float dt = state.stamp.gameTime - lastFrameTime;
  lastFrameTime = state.stamp.gameTime;
  if (dt <= 0.0f || !std::isfinite(dt) || !state.self.onGround ||
      runtimeResult.type == ai::ActionResultType::Invalid ||
      runtimeResult.type == ai::ActionResultType::Rejected) {
    abortTrial(&bot, "invalid movement state");
    return true;
  }
  const float yaw = std::atan2(target.y - state.self.origin.y, target.x - state.self.origin.x);
  auto angles = bot.pev->v_angle;
  angles.y = yaw * 57.29577951308232f;
  angles.x = 0.0f;
  bot.pev->v_angle = angles;
  bot.pev->angles.y = angles.y;
  bot.pev->button = 0;
  bot.pev->impulse = 0;
  const float speed = cr::min(state.self.maxSpeed, 180.0f);
  const float boundedDt = cr::min(dt, 0.05f);
  const auto milliseconds = static_cast<uint8_t>(cr::max(1, static_cast<int>(boundedDt * 1000.0f)));
  engfuncs.pfnRunPlayerMove(bot.ent(), angles, speed, 0.0f, 0.0f, 0, 0, milliseconds);
  return true;
}

void cancelBot(int botIndex) {
  if (selectedBot == botIndex) {
    g_trace.cancel();
    clearSelection();
  }
}

void reset() {
  clearSelection();
  g_trace.reset();
}

} // namespace ai::mock
