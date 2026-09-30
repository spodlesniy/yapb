//
// AiPB - AI abstraction unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_controller.h>
#include <ai/ai_observation_builder.h>

#include <cstdio>
#include <limits>

namespace {

int g_failures {};

void expect (bool condition, const char *message) {
   if (condition) {
      return;
   }

   std::fprintf (stderr, "FAIL: %s\n", message);
   ++g_failures;
}

class TestPolicy final : public ai::Policy {
public:
   ai::Action decide (const ai::Observation &observation) const override {
      ai::Action action {};
      action.type = ai::ActionType::SelectTargetNode;
      action.targetNode = observation.bot.currentNode + 1;
      action.confidence = 0.75f;

      return action;
   }
};

void testObservationDefaults () {
   const ai::Observation observation {};

   expect (observation.playerCount == 0, "observation player count defaults to zero");
   expect (observation.waypointCount == 0, "observation waypoint count defaults to zero");
   expect (observation.bot.currentNode == -1, "bot current node defaults to invalid");
   expect (observation.bot.currentGoalNode == -1, "bot goal node defaults to invalid");
   expect (observation.personality.skill == 0.5f, "default skill is neutral");
   expect (observation.personality.aggression == 0.5f, "default aggression is neutral");
   expect (observation.personality.objectiveFocus == 0.5f, "default objective focus is neutral");
}

void testActionDefaults () {
   const ai::Action action {};

   expect (action.type == ai::ActionType::None, "default action type is none");
   expect (action.targetNode == -1, "default action target node is invalid");
   expect (action.targetPlayer == -1, "default action target player is invalid");
   expect (action.confidence == 0.0f, "default action confidence is zero");
}

void testLegacyController () {
   const ai::Controller controller {};

   expect (controller.getMode () == ai::ControlMode::Legacy, "controller defaults to legacy mode");
   expect (controller.getPolicy () == nullptr, "legacy controller starts without a policy");

   const ai::Action action = controller.decide ({});
   expect (action.type == ai::ActionType::None, "legacy policy is non-invasive");
}

void testPolicyInjection () {
   ai::Controller controller {};
   TestPolicy policy {};
   ai::Observation observation {};
   observation.bot.currentNode = 41;

   controller.setMode (ai::ControlMode::Neural);
   expect (controller.getMode () == ai::ControlMode::Neural, "controller stores selected mode");

   controller.setPolicy (&policy);
   expect (controller.getPolicy () == &policy, "controller stores custom policy");

   const ai::Action action = controller.decide (observation);

   expect (action.type == ai::ActionType::SelectTargetNode, "custom policy action type is returned");
   expect (action.targetNode == 42, "custom policy receives observation");
   expect (action.confidence == 0.75f, "custom policy confidence is preserved");
}

void testObservationBuilder () {
   ai::ObservationInput input {};
   input.gameTime = 12.5f;
   input.roundTimeRemaining = 42.0f;
   input.bot.origin = { 100.0f, 200.0f, 300.0f };
   input.bot.velocity = { 10.0f, -20.0f, 0.0f };
   input.bot.health = 87.0f;
   input.bot.team = 1;
   input.bot.currentNode = 7;
   input.bot.currentGoalNode = 12;
   input.bot.alive = true;
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

   const ai::Observation observation = ai::buildObservation (input);

   expect (observation.gameTime == 12.5f, "builder preserves game time");
   expect (observation.bot.currentNode == 7, "builder preserves current node");
   expect (observation.bot.origin.x == 100.0f, "builder preserves bot origin");
   expect (observation.playerCount == 1, "builder preserves player count");
   expect (observation.players[0].entityIndex == 9, "builder preserves player entity index");
   expect (observation.players[0].relativeOrigin.x == 3.0f, "player x position is relative to bot");
   expect (observation.players[0].relativeOrigin.y == 4.0f, "player y position is relative to bot");
   expect (observation.players[0].relativeOrigin.z == 4.0f, "player z position is relative to bot");
   expect (std::fabs (observation.players[0].distance - 6.4031243f) < 0.00001f, "player distance is calculated from relative position");
   expect (observation.waypointCount == 1, "builder preserves waypoint count");
   expect (observation.waypoints[0].relativeOrigin.x == -10.0f, "waypoint x position is relative to bot");
   expect (observation.waypoints[0].relativeOrigin.y == -20.0f, "waypoint y position is relative to bot");
   expect (observation.waypoints[0].nodeFlags == 0x12u, "builder preserves waypoint flags");
   expect (observation.personality.aggression == 0.8f, "builder preserves personality");

   input.playerCount = 255;
   input.waypointCount = 255;
   input.gameTime = std::numeric_limits<float>::infinity ();
   input.players[0].origin.x = std::numeric_limits<float>::quiet_NaN ();

   const ai::Observation sanitized = ai::buildObservation (input);

   expect (sanitized.playerCount == ai::kMaxObservedPlayers, "builder clamps player count");
   expect (sanitized.waypointCount == ai::kMaxObservedWaypoints, "builder clamps waypoint count");
   expect (sanitized.gameTime == 0.0f, "builder sanitizes non-finite game time");
   expect (sanitized.players[0].relativeOrigin.x == 0.0f, "builder sanitizes non-finite positions");
}

void testPolicyReset () {
   ai::Controller controller {};
   TestPolicy policy {};

   controller.setPolicy (&policy);
   controller.setPolicy (nullptr);

   expect (controller.getPolicy () == nullptr, "null policy clears the active policy");
   expect (controller.decide ({}).type == ai::ActionType::None, "restored policy is non-invasive");
}

} // namespace

int main () {
   testObservationDefaults ();
   testActionDefaults ();
   testLegacyController ();
   testPolicyInjection ();
   testObservationBuilder ();
   testPolicyReset ();

   if (g_failures != 0) {
      std::fprintf (stderr, "%d AI unit test(s) failed.\n", g_failures);
      return 1;
   }

   std::printf ("AI unit tests passed.\n");
   return 0;
}
