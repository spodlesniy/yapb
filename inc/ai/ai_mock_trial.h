//
// AiPB - opt-in single-bot mock integration trial.
//
// SPDX-License-Identifier: MIT
//

#pragma once

class Bot;

namespace ai::mock {

// Returns true only for the server-console mock commands.
bool handleServer();
void frame(Bot &bot);
void reset();
void cancelBot(int botIndex);

} // namespace ai::mock
