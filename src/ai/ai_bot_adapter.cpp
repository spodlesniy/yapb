//
// AiPB - Bot-to-AI observation adapter.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

#include <ai/ai_bot_adapter.h>

namespace ai {
namespace {

float normalizeDifficulty (int difficulty) {
  if (difficulty <= Difficulty::Noob) {
    return 0.0f;
  }

  if (difficulty >= Difficulty::Expert) {
    return 1.0f;
  }

  return static_cast<float> (difficulty) / static_cast<float> (Difficulty::Expert);
}

float unitValue (float value) {
  return cr::clamp (value, 0.0f, 1.0f);
}

void appendWaypoint (ObservationInput &input, int index, uint16_t connectionFlags) {
  if (!graph.exists (index) || input.waypointCount >= kMaxObservedWaypoints) {
    return;
  }

  auto &waypoint = input.waypoints[input.waypointCount++];
  const auto &path = graph[index];

  waypoint.index = path.number;
  waypoint.origin = { path.origin.x, path.origin.y, path.origin.z };
  waypoint.nodeFlags = static_cast<uint32_t> (path.flags);
  waypoint.connectionFlags = connectionFlags;
}

} // namespace

ObservationInput buildObservationInput (const Bot &bot) {
  ObservationInput input {};

  if (bot.pev == nullptr) {
    return input;
  }

  const auto *entity = bot.ent ();
  if (entity == nullptr) {
    return input;
  }

  input.gameTime = game.time ();
  input.roundTimeRemaining = cr::max (0.0f, gameState.getRoundEndTime () - input.gameTime);

  input.bot.origin = { bot.pev->origin.x, bot.pev->origin.y, bot.pev->origin.z };
  input.bot.velocity = { bot.pev->velocity.x, bot.pev->velocity.y, bot.pev->velocity.z };
  input.bot.health = bot.m_healthValue;
  input.bot.armor = bot.pev->armorvalue;
  input.bot.maxSpeed = bot.pev->maxspeed;
  input.bot.team = bot.m_team;
  input.bot.difficulty = bot.m_difficulty;
  input.bot.currentWeapon = bot.m_currentWeapon;
  input.bot.currentNode = bot.m_currentNodeIndex;
  input.bot.currentGoalNode = bot.m_chosenGoalIndex;
  input.bot.alive = bot.m_isAlive;
  input.bot.hasC4 = bot.m_hasC4;
  input.bot.hasHostage = bot.m_hasHostage;
  input.bot.inBombZone = bot.m_inBombZone;
  input.bot.inBuyZone = bot.m_inBuyZone;
  input.bot.inRescueZone = bot.m_inRescueZone;

  input.personality.skill = normalizeDifficulty (bot.m_difficulty);
  input.personality.aggression = unitValue (bot.m_agressionLevel);
  input.personality.risk = 1.0f - unitValue (bot.m_fearLevel);

  const auto currentNode = bot.m_currentNodeIndex;
  appendWaypoint (input, currentNode, 0);

  if (graph.exists (currentNode)) {
    const auto &path = graph[currentNode];

    for (const auto &link : path.links) {
      if (link.index == kInvalidNodeIndex) {
        continue;
      }

      appendWaypoint (input, link.index, link.flags);
    }
  }

  for (const auto &client : util.getClients ()) {
    if (input.playerCount >= kMaxObservedPlayers) {
      break;
    }

    if (!(client.flags & ClientFlags::Used) || client.ent == nullptr || client.ent == entity) {
      continue;
    }

    auto &player = input.players[input.playerCount++];
    const int playerTeam = game.is (GameFlags::FreeForAll) ? game.getRealPlayerTeam (client.ent) : game.getPlayerTeam (client.ent);

    player.entityIndex = game.indexOfEntity (client.ent);
    player.origin = { client.ent->v.origin.x, client.ent->v.origin.y, client.ent->v.origin.z };
    player.health = client.ent->v.health;
    player.armor = client.ent->v.armorvalue;
    player.team = playerTeam;
    player.valid = true;
    player.alive = !!(client.flags & ClientFlags::Alive);
    player.enemy = (playerTeam == Team::Terrorist || playerTeam == Team::CT) && playerTeam != bot.m_team;
    player.visible = client.ent == bot.m_enemy
      && (bot.m_states & Sense::SeeingEnemy)
      && !(bot.m_states & Sense::SuspectEnemy);
    player.heard = client.ent == bot.m_hearedEnemy;
  }

  return input;
}

} // namespace ai
