//
// AiPB - Bot AI runtime.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action_runtime.h>
#include <ai/ai_bot_action_executor.h>
#include <ai/ai_goal_navigation_policy.h>

namespace ai {

// Binds the engine-independent action runtime to a live YaPB bot.
// Runtime mode and policy selection remain controlled through ActionRuntime.
// The runtime is intentionally dormant while the controller stays in Legacy mode.
class BotRuntime final {
private:
   GoalNavigationPolicy m_goalNavigationPolicy {};
   BotActionExecutor m_executor;
   ActionRuntime m_runtime;

public:
   explicit BotRuntime (Bot &bot);

   void setMode (ControlMode mode) {
      m_runtime.setMode (mode);
   }

   ControlMode getMode () const {
      return m_runtime.getMode ();
   }

   void setPolicy (const Policy *policy) {
      m_runtime.setPolicy (policy);
   }

   const Policy *getPolicy () const {
      return m_runtime.getPolicy ();
   }

   bool isControlEnabled () const {
      return m_runtime.isControlEnabled ();
   }

   Controller &controller () {
      return m_runtime.controller ();
   }

   const Controller &controller () const {
      return m_runtime.controller ();
   }

   ActionState &actionState () {
      return m_runtime.actionState ();
   }

   const ActionState &actionState () const {
      return m_runtime.actionState ();
   }

   ActionResult step (const Observation &observation) {
      return m_runtime.step (observation);
   }

   bool cancel () {
      return m_runtime.cancel ();
   }

   void reset () {
      m_runtime.reset ();
   }

   bool isActive () const {
      return m_runtime.isActive ();
   }

   const Action &activeAction () const {
      return m_runtime.activeAction ();
   }

   const ActionResult &result () const {
      return m_runtime.result ();
   }
};

} // namespace ai
