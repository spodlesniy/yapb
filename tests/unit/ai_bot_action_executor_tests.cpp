//
// AiPB - AI action executor unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <cmath>
#include <limits>

#include <ai/ai_action_execution_context.h>
#include <ai/ai_bot_action_executor.h>

using ai::test::expect;
using ai::test::expectNear;

namespace {

class MockActionExecutionContext final : public ai::ActionExecutionContext {
public:
  bool alive { true };
  bool navigationOverrideAllowed { true };
  bool targetReached {};
  int nearestNode { 7 };

  int moveToNodeCalls {};
  int moveToPositionCalls {};
  int attackTargetCalls {};
  int cancelAttackTargetCalls {};
  int huntTargetCalls {};
  int cancelHuntTargetCalls {};
  int seekCoverCalls {};
  int cancelSeekCoverCalls {};
  int lastNode { -1 };
  int lastAttackTarget { -1 };
  int lastHuntTarget { -1 };
  bool huntTargetReached {};
  bool seekCoverReached {};
  ai::Vec3 lastPosition {};

  bool isAlive() const override {
    return alive;
  }

  bool allowsNavigationOverride() const override {
    return navigationOverrideAllowed;
  }

  bool navigationNodeExists(int node) const override {
    return node >= 0 && node < 100;
  }

  int navigationNodeForPosition(const ai::Vec3 &) const override {
    return nearestNode;
  }

  bool isNavigationTargetReached(int node) const override {
    return targetReached && node == nearestNode;
  }

  void moveToNode(int node) override {
    ++moveToNodeCalls;
    lastNode = node;
  }

  void moveToPosition(const ai::Vec3 &position, int node) override {
    ++moveToPositionCalls;
    lastPosition = position;
    lastNode = node;
  }

  bool attackTarget(int targetPlayer) override {
    ++attackTargetCalls;
    lastAttackTarget = targetPlayer;
    return true;
  }

  void cancelAttackTarget(int targetPlayer) override {
    ++cancelAttackTargetCalls;
    lastAttackTarget = targetPlayer;
  }

  bool huntTarget(int targetPlayer) override {
    ++huntTargetCalls;
    lastHuntTarget = targetPlayer;
    return true;
  }

  bool isHuntTargetReached(int targetPlayer) const override {
    return huntTargetReached && targetPlayer == lastHuntTarget;
  }

  void cancelHuntTarget(int targetPlayer) override {
    ++cancelHuntTargetCalls;
    lastHuntTarget = targetPlayer;
  }

  bool seekCover() override {
    ++seekCoverCalls;
    return true;
  }

  bool isSeekCoverReached() const override {
    return seekCoverReached;
  }

  void cancelSeekCover() override {
    ++cancelSeekCoverCalls;
  }
};

ai::Observation aliveObservation() {
  ai::Observation observation {};
  observation.bot.alive = true;
  return observation;
}

} // namespace

AI_TEST(testBotActionExecutorMovesToNodeThroughContext) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::MoveToNode;
  action.targetNode = 12;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Accepted, "valid node action is accepted");
  expect(context.moveToNodeCalls == 1, "node move is delegated once");
  expect(context.lastNode == 12, "delegated node is preserved");
}

AI_TEST(testBotActionExecutorRejectsUnknownNode) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::MoveToNode;
  action.targetNode = 1000;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Rejected, "unknown node is rejected");
  expect(context.moveToNodeCalls == 0, "unknown node is not delegated");
}

AI_TEST(testBotActionExecutorCompletesReachedNode) {
  MockActionExecutionContext context {};
  context.nearestNode = 12;
  context.targetReached = true;
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::MoveToNode;
  action.targetNode = 12;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Completed, "reached node completes the action");
  expect(context.moveToNodeCalls == 0, "reached node is not delegated");
}

AI_TEST(testBotActionExecutorInterruptsNavigationWithoutOwnership) {
  MockActionExecutionContext context {};
  context.navigationOverrideAllowed = false;
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::MoveToNode;
  action.targetNode = 12;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Interrupted, "navigation loses ownership when override is disallowed");
  expect(context.moveToNodeCalls == 0, "navigation is not delegated after ownership loss");
}

AI_TEST(testBotActionExecutorRejectsInactiveContext) {
  MockActionExecutionContext context {};
  context.alive = false;
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::MoveToNode;
  action.targetNode = 12;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Rejected, "inactive context rejects execution");
}

AI_TEST(testBotActionExecutorMovesToPositionThroughContext) {
  MockActionExecutionContext context {};
  context.nearestNode = 21;
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::MoveToPosition;
  action.targetPosition = { 10.0f, 20.0f, 30.0f };

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Accepted, "valid position action is accepted");
  expect(context.moveToPositionCalls == 1, "position move is delegated once");
  expect(context.lastNode == 21, "position uses the nearest waypoint");
  expectNear(context.lastPosition.x, 10.0f, 0.001f, "x position is preserved");
  expectNear(context.lastPosition.y, 20.0f, 0.001f, "y position is preserved");
  expectNear(context.lastPosition.z, 30.0f, 0.001f, "z position is preserved");
}

AI_TEST(testBotActionExecutorRejectsNonFinitePosition) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::MoveToPosition;
  action.targetPosition.x = std::numeric_limits<float>::quiet_NaN();

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Invalid, "non-finite position is invalid");
  expect(context.moveToPositionCalls == 0, "invalid position is not delegated");
}

ai::Observation attackObservation(int targetPlayer) {
  auto observation = aliveObservation();
  observation.combat.enemyEntity = targetPlayer;
  observation.combat.perceptionFlags = static_cast<uint32_t>(ai::PerceptionFlag::SeeingEnemy);
  observation.playerCount = 1;
  observation.players[0].entityIndex = targetPlayer;
  observation.players[0].valid = true;
  observation.players[0].alive = true;
  observation.players[0].enemy = true;
  observation.players[0].visible = true;
  return observation;
}

AI_TEST(testBotActionExecutorDirectlyExecutesAttackTarget) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  ai::Action action {};
  action.type = ai::ActionType::AttackTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;
  const auto result = executor.execute(action, attackObservation(9));
  expect(result.type == ai::ActionResultType::Accepted, "visible enemy attack is accepted");
  expect(context.attackTargetCalls == 1, "attack target is delegated");
  expect(context.lastAttackTarget == 9, "attack target is preserved");
  expect(executor.suppressesLegacyTaskExecution(), "direct attack owns task execution");
}

AI_TEST(testBotActionExecutorCompletesAttackWhenTargetChanges) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  ai::Action action {};
  action.type = ai::ActionType::AttackTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;
  executor.execute(action, attackObservation(9));
  const auto result = executor.execute(action, attackObservation(10));
  expect(result.type == ai::ActionResultType::Completed, "changed target completes active attack");
  expect(context.cancelAttackTargetCalls == 1, "completed attack releases target");
  expect(!executor.suppressesLegacyTaskExecution(), "completed attack releases ownership");
}

ai::Observation huntObservation(int targetPlayer) {
  auto observation = aliveObservation();
  observation.combat.lastEnemyEntity = targetPlayer;
  observation.playerCount = 1;
  observation.players[0].entityIndex = targetPlayer;
  observation.players[0].valid = true;
  observation.players[0].alive = true;
  observation.players[0].enemy = true;
  return observation;
}

AI_TEST(testBotActionExecutorDirectlyExecutesHuntTarget) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::HuntTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;

  const auto result = executor.execute(action, huntObservation(9));

  expect(result.type == ai::ActionResultType::Accepted, "remembered enemy hunt is accepted");
  expect(context.huntTargetCalls == 1, "hunt target is delegated");
  expect(context.lastHuntTarget == 9, "hunt target is preserved");
}

AI_TEST(testBotActionExecutorDirectlyExecutesSeekCover) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::SeekCover;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Accepted, "seek cover is accepted");
  expect(context.seekCoverCalls == 1, "seek cover is delegated");
}

AI_TEST(testBotActionExecutorCompletesSeekCoverWhenReached) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::SeekCover;

  auto result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Accepted, "seek cover starts before completion");

  context.seekCoverReached = true;
  result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Completed, "reached cover completes the active action");
  expect(context.cancelSeekCoverCalls == 1, "completion releases the active cover action");
}

AI_TEST(testBotActionExecutorCancelsSeekCover) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::SeekCover;

  executor.execute(action, aliveObservation());
  executor.cancel();

  expect(context.cancelSeekCoverCalls == 1, "cancel releases direct seek cover");
}

AI_TEST(testBotActionExecutorCompletesHuntWhenTargetPositionIsReached) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::HuntTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;

  auto result = executor.execute(action, huntObservation(9));
  expect(result.type == ai::ActionResultType::Accepted, "hunt starts before target is reached");
  expect(context.huntTargetCalls == 1, "hunt target is delegated before completion");

  context.huntTargetReached = true;
  result = executor.execute(action, huntObservation(9));

  expect(result.type == ai::ActionResultType::Completed, "reached hunt target completes the active action");
  expect(context.cancelHuntTargetCalls == 1, "completion releases the active hunt target");
}

AI_TEST(testBotActionExecutorCancelsDirectHunt) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::HuntTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;

  executor.execute(action, huntObservation(9));
  executor.cancel();

  expect(context.cancelHuntTargetCalls == 1, "cancel releases direct hunt");
  expect(context.lastHuntTarget == 9, "cancel releases active hunt target");
}

AI_TEST(testBotActionExecutorCancelReleasesDirectAttack) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  ai::Action action {};
  action.type = ai::ActionType::AttackTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;
  executor.execute(action, attackObservation(9));
  executor.cancel();
  expect(context.cancelAttackTargetCalls == 1, "cancel releases direct attack");
  expect(!executor.suppressesLegacyTaskExecution(), "cancel releases ownership");
}

AI_TEST(testBotActionExecutorCompletesObservedTaskLifecycle) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  ai::Observation observation = aliveObservation();
  observation.bot.currentTask = ai::TaskType::PlantBomb;

  auto action = ai::Action {};
  action.type = ai::ActionType::PlantBomb;

  auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "matching task starts the action");

  result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "matching task keeps the action active");

  observation.bot.currentTask = ai::TaskType::Normal;
  result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Completed, "leaving the observed task completes the action");
}
