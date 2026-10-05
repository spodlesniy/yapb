//
// AiPB - ONNX Runtime model runner unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "ai_test.h"

#include <ai/ai_onnx_model_runner.h>

using ai::test::expect;
using ai::test::expectNear;

namespace {

bool readBinaryFile(const char *path, std::vector<uint8_t> &data) {
  std::FILE *file = std::fopen(path, "rb");
  if (file == nullptr) {
    return false;
  }

  if (std::fseek(file, 0, SEEK_END) != 0) {
    std::fclose(file);
    return false;
  }

  const long fileSize = std::ftell(file);
  if (fileSize <= 0 || std::fseek(file, 0, SEEK_SET) != 0) {
    std::fclose(file);
    return false;
  }

  data.resize(static_cast<size_t>(fileSize));
  const size_t bytesRead = std::fread(data.data(), 1, data.size(), file);
  std::fclose(file);

  if (bytesRead != data.size()) {
    data.clear();
    return false;
  }

  return true;
}

bool hasReferenceModelInputWidth(const std::vector<uint8_t> &modelData, size_t expectedWidth) {
  const uint8_t inputShapePrefix[] = {
    0x0a, 0x05, 'i', 'n', 'p', 'u', 't', 0x12, 0x0f, 0x0a, 0x0d, 0x08, 0x01, 0x12, 0x09, 0x0a, 0x02, 0x08,
    0x01, 0x0a, 0x03, 0x08
  };
  uint8_t encodedWidth[10] {};
  size_t encodedWidthSize = 0;
  size_t value = expectedWidth;

  do {
    encodedWidth[encodedWidthSize++] = static_cast<uint8_t>(value & 0x7fU);
    value >>= 7;
  } while (value != 0 && encodedWidthSize < sizeof(encodedWidth));

  if (value != 0) {
    return false;
  }

  for (size_t offset = 0; offset + sizeof(inputShapePrefix) + encodedWidthSize <= modelData.size(); ++offset) {
    bool matches = true;
    for (size_t index = 0; index < sizeof(inputShapePrefix); ++index) {
      if (modelData[offset + index] != inputShapePrefix[index]) {
        matches = false;
        break;
      }
    }

    if (!matches) {
      continue;
    }

    for (size_t index = 0; index + 1 < encodedWidthSize; ++index) {
      encodedWidth[index] |= 0x80;
    }

    for (size_t index = 0; index < encodedWidthSize; ++index) {
      if (modelData[offset + sizeof(inputShapePrefix) + index] != encodedWidth[index]) {
        matches = false;
        break;
      }
    }

    if (matches) {
      return true;
    }
  }

  return false;
}

} // namespace

AI_TEST(testOnnxReferenceModelUsesCurrentInputFeatureWidth) {
  std::vector<uint8_t> modelData {};
  const char *modelPath = AIPB_TEST_SOURCE_ROOT "/tests/data/aipb_reference_model.onnx";

  const bool loaded = readBinaryFile(modelPath, modelData);
  expect(loaded, "reference ONNX model fixture is readable without ONNX Runtime");
  if (!loaded) {
    return;
  }

  // The regular runtime contract test is conditional on the optional ONNX Runtime dependency.
  // Validate the serialized fixture shape here so the CI test target cannot silently accept a stale model.
  expect(hasReferenceModelInputWidth(modelData, ai::kInferenceFeatureCount),
         "reference ONNX model stores the current AiPB input feature width");
}

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
