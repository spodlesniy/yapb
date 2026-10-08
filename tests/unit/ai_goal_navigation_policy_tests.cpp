//
// AiPB - task-aware teacher policy unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_bomb_defense_guard.h>
#include <ai/ai_bomb_search_guard.h>
#include <ai/ai_goal_navigation_policy.h>
#include <ai/ai_navigation_task_guard.h>
#include <ai/ai_objective_navigation_guard.h>

using ai::test::expect;
using ai::test::expectNear;

namespace {

ai::Observation makeObservation() {
  ai::Observation observation {};
  observation.bot.alive = true;
  observation.bot.currentNode = 10;
  observation.bot.currentGoalNode = 20;
  return observation;
}

void addObservedEnemy(ai::Observation &observation, int32_t entityIndex) {
  observation.playerCount = 1;
  observation.players[0].entityIndex = entityIndex;
  observation.players[0].valid = true;
  observation.players[0].alive = true;
  observation.players[0].enemy = true;
  observation.combat.enemyEntity = entityIndex;
}

void addFollowTarget(ai::Observation &observation, int32_t entityIndex) {
  observation.bot.followTargetPlayer = entityIndex;
  observation.playerCount = 1;
  observation.players[0].entityIndex = entityIndex;
  observation.players[0].valid = true;
  observation.players[0].alive = true;
}

void addGoalWaypoint(ai::Observation &observation, float distance) {
  observation.waypointCount = 1;
  observation.waypoints[0].index = observation.bot.currentGoalNode;
  observation.waypoints[0].distance = distance;
}

} // namespace


AI_TEST(testGoalNavigationPolicyKeepsBombCarrierSeekCoverBounded) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::SeekCover;
  observation.bot.hasC4 = true;
  observation.bot.health = 14.0f;
  observation.personality.aggression = 0.5f;
  observation.combat.perceptionFlags |= static_cast<uint32_t>(ai::PerceptionFlag::SeeingEnemy);

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::SeekCover,
         "low-health visible combat keeps C4 carrier cover bounded instead of starting retreat-hide lifecycle");
}

AI_TEST(testGoalNavigationPolicyStillRetreatsLowHealthNonCarrier) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::SeekCover;
  observation.bot.health = 14.0f;
  observation.personality.aggression = 0.5f;
  observation.combat.perceptionFlags |= static_cast<uint32_t>(ai::PerceptionFlag::SeeingEnemy);

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::Retreat,
         "low-health visible combat still starts retreat for a bot without C4");
}

AI_TEST(testGoalNavigationPolicyExploresForNormalTask) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Normal;
  ai::GoalNavigationPolicy policy {};

  const auto action = policy.decide(observation);

  expect(action.type == ai::ActionType::Explore, "normal task maps to explore");
  expect(action.targetType == ai::TargetType::None, "explore leaves waypoint selection to the execution context");
}

AI_TEST(testGoalNavigationPolicyYieldsBombCarrierNavigationToLegacyObjectiveLogic) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Normal;
  observation.bot.hasC4 = true;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::None, "bomb carrier yields normal navigation to legacy objective logic");
}

AI_TEST(testGoalNavigationPolicyYieldsDroppedBombNavigationToLegacyObjectiveLogic) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Normal;
  observation.bot.team = 0;
  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombDropped;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::None, "terrorist yields normal navigation to legacy dropped-bomb recovery");
}

AI_TEST(testGoalNavigationPolicyYieldsDroppedBombMoveTaskToLegacyObjectiveLogic) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::MoveToPosition;
  observation.bot.destination = { 100.0f, 200.0f, 300.0f };
  observation.bot.team = 0;
  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombDropped;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::None, "terrorist yields legacy move-to-position while recovering dropped C4");
}

AI_TEST(testGoalNavigationPolicyYieldsDroppedBombHuntToLegacyObjectiveLogic) {
  auto observation = makeObservation();
  addObservedEnemy(observation, 7);
  observation.combat.lastEnemyEntity = 7;
  observation.combat.enemyEntity = -1;
  observation.bot.currentTask = ai::TaskType::Hunt;
  observation.bot.team = 0;
  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombDropped;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::None, "stale hunt yields to legacy dropped-C4 recovery");
}

AI_TEST(testGoalNavigationPolicyYieldsBombZoneToPlantTaskSelection) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Normal;
  observation.bot.hasC4 = true;
  observation.bot.inBombZone = true;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::None, "bomb carrier in a bomb zone yields to legacy plant-task selection");
}

AI_TEST(testGoalNavigationPolicyRejectsBombZonePickupForCarrier) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::PickupItem;
  observation.bot.hasC4 = true;
  observation.bot.inBombZone = true;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::None, "bomb carrier in a bomb zone does not learn a pickup detour");
}

AI_TEST(testGoalNavigationPolicyPrioritizesBombsiteWhenRoundTimeIsCritical) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Attack;
  observation.bot.hasC4 = true;
  observation.bot.maxSpeed = 250.0f;
  observation.roundTimeRemaining = 20.0f;
  addGoalWaypoint(observation, 2500.0f);
  addObservedEnemy(observation, 7);

  auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::MoveToNode, "urgent bomb carrier prioritizes the bombsite goal");
  expect(action.targetNode == 20, "urgent bomb carrier preserves the current objective goal");

  observation.roundTimeRemaining = 30.0f;
  action = ai::GoalNavigationPolicy {}.decide(observation);
  expect(action.type == ai::ActionType::AttackTarget, "bomb carrier keeps combat priority while enough round time remains");
}

AI_TEST(testGoalNavigationPolicyYieldsCtNormalNavigationAfterBombPlant) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Normal;
  observation.bot.team = 1;
  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombPlanted;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::None, "CT normal navigation yields to legacy planted-bomb search");
}

AI_TEST(testGoalNavigationPolicyYieldsCtMoveTaskAfterBombPlant) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::MoveToPosition;
  observation.bot.destination = { 100.0f, 200.0f, 300.0f };
  observation.bot.team = 1;
  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombPlanted;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::None, "CT legacy move-to-position keeps planted-bomb search ownership");
}

AI_TEST(testNavigationOverrideYieldsToLegacyObjectiveNavigation) {
  expect(!ai::allowsNavigationOverride(
             ai::TaskType::MoveToPosition, ai::TaskType::Normal, ai::TaskType::MoveToPosition, true),
         "legacy objective revokes generic move-to-position ownership");
  expect(ai::allowsNavigationOverride(
             ai::TaskType::MoveToPosition, ai::TaskType::Normal, ai::TaskType::MoveToPosition, false),
         "ordinary move-to-position remains eligible for generic navigation ownership");
}

AI_TEST(testGoalNavigationPolicyYieldsLegacyMoveToPositionTask) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::MoveToPosition;
  observation.bot.destination = { 100.0f, 200.0f, 300.0f };

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::None,
         "legacy move-to-position task yields because observation destination is only the transient path step");
}

AI_TEST(testGoalNavigationPolicyMapsCombatTasks) {
  auto observation = makeObservation();
  addObservedEnemy(observation, 7);
  ai::GoalNavigationPolicy policy {};

  observation.bot.currentTask = ai::TaskType::Attack;
  auto action = policy.decide(observation);
  expect(action.type == ai::ActionType::AttackTarget, "attack task maps to attack target");
  expect(action.targetPlayer == 7, "attack target preserves enemy entity");

  observation.combat.firePauseRemaining = 0.25f;
  action = policy.decide(observation);
  expect(action.type == ai::ActionType::AimAtTarget, "attack task during fire pause maps to aim at target");
  expect(action.targetPlayer == 7, "aim target preserves enemy entity");

  observation.combat.firePauseRemaining = 0.0f;
  observation.combat.lastEnemyEntity = 7;
  observation.combat.enemyEntity = -1;
  observation.bot.currentTask = ai::TaskType::Hunt;
  action = policy.decide(observation);
  expect(action.type == ai::ActionType::HuntTarget, "hunt task maps to hunt target");
  expect(action.targetPlayer == 7, "hunt target preserves last enemy entity without a current enemy");
  expect(action.targetType == ai::TargetType::Player, "hunt targets the observed player");
}

AI_TEST(testGoalNavigationPolicyHuntsLastEnemyWhenCurrentEnemyDiffers) {
  auto observation = makeObservation();
  addObservedEnemy(observation, 7);
  observation.playerCount = 2;
  observation.players[1] = observation.players[0];
  observation.players[1].entityIndex = 9;
  observation.combat.lastEnemyEntity = 9;
  observation.bot.currentTask = ai::TaskType::Hunt;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);
  expect(action.type == ai::ActionType::HuntTarget, "hunt selects a remembered enemy");
  expect(action.targetPlayer == 9, "hunt uses last enemy instead of current enemy");

  observation.bot.currentTask = ai::TaskType::Attack;
  observation.combat.perceptionFlags |= static_cast<uint32_t>(ai::PerceptionFlag::SeeingEnemy);
  const auto attack = ai::GoalNavigationPolicy {}.decide(observation);
  expect(attack.type == ai::ActionType::AttackTarget && attack.targetPlayer == 7,
         "visible attack still targets the current enemy");
}

AI_TEST(testGoalNavigationPolicyYieldsHuntForInvalidLastEnemy) {
  auto observation = makeObservation();
  addObservedEnemy(observation, 7);
  observation.combat.lastEnemyEntity = 9;
  observation.bot.currentTask = ai::TaskType::Hunt;
  ai::GoalNavigationPolicy policy {};
  expect(policy.decide(observation).type == ai::ActionType::None, "unobserved last enemy yields despite a current enemy and goal");

  observation.combat.lastEnemyEntity = 7;
  observation.players[0].valid = false;
  expect(policy.decide(observation).type == ai::ActionType::None, "invalid last enemy yields");
  observation.players[0].valid = true;
  observation.players[0].alive = false;
  expect(policy.decide(observation).type == ai::ActionType::None, "dead last enemy yields");
  observation.players[0].alive = true;
  observation.players[0].enemy = false;
  expect(policy.decide(observation).type == ai::ActionType::None, "friendly last enemy yields");
}

AI_TEST(testGoalNavigationPolicyMapsFollowUser) {
  auto observation = makeObservation();
  addFollowTarget(observation, 7);
  observation.bot.currentTask = ai::TaskType::FollowUser;
  const auto action = ai::GoalNavigationPolicy {}.decide(observation);
  expect(action.type == ai::ActionType::FollowPlayer, "follow task maps to follow player");
  expect(action.targetPlayer == 7, "follow player preserves target entity");
}

AI_TEST(testGoalNavigationPolicyMapsThrowSmoke) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::ThrowSmoke;
  observation.bot.throwTarget = { 100.0f, 200.0f, 300.0f };

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::ThrowSmoke, "smoke task maps to smoke action");
  expect(action.targetType == ai::TargetType::Position, "smoke action targets a position");
  expect(action.targetPosition.x == 100.0f, "smoke target x is preserved");
  expect(action.targetPosition.y == 200.0f, "smoke target y is preserved");
  expect(action.targetPosition.z == 300.0f, "smoke target z is preserved");
  expect(action.grenadeType == ai::GrenadeType::Smoke, "smoke grenade type is preserved");
}

AI_TEST(testGoalNavigationPolicyMapsThrowFlashbang) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::ThrowFlashbang;
  observation.bot.throwTarget = { 100.0f, 200.0f, 300.0f };
  const auto action = ai::GoalNavigationPolicy {}.decide(observation);
  expect(action.type == ai::ActionType::ThrowFlashbang, "flashbang task maps to flashbang action");
  expect(action.targetType == ai::TargetType::Position, "flashbang action targets a position");
  expect(action.targetPosition.x == 100.0f, "flashbang target position is preserved");
  expect(action.grenadeType == ai::GrenadeType::Flashbang, "flashbang action preserves grenade type");
}

AI_TEST(testGoalNavigationPolicyFallsBackForFollowUserWithoutTarget) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::FollowUser;
  const auto action = ai::GoalNavigationPolicy {}.decide(observation);
  expect(action.type == ai::ActionType::MoveToNode, "follow without target falls back to goal");
}

AI_TEST(testGoalNavigationPolicyMapsReloadState) {
  auto observation = makeObservation();
  observation.combat.reloadState = ai::ReloadState::Primary;
  observation.combat.weaponType = ai::WeaponType::Rifle;

  auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::Reload, "primary reload maps to reload for a rifle");
  expect(action.weaponType == ai::WeaponType::Unknown, "primary reload leaves automatic weapon selection");

  observation.combat.reloadState = ai::ReloadState::Secondary;
  observation.combat.weaponType = ai::WeaponType::Pistol;
  action = ai::GoalNavigationPolicy {}.decide(observation);
  expect(action.type == ai::ActionType::Reload, "secondary reload maps to reload for a pistol");
  expect(action.weaponType == ai::WeaponType::Pistol, "secondary reload maps to pistol category");
}

AI_TEST(testGoalNavigationPolicyMapsPrimaryReloadForAllPrimaryWeaponCategories) {
  constexpr ai::WeaponType weapons[] = {
    ai::WeaponType::Shotgun,
    ai::WeaponType::ZoomRifle,
    ai::WeaponType::Rifle,
    ai::WeaponType::SMG,
    ai::WeaponType::Sniper,
    ai::WeaponType::Heavy,
  };

  auto observation = makeObservation();
  observation.combat.reloadState = ai::ReloadState::Primary;

  for (const auto weaponType : weapons) {
    observation.combat.weaponType = weaponType;
    const auto action = ai::GoalNavigationPolicy {}.decide(observation);
    expect(action.type == ai::ActionType::Reload, "primary reload is accepted for a reloadable primary weapon");
    expect(action.weaponType == ai::WeaponType::Unknown, "primary reload keeps automatic weapon selection");
  }
}

AI_TEST(testGoalNavigationPolicyRejectsIncompatiblePrimaryReload) {
  constexpr ai::WeaponType weapons[] = {
    ai::WeaponType::Unknown,
    ai::WeaponType::None,
    ai::WeaponType::Melee,
    ai::WeaponType::Pistol,
  };

  auto observation = makeObservation();
  observation.combat.reloadState = ai::ReloadState::Primary;

  for (const auto weaponType : weapons) {
    observation.combat.weaponType = weaponType;
    const auto action = ai::GoalNavigationPolicy {}.decide(observation);
    expect(action.type == ai::ActionType::MoveToNode, "incompatible primary reload falls back to goal navigation");
  }
}

AI_TEST(testGoalNavigationPolicyRejectsIncompatibleSecondaryReload) {
  constexpr ai::WeaponType weapons[] = {
    ai::WeaponType::Unknown,
    ai::WeaponType::None,
    ai::WeaponType::Melee,
    ai::WeaponType::Shotgun,
    ai::WeaponType::ZoomRifle,
    ai::WeaponType::Rifle,
    ai::WeaponType::SMG,
    ai::WeaponType::Sniper,
    ai::WeaponType::Heavy,
  };

  auto observation = makeObservation();
  observation.combat.reloadState = ai::ReloadState::Secondary;

  for (const auto weaponType : weapons) {
    observation.combat.weaponType = weaponType;
    const auto action = ai::GoalNavigationPolicy {}.decide(observation);
    expect(action.type == ai::ActionType::MoveToNode, "incompatible secondary reload falls back to goal navigation");
  }
}

AI_TEST(testGoalNavigationPolicyMapsBombDefenseToProtectObjective) {
  constexpr ai::TaskType defenseTasks[] = {
    ai::TaskType::Normal,
    ai::TaskType::MoveToPosition,
    ai::TaskType::Camp,
  };

  for (const auto task : defenseTasks) {
    auto observation = makeObservation();
    observation.bot.team = 0;
    observation.bot.currentTask = task;
    observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombPlanted;

    const auto action = ai::GoalNavigationPolicy {}.decide(observation);

    expect(action.type == ai::ActionType::ProtectObjective, "planted-bomb defense maps to protect objective");
    expect(action.targetType == ai::TargetType::None, "protect objective has no explicit target payload");
  }
}

AI_TEST(testGoalNavigationPolicyMapsStaleBombDefenseHuntToProtectObjective) {
  auto observation = makeObservation();
  addObservedEnemy(observation, 7);
  observation.combat.lastEnemyEntity = 7;
  observation.combat.enemyEntity = -1;
  observation.bot.team = 0;
  observation.bot.currentTask = ai::TaskType::Hunt;
  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombPlanted;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::ProtectObjective, "stale hunt maps to planted-C4 protection");
}

AI_TEST(testGoalNavigationPolicyKeepsCombatPriorityDuringBombDefense) {
  auto observation = makeObservation();
  observation.bot.team = 0;
  observation.bot.currentTask = ai::TaskType::Attack;
  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombPlanted;
  observation.combat.perceptionFlags |= static_cast<uint32_t>(ai::PerceptionFlag::SeeingEnemy);
  addObservedEnemy(observation, 7);

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::AttackTarget, "visible combat remains explicit while protecting planted C4");
}

AI_TEST(testGoalNavigationPolicyYieldsCtCampAfterBombPlantToLegacyObjectiveLogic) {
  auto observation = makeObservation();
  observation.bot.team = 1;
  observation.bot.currentTask = ai::TaskType::Camp;
  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombPlanted;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::None, "CT camp yields planted-bomb objective handling to legacy logic");
}

AI_TEST(testGoalNavigationPolicyMapsHostageRescue) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Normal;
  observation.bot.hasHostage = true;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::RescueHostage, "hostage carrying maps to rescue hostage");
  expect(action.targetType == ai::TargetType::None, "rescue hostage has no explicit target payload");
}

AI_TEST(testGoalNavigationPolicyMapsHostageRescueFromMoveToPosition) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::MoveToPosition;
  observation.bot.hasHostage = true;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::RescueHostage, "hostage carrying during navigation maps to rescue hostage");
}

AI_TEST(testGoalNavigationPolicyDoesNotOverrideUnsupportedTaskForHostageRescue) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Attack;
  observation.bot.hasHostage = true;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::MoveToNode, "hostage rescue does not override an incompatible active task");
}

AI_TEST(testGoalNavigationPolicyFallsBackAfterHostageRescueZone) {
  auto observation = makeObservation();
  observation.bot.hasHostage = true;
  observation.bot.inRescueZone = true;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::MoveToNode, "rescued hostage falls back to normal goal navigation");
}

AI_TEST(testGoalNavigationPolicyMapsRetreatFromActiveSeekCover) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::SeekCover;
  observation.bot.health = 50.0f;
  observation.personality.aggression = 0.5f;
  observation.combat.perceptionFlags |= static_cast<uint32_t>(ai::PerceptionFlag::SeeingEnemy);

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::Retreat, "visible enemy and low combat approach map seek cover to retreat");
}

AI_TEST(testGoalNavigationPolicyKeepsSeekCoverForNonRetreatState) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::SeekCover;
  observation.bot.health = 50.0f;
  observation.personality.aggression = 0.8f;
  observation.combat.perceptionFlags |= static_cast<uint32_t>(ai::PerceptionFlag::SeeingEnemy);

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::SeekCover, "higher combat approach keeps generic seek cover intent");
}

AI_TEST(testGoalNavigationPolicyKeepsSeekCoverWithoutVisibleEnemy) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::SeekCover;
  observation.bot.health = 10.0f;
  observation.personality.aggression = 0.5f;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::SeekCover, "seek cover without a visible enemy remains generic cover intent");
}

AI_TEST(testGoalNavigationPolicyYieldsBlindTaskToLegacyExecution) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Blind;
  observation.bot.currentGoalNode = 20;
  observation.bot.currentNode = 10;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::None,
         "blind task yields instead of starting generic navigation that cannot own the legacy task");
}

AI_TEST(testGoalNavigationPolicyMapsTaskActions) {
  auto observation = makeObservation();
  ai::GoalNavigationPolicy policy {};

  observation.bot.currentTask = ai::TaskType::PickupItem;
  expect(policy.decide(observation).type == ai::ActionType::PickupItem, "pickup task maps to pickup action");

  observation.bot.currentTask = ai::TaskType::Camp;
  expect(policy.decide(observation).type == ai::ActionType::Camp, "camp task maps to camp action");

  observation.bot.currentTask = ai::TaskType::PlantBomb;
  expect(policy.decide(observation).type == ai::ActionType::PlantBomb, "plant task maps to plant action");

  observation.bot.currentTask = ai::TaskType::DefuseBomb;
  expect(policy.decide(observation).type == ai::ActionType::DefuseBomb, "defuse task maps to defuse action");

  observation.bot.currentTask = ai::TaskType::SeekCover;
  expect(policy.decide(observation).type == ai::ActionType::SeekCover, "cover task maps to cover action");

  observation.bot.currentTask = ai::TaskType::EscapeFromBomb;
  expect(policy.decide(observation).type == ai::ActionType::EscapeFromBomb, "escape task maps to escape action");

  observation.bot.currentTask = ai::TaskType::ShootBreakable;
  expect(policy.decide(observation).type == ai::ActionType::Fire, "breakable task maps to fire action");

  observation.bot.currentTask = ai::TaskType::Pause;
  observation.bot.taskTimeRemaining = 2.0f;
  expect(policy.decide(observation).type == ai::ActionType::Wait, "short pause maps to wait");

  observation.bot.taskTimeRemaining = 30.0f;
  expect(policy.decide(observation).type == ai::ActionType::HoldPosition, "long pause maps to hold position");

  observation.bot.currentTask = ai::TaskType::Hide;
  expect(policy.decide(observation).type == ai::ActionType::Hide, "hide task maps to hide action");
}

AI_TEST(testGoalNavigationPolicyDefinesOutcomeForEveryTaskType) {
  constexpr size_t kTaskTypeCount = static_cast<size_t>(ai::TaskType::Spraypaint) + 1;
  constexpr ai::ActionType expected[kTaskTypeCount] = {
    ai::ActionType::MoveToNode,
    ai::ActionType::Explore,
    ai::ActionType::None,
    ai::ActionType::None,
    ai::ActionType::FollowPlayer,
    ai::ActionType::PickupItem,
    ai::ActionType::Camp,
    ai::ActionType::PlantBomb,
    ai::ActionType::DefuseBomb,
    ai::ActionType::MoveToNode,
    ai::ActionType::None,
    ai::ActionType::SeekCover,
    ai::ActionType::ThrowGrenade,
    ai::ActionType::ThrowFlashbang,
    ai::ActionType::ThrowSmoke,
    ai::ActionType::MoveToNode,
    ai::ActionType::EscapeFromBomb,
    ai::ActionType::Fire,
    ai::ActionType::Hide,
    ai::ActionType::None,
    ai::ActionType::MoveToNode,
  };

  ai::GoalNavigationPolicy policy {};
  auto observation = makeObservation();

  for (size_t index = 0; index < kTaskTypeCount; ++index) {
    if (index == static_cast<size_t>(ai::TaskType::Pause)) {
      continue;
    }

    observation.bot.currentTask = static_cast<ai::TaskType>(index);
    if (observation.bot.currentTask == ai::TaskType::FollowUser) {
      addFollowTarget(observation, 7);
    }
    if (observation.bot.currentTask == ai::TaskType::ThrowExplosive || observation.bot.currentTask == ai::TaskType::ThrowFlashbang) {
      observation.bot.throwTarget = { 100.0f, 200.0f, 300.0f };
    }
    const auto action = policy.decide(observation);

    expect(action.type == expected[index], "teacher defines a deterministic outcome for every non-Pause task");
  }
}

AI_TEST(testGoalNavigationPolicyYieldsHuntWhenLastEnemyIsUnavailable) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Hunt;
  ai::GoalNavigationPolicy policy {};

  const auto action = policy.decide(observation);

  expect(action.type == ai::ActionType::None, "hunt without last enemy yields instead of adopting the navigation goal");
}

AI_TEST(testGoalNavigationPolicyFallsBackWhenCombatTargetIsUnavailable) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Attack;
  ai::GoalNavigationPolicy policy {};

  const auto action = policy.decide(observation);

  expect(action.type == ai::ActionType::MoveToNode, "attack without observed target falls back to goal");
  expect(action.targetNode == 20, "fallback preserves navigation goal");
}

AI_TEST(testGoalNavigationPolicyStopsForDeadBot) {
  auto observation = makeObservation();
  observation.bot.alive = false;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::None, "dead bot produces no teacher action");
}

AI_TEST(testGoalNavigationPolicyStopsAtCurrentFallbackGoal) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Unknown;
  observation.bot.currentGoalNode = observation.bot.currentNode;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::None, "reached fallback goal produces no teacher action");
}

AI_TEST(testGoalNavigationPolicyExploresAtCurrentLegacyGoal) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Normal;
  observation.bot.currentGoalNode = observation.bot.currentNode;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::Explore, "normal task explores independently of the legacy goal");
}

AI_TEST(testObjectiveApproachUsesGraphUntilInteractionRange) {
  constexpr float interactionDistanceSq = 80.0f * 80.0f;

  expect(ai::shouldUseGraphObjectiveApproach(90.0f * 90.0f, interactionDistanceSq, true, false),
         "objective outside interaction range uses graph navigation");
  expect(!ai::shouldUseGraphObjectiveApproach(79.0f * 79.0f, interactionDistanceSq, true, false),
         "objective inside interaction range uses the direct interaction approach");
  expect(!ai::shouldUseGraphObjectiveApproach(90.0f * 90.0f, interactionDistanceSq, false, false),
         "missing objective node falls back to the direct approach");
  expect(!ai::shouldUseGraphObjectiveApproach(90.0f * 90.0f, interactionDistanceSq, true, true),
         "reaching the objective node hands off to the final direct interaction approach");

  expect(ai::shouldFinishObjectiveApproachDirectly(
             80.93f * 80.93f, interactionDistanceSq, true, true),
         "reached interaction-safe node finishes the small remaining distance directly");
  expect(!ai::shouldFinishObjectiveApproachDirectly(
             79.0f * 79.0f, interactionDistanceSq, true, true),
         "direct finishing stops after entering the interaction sphere");
  expect(!ai::shouldFinishObjectiveApproachDirectly(
             90.0f * 90.0f, interactionDistanceSq, true, false),
         "interaction-safe node must be reached before the direct finishing stage");
  expect(!ai::shouldFinishObjectiveApproachDirectly(
             90.0f * 90.0f, interactionDistanceSq, false, true),
         "a non-interaction node does not acquire the safe direct finishing path");

  expect(ai::shouldFinishObjectiveApproachViaInteractionNode(
             90.0f * 90.0f, interactionDistanceSq, true, true, false),
         "blocked direct finish falls back to the interaction-safe node center");
  expect(!ai::shouldFinishObjectiveApproachViaInteractionNode(
             90.0f * 90.0f, interactionDistanceSq, true, true, true),
         "clear direct finish keeps the D151 short approach");
  expect(!ai::shouldFinishObjectiveApproachViaInteractionNode(
             79.0f * 79.0f, interactionDistanceSq, true, true, false),
         "node-center fallback stops once the bot is already inside interaction range");
  expect(!ai::shouldFinishObjectiveApproachViaInteractionNode(
             90.0f * 90.0f, interactionDistanceSq, true, false, false),
         "node-center fallback requires the interaction-safe node to be reached first");
}

AI_TEST(testPlantedBombPickupScannerYieldsToActiveDefuse) {
  expect(ai::preservesPlantedBombPickupDuringDefuse(ai::TaskType::DefuseBomb, ai::TaskType::DefuseBomb),
         "active defuse preserves the handed-off planted-C4 pickup binding");
  expect(!ai::preservesPlantedBombPickupDuringDefuse(ai::TaskType::PickupItem, ai::TaskType::DefuseBomb),
         "pickup approach remains eligible for ordinary pickup scanning");
}

AI_TEST(testObjectivePickupKeepsOwnershipThroughNavigationPause) {
  expect(ai::ownsObjectivePickupTask(
             ai::TaskType::PickupItem, ai::TaskType::PickupItem, ai::TaskType::Pause, false),
         "ordinary pickup owns its pickup task");
  expect(ai::ownsObjectivePickupTask(
             ai::TaskType::Pause, ai::TaskType::PickupItem, ai::TaskType::Pause, true),
         "active objective pickup survives the temporary navigation pause");
  expect(!ai::ownsObjectivePickupTask(
             ai::TaskType::Pause, ai::TaskType::PickupItem, ai::TaskType::Pause, false),
         "ordinary pickup does not absorb an unrelated pause");
}

AI_TEST(testBombSearchGoalSelectionUsesVisitedStateAndOwnDistance) {
  expect(ai::isBombSearchGoalEligible(false, false),
         "unchecked bombsite remains eligible during the first search pass");
  expect(!ai::isBombSearchGoalEligible(true, false),
         "visited bombsite is skipped while unchecked sites remain");
  expect(ai::isBombSearchGoalEligible(true, true),
         "visited bombsite becomes eligible only for deterministic re-check");

  expect(ai::isBetterBombSearchGoal(500.0f, 20, 700.0f, 10),
         "nearer bombsite path is preferred");
  expect(!ai::isBetterBombSearchGoal(900.0f, 5, 700.0f, 10),
         "farther bombsite path is not preferred");
  expect(ai::isBetterBombSearchGoal(700.0f, 5, 700.0f, 10),
         "equal-distance bombsites use node index as deterministic tie-break");
}

AI_TEST(testBombCarrierBlocksSidePickupsWhileObjectiveIsEnabled) {
  expect(ai::blocksBombCarrierSidePickups(true, true),
         "active C4 carrier does not divert to optional pickups");
  expect(!ai::blocksBombCarrierSidePickups(false, true),
         "ordinary Terrorist remains eligible for pickups");
  expect(!ai::blocksBombCarrierSidePickups(true, false),
         "ignored objectives do not impose bomb-carrier pickup ownership");
}

AI_TEST(testPlantedBombApproachPreservesFirearmUnderThreat) {
  expect(ai::preservesFirearmForPlantedBombApproach(true, true, true, true, true),
         "CT keeps a firearm ready while approaching a planted C4 with living enemies");
  expect(!ai::preservesFirearmForPlantedBombApproach(false, true, true, true, true),
         "Terrorist traversal keeps the normal jump knife optimization");
  expect(!ai::preservesFirearmForPlantedBombApproach(true, false, true, true, true),
         "non-demolition maps keep the normal jump knife optimization");
  expect(!ai::preservesFirearmForPlantedBombApproach(true, true, false, true, true),
         "unplanted bomb state keeps the normal jump knife optimization");
  expect(!ai::preservesFirearmForPlantedBombApproach(true, true, true, false, true),
         "CT may use the jump knife optimization after all enemies are gone");
  expect(!ai::preservesFirearmForPlantedBombApproach(true, true, true, true, false),
         "ignored objectives keep the legacy traversal optimization");
}

AI_TEST(testDroppedBombDefenderEligibilityPreservesCombatPriority) {
  expect(ai::isDroppedBombDefenderEligible(true, true, true, false, false, false),
         "alive CT that sees dropped C4 is eligible to guard it");
  expect(!ai::isDroppedBombDefenderEligible(false, true, true, false, false, false),
         "dead CT cannot own dropped-C4 defense");
  expect(!ai::isDroppedBombDefenderEligible(true, false, true, false, false, false),
         "Terrorist cannot own CT dropped-C4 defense");
  expect(!ai::isDroppedBombDefenderEligible(true, true, false, false, false, false),
         "CT must actually perceive the dropped C4 before being selected");
  expect(!ai::isDroppedBombDefenderEligible(true, true, true, true, false, false),
         "visible combat outranks dropped-C4 defense assignment");
  expect(!ai::isDroppedBombDefenderEligible(true, true, true, false, true, false),
         "ladder traversal is not interrupted for dropped-C4 defense");
  expect(!ai::isDroppedBombDefenderEligible(true, true, true, false, false, true),
         "bomb escape traversal is not interrupted for dropped-C4 defense");
}

AI_TEST(testDroppedBombDefenderSelectionPrefersNearestThenIndex) {
  expect(ai::isBetterDroppedBombDefender(100.0f, 8, 200.0f, 4),
         "nearer eligible CT wins dropped-C4 defense");
  expect(!ai::isBetterDroppedBombDefender(300.0f, 2, 200.0f, 4),
         "farther eligible CT does not replace the defender candidate");
  expect(ai::isBetterDroppedBombDefender(200.0f, 2, 200.0f, 4),
         "equal-distance defender selection uses bot index deterministically");
  expect(!ai::isBetterDroppedBombDefender(200.0f, 6, 200.0f, 4),
         "higher bot index loses an equal-distance defender tie");
}

AI_TEST(testObjectiveInteractionRangeUsesFullThreeDimensionalDistance) {
  expect(ai::isWithinObjectiveInteractionRange(30.0f, 40.0f, 20.0f, 80.0f),
         "nearby objective node inside the full XYZ radius is interaction-safe");
  expect(!ai::isWithinObjectiveInteractionRange(10.0f, 10.0f, 81.0f, 80.0f),
         "node close in XY but too far below or above the objective is rejected");
  expect(!ai::isWithinObjectiveInteractionRange(0.0f, 0.0f, 80.0f, 80.0f),
         "interaction boundary remains strict at exactly 80 vertical units");

  expect(ai::isBetterObjectiveApproachNode(300.0f, 20, 500.0f, 10),
         "shorter graph route wins among interaction-safe nodes");
  expect(ai::isBetterObjectiveApproachNode(500.0f, 5, 500.0f, 10),
         "equal route distance uses node index as deterministic tie-break");
}

AI_TEST(testBombDefensePrefersCampNodesBeforeGenericFallback) {
  expect(ai::isBombDefenseNodeEligibleForPass(true, true, false),
         "camp waypoint is eligible during preferred bomb-defense pass");
  expect(!ai::isBombDefenseNodeEligibleForPass(true, false, false),
         "ordinary waypoint is excluded while a camp-only pass is active");
  expect(ai::isBombDefenseNodeEligibleForPass(false, false, false),
         "ordinary waypoint remains eligible for compatibility fallback");
  expect(!ai::isBombDefenseNodeEligibleForPass(false, true, true),
         "ladder waypoint is excluded from both bomb-defense passes");
}

AI_TEST(testDroppedBombDefenseCoverRankingPrefersLowerExposure) {
  expect(ai::isBetterBombDefenseCover(20, 100, 500.0f, 8, 60, 5, 200.0f, 4),
         "lower world-visibility exposure outranks route length and historical damage");
  expect(ai::isBetterBombDefenseCover(20, 10, 500.0f, 8, 20, 30, 200.0f, 4),
         "lower historical damage breaks equal-exposure ties");
  expect(ai::isBetterBombDefenseCover(20, 10, 150.0f, 8, 20, 10, 300.0f, 4),
         "shorter route breaks equal exposure and damage ties");
  expect(ai::isBetterBombDefenseCover(20, 10, 150.0f, 2, 20, 10, 150.0f, 4),
         "node index deterministically breaks a complete cover-ranking tie");
  expect(!ai::isBetterBombDefenseCover(80, 0, 50.0f, 1, 20, 100, 500.0f, 9),
         "an exposed node cannot win merely because it is closer or historically safer");
}


AI_TEST(testDefuseRouteComparisonPrefersLargerSafetySlack) {
  using Choice = ai::DefuseRouteChoice;
  const auto direct = ai::chooseDefuseRoute(false, 40.0f, 200.0f,
    { true, 400.0f }, { true, 1000.0f }, { true, 1000.0f }, 10, 0);
  expect(direct.choice == Choice::Direct, "larger completion-time slack outranks low-risk detour");
  expectNear(direct.direct.slackSeconds, 23.0f, 0.001f, "direct route includes defuse, approach and safety margin");
  expectNear(direct.viaKit.slackSeconds, 19.0f, 0.001f, "kit route includes both legs and kit pickup time");

  const auto kit = ai::chooseDefuseRoute(false, 40.0f, 200.0f,
    { true, 1000.0f }, { true, 100.0f }, { true, 100.0f }, 0, 10);
  expect(kit.choice == Choice::ViaKit, "kit route wins if it finishes much sooner even at greater risk");
}

AI_TEST(testDefuseRouteComparisonPrefersLowerRiskForSimilarSlack) {
  using Choice = ai::DefuseRouteChoice;
  const auto choice = ai::chooseDefuseRoute(false, 40.0f, 200.0f,
    { true, 400.0f }, { true, 700.0f }, { true, 700.0f }, 20, 2);
  expect(choice.choice == Choice::ViaKit, "lower risk wins when both routes have similar safety margins");
  expect(ai::chooseDefuseRoute(false, 40.0f, 200.0f,
    { true, 400.0f }, { true, 700.0f }, { true, 700.0f }, -1, -1).choice == Choice::Direct,
    "unknown route risks fall back to larger remaining safety margin");
}

AI_TEST(testDefuseRouteComparisonRejectsUnavailableAndInvalidPaths) {
  using Choice = ai::DefuseRouteChoice;
  expect(ai::chooseDefuseRoute(true, 40.0f, 200.0f,
    { true, 400.0f }, { true, 50.0f }, { true, 50.0f }, 0, 0).choice == Choice::Direct,
    "CT with an existing kit always ignores dropped kits");
  expect(ai::chooseDefuseRoute(false, 40.0f, 200.0f,
    { false, 400.0f }, { true, 50.0f }, { true, 50.0f }, 0, 0).choice == Choice::ViaKit,
    "only the reachable kit path can win when direct graph route is unavailable");
  expect(ai::chooseDefuseRoute(false, 40.0f, 200.0f,
    { true, 400.0f }, { true, 32767.0f }, { true, 50.0f }, 0, 0).choice == Choice::Direct,
    "Floyd unreachable sentinel cannot authorize a kit detour");
  expect(ai::chooseDefuseRoute(false, 40.0f, 200.0f,
    { true, 400.0f }, { true, -2.0f }, { true, 50.0f }, 0, 0).choice == Choice::Direct,
    "negative graph distance cannot authorize a kit detour");
  expect(ai::chooseDefuseRoute(false, 40.0f, 0.0f,
    { true, 400.0f }, { true, 50.0f }, { true, 50.0f }, 0, 0).choice == Choice::None,
    "zero travel speed makes both options infeasible");
  expect(ai::chooseDefuseRoute(false, 10.0f, 200.0f,
    { true, 400.0f }, { true, 50.0f }, { true, 50.0f }, 0, 0).choice == Choice::None,
    "neither route is selected if a full safe defuse is impossible");
}

AI_TEST(testDefuseRouteComparisonAbandonsUnviableKitAndPreventsChurn) {
  using Choice = ai::DefuseRouteChoice;
  const auto stable = ai::chooseDefuseRoute(false, 40.0f, 200.0f,
    { true, 400.0f }, { true, 700.0f }, { true, 700.0f }, 20, 2, Choice::Direct);
  expect(stable.choice == Choice::Direct, "small apparent risk advantage does not churn an active direct route");

  const auto abandoned = ai::chooseDefuseRoute(false, 17.5f, 200.0f,
    { true, 400.0f }, { true, 700.0f }, { true, 700.0f }, 20, 2, Choice::ViaKit);
  expect(abandoned.choice == Choice::Direct, "kit route is abandoned as soon as its defuse budget expires");
  expect(!abandoned.viaKit.feasible && abandoned.direct.feasible,
    "the route switch is driven by actual feasibility, not kit proximity");

  expect(ai::chooseDefuseRoute(false, 17.0f, 200.0f,
    { true, 400.0f }, { true, 700.0f }, { true, 700.0f }, 20, 2, Choice::Direct).choice == Choice::None,
    "exact safety margin boundary does not claim guaranteed defuse");
}


AI_TEST(testDefuseKitCandidatePrefersStableCurrentKit) {
  expect(ai::isBetterDefuseKitCandidate(10.0f, false, -1.0f, false),
         "first feasible kit can be selected");
  expect(!ai::isBetterDefuseKitCandidate(10.4f, false, 10.0f, true),
         "small gain cannot steal an already selected kit");
  expect(ai::isBetterDefuseKitCandidate(10.0f, true, 10.4f, false),
         "current kit wins a near-equal comparison");
  expect(ai::isBetterDefuseKitCandidate(12.0f, false, 10.0f, true),
         "sufficient improvement permits switching to another kit");
  expect(!ai::isBetterDefuseKitCandidate(0.0f, true, 10.0f, false),
         "infeasible current kit must not be retained");
  expect(!ai::isBetterDefuseKitCandidate(-1.0f, false, -1.0f, false),
         "invalid kit is not selected when none exists");
}
