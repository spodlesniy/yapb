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

float normalizeDifficulty(int difficulty) {
  if (difficulty <= Difficulty::Noob) {
    return 0.0f;
  }

  if (difficulty >= Difficulty::Expert) {
    return 1.0f;
  }

  return static_cast<float>(difficulty) / static_cast<float>(Difficulty::Expert);
}

float unitValue(float value) {
  return cr::clamp(value, 0.0f, 1.0f);
}

WeaponType mapWeaponType(int weaponType) {
  switch (weaponType) {
  case ::WeaponType::None:
    return WeaponType::None;
  case ::WeaponType::Melee:
    return WeaponType::Melee;
  case ::WeaponType::Pistol:
    return WeaponType::Pistol;
  case ::WeaponType::Shotgun:
    return WeaponType::Shotgun;
  case ::WeaponType::ZoomRifle:
    return WeaponType::ZoomRifle;
  case ::WeaponType::Rifle:
    return WeaponType::Rifle;
  case ::WeaponType::SMG:
    return WeaponType::SMG;
  case ::WeaponType::Sniper:
    return WeaponType::Sniper;
  case ::WeaponType::Heavy:
    return WeaponType::Heavy;
  }
  return WeaponType::Unknown;
}

ReloadState mapReloadState(int reloadState) {
  switch (reloadState) {
  case ::Reload::None:
    return ReloadState::None;
  case ::Reload::Primary:
    return ReloadState::Primary;
  case ::Reload::Secondary:
    return ReloadState::Secondary;
  }
  return ReloadState::None;
}

TaskType mapTask(Task task) {
  switch (task) {
  case Task::Normal:
    return TaskType::Normal;
  case Task::Pause:
    return TaskType::Pause;
  case Task::MoveToPosition:
    return TaskType::MoveToPosition;
  case Task::FollowUser:
    return TaskType::FollowUser;
  case Task::PickupItem:
    return TaskType::PickupItem;
  case Task::Camp:
    return TaskType::Camp;
  case Task::PlantBomb:
    return TaskType::PlantBomb;
  case Task::DefuseBomb:
    return TaskType::DefuseBomb;
  case Task::Attack:
    return TaskType::Attack;
  case Task::Hunt:
    return TaskType::Hunt;
  case Task::SeekCover:
    return TaskType::SeekCover;
  case Task::ThrowExplosive:
    return TaskType::ThrowExplosive;
  case Task::ThrowFlashbang:
    return TaskType::ThrowFlashbang;
  case Task::ThrowSmoke:
    return TaskType::ThrowSmoke;
  case Task::DoubleJump:
    return TaskType::DoubleJump;
  case Task::EscapeFromBomb:
    return TaskType::EscapeFromBomb;
  case Task::ShootBreakable:
    return TaskType::ShootBreakable;
  case Task::Hide:
    return TaskType::Hide;
  case Task::Blind:
    return TaskType::Blind;
  case Task::Spraypaint:
    return TaskType::Spraypaint;
  case Task::Max:
    break;
  }
  return TaskType::Unknown;
}

void appendWaypoint(ObservationInput &input, int index, uint16_t connectionFlags) {
  if (!graph.exists(index) || input.waypointCount >= kMaxObservedWaypoints) {
    return;
  }

  auto &waypoint = input.waypoints[input.waypointCount++];
  const auto &path = graph[index];

  waypoint.index = path.number;
  waypoint.origin = { path.origin.x, path.origin.y, path.origin.z };
  waypoint.nodeFlags = static_cast<uint32_t>(path.flags);
  waypoint.connectionFlags = connectionFlags;
}

} // namespace

ObservationInput buildObservationInput(const Bot &bot) {
  ObservationInput input {};

  if (bot.pev == nullptr) {
    return input;
  }

  const auto *entity = bot.ent();
  if (entity == nullptr) {
    return input;
  }

  input.gameTime = game.time();
  input.roundTimeRemaining = cr::max(0.0f, gameState.getRoundEndTime() - input.gameTime);

  input.bot.origin = { bot.pev->origin.x, bot.pev->origin.y, bot.pev->origin.z };
  input.bot.velocity = { bot.pev->velocity.x, bot.pev->velocity.y, bot.pev->velocity.z };
  input.bot.destination = { bot.m_destOrigin.x, bot.m_destOrigin.y, bot.m_destOrigin.z };
  input.bot.desiredVelocity = { bot.m_desiredVelocity.x, bot.m_desiredVelocity.y, bot.m_desiredVelocity.z };
  input.bot.health = bot.m_healthValue;
  input.bot.armor = bot.pev->armorvalue;
  input.bot.maxSpeed = bot.pev->maxspeed;
  input.bot.team = bot.m_team;
  input.bot.difficulty = bot.m_difficulty;
  input.bot.currentWeapon = bot.m_currentWeapon;
  input.bot.currentNode = bot.m_currentNodeIndex;
  input.bot.currentGoalNode = bot.m_chosenGoalIndex;
  input.bot.currentTask = mapTask(bot.getCurrentTaskId());
  input.bot.alive = bot.m_isAlive;
  input.bot.hasC4 = bot.m_hasC4;
  input.bot.hasHostage = bot.m_hasHostage;
  input.bot.inBombZone = bot.m_inBombZone;
  input.bot.inBuyZone = bot.m_inBuyZone;
  input.bot.inRescueZone = bot.m_inRescueZone;

  if (bot.m_currentTravelFlags & PathFlag::Jump) {
    input.bot.navigationFlags |= static_cast<uint32_t>(NavigationFlag::Jump);
  }
  if (bot.m_pathFlags & NodeFlag::Ladder) {
    input.bot.navigationFlags |= static_cast<uint32_t>(NavigationFlag::Ladder);
  }
  if (bot.m_pathFlags & NodeFlag::Crouch) {
    input.bot.navigationFlags |= static_cast<uint32_t>(NavigationFlag::Crouch);
  }
  if (bot.m_isFallDown) {
    input.bot.navigationFlags |= static_cast<uint32_t>(NavigationFlag::Falling);
  }
  input.bot.movingToGoal = bot.m_moveToGoal;
  input.bot.stuck = bot.m_isStuck;

  input.combat.weaponType = mapWeaponType(bot.m_weaponType);
  if (bot.m_currentWeapon >= 0 && bot.m_currentWeapon < kMaxWeapons) {
    input.combat.ammoInClip = bot.m_ammoInClip[bot.m_currentWeapon];
  }
  input.combat.reloadState = mapReloadState(bot.m_reloadState);
  input.combat.blind = bot.m_blindTime > game.time();
  input.combat.blindTimeRemaining = input.combat.blind ? cr::max(0.0f, bot.m_blindTime - game.time()) : 0.0f;
  input.combat.firePauseRemaining = cr::max(0.0f, bot.m_firePause - game.time());

  if (!game.isNullEntity(bot.m_enemy)) {
    input.combat.enemyEntity = game.indexOfEntity(bot.m_enemy);
    input.combat.enemyOrigin = { bot.m_enemy->v.origin.x, bot.m_enemy->v.origin.y, bot.m_enemy->v.origin.z };
  }
  if (!game.isNullEntity(bot.m_lastEnemy)) {
    input.combat.lastEnemyEntity = game.indexOfEntity(bot.m_lastEnemy);
  }
  input.combat.lastEnemyOrigin = { bot.m_lastEnemyOrigin.x, bot.m_lastEnemyOrigin.y, bot.m_lastEnemyOrigin.z };

  if (bot.m_states & Sense::SeeingEnemy) {
    input.combat.perceptionFlags |= static_cast<uint32_t>(PerceptionFlag::SeeingEnemy);
  }
  if (bot.m_states & Sense::HearingEnemy) {
    input.combat.perceptionFlags |= static_cast<uint32_t>(PerceptionFlag::HearingEnemy);
  }
  if (bot.m_states & Sense::SuspectEnemy) {
    input.combat.perceptionFlags |= static_cast<uint32_t>(PerceptionFlag::SuspectedEnemy);
  }
  if (bot.m_isEnemyReachable) {
    input.combat.perceptionFlags |= static_cast<uint32_t>(PerceptionFlag::EnemyReachable);
  }

  if (gameState.isBombPlanted()) {
    input.bot.objectiveFlags |= ObjectiveFlag::BombPlanted;
  }
  if (bot.m_hasC4) {
    input.bot.objectiveFlags |= ObjectiveFlag::BombCarrier;
  }
  if (bot.m_hasHostage) {
    input.bot.objectiveFlags |= ObjectiveFlag::HasHostage;
  }
  if (bot.m_inBombZone) {
    input.bot.objectiveFlags |= ObjectiveFlag::InBombZone;
  }
  if (bot.m_inRescueZone) {
    input.bot.objectiveFlags |= ObjectiveFlag::InRescueZone;
  }
  if (bot.m_inEscapeZone) {
    input.bot.objectiveFlags |= ObjectiveFlag::InEscapeZone;
  }
  if (bot.m_inVIPZone) {
    input.bot.objectiveFlags |= ObjectiveFlag::InVIPZone;
  }

  input.personality.skill = normalizeDifficulty(bot.m_difficulty);
  input.personality.aggression = unitValue(bot.m_agressionLevel);
  input.personality.risk = 1.0f - unitValue(bot.m_fearLevel);

  const auto currentNode = bot.m_currentNodeIndex;
  appendWaypoint(input, currentNode, 0);

  if (graph.exists(currentNode)) {
    const auto &path = graph[currentNode];

    for (const auto &link : path.links) {
      if (link.index == kInvalidNodeIndex) {
        continue;
      }

      appendWaypoint(input, link.index, link.flags);
    }
  }

  for (const auto &client : util.getClients()) {
    if (input.playerCount >= kMaxObservedPlayers) {
      break;
    }

    if (!(client.flags & ClientFlags::Used) || client.ent == nullptr || client.ent == entity) {
      continue;
    }

    auto &player = input.players[input.playerCount++];
    const int playerTeam = game.is(GameFlags::FreeForAll) ? game.getRealPlayerTeam(client.ent) : game.getPlayerTeam(client.ent);

    player.entityIndex = game.indexOfEntity(client.ent);
    player.origin = { client.ent->v.origin.x, client.ent->v.origin.y, client.ent->v.origin.z };
    player.health = client.ent->v.health;
    player.armor = client.ent->v.armorvalue;
    player.team = playerTeam;
    player.valid = true;
    player.alive = !!(client.flags & ClientFlags::Alive);
    player.enemy = (playerTeam == Team::Terrorist || playerTeam == Team::CT) && playerTeam != bot.m_team;
    player.visible = client.ent == bot.m_enemy && (bot.m_states & Sense::SeeingEnemy) && !(bot.m_states & Sense::SuspectEnemy);
    player.heard = client.ent == bot.m_hearedEnemy;
  }

  return input;
}

} // namespace ai
