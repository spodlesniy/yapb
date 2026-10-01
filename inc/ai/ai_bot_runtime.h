//
// AiPB - Bot AI runtime.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action_loop.h>
#include <ai/ai_bot_action_executor.h>
#include <ai/ai_goal_navigation_policy.h>

namespace ai {

// Owns the AI control pipeline for a single live YaPB bot.
// Runtime mode and policy selection remain controlled through Controller.
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
   Controller m_controller {};
   ActionState m_actionState {};
   BotActionExecutor m_executor;
   ActionPipeline m_pipeline;
   ActionLoop m_loop;

public:
   explicit BotRuntime (Bot &bot);

   void setMode (ControlMode mode) {
      if (mode == m_controller.getMode ()) {
         return;
      }

      if (mode != ControlMode::Neural) {
         m_loop.cancel ();
      }

      m_controller.setMode (mode);
   }

   void setPolicy (const Policy *policy) {
      if (policy == m_controller.getPolicy ()) {
         return;
      }

      m_loop.cancel ();
      m_controller.setPolicy (policy);
   }

   bool isControlEnabled () const {
      return m_controller.getMode () == ControlMode::Neural
         && m_controller.getPolicy () != nullptr;
   }

   Controller &controller () {
      return m_controller;
   }

   const Controller &controller () const {
      return m_controller;
   }

   ActionState &actionState () {
      return m_actionState;
   }

   const ActionState &actionState () const {
      return m_actionState;
   }

   ActionResult step (const Observation &observation) {
      return m_loop.step (observation);
   }

   bool cancel () {
      return m_loop.cancel ();
   }

   void reset () {
      m_loop.reset ();
   }

   bool isActive () const {
      return m_loop.isActive ();
   }
};

} // namespace ai
