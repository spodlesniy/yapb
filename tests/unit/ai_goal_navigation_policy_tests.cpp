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

AI_TEST(testGoalNavigationPolicyUsesGoalForNormalTask) {
  const auto observation = makeObservation();
  ai::GoalNavigationPolicy policy {};

  const auto action = policy.decide(observation);

  expect(action.type == ai::ActionType::MoveToNode, "normal task uses goal navigation action");
  expect(action.targetType == ai::TargetType::Node, "goal navigation action targets a node");
  expect(action.targetNode == 20, "goal navigation action preserves goal node");
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

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::Reload, "active primary reload maps to reload");
  expect(action.weaponType == ai::WeaponType::Unknown, "primary reload leaves automatic weapon selection");
  observation.combat.reloadState = ai::ReloadState::Secondary;
  expect(ai::GoalNavigationPolicy {}.decide(observation).weaponType == ai::WeaponType::Pistol,
         "secondary reload maps to pistol category");
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
    ai::ActionType::MoveToNode,
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

AI_TEST(testGoalNavigationPolicyStopsAtCurrentGoal) {
  auto observation = makeObservation();
  observation.bot.currentGoalNode = observation.bot.currentNode;

  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::None, "reached goal produces no teacher action");
}
