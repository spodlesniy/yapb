//
// AiPB - AI navigation task ownership guard.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

namespace ai {

// AI navigation may only own the bot while the legacy task stack is in a
// navigation-neutral state. The task type itself remains engine-owned; this
// generic helper keeps task and objective ownership testable without the GoldSrc runtime.
template <typename TaskType>
constexpr bool allowsNavigationOverride(TaskType currentTask, TaskType normalTask, TaskType moveToPositionTask,
                                        bool legacyObjectiveActive = false) {
  return !legacyObjectiveActive && (currentTask == normalTask || currentTask == moveToPositionTask);
}

} // namespace ai
