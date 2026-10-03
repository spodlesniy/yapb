//
// AiPB - ONNX Runtime model runner unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_onnx_model_runner.h>

#include <cstring>

using ai::test::expect;

AI_TEST(testOnnxModelRunnerUnavailableWithoutRuntime) {
  ai::OnnxModelRunner runner {};

  expect(!runner.isReady(), "ONNX runner is unavailable until a model is loaded");
  expect(runner.getLastError() != nullptr, "ONNX runner exposes an error string");
}

AI_TEST(testOnnxModelRunnerRejectsMissingModel) {
  ai::OnnxModelRunner runner {};

  expect(!runner.load("__aipb_missing_model__.onnx"), "missing ONNX model is rejected");
  expect(!runner.isReady(), "failed ONNX model load leaves runner inactive");
  expect(runner.getLastError() != nullptr && std::strlen(runner.getLastError()) > 0, "failed model load records an error");
}

AI_TEST(testOnnxModelRunnerCanBeUnloaded) {
  ai::OnnxModelRunner runner {};
  runner.unload();

  expect(!runner.isReady(), "unloaded ONNX runner is inactive");
}

#if defined(AIPB_WITH_ONNXRUNTIME)

AI_TEST(testOnnxModelRunnerRejectsMismatchedReferenceModelNames) {
  ai::OnnxModelRunner runner {};

  const char *modelPath = AIPB_TEST_SOURCE_ROOT "/tests/data/aipb_reference_model.onnx";

  expect(!runner.load(modelPath, "wrong_input", "output"), "mismatched input name is rejected");
  expect(!runner.isReady(), "input name rejection leaves runner inactive");

  expect(!runner.load(modelPath, "input", "wrong_output"), "mismatched output name is rejected");
  expect(!runner.isReady(), "output name rejection leaves runner inactive");
}

AI_TEST(testOnnxModelRunnerLoadsReferenceModel) {
  ai::OnnxModelRunner runner {};

  const bool loaded = runner.load (AIPB_TEST_SOURCE_ROOT "/tests/data/aipb_reference_model.onnx");

  expect (loaded, "ONNX Runtime loads the AiPB reference model");
  expect (runner.isReady (), "reference model leaves the ONNX runner ready");

  if (!loaded) {
    return;
  }

  ai::InferenceFeatures features {};
  const auto result = runner.run (features);

  expect (result.status == ai::InferenceStatus::Success, "reference model inference succeeds");
  expect (result.output.actionId == static_cast<uint8_t> (ai::InferenceActionId::MoveToNode),
     "reference model returns the expected action id");
  expect (result.output.targetNode == 42, "reference model returns the expected target node");
  expectNear (result.output.confidence, 0.95f, 0.0001f, "reference model returns the expected confidence");
}

#endif
