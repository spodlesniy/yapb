//
// AiPB - inference-backed policy adapter.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_inference_action_decoder.h>
#include <ai/ai_inference_provider.h>
#include <ai/ai_policy.h>

namespace ai {

// Adapts a backend-neutral InferenceProvider to the existing Policy contract.
// Provider errors, incompatible schemas, and invalid model actions are
// converted to a no-op action unless an explicit fallback policy is configured.
// The inference boundary cannot inject undefined behavior into the action runtime.
class InferencePolicy final : public Policy {
private:
  const InferenceProvider *m_provider {};
  const Policy *m_fallbackPolicy {};

public:
  explicit InferencePolicy(const InferenceProvider *provider = nullptr) : m_provider(provider) {
  }

  void setProvider(const InferenceProvider *provider) {
    m_provider = provider;
  }

  const InferenceProvider *getProvider() const {
    return m_provider;
  }

  void setFallbackPolicy(const Policy *policy) {
    m_fallbackPolicy = policy;
  }

  const Policy *getFallbackPolicy() const {
    return m_fallbackPolicy;
  }

  Action decide(const Observation &observation) const override {
    const auto fallback = [&]() {
      return m_fallbackPolicy != nullptr ? m_fallbackPolicy->decide(observation) : Action {};
    };

    if (m_provider == nullptr) {
      return fallback();
    }

    const InferenceInput input { kInferenceInputSchemaVersion, observation };
    if (!input.hasSupportedSchema()) {
      return fallback();
    }

    const InferenceResult result = m_provider->infer(input);

    if (!result.isSuccess()) {
      return fallback();
    }

    const auto decoded = decodeInferenceAction(result.output, observation);
    if (!decoded.isValid()) {
      return fallback();
    }

    return decoded.action;
  }
};

} // namespace ai
