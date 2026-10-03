//
// AiPB - ONNX Runtime model runner.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_inference_model_contract.h>
#include <ai/ai_inference_model_runner.h>

namespace ai {

// First concrete ML backend for AiPB.
//
// The ONNX Runtime dependency is optional at build time. When disabled,
// the runner remains a safe unavailable backend and does not change normal
// YaPB/AiPB behavior.
class OnnxModelRunner final : public InferenceModelRunner {
private:
  struct Impl;
  Impl *m_impl {};

public:
  OnnxModelRunner();
  ~OnnxModelRunner() override;

  OnnxModelRunner(const OnnxModelRunner &) = delete;
  OnnxModelRunner &operator=(const OnnxModelRunner &) = delete;
  OnnxModelRunner(OnnxModelRunner &&) = delete;
  OnnxModelRunner &operator=(OnnxModelRunner &&) = delete;

  bool load(const char *modelPath, const char *inputName = kInferenceModelInputName, const char *outputName = kInferenceModelOutputName);
  void unload();

  bool isReady() const;
  const char *getLastError() const;

  InferenceResult run(const InferenceFeatures &features) const override;
};

} // namespace ai
