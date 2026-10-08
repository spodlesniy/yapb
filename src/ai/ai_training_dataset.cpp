//
// AiPB - training dataset writer implementation.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_training_dataset.h>

#include <cstdio>

#include <crlib/twin.h>
#include <crlib/files.h>

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

bool writeCombatEvent(cr::File &file, const CombatEvent &event) {
  return writeString(file, "{\"type\":\"combat_event\",\"event\":\"")
      && writeString(file, combatEventName(event.type))
      && writeString(file, "\",\"evidence_source\":\"")
      && writeString(file, combatEventEvidence(event.type))
      && writeString(file, "\",\"game_time\":") && writeFloat(file, event.gameTime)
      && writeString(file, ",\"round_start_time\":") && writeFloat(file, event.roundStartTime)
      && writeString(file, ",\"round_id\":") && writeUnsigned(file, event.roundId)
      && writeString(file, ",\"episode_id\":") && writeUnsigned(file, event.episodeId)
      && writeString(file, ",\"bot_id\":") && writeInt(file, event.botId)
      && writeString(file, ",\"team\":") && writeInt(file, event.team)
      && writeString(file, ",\"task\":") && writeInt(file, event.task)
      && writeString(file, ",\"ai_action\":") && writeInt(file, event.aiAction)
      && writeString(file, ",\"position\":") && writeVec3(file, event.position)
      && writeString(file, ",\"aim_direction\":") && writeVec3(file, event.aimDirection)
      && writeString(file, ",\"weapon_id\":") && writeInt(file, event.weaponId)
      && writeString(file, ",\"target_id\":") && writeInt(file, event.targetId)
      && writeString(file, ",\"attacker_id\":") && writeInt(file, event.attackerId)
      && writeString(file, ",\"victim_id\":") && writeInt(file, event.victimId)
      && writeString(file, ",\"source_entity_id\":") && writeInt(file, event.sourceEntityId)
      && writeString(file, ",\"ammo_before\":") && writeInt(file, event.ammoBefore)
      && writeString(file, ",\"ammo_after\":") && writeInt(file, event.ammoAfter)
      && writeString(file, ",\"health_damage\":") && writeInt(file, event.healthDamage)
      && writeString(file, ",\"armor_damage\":") && writeInt(file, event.armorDamage)
      && writeString(file, ",\"flash_alpha\":") && writeInt(file, event.flashAlpha)
      && writeString(file, ",\"blind_time_remaining\":") && writeFloat(file, event.blindTimeRemaining)
      && writeString(file, ",\"attack_button_pressed\":")
      && writeString(file, event.attackPressed ? "true" : "false")
      && writeString(file, ",\"enemy_visible\":")
      && writeString(file, event.enemyVisible ? "true" : "false")
      && writeString(file, ",\"enemy_heard\":")
      && writeString(file, event.enemyHeard ? "true" : "false")
      && writeString(file, "}\n");
}

bool writeNavigationEvent(cr::File &file, const NavigationEvent &event) {
  if (!writeString(file, "{\"type\":\"navigation_event\",\"event\":\"")
      || !writeString(file, navigationEventName(event.type))
      || !writeString(file, "\",\"reason\":\"")
      || !writeString(file, navigationEventReasonName(event.reason))
      || !writeString(file, "\",\"game_time\":") || !writeFloat(file, event.gameTime)
      || !writeString(file, ",\"round_start_time\":") || !writeFloat(file, event.roundStartTime)
      || !writeString(file, ",\"round_id\":") || !writeUnsigned(file, event.roundId)
      || !writeString(file, ",\"episode_id\":") || !writeUnsigned(file, event.episodeId)
      || !writeString(file, ",\"bot_id\":") || !writeInt(file, event.botId)
      || !writeString(file, ",\"team\":") || !writeInt(file, event.team)
      || !writeString(file, ",\"task\":") || !writeInt(file, event.task)
      || !writeString(file, ",\"ai_action\":") || !writeInt(file, event.aiAction)
      || !writeString(file, ",\"previous_task\":") || !writeInt(file, event.previousTask)
      || !writeString(file, ",\"next_task\":") || !writeInt(file, event.nextTask)
      || !writeString(file, ",\"previous_node\":") || !writeInt(file, event.previousNode)
      || !writeString(file, ",\"current_node\":") || !writeInt(file, event.currentNode)
      || !writeString(file, ",\"goal_node\":") || !writeInt(file, event.goalNode)
      || !writeString(file, ",\"route_source\":") || !writeInt(file, event.routeSource)
      || !writeString(file, ",\"route_destination\":") || !writeInt(file, event.routeDestination)
      || !writeString(file, ",\"path_type\":") || !writeInt(file, event.pathType)
      || !writeString(file, ",\"path_node_count\":") || !writeInt(file, event.pathNodeCount)
      || !writeString(file, ",\"path_truncated\":")
      || !writeString(file, event.pathTruncated ? "true" : "false")
      || !writeString(file, ",\"path_nodes\":[")) return false;

  const int stored = event.pathNodeCount < kNavigationDiagnosticPathNodes
      ? event.pathNodeCount : kNavigationDiagnosticPathNodes;
  for (int i = 0; i < stored; ++i) {
    if ((i && !writeString(file, ",")) || !writeInt(file, event.pathNodes[i])) return false;
  }
  return writeString(file, "],\"estimated_path_distance\":")
      && writeFloat(file, event.estimatedPathDistance)
      && writeString(file, ",\"physical_displacement\":") && writeFloat(file, event.physicalDisplacement)
      && writeString(file, ",\"distance_to_goal\":") && writeFloat(file, event.distanceToGoal)
      && writeString(file, ",\"position\":") && writeVec3(file, event.position)
      && writeString(file, ",\"velocity\":") && writeVec3(file, event.velocity)
      && writeString(file, "}\n");
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

  // Diagnostics follow transitions; game_time and round_id allow merging
  // them into a single timeline without altering the training sample schema.
  for (size_t i = 0; i < buffer.combatEventCount(); ++i) {
    if (!writeCombatEvent(file, buffer.combatEventAt(i))) {
      result.error = TrainingDatasetWriteError::IoError;
      return result;
    }
    ++result.combatEventCount;
  }

  for (size_t i = 0; i < buffer.navigationEventCount(); ++i) {
    if (!writeNavigationEvent(file, buffer.navigationEventAt(i))) {
      result.error = TrainingDatasetWriteError::IoError;
      return result;
    }
    ++result.navigationEventCount;
  }

  if (file.flush()) {
    result.error = TrainingDatasetWriteError::IoError;
    return result;
  }

  return result;
}

} // namespace ai
