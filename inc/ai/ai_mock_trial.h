//
// AiPB - opt-in single-bot mock integration trial.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstdint>

class Bot;

namespace ai::mock {

// Returns true only for the server-console mock commands.
bool handleServer();
// Returns true when Mock owns the frame and normal YaPB logic must be skipped.
bool frame(Bot &bot);
bool active();
uint64_t nextWorldQueryRequestId();
void reset();
void cancelBot(int botIndex);

} // namespace ai::mock
