//
// AiPB - inference model tensor contract unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_inference_action.h>
#include <ai/ai_inference_features.h>
#include <ai/ai_inference_model_contract.h>

using ai::test::expect;

AI_TEST(testInferenceTensorContract) {
  const ai::InferenceTensorMetadata valid { true, true, 2, 1, static_cast<int64_t>(ai::kInferenceFeatureCount) };

  expect(ai::validateInferenceTensor(valid, ai::kInferenceFeatureCount) == ai::InferenceTensorValidationError::None,
         "valid input tensor contract is accepted");
}

AI_TEST(testInferenceTensorContractRejectsInvalidType) {
  auto metadata = ai::InferenceTensorMetadata { true, false, 2, 1, 10 };

  expect(ai::validateInferenceTensor(metadata, 10) == ai::InferenceTensorValidationError::WrongElementType,
         "non-float32 tensor is rejected");

  metadata.isTensor = false;
  metadata.isFloat32 = true;

  expect(ai::validateInferenceTensor(metadata, 10) == ai::InferenceTensorValidationError::NotTensor, "non-tensor model value is rejected");
}

AI_TEST(testInferenceTensorContractRejectsShape) {
  auto metadata = ai::InferenceTensorMetadata { true, true, 3, 1, 10 };
  expect(ai::validateInferenceTensor(metadata, 10) == ai::InferenceTensorValidationError::WrongRank, "wrong tensor rank is rejected");

  metadata.rank = 2;
  metadata.batch = 2;
  expect(ai::validateInferenceTensor(metadata, 10) == ai::InferenceTensorValidationError::WrongBatch, "non-singleton batch is rejected");

  metadata.batch = 1;
  metadata.width = 11;
  expect(ai::validateInferenceTensor(metadata, 10) == ai::InferenceTensorValidationError::WrongWidth, "wrong tensor width is rejected");
}

AI_TEST(testInferenceModelContractConstants) {
  expect(ai::kInferenceActionTensorSize == 10, "model output tensor size is fixed at ten values");
  expect(ai::kInferenceFeatureCount > 0, "model input tensor has a fixed positive width");
}
