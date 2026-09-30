//
// YaPB - AI abstraction unit tests.
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_controller.h>
#include <ai/ai_observation_builder.h>

#include <cstdio>\n#include <limits>

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
