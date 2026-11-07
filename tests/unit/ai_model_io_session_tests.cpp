//
// AiPB - bounded external Model I/O session tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <cstdint>
#include <limits>

#include <ai/ai_model_io_sequence.h>
#include <ai/ai_model_io_session.h>

using ai::test::expect;
namespace io = ai::runtime_io;

AI_TEST(testModelIOSessionActionLimitsAndVerifiedProgress) {
  io::ModelIOSequenceProgress progress {};
  expect(!progress.armSession(2) && !progress.armSession(3) &&
         !progress.armSession(9), "extended session rejects old or excessive action counts");
  expect(progress.armSession(8) && progress.bindSession(17),
         "operator may authorize at most eight actions in one bot session");
  for (int i = 0; i < 8; ++i) {
    expect(progress.hasNext() &&
           progress.completeLeg(17, true, true, 2, 84.0f),
           "every action requires physical completion and full recovery");
  }
  expect(progress.finished() && progress.completed() == 8 &&
         progress.requests() == 16 && progress.distance() == 672.0f &&
         !progress.completeLeg(17, true, true, 2, 84.0f),
         "eighth verified leg exhausts authorization without an unbounded ninth");
  progress.reset();
  expect(progress.arm(2) && !progress.arm(8),
         "legacy two-to-three-leg mode remains strictly bounded");
}

AI_TEST(testModelIOSessionAbsoluteWallClockDeadline) {
  io::ModelIOSessionBudget budget {};
  expect(!budget.arm(4, 100) && !budget.arm(31, 100),
         "invalid short and long session deadlines rejected");
  expect(budget.arm(5, 100) && !budget.arm(5, 100),
         "single explicit deadline cannot be extended while active");
  expect(!budget.expired(100) && !budget.expired(5099) &&
         budget.elapsed(5099) == 4999,
         "progress and per-leg resets cannot restart the global deadline");
  expect(budget.expired(5100) && budget.expired(99),
         "deadline and monotonic-clock regression fail closed");
  budget.reset();
  expect(budget.arm(30, 10) && !budget.expired(30009) &&
         budget.expired(30010),
         "maximum thirty-second session is also bounded");
  budget.reset();
  expect(!budget.arm(5, std::numeric_limits<uint64_t>::max() - 4999),
         "overflowing deadline is rejected before arming");
}
