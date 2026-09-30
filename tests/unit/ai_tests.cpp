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
    action.type = ai::ActionType::SelectTargetNode;
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

  expect(action.type == ai::ActionType::SelectTargetNode, "custom policy action type is returned");
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
  testActionDefaults();
  testLegacyController();
  testPolicyInjection();
  testObservationBuilder();
  testCombatResourceObservation();
  testPolicyReset();
  testObservationState();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d AI unit test(s) failed.\n", g_failures);
    return 1;
  }

  std::printf("AI unit tests passed.\n");
  return 0;
}
