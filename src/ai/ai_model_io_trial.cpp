//
// AiPB - opt-in stamped model decision to physical movement integration trial.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

#include <ai/ai_model_io_trial.h>
#include <ai/ai_mock_trial.h>
#include <ai/ai_model_io_test_backend.h>
#include <ai/ai_model_io_loopback.h>
#include <ai/ai_model_io_external_trial_backend.h>
#include <ai/ai_exploration_probe.h>
#include <ai/ai_server_state_adapter.h>
#include <ai/ai_world_query_adapter.h>
#include <ai/ai_yapb_action_execution_context.h>
#include <ai/ai_bot_adapter.h>
#include <ai/ai_observation_builder.h>
#include <ai/ai_mock_motion.h>
#include <ai/ai_model_io_recovery.h>
#include <ai/ai_model_io_sequence.h>
#include <ai/ai_model_io_session.h>

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
ai::runtime_io::ModelIOSteadyClock externalClock {};
ai::runtime_io::ModelIOLoopbackTcpTransport externalTransport { 0 };
ai::runtime_io::ModelIOTransportBackend transportBackend { externalTransport, externalClock };
ai::runtime_io::ModelIOExternalTrialBackend externalBackend { transportBackend };
bool externalMode {};
uint16_t externalPort {};
ai::runtime_io::ModelIOSequenceProgress sequence {};
ai::runtime_io::ModelIOSessionBudget sessionBudget {};
bool sessionMode {};
uint32_t lastSessionSeconds {};
uint64_t lastSessionElapsedMs {};
uint8_t lastSequenceTotal {};
uint8_t lastSequenceCompleted {};
uint32_t lastSequenceRequests {};

bool lastCompleted {};
float lastMoved {};
float lastRemaining {};
float lastElapsed {};
uint32_t lastRequests {};
uint64_t lastDecisionFrame {};
uint64_t lastFeedbackFrame {};
bool lastRequestHadFeedback {};
uint64_t lastRawDecisionFrame {};
uint64_t lastRawObservedFrame {};
ai::InferenceStatus lastSourceStatus { ai::InferenceStatus::NoDecision };
ai::runtime_io::ModelIOTransportStatus lastTransportStatus { ai::runtime_io::ModelIOTransportStatus::Idle };
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
  externalBackend.reset();
  externalMode = false;
  externalPort = 0;
  sessionMode = false;
}

void fail(Bot *bot, ai::runtime_io::ModelIOTrialStopReason reason, const char *detail) {
  const uint32_t count = externalMode ? externalBackend.requests() : backend.requests();
  const auto decision = externalMode ? externalBackend.decisionFrame() : backend.decisionFrame();
  const auto feedback = externalMode ? externalBackend.feedbackFrame() : backend.feedbackFrame();
  lastRequests = count;
  lastDecisionFrame = decision;
  lastFeedbackFrame = feedback;
  if (externalMode) {
    lastRequestHadFeedback = externalBackend.lastRequestHadFeedback();
    lastRawDecisionFrame = externalBackend.lastFeedbackDecisionFrame();
    lastRawObservedFrame = externalBackend.lastFeedbackObservedFrame();
    lastSourceStatus = externalBackend.lastSourceStatus();
    lastTransportStatus = transportBackend.lastStatus();
  }
  lastCompleted = false;
  if (sessionBudget.active()) {
    lastSessionElapsedMs = sessionBudget.elapsed(externalClock.milliseconds());
    sessionBudget.reset();
  }
  if (sequence.total() > 0) {
    lastSequenceTotal = sequence.total();
    lastSequenceCompleted = sequence.completed();
    lastSequenceRequests = sequence.requests();
    sequence.reset();
  }
  clear(bot, reason);
  game.print("AiPB modelio: stopped (%s, %s); requests=%u; decision_frame=%llu; feedback_frame=%llu; recovered=%d.",
      ai::runtime_io::modelIOTrialStopReasonName(lastStopReason), detail, count,
      static_cast<unsigned long long>(decision),
      static_cast<unsigned long long>(feedback), lastRecovery.verified());
  if (count != 0 && lastTransportStatus != ai::runtime_io::ModelIOTransportStatus::Idle) {
    game.print("AiPB modelio: external diagnostic; transport=%s; source_status=%d; request_has_feedback=%d; feedback_decision=%llu; feedback_observed=%llu; attempts=%llu; completed=%llu.",
        ai::runtime_io::modelIOTransportStatusName(lastTransportStatus),
        static_cast<int>(lastSourceStatus), lastRequestHadFeedback,
        static_cast<unsigned long long>(lastRawDecisionFrame),
        static_cast<unsigned long long>(lastRawObservedFrame),
        static_cast<unsigned long long>(transportBackend.attempts()),
        static_cast<unsigned long long>(transportBackend.completed()));
  }
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
    if (lastSequenceTotal > 0) {
      game.print("AiPB modelio: sequence progress=%u/%u; total_requests=%u.",
          static_cast<unsigned>(lastSequenceCompleted), static_cast<unsigned>(lastSequenceTotal),
          lastSequenceRequests);
    }
    if (lastSessionSeconds != 0) {
      game.print("AiPB modelio: session budget=%us; elapsed_ms=%llu; active=%d.",
          lastSessionSeconds,
          static_cast<unsigned long long>(sessionBudget.active()
              ? sessionBudget.elapsed(externalClock.milliseconds()) : lastSessionElapsedMs),
          sessionBudget.active());
    }
    return true;
  }
  if (argc == 3 && std::strcmp(cmd, "stop") == 0) {
    auto *bot = selected >= 0 ? bots.findBotByIndex(selected) : nullptr;
    if (active()) {
      if (sessionBudget.active()) {
        lastSessionElapsedMs = sessionBudget.elapsed(externalClock.milliseconds());
        sessionBudget.reset();
      }
      clear(bot, ai::runtime_io::ModelIOTrialStopReason::Requested);
      lastCompleted = false;
    }
    if (sequence.total() > 0) {
      lastSequenceTotal = sequence.total();
      lastSequenceCompleted = sequence.completed();
      lastSequenceRequests = sequence.requests();
      sequence.reset();
    }
    game.print("AiPB modelio: stopped; reason=%s; recovered=%d.",
        ai::runtime_io::modelIOTrialStopReasonName(lastStopReason),
        lastRecovery.verified());
    return true;
  }
  const bool useSequence = std::strcmp(cmd, "sequence") == 0;
  const bool useSession = std::strcmp(cmd, "session") == 0;
  const bool useExternal = useSession || useSequence || std::strcmp(cmd, "external") == 0;
  if ((std::strcmp(cmd, "run") != 0 || argc != 4) &&
      (!useExternal || argc != (useSession ? 7 : useSequence ? 6 : 5))) {
    game.print("Usage: aipb modelio run <bot-index> | external <bot-index> <port> | sequence <bot-index> <port> <legs:2-3> | session <bot-index> <port> <actions:4-8> <seconds:5-30> | status | stop");
    return true;
  }
  uint8_t requestedLegs {};
  uint32_t requestedSeconds {};
  if (useSequence || useSession) {
    const char *legArg = engfuncs.pfnCmd_Argv(5);
    char *legEnd {};
    const long parsed = std::strtol(legArg, &legEnd, 10);
    const long minimum = useSession ? ai::runtime_io::kModelIOSessionMinActions
                                    : ai::runtime_io::kModelIOSequenceMinLegs;
    const long maximum = useSession ? ai::runtime_io::kModelIOSessionMaxActions
                                    : ai::runtime_io::kModelIOSequenceMaxLegs;
    if (legArg == legEnd || *legEnd != '\0' || parsed < minimum || parsed > maximum) {
      game.print("AiPB modelio: rejected (%s).", useSession ? "InvalidActionCount" : "InvalidSequenceLength");
      return true;
    }
    requestedLegs = static_cast<uint8_t>(parsed);
  }
  if (useSession) {
    const char *secondsArg = engfuncs.pfnCmd_Argv(6);
    char *secondsEnd {};
    const long seconds = std::strtol(secondsArg, &secondsEnd, 10);
    if (secondsArg == secondsEnd || *secondsEnd != '\0' ||
        seconds < ai::runtime_io::kModelIOSessionMinSeconds ||
        seconds > ai::runtime_io::kModelIOSessionMaxSeconds) {
      game.print("AiPB modelio: rejected (InvalidSessionDeadline).");
      return true;
    }
    requestedSeconds = static_cast<uint32_t>(seconds);
  }
  uint16_t requestedPort {};
  if (useExternal) {
    const char *portArg = engfuncs.pfnCmd_Argv(4);
    char *portEnd {};
    const long parsed = std::strtol(portArg, &portEnd, 10);
    if (portArg == portEnd || *portEnd != '\0' || parsed <= 0 || parsed > 65535) {
      game.print("AiPB modelio: rejected (InvalidLoopbackPort).");
      return true;
    }
    requestedPort = static_cast<uint16_t>(parsed);
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
  if ((useSequence && !sequence.arm(requestedLegs)) ||
      (useSession && !sequence.armSession(requestedLegs))) {
    game.print("AiPB modelio: rejected (UnableToArmSequence).");
    return true;
  }
  if (!useSession && !useSequence) {
    sequence.reset();
  }
  sessionBudget.reset();
  if (useSession && !sessionBudget.arm(requestedSeconds, externalClock.milliseconds())) {
    sequence.reset();
    game.print("AiPB modelio: rejected (UnableToArmSessionBudget).");
    return true;
  }
  sessionMode = useSession;
  selected = static_cast<int>(index);
  externalMode = useExternal;
  externalPort = requestedPort;
  pending = true;
  lastCompleted = false;
  lastMoved = lastRemaining = lastElapsed = 0.0f;
  lastRequests = 0;
  lastDecisionFrame = lastFeedbackFrame = 0;
  lastSequenceTotal = requestedLegs;
  lastSequenceCompleted = 0;
  lastSequenceRequests = 0;
  lastSessionSeconds = requestedSeconds;
  lastSessionElapsedMs = 0;
  lastRequestHadFeedback = false;
  lastRawDecisionFrame = lastRawObservedFrame = 0;
  lastSourceStatus = ai::InferenceStatus::NoDecision;
  lastTransportStatus = ai::runtime_io::ModelIOTransportStatus::Idle;
  lastStopReason = ai::runtime_io::ModelIOTrialStopReason::None;
  lastRecovery = {};
  game.print("AiPB modelio: requested for bot #%d; backend=%s; port=%u; legs=%u; budget_seconds=%u.",
       selected, useSession ? "ExternalSession" :
       useSequence ? "ExternalSequence" : externalMode ? "ExternalLoopback" : "Deterministic",
       externalPort, requestedLegs != 0 ? requestedLegs : 1, requestedSeconds);
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
  if (sessionMode && sessionBudget.expired(externalClock.milliseconds())) {
    fail(&bot, ai::runtime_io::ModelIOTrialStopReason::Timeout, "session wall-clock deadline");
    return true;
  }

  if (pending) {
    if (sequence.total() > 0 && !sequence.bindSession(state.stamp.sessionId)) {
      fail(&bot, ai::runtime_io::ModelIOTrialStopReason::SessionChanged, "sequence session changed");
      return true;
    }
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
    backend.reset();
    externalBackend.reset();
    const ai::runtime_io::ModelIOBackend *trialBackend {};
    bool armed {};
    if (externalMode) {
      externalTransport = ai::runtime_io::ModelIOLoopbackTcpTransport(externalPort);
      transportBackend.setTimeoutMs(ai::runtime_io::kModelIOTransportMaxTimeoutMs);
      armed = externalBackend.arm(request.end);
      trialBackend = &externalBackend;
    }
    else {
      armed = backend.arm(request.end);
      trialBackend = &backend;
    }
    if (!ai::runtime_io::isCompleted(request, result) || result.startSolid ||
        result.fraction < 0.999f || !armed ||
        !ai::YaPBActionExecutionContext::armModelIOTrial(bot, trialBackend, request.end)) {
      fail(&bot, ai::runtime_io::ModelIOTrialStopReason::InvalidAction, "unsafe trace or unable to arm backend");
      return true;
    }

    destination = request.end;
    origin = state.self.origin;
    sessionId = state.stamp.sessionId;
    started = lastTime = state.stamp.gameTime;
    pending = false;
    running = true;
    game.print("AiPB modelio: armed; bot=%d; backend=%s; port=%u; from=(%.1f,%.1f,%.1f); to=(%.1f,%.1f,%.1f).",
        selected, externalMode ? "ExternalLoopback" : "Deterministic", externalPort,
        origin.x, origin.y, origin.z,
        destination.x, destination.y, destination.z);
    return true;
  }

  if (state.stamp.gameTime - started > kTimeout) {
    fail(&bot, ai::runtime_io::ModelIOTrialStopReason::Timeout, "timeout");
    return true;
  }
  const auto observation = ai::buildObservation(ai::buildObservationInput(bot));
  const auto result = ai::YaPBActionExecutionContext::stepModelIOTrial(bot, observation);
  if (sessionMode && sessionBudget.expired(externalClock.milliseconds())) {
    fail(&bot, ai::runtime_io::ModelIOTrialStopReason::Timeout, "session wall-clock deadline");
    return true;
  }

  if (awaitingFeedback) {
    const bool observed = externalMode ? externalBackend.terminalFeedbackObserved() :
        backend.terminalFeedbackObserved();
    const auto feedbackResult = externalMode ? externalBackend.feedbackResult() : backend.feedbackResult();
    const auto feedbackSession = externalMode ? externalBackend.sessionId() : backend.sessionId();
    const auto feedbackFrame = externalMode ? externalBackend.feedbackFrame() : backend.feedbackFrame();
    const auto decisionFrame = externalMode ? externalBackend.decisionFrame() : backend.decisionFrame();
    if (!observed || feedbackResult != ai::ActionResultType::Completed ||
        feedbackSession != sessionId || feedbackFrame <= decisionFrame) {
      fail(&bot, ai::runtime_io::ModelIOTrialStopReason::InvalidAction, "missing or invalid terminal feedback");
      return true;
    }
    lastRequests = externalMode ? externalBackend.requests() : backend.requests();
    lastDecisionFrame = decisionFrame;
    lastFeedbackFrame = feedbackFrame;
    lastElapsed = state.stamp.gameTime - started;
    lastMoved = ai::mockDistanceToTarget(origin, state.self.origin);
    lastRemaining = ai::mockDistanceToTarget(state.self.origin, destination);
    const auto completedSession = sessionId;
    const bool physicallyReached = ai::mockReachedTarget(state.self.origin, destination);
    const bool sequential = sequence.total() > 0;
    const bool continuingSession = sessionMode;
    const int botIndex = selected;
    const uint16_t port = externalPort;
    clear(&bot, ai::runtime_io::ModelIOTrialStopReason::Completed);
    const bool reachedAndRecovered = physicallyReached && lastRecovery.verified();
    if (sequential) {
      if (!sequence.completeLeg(completedSession, physicallyReached, lastRecovery.verified(),
          lastRequests, lastMoved)) {
        lastCompleted = false;
        lastStopReason = physicallyReached
            ? ai::runtime_io::ModelIOTrialStopReason::RecoveryFailed
            : ai::runtime_io::ModelIOTrialStopReason::InvalidAction;
        lastSequenceTotal = sequence.total();
        lastSequenceCompleted = sequence.completed();
        lastSequenceRequests = sequence.requests();
        sequence.reset();
        if (sessionBudget.active()) {
          lastSessionElapsedMs = sessionBudget.elapsed(externalClock.milliseconds());
          sessionBudget.reset();
        }
        game.print("AiPB modelio: sequence stopped; reason=%s; completed=%u/%u; recovered=%d.",
            ai::runtime_io::modelIOTrialStopReasonName(lastStopReason),
            static_cast<unsigned>(lastSequenceCompleted), static_cast<unsigned>(lastSequenceTotal),
            lastRecovery.verified());
        return true;
      }
      lastSequenceTotal = sequence.total();
      lastSequenceCompleted = sequence.completed();
      lastSequenceRequests = sequence.requests();
      game.print("AiPB modelio: sequence leg=%u/%u PASS; requests=%u; moved=%.2f; recovered=%d.",
          static_cast<unsigned>(sequence.completed()), static_cast<unsigned>(sequence.total()),
          lastRequests, lastMoved, lastRecovery.verified());
      if (sequence.hasNext()) {
        selected = botIndex;
        externalMode = true;
        externalPort = port;
        sessionMode = continuingSession;
        pending = true;
        running = false;
        awaitingFeedback = false;
        game.print("AiPB modelio: sequence awaiting fresh preflight for leg=%u.",
            static_cast<unsigned>(sequence.completed() + 1));
        return true;
      }
      sequence.reset();
    }
    if (sessionBudget.active()) {
      lastSessionElapsedMs = sessionBudget.elapsed(externalClock.milliseconds());
      sessionBudget.reset();
    }
    lastCompleted = reachedAndRecovered;
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
  if (result.type != ai::ActionResultType::Accepted ||
      !(externalMode ? externalBackend.decided() : backend.decided()) ||
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
    if (sessionBudget.active()) {
      lastSessionElapsedMs = sessionBudget.elapsed(externalClock.milliseconds());
      sessionBudget.reset();
    }
    if (sequence.total() > 0) {
      lastSequenceTotal = sequence.total();
      lastSequenceCompleted = sequence.completed();
      lastSequenceRequests = sequence.requests();
      sequence.reset();
    }
    clear(bot, ai::runtime_io::ModelIOTrialStopReason::BotDisconnected);
  }
}

void reset() {
  auto *bot = selected >= 0 ? bots.findBotByIndex(selected) : nullptr;
  if (sessionBudget.active()) {
    lastSessionElapsedMs = sessionBudget.elapsed(externalClock.milliseconds());
    sessionBudget.reset();
  }
  if (sequence.total() > 0) {
    lastSequenceTotal = sequence.total();
    lastSequenceCompleted = sequence.completed();
    lastSequenceRequests = sequence.requests();
    sequence.reset();
  }
  clear(bot, ai::runtime_io::ModelIOTrialStopReason::MapReset);
  lastCompleted = false;
}

} // namespace ai::model_io_trial
