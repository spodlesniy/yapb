//
// AiPB - allocation boundary for bounded training recorder streams.
// SPDX-License-Identifier: MIT
//

#include <new>

#include <ai/ai_training_recorder.h>

namespace ai {

template <typename T>
T *allocateTrainingRecords(size_t capacity) noexcept {
  return new (std::nothrow) T[capacity] {};
}

template TrainingTransition *allocateTrainingRecords<TrainingTransition>(size_t) noexcept;
template CombatEvent *allocateTrainingRecords<CombatEvent>(size_t) noexcept;
template NavigationEvent *allocateTrainingRecords<NavigationEvent>(size_t) noexcept;
template DefuseEvent *allocateTrainingRecords<DefuseEvent>(size_t) noexcept;
template AimEvent *allocateTrainingRecords<AimEvent>(size_t) noexcept;

} // namespace ai
