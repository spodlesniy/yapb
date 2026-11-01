//
// AiPB - GoldSrc read-only bot state adapter.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

#include <ai/ai_server_state_adapter.h>

#include <cstdint>

namespace ai::runtime_io {
namespace {

ServerStateCache g_serverState {};

} // namespace

void captureBotFrame(Bot &bot) {
  const int slot = bot.index();
  if (slot < 0 || slot >= kServerBotSlots) {
    return;
  }

  auto *entity = bot.ent();
  if (globals == nullptr || entity == nullptr || bot.pev != &entity->v ||
      bots.findBotByIndex(slot) != &bot || !game.isFakeClientEntity(entity)) {
    g_serverState.release(slot);
    return;
  }

  const auto &state = entity->v;
  BotSelfState self {};
  self.origin = { state.origin.x, state.origin.y, state.origin.z };
  self.velocity = { state.velocity.x, state.velocity.y, state.velocity.z };
  self.viewAngles = { state.v_angle.x, state.v_angle.y, state.v_angle.z };
  self.health = state.health;
  self.armor = state.armorvalue;
  self.maxSpeed = state.maxspeed;
  self.alive = game.isAliveEntity(entity);
  self.onGround = !!(state.flags & FL_ONGROUND);
  self.crouching = !!(state.flags & FL_DUCKING);

  // Bot object identity is scoped by explicit disconnect/map invalidation.
  const auto identity = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&bot));
  g_serverState.capture(slot, identity, gameState.getTelemetryRoundId(), game.time(), globals->frametime, self);
}

void releaseBot(int botSlot) {
  g_serverState.release(botSlot);
}

void resetServerState() {
  g_serverState.reset();
}

bool readServerBotSnapshot(int botSlot, ServerBotSnapshot &snapshot) {
  return g_serverState.read(botSlot, snapshot);
}

} // namespace ai::runtime_io
