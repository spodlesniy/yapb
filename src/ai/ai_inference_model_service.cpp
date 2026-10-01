//
// AiPB - inference model service implementation.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_inference_model_service.h>

#include <cstdio>
#include <cstring>

namespace ai {

InferenceModelConfigureResult InferenceModelService::configure(const char *modelPath) {
  const char *requestedPath = modelPath != nullptr ? modelPath : "";

  if (m_configured && std::strcmp(m_modelPath, requestedPath) == 0) {
    return m_runner.isReady() ? InferenceModelConfigureResult::Unchanged : InferenceModelConfigureResult::Failed;
  }

  m_configured = true;
  m_modelPath[0] = '\0';
  m_runner.unload();

  if (*requestedPath == '\0') {
    return InferenceModelConfigureResult::Disabled;
  }

  if (std::strlen(requestedPath) >= sizeof(m_modelPath)) {
    std::snprintf(m_modelPath, sizeof(m_modelPath), "%s", requestedPath);
    return InferenceModelConfigureResult::Failed;
  }

  std::snprintf(m_modelPath, sizeof(m_modelPath), "%s", requestedPath);

  return m_runner.load(m_modelPath) ? InferenceModelConfigureResult::Loaded : InferenceModelConfigureResult::Failed;
}

void InferenceModelService::reset() {
  m_configured = false;
  m_modelPath[0] = '\0';
  m_runner.unload();
}

InferenceModelService &getInferenceModelService() {
  static InferenceModelService service {};
  return service;
}

} // namespace ai
