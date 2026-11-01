//
// AiPB - bounded GoldSrc geometry query adapter.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_world_query_gate.h>

namespace ai::runtime_io {

// Call synchronously from the GoldSrc game thread only.
// The requested session/frame must match the currently captured bot snapshot.
WorldQueryResult queryBotWorld(int botSlot, const WorldQueryRequest &request);
void releaseWorldQueryBot(int botSlot);
void resetWorldQueries();

} // namespace ai::runtime_io
