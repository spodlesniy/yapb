//
// AiPB - Model I/O trial recovery verification and stop reasons.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_inference_provider.h>

namespace ai::runtime_io {

// Releasing an opt-in test owner does not prove the original ONNX model is
// loaded. It verifies that the same provider pointer and fallback choice are
// restored and no test action/backend remains installed.
struct ModelIORecoveryReport {
  bool providerRestored {};
  bool fallbackRestored {};
  bool backendReleased {};
  bool actionIdle {};
  bool trialReleased {};

  bool verified() const {
    return providerRestored && fallbackRestored && backendReleased &&
           actionIdle && trialReleased;
  }
};

inline ModelIORecoveryReport verifyModelIORecovery(
    const InferenceProvider *expectedProvider, const InferenceProvider *actualProvider,
    bool expectedFallback, bool actualFallback, bool backendAttached,
    bool actionActive, bool trialActive) {
  ModelIORecoveryReport report {};
  report.providerRestored = expectedProvider == actualProvider;
  report.fallbackRestored = expectedFallback == actualFallback;
  report.backendReleased = !backendAttached;
  report.actionIdle = !actionActive;
  report.trialReleased = !trialActive;
  return report;
}

enum class ModelIOTrialStopReason {
  None,
  Completed,
  Requested,
  ModeChanged,
  BotDisconnected,
  MapReset,
  RoundEnded,
  OwnerConflict,
  BotUnavailable,
  SessionChanged,
  Timeout,
  InvalidAction,
  RecoveryFailed,
  GoalNotReached,
  GoalCycle,
  GoalNoProgress,
  GoalNoNovelDirection,
  GoalDeadEnd,
};

inline const char *modelIOTrialStopReasonName(ModelIOTrialStopReason reason) {
  switch (reason) {
  case ModelIOTrialStopReason::None: return "None";
  case ModelIOTrialStopReason::Completed: return "Completed";
  case ModelIOTrialStopReason::Requested: return "Requested";
  case ModelIOTrialStopReason::ModeChanged: return "ModeChanged";
  case ModelIOTrialStopReason::BotDisconnected: return "BotDisconnected";
  case ModelIOTrialStopReason::MapReset: return "MapReset";
  case ModelIOTrialStopReason::RoundEnded: return "RoundEnded";
  case ModelIOTrialStopReason::OwnerConflict: return "OwnerConflict";
  case ModelIOTrialStopReason::BotUnavailable: return "BotUnavailable";
  case ModelIOTrialStopReason::SessionChanged: return "SessionChanged";
  case ModelIOTrialStopReason::Timeout: return "Timeout";
  case ModelIOTrialStopReason::InvalidAction: return "InvalidAction";
  case ModelIOTrialStopReason::RecoveryFailed: return "RecoveryFailed";
  case ModelIOTrialStopReason::GoalNotReached: return "GoalNotReached";
  case ModelIOTrialStopReason::GoalCycle: return "GoalCycle";
  case ModelIOTrialStopReason::GoalNoProgress: return "GoalNoProgress";
  case ModelIOTrialStopReason::GoalNoNovelDirection: return "GoalNoNovelDirection";
  case ModelIOTrialStopReason::GoalDeadEnd: return "GoalDeadEnd";
  }
  return "Unknown";
}

// An external mode change must be detected from the server cvar as well as
// the BotRuntime mode: the isolated trial skips ordinary Bot::update().
inline ModelIOTrialStopReason modelIOTrialInvalidation(
    bool botAvailable, bool roundEnded, bool ownerConflict, bool globalNeural,
    bool botNeural, bool sameSession) {
  if (!botAvailable) {
    return ModelIOTrialStopReason::BotUnavailable;
  }
  if (roundEnded) {
    return ModelIOTrialStopReason::RoundEnded;
  }
  if (ownerConflict) {
    return ModelIOTrialStopReason::OwnerConflict;
  }
  if (!globalNeural || !botNeural) {
    return ModelIOTrialStopReason::ModeChanged;
  }
  if (!sameSession) {
    return ModelIOTrialStopReason::SessionChanged;
  }
  return ModelIOTrialStopReason::None;
}

} // namespace ai::runtime_io
