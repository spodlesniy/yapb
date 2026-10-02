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
#include <ai/ai_action_runtime.h>
#include <ai/ai_action_state.h>
#include <ai/ai_navigation_task_guard.h>

using ai::test::expect;
using ai::test::TestExecutor;
using ai::test::TestPolicy;

AI_TEST(testActionExecutor) {
  TestExecutor executor {};

  ai::Action action {};
  action.type = ai::ActionType::MoveToNode;

  ai::Observation observation {};
  observation.bot.alive = true;

  const ai::ActionResult accepted = executor.execute(action, observation);
  expect(accepted.action == ai::ActionType::MoveToNode, "executor result preserves submitted action");
  expect(accepted.type == ai::ActionResultType::Accepted, "executor can accept an action");

  observation.bot.alive = false;

  const ai::ActionResult rejected = executor.execute(action, observation);
  expect(rejected.action == ai::ActionType::MoveToNode, "executor preserves action on rejection");
  expect(rejected.type == ai::ActionResultType::Rejected, "executor can reject an action");
}

AI_TEST(testActionPipeline) {
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

  const ai::ActionResult accepted = pipeline.execute(valid, observation);
  expect(accepted.action == ai::ActionType::MoveToNode, "pipeline preserves valid action type");
  expect(accepted.type == ai::ActionResultType::Accepted, "pipeline forwards valid action to executor");
  expect(pipeline.isActive(), "pipeline keeps accepted action active");
  expect(pipeline.activeAction().targetNode == 12, "pipeline stores active action payload");
  expect(state.isActive(), "pipeline shares the externally owned action state");
  expect(executor.callCount() == 1, "pipeline invokes executor for valid action");

  ai::Action replacement {};
  replacement.type = ai::ActionType::Reload;
  replacement.weaponType = ai::WeaponType::Rifle;

  const ai::ActionResult continued = pipeline.execute(replacement, observation);
  expect(continued.action == ai::ActionType::MoveToNode, "pipeline keeps active action when new action is submitted");
  expect(continued.type == ai::ActionResultType::Accepted, "pipeline continues executing active action");
  expect(pipeline.activeAction().type == ai::ActionType::MoveToNode, "pipeline ignores replacement action while active");
  expect(executor.callCount() == 2, "pipeline executes active action again");

  ai::Action invalid {};
  invalid.type = ai::ActionType::MoveToNode;
  invalid.targetType = ai::TargetType::Node;
  invalid.targetNode = -1;

  const int callsBeforeInvalid = executor.callCount();
  const ai::ActionResult stillActive = pipeline.execute(invalid, observation);
  expect(stillActive.action == ai::ActionType::MoveToNode, "invalid new action does not replace active action");
  expect(executor.callCount() == callsBeforeInvalid + 1, "pipeline keeps executing active action");

  observation.bot.alive = false;

  const ai::ActionResult runtimeRejected = pipeline.execute(valid, observation);
  expect(runtimeRejected.action == ai::ActionType::MoveToNode, "pipeline preserves active action on terminal rejection");
  expect(runtimeRejected.type == ai::ActionResultType::Rejected, "pipeline preserves executor rejection");
  expect(!pipeline.isActive(), "terminal executor result clears active action");

  const ai::ActionResult invalidAfterTerminal = pipeline.execute(invalid, observation);
  expect(invalidAfterTerminal.type == ai::ActionResultType::Invalid, "pipeline validates new action after terminal result");
  expect(executor.callCount() == callsBeforeInvalid + 2, "invalid action after terminal result does not reach executor");

  pipeline.reset();
  expect(!pipeline.isActive(), "pipeline reset clears action state");
}

AI_TEST(testActionStateClearsOnCompletion) {
  TestExecutor executor {};
  ai::ActionState state {};
  ai::ActionPipeline pipeline { executor, state };

  ai::Observation observation {};
  observation.bot.alive = true;

  ai::Action action {};
  action.type = ai::ActionType::MoveToNode;
  action.targetType = ai::TargetType::Node;
  action.targetNode = 41;
  action.confidence = 0.75f;

  const ai::ActionResult result = pipeline.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "accepted action is stored as active");
  expect(state.isActive(), "accepted action remains active");

  executor.setResult(ai::ActionResultType::Completed);
  const ai::ActionResult completed = pipeline.execute(action, observation);
  expect(completed.type == ai::ActionResultType::Completed, "completed action returns terminal result");
  expect(!state.isActive(), "completed action is cleared immediately");
}

AI_TEST(testNavigationTaskOwnership) {
  enum class TestTask { Normal, MoveToPosition, Pause, Attack, DefuseBomb, PlantBomb, Camp, SeekCover, EscapeFromBomb };

  expect(ai::allowsNavigationOverride(TestTask::Normal, TestTask::Normal, TestTask::MoveToPosition), "Normal task allows AI navigation");
  expect(ai::allowsNavigationOverride(TestTask::MoveToPosition, TestTask::Normal, TestTask::MoveToPosition),
         "MoveToPosition task allows AI navigation");

  const TestTask blockingTasks[] = { TestTask::Pause, TestTask::Attack,    TestTask::DefuseBomb,    TestTask::PlantBomb,
                                     TestTask::Camp,  TestTask::SeekCover, TestTask::EscapeFromBomb };

  for (const auto task : blockingTasks) {
    expect(!ai::allowsNavigationOverride(task, TestTask::Normal, TestTask::MoveToPosition),
           "higher-priority legacy task blocks AI navigation");
  }
}

AI_TEST(testActionLoopControlModes) {
  TestPolicy policy {};
  ai::Observation observation {};
  observation.bot.alive = true;
  observation.bot.currentNode = 40;

  {
    ai::Controller controller { ai::ControlMode::Legacy };
    TestExecutor executor {};
    ai::ActionState state {};
    ai::ActionPipeline pipeline { executor, state };
    ai::ActionLoop loop { controller, pipeline };

    controller.setPolicy(&policy);

    const ai::ActionResult result = loop.step(observation);
    expect(result.action == ai::ActionType::None, "action loop blocks policy in Legacy mode");
    expect(result.type == ai::ActionResultType::None, "Legacy mode produces no action result");
    expect(executor.callCount() == 0, "Legacy mode does not invoke the executor");
    expect(!loop.isActive(), "Legacy mode leaves the action loop inactive");
  }

  {
    ai::Controller controller { ai::ControlMode::Neural };
    TestExecutor executor {};
    ai::ActionState state {};
    ai::ActionPipeline pipeline { executor, state };
    ai::ActionLoop loop { controller, pipeline };

    controller.setPolicy(&policy);

    const ai::ActionResult result = loop.step(observation);
    expect(result.action == ai::ActionType::MoveToNode, "action loop executes policy in Neural mode");
    expect(result.type == ai::ActionResultType::Accepted, "Neural mode returns the executor result");
    expect(executor.callCount() == 1, "Neural mode invokes the executor");
    expect(loop.isActive(), "Neural mode activates the action loop");
  }

  {
    ai::Controller controller { ai::ControlMode::Training };
    TestExecutor executor {};
    ai::ActionState state {};
    ai::ActionPipeline pipeline { executor, state };
    ai::ActionLoop loop { controller, pipeline };

    controller.setPolicy(&policy);

    const ai::ActionResult result = loop.step(observation);
    expect(result.action == ai::ActionType::MoveToNode, "action loop executes policy in Training mode");
    expect(result.type == ai::ActionResultType::Accepted, "Training mode returns the executor result");
    expect(executor.callCount() == 1, "Training mode invokes the executor");
    expect(loop.isActive(), "Training mode activates the action loop");
  };

  {
    ai::Controller controller { ai::ControlMode::Training };
    TestExecutor executor {};
    ai::ActionState state {};
    ai::ActionPipeline pipeline { executor, state };
    ai::ActionLoop loop { controller, pipeline };

    controller.setPolicy(&policy);

    expect(controller.decide(observation).type == ai::ActionType::MoveToNode,
           "training controller exposes its configured policy");
  }
}

AI_TEST(testActionLoop) {
  ai::Controller controller { ai::ControlMode::Neural };
  TestExecutor executor {};
  ai::ActionState state {};
  ai::ActionPipeline pipeline { executor, state };
  ai::ActionLoop loop { controller, pipeline };

  ai::Observation observation {};
  observation.bot.alive = true;
  observation.bot.currentNode = 40;

  const ai::ActionResult noPolicy = loop.step(observation);
  expect(noPolicy.action == ai::ActionType::None, "action loop is inactive without a policy");
  expect(noPolicy.type == ai::ActionResultType::None, "action loop does not execute a no-op action");
  expect(executor.callCount() == 0, "action loop does not invoke executor without a policy");

  TestPolicy policy {};
  controller.setPolicy(&policy);

  const ai::ActionResult accepted = loop.step(observation);
  expect(accepted.action == ai::ActionType::MoveToNode, "action loop forwards policy action type");
  expect(accepted.type == ai::ActionResultType::Accepted, "action loop executes policy action");
  expect(loop.isActive(), "action loop exposes active action state");
  expect(loop.activeAction().targetNode == 41, "action loop preserves policy target");
  expect(executor.callCount() == 1, "action loop invokes executor for new action");

  observation.bot.currentNode = 80;

  const ai::ActionResult continued = loop.step(observation);
  expect(continued.action == ai::ActionType::MoveToNode, "action loop continues active action");
  expect(continued.type == ai::ActionResultType::Accepted, "active action remains executable");
  expect(loop.activeAction().targetNode == 41, "active action is not replaced by later policy output");
  expect(executor.callCount() == 2, "action loop executes active action again");

  observation.bot.alive = false;

  const ai::ActionResult rejected = loop.step(observation);
  expect(rejected.type == ai::ActionResultType::Rejected, "action loop preserves executor rejection");
  expect(!loop.isActive(), "action loop clears terminal action");

  controller.setPolicy(nullptr);

  observation.bot.alive = true;

  const ai::ActionResult afterCompletion = loop.step(observation);
  expect(afterCompletion.action == ai::ActionType::None, "action loop stops when policy is removed");
  expect(afterCompletion.type == ai::ActionResultType::None, "removed policy produces no execution");
  expect(executor.callCount() == 3, "action loop does not execute after policy removal");
}

class InterruptingExecutor final : public ai::ActionExecutor {
public:
  enum class Task { Normal, MoveToPosition, Attack };

private:
  Task m_currentTask { Task::Normal };
  int m_callCount {};

public:
  ai::ActionResult execute(const ai::Action &action, const ai::Observation &) override {
    ++m_callCount;

    if (!ai::allowsNavigationOverride(m_currentTask, Task::Normal, Task::MoveToPosition)) {
      return { action.type, ai::ActionResultType::Interrupted, 0.0f };
    }

    return { action.type, ai::ActionResultType::Accepted, 0.0f };
  }

  void setTask(Task task) {
    m_currentTask = task;
  }

  int callCount() const {
    return m_callCount;
  }
};

AI_TEST(testActionLoopLegacyTaskTransition) {
  InterruptingExecutor executor {};
  ai::Controller controller { ai::ControlMode::Neural };
  ai::ActionState state {};
  ai::ActionPipeline pipeline { executor, state };
  ai::ActionLoop loop { controller, pipeline };
  TestPolicy policy {};

  ai::Observation observation {};
  observation.bot.alive = true;
  observation.bot.currentNode = 40;

  controller.setPolicy(&policy);

  const ai::ActionResult accepted = loop.step(observation);
  expect(accepted.type == ai::ActionResultType::Accepted, "AI action is accepted before a legacy task transition");
  expect(loop.isActive(), "AI action remains active while the navigation task owns execution");

  executor.setTask(InterruptingExecutor::Task::Attack);

  const ai::ActionResult interrupted = loop.step(observation);
  expect(interrupted.action == ai::ActionType::MoveToNode, "legacy task transition preserves the interrupted AI action type");
  expect(interrupted.type == ai::ActionResultType::Interrupted, "legacy task transition interrupts the active AI action");
  expect(!loop.isActive(), "legacy task transition clears the active AI action");
  expect(executor.callCount() == 2, "active AI action is executed again to observe the task transition");
}

AI_TEST(testActionLoopLegacyTaskRecovery) {
  InterruptingExecutor executor {};
  ai::Controller controller { ai::ControlMode::Neural };
  ai::ActionState state {};
  ai::ActionPipeline pipeline { executor, state };
  ai::ActionLoop loop { controller, pipeline };
  TestPolicy policy {};

  ai::Observation observation {};
  observation.bot.alive = true;
  observation.bot.currentNode = 40;

  controller.setPolicy(&policy);

  const ai::ActionResult first = loop.step(observation);
  expect(first.type == ai::ActionResultType::Accepted, "AI action starts while legacy task is Normal");
  expect(loop.isActive(), "AI action is active before legacy interruption");

  executor.setTask(InterruptingExecutor::Task::Attack);

  const ai::ActionResult interrupted = loop.step(observation);
  expect(interrupted.type == ai::ActionResultType::Interrupted, "legacy Attack task interrupts the active AI action");
  expect(!loop.isActive(), "interrupted AI action is cleared");

  executor.setTask(InterruptingExecutor::Task::Normal);

  const ai::ActionResult recovered = loop.step(observation);
  expect(recovered.type == ai::ActionResultType::Accepted, "AI policy can start a new action after legacy task releases control");
  expect(loop.isActive(), "new AI action becomes active after legacy task recovery");
  expect(loop.activeAction().targetNode == 41, "recovered AI control creates the policy-selected action rather than restoring stale state");
  expect(executor.callCount() == 3, "recovery executes exactly one new AI action after interruption");
}

AI_TEST(testActionState) {
  ai::ActionState state {};

  expect(!state.isActive(), "action state starts inactive");
  expect(state.action().type == ai::ActionType::None, "inactive action state has no action");
  expect(state.result().type == ai::ActionResultType::None, "inactive action state has no result");

  ai::Action action {};
  action.type = ai::ActionType::MoveToNode;
  action.targetType = ai::TargetType::Node;
  action.targetNode = 24;

  expect(state.start(action), "action state accepts first action");
  expect(state.isActive(), "action state becomes active after start");
  expect(state.action().type == ai::ActionType::MoveToNode, "action state preserves active action");
  expect(state.action().targetNode == 24, "action state preserves action payload");
  expect(state.result().action == ai::ActionType::MoveToNode, "action state initializes result with action type");
  expect(state.result().type == ai::ActionResultType::None, "action state starts with no result status");

  expect(!state.start(action), "action state rejects replacement while active");

  ai::ActionResult accepted {};
  accepted.action = ai::ActionType::MoveToNode;
  accepted.type = ai::ActionResultType::Accepted;
  accepted.elapsedTime = 0.5f;

  expect(state.updateResult(accepted), "action state accepts non-terminal result");
  expect(state.isActive(), "non-terminal result keeps action active");
  expect(state.result().type == ai::ActionResultType::Accepted, "action state stores latest result");
  expect(state.result().elapsedTime == 0.5f, "action state stores result elapsed time");

  ai::ActionResult wrongAction {};
  wrongAction.action = ai::ActionType::AttackTarget;
  wrongAction.type = ai::ActionResultType::Completed;
  expect(!state.updateResult(wrongAction), "action state rejects result for another action");
  expect(state.result().type == ai::ActionResultType::Accepted, "mismatched result does not overwrite state");

  ai::ActionResult completed {};
  completed.action = ai::ActionType::MoveToNode;
  completed.type = ai::ActionResultType::Completed;
  completed.elapsedTime = 1.25f;

  expect(state.updateResult(completed), "action state accepts terminal result");
  expect(!state.isActive(), "terminal result deactivates action");
  expect(state.result().type == ai::ActionResultType::Completed, "action state stores terminal result");

  ai::Action next {};
  next.type = ai::ActionType::Reload;
  expect(state.start(next), "action state accepts new action after terminal result");
  expect(state.isActive(), "new action becomes active");
  expect(state.action().type == ai::ActionType::Reload, "new action replaces completed action");

  state.reset();
  expect(!state.isActive(), "reset deactivates action state");
  expect(state.action().type == ai::ActionType::None, "reset clears action");
  expect(state.result().type == ai::ActionResultType::None, "reset clears result");
}

AI_TEST(testActionCancellation) {
  TestExecutor executor {};
  ai::ActionState state {};
  ai::ActionPipeline pipeline { executor, state };

  ai::Observation observation {};
  observation.bot.alive = true;

  ai::Action action {};
  action.type = ai::ActionType::MoveToNode;
  action.targetType = ai::TargetType::Node;
  action.targetNode = 33;

  const ai::ActionResult accepted = pipeline.execute(action, observation);
  expect(accepted.type == ai::ActionResultType::Accepted, "cancellation test starts with accepted action");
  expect(pipeline.isActive(), "pipeline is active before cancellation");

  expect(pipeline.cancel(), "pipeline cancels active action");
  expect(!pipeline.isActive(), "cancellation clears active state");
  expect(pipeline.result().action == ai::ActionType::MoveToNode, "cancellation preserves action type");
  expect(pipeline.result().type == ai::ActionResultType::Interrupted, "cancellation produces interrupted result");
  expect(pipeline.result().isTerminal(), "interrupted cancellation result is terminal");

  const int callsAfterCancel = executor.callCount();
  expect(!pipeline.cancel(), "pipeline reports false when cancelling inactive state");
  expect(executor.callCount() == callsAfterCancel, "cancelling inactive pipeline does not execute anything");

  ai::Action next {};
  next.type = ai::ActionType::Reload;
  next.weaponType = ai::WeaponType::Rifle;

  const ai::ActionResult restarted = pipeline.execute(next, observation);
  expect(restarted.action == ai::ActionType::Reload, "pipeline accepts new action after cancellation");
  expect(pipeline.isActive(), "new action becomes active after cancellation");
}

AI_TEST(testActionRuntime) {
  ai::Observation observation {};
  observation.bot.alive = true;
  observation.bot.currentNode = 40;

  {
    TestExecutor executor {};
    ai::ActionRuntime runtime { executor };
    TestPolicy policy {};

    expect(runtime.getMode() == ai::ControlMode::Legacy, "runtime starts in Legacy mode");
    expect(!runtime.isControlEnabled(), "runtime is disabled in Legacy mode");

    runtime.setMode(ai::ControlMode::Training);
    runtime.setPolicy(&policy);

    expect(runtime.isControlEnabled(), "Training runtime enables control when a policy is available");

    const ai::ActionResult trainingResult = runtime.step(observation);
    expect(trainingResult.action == ai::ActionType::MoveToNode, "Training runtime forwards policy action");
    expect(trainingResult.type == ai::ActionResultType::Accepted, "Training runtime executes policy action");
    expect(runtime.isActive(), "Training runtime keeps the accepted action active");
    expect(executor.callCount() == 1, "Training runtime invokes the executor once");
  }

  {
    TestExecutor executor {};
    ai::ActionRuntime runtime { executor };

    runtime.setMode(ai::ControlMode::Neural);

    const ai::ActionResult withoutPolicy = runtime.step(observation);
    expect(withoutPolicy.action == ai::ActionType::None, "Neural runtime is a no-op without a policy");
    expect(withoutPolicy.type == ai::ActionResultType::None, "runtime returns no result without a policy");
    expect(executor.callCount() == 0, "runtime does not execute without a policy");
    expect(!runtime.isActive(), "runtime remains inactive without a policy");

    TestPolicy policy {};
    runtime.setPolicy(&policy);

    expect(runtime.isControlEnabled(), "Neural runtime enables control when a policy is available");

    const ai::ActionResult accepted = runtime.step(observation);
    expect(accepted.action == ai::ActionType::MoveToNode, "runtime forwards policy action");
    expect(accepted.type == ai::ActionResultType::Accepted, "runtime executes policy action");
    expect(runtime.isActive(), "runtime keeps accepted action active");
    expect(runtime.activeAction().targetNode == 41, "runtime stores policy target");
    expect(runtime.actionState().isActive(), "runtime exposes shared action state");
    expect(executor.lastAction().targetNode == 41, "runtime sends the policy action to the executor");
    expect(executor.callCount() == 1, "runtime invokes the executor once");

    observation.bot.currentNode = 80;

    const ai::ActionResult continued = runtime.step(observation);
    expect(continued.action == ai::ActionType::MoveToNode, "runtime continues active action");
    expect(runtime.activeAction().targetNode == 41, "runtime does not replace an active action with a later policy output");
    expect(executor.lastAction().targetNode == 41, "executor receives the active action again");
    expect(executor.callCount() == 2, "runtime executes active action again");

    observation.bot.alive = false;

    const ai::ActionResult rejected = runtime.step(observation);
    expect(rejected.type == ai::ActionResultType::Rejected, "runtime propagates executor terminal result");
    expect(!runtime.isActive(), "terminal result clears runtime action state");
    expect(runtime.result().type == ai::ActionResultType::Rejected, "runtime preserves terminal result");

    observation.bot.alive = true;

    const ai::ActionResult restarted = runtime.step(observation);
    expect(restarted.type == ai::ActionResultType::Accepted, "runtime can start another action after completion");
    expect(runtime.isActive(), "runtime is active before mode cancellation");

    runtime.setMode(ai::ControlMode::Legacy);

    expect(!runtime.isActive(), "switching away from Neural mode cancels active action");
    expect(runtime.result().type == ai::ActionResultType::Interrupted, "mode cancellation produces interrupted result");
    expect(runtime.getMode() == ai::ControlMode::Legacy, "runtime reports the new control mode");
    expect(!runtime.isControlEnabled(), "runtime disables control outside Neural mode");
  }
}
