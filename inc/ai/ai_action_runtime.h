//
// AiPB - AI action runtime.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action_loop.h>

namespace ai {

// Owns the engine-independent AI action runtime.
// The runtime coordinates controller, action state, validation, and execution
// without depending on GoldSrc or a concrete bot implementation.
class ActionRuntime final {
private:
  Controller m_controller {};
  ActionState m_actionState {};
  ActionPipeline m_pipeline;
  ActionLoop m_loop;

public:
  explicit ActionRuntime(ActionExecutor &executor);

  ActionRuntime(const ActionRuntime &) = delete;
  ActionRuntime &operator=(const ActionRuntime &) = delete;
  ActionRuntime(ActionRuntime &&) = delete;
  ActionRuntime &operator=(ActionRuntime &&) = delete;

  void setMode(ControlMode mode);
  ControlMode getMode() const;

  void setPolicy(const Policy *policy);
  const Policy *getPolicy() const;

  bool isControlEnabled() const;

  Controller &controller();
  const Controller &controller() const;

  ActionState &actionState();
  const ActionState &actionState() const;

  ActionResult step(const Observation &observation, bool allowDecision = true);

  bool cancel();
  void reset();

  bool isActive() const;
  const Action &activeAction() const;
  const ActionResult &result() const;
};

} // namespace ai
