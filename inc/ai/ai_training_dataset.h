//
// AiPB - training dataset writer.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstddef>
#include <cstdint>

#include <ai/ai_training_sample.h>

namespace ai {

constexpr uint32_t kTrainingDatasetFormatVersion = 3;
constexpr const char kTrainingDatasetDirectory[] = "training";
constexpr const char kTrainingDatasetFilePrefix[] = "ai_training";

enum class TrainingDatasetWriteError : uint8_t {
  None,
  InvalidPath,
  IoError,
  InvalidTransition,
  UnsupportedAction,
};

struct TrainingDatasetWriteResult {
  size_t count {};
  size_t combatEventCount {};
  size_t navigationEventCount {};
  size_t defuseEventCount {};
  size_t aimEventCount {};
  size_t failedIndex {};
  TrainingDatasetWriteError error { TrainingDatasetWriteError::None };

  bool isValid() const {
    return error == TrainingDatasetWriteError::None;
  }
};

TrainingDatasetWriteResult writeTrainingDataset(const TrainingBuffer &buffer, const char *filePath);

} // namespace ai
