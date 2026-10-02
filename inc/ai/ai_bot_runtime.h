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
#include <ai/ai_reward_provider.h>
#include <ai/ai_training_collector.h>
#include <ai/ai_training_recorder.h>

namespace ai {

// Binds the engine-independent action runtime to a live YaPB bot.
// Runtime mode and policy selection remain controlled through ActionRuntime.
// The runtime is intentionally dormant while the controller stays in Legacy mode.
class BotRuntime final {
private:
  GoalNavigationPolicy m_goalNavigationPolicy {};
  InferencePolicy m_inferencePolicy {};
  TrainingRecorder m_trainingRecorder {};
  ZeroRewardProvider m_zeroRewardProvider {};
  TrainingCollector m_trainingCollector;
  BotActionExecutor m_executor;
  ActionRuntime m_runtime;

public:
  explicit BotRuntime(Bot &bot);

  void setMode(ControlMode mode) {
    const auto previousMode = m_runtime.getMode();
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
      // Training uses the deterministic navigation policy as its behavior source.
      // TrainingCollector records the resulting action lifecycle separately.
      m_runtime.setPolicy(&m_goalNavigationPolicy);
      if (previousMode != ControlMode::Training) {
        beginTrainingEpisode();
      }
      break;
    }

    if (previousMode == ControlMode::Training && mode != ControlMode::Training) {
      m_trainingCollector.endEpisode();
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

  void beginTrainingEpisode() {
    if (m_runtime.getMode() != ControlMode::Training) {
      return;
    }

    m_trainingRecorder.beginEpisode();
  }

  void setRewardProvider(const RewardProvider *provider) {
    m_trainingCollector.setRewardProvider(provider != nullptr ? *provider : m_zeroRewardProvider);
  }

  TrainingRecorder &trainingRecorder() {
    return m_trainingRecorder;
  }

  const TrainingRecorder &trainingRecorder() const {
    return m_trainingRecorder;
  }

  TrainingBuffer &trainingBuffer() {
    return m_trainingRecorder.buffer();
  }

  const TrainingBuffer &trainingBuffer() const {
    return m_trainingRecorder.buffer();
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
    return m_trainingCollector.step(m_runtime, observation, allowDecision);
  }

  bool cancel(const Observation &nextObservation) {
    return m_trainingCollector.cancel(m_runtime, nextObservation);
  }

  void reset() {
    m_runtime.reset();
    m_trainingCollector.reset();
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
