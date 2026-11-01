//
// AiPB - one-shot deterministic mock policy tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_action_validator.h>
#include <ai/ai_mock_policy.h>

#include <limits>

using ai::test::expect;

AI_TEST(testMockPolicyRequiresExplicitArming) {
  ai::MockMovePolicy policy {};
  ai::Observation observation {};
  observation.bot.alive = true;
  expect(policy.decide(observation).type == ai::ActionType::None, "disabled mock is a no-op");
  expect(policy.arm({ 40.0f, 20.0f, 10.0f }), "finite target can arm");
  expect(!policy.arm({ 10.0f, 10.0f, 10.0f }), "already-armed mock cannot change target");
  const auto action = policy.decide(observation);
  expect(action.type == ai::ActionType::MoveToPosition && action.targetType == ai::TargetType::Position,
         "one deterministic movement intent");
  expect(action.targetPosition.x == 40.0f && action.targetPosition.y == 20.0f,
         "explicit target preserved");
  expect(ai::ActionValidator::validate(action, observation).isValid(), "normal executor contract accepts mock action");
  expect(policy.decide(observation).type == ai::ActionType::None, "no second intent is generated");
}

AI_TEST(testMockPolicyDoesNotActWhenDeadAndRejectsInvalidTarget) {
  ai::MockMovePolicy policy {};
  ai::Observation observation {};
  auto bad = ai::Vec3 { std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f };
  expect(!policy.arm(bad), "NaN target rejected");
  expect(!policy.armed(), "invalid target does not arm");
  expect(policy.arm({ 10.0f, 0.0f, 0.0f }), "valid target accepted");
  expect(policy.decide(observation).type == ai::ActionType::None, "dead bot cannot execute mock");
  observation.bot.alive = true;
  expect(policy.decide(observation).type == ai::ActionType::MoveToPosition, "living bot gets single intended action");
  policy.reset();
  expect(!policy.armed() && policy.decide(observation).type == ai::ActionType::None, "reset disables policy");
}

AI_TEST(testMockPolicyRearmingOnlyAfterReset) {
  ai::MockMovePolicy policy {};
  ai::Observation observation {};
  observation.bot.alive = true;
  expect(policy.arm({ 1.0f, 2.0f, 3.0f }), "first run");
  expect(policy.decide(observation).type == ai::ActionType::MoveToPosition, "first run emits action");
  expect(policy.decide(observation).type == ai::ActionType::None, "first run is one shot");
  policy.reset();
  expect(policy.arm({ 4.0f, 5.0f, 6.0f }), "second run after explicit reset");
  auto action = policy.decide(observation);
  expect(action.targetPosition.z == 6.0f, "second run target distinct");
}
