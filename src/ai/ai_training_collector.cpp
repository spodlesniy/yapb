//
// AiPB - automatic training lifecycle collector.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_training_collector.h>

namespace ai {

ActionResult TrainingCollector::step(ActionRuntime &runtime, const Observation &observation, bool allowDecision) {
  const bool wasActive = runtime.isActive();
  const auto result = runtime.step(observation, allowDecision);

  const auto mode = runtime.getMode();
  const bool recordingMode = mode == ControlMode::Neural || mode == ControlMode::Training;

  if (!recordingMode) {
    return result;
  }

  bool ownsResult = wasActive;

  if (!wasActive && result.action != ActionType::None) {
    if (m_recorder->episodeId() == 0) {
      m_recorder->beginEpisode();
    }

    ownsResult = m_recorder->startAction(observation, runtime.activeAction());
  }

  if (ownsResult && result.isTerminal() && m_recorder->hasPendingAction()) {
    const float reward =
      m_rewardProvider->compute(m_recorder->pendingObservation(), m_recorder->pendingAction(), observation, result);
    m_recorder->finishAction(observation, result, reward);
  }

  return result;
}

bool TrainingCollector::cancel(ActionRuntime &runtime, const Observation &nextObservation) {
  if (!runtime.cancel()) {
    return false;
  }

  const auto mode = runtime.getMode();
  const bool recordingMode = mode == ControlMode::Neural || mode == ControlMode::Training;

  if (!recordingMode || !m_recorder->hasPendingAction()) {
    return true;
  }

  const auto &result = runtime.result();
  if (!result.isTerminal()) {
    return true;
  }

  const float reward =
    m_rewardProvider->compute(m_recorder->pendingObservation(), m_recorder->pendingAction(), nextObservation, result);
  m_recorder->finishAction(nextObservation, result, reward);
  return true;
}

void TrainingCollector::reset() {
  m_recorder->reset();
}

} // namespace ai
