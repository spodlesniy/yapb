//
// AiPB - opt-in stamped model decision to physical movement integration trial.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

#include <ai/ai_model_io_trial.h>
#include <ai/ai_mock_trial.h>
#include <ai/ai_model_io_test_backend.h>
#include <ai/ai_exploration_probe.h>
#include <ai/ai_server_state_adapter.h>
#include <ai/ai_world_query_adapter.h>
#include <ai/ai_yapb_action_execution_context.h>
#include <ai/ai_bot_adapter.h>
#include <ai/ai_observation_builder.h>
#include <ai/ai_mock_motion.h>
#include <ai/ai_model_io_recovery.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

extern ConVar cv_ai_mode;

namespace ai::model_io_trial {
namespace {

constexpr float kTravel = 96.0f;
constexpr float kTimeout = 5.0f;
int selected = -1;
bool pending {};
bool running {};
bool awaitingFeedback {};
float started {};
float lastTime {};
ai::Vec3 origin {};
ai::Vec3 destination {};
uint64_t sessionId {};
ai::runtime_io::ModelIOTestBackend backend {};

bool lastCompleted {};
float lastMoved {};
float lastRemaining {};
float lastElapsed {};
uint32_t lastRequests {};
uint64_t lastDecisionFrame {};
uint64_t lastFeedbackFrame {};
ai::runtime_io::ModelIOTrialStopReason lastStopReason {
    ai::runtime_io::ModelIOTrialStopReason::None
};
ai::runtime_io::ModelIORecoveryReport lastRecovery {};

const char *name(ai::ActionResultType type) {
  switch (type) {
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

void clear(Bot *bot, ai::runtime_io::ModelIOTrialStopReason reason) {
  ai::runtime_io::ModelIORecoveryReport recovery {};
  const bool hadRuntime = running;
  if (bot != nullptr && running) {
    recovery = ai::YaPBActionExecutionContext::stopModelIOTrial(*bot);
  }
  else if (!running) {
    recovery = ai::runtime_io::verifyModelIORecovery(nullptr, nullptr,
        false, false, false, false, false);
  }
  // Disconnect may have already destroyed its bot. Never keep a backend
  // pointer owned by a deleted BotRuntime; it is reinitialized on next run.
  if (hadRuntime && !recovery.verified() &&
      reason != ai::runtime_io::ModelIOTrialStopReason::BotDisconnected &&
      reason != ai::runtime_io::ModelIOTrialStopReason::MapReset) {
    game.print("AiPB modelio: recovery FAILED; provider=%d; fallback=%d; backend=%d; idle=%d; owner=%d.",
        recovery.providerRestored, recovery.fallbackRestored, recovery.backendReleased,
        recovery.actionIdle, recovery.trialReleased);
    reason = ai::runtime_io::ModelIOTrialStopReason::RecoveryFailed;
  }
  lastRecovery = recovery;
  lastStopReason = reason;
  selected = -1;
  pending = false;
  running = false;
  awaitingFeedback = false;
  sessionId = 0;
  backend.reset();
}

void fail(Bot *bot, ai::runtime_io::ModelIOTrialStopReason reason, const char *detail) {
  const uint32_t count = backend.requests();
  const auto decision = backend.decisionFrame();
  const auto feedback = backend.feedbackFrame();
  lastCompleted = false;
  clear(bot, reason);
  game.print("AiPB modelio: stopped (%s, %s); requests=%u; decision_frame=%llu; feedback_frame=%llu; recovered=%d.",
      ai::runtime_io::modelIOTrialStopReasonName(lastStopReason), detail, count,
      static_cast<unsigned long long>(decision),
      static_cast<unsigned long long>(feedback), lastRecovery.verified());
}

} // namespace

bool active() {
  return selected >= 0;
}

bool handleServer() {
  if (engfuncs.pfnCmd_Argc() < 2 || std::strcmp(engfuncs.pfnCmd_Argv(1), "modelio") != 0) {
    return false;
  }
  const int argc = engfuncs.pfnCmd_Argc();
  const char *cmd = argc >= 3 ? engfuncs.pfnCmd_Argv(2) : "";
  if (argc == 3 && std::strcmp(cmd, "status") == 0) {
    game.print("AiPB modelio: %s; selected_bot=%d; last=%s; stop_reason=%s; recovered=%d; provider=%d; fallback=%d; backend=%d; idle=%d; owner=%d; requests=%u; decision_frame=%llu; feedback_frame=%llu; moved=%.2f; remaining=%.2f; elapsed=%.3f.",
        pending ? "Pending" : running ? (awaitingFeedback ? "Feedback" : "Executing") : "Idle",
        selected, lastCompleted ? "PASS" : "None/Failed",
        ai::runtime_io::modelIOTrialStopReasonName(lastStopReason),
        lastRecovery.verified(), lastRecovery.providerRestored,
        lastRecovery.fallbackRestored, lastRecovery.backendReleased,
        lastRecovery.actionIdle, lastRecovery.trialReleased,
        lastRequests, static_cast<unsigned long long>(lastDecisionFrame),
        static_cast<unsigned long long>(lastFeedbackFrame),
        lastMoved, lastRemaining, lastElapsed);
    return true;
  }
  if (argc == 3 && std::strcmp(cmd, "stop") == 0) {
    auto *bot = selected >= 0 ? bots.findBotByIndex(selected) : nullptr;
    if (active()) {
      clear(bot, ai::runtime_io::ModelIOTrialStopReason::Requested);
      lastCompleted = false;
    }
    game.print("AiPB modelio: stopped; reason=%s; recovered=%d.",
        ai::runtime_io::modelIOTrialStopReasonName(lastStopReason),
        lastRecovery.verified());
    return true;
  }
  if (argc != 4 || std::strcmp(cmd, "run") != 0) {
    game.print("Usage: aipb modelio run <bot-index> | status | stop");
    return true;
  }
  const char *arg = engfuncs.pfnCmd_Argv(3);
  char *end {};
  const long index = std::strtol(arg, &end, 10);
  if (arg == end || *end != '\0' || index < 0 ||
      index >= ai::runtime_io::kServerBotSlots || active() ||
      ai::mock::active() || ai::explore::active() || gameState.isRoundOver()) {
    game.print("AiPB modelio: rejected (InvalidBotOrBusy).");
    return true;
  }
  auto *bot = bots.findBotByIndex(static_cast<int>(index));
  if (bot == nullptr || bot->pev == nullptr || !game.isAliveEntity(bot->ent()) ||
      ai::YaPBActionExecutionContext::runtimeMode(*bot) != ai::ControlMode::Neural) {
    game.print("AiPB modelio: rejected (BotUnavailableOrMode).");
    return true;
  }
  selected = static_cast<int>(index);
  pending = true;
  lastCompleted = false;
  lastMoved = lastRemaining = lastElapsed = 0.0f;
  lastRequests = 0;
  lastDecisionFrame = lastFeedbackFrame = 0;
  lastStopReason = ai::runtime_io::ModelIOTrialStopReason::None;
  lastRecovery = {};
  game.print("AiPB modelio: requested for bot #%d.", selected);
  return true;
}

bool frame(Bot &bot) {
  if (selected != bot.index()) {
    return false;
  }

  ai::runtime_io::ServerBotSnapshot state {};
  const bool stateAvailable = ai::runtime_io::readServerBotSnapshot(selected, state);
  const bool botAvailable = stateAvailable && state.self.alive &&
      std::isfinite(state.self.maxSpeed) && state.self.maxSpeed >= 10.0f;
  const auto invalid = ai::runtime_io::modelIOTrialInvalidation(
      botAvailable, gameState.isRoundOver(), ai::explore::active() || ai::mock::active(),
      cv_ai_mode.as<int>() == 1,
      ai::YaPBActionExecutionContext::runtimeMode(bot) == ai::ControlMode::Neural,
      !running || (stateAvailable && state.stamp.sessionId == sessionId));
  if (invalid != ai::runtime_io::ModelIOTrialStopReason::None) {
    fail(&bot, invalid, "lifecycle gate");
    return true;
  }

  if (pending) {
    if (!state.self.onGround) {
      fail(&bot, ai::runtime_io::ModelIOTrialStopReason::BotUnavailable, "not grounded");
      return true;
    }
    ai::runtime_io::WorldQueryRequest request {};
    request.requestId = ai::mock::nextWorldQueryRequestId();
    request.sessionId = state.stamp.sessionId;
    request.observationFrameId = state.stamp.frameId;
    request.kind = ai::runtime_io::QueryKind::TraceHull;
    request.hull = state.self.crouching ? ai::runtime_io::kWorldQueryCrouchingHull :
        ai::runtime_io::kWorldQueryStandingHull;
    const float yaw = state.self.viewAngles.y * 0.017453292519943295f;
    request.start = state.self.origin;
    request.end = { request.start.x + std::cos(yaw) * kTravel,
                    request.start.y + std::sin(yaw) * kTravel,
                    request.start.z };
    const auto result = ai::runtime_io::queryBotWorld(selected, request);
    if (!ai::runtime_io::isCompleted(request, result) || result.startSolid ||
        result.fraction < 0.999f || (backend.reset(), !backend.arm(request.end)) ||
        !ai::YaPBActionExecutionContext::armModelIOTrial(bot, &backend, request.end)) {
      fail(&bot, ai::runtime_io::ModelIOTrialStopReason::InvalidAction, "unsafe trace or unable to arm backend");
      return true;
    }

    destination = request.end;
    origin = state.self.origin;
    sessionId = state.stamp.sessionId;
    started = lastTime = state.stamp.gameTime;
    pending = false;
    running = true;
    game.print("AiPB modelio: armed; bot=%d; from=(%.1f,%.1f,%.1f); to=(%.1f,%.1f,%.1f).",
        selected, origin.x, origin.y, origin.z,
        destination.x, destination.y, destination.z);
    return true;
  }

  if (state.stamp.gameTime - started > kTimeout) {
    fail(&bot, ai::runtime_io::ModelIOTrialStopReason::Timeout, "timeout");
    return true;
  }
  const auto observation = ai::buildObservation(ai::buildObservationInput(bot));
  const auto result = ai::YaPBActionExecutionContext::stepModelIOTrial(bot, observation);

  if (awaitingFeedback) {
    if (!backend.terminalFeedbackObserved() ||
        backend.feedbackResult() != ai::ActionResultType::Completed ||
        backend.sessionId() != sessionId ||
        backend.feedbackFrame() <= backend.decisionFrame()) {
      fail(&bot, ai::runtime_io::ModelIOTrialStopReason::InvalidAction, "missing or invalid terminal feedback");
      return true;
    }
    lastRequests = backend.requests();
    lastDecisionFrame = backend.decisionFrame();
    lastFeedbackFrame = backend.feedbackFrame();
    lastElapsed = state.stamp.gameTime - started;
    lastMoved = ai::mockDistanceToTarget(origin, state.self.origin);
    lastRemaining = ai::mockDistanceToTarget(state.self.origin, destination);
    const auto completedSession = sessionId;
    const bool physicallyReached = ai::mockReachedTarget(state.self.origin, destination);
    clear(&bot, ai::runtime_io::ModelIOTrialStopReason::Completed);
    lastCompleted = physicallyReached && lastRecovery.verified();
    game.print("AiPB modelio: %s; result=Completed; recovered=%d; requests=%u; decision_frame=%llu; feedback_frame=%llu; moved=%.2f; remaining=%.2f; elapsed=%.3f; session=%llu.",
        lastCompleted ? "PASS" : "FAIL", lastRecovery.verified(), lastRequests,
        static_cast<unsigned long long>(lastDecisionFrame),
        static_cast<unsigned long long>(lastFeedbackFrame),
        lastMoved, lastRemaining, lastElapsed,
        static_cast<unsigned long long>(completedSession));
    game.print("AiPB modelio: recovery=%s; provider=%d; fallback=%d; backend=%d; idle=%d; owner=%d.",
        lastRecovery.verified() ? "PASS" : "FAIL",
        lastRecovery.providerRestored, lastRecovery.fallbackRestored,
        lastRecovery.backendReleased, lastRecovery.actionIdle,
        lastRecovery.trialReleased);
    return true;
  }

  if (result.isTerminal()) {
    if (result.action != ai::ActionType::MoveToPosition ||
        result.type != ai::ActionResultType::Completed ||
        !ai::mockReachedTarget(state.self.origin, destination)) {
      fail(&bot, ai::runtime_io::ModelIOTrialStopReason::InvalidAction, "terminal without physical completion");
      return true;
    }
    awaitingFeedback = true;
    return true;
  }
  if (result.type != ai::ActionResultType::Accepted || !backend.decided() ||
      result.action != ai::ActionType::MoveToPosition || !state.self.onGround) {
    fail(&bot, ai::runtime_io::ModelIOTrialStopReason::InvalidAction, "decision or movement state invalid");
    return true;
  }

  const float dt = state.stamp.gameTime - lastTime;
  lastTime = state.stamp.gameTime;
  if (!std::isfinite(dt) || dt <= 0.0f) {
    fail(&bot, ai::runtime_io::ModelIOTrialStopReason::InvalidAction, "non-monotonic engine time");
    return true;
  }
  const float yaw = std::atan2(destination.y - state.self.origin.y,
                               destination.x - state.self.origin.x);
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
  if (selected == botIndex) {
    auto *bot = bots.findBotByIndex(selected);
    lastCompleted = false;
    clear(bot, ai::runtime_io::ModelIOTrialStopReason::BotDisconnected);
  }
}

void reset() {
  auto *bot = selected >= 0 ? bots.findBotByIndex(selected) : nullptr;
  clear(bot, ai::runtime_io::ModelIOTrialStopReason::MapReset);
  lastCompleted = false;
}

} // namespace ai::model_io_trial
