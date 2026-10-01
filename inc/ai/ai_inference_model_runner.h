//
// AiPB - AI model runner abstraction.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_inference_action.h>
#include <ai/ai_inference_features.h>
#include <ai/ai_inference_provider.h>

namespace ai {

// Model-runtime boundary.
//
// A runner receives only the fixed feature vector and returns primitive model
// output. A concrete backend such as ONNX Runtime implements this interface;
// it knows nothing about Observation, Bot, tasks, or Action validation.
class InferenceModelRunner {
public:
   virtual ~InferenceModelRunner () = default;

   virtual InferenceResult run (const InferenceFeatures &features) const = 0;
};

class ModelInferenceProvider final : public InferenceProvider {
private:
   const InferenceModelRunner *m_runner {};

public:
   explicit ModelInferenceProvider (const InferenceModelRunner *runner = nullptr)
      : m_runner (runner) {
   }

   void setRunner (const InferenceModelRunner *runner) {
      m_runner = runner;
   }

   const InferenceModelRunner *getRunner () const {
      return m_runner;
   }

   InferenceResult infer (const InferenceInput &input) const override {
      if (!input.hasSupportedSchema ()) {
         return { InferenceStatus::InvalidInput, {} };
      }

      if (m_runner == nullptr) {
         return { InferenceStatus::Error, {} };
      }

      const auto features = encodeInferenceFeatures (input.observation);
      return m_runner->run (features);
   }
};

} // namespace ai
