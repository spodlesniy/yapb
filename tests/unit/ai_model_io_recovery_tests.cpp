//
// AiPB - Model I/O trial lifecycle and recovery unit tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_model_io_recovery.h>

using ai::test::expect;
namespace io = ai::runtime_io;

namespace {

class RecoveryProvider final : public ai::InferenceProvider {
public:
  ai::InferenceResult infer(const ai::InferenceInput &) const override {
    return {};
  }
};

} // namespace

AI_TEST(testModelIORecoveryVerifiesProviderAndFallback) {
  RecoveryProvider original {};
  RecoveryProvider other {};
  const auto restored = io::verifyModelIORecovery(&original, &original,
      true, true, false, false, false);
  expect(restored.verified(), "original provider, fallback and idle state restored");
  expect(!io::verifyModelIORecovery(&original, &other, true, true,
      false, false, false).verified(), "different provider cannot pass recovery");
  expect(!io::verifyModelIORecovery(&original, &original, true, false,
      false, false, false).verified(), "fallback mismatch cannot pass recovery");
  expect(io::verifyModelIORecovery(nullptr, nullptr, false, false,
      false, false, false).verified(), "disabled provider and fallback can be restored");
}

AI_TEST(testModelIORecoveryRequiresBackendAndActionRelease) {
  RecoveryProvider original {};
  expect(!io::verifyModelIORecovery(&original, &original, false, false,
      true, false, false).verified(), "test backend attached is not recovered");
  expect(!io::verifyModelIORecovery(&original, &original, false, false,
      false, true, false).verified(), "active action cannot be reported idle");
  expect(!io::verifyModelIORecovery(&original, &original, false, false,
      false, false, true).verified(), "active trial ownership cannot be reported released");
  const auto released = io::verifyModelIORecovery(&original, &original, false, false,
      false, false, false);
  expect(released.providerRestored && released.fallbackRestored &&
      released.backendReleased && released.actionIdle && released.trialReleased,
      "all five recovery conditions checked independently");
}

AI_TEST(testModelIOTrialStopsOnModeAndOwnerChange) {
  const auto valid = io::modelIOTrialInvalidation(true, false, false, true, true, true);
  expect(valid == io::ModelIOTrialStopReason::None, "healthy active Neural trial remains running");
  expect(io::modelIOTrialInvalidation(true, false, false, false, true, true) ==
      io::ModelIOTrialStopReason::ModeChanged,
      "global Legacy change stops isolated trial before Bot::update");
  expect(io::modelIOTrialInvalidation(true, false, false, true, false, true) ==
      io::ModelIOTrialStopReason::ModeChanged,
      "per-bot mode change also stops trial");
  expect(io::modelIOTrialInvalidation(true, false, true, true, true, true) ==
      io::ModelIOTrialStopReason::OwnerConflict,
      "Explorer/Mock ownership conflict stops trial");
  expect(io::modelIOTrialInvalidation(true, true, false, true, true, true) ==
      io::ModelIOTrialStopReason::RoundEnded,
      "round end prevents further trial movement");
}

AI_TEST(testModelIOTrialStopsOnBotOrSessionLoss) {
  expect(io::modelIOTrialInvalidation(false, false, false, true, true, true) ==
      io::ModelIOTrialStopReason::BotUnavailable,
      "dead or unavailable bot stops movement");
  expect(io::modelIOTrialInvalidation(true, false, false, true, true, false) ==
      io::ModelIOTrialStopReason::SessionChanged,
      "old bot identity cannot continue trial");
  expect(io::modelIOTrialInvalidation(false, true, true, false, false, false) ==
      io::ModelIOTrialStopReason::BotUnavailable,
      "unavailable bot takes precedence over other invalid states");
}

AI_TEST(testModelIOStopReasonNamesRemainStable) {
  expect(io::modelIOTrialStopReasonName(io::ModelIOTrialStopReason::Requested)[0] == 'R',
      "explicit server stop has stable reason");
  expect(io::modelIOTrialStopReasonName(io::ModelIOTrialStopReason::ModeChanged)[0] == 'M',
      "mode changes reported distinctly");
  expect(io::modelIOTrialStopReasonName(io::ModelIOTrialStopReason::MapReset)[0] == 'M',
      "map reset reason available");
  expect(io::modelIOTrialStopReasonName(io::ModelIOTrialStopReason::BotDisconnected)[0] == 'B',
      "disconnect has distinct reason");
  expect(io::modelIOTrialStopReasonName(io::ModelIOTrialStopReason::RecoveryFailed)[0] == 'R',
      "recovery failure has explicit terminal status");
}
