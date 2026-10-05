//
// AiPB - task-aware teacher policy unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_goal_navigation_policy.h>

using ai::test::expect;

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

} // namespace

AI_TEST(testGoalNavigationPolicyExploresForNormalTask) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Normal;
  ai::GoalNavigationPolicy policy {};

  const auto action = policy.decide(observation);

  expect(action.type == ai::ActionType::Explore, "normal task maps to explore");
  expect(action.targetType == ai::TargetType::None, "explore leaves waypoint selection to the execution context");
}

AI_TEST(testGoalNavigationPolicyUsesObjectiveGoalForBombCarrier) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Normal;
  observation.bot.hasC4 = true;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::MoveToNode, "bomb carrier keeps the legacy objective goal");
  expect(action.targetType == ai::TargetType::Node, "bomb carrier objective navigation targets a node");
  expect(action.targetNode == 20, "bomb carrier preserves the selected bombsite goal");
}

AI_TEST(testGoalNavigationPolicyYieldsBombZoneToPlantTaskSelection) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Normal;
  observation.bot.hasC4 = true;
  observation.bot.inBombZone = true;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::None, "bomb carrier in a bomb zone yields to legacy plant-task selection");
}

AI_TEST(testGoalNavigationPolicyMovesToPosition) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::MoveToPosition;
  observation.bot.destination = { 100.0f, 200.0f, 300.0f };

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::MoveToPosition, "move task maps to move-to-position");
  expect(action.targetType == ai::TargetType::Position, "move-to-position targets a position");
  expect(action.targetPosition.x == 100.0f, "move-to-position preserves x");
  expect(action.targetPosition.y == 200.0f, "move-to-position preserves y");
  expect(action.targetPosition.z == 300.0f, "move-to-position preserves z");
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
  observation.bot.currentTask = ai::TaskType::Hunt;
  action = policy.decide(observation);
  expect(action.type == ai::ActionType::HuntTarget, "hunt task maps to hunt target");
  expect(action.targetPlayer == 7, "hunt target preserves enemy entity");
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
  auto observation = makeObservation();
  observation.bot.team = 0;
  observation.bot.currentTask = ai::TaskType::Camp;
  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombPlanted;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::ProtectObjective, "planted-bomb defense maps to protect objective");
  expect(action.targetType == ai::TargetType::None, "protect objective has no explicit target payload");
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
    ai::ActionType::MoveToPosition,
    ai::ActionType::FollowPlayer,
    ai::ActionType::PickupItem,
    ai::ActionType::Camp,
    ai::ActionType::PlantBomb,
    ai::ActionType::DefuseBomb,
    ai::ActionType::MoveToNode,
    ai::ActionType::MoveToNode,
    ai::ActionType::SeekCover,
    ai::ActionType::ThrowGrenade,
    ai::ActionType::ThrowFlashbang,
    ai::ActionType::ThrowSmoke,
    ai::ActionType::MoveToNode,
    ai::ActionType::EscapeFromBomb,
    ai::ActionType::Fire,
    ai::ActionType::Hide,
    ai::ActionType::MoveToNode,
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

AI_TEST(testGoalNavigationPolicyFallsBackForHuntWhenCombatTargetIsUnavailable) {
  auto observation = makeObservation();
  observation.bot.currentTask = ai::TaskType::Hunt;
  ai::GoalNavigationPolicy policy {};

  const auto action = policy.decide(observation);

  expect(action.type == ai::ActionType::MoveToNode, "hunt without observed target falls back to goal");
  expect(action.targetNode == 20, "hunt fallback preserves navigation goal");
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
