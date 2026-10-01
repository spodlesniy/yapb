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
// intentionally converted to a no-op action so the inference boundary cannot
// inject undefined behavior into the action runtime.
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

      const InferenceInput input { kInferenceInputSchemaVersion, observation };
      if (!input.hasSupportedSchema ()) {
         return {};
      }

      const InferenceResult result = m_provider->infer (input);

      if (!result.isSuccess ()) {
         return {};
      }

      const auto decoded = decodeInferenceAction (result.output, observation);
      if (!decoded.isValid ()) {
         return {};
      }

      return decoded.action;
   }
};

} // namespace ai
