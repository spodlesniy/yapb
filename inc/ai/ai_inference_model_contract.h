//
// AiPB - inference model tensor contract.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ai {

constexpr const char kInferenceModelInputName[] = "input";
constexpr const char kInferenceModelOutputName[] = "output";

inline bool inferenceModelNameMatches(const char *actualName, const char *expectedName) {
  return actualName != nullptr && expectedName != nullptr && std::strcmp(actualName, expectedName) == 0;
}

enum class InferenceTensorValidationError : uint8_t {
  None,
  NotTensor,
  WrongElementType,
  WrongRank,
  WrongBatch,
  WrongWidth,
};

struct InferenceTensorMetadata {
  bool isTensor {};
  bool isFloat32 {};
  size_t rank {};
  int64_t batch {};
  int64_t width {};
};

constexpr InferenceTensorValidationError validateInferenceTensor(const InferenceTensorMetadata &metadata, size_t expectedWidth) {
  if (!metadata.isTensor) {
    return InferenceTensorValidationError::NotTensor;
  }

  if (!metadata.isFloat32) {
    return InferenceTensorValidationError::WrongElementType;
  }

  if (metadata.rank != 2) {
    return InferenceTensorValidationError::WrongRank;
  }

  if (metadata.batch != 1) {
    return InferenceTensorValidationError::WrongBatch;
  }

  if (metadata.width != static_cast<int64_t>(expectedWidth)) {
    return InferenceTensorValidationError::WrongWidth;
  }

  return InferenceTensorValidationError::None;
}

} // namespace ai
