//
// AiPB - inference model service.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_inference_provider.h>
#include <ai/ai_onnx_model_runner.h>

#include <cstdint>

namespace ai {

constexpr const char kDefaultInferenceModelPath[] = "addons/yapb/data/models/aipb_policy.onnx";

enum class InferenceModelConfigureResult : uint8_t {
  Unchanged,
  Disabled,
  Loaded,
  Failed,
};

// Owns the process-wide inference model instance used by live bots.
//
// Model loading is deliberately centralized here: a server may have many
// bots, but all of them must share the same model session.
class InferenceModelService final {
private:
  OnnxModelRunner m_runner {};
  ModelInferenceProvider m_provider { &m_runner };
  bool m_configured {};
  char m_modelPath[1024] {};

public:
  InferenceModelConfigureResult configure(const char *modelPath);

  void reset();

  bool isReady() const {
    return m_runner.isReady();
  }

  const char *getLastError() const {
    return m_runner.getLastError();
  }

  const InferenceProvider *getProvider() const {
    return m_runner.isReady() ? static_cast<const InferenceProvider *>(&m_provider) : nullptr;
  }
};

InferenceModelService &getInferenceModelService();

} // namespace ai
