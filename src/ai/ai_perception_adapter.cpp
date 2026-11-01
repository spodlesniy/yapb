//
// AiPB - GoldSrc perception adapter, read-only and without model control.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

#include <ai/ai_perception_adapter.h>
#include <ai/ai_perception_guard.h>
#include <ai/ai_server_state_adapter.h>

#include <cmath>

namespace ai::runtime_io {
namespace {

PerceptionFrameCache g_perception {};

} // namespace

void capturePerceptionFrame(Bot &bot) {
  const int slot = bot.index();
  if (slot < 0 || slot >= kServerBotSlots) {
    return;
  }

  ServerBotSnapshot snapshot {};
  if (!readServerBotSnapshot(slot, snapshot)) {
    g_perception.release(slot);
    return;
  }

  PerceptionSignals signals {};
  signals.roundOver = gameState.isRoundOver();
  const float now = snapshot.stamp.gameTime;

  // Use only YaPB's confirmed sighting and its previously observed location.
  // Never query the target's current entvars position behind the perception boundary.
  if (bot.m_blindTime <= now &&
      canUseLiveEnemyState(!!(bot.m_states & Sense::SeeingEnemy),
                           !!(bot.m_states & Sense::SuspectEnemy),
                           bot.m_enemy != nullptr && bot.m_enemy == bot.m_lastEnemy) &&
      !game.isNullEntity(bot.m_enemy) && game.isPlayerEntity(bot.m_enemy) &&
      game.isAliveEntity(bot.m_enemy)) {
    signals.visibleEnemy = true;
    signals.enemyEntityIndex = game.indexOfEntity(bot.m_enemy);
    signals.seenPosition = { bot.m_lastEnemyOrigin.x, bot.m_lastEnemyOrigin.y, bot.m_lastEnemyOrigin.z };
  }

  // The shared client noise cache stores the sound origin and its audible radius.
  // The source entity and its live coordinates are intentionally not exported.
  if ((bot.m_states & Sense::HearingEnemy) &&
      !game.isNullEntity(bot.m_hearedEnemy) &&
      game.isPlayerEntity(bot.m_hearedEnemy) && bot.pev != nullptr) {
    for (const auto &client : util.getClients()) {
      if (!(client.flags & ClientFlags::Used) || client.ent != bot.m_hearedEnemy) {
        continue;
      }
      const auto &noise = client.noise;
      if (std::isfinite(noise.last) && noise.last > now &&
          std::isfinite(noise.dist) && noise.dist > 0.0f &&
          std::isfinite(noise.pos.x) && std::isfinite(noise.pos.y) && std::isfinite(noise.pos.z) &&
          noise.pos.distanceSq(bot.pev->origin) <= noise.dist * noise.dist) {
        signals.heardSound = true;
        signals.soundPosition = { noise.pos.x, noise.pos.y, noise.pos.z };
      }
      break;
    }
  }

  g_perception.update(slot, snapshot, signals);
}

void releasePerceptionBot(int botSlot) {
  g_perception.release(botSlot);
}

void resetPerceptionFrames() {
  g_perception.reset();
}

bool readPerceptionFrame(int botSlot, PerceptionFrame &frame) {
  return g_perception.read(botSlot, frame);
}

} // namespace ai::runtime_io
