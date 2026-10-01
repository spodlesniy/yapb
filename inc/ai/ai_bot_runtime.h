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
#include <ai/ai_inference_policy.h>

namespace ai {

// Binds the engine-independent action runtime to a live YaPB bot.
// Runtime mode and policy selection remain controlled through ActionRuntime.
// The runtime is intentionally dormant while the controller stays in Legacy mode.
class BotRuntime final {
private:
  GoalNavigationPolicy m_goalNavigationPolicy {};
  InferencePolicy m_inferencePolicy {};
  BotActionExecutor m_executor;
  ActionRuntime m_runtime;

public:
  explicit BotRuntime(Bot &bot);

  void setMode(ControlMode mode) {
    m_runtime.setMode(mode);

    switch (mode) {
    case ControlMode::Legacy:
      m_runtime.setPolicy(&m_goalNavigationPolicy);
      break;

    case ControlMode::Neural:
      m_runtime.setPolicy(m_inferencePolicy.getProvider() != nullptr
                              ? static_cast<const Policy *>(&m_inferencePolicy)
                              : nullptr);
      break;

    case ControlMode::Training:
      m_runtime.setPolicy(nullptr);
      break;
    }
  }

  ControlMode getMode() const {
    return m_runtime.getMode();
  }

  void setPolicy(const Policy *policy) {
    m_runtime.setPolicy(policy);
  }

  const Policy *getPolicy() const {
    return m_runtime.getPolicy();
  }

  void setInferenceProvider(const InferenceProvider *provider) {
    m_inferencePolicy.setProvider(provider);

    if (m_runtime.getMode() == ControlMode::Neural) {
      m_runtime.setPolicy(provider != nullptr ? static_cast<const Policy *>(&m_inferencePolicy) : nullptr);
    }
  }

  const InferenceProvider *getInferenceProvider() const {
    return m_inferencePolicy.getProvider();
  }

  bool isControlEnabled() const {
    return m_runtime.isControlEnabled();
  }

  Controller &controller() {
    return m_runtime.controller();
  }

  const Controller &controller() const {
    return m_runtime.controller();
  }

  ActionState &actionState() {
    return m_runtime.actionState();
  }

  const ActionState &actionState() const {
    return m_runtime.actionState();
  }

  ActionResult step(const Observation &observation, bool allowDecision = true) {
    return m_runtime.step(observation, allowDecision);
  }

  bool cancel() {
    return m_runtime.cancel();
  }

  void reset() {
    m_runtime.reset();
  }

  bool isActive() const {
    return m_runtime.isActive();
  }

  const Action &activeAction() const {
    return m_runtime.activeAction();
  }

  const ActionResult &result() const {
    return m_runtime.result();
  }
};

} // namespace ai
