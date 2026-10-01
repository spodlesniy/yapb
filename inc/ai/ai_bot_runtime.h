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

namespace ai {

// Owns the AI control pipeline for a single live YaPB bot.
// Runtime mode and policy selection remain controlled through Controller.
// The runtime is intentionally dormant while the controller stays in Legacy mode.
class BotRuntime final {
private:
   Controller m_controller {};
   ActionState m_actionState {};
   BotActionExecutor m_executor;
   ActionPipeline m_pipeline;
   ActionLoop m_loop;

public:
   explicit BotRuntime (Bot &bot);

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
