//
// AiPB - AI action-runtime unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"
#include "ai_test_tools.h"

#include <ai/ai_action_loop.h>
#include <ai/ai_action_pipeline.h>
#include <ai/ai_action_state.h>

using ai::test::TestExecutor;
using ai::test::TestPolicy;
using ai::test::expect;

AI_TEST (testActionExecutor) {
   TestExecutor executor {};

   ai::Action action {};
   action.type = ai::ActionType::MoveToNode;

   ai::Observation observation {};
   observation.bot.alive = true;

   const ai::ActionResult accepted = executor.execute (action, observation);
   expect (accepted.action == ai::ActionType::MoveToNode, "executor result preserves submitted action");
   expect (accepted.type == ai::ActionResultType::Accepted, "executor can accept an action");

   observation.bot.alive = false;

   const ai::ActionResult rejected = executor.execute (action, observation);
   expect (rejected.action == ai::ActionType::MoveToNode, "executor preserves action on rejection");
   expect (rejected.type == ai::ActionResultType::Rejected, "executor can reject an action");
}

AI_TEST (testActionPipeline) {
   TestExecutor executor {};
   ai::ActionState state {};
   ai::ActionPipeline pipeline { executor, state };

   ai::Observation observation {};
   observation.bot.alive = true;

   ai::Action valid {};
   valid.type = ai::ActionType::MoveToNode;
   valid.targetType = ai::TargetType::Node;
   valid.targetNode = 12;
   valid.confidence = 0.75f;

   const ai::ActionResult accepted = pipeline.execute (valid, observation);
   expect (accepted.action == ai::ActionType::MoveToNode, "pipeline preserves valid action type");
   expect (accepted.type == ai::ActionResultType::Accepted, "pipeline forwards valid action to executor");
   expect (pipeline.isActive (), "pipeline keeps accepted action active");
   expect (pipeline.activeAction ().targetNode == 12, "pipeline stores active action payload");
   expect (state.isActive (), "pipeline shares the externally owned action state");
   expect (executor.callCount () == 1, "pipeline invokes executor for valid action");

   ai::Action replacement {};
   replacement.type = ai::ActionType::Reload;
   replacement.weaponType = ai::WeaponType::Rifle;

   const ai::ActionResult continued = pipeline.execute (replacement, observation);
   expect (continued.action == ai::ActionType::MoveToNode, "pipeline keeps active action when new action is submitted");
   expect (continued.type == ai::ActionResultType::Accepted, "pipeline continues executing active action");
   expect (pipeline.activeAction ().type == ai::ActionType::MoveToNode, "pipeline ignores replacement action while active");
   expect (executor.callCount () == 2, "pipeline executes active action again");

   ai::Action invalid {};
   invalid.type = ai::ActionType::MoveToNode;
   invalid.targetType = ai::TargetType::Node;
   invalid.targetNode = -1;

   const int callsBeforeInvalid = executor.callCount ();
   const ai::ActionResult stillActive = pipeline.execute (invalid, observation);
   expect (stillActive.action == ai::ActionType::MoveToNode, "invalid new action does not replace active action");
   expect (executor.callCount () == callsBeforeInvalid + 1, "pipeline keeps executing active action");

   observation.bot.alive = false;

   const ai::ActionResult runtimeRejected = pipeline.execute (valid, observation);
   expect (runtimeRejected.action == ai::ActionType::MoveToNode, "pipeline preserves active action on terminal rejection");
   expect (runtimeRejected.type == ai::ActionResultType::Rejected, "pipeline preserves executor rejection");
   expect (!pipeline.isActive (), "terminal executor result clears active action");

   const ai::ActionResult invalidAfterTerminal = pipeline.execute (invalid, observation);
   expect (invalidAfterTerminal.type == ai::ActionResultType::Invalid, "pipeline validates new action after terminal result");
   expect (executor.callCount () == callsBeforeInvalid + 2, "invalid action after terminal result does not reach executor");

   pipeline.reset ();
   expect (!pipeline.isActive (), "pipeline reset clears action state");
}

AI_TEST (testActionLoop) {
   ai::Controller controller {};
   TestExecutor executor {};
   ai::ActionState state {};
   ai::ActionPipeline pipeline { executor, state };
   ai::ActionLoop loop { controller, pipeline };

   ai::Observation observation {};
   observation.bot.alive = true;
   observation.bot.currentNode = 40;

   const ai::ActionResult noPolicy = loop.step (observation);
   expect (noPolicy.action == ai::ActionType::None, "action loop is inactive without a policy");
   expect (noPolicy.type == ai::ActionResultType::None, "action loop does not execute a no-op action");
   expect (executor.callCount () == 0, "action loop does not invoke executor without a policy");

   TestPolicy policy {};
   controller.setPolicy (&policy);

   const ai::ActionResult accepted = loop.step (observation);
   expect (accepted.action == ai::ActionType::MoveToNode, "action loop forwards policy action type");
   expect (accepted.type == ai::ActionResultType::Accepted, "action loop executes policy action");
   expect (loop.isActive (), "action loop exposes active action state");
   expect (loop.activeAction ().targetNode == 41, "action loop preserves policy target");
   expect (executor.callCount () == 1, "action loop invokes executor for new action");

   observation.bot.currentNode = 80;

   const ai::ActionResult continued = loop.step (observation);
   expect (continued.action == ai::ActionType::MoveToNode, "action loop continues active action");
   expect (continued.type == ai::ActionResultType::Accepted, "active action remains executable");
   expect (loop.activeAction ().targetNode == 41, "active action is not replaced by later policy output");
   expect (executor.callCount () == 2, "action loop executes active action again");

   observation.bot.alive = false;

   const ai::ActionResult rejected = loop.step (observation);
   expect (rejected.type == ai::ActionResultType::Rejected, "action loop preserves executor rejection");
   expect (!loop.isActive (), "action loop clears terminal action");

   controller.setPolicy (nullptr);

   observation.bot.alive = true;

   const ai::ActionResult afterCompletion = loop.step (observation);
   expect (afterCompletion.action == ai::ActionType::None, "action loop stops when policy is removed");
   expect (afterCompletion.type == ai::ActionResultType::None, "removed policy produces no execution");
   expect (executor.callCount () == 3, "action loop does not execute after policy removal");
}

AI_TEST (testActionState) {
   ai::ActionState state {};

   expect (!state.isActive (), "action state starts inactive");
   expect (state.action ().type == ai::ActionType::None, "inactive action state has no action");
   expect (state.result ().type == ai::ActionResultType::None, "inactive action state has no result");

   ai::Action action {};
   action.type = ai::ActionType::MoveToNode;
   action.targetType = ai::TargetType::Node;
   action.targetNode = 24;

   expect (state.start (action), "action state accepts first action");
   expect (state.isActive (), "action state becomes active after start");
   expect (state.action ().type == ai::ActionType::MoveToNode, "action state preserves active action");
   expect (state.action ().targetNode == 24, "action state preserves action payload");
   expect (state.result ().action == ai::ActionType::MoveToNode, "action state initializes result with action type");
   expect (state.result ().type == ai::ActionResultType::None, "action state starts with no result status");

   expect (!state.start (action), "action state rejects replacement while active");

   ai::ActionResult accepted {};
   accepted.action = ai::ActionType::MoveToNode;
   accepted.type = ai::ActionResultType::Accepted;
   accepted.elapsedTime = 0.5f;

   expect (state.updateResult (accepted), "action state accepts non-terminal result");
   expect (state.isActive (), "non-terminal result keeps action active");
   expect (state.result ().type == ai::ActionResultType::Accepted, "action state stores latest result");
   expect (state.result ().elapsedTime == 0.5f, "action state stores result elapsed time");

   ai::ActionResult wrongAction {};
   wrongAction.action = ai::ActionType::AttackTarget;
   wrongAction.type = ai::ActionResultType::Completed;
   expect (!state.updateResult (wrongAction), "action state rejects result for another action");
   expect (state.result ().type == ai::ActionResultType::Accepted, "mismatched result does not overwrite state");

   ai::ActionResult completed {};
   completed.action = ai::ActionType::MoveToNode;
   completed.type = ai::ActionResultType::Completed;
   completed.elapsedTime = 1.25f;

   expect (state.updateResult (completed), "action state accepts terminal result");
   expect (!state.isActive (), "terminal result deactivates action");
   expect (state.result ().type == ai::ActionResultType::Completed, "action state stores terminal result");

   ai::Action next {};
   next.type = ai::ActionType::Reload;
   expect (state.start (next), "action state accepts new action after terminal result");
   expect (state.isActive (), "new action becomes active");
   expect (state.action ().type == ai::ActionType::Reload, "new action replaces completed action");

   state.reset ();
   expect (!state.isActive (), "reset deactivates action state");
   expect (state.action ().type == ai::ActionType::None, "reset clears action");
   expect (state.result ().type == ai::ActionResultType::None, "reset clears result");
}

AI_TEST (testActionCancellation) {
   TestExecutor executor {};
   ai::ActionState state {};
   ai::ActionPipeline pipeline { executor, state };

   ai::Observation observation {};
   observation.bot.alive = true;

   ai::Action action {};
   action.type = ai::ActionType::MoveToNode;
   action.targetType = ai::TargetType::Node;
   action.targetNode = 33;

   const ai::ActionResult accepted = pipeline.execute (action, observation);
   expect (accepted.type == ai::ActionResultType::Accepted, "cancellation test starts with accepted action");
   expect (pipeline.isActive (), "pipeline is active before cancellation");

   expect (pipeline.cancel (), "pipeline cancels active action");
   expect (!pipeline.isActive (), "cancellation clears active state");
   expect (pipeline.result ().action == ai::ActionType::MoveToNode, "cancellation preserves action type");
   expect (pipeline.result ().type == ai::ActionResultType::Interrupted, "cancellation produces interrupted result");
   expect (pipeline.result ().isTerminal (), "interrupted cancellation result is terminal");

   const int callsAfterCancel = executor.callCount ();
   expect (!pipeline.cancel (), "pipeline reports false when cancelling inactive state");
   expect (executor.callCount () == callsAfterCancel, "cancelling inactive pipeline does not execute anything");

   ai::Action next {};
   next.type = ai::ActionType::Reload;
   next.weaponType = ai::WeaponType::Rifle;

   const ai::ActionResult restarted = pipeline.execute (next, observation);
   expect (restarted.action == ai::ActionType::Reload, "pipeline accepts new action after cancellation");
   expect (pipeline.isActive (), "new action becomes active after cancellation");
}
