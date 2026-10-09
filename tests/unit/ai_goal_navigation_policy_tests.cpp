//
// AiPB - task-aware teacher policy unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_attack_movement_guard.h>
#include <ai/ai_bomb_carrier_goal_guard.h>
#include <ai/ai_bomb_defense_guard.h>
#include <ai/ai_bomb_search_guard.h>
#include <ai/ai_ct_defuse_path_guard.h>
#include <ai/ai_defuse_event.h>
#include <ai/ai_goal_navigation_policy.h>
#include <ai/ai_navigation_task_guard.h>
#include <ai/ai_objective_navigation_guard.h>
#include <ai/ai_perception_guard.h>
#include <ai/ai_semiclip_navigation_guard.h>

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

AI_TEST(testFlashBlindBlocksPreciseAimIndependentOfTask) {
  expect(ai::suppressPreciseBlindAim(0.01f),
         "active flash always suppresses precise aiming");
  expect(ai::suppressPreciseBlindAim(3.0f),
         "strong flash suppresses precision throughout the timer");
  expect(!ai::suppressPreciseBlindAim(0.0f),
         "precision may recover at the exact end of blindness");
  expect(!ai::suppressPreciseBlindAim(-0.01f),
         "expired flash must not suppress ordinary aiming");

  // Blind task ownership is deliberately irrelevant to the global guard:
  // this also covers Attack, AimAtTarget, Pause, and other active actions.
  const ai::TaskType tasks[] = {
    ai::TaskType::Blind, ai::TaskType::Attack, ai::TaskType::Pause,
    ai::TaskType::Normal, ai::TaskType::Camp
  };
  for (const auto task : tasks) {
    (void) task;
    expect(ai::suppressPreciseBlindAim(2.0f),
           "flash aim suppression is not conditional on legacy task");
  }
}

AI_TEST(testVisibleEnemyTargetHysteresisPreventsEqualDistanceOscillation) {
  // Squared distances match lookupEnemies() and preserve its visibility range.
  const float current = 400.0f * 400.0f;
  expect(ai::shouldKeepCurrentVisibleEnemy(true, false, false, current, 395.0f * 395.0f),
         "small distance difference preserves the currently visible target");
  expect(ai::shouldKeepCurrentVisibleEnemy(true, false, false, current, 300.0f * 300.0f),
         "exactly 25 percent closer is not enough to force a switch");
  expect(!ai::shouldKeepCurrentVisibleEnemy(true, false, false, current, 295.0f * 295.0f),
         "a substantially closer challenger can replace the current enemy");

  // Once switched, near-equal oscillations must not immediately switch back.
  const float newCurrent = 295.0f * 295.0f;
  expect(ai::shouldKeepCurrentVisibleEnemy(true, false, false, newCurrent, current),
         "a formerly selected target cannot win back focus while farther away");
}

AI_TEST(testVisibleEnemyTargetHysteresisAllowsLossAndPriorityTargets) {
  const float current = 400.0f * 400.0f;
  expect(!ai::shouldKeepCurrentVisibleEnemy(false, false, false, current, 398.0f * 398.0f),
         "lost or hidden current enemy never receives a sticky target bonus");
  expect(!ai::shouldKeepCurrentVisibleEnemy(true, false, true, current, 399.0f * 399.0f),
         "a newly visible VIP bypasses hysteresis on assassination maps");
  expect(ai::shouldKeepCurrentVisibleEnemy(true, true, false, current, 200.0f * 200.0f),
         "a visible VIP retains priority over a non-VIP challenger");
  expect(!ai::shouldKeepCurrentVisibleEnemy(true, false, false, -1.0f, current),
         "invalid current distance cannot lock a target");
  expect(!ai::shouldKeepCurrentVisibleEnemy(true, false, false, current, -1.0f),
         "invalid candidate distance cannot lock a target");
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

AI_TEST(testD187_1LowCeilingUsesPhysicalDuckClearance) {
  expect(ai::shouldDuckForLowCeiling(true, true, false, true, true),
         "head-level obstacle with a clear duck passage requires crouching");
  expect(!ai::shouldDuckForLowCeiling(true, true, false, false, true),
         "open standing passage does not force crouching");
  expect(!ai::shouldDuckForLowCeiling(true, true, false, true, false),
         "solid wall must not be mistaken for a crouch tunnel");
  expect(!ai::shouldDuckForLowCeiling(false, true, false, true, true),
         "stationary guard does not enter a crouch loop");
  expect(!ai::shouldDuckForLowCeiling(true, false, false, true, true),
         "airborne bots retain jumping movement");
  expect(!ai::shouldDuckForLowCeiling(true, true, true, true, true),
         "ladder logic retains movement authority");
}

AI_TEST(testCtBombDefuserElectionAndTakeover) {
  expect(ai::isCtBombDefuserCandidate(true, true, false, false, false, true),
         "reachable localized CT is eligible");
  expect(!ai::isCtBombDefuserCandidate(true, false, false, false, false, true),
         "unlocalized CT cannot use shared hidden C4 location");
  expect(!ai::isCtBombDefuserCandidate(false, true, false, false, false, true),
         "dead CT cannot own defuse");
  expect(!ai::isCtBombDefuserCandidate(true, true, true, false, false, true),
         "escaping CT cannot own defuse");
  expect(!ai::isCtBombDefuserCandidate(true, true, false, true, false, true),
         "CT fighting a visible enemy does not take over");
  expect(ai::isCtBombDefuserCandidate(true, true, false, true, true, false),
         "active defuser is not displaced by enemy or missing waypoint");
  const float kit = ai::ctBombDefuserEstimatedCompletion(600.0f, 250.0f, true, false, false);
  const float plain = ai::ctBombDefuserEstimatedCompletion(600.0f, 250.0f, false, false, false);
  expect(kit < plain, "kit beats equally distant no-kit candidate");
  expect(ai::isBetterCtBombDefuser(kit, 2, plain, 1), "faster defuser wins");
  expect(ai::isBetterCtBombDefuser(kit, 1, kit, 2), "equal ETA ties by stable bot id");
  expect(!ai::isBetterCtBombDefuser(kit, 3, kit, 2), "no equal-score oscillation");
  expect(ai::ctBombDefuserEstimatedCompletion(0.0f, 250.0f, false, false, true) < kit,
         "active defuse outranks candidate travel estimate");
  expect(ai::isBetterCtBombDefuser(12.0f, 8, 15.0f, 2),
         "surviving replacement can be elected when original is gone");
}

AI_TEST(testCtBombCoverReservationAndOwnerLifecycle) {
  expect(ai::conflictsWithCtBombCoverGoal(true, 100.0f * 100.0f),
         "nearby reserved cover location is rejected");
  expect(!ai::conflictsWithCtBombCoverGoal(true, 200.0f * 200.0f),
         "separate cover flank remains valid");
  expect(!ai::conflictsWithCtBombCoverGoal(false, 0.0f),
         "unreserved node does not block cover");
  expect(ai::shouldHoldCtBombCover(true, true, false),
         "active teammate keeps supporting cover role");
  expect(ai::shouldHoldCtBombCover(true, false, true),
         "confirmed ongoing defuse keeps cover role");
  expect(!ai::shouldHoldCtBombCover(true, false, false),
         "dead or absent defuser releases role for takeover");
  expect(!ai::shouldHoldCtBombCover(false, true, true),
         "end of bomb objective releases cover");
}

AI_TEST(testBlockedC4InsideUseRadiusStillNeedsGraphStaging) {
  expect(ai::needsPlantedBombInteractionRoute(35.0f * 35.0f, 60.0f * 60.0f, false),
         "box blocks direct C4 approach even inside the 3D use sphere");
  expect(!ai::needsPlantedBombInteractionRoute(35.0f * 35.0f, 60.0f * 60.0f, true),
         "clear local approach needs no detour");
  expect(ai::needsPlantedBombInteractionRoute(90.0f * 90.0f, 60.0f * 60.0f, true),
         "distant C4 still requires graph navigation");
  expect(ai::shouldFinishObjectiveApproachViaInteractionNode(
         35.0f * 35.0f, 60.0f * 60.0f, true, true, false),
         "blocked local finish uses reachable in-range waypoint center");
}

AI_TEST(testC4InteractionNodeRequiresPhysicalReachabilityWhenRouteIsZero) {
  expect(ai::hasUsablePlantedBombInteractionRoute(true, true, 160.0f, 32767.0f, false, false),
         "graph can climb to elevated approach waypoint");
  expect(!ai::hasUsablePlantedBombInteractionRoute(true, true, 0.0f, 32767.0f, true, false),
         "zero-length graph route cannot cross the side of a box");
  expect(ai::hasUsablePlantedBombInteractionRoute(true, true, 0.0f, 32767.0f, true, true),
         "zero-length path is valid when waypoint center is physically accessible");
  expect(!ai::hasUsablePlantedBombInteractionRoute(true, true, 32767.0f, 32767.0f, false, true),
         "unreachable graph route is rejected");
}

AI_TEST(testDefuseApproachDiagnosticThrottle) {
  ai::DefuseApproachDiagnosticGate gate {};
  expect(gate.allow(10.0f, false), "first blocked approach reported");
  expect(!gate.allow(11.0f, false), "rapid repeats suppressed");
  expect(gate.allow(11.0f, true), "separate no-route result reported");
  expect(!gate.allow(12.0f, true), "rapid no-route repeats suppressed");
  expect(gate.allow(15.0f, false) && gate.allow(16.0f, true),
         "long-running failures remain observable");
  gate.reset();
  expect(gate.allow(0.0f, false), "new round resets diagnostic gate");
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
  expect(ai::shouldFinishObjectiveApproachViaInteractionNode(
             79.0f * 79.0f, interactionDistanceSq, true, true, false),
         "blocked local approach still needs node-center fallback inside interaction range");
  expect(!ai::shouldFinishObjectiveApproachViaInteractionNode(
             90.0f * 90.0f, interactionDistanceSq, true, false, false),
         "node-center fallback requires the interaction-safe node to be reached first");
}

AI_TEST(testPlantedBombDefuseRequiresInteractionProximityAndClearApproach) {
  expect(ai::canBeginPlantedBombUse(59.0f * 59.0f, 60.0f, true),
         "CT starts USE inside the conservative interaction sphere");
  expect(!ai::canBeginPlantedBombUse(79.0f * 79.0f, 60.0f, true),
         "old 80-unit pickup range must not prematurely stop movement");
  expect(!ai::canBeginPlantedBombUse(25.0f * 25.0f, 60.0f, false),
         "C4 behind a crate or wall is not ready solely because it is nearby");
  expect(!ai::canBeginPlantedBombUse(60.0f * 60.0f, 60.0f, true),
         "edge of use sphere is not assumed safe");
  expect(ai::canBeginPlantedBombUse(40.0f * 40.0f,
         ai::plantedBombDefuseApproachRadius(true), true),
         "recovery retries USE after taking a tighter approach");
  expect(!ai::canBeginPlantedBombUse(55.0f * 55.0f,
         ai::plantedBombDefuseApproachRadius(true), true),
         "same distant position is not immediately retried after a failed USE");
}

AI_TEST(testUnconfirmedBombDefuseRecoveryNeverInterruptsBarTime) {
  expect(!ai::shouldChangeUnconfirmedDefuseStance(1.0f, false, false),
         "early USE attempt retains stance");
  expect(ai::shouldChangeUnconfirmedDefuseStance(1.3f, false, false),
         "missing progress switches stance once");
  expect(!ai::shouldChangeUnconfirmedDefuseStance(1.9f, false, true),
         "stance cannot toggle every frame");
  expect(!ai::shouldChangeUnconfirmedDefuseStance(2.0f, true, false),
         "confirmed BarTime prevents stance changes");
  expect(ai::shouldReapproachUnconfirmedDefuse(3.1f, false),
         "failed USE eventually returns to C4 approach instead of freezing");
  expect(!ai::shouldReapproachUnconfirmedDefuse(6.0f, true),
         "active defuse must never be interrupted by the retry watchdog");
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

AI_TEST(testD186_3MobileGuardDoesNotRequireCamp) {
  expect(ai::shouldUseMobileDroppedBombGuard(true, true),
         "guard route requires eligible reachable target");
  expect(!ai::shouldUseMobileDroppedBombGuard(true, false),
         "missing route must not create a stationary guard");
  expect(!ai::shouldUseMobileDroppedBombGuard(false, true),
         "ineligible CT must not be forced to hold");
}

AI_TEST(testD186DroppedBombGuardCoverAndOwnership) {
  expect(ai::isSafeDroppedBombGuardNode(true, false, true, true, false,
         220.0f * 220.0f, 400.0f, false), "reachable and visible safe guard waypoint");
  expect(!ai::isSafeDroppedBombGuardNode(true, false, true, true, false,
         60.0f * 60.0f, 400.0f, false), "keep CT away from pickup interaction area");
  expect(!ai::isSafeDroppedBombGuardNode(true, false, true, false, false,
         220.0f * 220.0f, 400.0f, false), "blocked geometry rejects waypoint");
  expect(!ai::isSafeDroppedBombGuardNode(true, false, true, true, false,
         220.0f * 220.0f, 32767.0f, false), "disconnected path never becomes cover");
  expect(!ai::isSafeDroppedBombGuardNode(true, false, true, true, false,
         220.0f * 220.0f, 400.0f, true), "teammate reserve prevents overlap");
  expect(ai::droppedBombGuardCoverCost(300.0f, 12, 3, true, 2, 2)
         < ai::droppedBombGuardCoverCost(300.0f, 60, 3, false, 2, 2),
         "low exposure and camp preference improve rank");
  expect(ai::isBetterDroppedBombGuardCover(60.0f, 2, 60.0f, 4),
         "stable waypoint id breaks equal scores");
  expect(ai::isDroppedBombGuardOwnerActive(true, true, true), "active CT retains dropped C4");
  expect(!ai::isDroppedBombGuardOwnerActive(false, true, true), "death releases reservation");
  expect(!ai::isDroppedBombGuardOwnerActive(true, false, true), "picked up bomb releases reservation");
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


AI_TEST(testPlantedBombPickupAllowsReachableGraphApproach) {
  expect(ai::isReachablePlantedBombGraphApproach(true, false, -1.0f, 32767.0f),
         "short unobstructed bomb approach remains allowed without a graph route");
  expect(ai::isReachablePlantedBombGraphApproach(false, true, 420.0f, 32767.0f),
         "blocked direct approach is accepted when a valid interaction node has a graph route");
  expect(ai::isReachablePlantedBombGraphApproach(false, true, 0.0f, 32767.0f),
         "bot already at an interaction-safe node remains eligible");
  expect(!ai::isReachablePlantedBombGraphApproach(false, false, 100.0f, 32767.0f),
         "a nearby waypoint outside the interaction sphere cannot rescue eligibility");
  expect(!ai::isReachablePlantedBombGraphApproach(false, true, 32767.0f, 32767.0f),
         "unreachable Floyd sentinel rejects graph approach");
  expect(!ai::isReachablePlantedBombGraphApproach(false, true, -1.0f, 32767.0f),
         "invalid negative graph distance does not rescue blocked direct approach");
}


AI_TEST(testHearingExpiryDoesNotDependOnScanThrottle) {
  expect(!ai::hasExpiredEnemyHearing(20.0f, 15.0f),
         "recent heard contact remains valid");
  expect(!ai::hasExpiredEnemyHearing(25.0f, 15.0f),
         "hearing expires only after its full ten second window");
  expect(ai::hasExpiredEnemyHearing(25.01f, 15.0f),
         "stale hearing expires even if a fresh sound scan was just performed");
  expect(!ai::hasExpiredEnemyHearing(25.01f, 25.0f),
         "new sound immediately refreshes hearing expiration");
}

AI_TEST(testAttackMoveOnlyAdvancesOnDistantVisibleThreat) {
  expect(ai::shouldAdvanceWhileAttacking(40, true, false, false, false, false, false, 700.0f * 700.0f),
         "medium aggression can advance while engaging a distant visible enemy");
  expect(!ai::shouldAdvanceWhileAttacking(40, false, false, false, false, false, false, 700.0f * 700.0f),
         "suspected but unseen targets do not authorize an advance");
  expect(!ai::shouldAdvanceWhileAttacking(40, true, false, false, false, false, false, 200.0f * 200.0f),
         "near enemies retain legacy stop-or-strafe combat movement");
  expect(!ai::shouldAdvanceWhileAttacking(20, true, false, false, false, false, false, 700.0f * 700.0f),
         "low-aggression cover and retreat branch remains unchanged");
  expect(!ai::shouldAdvanceWhileAttacking(40, true, true, false, false, false, false, 700.0f * 700.0f),
         "reload remains a reason not to advance");
  expect(!ai::shouldAdvanceWhileAttacking(40, true, false, true, false, false, false, 700.0f * 700.0f),
         "sniper weapon preserves stand-off movement");
  expect(!ai::shouldAdvanceWhileAttacking(40, true, false, false, true, false, false, 700.0f * 700.0f),
         "VIP must not advance through distant combat");
  expect(!ai::shouldAdvanceWhileAttacking(40, true, false, false, false, true, false, 700.0f * 700.0f),
         "ducking remains stationary");
  expect(!ai::shouldAdvanceWhileAttacking(40, true, false, false, false, false, true, 700.0f * 700.0f),
         "narrow passages preserve legacy combat movement");
}


AI_TEST(testAudibleBombCanRetargetActiveCtBombsiteSearch) {
  expect(ai::canRetargetCtToAudibleBomb(true, true, true, true),
         "a reachable heard C4 authorizes immediate CT bombsite retargeting");
  expect(!ai::canRetargetCtToAudibleBomb(true, true, false, true),
         "planted-bomb coordinates cannot redirect CT without audible evidence");
  expect(!ai::canRetargetCtToAudibleBomb(true, true, true, false),
         "unreachable bomb-side graph node cannot replace the active CT route");
  expect(!ai::canRetargetCtToAudibleBomb(false, true, true, true),
         "CT does not retarget without a planted C4");
  expect(!ai::canRetargetCtToAudibleBomb(true, false, true, true),
         "explicitly disabled objectives preserve current navigation");
  expect(ai::shouldChangeAudibleBombGoal(12, 16),
         "a different heard-bomb node invalidates the old bombplace route");
  expect(!ai::shouldChangeAudibleBombGoal(16, 16),
         "matching goal must not repeatedly reset CT navigation");
}


AI_TEST(testBombsiteHearingRadiusMatchesLiveThresholds) {
  expectNear(ai::bombAudibleRadiusAtPercent(0.0f), 768.0f, 0.001f,
             "first planted C4 ticks have legacy hearing radius");
  expectNear(ai::bombAudibleRadiusAtPercent(28.0f), 768.0f, 0.001f,
             "28 percent threshold is strict");
  expectNear(ai::bombAudibleRadiusAtPercent(28.1f), 1024.0f, 0.001f,
             "hearing radius expands after 28 percent");
  expectNear(ai::bombAudibleRadiusAtPercent(52.1f), 1280.0f, 0.001f,
             "hearing radius expands after 52 percent");
  expectNear(ai::bombAudibleRadiusAtPercent(68.1f), 2048.0f, 0.001f,
             "hearing radius expands after 68 percent");
  expectNear(ai::bombAudibleRadiusAtPercent(85.1f), 4096.0f, 0.001f,
             "final C4 ticks travel farther");
}

AI_TEST(testSilentBombsiteRequiresCompleteAudibleBrushBounds) {
  expect(ai::isGoalInsideBombTargetVolume(64.0f, 64.0f, 0.0f, 0.0f, 128.0f, 128.0f),
         "goal waypoint in known bomb brush may be evaluated");
  expect(ai::isBombTargetNearGoal(64.0f, 64.0f, 0.0f, 0.0f, 128.0f, 128.0f),
         "known bomb target brush is associated with search goal");
  expect(ai::isEntireBombTargetAudible(200.0f, 200.0f,
            0.0f, 0.0f, 128.0f, 128.0f, 768.0f),
         "near CT can rule out a fully covered silent bombsite");
  expect(!ai::isEntireBombTargetAudible(710.0f, 0.0f,
            -100.0f, -100.0f, 100.0f, 100.0f, 768.0f),
         "hearing the brush center alone is insufficient when far corners remain out of range");
  expect(!ai::isEntireBombTargetAudible(0.0f, 0.0f,
            -500.0f, -500.0f, 500.0f, 500.0f, 768.0f),
         "large planting zone cannot be excluded by silence near its center");
  expect(!ai::isEntireBombTargetAudible(0.0f, 0.0f,
            100.0f, 100.0f, 0.0f, 0.0f, 768.0f),
         "invalid engine bounds cannot be used as negative evidence");
  expect(!ai::isEntireBombTargetAudible(0.0f, 0.0f,
            0.0f, 0.0f, 128.0f, 128.0f, 0.0f),
         "unknown hearing radius does not justify excluding a site");
  expect(!ai::isGoalInsideBombTargetVolume(900.0f, 900.0f,
            0.0f, 0.0f, 128.0f, 128.0f),
         "unrelated bomb goal must not be excluded");
  expect(ai::isBombTargetNearGoal(400.0f, 0.0f,
            0.0f, 0.0f, 128.0f, 128.0f),
         "all bomb brushes near the candidate goal must be checked");
}


AI_TEST(testD185PreplantDefenseUsesCommittedCarrierGoalOnly) {
  expect(!ai::isCommittedPreplantBombsite(true, false, true, 100.0f, true),
         "an ally without C4 cannot reveal the planting goal");
  expect(!ai::isCommittedPreplantBombsite(false, true, true, 100.0f, true),
         "dead carrier cannot assign defense");
  expect(!ai::isCommittedPreplantBombsite(true, true, false, 100.0f, true),
         "missing goal is not assumed from hidden bomb origin");
  expect(!ai::isCommittedPreplantBombsite(true, true, true, 900.0f * 900.0f, false),
         "distant undecided bombsite does not preempt normal navigation");
  expect(ai::isCommittedPreplantBombsite(true, true, true, 320.0f * 320.0f, false),
         "nearby committed carrier allows advance guard placement");
  expect(ai::isCommittedPreplantBombsite(true, true, true, 900.0f * 900.0f, true),
         "carrier inside planted zone can call for site support");
}

AI_TEST(testD185_1PreplantStagingReleasesTaskOnce) {
  expect(ai::shouldFinishPreplantStage(true, true),
         "completed pre-plant route releases its reserved waypoint");
  expect(!ai::shouldFinishPreplantStage(true, false),
         "active movement still owns its waypoint");
  expect(!ai::shouldFinishPreplantStage(false, true),
         "unowned staging must not alter ordinary navigation");
  expect(!ai::shouldAssignNewPreplantStage(896, 896),
         "same committed bombsite is staged only once, never camped repeatedly");
  expect(ai::shouldAssignNewPreplantStage(888, 896),
         "new carrier site allows another early flank");
  expect(!ai::shouldAssignNewPreplantStage(-1, 896),
         "lost carrier intent does not assign a hidden bombsite");
}

AI_TEST(testD185PreplantDefensePreservesPlantCombatAndValidWaypoints) {
  expect(ai::canStagePreplantBombDefense(true, true, true, false, true,
         false, false, false, false, false), "ordinary T teammate can stage");
  expect(!ai::canStagePreplantBombDefense(true, true, true, false, true,
         true, false, false, false, false), "bomb carrier must keep planting");
  expect(!ai::canStagePreplantBombDefense(true, true, true, true, true,
         false, false, false, false, false), "planted bomb transfers to ProtectObjective");
  expect(!ai::canStagePreplantBombDefense(true, true, true, false, true,
         false, false, true, false, false), "visible enemy takes priority");
  expect(!ai::canStagePreplantBombDefense(true, true, true, false, false,
         false, false, false, false, false), "disabled objectives prevent staging");
  expect(ai::isUsablePreplantBombDefenseNode(true, false, true, 200.0f * 200.0f, 800.0f),
         "safe reachable flank is eligible");
  expect(!ai::isUsablePreplantBombDefenseNode(true, false, true, 40.0f * 40.0f, 800.0f),
         "planting interaction area is not crowded");
  expect(!ai::isUsablePreplantBombDefenseNode(true, false, true, 200.0f * 200.0f, 2000.0f),
         "distant route is not a valid early staging route");
  expect(!ai::isUsablePreplantBombDefenseNode(true, false, false, 200.0f * 200.0f, 800.0f),
         "blind waypoint is not treated as C4 coverage");
  expect(ai::shouldReleasePreplantBombDefense(true, true, true, true),
         "plant confirmation releases staging to ordinary defense");
  expect(ai::shouldReleasePreplantBombDefense(false, true, false, true),
         "loss of carrier goal releases staging");
  expect(!ai::shouldReleasePreplantBombDefense(false, true, true, true),
         "stable preplant site retains waypoint ownership");
}

AI_TEST(testD188PlantedBombDefenseRequiresPhysicalCover) {
  expect(!ai::hasPlantedBombWorldCover(0u),
         "unobstructed open position is not a valid defensive camp");
  expect(!ai::hasPlantedBombWorldCover(1u << 1),
         "one obscured sector is insufficient proof of protection");
  expect(!ai::hasPlantedBombWorldCover((1u << 0) | (1u << 1)),
         "two adjacent sectors do not justify a long camp");
  expect(ai::hasPlantedBombWorldCover((1u << 0) | (1u << 2)),
         "separated world obstacles permit guarded stationary defense");
  expect(!ai::mayCampOnPlantedBombDefense(false, false, true),
         "arriving at an exposed node must not trigger Camp");
  expect(!ai::mayCampOnPlantedBombDefense(true, true, true),
         "defuse alarm requires combat response rather than stationary camping");
  expect(!ai::mayCampOnPlantedBombDefense(true, false, false),
         "an unfinished route must not be treated as reached");
  expect(ai::mayCampOnPlantedBombDefense(true, false, true),
         "physical cover after a completed route permits C4 protection");
  expect(ai::isMobilePlantedBombFlank(true, false, true, 300.0f * 300.0f, 220.0f),
         "exposed defender may patrol a separate reachable flank");
  expect(!ai::isMobilePlantedBombFlank(true, false, true, 300.0f * 300.0f, 0.0f),
         "zero-distance movement cannot substitute for protection");
}

AI_TEST(testPlantedBombDefensePreservesOwnershipAcrossTemporaryCombat) {
  using Task = ai::TaskType;
  expect(ai::isTransientPlantedBombDefenseTask(Task::Attack, Task::Attack, Task::SeekCover, Task::Blind),
         "legacy attack may temporarily take precedence without abandoning planted bomb defense");
  expect(ai::isTransientPlantedBombDefenseTask(Task::SeekCover, Task::Attack, Task::SeekCover, Task::Blind),
         "temporary cover movement retains the existing bomb defense destination");
  expect(ai::isTransientPlantedBombDefenseTask(Task::Blind, Task::Attack, Task::SeekCover, Task::Blind),
         "flash blindness does not discard a previously accepted protection task");
  expect(!ai::isTransientPlantedBombDefenseTask(Task::Hunt, Task::Attack, Task::SeekCover, Task::Blind),
         "stale Hunt should yield to objective defense rather than preserve its target");
  expect(!ai::isTransientPlantedBombDefenseTask(Task::MoveToPosition, Task::Attack, Task::SeekCover, Task::Blind),
         "defense movement itself should continue through the normal objective handler");
}

AI_TEST(testPlantedBombDefenseOnlyCampsAfterReachingSelectedNode) {
  expect(ai::hasReachedPlantedBombDefenseNode(12, 12, 100.0f, 2304.0f),
         "defender inside the selected node reach radius may start camping");
  expect(!ai::hasReachedPlantedBombDefenseNode(12, 13, 100.0f, 2304.0f),
         "being close to an unrelated node does not complete the defense route");
  expect(!ai::hasReachedPlantedBombDefenseNode(12, 12, 10000.0f, 2304.0f),
         "interrupted movement cannot start camping far from the selected node");
  expect(ai::hasReachedPlantedBombDefenseNode(12, 12, 2304.0f, 2304.0f),
         "reach radius boundary is accepted");
  expect(!ai::hasReachedPlantedBombDefenseNode(-1, 12, 100.0f, 2304.0f),
         "invalid current node must not be considered a completed route");
  expect(!ai::hasReachedPlantedBombDefenseNode(12, -1, 100.0f, 2304.0f),
         "invalid defense destination must not authorize camping");
}


AI_TEST(testPlantedBombProtectionCampIgnoresOrdinaryCampingBan) {
   expect(ai::mayContinuePlantedBombDefenseCamp(true, false, false, false, false, false),
          "ordinary camping stays allowed when camping cvar is on");
   expect(ai::mayContinuePlantedBombDefenseCamp(false, false, true, true, true, true),
          "active planted-C4 ProtectObjective may hold with ordinary camping disabled");
   expect(!ai::mayContinuePlantedBombDefenseCamp(false, false, true, true, true, false),
          "ordinary Terrorist camp cannot bypass the camping ban");
   expect(!ai::mayContinuePlantedBombDefenseCamp(false, false, false, true, true, true),
          "Counter-Terrorist cannot claim Terrorist C4 defense");
   expect(!ai::mayContinuePlantedBombDefenseCamp(false, false, true, false, true, true),
          "non-demolition game cannot claim planted-C4 protection");
   expect(!ai::mayContinuePlantedBombDefenseCamp(false, false, true, true, false, true),
          "disappeared objective ends camping exemption");
   expect(!ai::mayContinuePlantedBombDefenseCamp(true, true, true, true, true, true),
          "knife mode still forbids even objective camping");
   expect(!ai::mayContinuePlantedBombDefenseCamp(false, true, true, true, true, true),
          "knife mode cannot be bypassed by the camping cvar exception");
}

AI_TEST(testLatePlantedBombDefenderHoldsLocalPosition) {
   constexpr float radius = ai::kPlantedBombReinforcementRadius;
   const float insideRadiusSq = (radius - 1.0f) * (radius - 1.0f);
   expect(!ai::canArriveAtPlantedBombDefenseInTime(0.0f, 250.0f, 3.0f),
          "reinforcement travel reserve still rejects a three-second budget");
   expect(ai::canHoldPlantedBombDefenseLocally(
              false, true, 3.0f, insideRadiusSq, 100.0f, 400.0f, 2304.0f),
          "bot already at a defense node may hold during final seconds");
   expect(ai::canHoldPlantedBombDefenseLocally(
              false, true, 0.1f, insideRadiusSq, insideRadiusSq, 0.0f, 0.0f),
          "no movement is required for an already reached defense waypoint");
   expect(ai::canHoldPlantedBombDefenseLocally(
              false, true, 3.0f, radius * radius, radius * radius, 2304.0f, 2304.0f),
          "exact defense and waypoint reach boundaries are allowed");
}

AI_TEST(testLatePlantedBombHoldRejectsInvalidOrThreatenedPositions) {
   const auto canHold = [](bool alarm, bool valid, float seconds, float botDistanceSq,
                           float waypointBombDistanceSq, float waypointDistanceSq,
                           float waypointReachSq) {
      return ai::canHoldPlantedBombDefenseLocally(
         alarm, valid, seconds, botDistanceSq, waypointBombDistanceSq,
         waypointDistanceSq, waypointReachSq);
   };
   const float outsideRadiusSq = ai::kPlantedBombReinforcementRadius
       * ai::kPlantedBombReinforcementRadius + 1.0f;
   expect(!canHold(true, true, 3.0f, 100.0f, 100.0f, 0.0f, 2304.0f),
          "actual CT defuse alarm retains the urgent intercept path");
   expect(!canHold(false, false, 3.0f, 100.0f, 100.0f, 0.0f, 2304.0f),
          "invalid, CT-only or ladder waypoints cannot be chosen");
   expect(!canHold(false, true, 0.0f, 100.0f, 100.0f, 0.0f, 2304.0f),
          "expired C4 does not enter a defense hold");
   expect(!canHold(false, true, 3.0f, outsideRadiusSq, 100.0f, 0.0f, 2304.0f),
          "a distant bot cannot claim planted-C4 local defense");
   expect(!canHold(false, true, 3.0f, 100.0f, outsideRadiusSq, 0.0f, 2304.0f),
          "a distant waypoint cannot claim local defense");
   expect(!canHold(false, true, 3.0f, 100.0f, 100.0f, 2305.0f, 2304.0f),
          "being near a waypoint is insufficient until its reach radius is met");
   expect(!canHold(false, true, 3.0f, 100.0f, 100.0f, -1.0f, 2304.0f),
          "invalid negative graph-derived distances cannot authorize a hold");
}

AI_TEST(testPlantedBombReinforcementUsesGraphTravelBudget) {
  expect(ai::canArriveAtPlantedBombDefenseInTime(1500.0f, 250.0f, 20.0f),
         "reinforcement can defend when graph travel time leaves a setup reserve");
  expect(!ai::canArriveAtPlantedBombDefenseInTime(3300.0f, 250.0f, 20.0f),
         "distant T must not claim a bomb defense position after its arrival deadline");
  expect(!ai::canArriveAtPlantedBombDefenseInTime(1500.0f, 250.0f, 12.0f),
         "short remaining bomb timer prevents late reinforcement");
  expect(!ai::canArriveAtPlantedBombDefenseInTime(0.0f, 250.0f, 4.0f),
         "arrival with no time to hold the bomb is not useful");
  expect(!ai::canArriveAtPlantedBombDefenseInTime(32767.0f, 250.0f, 40.0f),
         "disconnected graph paths cannot be used as reinforcement estimates");
  expect(!ai::canArriveAtPlantedBombDefenseInTime(-1.0f, 250.0f, 40.0f),
         "negative route distances cannot authorize defense");
  expect(!ai::canArriveAtPlantedBombDefenseInTime(100.0f, 0.0f, 40.0f),
         "invalid player speed does not authorize a defense route");
}

AI_TEST(testPlantedBombReinforcementPrefersPromptCampRoutes) {
  expect(ai::isBetterPlantedBombReinforcementNode(500.0f, true, 12, 460.0f, false, 13),
         "authored camp node wins when its route detour is small");
  expect(!ai::isBetterPlantedBombReinforcementNode(900.0f, true, 12, 460.0f, false, 13),
         "camp preference must not outweigh an excessive travel delay");
  expect(ai::isBetterPlantedBombReinforcementNode(300.0f, false, 12, 500.0f, true, 13),
         "a significantly faster route outranks a distant camp");
  expect(ai::isBetterPlantedBombReinforcementNode(500.0f, false, 8, 500.0f, false, 9),
         "ties choose stable lowest waypoint id rather than random targets");
  expect(!ai::isBetterPlantedBombReinforcementNode(500.0f, false, 9, 500.0f, false, 8),
         "higher-id equal cost node must not trigger route churn");
}

AI_TEST(testDistributedPlantedC4DefenseNodeSelection) {
  expect(ai::bombDefenseCrowdingCost(100.0f * 100.0f) > 0.0f,
         "another assigned nearby defender incurs crowding penalty");
  expect(ai::bombDefenseCrowdingCost(224.0f * 224.0f) == 0.0f,
         "spaced defense positions remain unpenalized");
  expect(ai::isBetterDistributedBombDefenseNode(550.0f, false, 0.0f, 21,
           350.0f, false, 512.0f, 20),
         "a modest detour is preferable to stacking defenders");
  expect(!ai::isBetterDistributedBombDefenseNode(1200.0f, false, 0.0f, 21,
           350.0f, false, 512.0f, 20),
         "very late reinforcement should not be chosen only for separation");
  expect(ai::isBetterDistributedBombDefenseNode(550.0f, true, 0.0f, 21,
           550.0f, false, 0.0f, 20),
         "authored camp node keeps its bounded preference");
  expect(ai::isBetterDistributedBombDefenseNode(550.0f, false, 0.0f, 19,
           550.0f, false, 0.0f, 20),
         "equal options use deterministic node order");
}

AI_TEST(testBombDefenseConnectionCountSkipsUnusableRoutes) {
  struct Link { int index; bool jump; bool ladder; };
  const Link links[] { { 3, false, false }, { 7, false, false },
                       { -1, false, false }, { 8, true, false },
                       { 9, false, true } };
  const int count = ai::countWalkableBombDefenseConnections(links, [](const Link &link) {
    return link.index >= 0 && !link.jump && !link.ladder;
  });
  expect(count == 2, "only traversable waypoint exits count toward defense topology");
}

AI_TEST(testBombDefenseTopologyPreservesCoverPriority) {
  expect(ai::bombDefenseConnectionPenalty(0, false) > ai::bombDefenseConnectionPenalty(2, false),
         "isolated nodes are not preferred merely because they have zero exits");
  expect(ai::bombDefenseConnectionPenalty(2, false) < ai::bombDefenseConnectionPenalty(4, false),
         "dropped C4 CT favors a recess over a busy crossing");
  expect(ai::bombDefenseConnectionPenalty(2, true) < ai::bombDefenseConnectionPenalty(1, true),
         "planted C4 T values a second escape route");
  expect(ai::bombDefenseConnectionPenalty(2, true) < ai::bombDefenseConnectionPenalty(4, true),
         "planted C4 defense favors a controlled entrance over an open intersection");

  expect(ai::isBetterBombDefenseCover(102, 100, 500.0f, 7,
                                     100, 100, 300.0f, 8, 2, 5),
         "similar cover exposure permits a topology-based preference");
  expect(!ai::isBetterBombDefenseCover(120, 100, 500.0f, 7,
                                      100, 100, 300.0f, 8, 2, 5),
         "substantially worse exposure must not be compensated by fewer links");
  expect(!ai::isBetterBombDefenseCover(100, 125, 500.0f, 7,
                                      100, 100, 300.0f, 8, 2, 5),
         "substantially higher damage must not be compensated by fewer links");
}

AI_TEST(testPlantedBombDefenseConnectionsHaveBoundedRoutePenalty) {
  expect(ai::isBetterDistributedBombDefenseNode(530.0f, false, 0.0f, 10,
            500.0f, false, 0.0f, 11, 2, 4),
         "two ordinary exits can win over a small planted-C4 travel detour");
  expect(!ai::isBetterDistributedBombDefenseNode(900.0f, false, 0.0f, 10,
            500.0f, false, 0.0f, 11, 2, 4),
         "topology bonus cannot justify arriving too late to reinforce");
  expect(!ai::isBetterDistributedBombDefenseNode(500.0f, false, 512.0f, 10,
            530.0f, false, 0.0f, 11, 2, 4),
         "avoiding teammate crowding outweighs the topology benefit");
}

AI_TEST(testBombDefenseExitSectorsDistinguishCorridorAndCrossing) {
  struct Exit { float dx; float dy; bool walkable; };
  const Exit corridor[] { { 100.0f, 0.0f, true }, { 200.0f, 10.0f, true },
                          { 250.0f, -12.0f, true }, { -100.0f, 0.0f, true } };
  const Exit crossing[] { { 100.0f, 0.0f, true }, { 0.0f, 100.0f, true },
                          { -100.0f, 0.0f, true }, { 0.0f, -100.0f, true } };
  const Exit ignored[] { { 100.0f, 0.0f, true }, { 0.0f, 100.0f, false },
                         { 0.0f, 0.0f, true } };
  const auto walkable = [](const Exit &e) { return e.walkable; };
  const auto x = [](const Exit &e) { return e.dx; };
  const auto y = [](const Exit &e) { return e.dy; };
  expect(ai::countBombDefenseExitSectors(corridor, walkable, x, y) == 2,
         "several links along one corridor count as two horizontal approach directions");
  expect(ai::countBombDefenseExitSectors(crossing, walkable, x, y) == 4,
         "four-way intersection exposes the defender to four distinct approach directions");
  expect(ai::countBombDefenseExitSectors(ignored, walkable, x, y) == 1,
         "unwalkable links and vertical-only links do not add approach directions");
  expect(ai::bombDefenseExitSector(1.0f, 1.0f) != ai::bombDefenseExitSector(-1.0f, -1.0f),
         "opposing diagonals belong to different sectors");
}

AI_TEST(testBombDefenseDirectionRankingIsBoundedBySafetyAndTravel) {
  expect(ai::bombDefenseDirectionPenalty(2, false) < ai::bombDefenseDirectionPenalty(4, false),
         "CT defending a dropped C4 prefers fewer independent approach angles");
  expect(ai::bombDefenseDirectionPenalty(2, true) < ai::bombDefenseDirectionPenalty(1, true),
         "T defending planted C4 retains the option to reposition");
  expect(ai::isBetterBombDefenseCover(102, 100, 500.0f, 8,
            100, 100, 400.0f, 9, 4, 4, false, 2, 4),
         "near-equal cover and connectivity allow narrower approach directions");
  expect(!ai::isBetterBombDefenseCover(120, 100, 500.0f, 8,
            100, 100, 400.0f, 9, 4, 4, false, 2, 4),
         "a dangerously exposed CT position cannot win from directional topology alone");
  expect(ai::isBetterDistributedBombDefenseNode(540.0f, false, 0.0f, 8,
            500.0f, false, 0.0f, 9, 4, 4, 2, 4),
         "T may make a modest route detour for controlled approach directions");
  expect(!ai::isBetterDistributedBombDefenseNode(900.0f, false, 0.0f, 8,
            500.0f, false, 0.0f, 9, 4, 4, 2, 4),
         "an excessive detour still outweighs fewer approach directions");
  expect(!ai::isBetterDistributedBombDefenseNode(540.0f, false, 512.0f, 8,
            500.0f, false, 0.0f, 9, 4, 4, 2, 4),
         "crowding avoidance remains more important than topology preference");
}

AI_TEST(testAiRetreatPreventsDuplicateLegacySeekCoverSelection) {
  expect(ai::shouldDeferLegacySeekCoverToAiRetreat(true, true, true),
         "active AI retreat owns its escape waypoint; legacy cover must not overwrite it");
  expect(!ai::shouldDeferLegacySeekCoverToAiRetreat(false, true, true),
         "pure legacy mode must retain normal seek-cover decisions");
  expect(!ai::shouldDeferLegacySeekCoverToAiRetreat(true, false, true),
         "inactive retreat cannot suppress fresh cover decisions");
  expect(!ai::shouldDeferLegacySeekCoverToAiRetreat(true, true, false),
         "other AI actions may not suppress ordinary legacy cover decisions");
}

AI_TEST(testAiRetreatPreservesRouteUnderTransientLegacyTasks) {
  using Task = ai::TaskType;
  expect(ai::isTemporaryRetreatTaskOverride(Task::SeekCover, Task::Attack, Task::Blind, Task::SeekCover),
         "already queued legacy cover must not restart an active AI retreat every frame");
  expect(ai::isTemporaryRetreatTaskOverride(Task::Attack, Task::Attack, Task::Blind, Task::SeekCover),
         "visible enemy combat temporarily overrides retreat without losing its destination");
  expect(ai::isTemporaryRetreatTaskOverride(Task::Blind, Task::Attack, Task::Blind, Task::SeekCover),
         "flash blindness temporarily overrides retreat without discarding navigation");
  expect(!ai::isTemporaryRetreatTaskOverride(Task::MoveToPosition, Task::Attack, Task::Blind, Task::SeekCover),
         "owned retreat navigation still follows the regular task handling");
  expect(!ai::isTemporaryRetreatTaskOverride(Task::Normal, Task::Attack, Task::Blind, Task::SeekCover),
         "normal task must resume previously selected retreat waypoint");
  expect(!ai::isTemporaryRetreatTaskOverride(Task::PlantBomb, Task::Attack, Task::Blind, Task::SeekCover),
         "objective tasks must not silently count as retreat progress");
}

AI_TEST(testCtDefuseRoutePrefersRiskAwarePathOnlyWithObjectiveSlack) {
  expect(ai::shouldTryRiskAwareCtBombRoute(true, 4, 1600.0f, 250.0f, 30.0f, false),
         "some CTs can choose risk-aware route to a heard planted C4 with sufficient time");
  expect(!ai::shouldTryRiskAwareCtBombRoute(true, 3, 1600.0f, 250.0f, 30.0f, false),
         "other CTs retain the fastest C4 approach");
  expect(!ai::shouldTryRiskAwareCtBombRoute(false, 4, 1600.0f, 250.0f, 30.0f, false),
         "searching an unknown bombsite cannot exploit hidden C4 location");
  expect(!ai::shouldTryRiskAwareCtBombRoute(true, 4, 1600.0f, 250.0f, 16.0f, false),
         "limited bomb timer requires fast defuse routing");
  expect(!ai::shouldTryRiskAwareCtBombRoute(true, 4, 32767.0f, 250.0f, 40.0f, true),
         "unreachable path never authorizes a risk-aware detour");
}

AI_TEST(testCtDefuseRouteValidatesActualAStarDetour) {
  expect(ai::canAffordRiskAwareCtBombRoute(1600.0f, 1850.0f, 250.0f, 30.0f, false),
         "moderately longer risk-aware route is accepted with bomb timer slack");
  expect(!ai::canAffordRiskAwareCtBombRoute(1600.0f, 2500.0f, 250.0f, 40.0f, false),
         "danger heuristic cannot send CT on an excessive detour");
  expect(!ai::canAffordRiskAwareCtBombRoute(1600.0f, 1850.0f, 250.0f, 20.0f, false),
         "route with insufficient time for a full defuse falls back to shortest");
  expect(ai::canAffordRiskAwareCtBombRoute(1600.0f, 1850.0f, 250.0f, 21.0f, true),
         "a defuse kit may make a moderate detour affordable");
  expect(!ai::canAffordRiskAwareCtBombRoute(1600.0f, -1.0f, 250.0f, 40.0f, true),
         "malformed negative route length must not be used");
  expect(!ai::canAffordRiskAwareCtBombRoute(1600.0f, 1850.0f, 0.0f, 40.0f, true),
         "invalid movement speed must not authorize a detour");
}

AI_TEST(testCtBombRouteCrowdingUsesPredictedAllyIntentNotSharedDestination) {
  expect(ai::isRelevantCtBombRouteAlly(true, true, 100.0f * 100.0f, true),
         "live CT heading near the same planted C4 may reserve approach nodes");
  expect(!ai::isRelevantCtBombRouteAlly(false, true, 100.0f, true),
         "dead teammate cannot reserve an approach");
  expect(!ai::isRelevantCtBombRouteAlly(true, false, 100.0f, true),
         "unknown bomb location cannot reserve planted-C4 routes");
  expect(!ai::isRelevantCtBombRouteAlly(true, true, 400.0f * 400.0f, true),
         "CT searching another bombsite must not affect this route");
  expect(!ai::isRelevantCtBombRouteAlly(true, true, 0.0f, false),
         "invalid graph goal or origin cannot be reserved");
  expect(ai::shouldReserveCtBombApproachNode(20, 10, 30),
         "shared intermediate hallway receives a route penalty");
  expect(!ai::shouldReserveCtBombApproachNode(30, 10, 30),
         "all CT routes may join at the planted C4 endpoint");
  expect(!ai::shouldReserveCtBombApproachNode(10, 10, 30),
         "own starting waypoint is not a meaningful crowding signal");
}

AI_TEST(testD187DiversifyFastCtBombsiteRoutesWithoutDelayingDefuse) {
  expect(ai::shouldDiversifyCtSiteApproach(true, true, false, true, true, false, false),
         "public CT bombsite goal allows early shared-route diversification");
  expect(ai::shouldDiversifyCtSiteApproach(true, true, true, false, true, false, false),
         "known planted C4 allows route diversity even off goal-marked nodes");
  expect(!ai::shouldDiversifyCtSiteApproach(true, true, false, false, true, false, false),
         "unmarked non-bomb objective must not share hidden bombsite knowledge");
  expect(!ai::shouldDiversifyCtSiteApproach(true, true, true, true, true, true, false),
         "active defuser must keep its direct path");
  expect(!ai::shouldDiversifyCtSiteApproach(true, true, true, true, true, false, true),
         "visible combat retains higher priority than path diversification");
  expect(!ai::shouldDiversifyCtSiteApproach(true, true, true, true, false, false, false),
         "unsupported path types remain unchanged");
  expect(!ai::shouldDiversifyCtSiteApproach(false, true, true, true, true, false, false),
         "terrorists cannot be affected by CT routing changes");
  expect(ai::canAffordSharedCtSiteRoute(1500.0f, 1800.0f),
         "moderate preplant detour can avoid a shared chokepoint");
  expect(!ai::canAffordSharedCtSiteRoute(1500.0f, 2500.0f),
         "excessive detour falls back to the shortest path");
  expect(!ai::canAffordSharedCtSiteRoute(-1.0f, 1000.0f),
         "invalid graph distance cannot trigger alternative routing");
}

AI_TEST(testCtBombRouteCongestionPenaltyIsBounded) {
  expectNear(ai::ctBombRouteTrafficPenalty(0), 0.0f, 0.001f,
             "unshared approach has no penalty");
  expectNear(ai::ctBombRouteTrafficPenalty(1), 160.0f, 0.001f,
             "first overlapping CT encourages an alternative entry");
  expectNear(ai::ctBombRouteTrafficPenalty(3), 320.0f, 0.001f,
             "overcrowding penalty remains bounded even with many CTs");
  expect(ai::canAffordRiskAwareCtBombRoute(1500.0f, 1725.0f, 250.0f, 30.0f, false),
         "a moderate alternative remains acceptable if defuse time allows");
  expect(!ai::canAffordRiskAwareCtBombRoute(1500.0f, 2400.0f, 250.0f, 40.0f, false),
         "route diversity cannot outweigh excessive travel length");
  expect(!ai::canAffordRiskAwareCtBombRoute(1500.0f, 1725.0f, 250.0f, 21.0f, false),
         "bomb timer still vetoes shared-route detours that cannot finish defuse");
}


AI_TEST(testBombCarrierSiteOptionsAreDistinctAndSortedByRouteQuality) {
  ai::BombCarrierGoalOption options[4] {};
  ai::retainBombCarrierGoal(options, { 10, 400.0f, 500.0f, 0.0f, 0.0f, 0.0f });
  ai::retainBombCarrierGoal(options, { 11, 300.0f, 510.0f, 100.0f, 0.0f, 0.0f });
  ai::retainBombCarrierGoal(options, { 20, 350.0f, 550.0f, 1200.0f, 0.0f, 0.0f });
  expect(ai::chooseBombCarrierGoal(options) == 11,
         "better waypoint of the same site replaces, not duplicates, the old choice");
  expect(options[0].node == 11 && options[1].node == 20 && options[2].node == -1,
         "two separate bombsites occupy two distinct candidate slots");

  ai::retainBombCarrierGoal(options, { 10, 450.0f, 500.0f, 0.0f, 0.0f, 0.0f });
  expect(options[0].node == 11 && options[1].node == 20,
         "inferior candidate from the same site never displaces its better waypoint");
  ai::retainBombCarrierGoal(options, { 21, 220.0f, 560.0f, 1300.0f, 0.0f, 0.0f });
  expect(ai::chooseBombCarrierGoal(options) == 21,
         "a better route to the other bombsite may change the plan");
}

AI_TEST(testBombCarrierDoesNotAutomaticallyRunToNearestSite) {
  const float nearest = 700.0f;
  const float alternative = 1100.0f;
  expect(ai::canConsiderBombCarrierGoal(alternative, nearest),
         "opposite bombsite within a sensible detour is considered");
  expect(!ai::canConsiderBombCarrierGoal(2700.0f, nearest),
         "grossly inefficient detour does not trump reaching a site");
  expect(!ai::canConsiderBombCarrierGoal(500.0f, 32767.0f),
         "unreachable base graph does not authorize a bombsite choice");
  expect(ai::bombCarrierGoalScore(alternative, 50, 250, 2, false, -60.0f, false)
           < ai::bombCarrierGoalScore(nearest, 1500, 0, 0, false, 60.0f, false),
         "safer well-supported farther bombsite can defeat nearby dangerous site");
  expect(ai::bombCarrierGoalScore(1000.0f, 0, 0, 0, true, 0.0f, false)
           < ai::bombCarrierGoalScore(1000.0f, 0, 0, 0, false, 0.0f, false),
         "continuity preference avoids unnecessary replanning");
  expect(ai::bombCarrierGoalScore(900.0f, 1000, 0, 0, false, 0.0f, true)
           < ai::bombCarrierGoalScore(900.0f, 1000, 0, 0, false, 0.0f, false),
         "rusher is less deterred by historical danger than a normal carrier");
}

AI_TEST(testBombCarrierSiteOptionsCapAndStableTies) {
  ai::BombCarrierGoalOption options[2] {};
  ai::retainBombCarrierGoal(options, { 8, 200.0f, 200.0f, 0.0f, 0.0f, 0.0f });
  ai::retainBombCarrierGoal(options, { 9, 300.0f, 300.0f, 1200.0f, 0.0f, 0.0f });
  ai::retainBombCarrierGoal(options, { 3, 250.0f, 250.0f, 2500.0f, 0.0f, 0.0f });
  expect(options[0].node == 8 && options[1].node == 3,
         "full candidate buffer replaces only its lowest-quality entry");
  ai::retainBombCarrierGoal(options, { 4, 200.0f, 200.0f, 2500.0f, 0.0f, 0.0f });
  expect(ai::chooseBombCarrierGoal(options) == 4,
         "equal scores are settled by stable waypoint index");
}

AI_TEST(testSemiclipNavigationAllowsSharedTraversalButPreservesTacticalPositions) {
  using Purpose = ai::NodeOccupancyPurpose;
  expect(ai::shouldIgnoreTeammateOccupancy(true, Purpose::Traversal, false),
         "semiclip allows a bot to traverse a teammate's occupied waypoint");
  expect(!ai::shouldIgnoreTeammateOccupancy(true, Purpose::Tactical, false),
         "semiclip must not discard tactical/camping waypoint reservations");
  expect(!ai::shouldIgnoreTeammateOccupancy(false, Purpose::Traversal, false),
         "without semiclip the normal solid-body movement avoidance remains");
  expect(!ai::shouldIgnoreTeammateOccupancy(false, Purpose::Tactical, false),
         "without semiclip tactical occupancy is unchanged");
  expect(!ai::shouldIgnoreTeammateOccupancy(true, Purpose::Traversal, true),
         "deliberate teammate boost keeps collision-aware node treatment");
}

AI_TEST(testSemiclipDoesNotMaskRealNodeOccupancyAmongSeveralTeammates) {
  expect(ai::hasTeammateWaypointReservation(42, 42, 13),
         "teammate at the current waypoint reserves its tactical position");
  expect(ai::hasTeammateWaypointReservation(42, 17, 42),
         "recent waypoint is still reserved for tactical positioning");
  expect(!ai::hasTeammateWaypointReservation(42, 17, 13),
         "unrelated teammate does not reserve the requested waypoint");
  const bool occupied = ai::hasTeammateWaypointReservation(42, 17, 13)
      || ai::hasTeammateWaypointReservation(42, 42, 29);
  expect(occupied, "later teammate cannot be hidden by an earlier unrelated one");
}
