//
// AiPB - bounded external Model I/O sequence progress tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_model_io_sequence.h>

using ai::test::expect;
namespace io = ai::runtime_io;

AI_TEST(testModelIOSequenceBoundsAndSessionIsolation) {
  io::ModelIOSequenceProgress sequence {};
  expect(!sequence.arm(0) && !sequence.arm(1) && !sequence.arm(4),
         "only 2-3 explicitly requested legs may run");
  expect(sequence.arm(3), "three-leg session authorized");
  expect(!sequence.bindSession(0) && sequence.bindSession(7) &&
         !sequence.bindSession(8), "all legs remain in the original bot session");
  expect(sequence.hasNext() && !sequence.finished() &&
         sequence.completed() == 0 && sequence.total() == 3,
         "new session starts at zero completed legs");
}

AI_TEST(testModelIOSequenceOnlyCountsRecoveredPhysicalLegs) {
  io::ModelIOSequenceProgress sequence {};
  expect(sequence.arm(2) && sequence.bindSession(7), "two-leg trial armed");
  expect(!sequence.completeLeg(8, true, true, 2, 84.0f) &&
         !sequence.completeLeg(7, false, true, 2, 84.0f) &&
         !sequence.completeLeg(7, true, false, 2, 84.0f) &&
         !sequence.completeLeg(7, true, true, 1, 84.0f),
         "wrong identity, missing physical reach, recovery or feedback cannot advance");
  expect(sequence.completed() == 0 && sequence.requests() == 0,
         "failed legs never increment progress");
  expect(sequence.completeLeg(7, true, true, 2, 84.0f) &&
         sequence.completed() == 1 && sequence.hasNext() &&
         sequence.requests() == 2 && sequence.distance() == 84.0f,
         "first verified movement permits another locally preflighted leg");
  expect(sequence.completeLeg(7, true, true, 2, 87.0f) &&
         !sequence.hasNext() && sequence.finished() &&
         sequence.completed() == 2 && sequence.requests() == 4 &&
         sequence.distance() == 171.0f,
         "two accepted actions and two feedbacks complete the bounded session");
  expect(!sequence.completeLeg(7, true, true, 2, 84.0f),
         "no third action after the configured cap");
  sequence.reset();
  expect(sequence.total() == 0 && sequence.requests() == 0 &&
         sequence.session() == 0, "new command cannot inherit previous session state");
}
