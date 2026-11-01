//
// AiPB - explicit one-shot mock action trial on live GoldSrc.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

#include <ai/ai_mock_trial.h>
#include <ai/ai_exploration_probe.h>
#include <ai/ai_server_state_adapter.h>
#include <ai/ai_world_query_adapter.h>
#include <ai/ai_yapb_action_execution_context.h>

#include <cmath>
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
ai::Vec3 origin {};
uint64_t expectedSession {};
uint64_t nextRequestId {};

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
  game.print("AiPB mock: cancelled (%s).", reason);
  clearSelection();
}

} // namespace

bool handleServer() {
  if (engfuncs.pfnCmd_Argc() < 2 || std::strcmp(engfuncs.pfnCmd_Argv(1), "mock") != 0) {
    return false;
  }

  const int argc = engfuncs.pfnCmd_Argc();
  const char *operation = argc >= 3 ? engfuncs.pfnCmd_Argv(2) : "";
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
    game.print("Usage: aipb mock run <bot-index> | status | stop");
    return true;
  }
  const char *argument = engfuncs.pfnCmd_Argv(3);
  char *end {};
  const long index = std::strtol(argument, &end, 10);
  if (argument == end || *end != '\0' || index < 0 || index >= ai::runtime_io::kServerBotSlots ||
      selectedBot >= 0 || ai::explore::active() || gameState.isRoundOver()) {
    game.print("AiPB mock: rejected (InvalidBotOrBusy).");
    return true;
  }
  auto *bot = bots.findBotByIndex(static_cast<int>(index));
  if (bot == nullptr || bot->pev == nullptr || !game.isAliveEntity(bot->ent()) ||
      ai::YaPBActionExecutionContext::runtimeActive(*bot)) {
    game.print("AiPB mock: rejected (BotUnavailableOrBusy).");
    return true;
  }
  selectedBot = static_cast<int>(index);
  pending = true;
  game.print("AiPB mock: requested for bot #%d.", selectedBot);
  return true;
}

void frame(Bot &bot) {
  if (selectedBot != bot.index()) {
    return;
  }
  ai::runtime_io::ServerBotSnapshot state {};
  if (!ai::runtime_io::readServerBotSnapshot(selectedBot, state) ||
      !state.self.alive || !std::isfinite(state.self.maxSpeed) || state.self.maxSpeed < 10.0f ||
      ai::explore::active() || gameState.isRoundOver() ||
      (executing && state.stamp.sessionId != expectedSession)) {
    abortTrial(&bot, "stale bot, round or competing owner");
    return;
  }

  if (pending) {
    if (!state.self.onGround || ai::YaPBActionExecutionContext::runtimeActive(bot)) {
      abortTrial(&bot, "bot not grounded or already executing");
      return;
    }
    const float yaw = state.self.viewAngles.y * 0.017453292519943295f;
    ai::runtime_io::WorldQueryRequest request {};
    request.requestId = ++nextRequestId;
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
      return;
    }
    origin = state.self.origin;
    expectedSession = state.stamp.sessionId;
    started = state.stamp.gameTime;
    pending = false;
    executing = true;
    game.print("AiPB mock: bot #%d armed MoveToPosition; from=(%.1f,%.1f,%.1f) to=(%.1f,%.1f,%.1f).",
               selectedBot, origin.x, origin.y, origin.z, request.end.x, request.end.y, request.end.z);
    return;
  }

  const auto outcome = ai::YaPBActionExecutionContext::mockRuntimeResult(bot);
  if (outcome.type != ai::ActionResultType::None && outcome.isTerminal()) {
    const float moved = displacement(origin, state.self.origin);
    game.print("AiPB mock: result=%s; physical_displacement=%.2f; action_elapsed=%.3f; session=%llu; frame=%llu.",
               resultName(outcome.type), moved, outcome.elapsedTime,
               static_cast<unsigned long long>(state.stamp.sessionId),
               static_cast<unsigned long long>(state.stamp.frameId));
    clearSelection();
  }
  else if (state.stamp.gameTime - started > kTrialTimeout) {
    abortTrial(&bot, "action timeout");
  }
}

void cancelBot(int botIndex) {
  if (selectedBot == botIndex) {
    clearSelection();
  }
}

void reset() {
  clearSelection();
}

} // namespace ai::mock
