//
// AiPB - training dataset writer implementation.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_training_dataset.h>

#include <cstdio>

#include <crlib/crlib.h>

namespace ai {
namespace {

bool writeString(cr::File &file, const char *value) {
  return file.puts(value);
}

bool writeFloat(cr::File &file, float value) {
  char buffer[64] {};
  std::snprintf(buffer, sizeof(buffer), "%.9g", static_cast<double>(value));
  return writeString(file, buffer);
}

bool writeInt(cr::File &file, int32_t value) {
  char buffer[32] {};
  std::snprintf(buffer, sizeof(buffer), "%d", value);
  return writeString(file, buffer);
}

bool writeUnsigned(cr::File &file, uint64_t value) {
  char buffer[32] {};
  std::snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
  return writeString(file, buffer);
}

bool writeByte(cr::File &file, uint8_t value) {
  char buffer[16] {};
  std::snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned int>(value));
  return writeString(file, buffer);
}

bool writeFloatArray(cr::File &file, const float *values, size_t count) {
  if (!writeString(file, "[")) {
    return false;
  }

  for (size_t i = 0; i < count; ++i) {
    if (i != 0 && !writeString(file, ",")) {
      return false;
    }
    if (!writeFloat(file, values[i])) {
      return false;
    }
  }

  return writeString(file, "]");
}

bool writeVec3(cr::File &file, const Vec3 &value) {
  const float values[3] = { value.x, value.y, value.z };
  return writeFloatArray(file, values, 3);
}

bool writeObservation(cr::File &file, const InferenceFeatures &observation) {
  return writeString(file, "{\"schema_version\":") &&
         writeUnsigned(file, observation.schemaVersion) &&
         writeString(file, ",\"values\":") &&
         writeFloatArray(file, observation.values, kInferenceFeatureCount) &&
         writeString(file, "}");
}

bool writeAction(cr::File &file, const InferenceActionOutput &action) {
  return writeString(file, "{\"schema_version\":") &&
         writeUnsigned(file, action.schemaVersion) &&
         writeString(file, ",\"action_id\":") &&
         writeByte(file, action.actionId) &&
         writeString(file, ",\"target_node\":") &&
         writeInt(file, action.targetNode) &&
         writeString(file, ",\"target_player\":") &&
         writeInt(file, action.targetPlayer) &&
         writeString(file, ",\"target_position\":") &&
         writeVec3(file, action.targetPosition) &&
         writeString(file, ",\"weapon_type\":") &&
         writeByte(file, action.weaponType) &&
         writeString(file, ",\"grenade_type\":") &&
         writeByte(file, action.grenadeType) &&
         writeString(file, ",\"duration\":") &&
         writeFloat(file, action.duration) &&
         writeString(file, ",\"confidence\":") &&
         writeFloat(file, action.confidence) &&
         writeString(file, "}");
}

bool writeSample(cr::File &file, const TrainingSample &sample) {
  return writeString(file, "{\"episode_id\":") &&
         writeUnsigned(file, sample.episodeId) &&
         writeString(file, ",\"observation\":") &&
         writeObservation(file, sample.observation) &&
         writeString(file, ",\"action\":") &&
         writeAction(file, sample.action) &&
         writeString(file, ",\"reward\":") &&
         writeFloat(file, sample.reward) &&
         writeString(file, ",\"next_observation\":") &&
         writeObservation(file, sample.nextObservation) &&
         writeString(file, ",\"result\":") &&
         writeByte(file, static_cast<uint8_t>(sample.result)) &&
         writeString(file, ",\"elapsed_time\":") &&
         writeFloat(file, sample.elapsedTime) &&
         writeString(file, ",\"terminal\":") &&
         writeString(file, sample.terminal ? "true" : "false") &&
         writeString(file, "}\n");
}

} // namespace

TrainingDatasetWriteResult writeTrainingDataset(const TrainingBuffer &buffer, const char *filePath) {
  TrainingDatasetWriteResult result {};

  if (filePath == nullptr || *filePath == '\0') {
    result.error = TrainingDatasetWriteError::InvalidPath;
    return result;
  }

  cr::File file { cr::StringRef(filePath), "wt" };
  if (!file) {
    result.error = TrainingDatasetWriteError::IoError;
    return result;
  }

  if (!writeString(file, "{\"format\":\"aipb-training-jsonl\",\"version\":") ||
      !writeUnsigned(file, kTrainingDatasetFormatVersion) ||
      !writeString(file, ",\"feature_schema_version\":") ||
      !writeUnsigned(file, kInferenceFeatureSchemaVersion) ||
      !writeString(file, ",\"action_schema_version\":") ||
      !writeUnsigned(file, kInferenceActionSchemaVersion) ||
      !writeString(file, ",\"type\":\"metadata\"}\n")) {
    result.error = TrainingDatasetWriteError::IoError;
    return result;
  }

  for (size_t i = 0; i < buffer.size(); ++i) {
    const auto encoded = encodeTrainingTransition(buffer.at(i));

    if (!encoded.isValid()) {
      result.failedIndex = i;
      result.error = encoded.error == TrainingSampleEncodeError::UnsupportedAction
                         ? TrainingDatasetWriteError::UnsupportedAction
                         : TrainingDatasetWriteError::InvalidTransition;
      return result;
    }

    if (!writeSample(file, encoded.sample)) {
      result.failedIndex = i;
      result.error = TrainingDatasetWriteError::IoError;
      return result;
    }

    result.count = i + 1;
  }

  if (!file.flush()) {
    result.error = TrainingDatasetWriteError::IoError;
    return result;
  }

  return result;
}

} // namespace ai
