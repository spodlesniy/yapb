//
// AiPB - AI model action output schema.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action.h>
#include <ai/ai_observation.h>

#include <cstddef>
#include <cstdint>

namespace ai {

constexpr uint32_t kInferenceActionSchemaVersion = 2;
constexpr size_t kInferenceActionTensorSize = 10;

// Stable positions in the single-output ONNX tensor.
// Every value is represented as float by the model, then decoded explicitly.
enum class InferenceActionTensorIndex : size_t {
  ActionId = 0,
  TargetNode,
  TargetPlayer,
  TargetPositionX,
  TargetPositionY,
  TargetPositionZ,
  WeaponType,
  GrenadeType,
  Duration,
  Confidence,
  Count,
};

// Stable numeric action identifiers exposed to a model.
//
// Values are part of the model contract and must not be reordered. The C++
// ActionType enum may evolve independently; decoding is intentionally explicit.
enum class InferenceActionId : uint8_t {
  None = 0,
  MoveToNode = 1,
  MoveToPosition = 2,
  FollowPlayer = 3,
  SeekCover = 4,
  Retreat = 5,
  HoldPosition = 6,
  Explore = 7,
  AttackTarget = 8,
  HuntTarget = 9,
  AimAtTarget = 10,
  Fire = 11,
  Reload = 12,
  ChangeWeapon = 13,
  PlantBomb = 14,
  DefuseBomb = 15,
  PickupItem = 16,
  EscapeFromBomb = 17,
  RescueHostage = 18,
  ProtectObjective = 19,
  Wait = 20,
  Camp = 21,
  ThrowGrenade = 22,
  ThrowFlashbang = 23,
  ThrowSmoke = 24,
  Hide = 25,
  Count = 26,
};

// Raw, backend-neutral model output.
//
// The model emits only numeric identifiers and primitive parameters. Target
// type is derived from the action specification rather than duplicated in the
// model contract, preventing contradictory action/target combinations.
struct InferenceActionOutput {
  uint32_t schemaVersion { kInferenceActionSchemaVersion };
  uint8_t actionId { static_cast<uint8_t>(InferenceActionId::None) };

  int32_t targetNode { -1 };
  int32_t targetPlayer { -1 };
  Vec3 targetPosition {};

  uint8_t weaponType { static_cast<uint8_t>(WeaponType::Unknown) };
  uint8_t grenadeType { static_cast<uint8_t>(GrenadeType::None) };

  float duration {};
  float confidence {};

  bool hasSupportedSchema() const {
    return schemaVersion == kInferenceActionSchemaVersion;
  }
};

} // namespace ai
