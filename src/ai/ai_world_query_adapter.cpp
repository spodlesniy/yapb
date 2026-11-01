//
// AiPB - bounded GoldSrc geometry query adapter.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

#include <ai/ai_server_state_adapter.h>
#include <ai/ai_world_query_adapter.h>

namespace ai::runtime_io {
namespace {

WorldQueryGate g_queryGate {};

} // namespace

WorldQueryResult queryBotWorld(int botSlot, const WorldQueryRequest &request) {
  auto result = rejectedWorldQuery(request, QueryStatus::Stale);
  if (botSlot < 0 || botSlot >= kServerBotSlots || globals == nullptr) {
    return result;
  }

  ServerBotSnapshot snapshot {};
  if (!readServerBotSnapshot(botSlot, snapshot) ||
      snapshot.stamp.gameTime != game.time() ||
      snapshot.stamp.roundId != gameState.getTelemetryRoundId()) {
    return result;
  }

  Bot *bot = bots.findBotByIndex(botSlot);
  if (bot == nullptr || bot->pev == nullptr || bot->ent() == nullptr ||
      bot->pev != &bot->ent()->v || !game.isFakeClientEntity(bot->ent())) {
    return result;
  }

  const QueryStatus status = g_queryGate.admit(botSlot, snapshot, request);
  if (status != QueryStatus::Completed) {
    return rejectedWorldQuery(request, status);
  }

  const Vector start(request.start.x, request.start.y, request.start.z);
  const Vector end(request.end.x, request.end.y, request.end.z);
  TraceResult trace {};
  if (request.kind == QueryKind::TraceLine) {
    game.testLine(start, end, TraceIgnore::Monsters, bot->ent(), &trace);
  }
  else {
    const int hull = request.hull == kWorldQueryCrouchingHull ? head_hull : human_hull;
    game.testHull(start, end, TraceIgnore::Monsters, hull, bot->ent(), &trace);
  }

  GeometryTraceSample sample {};
  sample.fraction = trace.flFraction;
  sample.endPosition = { trace.vecEndPos.x, trace.vecEndPos.y, trace.vecEndPos.z };
  sample.planeNormal = { trace.vecPlaneNormal.x, trace.vecPlaneNormal.y, trace.vecPlaneNormal.z };
  sample.startSolid = !!trace.fStartSolid;
  sample.hitPlayer = trace.pHit != nullptr && game.isPlayerEntity(trace.pHit);
  return worldTraceResult(request, sample);
}

void releaseWorldQueryBot(int botSlot) {
  g_queryGate.release(botSlot);
}

void resetWorldQueries() {
  g_queryGate.reset();
}

} // namespace ai::runtime_io
