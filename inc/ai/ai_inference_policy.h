//
// AiPB - inference-backed policy adapter.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_inference_provider.h>
#include <ai/ai_policy.h>

namespace ai {

// Adapts a backend-neutral InferenceProvider to the existing Policy contract.
// This keeps Controller and the action runtime independent from the model
// runtime and allows deterministic providers to be used in tests.
class InferencePolicy final : public Policy {
private:
   const InferenceProvider *m_provider {};

public:
   explicit InferencePolicy (const InferenceProvider *provider = nullptr)
      : m_provider (provider) {
   }

   void setProvider (const InferenceProvider *provider) {
      m_provider = provider;
   }

   const InferenceProvider *getProvider () const {
      return m_provider;
   }

   Action decide (const Observation &observation) const override {
      if (m_provider == nullptr) {
         return {};
      }

      return m_provider->infer (observation);
   }
};

} // namespace ai
