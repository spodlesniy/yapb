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
#include <ai/ai_yapb_action_execution_context.h>
#include <ai/ai_goal_navigation_policy.h>
#include <ai/ai_inference_policy.h>
#include <ai/ai_model_io.h>
#include <ai/ai_model_io_recovery.h>
#include <ai/ai_server_state_adapter.h>
#include <ai/ai_perception_adapter.h>
#include <ai/ai_mock_policy.h>
#include <ai/ai_mock_motion.h>
#include <ai/ai_execution_ownership_adapter.h>
#include <ai/ai_reward_provider.h>
#include <ai/ai_training_collector.h>
#include <ai/ai_training_dataset.h>
#include <ai/ai_training_recorder.h>

namespace ai {

// Binds the engine-independent action runtime to a live YaPB bot.
// Runtime mode and policy selection remain controlled through ActionRuntime.
// The runtime is intentionally dormant while the controller stays in Legacy mode.
class BotRuntime final {
private:
  Bot *m_bot {};
  GoalNavigationPolicy m_goalNavigationPolicy {};
  MockMovePolicy m_mockPolicy {};
  bool m_mockMoveActive {};
  bool m_modelIOTrialActive {};
  bool m_modelIOTrialFallbackEnabled {};
  const InferenceProvider *m_modelIOTrialOriginalProvider {};
  runtime_io::ModelIORecoveryReport m_lastModelIORecovery {};
  Vec3 m_modelIOTrialTarget {};
  ActionResult m_mockMoveResult {};
  Vec3 m_mockTarget {};
  float m_mockStartedAt { -1.0f };
  InferencePolicy m_inferencePolicy {};
  runtime_io::ModelIOAdapter m_modelIOAdapter {};
  const InferenceProvider *m_standardInferenceProvider {};
  TrainingRecorder m_trainingRecorder {};
  ActionOutcomeRewardProvider m_defaultRewardProvider {};
  ZeroRewardProvider m_zeroRewardProvider {};
  // ActionOutcomeRewardProvider is the default training reward source.
  // ZeroRewardProvider remains available through an explicit nullptr override.
  TrainingCollector m_trainingCollector;
  YaPBActionExecutionContext m_executionContext;
  BotActionExecutor m_executor;
  ActionRuntime m_runtime;

  Observation captureObservation() const;
  int botSlot() const;

public:
  explicit BotRuntime(Bot &bot);

  void setMode(ControlMode mode, const Observation &) {
    if (m_mockMoveActive && mode != ControlMode::Neural) {
      cancelMockMove();
    }
    if (m_modelIOTrialActive && mode != ControlMode::Neural) {
      stopModelIOTrial();
    }
    const auto previousMode = m_runtime.getMode();
    if (mode == previousMode) {
      return;
    }

    if ((previousMode == ControlMode::Neural || previousMode == ControlMode::Training) && m_runtime.isActive()) {
      if (m_trainingCollector.cancel(m_runtime)) {
        if (m_bot != nullptr) {
          runtime_io::recordBotActionResult(*m_bot, m_runtime.result());
        }
        m_trainingCollector.finalizeTerminal(captureObservation());
      }
    }

    m_runtime.setMode(mode);

    switch (mode) {
    case ControlMode::Legacy:
      m_runtime.setPolicy(&m_goalNavigationPolicy);
      break;

    case ControlMode::Neural:
      m_runtime.setPolicy(m_mockMoveActive ? static_cast<const Policy *>(&m_mockPolicy) :
          (m_inferencePolicy.getProvider() != nullptr || m_inferencePolicy.getFallbackPolicy() != nullptr)
              ? static_cast<const Policy *>(&m_inferencePolicy) : nullptr);
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
    if (!m_mockMoveActive) {
      m_runtime.setPolicy(policy);
    }
  }

  // Only an explicit single-bot server-console trial can enable the mock.
  bool armMockMove(const Vec3 &target) {
    if (m_mockMoveActive || m_modelIOTrialActive || !mockTrialModeAllowed(m_runtime.getMode()) ||
        !m_mockPolicy.arm(target)) {
      return false;
    }
    // Preempt a previous Neural action before replacing its policy.
    if (m_runtime.isActive() && m_trainingCollector.cancel(m_runtime)) {
      if (m_bot != nullptr) {
        runtime_io::recordBotActionResult(*m_bot, m_runtime.result());
      }
      m_trainingCollector.finalizeTerminal(captureObservation());
    }
    if (m_runtime.isActive()) {
      m_mockPolicy.reset();
      return false;
    }
    m_mockTarget = target;
    m_mockStartedAt = -1.0f;
    m_mockMoveResult = {};
    m_mockMoveActive = true;
    m_runtime.setPolicy(&m_mockPolicy);
    return true;
  }

  bool mockMoveActive() const {
    return m_mockMoveActive;
  }

  // D323 uses the same direct physical completion path, but never the Mock
  // policy: the trial's action must originate from ModelIOTestBackend.
  bool modelIOTrialActive() const {
    return m_modelIOTrialActive;
  }

  Vec3 modelIOTrialTarget() const {
    return m_modelIOTrialTarget;
  }

  bool armModelIOTrial(const runtime_io::ModelIOBackend *backend, const Vec3 &target) {
    if (m_mockMoveActive || m_modelIOTrialActive || !mockTrialModeAllowed(m_runtime.getMode()) ||
        backend == nullptr || !runtime_io::finiteVector(target)) {
      return false;
    }
    if (m_runtime.isActive() && m_trainingCollector.cancel(m_runtime)) {
      if (m_bot != nullptr) {
        runtime_io::recordBotActionResult(*m_bot, m_runtime.result());
      }
      m_trainingCollector.finalizeTerminal(captureObservation());
    }
    if (m_runtime.isActive() || !setModelIOBackend(backend)) {
      return false;
    }
    m_modelIOTrialFallbackEnabled = isInferenceFallbackEnabled();
    m_modelIOTrialOriginalProvider = m_standardInferenceProvider;
    m_lastModelIORecovery = {};
    setInferenceFallbackEnabled(false);
    m_modelIOTrialActive = true;
    m_modelIOTrialTarget = target;
    return true;
  }

  runtime_io::ModelIORecoveryReport stopModelIOTrial() {
    if (!m_modelIOTrialActive) {
      return m_lastModelIORecovery;
    }
    if (m_runtime.isActive() && m_trainingCollector.cancel(m_runtime)) {
      if (m_bot != nullptr) {
        runtime_io::recordBotActionResult(*m_bot, m_runtime.result());
      }
      m_trainingCollector.finalizeTerminal(captureObservation());
    }
    m_modelIOTrialActive = false;
    m_modelIOTrialTarget = {};
    // Detach the trial while the policy is idle. Do not count the model's
    // own availability as recovery: this verifies only provider identity.
    const bool detached = setModelIOBackend(nullptr);
    setInferenceFallbackEnabled(m_modelIOTrialFallbackEnabled);
    m_lastModelIORecovery = runtime_io::verifyModelIORecovery(
        m_modelIOTrialOriginalProvider, m_inferencePolicy.getProvider(),
        m_modelIOTrialFallbackEnabled, isInferenceFallbackEnabled(),
        getModelIOBackend() != nullptr, m_runtime.isActive(), m_modelIOTrialActive);
    if (!detached) {
      m_lastModelIORecovery.backendReleased = false;
    }
    m_modelIOTrialOriginalProvider = nullptr;
    return m_lastModelIORecovery;
  }

  runtime_io::ModelIORecoveryReport lastModelIORecovery() const {
    return m_lastModelIORecovery;
  }

  Vec3 mockMoveTarget() const {
    return m_mockTarget;
  }

  void restoreNeuralPolicy() {
    if (m_runtime.getMode() == ControlMode::Neural) {
      const bool available = m_inferencePolicy.getProvider() != nullptr ||
                             m_inferencePolicy.getFallbackPolicy() != nullptr;
      m_runtime.setPolicy(available ? static_cast<const Policy *>(&m_inferencePolicy) : nullptr);
    }
  }

  const ActionResult &mockMoveResult() const {
    return m_mockMoveResult;
  }

  void cancelMockMove() {
    if (!m_mockMoveActive) {
      return;
    }
    if (m_runtime.isActive() && m_trainingCollector.cancel(m_runtime)) {
      if (m_bot != nullptr) {
        runtime_io::recordBotActionResult(*m_bot, m_runtime.result());
      }
      m_trainingCollector.finalizeTerminal(captureObservation());
      m_mockMoveResult = m_runtime.result();
    }
    else {
      m_mockMoveResult = { ActionType::MoveToPosition, ActionResultType::Interrupted, 0.0f };
    }
    m_mockMoveActive = false;
    m_mockPolicy.reset();
    m_mockStartedAt = -1.0f;
    restoreNeuralPolicy();
  }

  const Policy *getPolicy() const {
    return m_runtime.getPolicy();
  }

  // Backend overrides the Neural inference source only when explicitly set;
  // normal ONNX and fallback selection continue to work unchanged by default.
  bool setModelIOBackend(const runtime_io::ModelIOBackend *backend) {
    if (m_runtime.isActive() || m_mockMoveActive ||
        (m_modelIOTrialActive && backend != nullptr)) {
      return false;
    }
    m_modelIOAdapter.setBackend(backend);
    m_inferencePolicy.setProvider(backend != nullptr
        ? static_cast<const InferenceProvider *>(&m_modelIOAdapter) : m_standardInferenceProvider);
    if (m_runtime.getMode() == ControlMode::Neural) {
      restoreNeuralPolicy();
    }
    return true;
  }

  const runtime_io::ModelIOBackend *getModelIOBackend() const {
    return m_modelIOAdapter.getBackend();
  }

  void setInferenceProvider(const InferenceProvider *provider) {
    if (m_modelIOTrialActive) {
      return;
    }
    m_standardInferenceProvider = provider;
    m_inferencePolicy.setProvider(m_modelIOAdapter.getBackend() != nullptr
        ? static_cast<const InferenceProvider *>(&m_modelIOAdapter) : provider);
    if (m_runtime.getMode() == ControlMode::Neural && !m_mockMoveActive) {
      restoreNeuralPolicy();
    }
  }

  void setInferenceFallbackEnabled(bool enabled) {
    if (m_modelIOTrialActive) {
      return;
    }
    m_inferencePolicy.setFallbackPolicy(enabled ? static_cast<const Policy *>(&m_goalNavigationPolicy) : nullptr);
    if (m_runtime.getMode() == ControlMode::Neural && !m_mockMoveActive) {
      restoreNeuralPolicy();
    }
  }

  bool isInferenceFallbackEnabled() const {
    return m_inferencePolicy.getFallbackPolicy() != nullptr;
  }

  const InferenceProvider *getInferenceProvider() const {
    return m_inferencePolicy.getProvider();
  }

  void beginTrainingEpisode() {
    if (m_runtime.getMode() != ControlMode::Training) {
      return;
    }

    if (m_trainingCollector.hasPendingTerminal()) {
      m_trainingCollector.finalizeTerminal(captureObservation());
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

  TrainingDatasetWriteResult saveTrainingDataset(const char *filePath) const {
    return writeTrainingDataset(m_trainingRecorder.buffer(), filePath);
  }

  bool isControlEnabled() const {
    return m_runtime.isControlEnabled();
  }

  bool suppressesLegacyTaskExecution() const {
    return m_executor.suppressesLegacyTaskExecution();
  }

  bool isActiveActionStillOwned() const {
    return !m_runtime.isActive() || m_executor.isActionStillOwned(m_runtime.activeAction());
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
    if (m_runtime.getMode() == ControlMode::Neural &&
        !m_mockMoveActive && m_modelIOAdapter.getBackend() != nullptr) {
      runtime_io::ServerBotSnapshot self {};
      runtime_io::PerceptionFrame perception {};
      runtime_io::ControlFrame ownership {};
      const bool available = m_bot != nullptr &&
          runtime_io::readServerBotSnapshot(botSlot(), self) &&
          runtime_io::readPerceptionFrame(botSlot(), perception) &&
          runtime_io::readControlFrame(botSlot(), ownership);
      // Invalid frames cannot be reused: explicitly make the adapter unready.
      if (!available || !m_modelIOAdapter.bindFrame(self, perception, ownership)) {
        m_modelIOAdapter.invalidateFrame();
        // An invalid frame cannot authorize another model or fallback decision.
        allowDecision = false;
      }
    }
    ActionResult result = m_trainingCollector.step(m_runtime, observation, allowDecision);
    if (m_mockMoveActive && result.action == ActionType::MoveToPosition &&
        (result.type == ActionResultType::Accepted || result.isTerminal())) {
      if (m_mockStartedAt < 0.0f) {
        m_mockStartedAt = observation.gameTime;
      }
      result.elapsedTime = observation.gameTime >= m_mockStartedAt
          ? observation.gameTime - m_mockStartedAt : 0.0f;
    }
    if (m_bot != nullptr) {
      runtime_io::recordBotActionResult(*m_bot, result);
    }
    if (result.isTerminal()) {
      m_trainingCollector.finalizeTerminal(captureObservation());
    }
    if (m_mockMoveActive && result.action == ActionType::MoveToPosition &&
        (result.type == ActionResultType::Accepted || result.isTerminal())) {
      m_mockMoveResult = result;
      if (result.isTerminal()) {
        m_mockMoveActive = false;
        m_mockPolicy.reset();
        m_mockStartedAt = -1.0f;
        restoreNeuralPolicy();
      }
    }
    return result;
  }

  bool cancel(const Observation &) {
    if (!m_trainingCollector.cancel(m_runtime)) {
      return false;
    }
    if (m_bot != nullptr) {
      runtime_io::recordBotActionResult(*m_bot, m_runtime.result());
    }

    m_trainingCollector.finalizeTerminal(captureObservation());
    return true;
  }

  void reset() {
    cancelMockMove();
    stopModelIOTrial();
    if (m_trainingCollector.hasPendingTerminal()) {
      m_trainingCollector.finalizeTerminal(captureObservation());
    }

    m_runtime.reset();
    m_trainingCollector.reset();
    m_modelIOAdapter.reset();
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
