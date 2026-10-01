#pragma once

#include <ai/ai_action_executor.h>
#include <ai/ai_controller.h>

namespace ai::test {

class TestExecutor final : public ai::ActionExecutor {
private:
   int m_callCount {};
   ai::Action m_lastAction {};

public:
   ai::ActionResult execute (const ai::Action &action, const ai::Observation &observation) override {
      ++m_callCount;
      m_lastAction = action;

      ai::ActionResult result {};
      result.action = action.type;
      result.type = observation.bot.alive ? ai::ActionResultType::Accepted : ai::ActionResultType::Rejected;
      return result;
   }

   int callCount () const {
      return m_callCount;
   }

   const ai::Action &lastAction () const {
      return m_lastAction;
   }
};

class TestPolicy final : public ai::Policy {
public:
   ai::Action decide (const ai::Observation &observation) const override {
      ai::Action action {};
      action.type = ai::ActionType::MoveToNode;
      action.targetType = ai::TargetType::Node;
      action.targetNode = observation.bot.currentNode + 1;
      action.confidence = 0.75f;

      return action;
   }
};

} // namespace ai::test
