//
// AiPB - Bot AI runtime.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_bot_adapter.h>
#include <ai/ai_bot_runtime.h>

namespace ai {

BotRuntime::BotRuntime(Bot &bot)
  : m_bot(&bot), m_trainingCollector(m_trainingRecorder, m_defaultRewardProvider), m_executionContext(bot),
    m_executor(m_executionContext), m_runtime(m_executor) {
  m_runtime.setPolicy(&m_goalNavigationPolicy);
}

Observation BotRuntime::captureObservation() const {
  if (m_bot == nullptr) {
    return {};
  }

  return buildObservation(buildObservationInput(*m_bot));
}

} // namespace ai
