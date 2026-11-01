//
// AiPB - GoldSrc perception adapter, read-only and without model control.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

#include <ai/ai_perception_adapter.h>
#include <ai/ai_bot_adapter.h>
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

  // buildObservationInput is the existing friend of Bot authorized to read
  // private sensory flags. Consume only confirmed evidence, never its raw
  // unseen player positions, health, or armor.
  const auto observed = ai::buildObservationInput(bot);
  const auto flags = observed.combat.perceptionFlags;
  const bool seeing = !!(flags & static_cast<uint32_t>(ai::PerceptionFlag::SeeingEnemy));
  const bool suspect = !!(flags & static_cast<uint32_t>(ai::PerceptionFlag::SuspectedEnemy));
  const bool hearing = !!(flags & static_cast<uint32_t>(ai::PerceptionFlag::HearingEnemy));

  if (!observed.combat.blind &&
      ai::canUseLiveEnemyState(seeing, suspect,
          observed.combat.enemyEntity > 0 &&
          observed.combat.enemyEntity == observed.combat.lastEnemyEntity) &&
      observed.combat.enemyEntity > 0) {
    signals.visibleEnemy = true;
    signals.enemyEntityIndex = observed.combat.enemyEntity;
    signals.seenPosition = observed.combat.lastEnemyOrigin;
  }

  // Only a client positively identified as heard by the existing friend
  // adapter may contribute its acoustic origin. Never export its entity ID.
  if (hearing && bot.pev != nullptr) {
    const auto count = observed.playerCount <= ai::kMaxObservedPlayers
        ? observed.playerCount : ai::kMaxObservedPlayers;
    for (size_t i = 0; i < count && !signals.heardSound; ++i) {
      const auto &heard = observed.players[i];
      if (!heard.valid || !heard.heard || !heard.enemy || heard.entityIndex <= 0) {
        continue;
      }
      for (const auto &client : util.getClients()) {
        if (!(client.flags & ClientFlags::Used) || client.ent == nullptr ||
            game.indexOfEntity(client.ent) != heard.entityIndex) {
          continue;
        }
        const auto &noise = client.noise;
        if (std::isfinite(noise.last) && noise.last > now &&
            std::isfinite(noise.dist) && noise.dist > 0.0f &&
            std::isfinite(noise.pos.x) && std::isfinite(noise.pos.y) &&
            std::isfinite(noise.pos.z) &&
            noise.pos.distanceSq(bot.pev->origin) <= noise.dist * noise.dist) {
          signals.heardSound = true;
          signals.soundPosition = { noise.pos.x, noise.pos.y, noise.pos.z };
        }
        break;
      }
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
