//
// AiPB - inference model service unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
//
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_inference_model_service.h>

#include <cstring>

using ai::test::expect;

AI_TEST(testInferenceModelServiceStartsUnavailable) {
  ai::InferenceModelService service {};

  expect(!service.isReady(), "model service starts without a loaded model");
  expect(service.getProvider() == nullptr, "model service exposes no provider without a loaded model");
}

AI_TEST(testInferenceModelServiceDisablesEmptyPath) {
  ai::InferenceModelService service {};

  const auto result = service.configure("");

  expect(result == ai::InferenceModelConfigureResult::Disabled, "empty model path disables inference");
  expect(!service.isReady(), "empty model path keeps inference unavailable");
  expect(service.getProvider() == nullptr, "disabled model service exposes no provider");
}

AI_TEST(testInferenceModelServiceRejectsMissingModel) {
  ai::InferenceModelService service {};

  const auto result = service.configure("__aipb_missing_model__.onnx");

  expect(result == ai::InferenceModelConfigureResult::Failed, "missing model path reports a failed configuration");
  expect(!service.isReady(), "failed model configuration keeps service inactive");
  expect(service.getProvider() == nullptr, "failed model configuration exposes no provider");
  expect(service.getLastError() != nullptr && std::strlen(service.getLastError()) > 0, "failed configuration keeps the backend error");
}

AI_TEST(testInferenceModelServiceDoesNotReloadUnchangedPath) {
  ai::InferenceModelService service {};

  const auto first = service.configure("__aipb_missing_model__.onnx");
  const auto second = service.configure("__aipb_missing_model__.onnx");

  expect(first == ai::InferenceModelConfigureResult::Failed, "first missing model configuration fails");
  expect(second == ai::InferenceModelConfigureResult::Failed, "unchanged failed configuration remains failed without reloading");
}

AI_TEST(testInferenceModelServiceReloadsWhenPathChanges) {
  ai::InferenceModelService service {};

  const auto first = service.configure("__aipb_missing_model_a__.onnx");
  const auto second = service.configure("__aipb_missing_model_b__.onnx");

  expect(first == ai::InferenceModelConfigureResult::Failed, "first missing model configuration fails");
  expect(second == ai::InferenceModelConfigureResult::Failed, "changed missing model path triggers a new load attempt");
}

AI_TEST(testInferenceModelServiceKeepsDisabledStateIdempotent) {
  ai::InferenceModelService service {};

  const auto first = service.configure ("");
  const auto second = service.configure ("");

  expect (first == ai::InferenceModelConfigureResult::Disabled, "first empty model configuration disables inference");
  expect (second == ai::InferenceModelConfigureResult::Disabled, "repeated empty model configuration remains disabled");
}

AI_TEST(testInferenceModelServiceResetDisablesLoadedState) {
  ai::InferenceModelService service {};

  const auto configureResult = service.configure ("__aipb_missing_model__.onnx");
  service.reset ();

  expect (configureResult == ai::InferenceModelConfigureResult::Failed, "reset test starts from a failed model load");
  expect (!service.isReady (), "reset leaves the model service unavailable");
  expect (service.getProvider () == nullptr, "reset removes the inference provider");
  expect (service.configure ("") == ai::InferenceModelConfigureResult::Disabled,
          "empty configuration after reset remains explicitly disabled");
}

AI_TEST(testInferenceModelServiceDefaultModelPath) {
  expect(std::strcmp(ai::kDefaultInferenceModelPath, "addons/yapb/data/models/aipb_policy.onnx") == 0,
         "default inference model path matches the package model location");
}
