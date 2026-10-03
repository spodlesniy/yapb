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
  int lastNode { -1 };
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

AI_TEST(testBotActionExecutorCompletesObservedTaskLifecycle) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  ai::Observation observation = aliveObservation();
  observation.bot.currentTask = ai::TaskType::Attack;
  observation.combat.enemyEntity = 9;

  auto action = ai::Action {};
  action.type = ai::ActionType::AttackTarget;
  action.targetPlayer = 9;

  auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "matching task starts the action");

  result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "matching task keeps the action active");

  observation.bot.currentTask = ai::TaskType::Normal;
  result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Completed, "leaving the observed task completes the action");
}
