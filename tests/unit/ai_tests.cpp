//
// AiPB - AI abstraction unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_controller.h>
#include <ai/ai_observation_builder.h>
#include <ai/ai_observation_state.h>
#include <ai/ai_action_spec.h>
#include <ai/ai_action_validator.h>

#include <cmath>
#include <cstdio>
#include <limits>

namespace {

int g_failures {};

void expect(bool condition, const char *message) {
  if (condition) {
    return;
  }

  std::fprintf(stderr, "FAIL: %s\n", message);
  ++g_failures;
}

class TestPolicy final : public ai::Policy {
public:
  ai::Action decide(const ai::Observation &observation) const override {
    ai::Action action {};
    action.type = ai::ActionType::MoveToNode;
    action.targetNode = observation.bot.currentNode + 1;
    action.confidence = 0.75f;

    return action;
  }
};

void testObservationDefaults() {
  const ai::Observation observation {};

  expect(observation.playerCount == 0, "observation player count defaults to zero");
  expect(observation.waypointCount == 0, "observation waypoint count defaults to zero");
  expect(observation.bot.currentNode == -1, "bot current node defaults to invalid");
  expect(observation.bot.currentGoalNode == -1, "bot goal node defaults to invalid");
  expect(observation.personality.skill == 0.5f, "default skill is neutral");
  expect(observation.personality.aggression == 0.5f, "default aggression is neutral");
  expect(observation.personality.objectiveFocus == 0.5f, "default objective focus is neutral");
}

void testActionTaxonomy() {
  expect(static_cast<uint8_t> (ai::ActionType::MoveToNode) != static_cast<uint8_t> (ai::ActionType::MoveToPosition),
    "navigation actions have distinct values");
  expect(static_cast<uint8_t> (ai::ActionType::AttackTarget) != static_cast<uint8_t> (ai::ActionType::HuntTarget),
    "combat targeting actions have distinct values");
  expect(static_cast<uint8_t> (ai::ActionType::PlantBomb) != static_cast<uint8_t> (ai::ActionType::DefuseBomb),
    "objective actions have distinct values");
  expect(static_cast<uint8_t> (ai::ActionType::Wait) != static_cast<uint8_t> (ai::ActionType::ThrowSmoke),
    "utility actions have distinct values");
  expect(static_cast<uint8_t> (ai::ActionType::Count) == static_cast<uint8_t> (ai::ActionType::ThrowSmoke) + 1u,
    "action taxonomy count follows the final action");
}


void testActionTargets() {
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

  expect(static_cast<uint8_t> (ai::TargetType::None) != static_cast<uint8_t> (ai::TargetType::Node),
    "target none and node have distinct values");
  expect(static_cast<uint8_t> (ai::TargetType::Node) != static_cast<uint8_t> (ai::TargetType::Player),
    "target node and player have distinct values");
  expect(static_cast<uint8_t> (ai::TargetType::Player) != static_cast<uint8_t> (ai::TargetType::Position),
    "target player and position have distinct values");
}

void testActionSpecifications() {
  const auto moveToNode = ai::getActionSpec(ai::ActionType::MoveToNode);
  expect(moveToNode.targetType == ai::TargetType::Node, "move-to-node requires node target");
  expect(moveToNode.requiredParameters == static_cast<uint32_t> (ai::ActionParameter::TargetNode),
    "move-to-node requires target node parameter");
  expect(moveToNode.optionalParameters == 0, "move-to-node has no optional parameters");

  const auto moveToPosition = ai::getActionSpec(ai::ActionType::MoveToPosition);
  expect(moveToPosition.targetType == ai::TargetType::Position, "move-to-position requires position target");
  expect(moveToPosition.requiredParameters == static_cast<uint32_t> (ai::ActionParameter::TargetPosition),
    "move-to-position requires target position parameter");

  const auto follow = ai::getActionSpec(ai::ActionType::FollowPlayer);
  expect(follow.targetType == ai::TargetType::Player, "follow-player requires player target");
  expect(follow.requiredParameters == static_cast<uint32_t> (ai::ActionParameter::TargetPlayer),
    "follow-player requires target player parameter");

  const auto attack = ai::getActionSpec(ai::ActionType::AttackTarget);
  expect(attack.targetType == ai::TargetType::Player, "attack-target requires player target");
  expect(attack.requiredParameters == static_cast<uint32_t> (ai::ActionParameter::TargetPlayer),
    "attack-target requires target player parameter");

  const auto hold = ai::getActionSpec(ai::ActionType::HoldPosition);
  expect(hold.targetType == ai::TargetType::None, "hold-position has no explicit target");
  expect(hold.requiredParameters == 0, "hold-position has no required parameters");
  expect(hold.optionalParameters == static_cast<uint32_t> (ai::ActionParameter::Duration),
    "hold-position accepts optional duration");

  const auto reload = ai::getActionSpec(ai::ActionType::Reload);
  expect(reload.requiredParameters == 0, "reload has no required parameters");
  expect(reload.optionalParameters == static_cast<uint32_t> (ai::ActionParameter::WeaponType),
    "reload accepts optional weapon type");

  const auto changeWeapon = ai::getActionSpec(ai::ActionType::ChangeWeapon);
  expect(changeWeapon.requiredParameters == static_cast<uint32_t> (ai::ActionParameter::WeaponType),
    "change-weapon requires weapon type");
  expect(changeWeapon.optionalParameters == 0, "change-weapon has no optional parameters");

  const auto throwGrenade = ai::getActionSpec(ai::ActionType::ThrowGrenade);
  expect(throwGrenade.targetType == ai::TargetType::Position, "throw-grenade requires position target");
  expect(throwGrenade.requiredParameters == (static_cast<uint32_t> (ai::ActionParameter::TargetPosition) |
    static_cast<uint32_t> (ai::ActionParameter::GrenadeType)), "throw-grenade requires position and grenade type");

  const auto throwFlashbang = ai::getActionSpec(ai::ActionType::ThrowFlashbang);
  expect(throwFlashbang.targetType == ai::TargetType::Position, "throw-flashbang requires position target");
  expect(throwFlashbang.requiredParameters == static_cast<uint32_t> (ai::ActionParameter::TargetPosition),
    "throw-flashbang requires target position");

  const auto objective = ai::getActionSpec(ai::ActionType::PlantBomb);
  expect(objective.targetType == ai::TargetType::None, "plant-bomb has no explicit target");
  expect(objective.requiredParameters == 0, "plant-bomb has no required parameters");
}

void testActionContract() {
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

void testActionDefaults() {
  const ai::Action action {};

  expect(action.type == ai::ActionType::None, "default action type is none");
  expect(action.targetNode == -1, "default action target node is invalid");
  expect(action.targetPlayer == -1, "default action target player is invalid");
  expect(action.confidence == 0.0f, "default action confidence is zero");
}

void testLegacyController() {
  const ai::Controller controller {};

  expect(controller.getMode() == ai::ControlMode::Legacy, "controller defaults to legacy mode");
  expect(controller.getPolicy() == nullptr, "legacy controller starts without a policy");

  const ai::Action action = controller.decide({});
  expect(action.type == ai::ActionType::None, "legacy policy is non-invasive");
}

void testPolicyInjection() {
  ai::Controller controller {};
  TestPolicy policy {};
  ai::Observation observation {};
  observation.bot.currentNode = 41;

  controller.setMode(ai::ControlMode::Neural);
  expect(controller.getMode() == ai::ControlMode::Neural, "controller stores selected mode");

  controller.setPolicy(&policy);
  expect(controller.getPolicy() == &policy, "controller stores custom policy");

  const ai::Action action = controller.decide(observation);

  expect(action.type == ai::ActionType::MoveToNode, "custom policy action type is returned");
  expect(action.targetNode == 42, "custom policy receives observation");
  expect(action.confidence == 0.75f, "custom policy confidence is preserved");
}

void testObservationBuilder() {
  ai::ObservationInput input {};
  input.gameTime = 12.5f;
  input.roundTimeRemaining = 42.0f;
  input.bot.origin = { 100.0f, 200.0f, 300.0f };
  input.bot.velocity = { 10.0f, -20.0f, 0.0f };
  input.bot.destination = { 150.0f, 250.0f, 300.0f };
  input.bot.desiredVelocity = { 20.0f, 0.0f, 0.0f };
  input.bot.health = 87.0f;
  input.bot.team = 1;
  input.bot.currentNode = 7;
  input.bot.currentGoalNode = 12;
  input.bot.currentTask = ai::TaskType::Attack;
  input.bot.alive = true;
  input.bot.objectiveFlags = ai::ObjectiveFlag::BombPlanted | ai::ObjectiveFlag::InBombZone;
  input.combat.weaponType = ai::WeaponType::Rifle;
  input.combat.ammoInClip = 24;
  input.combat.reloadState = ai::ReloadState::Primary;
  input.combat.blind = true;
  input.combat.blindTimeRemaining = 1.5f;
  input.combat.firePauseRemaining = 0.25f;
  input.combat.enemyEntity = 22;
  input.combat.enemyOrigin = { 130.0f, 240.0f, 300.0f };
  input.combat.lastEnemyEntity = 21;
  input.combat.lastEnemyOrigin = { 80.0f, 180.0f, 300.0f };
  input.combat.perceptionFlags = static_cast<uint32_t> (ai::PerceptionFlag::SeeingEnemy) | static_cast<uint32_t> (ai::PerceptionFlag::EnemyReachable);
  input.bot.navigationFlags = static_cast<uint32_t> (ai::NavigationFlag::Jump) | static_cast<uint32_t> (ai::NavigationFlag::Ladder);
  input.bot.movingToGoal = true;
  input.bot.stuck = true;
  input.personality.aggression = 0.8f;

  input.playerCount = 1;
  input.players[0].entityIndex = 9;
  input.players[0].origin = { 103.0f, 204.0f, 304.0f };
  input.players[0].health = 55.0f;
  input.players[0].enemy = true;
  input.players[0].visible = true;

  input.waypointCount = 1;
  input.waypoints[0].index = 12;
  input.waypoints[0].origin = { 90.0f, 180.0f, 300.0f };
  input.waypoints[0].nodeFlags = 0x12u;
  input.waypoints[0].connectionFlags = 0x34u;

  const ai::Observation observation = ai::buildObservation(input);

  expect(observation.gameTime == 12.5f, "builder preserves game time");
  expect(observation.bot.currentNode == 7, "builder preserves current node");
  expect(observation.bot.currentTask == ai::TaskType::Attack, "builder preserves current task");
  expect(observation.bot.objectiveFlags == (ai::ObjectiveFlag::BombPlanted | ai::ObjectiveFlag::InBombZone), "builder preserves objective flags");
  expect(observation.combat.weaponType == ai::WeaponType::Rifle, "builder preserves weapon type");
  expect(observation.combat.ammoInClip == 24, "builder preserves ammo in clip");
  expect(observation.combat.reloadState == ai::ReloadState::Primary, "builder preserves reload state");
  expect(observation.combat.blind, "builder preserves blind state");
  expect(std::fabs(observation.combat.blindTimeRemaining - 1.5f) < 0.00001f, "builder preserves blind time remaining");
  expect(std::fabs(observation.combat.firePauseRemaining - 0.25f) < 0.00001f, "builder preserves fire pause remaining");
  expect(observation.combat.enemyEntity == 22, "builder preserves current enemy entity");
  expect(observation.combat.lastEnemyEntity == 21, "builder preserves last enemy entity");
  expect(observation.combat.enemyRelativeOrigin.x == 30.0f, "combat enemy x position is relative to bot");
  expect(std::fabs(observation.combat.enemyDistance - 50.0f) < 0.00001f, "combat enemy distance is calculated");
  expect(observation.combat.lastEnemyRelativeOrigin.x == -20.0f, "last enemy x position is relative to bot");
  expect(observation.combat.perceptionFlags == (static_cast<uint32_t> (ai::PerceptionFlag::SeeingEnemy) | static_cast<uint32_t> (ai::PerceptionFlag::EnemyReachable)), "builder preserves perception flags");
  expect(observation.bot.origin.x == 100.0f, "builder preserves bot origin");
  expect(observation.bot.destination.x == 150.0f, "builder preserves navigation destination");
  expect(observation.bot.desiredVelocity.x == 20.0f, "builder preserves desired velocity");
  expect(observation.bot.navigationFlags == (static_cast<uint32_t> (ai::NavigationFlag::Jump) | static_cast<uint32_t> (ai::NavigationFlag::Ladder)), "builder preserves navigation flags");
  expect(observation.bot.movingToGoal, "builder preserves moving-to-goal state");
  expect(observation.bot.stuck, "builder preserves stuck state");
  expect(observation.playerCount == 1, "builder preserves player count");
  expect(observation.players[0].entityIndex == 9, "builder preserves player entity index");
  expect(observation.players[0].relativeOrigin.x == 3.0f, "player x position is relative to bot");
  expect(observation.players[0].relativeOrigin.y == 4.0f, "player y position is relative to bot");
  expect(observation.players[0].relativeOrigin.z == 4.0f, "player z position is relative to bot");
  expect(std::fabs(observation.players[0].distance - 6.4031243f) < 0.00001f, "player distance is calculated from relative position");
  expect(observation.waypointCount == 1, "builder preserves waypoint count");
  expect(observation.waypoints[0].relativeOrigin.x == -10.0f, "waypoint x position is relative to bot");
  expect(observation.waypoints[0].relativeOrigin.y == -20.0f, "waypoint y position is relative to bot");
  expect(observation.waypoints[0].nodeFlags == 0x12u, "builder preserves waypoint flags");
  expect(observation.personality.aggression == 0.8f, "builder preserves personality");

  input.playerCount = 255;
  input.waypointCount = 255;
  input.gameTime = std::numeric_limits<float>::infinity();
  input.players[0].origin.x = std::numeric_limits<float>::quiet_NaN();

  const ai::Observation sanitized = ai::buildObservation(input);

  expect(sanitized.playerCount == ai::kMaxObservedPlayers, "builder clamps player count");
  expect(sanitized.waypointCount == ai::kMaxObservedWaypoints, "builder clamps waypoint count");
  expect(sanitized.gameTime == 0.0f, "builder sanitizes non-finite game time");
  expect(sanitized.players[0].relativeOrigin.x == -100.0f, "builder sanitizes non-finite positions before relative transform");
}


void testCombatResourceObservation() {
  ai::ObservationInput input {};

  expect(input.combat.weaponType == ai::WeaponType::Unknown, "combat weapon type defaults to unknown");
  expect(input.combat.ammoInClip == 0, "combat ammo defaults to zero");
  expect(input.combat.reloadState == ai::ReloadState::None, "combat reload state defaults to none");
  expect(!input.combat.blind, "combat blind state defaults to false");
  expect(input.combat.blindTimeRemaining == 0.0f, "combat blind time defaults to zero");
  expect(input.combat.firePauseRemaining == 0.0f, "combat fire pause defaults to zero");

  input.combat.weaponType = ai::WeaponType::Sniper;
  input.combat.ammoInClip = 0;
  input.combat.reloadState = ai::ReloadState::Secondary;
  input.combat.blind = true;
  input.combat.blindTimeRemaining = std::numeric_limits<float>::infinity();
  input.combat.firePauseRemaining = std::numeric_limits<float>::quiet_NaN();

  const ai::Observation observation = ai::buildObservation(input);

  expect(observation.combat.weaponType == ai::WeaponType::Sniper, "builder preserves sniper weapon type");
  expect(observation.combat.ammoInClip == 0, "builder preserves empty magazine");
  expect(observation.combat.reloadState == ai::ReloadState::Secondary, "builder preserves secondary reload state");
  expect(observation.combat.blind, "builder preserves active blind state");
  expect(observation.combat.blindTimeRemaining == 0.0f, "builder sanitizes non-finite blind time");
  expect(observation.combat.firePauseRemaining == 0.0f, "builder sanitizes non-finite fire pause");
}


void testActionValidation() {
  ai::Observation observation {};
  observation.playerCount = 1;
  observation.players[0].entityIndex = 7;
  observation.players[0].valid = true;

  ai::Action none {};
  expect(ai::ActionValidator::validate(none, observation).isValid(), "default none action is valid");

  ai::Action node {};
  node.type = ai::ActionType::MoveToNode;
  node.targetType = ai::TargetType::Node;
  node.targetNode = 12;
  node.confidence = 0.8f;
  expect(ai::ActionValidator::validate(node, observation).isValid(), "valid node action is accepted");

  node.targetNode = -1;
  expect(ai::ActionValidator::validate(node, observation).error == ai::ActionValidationError::MissingTargetNode,
    "missing node target is rejected");

  node.targetNode = 12;
  node.targetType = ai::TargetType::Player;
  expect(ai::ActionValidator::validate(node, observation).error == ai::ActionValidationError::TargetTypeMismatch,
    "node action with player target type is rejected");

  ai::Action position {};
  position.type = ai::ActionType::MoveToPosition;
  position.targetType = ai::TargetType::Position;
  position.targetPosition = { 10.0f, -20.0f, 30.0f };
  position.confidence = 0.5f;
  expect(ai::ActionValidator::validate(position, observation).isValid(), "valid position action is accepted");

  position.targetPosition.x = std::numeric_limits<float>::quiet_NaN();
  expect(ai::ActionValidator::validate(position, observation).error == ai::ActionValidationError::InvalidTargetPosition,
    "non-finite target position is rejected");

  ai::Action attack {};
  attack.type = ai::ActionType::AttackTarget;
  attack.targetType = ai::TargetType::Player;
  attack.targetPlayer = 7;
  expect(ai::ActionValidator::validate(attack, observation).isValid(), "observed player target is accepted");

  attack.targetPlayer = 99;
  expect(ai::ActionValidator::validate(attack, observation).error == ai::ActionValidationError::UnknownTargetPlayer,
    "unknown player target is rejected");

  attack.targetPlayer = -1;
  expect(ai::ActionValidator::validate(attack, observation).error == ai::ActionValidationError::MissingTargetPlayer,
    "missing player target is rejected");

  ai::Action changeWeapon {};
  changeWeapon.type = ai::ActionType::ChangeWeapon;
  changeWeapon.weaponType = ai::WeaponType::Rifle;
  expect(ai::ActionValidator::validate(changeWeapon, observation).isValid(), "valid weapon change is accepted");

  changeWeapon.weaponType = ai::WeaponType::Unknown;
  expect(ai::ActionValidator::validate(changeWeapon, observation).error == ai::ActionValidationError::InvalidWeaponType,
    "missing weapon type is rejected");

  ai::Action grenade {};
  grenade.type = ai::ActionType::ThrowGrenade;
  grenade.targetType = ai::TargetType::Position;
  grenade.targetPosition = { 1.0f, 2.0f, 3.0f };
  grenade.grenadeType = ai::GrenadeType::Smoke;
  expect(ai::ActionValidator::validate(grenade, observation).isValid(), "valid grenade action is accepted");

  grenade.grenadeType = ai::GrenadeType::None;
  expect(ai::ActionValidator::validate(grenade, observation).error == ai::ActionValidationError::InvalidGrenadeType,
    "missing grenade type is rejected");

  ai::Action wait {};
  wait.type = ai::ActionType::Wait;
  wait.duration = 2.0f;
  expect(ai::ActionValidator::validate(wait, observation).isValid(), "positive wait duration is accepted");

  wait.duration = -1.0f;
  expect(ai::ActionValidator::validate(wait, observation).error == ai::ActionValidationError::InvalidDuration,
    "negative duration is rejected");

  ai::Action confidence {};
  confidence.type = ai::ActionType::Fire;
  confidence.confidence = std::numeric_limits<float>::infinity();
  expect(ai::ActionValidator::validate(confidence, observation).error == ai::ActionValidationError::InvalidConfidence,
    "non-finite confidence is rejected");

  confidence.confidence = 1.1f;
  expect(ai::ActionValidator::validate(confidence, observation).error == ai::ActionValidationError::InvalidConfidence,
    "out-of-range confidence is rejected");

  ai::Action unexpected {};
  unexpected.type = ai::ActionType::Fire;
  unexpected.targetNode = 4;
  expect(ai::ActionValidator::validate(unexpected, observation).error == ai::ActionValidationError::UnexpectedParameter,
    "unexpected action parameter is rejected");

  ai::Action invalidType {};
  invalidType.type = static_cast<ai::ActionType> (255);
  expect(ai::ActionValidator::validate(invalidType, observation).error == ai::ActionValidationError::InvalidActionType,
    "unknown action type is rejected");

  ai::Action invalidTarget {};
  invalidTarget.targetType = static_cast<ai::TargetType> (255);
  expect(ai::ActionValidator::validate(invalidTarget, observation).error == ai::ActionValidationError::InvalidTargetType,
    "unknown target type is rejected");
}


void testObservationState() {
  ai::ObservationState state {};

  expect(!state.isValid(), "observation state starts invalid");
  expect(state.sequence() == 0, "observation sequence starts at zero");

  state.markUpdated();
  expect(state.isValid(), "observation state becomes valid after update");
  expect(state.sequence() == 1, "observation sequence advances on update");

  state.markUpdated();
  expect(state.isValid(), "observation state stays valid after consecutive update");
  expect(state.sequence() == 2, "observation sequence advances monotonically");

  state.invalidate();
  expect(!state.isValid(), "observation state becomes invalid when invalidated");
  expect(state.sequence() == 2, "invalidating observation does not advance sequence");

  state.markUpdated();
  expect(state.isValid(), "observation state can become valid again");
  expect(state.sequence() == 3, "observation sequence resumes after invalidation");
}

void testPolicyReset() {
  ai::Controller controller {};
  TestPolicy policy {};

  controller.setPolicy(&policy);
  controller.setPolicy(nullptr);

  expect(controller.getPolicy() == nullptr, "null policy clears the active policy");
  expect(controller.decide({}).type == ai::ActionType::None, "restored policy is non-invasive");
}

} // namespace

int main() {
  testObservationDefaults();
  testActionTaxonomy();
  testActionContract();
  testActionSpecifications();
  testActionTargets();
  testActionDefaults();
  testLegacyController();
  testPolicyInjection();
  testObservationBuilder();
  testCombatResourceObservation();
  testPolicyReset();
  testActionValidation();
  testObservationState();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d AI unit test(s) failed.\n", g_failures);
    return 1;
  }

  std::printf("AI unit tests passed.\n");
  return 0;
}
