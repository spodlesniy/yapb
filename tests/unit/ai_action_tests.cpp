//
// AiPB - AI action unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_action.h>
#include <ai/ai_action_result.h>
#include <ai/ai_action_spec.h>

#include <cstdint>

using ai::test::expect;

AI_TEST(testActionTaxonomy) {
  expect(static_cast<uint8_t>(ai::ActionType::MoveToNode) != static_cast<uint8_t>(ai::ActionType::MoveToPosition),
         "navigation actions have distinct values");
  expect(static_cast<uint8_t>(ai::ActionType::AttackTarget) != static_cast<uint8_t>(ai::ActionType::HuntTarget),
         "combat targeting actions have distinct values");
  expect(static_cast<uint8_t>(ai::ActionType::PlantBomb) != static_cast<uint8_t>(ai::ActionType::DefuseBomb),
         "objective actions have distinct values");
  expect(static_cast<uint8_t>(ai::ActionType::Wait) != static_cast<uint8_t>(ai::ActionType::ThrowSmoke),
         "utility actions have distinct values");
  expect(static_cast<uint8_t>(ai::ActionType::Hide) != static_cast<uint8_t>(ai::ActionType::HoldPosition),
         "hide and hold-position actions have distinct values");
  expect(static_cast<uint8_t>(ai::ActionType::Count) == static_cast<uint8_t>(ai::ActionType::Hide) + 1u,
         "action taxonomy count follows the final action");
}

AI_TEST(testActionTargets) {
  const ai::Action action {};

  expect(action.targetType == ai::TargetType::None, "action target type defaults to none");

  ai::Action nodeAction {};
  nodeAction.type = ai::ActionType::MoveToNode;
  nodeAction.targetType = ai::TargetType::Node;
  nodeAction.targetNode = 17;

  expect(nodeAction.targetType == ai::TargetType::Node, "node action declares node target");
  expect(nodeAction.targetNode == 17, "node action preserves target node");

  ai::Action playerAction {};
  playerAction.type = ai::ActionType::AttackTarget;
  playerAction.targetType = ai::TargetType::Player;
  playerAction.targetPlayer = 5;

  expect(playerAction.targetType == ai::TargetType::Player, "combat action declares player target");
  expect(playerAction.targetPlayer == 5, "player action preserves target player");

  ai::Action positionAction {};
  positionAction.type = ai::ActionType::MoveToPosition;
  positionAction.targetType = ai::TargetType::Position;
  positionAction.targetPosition = { 12.0f, 34.0f, 56.0f };

  expect(positionAction.targetType == ai::TargetType::Position, "position action declares position target");
  expect(positionAction.targetPosition.x == 12.0f, "position action preserves target position");

  expect(static_cast<uint8_t>(ai::TargetType::None) != static_cast<uint8_t>(ai::TargetType::Node),
         "target none and node have distinct values");
  expect(static_cast<uint8_t>(ai::TargetType::Node) != static_cast<uint8_t>(ai::TargetType::Player),
         "target node and player have distinct values");
  expect(static_cast<uint8_t>(ai::TargetType::Player) != static_cast<uint8_t>(ai::TargetType::Position),
         "target player and position have distinct values");
}

AI_TEST(testActionSpecifications) {
  const auto moveToNode = ai::getActionSpec(ai::ActionType::MoveToNode);
  expect(moveToNode.targetType == ai::TargetType::Node, "move-to-node requires node target");
  expect(moveToNode.requiredParameters == static_cast<uint32_t>(ai::ActionParameter::TargetNode),
         "move-to-node requires target node parameter");
  expect(moveToNode.optionalParameters == 0, "move-to-node has no optional parameters");

  const auto moveToPosition = ai::getActionSpec(ai::ActionType::MoveToPosition);
  expect(moveToPosition.targetType == ai::TargetType::Position, "move-to-position requires position target");
  expect(moveToPosition.requiredParameters == static_cast<uint32_t>(ai::ActionParameter::TargetPosition),
         "move-to-position requires target position parameter");

  const auto follow = ai::getActionSpec(ai::ActionType::FollowPlayer);
  expect(follow.targetType == ai::TargetType::Player, "follow-player requires player target");
  expect(follow.requiredParameters == static_cast<uint32_t>(ai::ActionParameter::TargetPlayer),
         "follow-player requires target player parameter");

  const auto attack = ai::getActionSpec(ai::ActionType::AttackTarget);
  expect(attack.targetType == ai::TargetType::Player, "attack-target requires player target");
  expect(attack.requiredParameters == static_cast<uint32_t>(ai::ActionParameter::TargetPlayer),
         "attack-target requires target player parameter");

  const auto hold = ai::getActionSpec(ai::ActionType::HoldPosition);
  expect(hold.targetType == ai::TargetType::None, "hold-position has no explicit target");
  expect(hold.requiredParameters == 0, "hold-position has no required parameters");
  expect(hold.optionalParameters == static_cast<uint32_t>(ai::ActionParameter::Duration), "hold-position accepts optional duration");

  const auto hide = ai::getActionSpec(ai::ActionType::Hide);
  expect(hide.targetType == ai::TargetType::None, "hide has no explicit target");
  expect(hide.requiredParameters == 0, "hide has no required parameters");
  expect(hide.optionalParameters == static_cast<uint32_t>(ai::ActionParameter::Duration), "hide accepts optional duration");

  const auto reload = ai::getActionSpec(ai::ActionType::Reload);
  expect(reload.requiredParameters == 0, "reload has no required parameters");
  expect(reload.optionalParameters == static_cast<uint32_t>(ai::ActionParameter::WeaponType), "reload accepts optional weapon type");

  const auto changeWeapon = ai::getActionSpec(ai::ActionType::ChangeWeapon);
  expect(changeWeapon.requiredParameters == static_cast<uint32_t>(ai::ActionParameter::WeaponType), "change-weapon requires weapon type");
  expect(changeWeapon.optionalParameters == 0, "change-weapon has no optional parameters");

  const auto throwGrenade = ai::getActionSpec(ai::ActionType::ThrowGrenade);
  expect(throwGrenade.targetType == ai::TargetType::Position, "throw-grenade requires position target");
  expect(throwGrenade.requiredParameters ==
             (static_cast<uint32_t>(ai::ActionParameter::TargetPosition) | static_cast<uint32_t>(ai::ActionParameter::GrenadeType)),
         "throw-grenade requires position and grenade type");

  const auto throwFlashbang = ai::getActionSpec(ai::ActionType::ThrowFlashbang);
  expect(throwFlashbang.targetType == ai::TargetType::Position, "throw-flashbang requires position target");
  expect(throwFlashbang.requiredParameters ==
             (static_cast<uint32_t>(ai::ActionParameter::TargetPosition) | static_cast<uint32_t>(ai::ActionParameter::GrenadeType)),
         "throw-flashbang requires position and grenade type");

  const auto throwSmoke = ai::getActionSpec(ai::ActionType::ThrowSmoke);
  expect(throwSmoke.targetType == ai::TargetType::Position, "throw-smoke requires position target");
  expect(throwSmoke.requiredParameters ==
             (static_cast<uint32_t>(ai::ActionParameter::TargetPosition) | static_cast<uint32_t>(ai::ActionParameter::GrenadeType)),
         "throw-smoke requires position and grenade type");


  const auto objective = ai::getActionSpec(ai::ActionType::PlantBomb);
  expect(objective.targetType == ai::TargetType::None, "plant-bomb has no explicit target");
  expect(objective.requiredParameters == 0, "plant-bomb has no required parameters");
}

AI_TEST(testActionContract) {
  const ai::Action action {};

  expect(action.targetPosition.x == 0.0f, "action target position defaults to zero");
  expect(action.targetPosition.y == 0.0f, "action target position y defaults to zero");
  expect(action.targetPosition.z == 0.0f, "action target position z defaults to zero");
  expect(action.weaponType == ai::WeaponType::Unknown, "action weapon type defaults to unknown");
  expect(action.grenadeType == ai::GrenadeType::None, "action grenade type defaults to none");
  expect(action.duration == 0.0f, "action duration defaults to zero");

  ai::Action parameterized {};
  parameterized.targetPosition = { 10.0f, -20.0f, 30.0f };
  parameterized.weaponType = ai::WeaponType::Pistol;
  parameterized.grenadeType = ai::GrenadeType::Smoke;
  parameterized.duration = 2.5f;
  parameterized.confidence = 0.9f;

  expect(parameterized.targetPosition.x == 10.0f, "action preserves target position x");
  expect(parameterized.targetPosition.y == -20.0f, "action preserves target position y");
  expect(parameterized.targetPosition.z == 30.0f, "action preserves target position z");
  expect(parameterized.weaponType == ai::WeaponType::Pistol, "action preserves weapon type");
  expect(parameterized.grenadeType == ai::GrenadeType::Smoke, "action preserves grenade type");
  expect(parameterized.duration == 2.5f, "action preserves duration");
  expect(parameterized.confidence == 0.9f, "action preserves confidence");
}

AI_TEST(testActionDefaults) {
  const ai::Action action {};

  expect(action.type == ai::ActionType::None, "default action type is none");
  expect(action.targetNode == -1, "default action target node is invalid");
  expect(action.targetPlayer == -1, "default action target player is invalid");
  expect(action.confidence == 0.0f, "default action confidence is zero");
}

AI_TEST(testActionResult) {
  const ai::ActionResult result {};

  expect(result.action == ai::ActionType::None, "action result defaults to no action");
  expect(result.type == ai::ActionResultType::None, "action result defaults to none");
  expect(result.elapsedTime == 0.0f, "action result elapsed time defaults to zero");
  expect(!result.isTerminal(), "default action result is not terminal");

  ai::ActionResult accepted {};
  accepted.action = ai::ActionType::MoveToNode;
  accepted.type = ai::ActionResultType::Accepted;
  accepted.elapsedTime = 0.125f;

  expect(accepted.action == ai::ActionType::MoveToNode, "action result preserves action type");
  expect(accepted.type == ai::ActionResultType::Accepted, "action result preserves accepted state");
  expect(accepted.elapsedTime == 0.125f, "action result preserves elapsed time");
  expect(!accepted.isTerminal(), "accepted action result is not terminal");

  const ai::ActionResultType terminalStates[] = {
    ai::ActionResultType::Completed, ai::ActionResultType::Rejected,    ai::ActionResultType::Invalid,
    ai::ActionResultType::Failed,    ai::ActionResultType::Interrupted,
  };

  for (const auto state : terminalStates) {
    ai::ActionResult terminal {};
    terminal.action = ai::ActionType::AttackTarget;
    terminal.type = state;
    expect(terminal.isTerminal(), "terminal action result state is recognized");
  }
}
