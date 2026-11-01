//
// AiPB - GoldSrc perception adapter, read-only and without model control.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_perception_events.h>

class Bot;

namespace ai::runtime_io {

// Takes a snapshot of existing YaPB sensory evidence on the game thread.
void capturePerceptionFrame(Bot &bot);
void releasePerceptionBot(int botSlot);
void resetPerceptionFrames();

// Returns only sanitized, per-bot, current-frame perceptions.
bool readPerceptionFrame(int botSlot, PerceptionFrame &frame);

} // namespace ai::runtime_io
