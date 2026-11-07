//
// AiPB - runtime binding for control ownership and action feedback.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_execution_ownership.h>

class Bot;

namespace ai::runtime_io {

// Called from the existing per-bot frame hook, before Explorer movement.
bool captureControlFrame(Bot &bot, bool explorerOwnsFrame);
// Called when the existing AI ActionRuntime reports an observed result.
void recordBotActionResult(const Bot &bot, const ActionResult &result);
bool readControlFrame(int botSlot, ControlFrame &frame);
// Clear stale terminal feedback only when no action is currently pending.
bool clearFinishedControlFeedback(int botSlot);
void releaseControlBot(int botSlot);
void resetControlFrames();

} // namespace ai::runtime_io
