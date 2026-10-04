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
