//
// AiPB - automatic training lifecycle collector.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action_runtime.h>
#include <ai/ai_reward_provider.h>
#include <ai/ai_training_recorder.h>

namespace ai {

// Observes ActionRuntime lifecycle transitions and turns them into training
// samples without coupling the recorder to engine or model implementation.
class TrainingCollector final {
private:
  TrainingRecorder *m_recorder {};
  const RewardProvider *m_rewardProvider {};

public:
  TrainingCollector(TrainingRecorder &recorder, const RewardProvider &rewardProvider)
    : m_recorder(&recorder), m_rewardProvider(&rewardProvider) {
  }

  void setRewardProvider(const RewardProvider &rewardProvider) {
    m_rewardProvider = &rewardProvider;
  }

  ActionResult step(ActionRuntime &runtime, const Observation &observation, bool allowDecision = true);

  bool cancel(ActionRuntime &runtime, const Observation &nextObservation);

  void endEpisode();

  void reset();
};

} // namespace ai
