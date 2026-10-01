//
// AiPB - AI controller unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"
#include "ai_test_tools.h"

using ai::test::TestPolicy;
using ai::test::expect;

AI_TEST (testLegacyController) {
   const ai::Controller controller {};

   expect (controller.getMode () == ai::ControlMode::Legacy, "controller defaults to legacy mode");
   expect (controller.getPolicy () == nullptr, "legacy controller starts without a policy");

   const ai::Action action = controller.decide ({});
   expect (action.type == ai::ActionType::None, "legacy policy is non-invasive");
}

AI_TEST (testPolicyInjection) {
   ai::Controller controller {};
   TestPolicy policy {};
   ai::Observation observation {};
   observation.bot.currentNode = 41;

   controller.setMode (ai::ControlMode::Neural);
   expect (controller.getMode () == ai::ControlMode::Neural, "controller stores selected mode");

   controller.setPolicy (&policy);
   expect (controller.getPolicy () == &policy, "controller stores custom policy");

   const ai::Action action = controller.decide (observation);

   expect (action.type == ai::ActionType::MoveToNode, "custom policy action type is returned");
   expect (action.targetNode == 42, "custom policy receives observation");
   expect (action.confidence == 0.75f, "custom policy confidence is preserved");
}

AI_TEST (testPolicyReset) {
   ai::Controller controller {};
   TestPolicy policy {};

   controller.setPolicy (&policy);
   controller.setPolicy (nullptr);

   expect (controller.getPolicy () == nullptr, "null policy clears the active policy");
   expect (controller.decide ({}).type == ai::ActionType::None, "restored policy is non-invasive");
}
